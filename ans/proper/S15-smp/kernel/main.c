/* S15 · SMP 安全 + 读写锁（参考解，承接 S13 多核启动）。
 *
 * 测试驱动（4 个 hart 真并行）：
 *   Phase A 无锁累加      —— 演示丢更新（仅打印，不作判据）
 *   Phase B 自旋锁累加    —— 无丢更新 → SPINLOCK_PASS
 *   Phase C 读多写少      —— 读写锁给读者一致快照、并允许读者并行 → RWLOCK_PASS
 *
 * 跨核同步全靠物理地址（恒等映射，satp=0）上的共享变量 + AMO 原子 + fence。
 */
#include "kernel.h"
#include "lock.h"

#define NHART        4          /* labctl 用 -smp 4 */
#define SPIN_ITERS   10000      /* 每核累加次数 */
#define K            8          /* 共享数据数组长度 */
#define RBASE        1000       /* 数据初值 */
#define RITERS       20000      /* 每个读者的读次数 */
#define WITERS       2000       /* 写者的写次数 */

/* —— Phase A/B：累加器 —— */
static spinlock_t lock = {0};
static volatile long counter = 0;       /* 自旋锁保护 */
static volatile long racy_counter = 0;  /* 无锁，故意制造竞态 */

/* —— Phase C：读写锁保护的共享数据 —— */
static rwlock_t rw = {0};
static volatile int  data[K];
static volatile long consistent_reads = 0; /* 读到自洽快照的次数 */
static volatile long torn_reads = 0;       /* 读到撕裂（半更新）的次数 */
static volatile int  active_readers = 0;   /* 当前在读临界区的读者数 */
static volatile int  max_readers = 0;      /* 观测到的最大并发读者数 */

/* —— 跨核屏障（sense 翻转，原子计数 + 代际标志）—— */
static volatile int bar_count = 0;
static volatile int bar_gen = 0;
static void barrier(void) {
    int g = bar_gen;
    if (__sync_add_and_fetch(&bar_count, 1) == NHART) {
        bar_count = 0;
        __sync_synchronize();
        bar_gen = g + 1;                 /* 最后到达者放行全员 */
    } else {
        while (bar_gen == g)
            __asm__ volatile("" ::: "memory");
    }
}

static volatile int harts_online = 1;    /* 引导 hart 自己先记一票 */
static volatile int role_ctr = 0;        /* 原子派发的 worker 序号（0..NHART-1）*/

extern void secondary_entry(void);       /* smp.S */

/* 每个 hart（含引导 hart）都跑同一段测试主体。
 * 角色按"原子派发的唯一序号"分，而非 hartid——这样无需知道引导 hartid
 * （OpenSBI 的引导 hart 未必是 0），仍能保证恰好一个写者。 */
static void worker(void) {
    int hart = __sync_fetch_and_add(&role_ctr, 1);  /* 0..NHART-1，唯一 */
    /* ---- Phase A：无锁累加（可能丢更新，仅演示）---- */
    for (int i = 0; i < SPIN_ITERS; i++)
        racy_counter++;                  /* 非原子读-改-写，跨核竞争 */
    barrier();

    /* ---- Phase B：自旋锁累加（互斥，无丢更新）---- */
    for (int i = 0; i < SPIN_ITERS; i++) {
        spin_lock(&lock);
        counter++;
        spin_unlock(&lock);
    }
    barrier();

    /* ---- Phase C：读多写少 ---- */
    if (hart == NHART - 1) {
        /* 唯一写者：写锁下把整个数组同步 +1（保持"全相等"不变式）*/
        for (int w = 0; w < WITERS; w++) {
            write_lock(&rw);
            for (int k = 0; k < K; k++)
                data[k] = data[k] + 1;
            write_unlock(&rw);
        }
    } else {
        /* 读者：读锁下取一致快照，检查全相等（写者绝不可能在读期间插入）*/
        for (int r = 0; r < RITERS; r++) {
            read_lock(&rw);
            /* 统计并发读者数，证明读者真的并行进入了临界区 */
            int n = __sync_add_and_fetch(&active_readers, 1);
            int m = max_readers;
            while (n > m && !__sync_bool_compare_and_swap(&max_readers, m, n))
                m = max_readers;
            int v0 = data[0];
            int ok = 1;
            for (int k = 1; k < K; k++)
                if (data[k] != v0) ok = 0;
            __sync_sub_and_fetch(&active_readers, 1);
            read_unlock(&rw);
            if (ok) __sync_fetch_and_add(&consistent_reads, 1);
            else    __sync_fetch_and_add(&torn_reads, 1);
        }
    }
    barrier();
}

/* 副 hart 落点（a0=hartid，仅 smp.S 用来选栈；这里逻辑不依赖它）：
 * 登记上线，跑测试，返回后由 smp.S 停泊。 */
void secondary_main(int hart) {
    (void)hart;
    __sync_fetch_and_add(&harts_online, 1);
    worker();
}

void kmain(void) {
    kputs("=== S15 SMP: spinlock + rwlock ===\n");
    for (int k = 0; k < K; k++) data[k] = RBASE;

    /* 启动其余 hart（SBI HSM hart_start，EID=0x48534D，fid=0）。
     * 引导 hartid 未知，故对 0..NHART-1 全部尝试：对自身的启动会被 SBI
     * 拒绝（已在运行），无害；其余 NHART-1 个核都会被唤醒。 */
    int started = 0;
    for (int h = 0; h < NHART; h++) {
        long r = sbi_call(0x48534D, 0, h, (long)secondary_entry, 0);
        if (r == 0) started++;
    }
    kputs("hart_start ok="); kputdec(started);
    kputs(" of "); kputdec(NHART - 1); kputs("\n");

    /* 等所有核上线，再一起进 worker（屏障按 NHART 计数，不能少人）*/
    while (harts_online < NHART)
        __asm__ volatile("" ::: "memory");

    worker();  /* 引导 hart 也参与 */

    /* ===== 仅引导 hart 在最后一道屏障后到此汇总 ===== */
    long spin_expect = (long)NHART * SPIN_ITERS;

    kputs("racy_counter="); kputdec((uint64_t)racy_counter);
    kputs(" (lossy, expect "); kputdec((uint64_t)spin_expect); kputs(")\n");

    kputs("counter="); kputdec((uint64_t)counter);
    kputs(" expect="); kputdec((uint64_t)spin_expect); kputs("\n");
    int spin_ok = (counter == spin_expect);
    if (spin_ok) kputs("SPINLOCK_PASS\n");
    else         kputs("SPINLOCK_BAD lost-updates\n");

    /* 读写锁结果 */
    int v0 = data[0], dok = 1;
    for (int k = 1; k < K; k++) if (data[k] != v0) dok = 0;
    long wexp = RBASE + WITERS;
    kputs("data[0]="); kputdec((uint64_t)data[0]);
    kputs(" expect="); kputdec((uint64_t)wexp);
    kputs(" all-equal="); kputdec((uint64_t)dok); kputs("\n");
    kputs("consistent_reads="); kputdec((uint64_t)consistent_reads);
    kputs(" torn_reads="); kputdec((uint64_t)torn_reads); kputs("\n");
    kputs("max_concurrent_readers="); kputdec((uint64_t)max_readers);
    kputs(" (>1 => readers ran in parallel, spin would force 1)\n");

    int rw_ok = (torn_reads == 0) && (consistent_reads > 0) &&
                dok && (data[0] == wexp);
    if (rw_ok) kputs("RWLOCK_PASS\n");
    else       kputs("RWLOCK_BAD inconsistent-snapshot\n");

    if (spin_ok && rw_ok) kputs("ALL_PASS\n");
    /* kmain 返回 → entry.S 调 k_shutdown，qemu 退出 */
}
