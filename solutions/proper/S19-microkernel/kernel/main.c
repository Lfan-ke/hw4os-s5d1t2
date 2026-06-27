/* S19 · 内核入口 / 测试驱动（给定）。
 * 同一个服务，两种形态各跑一遍并对比，再加一道可插拔驱动注册收尾：
 *   ① 宏内核：服务在内核态直接调用            → MACRO_PASS
 *   ② 微内核：服务作用户态任务，经 IPC 请求/应答 → MICRO_PASS（结果须与①逐项一致）
 *   ③ 驱动系统：按名字注册 + 派发可插拔驱动     → DRIVER_SYS_PASS
 * 三项全过打印 ALL_PASS；kmain 返回 → entry.S 调 k_shutdown 让 qemu 退出。 */
#include "kernel.h"
#include <stdint.h>

int run_macro_test(uint32_t *expect, int *n);
int run_micro_test(const uint32_t *expect, int n);
int run_driver_test(void);

void kmain(void) {
    uint32_t expect[8];
    int n = 0;
    int ok_macro, ok_micro, ok_drv;

    kputs("\n[S19] microkernel vs monolithic: one service, two shapes + pluggable drivers\n");

    /* ① 宏内核：直接调用，并记录基准结果。 */
    ok_macro = run_macro_test(expect, &n);
    if (ok_macro) kputs("MACRO_PASS\n");
    else          kputs("monolithic in-kernel service mismatch\n");

    /* ② 微内核：经 IPC 请求/应答，结果须与①一致。 */
    ok_micro = run_micro_test(expect, n);
    if (ok_micro) kputs("MICRO_PASS\n");
    else          kputs("microkernel ipc service mismatch (implement ipc_send/ipc_recv)\n");

    /* ③ 驱动系统：可插拔注册 + 按名字派发。 */
    ok_drv = run_driver_test();
    if (ok_drv) kputs("DRIVER_SYS_PASS\n");
    else        kputs("driver registry mismatch (implement driver_register)\n");

    if (ok_macro && ok_micro && ok_drv)
        kputs("ALL_PASS\n");
    else
        kputs("some checks incomplete\n");
}
