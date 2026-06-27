/* S5d · 三种阻塞式同步原语（参考解）。
 * 互斥锁 / 信号量 / 条件变量都建在同一对“阻塞核心 + 唤醒核心”之上：
 *   wq_block   —— 抢不到资源：入等待队列 + 置 BLOCKED + 让出 CPU（零占用，直到被唤醒）；
 *   wq_wake_one—— 释放资源：出等待队列一个等待者 + 置 READY（让它重新可被调度）。
 * 教学点：阻塞 ≠ 自旋。被阻塞的任务在被唤醒前根本不会被调度器选中——
 *   对照 S15 的自旋锁：自旋者反复占 CPU 空转，阻塞者交出 CPU、零占用。
 * 学生需实现的就是下面两个核心（wq_block / wq_wake_one），其余外壳与测试均给定。 */
#include "kernel.h"
#include "sched.h"
#include "sync.h"

void wq_init(struct WaitQueue *wq) {
    wq->head = wq->tail = wq->count = 0;
}

/* ===================== 阻塞核心 / 唤醒核心（本课的两处实现） ===================== */

/* 阻塞核心：把“当前任务”挂到等待队列 wq，并真正阻塞它（置 BLOCKED + 让出 CPU）。
 * 被 wq_wake_one 置回 READY、调度器再次选中它时，控制流从本函数返回，
 * 回到调用方（lock/down/wait）的 while 处重检条件。 */
static void wq_block(struct WaitQueue *wq) {
    int id = cur_task();
    wq->ids[wq->tail] = id;                 /* 入队（队尾） */
    wq->tail = (wq->tail + 1) % WAIT_CAP;
    wq->count++;
    sched_block();                          /* 置 BLOCKED + __switch 走人：零 CPU 占用直到被唤醒 */
}

/* 唤醒核心：从等待队列取出一个等待者，置回 READY。队列空返回 -1。
 * 注意：只是让它“重新可被调度”，并不立即切过去——单核协作下要等当前任务自己让出/阻塞/退出。 */
static int wq_wake_one(struct WaitQueue *wq) {
    int id;
    if (wq->count == 0) return -1;
    id = wq->ids[wq->head];                 /* 出队（队头） */
    wq->head = (wq->head + 1) % WAIT_CAP;
    wq->count--;
    sched_wake(id);                         /* 置 READY */
    return id;
}

/* ===================== ① 互斥锁 ===================== */
void mutex_init(struct Mutex *m) {
    m->locked = 0;
    wq_init(&m->wq);
}
void mutex_lock(struct Mutex *m) {
    while (m->locked)            /* while 重检：醒来后锁可能又被别人抢走，必须再判一次 */
        wq_block(&m->wq);        /* 锁被占：阻塞等待（不自旋） */
    m->locked = 1;
}
void mutex_unlock(struct Mutex *m) {
    m->locked = 0;
    wq_wake_one(&m->wq);         /* 唤醒一个等待者（它醒来后重检并获取） */
}

/* ===================== ② 信号量 ===================== */
void sem_init(struct Sem *s, int initial) {
    s->count = initial;
    wq_init(&s->wq);
}
void sem_down(struct Sem *s) {
    while (s->count == 0)        /* 无资源：阻塞；while 防虚假唤醒/醒来又被别人取走 */
        wq_block(&s->wq);
    s->count--;
}
void sem_up(struct Sem *s) {
    s->count++;
    wq_wake_one(&s->wq);
}

/* ===================== ③ 条件变量 ===================== */
void condvar_init(struct Condvar *c) {
    wq_init(&c->wq);
}
void condvar_wait(struct Condvar *c, struct Mutex *m) {
    mutex_unlock(m);            /* 先放锁——单核协作下 unlock→block 之间无切换点，等效原子 */
    wq_block(&c->wq);          /* 阻塞在条件变量队列上 */
    mutex_lock(m);             /* 被 signal 唤醒后重新拿锁返回（调用方再 while 重检条件） */
}
void condvar_signal(struct Condvar *c) {
    wq_wake_one(&c->wq);       /* 唤醒一个等待条件的任务；不放锁、不切换 */
}

/* ============================================================ *
 * 以下：三个测试的任务体 + 校验（给定，不需改）。
 * ============================================================ */

/* ---- ① 互斥锁：两任务在锁内对共享计数器各加 N 次，结果须精确无丢更新 ---- */
#define MUTEX_ITERS 8
static struct Mutex g_mtx;
static int g_counter;

/* 关键设计：持锁期间 sync_yield 让出，逼另一个任务来争锁——它若没拿到锁就必须阻塞。
 * 临界区是“读 v → 让出 → 写 v+1”的非原子读改写：只要互斥真正生效（对方被挡在锁外），
 * 就不会丢更新；若互斥失效（对方也进了临界区），必丢更新、计数对不上。 */
static void mutex_task(void) {
    int i;
    for (i = 0; i < MUTEX_ITERS; i++) {
        int v;
        mutex_lock(&g_mtx);
        v = g_counter;
        sync_yield();           /* 持锁让出：制造争用 */
        g_counter = v + 1;
        mutex_unlock(&g_mtx);
    }
}
int run_mutex_test(void) {
    g_counter = 0;
    mutex_init(&g_mtx);
    if (!sched_run_pair(mutex_task, mutex_task)) return 0;  /* 活锁/死锁守卫触发 */
    if (g_counter != 2 * MUTEX_ITERS)            return 0;  /* 丢更新 → 互斥失效 */
    if (g_block_events == 0)                      return 0;  /* 没人真阻塞（自旋）→ 不算过 */
    return 1;
}

/* ---- ② 信号量：有界缓冲生产者-消费者（empty/full 两信号量 + 一把 mutex）---- */
#define SEM_ITEMS 16
#define SEM_CAP   4
static struct Sem   sem_empty, sem_full;
static struct Mutex sem_mtx;
static int sem_ring[SEM_CAP], sem_rh, sem_rt;
static int sem_got[SEM_ITEMS], sem_gi;

static void sem_producer(void) {
    int i;
    for (i = 0; i < SEM_ITEMS; i++) {
        sem_down(&sem_empty);           /* 等一个空槽（满则阻塞） */
        mutex_lock(&sem_mtx);
        sem_ring[sem_rt] = i * 3 + 1;   /* 第 i 件产品 */
        sem_rt = (sem_rt + 1) % SEM_CAP;
        mutex_unlock(&sem_mtx);
        sem_up(&sem_full);              /* 多了一件可消费 */
    }
}
static void sem_consumer(void) {
    int i;
    for (i = 0; i < SEM_ITEMS; i++) {
        int v;
        sem_down(&sem_full);            /* 等一件产品（空则阻塞） */
        mutex_lock(&sem_mtx);
        v = sem_ring[sem_rh];
        sem_rh = (sem_rh + 1) % SEM_CAP;
        mutex_unlock(&sem_mtx);
        sem_up(&sem_empty);             /* 腾出一个空槽 */
        sem_got[sem_gi++] = v;
    }
}
int run_sem_test(void) {
    int i;
    sem_rh = sem_rt = 0;
    sem_gi = 0;
    for (i = 0; i < SEM_ITEMS; i++) sem_got[i] = -1;
    sem_init(&sem_empty, SEM_CAP);      /* 初始 CAP 个空槽 */
    sem_init(&sem_full, 0);             /* 初始 0 件产品 */
    mutex_init(&sem_mtx);
    if (!sched_run_pair(sem_consumer, sem_producer)) return 0; /* 消费者先跑→先阻塞在 full */
    if (sem_gi != SEM_ITEMS)                          return 0;
    for (i = 0; i < SEM_ITEMS; i++)
        if (sem_got[i] != i * 3 + 1)                  return 0; /* 内容/顺序须完全对上 */
    if (g_block_events == 0)                          return 0;
    return 1;
}

/* ---- ③ 条件变量：再做一遍生产者-消费者（消费者条件不满足则 wait，生产者 signal 唤醒）---- */
#define CV_ITEMS 16
#define CV_CAP   32
static struct Mutex   cv_mtx;
static struct Condvar cv_notempty;
static int cv_buf[CV_CAP], cv_head, cv_tail, cv_count;
static int cv_got[CV_ITEMS], cv_gi;

static void cv_producer(void) {
    int i;
    for (i = 0; i < CV_ITEMS; i++) {
        mutex_lock(&cv_mtx);
        cv_buf[cv_tail] = i * 5 + 2;
        cv_tail = (cv_tail + 1) % CV_CAP;
        cv_count++;
        condvar_signal(&cv_notempty);   /* 通知“缓冲非空” */
        mutex_unlock(&cv_mtx);
    }
}
static void cv_consumer(void) {
    int i;
    for (i = 0; i < CV_ITEMS; i++) {
        int v;
        mutex_lock(&cv_mtx);
        while (cv_count == 0)            /* while 重检条件：防虚假唤醒/醒来又被取空 */
            condvar_wait(&cv_notempty, &cv_mtx);
        v = cv_buf[cv_head];
        cv_head = (cv_head + 1) % CV_CAP;
        cv_count--;
        mutex_unlock(&cv_mtx);
        cv_got[cv_gi++] = v;
    }
}
int run_condvar_test(void) {
    int i;
    cv_head = cv_tail = cv_count = 0;
    cv_gi = 0;
    for (i = 0; i < CV_ITEMS; i++) cv_got[i] = -1;
    mutex_init(&cv_mtx);
    condvar_init(&cv_notempty);
    if (!sched_run_pair(cv_consumer, cv_producer)) return 0; /* 消费者先跑→缓冲空→wait */
    if (cv_gi != CV_ITEMS)                          return 0;
    for (i = 0; i < CV_ITEMS; i++)
        if (cv_got[i] != i * 5 + 2)                 return 0;
    if (g_block_events == 0)                        return 0;
    return 1;
}
