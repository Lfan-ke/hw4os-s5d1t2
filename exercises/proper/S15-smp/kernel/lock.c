/* S15 · 锁实现（学生填空版）。
 *
 * 你要补全 5 处临界原语。约束：SMP 真并行下必须用硬件原子指令——
 * RV64 上 GCC 的 __sync_* 内建会展开为 amoswap / amoadd / lr.w+sc.w；
 * 关中断只挡本核抢占，挡不住别的核同时改同一物理地址，所以不够用。
 *
 * 占位实现（不加锁）能编译、能跑，但会丢更新 / 读到撕裂数据，
 * 于是 SPINLOCK_PASS / RWLOCK_PASS / ALL_PASS 都不会出现——这正是"未完成"的样子。
 * 把 // TODO 换成真正的原子实现后，三个 PASS 才会齐。
 */
#include "lock.h"

/* ===== 自旋锁 ===== */
void spin_lock(spinlock_t *l) {
    (void)l;
    /* TODO: 用 __sync_lock_test_and_set(&l->locked, 1)（即 amoswap.w）抢锁；
     *       旧值非 0 说明别人持有，自旋等待（建议 test-and-test-and-set，
     *       内层只读 l->locked 自旋，减少总线写竞争）。 */
}

void spin_unlock(spinlock_t *l) {
    (void)l;
    /* TODO: 用 __sync_lock_release(&l->locked)（带 release 语义的原子写 0）放锁。 */
}

/* ===== 读写锁（读者优先）=====
 * state: 0=空闲  >0=读者数  -1=写者独占 */
void read_lock(rwlock_t *l) {
    (void)l;
    /* TODO: 自旋直到没有写者：读 s=l->state，若 s>=0 则
     *       __sync_bool_compare_and_swap(&l->state, s, s+1) 把读者计数 +1；
     *       成功即返回。多个读者可同时成功 → 读临界区并行。 */
}

void read_unlock(rwlock_t *l) {
    (void)l;
    /* TODO: __sync_fetch_and_sub(&l->state, 1) 退出读临界区。 */
}

void write_lock(rwlock_t *l) {
    (void)l;
    /* TODO: 自旋直到 __sync_bool_compare_and_swap(&l->state, 0, -1) 成功（独占）。 */
}

void write_unlock(rwlock_t *l) {
    (void)l;
    /* TODO: 把 state 从 -1 改回 0（如 __sync_bool_compare_and_swap(&l->state, -1, 0)）。 */
}
