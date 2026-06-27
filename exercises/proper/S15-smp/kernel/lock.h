/* S15 · SMP 锁原语：自旋锁 + 读写锁。
 * 全部以 AMO/LR-SC 原子指令实现（__sync_* 内建在 RV64 上展开为 amoswap/amoadd/lr.w+sc.w）。
 * 无分页：所有 hart satp=0，共享变量按物理地址（恒等映射）天然可见，再以 fence 保内存序。 */
#ifndef OSLAB_S15_LOCK_H
#define OSLAB_S15_LOCK_H

/* —— 自旋锁：单一持有者（互斥）—— */
typedef struct {
    volatile int locked; /* 0=空闲 1=被占 */
} spinlock_t;

void spin_lock(spinlock_t *l);
void spin_unlock(spinlock_t *l);

/* —— 读写锁：多读单写 ——
 * state 语义： 0 = 空闲
 *             >0 = 当前读者数量（多个读者可同时持有）
 *             -1 = 一个写者独占 */
typedef struct {
    volatile int state;
} rwlock_t;

void read_lock(rwlock_t *l);
void read_unlock(rwlock_t *l);
void write_lock(rwlock_t *l);
void write_unlock(rwlock_t *l);

#endif
