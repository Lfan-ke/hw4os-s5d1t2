/* S5d · 内核入口 / 测试驱动（给定）。
 * 在协作式两任务运行时之上，依次验证三种阻塞式同步原语：
 *   ① 互斥锁：两任务在锁内对共享计数器各加 N 次，结果精确无丢更新，且抢不到的任务真阻塞；
 *   ② 信号量：empty/full 两信号量 + 一把 mutex 的有界缓冲生产者-消费者；
 *   ③ 条件变量：mutex + 条件变量再做一遍生产者-消费者。
 * 三项全过才打印 ALL_PASS；跑完返回 → entry.S 调 k_shutdown 让 qemu 退出。
 * 每个 *_PASS 都要求 g_block_events>0：证明确有任务“真阻塞”（让出 CPU 被唤醒），而非自旋。 */
#include "kernel.h"
#include "sync.h"

void kmain(void) {
    int ok_m, ok_s, ok_c;

    kputs("\n[S5d] blocking sync primitives: mutex / semaphore / condvar (block, not spin)\n");

    /* ① 互斥锁：临界区内非原子读改写，靠真正的互斥 + 阻塞防止丢更新。 */
    ok_m = run_mutex_test();
    if (ok_m) kputs("MUTEX_PASS\n");
    else      kputs("MUTEX_MISS (counter wrong or no real blocking; implement wq_block/wq_wake_one)\n");

    /* ② 信号量：有界缓冲生产者-消费者，16 件产品穿过容量 4 的环。 */
    ok_s = run_sem_test();
    if (ok_s) kputs("SEM_PASS\n");
    else      kputs("SEM_MISS (bounded-buffer mismatch or no real blocking)\n");

    /* ③ 条件变量：消费者条件不满足则 wait，生产者 signal 唤醒。 */
    ok_c = run_condvar_test();
    if (ok_c) kputs("CONDVAR_PASS\n");
    else      kputs("CONDVAR_MISS (producer-consumer mismatch or no real blocking)\n");

    if (ok_m && ok_s && ok_c)
        kputs("ALL_PASS\n");
    else
        kputs("some sync checks incomplete\n");
}
