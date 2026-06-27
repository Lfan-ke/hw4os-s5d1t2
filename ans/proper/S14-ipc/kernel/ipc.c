/* S14 · IPC 三件套（参考解）。
 * 两个协作任务（生产者 / 消费者）在单核上轮转，通过三种机制传数据：
 *   ① 管道（字节流环形缓冲）→ PIPE_PASS
 *   ② 消息队列（定长消息环形队列）→ MSG_PASS
 *   ③ 共享内存 + 完成标志握手 → SHM_PASS
 * 学生需实现的：pipe_write_byte / pipe_read_byte / mq_push / mq_pop 四个核心。
 * 其余（环形缓冲的“满/空判定靠 count”心智、生产者-消费者协作循环、握手）均给定。 */
#include "kernel.h"
#include "sched.h"
#include "ipc.h"

/* ===================== ① 管道：字节流环形缓冲 ===================== */
void pipe_init(struct Pipe *p) {
    p->head = p->tail = p->count = 0;
}

int pipe_write_byte(struct Pipe *p, uint8_t b) {
    if (p->count == PIPE_CAP) return 0;          /* 满：写不进 */
    p->buf[p->tail] = b;
    p->tail = (p->tail + 1) % PIPE_CAP;          /* 环形推进 */
    p->count++;
    return 1;
}

int pipe_read_byte(struct Pipe *p, uint8_t *out) {
    if (p->count == 0) return 0;                 /* 空：读不出 */
    *out = p->buf[p->head];
    p->head = (p->head + 1) % PIPE_CAP;
    p->count--;
    return 1;
}

/* ===================== ② 消息队列：定长消息环形队列 ===================== */
void mq_init(struct MsgQueue *q) {
    q->head = q->tail = q->count = 0;
}

int mq_push(struct MsgQueue *q, const struct Msg *m) {
    int i;
    if (q->count == MQ_CAP) return 0;            /* 满 */
    for (i = 0; i < MSG_LEN; i++)
        q->slots[q->tail].data[i] = m->data[i];  /* 整条消息拷入 */
    q->tail = (q->tail + 1) % MQ_CAP;
    q->count++;
    return 1;
}

int mq_pop(struct MsgQueue *q, struct Msg *out) {
    int i;
    if (q->count == 0) return 0;                 /* 空 */
    for (i = 0; i < MSG_LEN; i++)
        out->data[i] = q->slots[q->head].data[i];/* 整条消息拷出 */
    q->head = (q->head + 1) % MQ_CAP;
    q->count--;
    return 1;
}

/* ============================================================ *
 * 以下：三个测试的协作任务体 + 校验（给定，不需改）。
 * ============================================================ */

/* ---- ① 管道：32 字节流穿过 8 字节环形缓冲 ---- */
#define PIPE_TOTAL 32
static struct Pipe g_pipe;
static uint8_t pipe_src[PIPE_TOTAL];
static uint8_t pipe_dst[PIPE_TOTAL];
static int     pipe_wi, pipe_ri;

static void pipe_producer(void) {
    while (pipe_wi < PIPE_TOTAL) {
        if (pipe_write_byte(&g_pipe, pipe_src[pipe_wi]))
            pipe_wi++;          /* 写进去了，推进 */
        else
            ipc_yield();        /* 管道满：让消费者来读 */
    }
}
static void pipe_consumer(void) {
    while (pipe_ri < PIPE_TOTAL) {
        uint8_t b;
        if (pipe_read_byte(&g_pipe, &b))
            pipe_dst[pipe_ri++] = b;
        else
            ipc_yield();        /* 管道空：让生产者来写 */
    }
}
static int pipe_test(void) {
    int i;
    pipe_init(&g_pipe);
    pipe_wi = pipe_ri = 0;
    for (i = 0; i < PIPE_TOTAL; i++) pipe_src[i] = (uint8_t)(i * 7 + 3);
    for (i = 0; i < PIPE_TOTAL; i++) pipe_dst[i] = 0;
    if (!sched_run_pair(pipe_producer, pipe_consumer)) return 0; /* 活锁守卫触发 */
    if (pipe_wi != PIPE_TOTAL || pipe_ri != PIPE_TOTAL) return 0;
    for (i = 0; i < PIPE_TOTAL; i++)
        if (pipe_dst[i] != pipe_src[i]) return 0;
    return 1;
}

/* ---- ② 消息队列：8 条定长消息穿过容量 4 的队列 ---- */
#define MSG_TOTAL 8
static struct MsgQueue g_mq;
static int  msg_wi, msg_ri;
static int  msg_ok;   /* 消费者一路比对的结果（1=至此全对） */

static void msg_producer(void) {
    while (msg_wi < MSG_TOTAL) {
        struct Msg m;
        int k;
        for (k = 0; k < MSG_LEN; k++)
            m.data[k] = (uint8_t)(msg_wi * 10 + k);   /* 第 i 条消息的内容 */
        if (mq_push(&g_mq, &m))
            msg_wi++;
        else
            ipc_yield();        /* 队列满：让消费者出队 */
    }
}
static void msg_consumer(void) {
    while (msg_ri < MSG_TOTAL) {
        struct Msg m;
        if (mq_pop(&g_mq, &m)) {
            int k;
            for (k = 0; k < MSG_LEN; k++)
                if (m.data[k] != (uint8_t)(msg_ri * 10 + k)) msg_ok = 0;
            msg_ri++;
        } else {
            ipc_yield();        /* 队列空：让生产者入队 */
        }
    }
}
static int msg_test(void) {
    mq_init(&g_mq);
    msg_wi = msg_ri = 0;
    msg_ok = 1;
    if (!sched_run_pair(msg_producer, msg_consumer)) return 0;
    return msg_ok && msg_wi == MSG_TOTAL && msg_ri == MSG_TOTAL;
}

/* ---- ③ 共享内存 + 完成标志握手 ---- */
static struct Shm g_shm;
static int shm_ok;

/* 消费者先上 CPU：轮询 ready，未置位就让出（“A 等 B 置位”的等待方）。 */
static void shm_consumer(void) {
    uint32_t s = 0;
    int i;
    while (!g_shm.ready) ipc_yield();   /* 等生产者置 ready */
    for (i = 0; i < SHM_N; i++) s += g_shm.data[i];
    shm_ok = (s == g_shm.sum);          /* 共享区数据 + 生产者声明的校验和一致 */
}
/* 生产者：填共享区、算校验和，最后一步才置 ready（置位前数据必须全部就绪）。 */
static void shm_producer(void) {
    uint32_t s = 0;
    int i;
    for (i = 0; i < SHM_N; i++) {
        g_shm.data[i] = (uint32_t)(i * i + 1);
        s += g_shm.data[i];
    }
    g_shm.sum = s;
    __asm__ volatile("fence" ::: "memory"); /* 先把数据/和落定，再宣布 ready */
    g_shm.ready = 1;
}
static int shm_test(void) {
    int i;
    g_shm.ready = 0;
    g_shm.sum = 0;
    for (i = 0; i < SHM_N; i++) g_shm.data[i] = 0;
    shm_ok = 0;
    /* 消费者（等待方）先跑，生产者后跑——刻意制造一次真实的“等待→置位→放行”握手。 */
    if (!sched_run_pair(shm_consumer, shm_producer)) return 0;
    return shm_ok;
}

/* ============================================================ *
 * 对外测试入口（main.c 调用）。
 * ============================================================ */
int run_pipe_test(void) { return pipe_test(); }
int run_msg_test(void)  { return msg_test(); }
int run_shm_test(void)  { return shm_test(); }
