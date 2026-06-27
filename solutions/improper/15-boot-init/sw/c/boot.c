/* 引导入门 · 启动握手（软件模拟 MMIO）—— C 参考解。
 * 一句话母题：软件是硬件的开机咒语，链接顺序决定谁先念咒。
 *
 * 设备模型与测试 harness 已给定、勿改。你只需填两处：
 *   1) boot_init()  —— 四步握手：解锁 → 配时钟 → 使能 → 轮询 READY。
 *   2) boot_ctor()  —— 让 boot_init 先于 app_main 跑（构造器登记）。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── 寄存器图（偏移即下标）── */
#define MMIO_UNLOCK 0u
#define MMIO_CLKDIV 1u
#define MMIO_CTRL   2u
#define MMIO_STATUS 3u
#define MMIO_DATA   4u

#define MAGIC   0xB007C0DEu     /* 解锁咒语 */
#define CTRL_EN 0x1u            /* CTRL.bit0 使能 */
#define CTRL_LE 0x2u            /* CTRL.bit1 锁定使能 */

#define ST_READY  0x1u          /* STATUS.bit0 就绪 */
#define ST_LOCKED 0x2u          /* STATUS.bit1 未解锁 */
#define ST_BADCLK 0x4u          /* STATUS.bit2 CLKDIV 非法 */
#define ST_NOTEN  0x8u          /* STATUS.bit3 未使能 */

#define BADBOOT 0x0BADB007u     /* 未就绪误用 DATA 读回的胡话 */

/* ── 设备模型（给定，勿改）──────────────────────────────────────── */
typedef struct {
    int      unlocked;          /* UNLOCK==MAGIC 写过 */
    uint32_t clkdiv;            /* 时钟分频 */
    int      en;                /* CTRL.EN */
    int      ready;             /* PLL 锁定/就绪 */
    int      lock_ctr;          /* 就绪倒计时（模拟 PLL lock 轮询） */
    uint32_t data_raw;          /* 最近写入 DATA（低 16 位） */
    int      touched_before_ready; /* 首次 DATA 访问发生在 READY 之前 */
} Device;

static void dev_reset(Device *d) { memset(d, 0, sizeof *d); }

static int clkdiv_valid(const Device *d) { return d->clkdiv >= 1u && d->clkdiv <= 15u; }

static uint32_t dev_transform(uint32_t raw) { return (raw ^ 0xCAFEu) & 0xFFFFu; }

/* 写 MMIO 寄存器 */
static void mmio_write(Device *d, unsigned reg, uint32_t val) {
    switch (reg) {
    case MMIO_UNLOCK:
        d->unlocked = (val == MAGIC);
        break;
    case MMIO_CLKDIV:
        d->clkdiv = val;
        break;
    case MMIO_CTRL:
        d->en = (val & CTRL_EN) ? 1 : 0;
        /* EN 写入且已解锁且 CLKDIV 合法 → 启动锁定倒计时 */
        if (d->en && d->unlocked && clkdiv_valid(d)) {
            if (!d->ready && d->lock_ctr == 0) d->lock_ctr = 3;
        } else {
            d->ready = 0;
            d->lock_ctr = 0;
        }
        break;
    case MMIO_DATA:
        d->data_raw = val & 0xFFFFu;
        if (!d->ready) { d->touched_before_ready = 1; }
        break;
    default:
        break;
    }
}

/* 读 MMIO 寄存器 */
static uint32_t mmio_read(Device *d, unsigned reg) {
    switch (reg) {
    case MMIO_STATUS: {
        /* 轮询期间时钟逐渐稳定：每读一次 STATUS 推进一拍锁定 */
        if (d->lock_ctr > 0) {
            d->lock_ctr--;
            if (d->lock_ctr == 0) d->ready = 1;
        }
        uint32_t s = 0;
        if (d->ready)            s |= ST_READY;
        if (!d->unlocked)        s |= ST_LOCKED;
        if (!clkdiv_valid(d))    s |= ST_BADCLK;
        if (!d->en)              s |= ST_NOTEN;
        return s;
    }
    case MMIO_DATA:
        if (!d->ready) {
            d->touched_before_ready = 1;
            return BADBOOT;                /* 未就绪误用 → 胡话 */
        }
        return dev_transform(d->data_raw);
    default:
        return 0;
    }
}

/* ── 你要填的 (1)：启动握手 ──────────────────────────────────────── */
/* boot_init：把设备从“半睡半醒”哄到 READY。四步：
 *   解锁 → 配 CLKDIV → 使能 → 轮询 STATUS.READY。
 */
static void boot_init(Device *d) {
    mmio_write(d, MMIO_UNLOCK, MAGIC);            /* ① 解锁配置总线 */
    mmio_write(d, MMIO_CLKDIV, 5u);               /* ② 合法分频 1..15 */
    mmio_write(d, MMIO_CTRL, CTRL_EN | CTRL_LE);  /* ③ 使能 */
    while (!(mmio_read(d, MMIO_STATUS) & ST_READY)) {
        /* ④ 忙等轮询 READY（PLL-lock 类比） */
    }
}

/* ── 应用程序：直接使用设备（假定 boot 已先跑好）── */
static Device g_dev;            /* 全局设备：静态零初始化 = 锁定、未就绪 */

static uint32_t app_main(Device *d) {
    mmio_write(d, MMIO_DATA, 0x42u);
    return mmio_read(d, MMIO_DATA);
}

/* ── 你要填的 (2)：把 boot_init 链接/排到 main 之前 ──────────────── */
/* boot_ctor 由 C 运行时在 main 之前调用（.init_array / 构造器机制）。
 * 让 boot_init 在这里先把 g_dev 哄就绪，app_main 才不会“抢跑”。
 */
__attribute__((constructor))
static void boot_ctor(void) {
    boot_init(&g_dev);          /* 先置位，后开工 */
}

/* ── 测试 harness（给定，勿改）──────────────────────────────────── */

/* 15.1 LOCK：观察“坏掉的启动”——未握手直接用 DATA 被正确拒。 */
static int stage_lock(void) {
    Device d; dev_reset(&d);
    uint32_t bad = mmio_read(&d, MMIO_DATA);   /* 跳过握手直接用 */
    uint32_t st  = mmio_read(&d, MMIO_STATUS);
    if (bad == BADBOOT && (st & ST_LOCKED)) {
        printf("LOCK_PASS\n");
        return 1;
    }
    printf("LOCK_FAIL 未握手应吐 0x%08X 且 LOCKED：got data=0x%08X status=0x%X\n", BADBOOT, bad, st);
    return 0;
}

/* 15.2 BOOT/USE：握手后就绪、读写 DATA 变换正确。 */
static int stage_boot_use(void) {
    Device d; dev_reset(&d);
    boot_init(&d);
    uint32_t st = mmio_read(&d, MMIO_STATUS);
    if (!(st & ST_READY)) {
        printf("BOOT_FAULT 握手后设备未就绪 status=0x%X\n", st);
        return 0;
    }
    printf("BOOT_PASS\n");
    mmio_write(&d, MMIO_DATA, 0x1234u);
    uint32_t r   = mmio_read(&d, MMIO_DATA);
    uint32_t exp = dev_transform(0x1234u);
    if (r != exp) {
        printf("USE_FAIL DATA 变换错 got=0x%X exp=0x%X\n", r, exp);
        return 0;
    }
    printf("USE_PASS\n");
    return 1;
}

/* 15.3 ORDER：boot 必须先于 app 跑。构造器已（应已）在 main 前置位 g_dev。 */
static int stage_order(void) {
    uint32_t r = app_main(&g_dev);
    if (!g_dev.ready || g_dev.touched_before_ready) {
        printf("BOOT_FAULT app_main 抢跑：首次 DATA 访问发生在 READY 之前\n");
        return 0;
    }
    uint32_t exp = dev_transform(0x42u);
    if (r != exp) {
        printf("ORDER_FAIL app 读回错 got=0x%X exp=0x%X\n", r, exp);
        return 0;
    }
    printf("ORDER_PASS\n");
    return 1;
}

int main(void) {
    int all = 1;
    all &= stage_lock();
    all &= stage_boot_use();
    all &= stage_order();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
