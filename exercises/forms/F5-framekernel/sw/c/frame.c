/* 形态认知 · F5 框内核(framekernel, Asterinas) —— C 近似。
 *
 * framekernel 的本质：把 TCB 收敛成最小「框架」，框架内把资源包成**安全 API**；
 * 其余子系统经该 API 访问，靠**类型系统**(而非 MMU)拿到隔离。
 *
 * 但 C 没有 borrow checker、没有 `#![forbid(unsafe_code)]`：没有任何编译期机制能阻止
 * 子系统把句柄强转成裸指针越权。所以 C 只能做「**约定式封装**」(opaque handle +
 * 受控访问函数 + 运行时审计计数器) 来近似——这正是与 Rust framekernel 的差距
 * （类比：C 也没有 async/await，协程只能靠手写状态机约定来模拟）。essay 详述。
 *
 * 你只需填 frame_write / frame_read 两个安全 API（边界检查 + 计数 + 裸访问）。
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define FRAME_OK  0
#define FRAME_OOB (-1)

/* ── 运行时审计：裸内存访问按「层」记账 ── */
static unsigned long g_tcb_raw_ops = 0;    /* 框架内裸内存访问次数（应 > 0） */
static unsigned long g_subsys_raw_ops = 0; /* 子系统内裸内存访问次数（约定应恒为 0） */

/* ════════════════════════════════════════════════════════════════
 * OS Framework（TCB）：唯一直接碰裸内存的层。
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *base;
    size_t   len;
} Frame;

typedef struct {
    uint8_t buf[256];
    size_t  size;
} Pool;

static void pool_init(Pool *p, size_t size) {
    p->size = size;
    for (size_t i = 0; i < size; i++) {
        p->buf[i] = 0;
    }
}

static Frame pool_carve(Pool *p, size_t base, size_t len) {
    Frame f;
    f.base = p->buf + base;
    f.len = len;
    return f;
}

/* 安全写：你来填。 */
static int frame_write(Frame *f, size_t i, uint8_t v) {
    /* TODO: 1) 边界检查 —— if (i >= f->len) return FRAME_OOB;
     *       2) g_tcb_raw_ops++;   记一次「框架内」裸访问
     *       3) f->base[i] = v;    框架内唯一裸写
     * 缺了第 1 步，越界写会越权改坏邻居 Frame（见 TYPESAFE 子题）；
     * 缺了第 2 步，MINTCB 审计统计不到框架裸访问。 */
    f->base[i] = v; /* 占位：缺边界检查与计数 */
    return FRAME_OK;
}

/* 安全读：你来填（同上：边界检查 + 计数 + 裸读到 *out）。 */
static int frame_read(Frame *f, size_t i, uint8_t *out) {
    /* TODO: if (i >= f->len) return FRAME_OOB; g_tcb_raw_ops++; *out = f->base[i]; */
    *out = f->base[i]; /* 占位 */
    return FRAME_OK;
}

static uint8_t pool_peek(Pool *p, size_t i) {
    return p->buf[i];
}

/* ════════════════════════════════════════════════════════════════
 * OS Services：子系统。**约定**只走安全 API，从不直接碰裸内存。
 * ════════════════════════════════════════════════════════════════ */

static int sub_store(Frame *f, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (frame_write(f, i, data[i]) != FRAME_OK) {
            return FRAME_OOB;
        }
    }
    return FRAME_OK;
}

static int sub_load(Frame *f, uint8_t *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (frame_read(f, i, &out[i]) != FRAME_OK) {
            return FRAME_OOB;
        }
    }
    return FRAME_OK;
}

static int sub_try_overreach(Frame *f, size_t far) {
    return frame_write(f, far, 0xFF);
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int check_frame(void) {
    int ok = 1;
    Pool pool;
    pool_init(&pool, 128);
    Frame frame = pool_carve(&pool, 0, 64);

    uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};
    if (sub_store(&frame, data, 4) != FRAME_OK) {
        printf("FRAME_MISS 安全 API 写入合法下标却被拒\n");
        ok = 0;
    }
    uint8_t back[4] = {0};
    if (sub_load(&frame, back, 4) != FRAME_OK) {
        printf("FRAME_MISS 安全 API 读取合法下标却被拒\n");
        ok = 0;
    }
    for (size_t i = 0; i < 4; i++) {
        if (back[i] != data[i]) {
            printf("FRAME_MISS 安全 API 往返不一致 i=%zu got=0x%02x 应=0x%02x\n",
                   i, back[i], data[i]);
            ok = 0;
        }
    }
    if (ok) {
        printf("FRAME_PASS\n");
    }
    return ok;
}

static int check_typesafe(void) {
    int ok = 1;
    Pool pool;
    pool_init(&pool, 128);
    Frame frame_a = pool_carve(&pool, 0, 64);
    Frame frame_b = pool_carve(&pool, 64, 64);

    uint8_t bmark[4] = {0xB0, 0xB1, 0xB2, 0xB3};
    if (sub_store(&frame_b, bmark, 4) != FRAME_OK) {
        printf("TYPESAFE_MISS B 写入自己的 Frame 失败\n");
        ok = 0;
    }

    if (sub_try_overreach(&frame_a, 64) == FRAME_OK) {
        printf("TYPESAFE_MISS A 越界写被放行（边界检查缺失，越权改坏了邻居）\n");
        ok = 0;
    }
    if (pool_peek(&pool, 64) != 0xB0) {
        printf("TYPESAFE_MISS 邻居 B[0] 被改坏 pool[64]=0x%02x 应=0xB0\n",
               pool_peek(&pool, 64));
        ok = 0;
    }
    if (sub_try_overreach(&frame_a, 63) != FRAME_OK) {
        printf("TYPESAFE_MISS A 在自己合法范围(下标 63)内的写被误拒\n");
        ok = 0;
    }
    if (ok) {
        printf("TYPESAFE_PASS\n");
    }
    return ok;
}

static int check_mintcb(void) {
    int ok = 1;
    printf("AUDIT framework_raw_ops=%lu subsystem_raw_ops=%lu\n",
           g_tcb_raw_ops, g_subsys_raw_ops);
    if (g_subsys_raw_ops != 0) {
        printf("MINTCB_MISS 子系统发生了 %lu 次裸内存访问（约定应为 0）\n",
               g_subsys_raw_ops);
        ok = 0;
    }
    if (g_tcb_raw_ops == 0) {
        printf("MINTCB_MISS 框架层未记录任何裸访问（说明计数/封装没接上）\n");
        ok = 0;
    }
    if (ok) {
        printf("MINTCB_PASS\n");
    }
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_frame();
    all &= check_typesafe();
    all &= check_mintcb();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
