/* 正经·S17 · 虚拟化软件模型（reference solution）。
 *
 * labctl 的 QEMU(-bios default, 默认 OpenSBI) 不带 RIS-V H 扩展，故本实验用
 * 纯软件模型在 S 态内核里演示 type1 hypervisor 的两个核心机制：
 *
 *   ① 两阶段地址转换 (two-stage address translation)
 *        guest VA --[stage-1 / VS 页表]--> guest PA --[stage-2 / G 页表]--> host PA
 *      H 扩展里 stage-1 受 vsatp 控制（guest 自己的页表），stage-2 受 hgatp
 *      控制（VMM 的客户机物理→宿主物理映射）。本模型用两套二级页表实现，
 *      最终读出宿主帧里的魔数即 TWOSTAGE_PASS。
 *
 *   ② 陷入并模拟 (trap-and-emulate)
 *        guest 在 VS 态读一个被虚拟化的特权 CSR；真机上该读会陷入 HS 态，
 *      由 VMM 拦截并回填一个“伪寄存器值”（guest 看不到真实硬件值）。本模型
 *      用一次跨“特权边界”的调用拦截 guest 的 CSR 读，VMM 返回伪值即
 *      TRAP_EMU_PASS。
 *
 * 两项都过 -> ALL_PASS，kmain 返回后由 entry.S 调 k_shutdown 让 QEMU 退出。
 */
#include "kernel.h"

/* ============================================================= *
 *  迷你物理内存与页表原语（Sv39 风格，仅用 2 级，每级 9 位索引）
 * ============================================================= */
#define PGSHIFT 12
#define PGSIZE  (1UL << PGSHIFT)
#define PXMASK  0x1FFUL               /* 每级页表 512 项 */

#define PTE_V   0x1UL                 /* valid */
#define PTE_R   0x2UL
#define PTE_W   0x4UL
#define PTE_X   0x8UL
#define PTE_LEAF (PTE_R | PTE_W | PTE_X)

/* 宿主物理内存竞技场：所有页表节点与数据帧都从这里分配，天然 4K 对齐，
 * 故其宿主地址可直接编码进 PTE 的 PPN 字段。 */
static uint8_t arena[64 * PGSIZE] __attribute__((aligned(PGSIZE)));
static uint64_t arena_off;

static void *alloc_page(void) {
    void *p = &arena[arena_off];
    arena_off += PGSIZE;
    for (int i = 0; i < (int)PGSIZE; i++) ((uint8_t *)p)[i] = 0;
    return p;
}

/* PTE 编码/解码：把宿主指针的 PPN(>>12) 放进 [53:10]，低 10 位放标志 */
static uint64_t pte_make_ptr(void *next, uint64_t flags) {
    return (((uint64_t)(uintptr_t)next >> PGSHIFT) << 10) | flags;
}
static uint64_t *pte_ptr(uint64_t pte) {
    return (uint64_t *)(uintptr_t)((pte >> 10) << PGSHIFT);
}

/* 把 (vpn -> 目标值) 写进一棵 2 级页表；leaf_val 是叶 PTE 的 PPN 字段语义，
 * 由调用方决定（stage-1 叶=guest 帧号；stage-2 叶=宿主帧 PPN）。 */
static void map2(uint64_t *root, uint64_t vpn, uint64_t leaf_ppn, uint64_t flags) {
    uint64_t i1 = (vpn >> 9) & PXMASK;
    uint64_t i0 = vpn & PXMASK;
    if (!(root[i1] & PTE_V)) {
        uint64_t *l0 = (uint64_t *)alloc_page();
        root[i1] = pte_make_ptr(l0, PTE_V);          /* 非叶：只 V */
    }
    uint64_t *l0 = pte_ptr(root[i1]);
    l0[i0] = (leaf_ppn << 10) | flags | PTE_V | PTE_LEAF;
}

/* ============================================================= *
 *  STAGE 1（VS 页表）：guest VA -> guest PA   —— 本实验给定
 * ============================================================= */
/* 叶 PTE 的 PPN 字段在 stage-1 里就是 “guest 物理帧号(GPPN)”。 */
static uint64_t stage1_walk(uint64_t *vs_root, uint64_t gva, int *ok) {
    uint64_t vpn = gva >> PGSHIFT;
    uint64_t off = gva & (PGSIZE - 1);
    uint64_t i1 = (vpn >> 9) & PXMASK;
    uint64_t i0 = vpn & PXMASK;

    uint64_t pte1 = vs_root[i1];
    if (!(pte1 & PTE_V)) { *ok = 0; return 0; }
    uint64_t *l0 = pte_ptr(pte1);
    uint64_t pte0 = l0[i0];
    if (!(pte0 & PTE_V) || !(pte0 & PTE_LEAF)) { *ok = 0; return 0; }

    uint64_t gppn = pte0 >> 10;          /* 客户机物理帧号 */
    *ok = 1;
    return (gppn << PGSHIFT) | off;      /* guest PA */
}

/* ============================================================= *
 *  STAGE 2（G 页表 / hgatp）：guest PA -> host PA  —— 学生填二级查找
 * ============================================================= */
/* 叶 PTE 的 PPN 字段在 stage-2 里是 “宿主帧”的 PPN（直接可解码成宿主指针）。
 * 返回宿主物理地址（= 宿主指针的整数值）；失败置 *ok=0。 */
static uint64_t stage2_walk(uint64_t *g_root, uint64_t gpa, int *ok) {
    uint64_t gppn = gpa >> PGSHIFT;        /* 把 GPA 当成被 stage-2 翻译的“虚拟”页 */
    uint64_t off  = gpa & (PGSIZE - 1);
    uint64_t i1 = (gppn >> 9) & PXMASK;
    uint64_t i0 = gppn & PXMASK;

    /* TODO: 实现 stage-2(G 页表) 的二级查找，照着上面 stage1_walk 的样子写：
     *   1) pte1 = g_root[i1]; 若 !(pte1 & PTE_V) 则 *ok=0 返回 0。
     *   2) l0 = pte_ptr(pte1) 取下一级 G 页表。
     *   3) pte0 = l0[i0]; 若 !(pte0 & PTE_V) 或 !(pte0 & PTE_LEAF) 则 *ok=0 返回 0。
     *   4) hppn = pte0 >> 10 是宿主物理帧号；*ok=1，返回 (hppn<<PGSHIFT)|off 即 host PA。
     * 现在先安全占位：报告未命中，使两阶段翻译不完整（跑得完但不会 ALL_PASS）。 */
    (void)i1; (void)i0; (void)off; (void)g_root;
    *ok = 0;
    return 0;
}

/* 完整两阶段翻译：guest VA -> host PA（先 stage-1 再 stage-2） */
static uint64_t two_stage_translate(uint64_t *vs_root, uint64_t *g_root,
                                    uint64_t gva, uint64_t *out_gpa, int *ok) {
    int ok1 = 0, ok2 = 0;
    uint64_t gpa = stage1_walk(vs_root, gva, &ok1);
    if (!ok1) { *ok = 0; return 0; }
    if (out_gpa) *out_gpa = gpa;
    uint64_t hpa = stage2_walk(g_root, gpa, &ok2);
    *ok = ok2;
    return hpa;
}

/* ============================================================= *
 *  ① 两阶段地址转换测试
 * ============================================================= */
#define MAGIC 0xC0FFEE12ABCD5678UL

static int test_two_stage(void) {
    kputs("[two-stage] build VS(stage-1) + G(stage-2) page tables\n");

    uint64_t *vs_root = (uint64_t *)alloc_page();   /* guest 自己的页表 */
    uint64_t *g_root  = (uint64_t *)alloc_page();   /* VMM 的 G 页表 */

    /* 选一个 guest 虚拟地址 / guest 物理帧 / 宿主帧 */
    uint64_t gva  = 0x0000000000402abcUL;           /* guest VA */
    uint64_t gppn = 7;                              /* 任选的客户机物理帧号 */
    void    *hframe = alloc_page();                 /* 真正承载数据的宿主帧 */
    uint64_t hppn = (uint64_t)(uintptr_t)hframe >> PGSHIFT;

    /* 把魔数写进宿主帧对应 gva 页内偏移处 */
    uint64_t off = gva & (PGSIZE - 1);
    *(volatile uint64_t *)((uint8_t *)hframe + off) = MAGIC;

    /* stage-1: gva 的页 -> 客户机物理帧 gppn */
    map2(vs_root, gva >> PGSHIFT, gppn, PTE_R | PTE_W);
    /* stage-2: 客户机物理帧 gppn -> 宿主帧 hppn */
    map2(g_root, gppn, hppn, PTE_R | PTE_W);

    kputs("[two-stage] GVA="); kputhex(gva);
    kputs(" expect GPPN="); kputdec(gppn); console_putchar('\n');

    uint64_t gpa = 0;
    int ok = 0;
    uint64_t hpa = two_stage_translate(vs_root, g_root, gva, &gpa, &ok);
    if (!ok) {
        kputs("[two-stage] translation incomplete (fill stage2_walk)\n");
        return 0;
    }
    kputs("[two-stage] -> GPA="); kputhex(gpa);
    kputs(" -> HPA="); kputhex(hpa); console_putchar('\n');

    /* 校验 1：stage-1 得到的 GPA 帧号确为 gppn */
    if ((gpa >> PGSHIFT) != gppn) {
        kputs("[two-stage] stage-1 produced wrong guest frame\n");
        return 0;
    }
    /* 校验 2：经 HPA 读出的内容 == 魔数 */
    uint64_t val = *(volatile uint64_t *)(uintptr_t)hpa;
    kputs("[two-stage] read via HPA = "); kputhex(val); console_putchar('\n');
    if (val != MAGIC) {
        kputs("[two-stage] value mismatch through stages\n");
        return 0;
    }

    kputs("TWOSTAGE_PASS\n");
    return 1;
}

/* ============================================================= *
 *  ② trap-and-emulate：拦截 guest 的特权 CSR 读
 * ============================================================= */
/* 被虚拟化的 CSR（编号仅作模型用） */
#define VCSR_VMID    0x100   /* guest 看到的“自己的” VM id（真机无此 CSR，纯模型）*/
#define VCSR_TIME    0xC01   /* time：VS 态读会陷入，VMM 返回 guest 视角的时间 */

/* VMM(HS 态) 的陷入处理：guest 在 VS 态读 CSR 触发 trap，hypervisor 决定回填值。
 * 真硬件上：VS 态非法/被虚拟化的 CSR 读 -> 陷入 HS，scause=2(illegal) 或被
 * H 扩展直接 redirect，VMM 解码 csr 号后把“伪值”写回 guest 的目标寄存器。 */
static uint64_t vmm_guest_vmid = 0x1234;
static uint64_t vmm_time_offset = 0x9000;   /* VMM 给该 guest 设的时间偏置 */

static uint64_t vmm_emulate_csr_read(int csr, int *trapped) {
    *trapped = 1;                            /* 标记：确实经过了 VMM 拦截 */
    switch (csr) {
    case VCSR_VMID:
        return vmm_guest_vmid;               /* 回填伪 VM id */
    case VCSR_TIME:
        /* guest 看到的是被偏置过的虚拟时间，而非宿主真实 mtime */
        return get_time() + vmm_time_offset;
    default:
        return 0;
    }
}

/* 模拟 guest(VS 态) 执行 `csrr a0, <csr>`：guest 没有直接读硬件的权限，
 * 控制权交给 VMM（= 一次跨特权边界的“陷入”）。 */
static uint64_t guest_execute_csrr(int csr, int *trapped) {
    /* —— 特权边界 —— guest 在这里“陷入”，下面是 hypervisor 代码 —— */
    return vmm_emulate_csr_read(csr, trapped);
}

static int test_trap_emulate(void) {
    kputs("[trap-emu] guest(VS) executes `csrr a0, VMID` (a virtualized CSR)\n");

    int trapped = 0;
    uint64_t v = guest_execute_csrr(VCSR_VMID, &trapped);
    kputs("[trap-emu] VMM returned VMID="); kputhex(v);
    kputs(" trapped="); kputdec((uint64_t)trapped); console_putchar('\n');
    if (!trapped || v != vmm_guest_vmid) {
        kputs("[trap-emu] guest read was not intercepted/emulated\n");
        return 0;
    }

    /* 再来一条：读虚拟 time，必须拿到“被偏置”的虚拟时间而非裸 mtime */
    trapped = 0;
    uint64_t t1 = guest_execute_csrr(VCSR_TIME, &trapped);
    uint64_t real_now = get_time();
    kputs("[trap-emu] guest sees virtual time="); kputhex(t1);
    kputs(" (host mtime~"); kputhex(real_now); kputs(")\n");
    /* 虚拟时间应明显大于真实 mtime（含 VMM 偏置），证明值被模拟过 */
    if (t1 < real_now + (vmm_time_offset / 2)) {
        kputs("[trap-emu] virtual time was not emulated\n");
        return 0;
    }

    kputs("TRAP_EMU_PASS\n");
    return 1;
}

/* ============================================================= *
 *  kmain
 * ============================================================= */
void kmain(void) {
    kputs("\n=== S17 virtualization (software model of RISC-V H ext) ===\n");

    int a = test_two_stage();
    int b = test_trap_emulate();

    if (a && b) {
        kputs("ALL_PASS\n");
    } else {
        kputs("[S17] not all checks passed\n");
    }
    /* 返回 -> entry.S 调 k_shutdown 退出 QEMU */
}
