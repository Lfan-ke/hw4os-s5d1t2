/* 系统调用：从 MCU 中断向量表 → MPU 系统调用（C 参考解，RV64 / qemu-user）。
 *
 * 一条演化链，四段递进（全在本程序里跑）：
 *   S1 向量分发器   —— 硬件按号查表的间接跳转（与 hw/v、hw/bsv 同构）
 *   S2 Trap 上下文  —— 陷入时整存寄存器现场、返回时 sepc+=4 整取
 *   S3 Syscall ABI  —— a7=号、a0..a5=参、a0=返，分发表 + 三个 handler
 *   S4 真实 ecall   —— 内联汇编真的陷入内核（RV64 `ecall` 指令，命中真实 GNU/Linux ABI）
 *
 * 你只需填【STUDENT】标注的函数体；下方 harness（向量 + 校验 + PASS 打印）勿改。
 * 判题：每组打印 *_PASS，全过再 ALL_PASS；任何 *_FAIL 即挂。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════
 * S1 · 硬件向量分发器（MCU 模型）
 *   mode=1 向量化 → handler_pc = base + (cause<<2)（查表）
 *   mode=0 direct   → handler_pc = base
 *   accept = trap_req
 * ════════════════════════════════════════════════════════════════════ */

/* 向量表：第 cause 项 = base + 4*cause（按号存地址的跳转表）。给定。 */
static void build_vector(uint64_t base, uint64_t v[16]) {
    for (int i = 0; i < 16; i++) v[i] = base + (uint64_t)i * 4;
}

/* 【STUDENT】组合分发：算出 *handler_pc 与 *accept。 */
static void dispatch(uint32_t mode, uint64_t base, uint32_t cause, int trap_req,
                     uint64_t *handler_pc, int *accept) {
    uint64_t v[16];
    build_vector(base, v);
    *handler_pc = (mode == 1) ? v[cause & 0xF] : base;
    *accept = trap_req;
}

/* ════════════════════════════════════════════════════════════════════
 * S2 · Trap 上下文保存 / 恢复（the trap frame）
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t regs[32]; /* x0..x31；x10=a0、x17=a7 */
    uint64_t sepc;
    uint64_t sstatus;
} Ctx;

/* 【STUDENT】陷入：把调用者寄存器现场整存进快照。 */
static Ctx ctx_save(const uint64_t regs[32], uint64_t sepc, uint64_t sstatus) {
    Ctx c;
    memcpy(c.regs, regs, sizeof(c.regs));
    c.sepc = sepc;
    c.sstatus = sstatus;
    return c;
}

/* 【STUDENT】返回前推进：a0(x10)=返回值，跳过 ecall 指令（sepc+=4）。 */
static void ctx_advance(Ctx *c, uint64_t retval) {
    c->regs[10] = retval;
    c->sepc += 4;
}

/* ════════════════════════════════════════════════════════════════════
 * S3 · Syscall ABI 分发表（GNU 规范化）
 *   真实 RV64 号：write=64、exit=93、getpid=172；未知号 → -ENOSYS。
 * ════════════════════════════════════════════════════════════════════ */

#define NR_WRITE  64
#define NR_EXIT   93
#define NR_GETPID 172
#define ENOSYS    38
#define FAKE_PID  42

typedef struct { int64_t a[8]; } Regs; /* a[7]=号，a[0..5]=参，a[0]=返回 */

typedef struct {
    uint8_t mem[64];
    int     mem_len;
    uint8_t capture[64];
    int     cap_len;
    int     exited;
    int64_t exit_code;
} Machine;

/* 【STUDENT】sys_write：把 mem[ptr..ptr+len] 回显进 capture 并打印，返回字节数。 */
static int64_t sys_write(Machine *m, int64_t ptr, int64_t len) {
    for (int64_t i = 0; i < len; i++)
        m->capture[m->cap_len++] = m->mem[ptr + i];
    printf("[sys_write] %.*s", (int)len, (const char *)(m->mem + ptr));
    return len;
}

/* 【STUDENT】sys_getpid：返回固定 pid。 */
static int64_t sys_getpid(Machine *m) {
    (void)m;
    return FAKE_PID;
}

/* 【STUDENT】sys_exit：记录退出码，返回 0。 */
static int64_t sys_exit(Machine *m, int64_t code) {
    m->exited = 1;
    m->exit_code = code;
    return 0;
}

/* 【STUDENT】分发：按 a7 选 handler，结果写回 a0；未知号 → -ENOSYS。 */
static void do_syscall(Machine *m, Regs *r) {
    int64_t ret;
    switch (r->a[7]) {
        case NR_WRITE:  ret = sys_write(m, r->a[1], r->a[2]); break;
        case NR_GETPID: ret = sys_getpid(m); break;
        case NR_EXIT:   ret = sys_exit(m, r->a[0]); break;
        default:        ret = -ENOSYS; break;
    }
    r->a[0] = ret;
}

/* ════════════════════════════════════════════════════════════════════
 * S4 · 真实 ecall 往返（user → kernel → user 的安检门）
 *   RV64 Linux：`ecall`，a7=号(write=64)、a0..a2=参，a0=返回值。
 * ════════════════════════════════════════════════════════════════════ */

/* 【STUDENT】裸 syscall 包装：装好寄存器、执行 `ecall`、取回返回值。 */
static long raw_syscall3(long n, long a0, long a1, long a2) {
    register long _a7 asm("a7") = n;
    register long _a0 asm("a0") = a0;
    register long _a1 asm("a1") = a1;
    register long _a2 asm("a2") = a2;
    asm volatile("ecall" : "+r"(_a0) : "r"(_a7), "r"(_a1), "r"(_a2) : "memory");
    return _a0;
}

/* ───────────────────────── 测试 harness（勿改）───────────────────────── */

static int one(uint32_t mode, uint64_t base, uint32_t cause, int trap_req,
               uint64_t exp_pc, int exp_acc) {
    uint64_t pc;
    int acc;
    dispatch(mode, base, cause, trap_req, &pc, &acc);
    if (pc != exp_pc || acc != exp_acc) {
        printf("S1_FAIL mode=%u cause=%u exp=(0x%llx,%d) got=(0x%llx,%d)\n",
               mode, cause, (unsigned long long)exp_pc, exp_acc, (unsigned long long)pc, acc);
        return 0;
    }
    return 1;
}

static int check_s1(void) {
    uint64_t base = 0x80000000ull;
    int ok = 1, g;

    g = 1;
    g &= one(0, base, 0, 1, base, 1);
    g &= one(0, base, 3, 1, base, 1);
    g &= one(0, base, 8, 1, base, 1);
    if (g) printf("DIRECT_PASS\n"); else ok = 0;

    g = 1;
    g &= one(1, base, 0, 1, base, 1);
    g &= one(1, base, 1, 1, base + 4, 1);
    g &= one(1, base, 8, 1, base + 0x20, 1);
    g &= one(1, base, 15, 1, base + 0x3C, 1);
    if (g) printf("VECTORED_PASS\n"); else ok = 0;

    g = 1;
    g &= one(1, base, 2, 1, base + 0x08, 1);
    g &= one(1, base, 5, 0, base + 0x14, 0); /* trap_req=0 → accept=0 */
    g &= one(0, base, 9, 1, base, 1);
    if (g) printf("DISPATCH_PASS\n"); else ok = 0;

    if (ok) printf("S1_PASS\n");
    return ok;
}

static int check_s2(void) {
    int ok = 1, g;
    uint64_t regs[32];
    for (int i = 0; i < 32; i++) regs[i] = 0xA000 + (uint64_t)i;
    uint64_t sepc = 0x1000, sstatus = 0x00000100;

    Ctx ctx = ctx_save(regs, sepc, sstatus);
    g = (memcmp(ctx.regs, regs, sizeof(regs)) == 0) && ctx.sepc == sepc && ctx.sstatus == sstatus;
    if (g) printf("SAVE_PASS\n"); else { printf("SAVE_FAIL 快照与输入不一致\n"); ok = 0; }

    Ctx c2 = ctx;
    ctx_advance(&c2, 7);

    if (c2.regs[10] == 7) printf("RETVAL_PASS\n");
    else { printf("RETVAL_FAIL a0=%llu\n", (unsigned long long)c2.regs[10]); ok = 0; }

    g = 1;
    for (int k = 0; k < 32; k++)
        if (k != 10 && c2.regs[k] != regs[k]) {
            printf("RESTORE_FAIL x%d 被改动\n", k);
            g = 0;
        }
    if (c2.sepc != sepc + 4) { printf("RESTORE_FAIL sepc 错\n"); g = 0; }
    if (c2.sstatus != sstatus) { printf("RESTORE_FAIL sstatus 被改动\n"); g = 0; }
    if (g) printf("RESTORE_PASS\n"); else ok = 0;

    if (ok) printf("S2_PASS\n");
    return ok;
}

static int check_s3(void) {
    int ok = 1;
    Machine m;
    memset(&m, 0, sizeof(m));
    m.mem[0] = 'h'; m.mem[1] = 'i'; m.mem_len = 2;

    Regs r;
    memset(&r, 0, sizeof(r));
    r.a[7] = NR_WRITE; r.a[1] = 0; r.a[2] = 2;
    do_syscall(&m, &r);
    printf("\n");
    if (r.a[0] == 2 && m.cap_len == 2 && m.capture[0] == 'h' && m.capture[1] == 'i')
        printf("NR_WRITE_PASS\n");
    else { printf("NR_WRITE_FAIL a0=%lld\n", (long long)r.a[0]); ok = 0; }

    memset(&r, 0, sizeof(r));
    r.a[7] = NR_GETPID;
    do_syscall(&m, &r);
    if (r.a[0] == FAKE_PID) printf("NR_GETPID_PASS\n");
    else { printf("NR_GETPID_FAIL a0=%lld\n", (long long)r.a[0]); ok = 0; }

    memset(&r, 0, sizeof(r));
    r.a[7] = NR_EXIT; r.a[0] = 5;
    do_syscall(&m, &r);
    if (m.exited && m.exit_code == 5 && r.a[0] == 0) printf("NR_EXIT_PASS\n");
    else { printf("NR_EXIT_FAIL\n"); ok = 0; }

    memset(&r, 0, sizeof(r));
    r.a[7] = 999;
    do_syscall(&m, &r);
    if (r.a[0] == -ENOSYS) printf("ENOSYS_PASS\n");
    else { printf("ENOSYS_FAIL a0=%lld\n", (long long)r.a[0]); ok = 0; }

    if (ok) printf("S3_PASS\n");
    return ok;
}

static int check_s4(void) {
    int ok = 1;
    const char *msg = "HELLO_SYSCALL\n";
    long len = (long)strlen(msg);

    long r = raw_syscall3(NR_WRITE, 1, (long)msg, len); /* write(fd=1, buf, len) */
    if (r >= 0) printf("ECALL_PASS\n"); else { printf("ECALL_FAIL ret=%ld\n", r); ok = 0; }
    if (r == len) printf("SYSRET_PASS\n"); else { printf("SYSRET_FAIL ret=%ld\n", r); ok = 0; }

    if (ok) printf("S4_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_s1();
    all &= check_s2();
    all &= check_s3();
    all &= check_s4();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
