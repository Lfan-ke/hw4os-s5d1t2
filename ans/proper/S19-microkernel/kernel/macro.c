/* S19 · 宏内核形态：服务就在内核态，客户直接 call（参考解，给定）。
 * 没有消息、没有切换：内核代码与服务代码同处一地址空间，调用即 ret，开销近零。
 * 这里顺手把“宏内核跑出来的结果”存进 expect[]，作为微内核路径的对照基准。 */
#include "kernel.h"
#include "service.h"

/* 运行标准工作负载（直接调用服务核心），把每条请求结果填入 expect[]，*n 置为请求数。
 * 返回 1=全部成功（建文件、回读均正常）。 */
int run_macro_test(uint32_t *expect, int *n) {
    int i, op, ok = 1;
    uint32_t a0, a1;

    svc_reset();
    for (i = 0; i < SVC_REQ_N; i++) {
        svc_demo_req(i, &op, &a0, &a1);
        if (op == OP_CREATE) {
            int id = svc_create(a0, a1);     /* 直接调用，无中介 */
            if (id < 0) ok = 0;
            expect[i] = (uint32_t)id;
        } else {
            uint32_t out = 0;
            if (!svc_read((int)a0, &out)) ok = 0;
            expect[i] = out;
        }
    }
    *n = SVC_REQ_N;
    return ok;
}
