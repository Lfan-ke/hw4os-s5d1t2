/* S15 · 锁实现（参考解）。
 * 关键点：在 SMP（真并行）下，"读-改-写" 必须靠硬件原子指令，关中断不再够用——
 * 关中断只挡住本核的抢占，挡不住别的核同时改同一块物理内存。 */
#include "lock.h"

/* ===== 自旋锁 =====
 * acquire：amoswap 把 locked 置 1 并取回旧值；旧值为 0 表示我抢到了。
 *          __sync_lock_test_and_set 在 RV64 上即 amoswap.w，且带 acquire 语义。
 *          抢不到时只读自旋（test-and-test-and-set），减少总线上的写竞争。
 * release：__sync_lock_release 即一条带 release 语义的原子写 0。 */
void spin_lock(spinlock_t *l) {
    while (__sync_lock_test_and_set(&l->locked, 1) != 0) {
        while (l->locked) {
            /* 只读自旋，等锁变空再回去 CAS，避免 cache 行反复抖动 */
        }
    }
}

void spin_unlock(spinlock_t *l) {
    __sync_lock_release(&l->locked);
}

/* ===== 读写锁（读者优先）=====
 * read_lock：只要没有写者（state>=0），就把读者计数 +1（CAS 保证原子地从 s→s+1）。
 *            多个读者可同时成功，于是读临界区可真并行——这正是相对自旋锁的优势。
 * write_lock：仅当 state==0（无读者无写者）时 CAS 到 -1，独占。
 * 所有 __sync CAS/add 都是全屏障，兼作内存序栅栏。 */
void read_lock(rwlock_t *l) {
    for (;;) {
        int s = l->state;
        if (s >= 0 && __sync_bool_compare_and_swap(&l->state, s, s + 1))
            return;
        /* s<0 表示有写者占用，自旋重试 */
    }
}

void read_unlock(rwlock_t *l) {
    __sync_fetch_and_sub(&l->state, 1);
}

void write_lock(rwlock_t *l) {
    while (!__sync_bool_compare_and_swap(&l->state, 0, -1)) {
        /* 有读者或写者，自旋重试 */
    }
}

void write_unlock(rwlock_t *l) {
    __sync_bool_compare_and_swap(&l->state, -1, 0);
}
