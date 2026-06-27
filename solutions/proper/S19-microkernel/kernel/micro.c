/* S19 · 微内核形态：服务作为“用户态服务任务”，客户经 IPC 请求/应答间接调用（参考解）。
 *
 * 宏内核（macro.c）里客户直接 call 服务函数，零中介；微内核里客户与服务是两个隔离的任务，
 * 唯一的桥是消息通道：客户把请求塞进请求通道、让出 CPU；服务任务收到、调用服务核心、把结果
 * 塞回应答通道、让出；客户再收回结果。每来回一趟都要若干次“让出/切换”——这就是微内核拿来换
 * 隔离性的 IPC 开销。本课用 S5 的 __switch 协作式两任务运行时承载这套请求/应答。
 *
 * 学生需实现的：ipc_send / ipc_recv 两个 IPC 原语（请求/应答的搬运核心）。
 * 客户/服务任务体、工作负载、开销统计、与宏内核结果的逐项对比均给定。 */
#include "kernel.h"
#include "sched.h"
#include "service.h"
#include "micro.h"

/* IPC 往返开销计数：每次 send/recv 各记一“跳”，用于和宏内核（0 跳）对比。 */
static long g_ipc_hops;

/* ================= 学生实现：IPC 请求/应答原语 ================= */

void ipc_send(struct Chan *c, const struct IpcMsg *m) {
    while (c->full) ipc_yield();   /* 通道里还有未取走的消息：让出，等对端取走 */
    c->msg  = *m;                  /* 整条消息拷入单槽 */
    c->full = 1;                   /* 宣布“有货” */
    g_ipc_hops++;
    ipc_yield();                   /* 让对端上 CPU 来收 */
}

void ipc_recv(struct Chan *c, struct IpcMsg *out) {
    while (!c->full) ipc_yield();  /* 通道空：让出，等对端送来 */
    *out    = c->msg;              /* 整条消息拷出 */
    c->full = 0;                   /* 腾空，放行下一条 */
    g_ipc_hops++;
}

/* ================= 以下给定：两任务协议体 + 对比驱动 ================= */

static struct Chan g_req;   /* client → server 请求通道 */
static struct Chan g_rep;   /* server → client 应答通道 */
static uint32_t    g_results[SVC_REQ_N];

/* 服务任务（“用户态服务进程”）：循环收请求 → 调服务核心 → 回应答，处理满 REQ_N 条后退出。 */
static void svc_server(void) {
    int i;
    for (i = 0; i < SVC_REQ_N; i++) {
        struct IpcMsg req, rep;
        ipc_recv(&g_req, &req);
        rep.op = req.op;
        rep.a0 = req.a0;
        rep.a1 = req.a1;
        if (req.op == OP_CREATE) {
            rep.result = (uint32_t)svc_create(req.a0, req.a1);
            rep.rc     = 1;
        } else {
            uint32_t out = 0;
            rep.rc     = svc_read((int)req.a0, &out);
            rep.result = out;
        }
        ipc_send(&g_rep, &rep);
    }
}

/* 客户任务：逐条发请求、收应答，把结果收集起来留待与宏内核对比。 */
static void svc_client(void) {
    int i, op;
    uint32_t a0, a1;
    for (i = 0; i < SVC_REQ_N; i++) {
        struct IpcMsg req, rep;
        svc_demo_req(i, &op, &a0, &a1);
        req.op = op; req.a0 = a0; req.a1 = a1; req.rc = 0; req.result = 0;
        ipc_send(&g_req, &req);
        ipc_recv(&g_rep, &rep);
        g_results[i] = rep.result;
    }
}

/* 微内核路径入口：跑客户/服务两任务，结果须与宏内核 expect[] 逐项一致，且确实付出了 IPC 开销。 */
int run_micro_test(const uint32_t *expect, int n) {
    int i, ok = 1;

    svc_reset();                 /* 干净的服务状态 */
    g_req.full = 0;
    g_rep.full = 0;
    g_ipc_hops = 0;

    if (!sched_run_pair(svc_client, svc_server)) return 0;  /* 活锁守卫触发 → 不达成 */

    for (i = 0; i < n; i++)
        if (g_results[i] != expect[i]) ok = 0;

    kputs("[compare] monolithic direct-call hops=");
    kputdec(0);
    kputs("  microkernel ipc hops=");
    kputdec((uint64_t)g_ipc_hops);
    kputs("  (microkernel trades IPC cost for isolation)\n");

    if (g_ipc_hops == 0) ok = 0; /* 微内核必须真的走 IPC，否则不算 */
    return ok;
}
