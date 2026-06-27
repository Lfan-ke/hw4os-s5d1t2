/* S14 · IPC 三件套（学生填空版）。
 * 两个协作任务（生产者 / 消费者）在单核上轮转，通过三种机制传数据：
 *   ① 管道（字节流环形缓冲）→ PIPE_PASS
 *   ② 消息队列（定长消息环形队列）→ MSG_PASS
 *   ③ 共享内存 + 完成标志握手 → SHM_PASS（给定，已可跑通）
 *
 * 你要实现 4 个核心：pipe_write_byte / pipe_read_byte / mq_push / mq_pop。
 * 环形缓冲的要点：head=读位置、tail=写位置、count=当前元素数；
 *   - 满 = (count == 容量)，写不进；空 = (count == 0)，读不出；
 *   - 推进下标用取模回绕：idx = (idx + 1) % 容量。
 * 其余（生产者-消费者协作循环、握手、校验）均已给定。
 *
 * 占位实现“假装成功但不真正搬运数据”，所以能编译、能跑、不死锁，但数据对不上 →
 * 不会打印 ALL_PASS。把 4 个 TODO 填对即可。 */
#include "kernel.h"
#include "sched.h"
#include "ipc.h"

/* ===================== ① 管道：字节流环形缓冲 ===================== */
void pipe_init(struct Pipe *p) {
    p->head = p->tail = p->count = 0;
}

int pipe_write_byte(struct Pipe *p, uint8_t b) {
    /* TODO: 若 count==PIPE_CAP 返回 0（满）；否则把 b 写到 buf[tail]，
     *       tail=(tail+1)%PIPE_CAP，count++，返回 1。 */
    (void)p; (void)b;
    return 1;            /* 占位：假装写成功（但没真写）→ 数据对不上 */
}

int pipe_read_byte(struct Pipe *p, uint8_t *out) {
    /* TODO: 若 count==0 返回 0（空）；否则把 buf[head] 取到 *out，
     *       head=(head+1)%PIPE_CAP，count--，返回 1。 */
    (void)p;
    *out = 0;            /* 占位：假装读成功（取到 0）→ 数据对不上 */
    return 1;
}

/* ===================== ② 消息队列：定长消息环形队列 ===================== */
void mq_init(struct MsgQueue *q) {
    q->head = q->tail = q->count = 0;
}

int mq_push(struct MsgQueue *q, const struct Msg *m) {
    /* TODO: 若 count==MQ_CAP 返回 0（满）；否则把整条消息 m 拷到 slots[tail]
     *       （逐字节复制 MSG_LEN 字节），tail=(tail+1)%MQ_CAP，count++，返回 1。 */
    (void)q; (void)m;
    return 1;            /* 占位：假装入队成功（但没真入）→ 数据对不上 */
}

int mq_pop(struct MsgQueue *q, struct Msg *out) {
    /* TODO: 若 count==0 返回 0（空）；否则把 slots[head] 整条拷到 *out，
     *       head=(head+1)%MQ_CAP，count--，返回 1。 */
    int i;
    (void)q;
    for (i = 0; i < MSG_LEN; i++) out->data[i] = 0;
    return 1;            /* 占位：假装出队成功（取到全 0）→ 数据对不上 */
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

/* ---- ③ 共享内存 + 完成标志握手（给定，已可跑通） ---- */
static struct Shm g_shm;
static int shm_ok;

static void shm_consumer(void) {
    uint32_t s = 0;
    int i;
    while (!g_shm.ready) ipc_yield();   /* 等生产者置 ready */
    for (i = 0; i < SHM_N; i++) s += g_shm.data[i];
    shm_ok = (s == g_shm.sum);
}
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
    if (!sched_run_pair(shm_consumer, shm_producer)) return 0;
    return shm_ok;
}

/* ============================================================ *
 * 对外测试入口（main.c 调用）。
 * ============================================================ */
int run_pipe_test(void) { return pipe_test(); }
int run_msg_test(void)  { return msg_test(); }
int run_shm_test(void)  { return shm_test(); }
