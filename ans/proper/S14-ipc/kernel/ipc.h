/* S14 · IPC 三件套的数据结构与接口。
 * ① 管道 Pipe：定长环形缓冲，承载字节流（read/write 由学生实现）。
 * ② 消息队列 MsgQueue：定长消息的环形队列（push/pop 由学生实现）。
 * ③ 共享内存 Shm：直接共享的数据区 + 完成标志位握手（给定，示范“A 等 B 置位”）。 */
#ifndef S14_IPC_H
#define S14_IPC_H
#include <stdint.h>

/* ====== ① 管道：字节流环形缓冲 ====== */
#define PIPE_CAP 8
struct Pipe {
    uint8_t  buf[PIPE_CAP];
    uint32_t head;   /* 下一个读出位置 */
    uint32_t tail;   /* 下一个写入位置 */
    uint32_t count;  /* 当前字节数（0..PIPE_CAP） */
};
void pipe_init(struct Pipe *p);
/* 写一个字节：成功返回 1；满了返回 0（一个字节都没写）。 */
int  pipe_write_byte(struct Pipe *p, uint8_t b);
/* 读一个字节到 *out：成功返回 1；空了返回 0（*out 不变）。 */
int  pipe_read_byte(struct Pipe *p, uint8_t *out);

/* ====== ② 消息队列：定长消息环形队列 ====== */
#define MSG_LEN 4
#define MQ_CAP  4
struct Msg { uint8_t data[MSG_LEN]; };
struct MsgQueue {
    struct Msg slots[MQ_CAP];
    uint32_t   head;
    uint32_t   tail;
    uint32_t   count;
};
void mq_init(struct MsgQueue *q);
/* 入队一条消息（整条拷入）：成功返回 1；满返回 0。 */
int  mq_push(struct MsgQueue *q, const struct Msg *m);
/* 出队一条消息到 *out（整条拷出）：成功返回 1；空返回 0。 */
int  mq_pop(struct MsgQueue *q, struct Msg *out);

/* ====== ③ 共享内存 + 完成标志握手 ====== */
#define SHM_N 16
struct Shm {
    volatile int ready;     /* 生产者写完置 1；消费者轮询此位（“A 等 B 置位”） */
    uint32_t     data[SHM_N];
    uint32_t     sum;       /* 生产者写入的校验和，供消费者比对 */
};

#endif
