/* S5e · 学生实现两处（参考解）：
 *   ① fork_copy_uvm —— fork 时复制父进程地址空间的「用户页」
 *      （栈：整页 eager 复制；数据页：CoW 标记 + 共享物理帧）。
 *   ② cow_fault     —— 写时复制缺页：把共享只读页分裂成私有可写页。
 *
 * 内核恒等映射（map_kernel）在 fork 前已替子进程建好；这里只处理用户私有页。 */
#include "kernel.h"
#include "proc.h"

/* ===== ① fork：复制父地址空间的用户页到子进程 ===== */
/* 返回 0 成功、-1 失败（帧耗尽）。child->root 已 alloc 且已 map_kernel。 */
int fork_copy_uvm(struct proc *parent, struct proc *child) {
    /* —— 栈：整页 eager 复制（每页一份私有帧）——
     * 栈不走 CoW：trap 就跑在用户栈上（共享 __alltraps 不换栈），
     * 若栈是只读 CoW，压栈那一刻又会缺页 → 处理缺页还要压栈 → 嵌套。
     * 故栈直接复制成私有可写页，最稳。 */
    for (uint64_t va = USTACK_BASE; va < USTACK_TOP; va += PAGE_SIZE) {
        uint64_t ppa = va2pa(parent->root, va);
        void *nf = frame_alloc();
        if (!nf) return -1;
        kmemcpy(nf, (void *)ppa, PAGE_SIZE);             /* 复制父栈页内容 */
        map_one(child->root, va, (uint64_t)nf, PTE_R | PTE_W | PTE_U);
    }

    /* —— 数据页：CoW 共享 —— 不复制物理页，父子共指同一帧，
     * 但双方都降级为「只读 + COW 标记」；谁先写谁触发缺页再分裂。 */
    uint64_t dpa = va2pa(parent->root, DATA_VA);
    map_one(parent->root, DATA_VA, dpa, PTE_R | PTE_U | PTE_COW); /* 父：去掉 W，置 COW */
    map_one(child->root,  DATA_VA, dpa, PTE_R | PTE_U | PTE_COW); /* 子：同帧、同样只读 COW */
    ref_inc(dpa);                                                 /* 该帧现被 父+子 共享 → 计数 +1 */

    asm volatile("sfence.vma"); /* 改了父页表项（降权）→ 刷 TLB */
    return 0;
}

/* ===== ② 写时复制缺页：把命中的 CoW 页分裂为私有可写页 ===== */
void cow_fault(uint64_t fault_va) {
    uint64_t vpage = fault_va & ~(PAGE_SIZE - 1);
    uint64_t pa = va2pa(current->root, vpage);
    if (pa == 0) { /* 不在已知用户页内：非 CoW 缺页，报错（占位安全） */
        kputs("COW_MISS no-mapping va=");
        kputhex(vpage);
        console_putchar('\n');
        return;
    }

    if (ref_get(pa) > 1) {
        /* 还有别人共享这帧：真正分裂——分配新帧、拷贝旧内容、旧帧计数 -1，
         * 把当前进程的该 VA 改指新帧并恢复可写（清 COW）。 */
        void *nf = frame_alloc();
        kmemcpy(nf, (void *)pa, PAGE_SIZE);
        ref_dec(pa);
        map_one(current->root, vpage, (uint64_t)nf, PTE_R | PTE_W | PTE_U);
        kputs("COW_PASS split va=");
        kputhex(vpage);
        kputs(" old=");
        kputhex(pa);
        kputs(" new=");
        kputhex((uint64_t)nf);
        console_putchar('\n');
    } else {
        /* 已是最后持有者：无需复制，直接恢复可写、清 COW。 */
        map_one(current->root, vpage, pa, PTE_R | PTE_W | PTE_U);
        kputs("COW_PASS sole-owner va=");
        kputhex(vpage);
        console_putchar('\n');
    }
    g_cow = 1;
    asm volatile("sfence.vma"); /* 改了叶 PTE → 刷 TLB，随后重执行出错指令 */
}
