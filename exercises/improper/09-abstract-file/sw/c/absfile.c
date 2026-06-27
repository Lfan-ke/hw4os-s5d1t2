/* 设备文件「一切皆文件」—— C。
 * 母题：设备文件 = 给硬件的副作用穿上 read/write 的外衣。
 * 用一张函数指针表（read/write）当 FileLike —— 正对应 xv6 的
 *   struct devsw { int (*read)(); int (*write)(); }，FD_DEVICE 走 devsw[major]。
 *   ① ConstDev   —— read 恒 1、write 恒 0（无存储）。
 *   ② RingSumDev —— 深度 2 环形寄存器，write 推入/233 复位，read 求和。
 * 你只填四个设备函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

typedef struct FileLike FileLike;
struct FileLike {
    uint32_t (*read)(FileLike *self);
    uint32_t (*write)(FileLike *self, uint32_t x);
    uint32_t r0, r1; /* 仅 RingSum 用到 */
};

/* ── 子实验 1：常量设备（read 恒 1 / write 恒 0）── */
/* 分支择一：
 *   // TODO[a] 拆 OneSource(只读 1)+NullSink(只写吞掉) 两个对象
 *   // ELSE[b] 合成一个 ConstDev 实现读写两面（推荐） */
static uint32_t const_read(FileLike *self) {
    (void)self;
    /* TODO: 常量源 —— 读出恒为 1。 */
    return 0; /* ← 占位（应返回 1） */
}
static uint32_t const_write(FileLike *self, uint32_t x) {
    (void)self;
    (void)x;
    /* TODO: 空洞 —— 吞掉数据、返回写了 0 个。 */
    return 1; /* ← 占位（应返回 0） */
}

/* ── 子实验 2：RingSum 有副作用的文件 ── */
static uint32_t ring_read(FileLike *self) {
    /* TODO: 返回 self->r0 + self->r1。 */
    (void)self;
    return 0; /* ← 占位 */
}
static uint32_t ring_write(FileLike *self, uint32_t x) {
    /* TODO: x==233 → 清空环(r0=r1=0)；否则压入新值、挤掉最旧。
     *   // TODO[a] 移位写法：self->r1 = self->r0; self->r0 = x;
     *   // ELSE[b] head 指针写法（两个槽轮流写，读出和相同）
     * 写完返回 0。 */
    (void)self;
    (void)x;
    return 0; /* ← 占位（read 恒 0 会判 RING_FAIL） */
}

/* ── 测试 harness（勿改）── */

static int check_filelike(FileLike *d) {
    int ok = 1;
    for (int i = 0; i < 3; i++) {
        uint32_t r = d->read(d);
        if (r != 1u) { printf("FILELIKE_FAIL read 应=1 实得=%u\n", r); ok = 0; }
    }
    uint32_t xs[3] = { 5u, 700u, 233u };
    for (int i = 0; i < 3; i++) {
        uint32_t w = d->write(d, xs[i]);
        if (w != 0u) { printf("FILELIKE_FAIL write(%u) 应=0 实得=%u\n", xs[i], w); ok = 0; }
    }
    if (ok) printf("FILELIKE_PASS\n");
    return ok;
}

static int check_ring(FileLike *d) {
    uint32_t wv[4]   = { 666u, 111u, 222u, 233u };
    uint32_t want[4] = { 666u, 777u, 333u, 0u };
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        d->write(d, wv[i]);
        uint32_t got = d->read(d);
        if (got != want[i]) { printf("RING_FAIL write %u 后 read=%u 应=%u\n", wv[i], got, want[i]); ok = 0; }
    }
    if (ok) printf("RING_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;

    FileLike cdev = { const_read, const_write, 0, 0 };
    all &= check_filelike(&cdev);

    FileLike rdev = { ring_read, ring_write, 0, 0 };
    all &= check_ring(&rdev);

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
