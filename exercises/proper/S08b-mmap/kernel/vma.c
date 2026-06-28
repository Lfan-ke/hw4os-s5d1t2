/* S08b · VMA 表 + mmap/munmap + 按需调页缺页处理（学生填空版）。
 *
 * 核心思想（对照 S05c「开机就把页全映射好」）：
 *   - mmap 只「登记」一段合法 VA（一个 VMA），不立即分配/映射任何物理页（lazy/惰性）。
 *   - 首次访问该区域 → 硬件缺页(scause=12/13/15) → trap 转到 vma_handle_fault：
 *       查 VMA：命中 → frame_alloc() 取一个零帧、map_one 映进页表、返回重执行该指令成功；
 *               未命中 → 返回 0 表示「这是真错误」（野地址 / 已 munmap）。
 *
 * 你只需补两个 // TODO：mmap（登记 VMA）与 vma_handle_fault（查 VMA + 命中补帧映射）。
 */
#include "kernel.h"
#include "paging.h"
#include "vma.h"

/* ===== 给定：VMA 表与根页表句柄 ===== */
static struct vma vmas[MAX_VMA];
static uint64_t  *g_root = 0;

void vma_init(uint64_t *root) {
    g_root = root;
    for (int i = 0; i < MAX_VMA; i++) vmas[i].used = 0;
}

/* 给定：查覆盖 va 的 VMA（[start, start+len) 命中）。 */
struct vma *vma_find(uint64_t va) {
    for (int i = 0; i < MAX_VMA; i++) {
        struct vma *v = &vmas[i];
        if (v->used && va >= v->start && va < v->start + v->len) return v;
    }
    return 0;
}

/* 给定：PROT_* → 叶 PTE 标志（W 蕴含 R，符合 SV39 对 W=1,R=0 的保留约定）。 */
static uint64_t prot_to_flags(uint64_t prot) {
    uint64_t f = 0;
    if (prot & PROT_READ)  f |= PTE_R;
    if (prot & PROT_WRITE) f |= PTE_R | PTE_W;
    if (prot & PROT_EXEC)  f |= PTE_X;
    if (f == 0) f = PTE_R; /* 至少可读，避免空权限叶 PTE */
    return f;
}

/* ===== 学生实现①：mmap —— 只登记一个 VMA，不分配物理页 ===== */
void *mmap(uint64_t addr, uint64_t len, uint64_t prot) {
    (void)len; (void)prot;
    /* TODO(mmap)：把这段 VA 登记成一个 VMA，**到此为止**——不要 frame_alloc、不要 map_one。
     *   addr = PAGE_DOWN(addr);
     *   len  = (len + PAGE_MASK) & ~PAGE_MASK;          // 向上取整到整页
     *   找一个 vmas[i].used==0 的空槽：
     *       vmas[i].used=1; vmas[i].start=addr; vmas[i].len=len; vmas[i].prot=prot;
     *       return (void*)addr;
     *   都满了：return (void*)-1;
     */
    return (void *)addr; /* 占位：未登记 VMA → 后续缺页查不到 → 非 ALL_PASS（不崩） */
}

/* ===== 学生实现②：缺页处理 —— 查 VMA，命中则补帧映射并恢复 ===== */
int vma_handle_fault(uint64_t fault_va) {
    (void)fault_va;
    /* TODO(vma_handle_fault)：
     *   struct vma *v = vma_find(fault_va);
     *   if (!v) return 0;                                  // 不在任何 VMA → 真错误
     *   uint64_t page_va = PAGE_DOWN(fault_va);            // 只补「出错的这一页」
     *   void *pa = frame_alloc();
     *   if (!pa) return 0;                                 // 帧耗尽 → 当错误
     *   map_one(g_root, page_va, (uint64_t)pa, prot_to_flags(v->prot));
     *   asm volatile("sfence.vma");                        // 刷 TLB，重执行才命中
     *   return 1;                                          // 已处理 → trap 不前移 sepc → 重跑成功
     */
    return 0; /* 占位：当作未命中 → trap 跳过出错指令安全恢复（不崩、不死循环），非 ALL_PASS */
}

/* ===== 给定：munmap —— 撤销 VMA，并拆掉已经按需映射的那些页 ===== */
void munmap(uint64_t addr, uint64_t len) {
    addr = PAGE_DOWN(addr);
    len  = (len + PAGE_MASK) & ~PAGE_MASK;
    for (int i = 0; i < MAX_VMA; i++) {
        struct vma *v = &vmas[i];
        if (!v->used) continue;
        if (addr <= v->start && v->start + v->len <= addr + len) {
            for (uint64_t va = v->start; va < v->start + v->len; va += PAGE_SIZE)
                unmap_one(g_root, va);
            v->used = 0;
        }
    }
    asm volatile("sfence.vma");
}
