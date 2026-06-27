/* S14 · 内核入口 / 测试驱动（给定）。
 * 在协作式两任务运行时之上，依次验证三种 IPC：管道 / 消息队列 / 共享内存握手。
 * 三项全过才打印 ALL_PASS；跑完返回 → entry.S 调 k_shutdown 让 qemu 退出。 */
#include "kernel.h"

int run_pipe_test(void);
int run_msg_test(void);
int run_shm_test(void);

void kmain(void) {
    int ok_pipe, ok_msg, ok_shm;

    kputs("\n[S14] IPC over cooperative tasks: pipe / message-queue / shared-memory\n");

    /* ① 管道：32 字节流穿过 8 字节环形缓冲（生产者写满则让出，消费者读空则让出）。 */
    ok_pipe = run_pipe_test();
    if (ok_pipe) kputs("PIPE_PASS\n");
    else         kputs("pipe byte-stream mismatch (implement pipe_write_byte/pipe_read_byte)\n");

    /* ② 消息队列：8 条定长消息穿过容量 4 的环形队列。 */
    ok_msg = run_msg_test();
    if (ok_msg) kputs("MSG_PASS\n");
    else        kputs("message queue mismatch (implement mq_push/mq_pop)\n");

    /* ③ 共享内存 + 完成标志握手：消费者等 ready，生产者写完置位放行。 */
    ok_shm = run_shm_test();
    if (ok_shm) kputs("SHM_PASS\n");
    else        kputs("shared-memory handshake mismatch\n");

    if (ok_pipe && ok_msg && ok_shm)
        kputs("ALL_PASS\n");
    else
        kputs("some IPC checks incomplete\n");
}
