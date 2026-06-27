/* S5e · 学生实现两处（填空版）：
 *   ① fork_copy_uvm —— fork 时复制父进程地址空间的「用户页」
 *      （栈：整页 eager 复制；数据页：CoW 标记 + 共享物理帧）。
 *   ② cow_fault     —— 写时复制缺页：把共享只读页分裂成私有可写页。
 *
 * 内核恒等映射 + 用户别名（map_kernel）在 fork 前已替子进程建好；这里只处理用户私有页。 */
#include "kernel.h"
#include "proc.h"

/* ===== ① fork：复制父地址空间的用户页到子进程 =====
 * 返回 0 成功、-1 失败。child->root 已 alloc 且已 map_kernel。 */
int fork_copy_uvm(struct proc *parent, struct proc *child) {
    (void)parent;
    (void)child;
    /* TODO(fork_copy_uvm)：
     *  — 栈：整页 eager 复制（栈不能走 CoW：trap 跑在用户栈上，只读栈一压栈就缺页嵌套）。
     *      for (uint64_t va = USTACK_BASE; va < USTACK_TOP; va += PAGE_SIZE) {
     *          uint64_t ppa = va2pa(parent->root, va);
     *          void *nf = frame_alloc(); if (!nf) return -1;
     *          kmemcpy(nf, (void *)ppa, PAGE_SIZE);
     *          map_one(child->root, va, (uint64_t)nf, PTE_R | PTE_W | PTE_U);
     *      }
     *  — 数据页：CoW 共享（不复制物理帧，父子共指同一帧、双方降为只读+COW）。
     *      uint64_t dpa = va2pa(parent->root, DATA_VA);
     *      map_one(parent->root, DATA_VA, dpa, PTE_R | PTE_U | PTE_COW);  // 父：去 W、置 COW
     *      map_one(child->root,  DATA_VA, dpa, PTE_R | PTE_U | PTE_COW);  // 子：同帧只读 COW
     *      ref_inc(dpa);                                                 // 该帧现被父+子共享
     *  — 最后：asm volatile("sfence.vma"); return 0;
     *
     * 占位：直接返回 -1 → fork 失败、不创建子进程，FORK/COW/EXEC/WAIT 都不会通过
     *       （但不崩、不死循环；填好上面后才会跑出 ALL_PASS）。 */
    return -1;
}

/* ===== ② 写时复制缺页：把命中的 CoW 页分裂为私有可写页 ===== */
void cow_fault(uint64_t fault_va) {
    (void)fault_va;
    /* TODO(cow_fault)：
     *   uint64_t vpage = fault_va & ~(PAGE_SIZE - 1);
     *   uint64_t pa = va2pa(current->root, vpage);
     *   if (pa == 0) { kputs("COW_MISS no-mapping\n"); return; }   // 占位安全：非 CoW 缺页
     *   if (ref_get(pa) > 1) {                                     // 还有人共享 → 真分裂
     *       void *nf = frame_alloc();
     *       kmemcpy(nf, (void *)pa, PAGE_SIZE);
     *       ref_dec(pa);
     *       map_one(current->root, vpage, (uint64_t)nf, PTE_R | PTE_W | PTE_U);
     *       kputs("COW_PASS split va="); kputhex(vpage);
     *       kputs(" old="); kputhex(pa); kputs(" new="); kputhex((uint64_t)nf);
     *       console_putchar('\n');
     *   } else {                                                   // 最后持有者 → 直接恢复可写
     *       map_one(current->root, vpage, pa, PTE_R | PTE_W | PTE_U);
     *       kputs("COW_PASS sole-owner\n");
     *   }
     *   g_cow = 1;
     *   asm volatile("sfence.vma");   // 改了叶 PTE → 刷 TLB，随后硬件重执行出错的写指令
     *
     * 注意：缺页处理不要前移 sepc——要重执行同一条访存指令（ecall 才 +4）。
     * 占位：不做任何修复（默认 fork 失败→不会有 CoW 缺页；若你已实现 fork，
     *       这里留空会让写指令反复缺页，靠 sched.c 的 STEP_GUARD 有界守卫兜底退出，不会真死循环）。 */
}
