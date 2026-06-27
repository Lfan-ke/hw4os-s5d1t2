/* S19 · 微内核 IPC 通道：单槽消息邮箱 + 满标志。
 * 微内核把服务赶出内核、变成一个用户态服务任务；客户任务要用它，只能通过【消息】请求/应答。
 * 这里用一条请求通道（client→server）+ 一条应答通道（server→client），各一个单槽邮箱。 */
#ifndef S19_MICRO_H
#define S19_MICRO_H
#include <stdint.h>

/* 一条 IPC 消息：操作码 + 两个参数 + 返回码 + 结果。 */
struct IpcMsg {
    int      op;        /* OP_CREATE / OP_READ */
    uint32_t a0;        /* 参数 0（create:key / read:id） */
    uint32_t a1;        /* 参数 1（create:val） */
    int      rc;        /* 服务返回码（1 成功 / 0 失败） */
    uint32_t result;    /* 服务结果（create:id / read:val） */
};

/* 单槽通道：一格消息 + 满标志（full=1 表示有一条未被取走的消息）。 */
struct Chan {
    struct IpcMsg msg;
    volatile int  full;
};

/* 发送：通道满则让出等待，腾空后写入并置 full、再让出给对端。 */
void ipc_send(struct Chan *c, const struct IpcMsg *m);
/* 接收：通道空则让出等待，到货后读出并清 full。 */
void ipc_recv(struct Chan *c, struct IpcMsg *out);

#endif
