/* 形态 F1 · 宏内核 / 单内核（monolithic）—— C。
 *
 * 本质：fs / 调度 / 驱动 / 内存 等所有服务都跑在**同一个地址空间**里，
 * 子系统之间靠**直接函数调用**协作——没有 IPC、没有特权切换。
 *   好处：快（一次系统调用 = 几次普通函数调用，零消息、零上下文切换）。
 *   代价：脆（没有隔离边界——一个驱动越界写就能直接踩坏调度器的内存）。
 * 真实例：xv6、Linux、FreeBSD——fs/sched/driver 全在 kernel/ 下共享 struct。
 *
 * 两段 demo：
 *   1. MONO    —— fs_read / sched_pick / driver_io 当「内核服务」直接串起来。
 *   2. FRAGILE —— 同一块 kmem 里驱动缓冲紧挨调度器队列；越界 DMA 写污染调度器。
 *
 * 你只需填 6 个函数体（标 TODO 处）；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── 单一内核地址空间布局 ──
 * kmem[8]：[0..4) 驱动 TX 缓冲区，[4..8) 调度器就绪队列优先级。
 * 这条边界只存在于注释里，硬件上它就是一根连续数组——宏内核「脆」的根源。 */
#define KMEM       8
#define DRV_BASE   0
#define DRV_LEN    4
#define SCHED_BASE 4
#define SCHED_LEN  4

#define CMD_WRITE 1u
#define CMD_READ  0u

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：6 个函数
 * ════════════════════════════════════════════════════════════════ */

/* ── 1. 三个「内核服务」── */

/* fs 服务：读 inode 的内容字。约定 content = 100 + inode。 */
static uint32_t fs_read(uint32_t inode) {
    /* TODO: 返回 100 + inode。 */
    (void)inode;
    return 0; /* ← 占位 */
}

/* 调度服务：在就绪队列里挑优先级最大者，返回下标；并列取最低下标。 */
static int sched_pick(const uint32_t *prios, int n) {
    /* TODO: 遍历 prios，返回最大值的下标（并列取最低下标）。 */
    (void)prios;
    (void)n;
    return 0; /* ← 占位 */
}

/* 驱动服务：WRITE 返回回执 (arg + 0x10)；READ 返回寄存器 0xD0。 */
static uint32_t driver_io(uint32_t cmd, uint32_t arg) {
    /* TODO: cmd==CMD_WRITE → arg + 0x10；否则 → 0xD0。 */
    (void)cmd;
    (void)arg;
    return 0; /* ← 占位 */
}

/* ── 2. 服务直接调用链 ── */

/* 宏内核 syscall 路径：直接调 fs_read → sched_pick → driver_io，无 IPC。
 * 把 result 写到 *result、调用次数写到 *hops（恒为 3）。 */
static void syscall_dispatch(uint32_t inode, const uint32_t *prios, int n,
                             uint32_t *result, uint32_t *hops) {
    /* TODO: 直接调用链——服务就是普通函数，一个接一个调：
     *   uint32_t data = fs_read(inode);                 // 调用 1
     *   uint32_t idx  = (uint32_t)sched_pick(prios, n); // 调用 2
     *   uint32_t ack  = driver_io(CMD_WRITE, data);     // 调用 3
     *   *result = data + idx + ack; *hops = 3;          */
    (void)inode;
    (void)prios;
    (void)n;
    *result = 0; /* ← 占位 */
    *hops = 0;
}

/* ── 3. 无隔离边界：越界 DMA 写 + 破坏检测 ── */

/* 驱动 DMA 写：把 val 写进 kmem 的 DRV_BASE+off 处，**不做边界检查**。
 * off 越过 DRV_LEN 就会踩进调度器的内存。 */
static void driver_dma_write(uint32_t *kmem, int off, uint32_t val) {
    /* TODO: kmem[DRV_BASE + off] = val;（不要加边界检查！） */
    (void)kmem;
    (void)off;
    (void)val;
}

/* 破坏检测：调度器区当前快照 region 与基准 baseline 不同即返回 1。 */
static int detect_corruption(const uint32_t *region, const uint32_t *baseline, int n) {
    /* TODO: memcmp(region, baseline, n*4) != 0 即返回 1。 */
    (void)region;
    (void)baseline;
    (void)n;
    return 0; /* ← 占位 */
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int check_mono(void) {
    int ok = 1;

    /* (a) 三个服务各自的契约。 */
    if (fs_read(3) != 103u) {
        printf("MONO_FAIL fs_read(3)=%u 应=103\n", fs_read(3));
        ok = 0;
    }
    uint32_t prios[4] = {10, 30, 20, 5};
    if (sched_pick(prios, 4) != 1) {
        printf("MONO_FAIL sched_pick([10,30,20,5])=%d 应=1\n", sched_pick(prios, 4));
        ok = 0;
    }
    if (driver_io(CMD_WRITE, 50) != 66u || driver_io(CMD_READ, 0) != 0xD0u) {
        printf("MONO_FAIL driver_io write=%u read=0x%x 应=66/0xD0\n",
               driver_io(CMD_WRITE, 50), driver_io(CMD_READ, 0));
        ok = 0;
    }

    /* (b) 直接调用链：结果 = 三服务产物之和，hops=3，IPC 全程为 0。 */
    uint32_t inode = 3;
    uint32_t expect = fs_read(inode) + (uint32_t)sched_pick(prios, 4) +
                      driver_io(CMD_WRITE, fs_read(inode));
    uint32_t result = 0, hops = 0;
    syscall_dispatch(inode, prios, 4, &result, &hops);
    if (result != expect) {
        printf("MONO_FAIL syscall_dispatch 结果=%u 应=%u\n", result, expect);
        ok = 0;
    }
    if (hops != 3u) {
        printf("MONO_FAIL 直接调用 hops=%u 应=3（fs→sched→driver 三次直调）\n", hops);
        ok = 0;
    }
    int ipc_msgs = 0;
    printf("MONO_DISPATCH ipc_msgs=%d hops=%u result=%u（直调，无消息传递）\n",
           ipc_msgs, hops, result);

    if (ok)
        printf("MONO_PASS\n");
    return ok;
}

static int check_fragile(void) {
    int ok = 1;
    const uint32_t base_kmem[KMEM] = {0, 0, 0, 0, 10, 30, 20, 5};
    uint32_t baseline[SCHED_LEN];
    memcpy(baseline, &base_kmem[SCHED_BASE], sizeof(baseline));

    int pick_before = sched_pick(&base_kmem[SCHED_BASE], SCHED_LEN);
    if (pick_before != 1) {
        printf("FRAGILE_FAIL 初始调度应选任务1(优先级30)，却选了 %d\n", pick_before);
        ok = 0;
    }

    /* (a) 守规矩的 DMA：off=2 在驱动缓冲区内 → 只动自己的地盘。 */
    uint32_t kmem[KMEM];
    memcpy(kmem, base_kmem, sizeof(kmem));
    driver_dma_write(kmem, 2, 0xAB);
    if (kmem[2] != 0xABu) {
        printf("FRAGILE_FAIL 合法 DMA 未写入驱动缓冲区 kmem[2]=%u\n", kmem[2]);
        ok = 0;
    }
    if (detect_corruption(&kmem[SCHED_BASE], baseline, SCHED_LEN)) {
        printf("FRAGILE_FAIL 合法 DMA 不该污染调度器，却报告了破坏\n");
        ok = 0;
    }
    if (sched_pick(&kmem[SCHED_BASE], SCHED_LEN) != pick_before) {
        printf("FRAGILE_FAIL 合法 DMA 后调度决策不该改变\n");
        ok = 0;
    }

    /* (b) 越界 DMA：off=5 越过 DRV_LEN=4 → kmem[5] = 调度器任务1。 */
    memcpy(kmem, base_kmem, sizeof(kmem));
    driver_dma_write(kmem, 5, 0); /* 把任务1的优先级 30 踩成 0 */
    if (!detect_corruption(&kmem[SCHED_BASE], baseline, SCHED_LEN)) {
        printf("FRAGILE_FAIL 越界 DMA 已污染调度器，检测却没发现（漏检）\n");
        ok = 0;
    }
    int pick_after = sched_pick(&kmem[SCHED_BASE], SCHED_LEN);
    if (pick_after == pick_before) {
        printf("FRAGILE_FAIL 调度器被污染后决策应改变，却仍选 %d\n", pick_after);
        ok = 0;
    }
    printf("FRAGILE_OBSERVE 驱动越界 off=5 踩进 kmem[5](调度器任务1)：决策 %d→%d，无隔离边界\n",
           pick_before, pick_after);

    if (ok)
        printf("FRAGILE_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_mono();
    all &= check_fragile();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
