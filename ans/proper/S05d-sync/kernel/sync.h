/* S05d · 三种阻塞式同步原语的数据结构与接口。
 * 共同底座：每个原语带一个等待队列 WaitQueue（FIFO 的任务 id 队列）。
 *   - 抢不到资源 → wq_block：把当前任务入队 + 置 BLOCKED + 让出 CPU；
 *   - 释放资源   → wq_wake_one：出队一个等待者 + 置 READY。
 * 这两步就是“阻塞核心 / 唤醒核心”，三种原语都复用它们（本课要你实现的两处）。 */
#ifndef S5D_SYNC_H
#define S5D_SYNC_H
#include "sched.h"

#define WAIT_CAP NTASK   /* 单核两任务：一个队列上最多 NTASK-1 个等待者，留足容量 */

/* 等待队列：阻塞在某资源上的任务 id 的 FIFO。 */
struct WaitQueue {
    int ids[WAIT_CAP];
    int head;
    int tail;
    int count;
};
void wq_init(struct WaitQueue *wq);

/* ====== ① 互斥锁 Mutex：阻塞式独占 ====== */
struct Mutex {
    int locked;              /* 0=空闲，1=被持有 */
    struct WaitQueue wq;     /* 等锁的任务 */
};
void mutex_init(struct Mutex *m);
void mutex_lock(struct Mutex *m);     /* 锁被占→阻塞等待；醒来重检后获取 */
void mutex_unlock(struct Mutex *m);   /* 释放锁→唤醒一个等待者 */

/* ====== ② 信号量 Semaphore：阻塞式计数 ====== */
struct Sem {
    int count;               /* 可用资源数 */
    struct WaitQueue wq;     /* 等资源的任务 */
};
void sem_init(struct Sem *s, int initial);
void sem_down(struct Sem *s);         /* P：count 为 0 则阻塞，否则 count-- */
void sem_up(struct Sem *s);           /* V：count++ 并唤醒一个等待者 */

/* ====== ③ 条件变量 Condvar：配一把 mutex 的阻塞等待 ====== */
struct Condvar {
    struct WaitQueue wq;     /* 等条件的任务 */
};
void condvar_init(struct Condvar *c);
/* 在持有 m 的前提下等待：原子地“放锁 + 阻塞”，被 signal 唤醒后重新拿锁返回。 */
void condvar_wait(struct Condvar *c, struct Mutex *m);
/* 唤醒一个在该条件变量上等待的任务（不放锁、不切换）。 */
void condvar_signal(struct Condvar *c);

/* 测试入口（main.c 调用）。 */
int run_mutex_test(void);
int run_sem_test(void);
int run_condvar_test(void);

#endif
