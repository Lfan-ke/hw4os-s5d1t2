/* 进程通信：原子操作、锁与「A 等 B 置位」的完成握手 —— C 参考解。
 *
 * 共享「控制字」(沿用 VLAN 的「包字」同构思路)，32-bit：
 *   [31]BUSY  [30]DONE  [29]LOCK  [28]START  [15:0]RESULT
 *
 * 四段逐题递进，全部建立在「共享状态 + 原子位」之上：
 *   1. done-bit 握手   —— B 干完置 DONE，A 死盯黑板，见 DONE 才取数。
 *   2. test_and_set 锁 —— 为什么「涂黑板」必须原子。
 *   3. 计数信号量      —— 把「一个位」推广到「N 个资源」。
 *   4. 编排 capstone   —— A 按门铃→等 DONE→做后续，B 见门铃→算→置位。
 *
 * 学生只需填 8 个函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

/* ── 控制字位布局 ── */
#define BUSY        (1u << 31)
#define DONE        (1u << 30)
#define LOCK        (1u << 29)
#define START       (1u << 28)
#define RESULT_MASK 0xFFFFu

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：8 个纯函数（硬件路径是同一字段的几个组合块）
 * ════════════════════════════════════════════════════════════════ */

/* ── 1. done-bit 握手 ── */

/* B 干完活：把 result 打进 [15:0]、置 DONE、清 BUSY。 */
static uint32_t b_finish(uint32_t result) {
    return DONE | (result & RESULT_MASK);
}

/* A 看黑板：返回 ready（DONE=1?），并把 [15:0] 写到 *result。 */
static int a_poll(uint32_t ctrl, uint32_t *result) {
    *result = ctrl & RESULT_MASK;
    return (ctrl & DONE) != 0;
}

/* ── 2. test_and_set 自旋锁 ── */

/* 原子 test-and-set：无条件把新值 1 写到 *newv，返回 got（旧值是否为 0）。对应 amoswap。 */
static int tas(uint32_t lock, uint32_t *newv) {
    *newv = 1;
    return lock == 0;
}

/* 释放锁：写 0。 */
static uint32_t unlock_op(void) {
    return 0;
}

/* 用 tas 拼出 try_lock：旧值写回新值（恒 1），返回是否抢到。 */
static int try_lock(uint32_t *lock) {
    uint32_t newv;
    int got = tas(*lock, &newv);
    *lock = newv;
    return got;
}

/* ── 3. 计数信号量 ── */

/* down(P)：count-1 写到 *out；返回 ok = count' >= 0。 */
static int down_sem(int count, int *out) {
    /* TODO[a] 阻塞式：恒减一，count' 变负即记录一个等待者，ok=false。 */
    int c = count - 1;
    *out = c;
    return c >= 0;
    /* ELSE[b] 自旋式：count>0 才减一并 ok=1，否则 *out=count 原样返回 ok=0。 */
}

/* up(V)：count+1。 */
static int up_sem(int count) {
    return count + 1;
}

/* ── 4. 编排 capstone ── */

/* B 的一步：见 START → 清 START、(置 BUSY 算完即清)、置 DONE、RESULT=job。 */
static uint32_t b_step(uint32_t ctrl, uint32_t job) {
    if (ctrl & START)
        return DONE | (job & RESULT_MASK);
    return ctrl;
}

/* A 的一步：phase=0 按门铃(置 START)→phase=1；
 * phase=1 见 DONE 才做后续(post=result*2)、清 DONE、回 phase=0；否则原地等。
 * 返回 ctrl'，并把新相位写 *phase_out、后续值写 *post_out。 */
static uint32_t a_step(uint32_t ctrl, uint32_t phase, uint32_t *phase_out, uint32_t *post_out) {
    if (phase == 0) {
        *phase_out = 1;
        *post_out = 0;
        return ctrl | START; /* 按门铃 */
    }
    if (ctrl & DONE) {
        /* 先用后清 DONE：取 result 算 post，再清 DONE 进下一轮。 */
        *post_out = (ctrl & RESULT_MASK) * 2;
        *phase_out = 0;
        return ctrl & ~DONE;
    }
    *phase_out = 1; /* B 没干完，A 死等 */
    *post_out = 0;
    return ctrl;
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* 给定的「非原子读-改-写」对照：示范丢更新。 */
static int naive_lost_update(void) {
    int shared = 0;
    int r0 = shared; /* proc0 读到 0 */
    int r1 = shared; /* proc1 也读到 0 */
    int w0 = r0 + 1;
    int w1 = r1 + 1; /* 覆盖写，丢一次自增 */
    (void)w0;
    return w1;
}

/* 双执行流握手所用的共享原子控制字与 B 线程。 */
static atomic_uint g_ctrl;

static void *b_thread(void *arg) {
    (void)arg;
    for (volatile int i = 0; i < 2000; i++) {
    }
    /* 先写 RESULT 再置 DONE：release 序保证 A 的 acquire 读到写全的字。 */
    atomic_store_explicit(&g_ctrl, b_finish(0xBEEFu), memory_order_release);
    return NULL;
}

static int check_handshake(void) {
    int ok = 1;
    uint32_t res;

    /* (a) B 运行中：DONE=0 + 垃圾 RESULT —— A 绝不能 ready。 */
    if (a_poll(BUSY | 0xDEADu, &res)) {
        printf("EARLY_FAIL A 在 DONE=0 时就绪了(读到垃圾值)\n");
        ok = 0;
    }
    /* (b) B 完成：DONE=1, RESULT=0x1234 —— A 必须 ready 且取数正确。 */
    int rdy = a_poll(b_finish(0x1234u), &res);
    if (!rdy || res != 0x1234u) {
        printf("HANDSHAKE_FAIL 完成态 ready=%d result=0x%04x 应=(1,0x1234)\n", rdy, res);
        ok = 0;
    }

    /* (c) 两个真执行流：B 线程置位，A 线程死盯黑板（有界自旋，防卡死）。 */
    atomic_store(&g_ctrl, BUSY);
    pthread_t tb;
    pthread_create(&tb, NULL, b_thread, NULL);
    uint32_t got = 0xFFFFFFFFu;
    int seen = 0;
    for (long i = 0; i < 50000000L; i++) {
        uint32_t c = atomic_load_explicit(&g_ctrl, memory_order_acquire);
        uint32_t v;
        if (a_poll(c, &v)) {
            got = v;
            seen = 1;
            break;
        }
    }
    pthread_join(tb, NULL);
    if (!seen || got != 0xBEEFu) {
        printf("HANDSHAKE_FAIL 双执行流握手 got=0x%08x 应=0xBEEF\n", got);
        ok = 0;
    }

    if (ok)
        printf("HANDSHAKE_PASS\n");
    return ok;
}

static int check_tas_mutex(void) {
    /* (a) tas 契约。 */
    int tok = 1;
    uint32_t n0, n1;
    int g0 = tas(0, &n0);
    int g1 = tas(1, &n1);
    if (n0 != 1 || !g0 || n1 != 1 || g1) {
        printf("TAS_FAIL tas(0)=(%u,%d) tas(1)=(%u,%d) 应=(1,1)/(1,0)\n", n0, g0, n1, g1);
        tok = 0;
    }
    if (unlock_op() != 0) {
        printf("TAS_FAIL unlock_op() 应=0\n");
        tok = 0;
    }
    if (tok)
        printf("TAS_PASS\n");

    /* (b) 给定交错调度：两个 proc 抢锁/放锁，断言临界区内 <= 1。 */
    int mok = 1;
    uint32_t lock = 0;
    int in_cs = 0;
    if (try_lock(&lock)) {
        in_cs++;
    } else {
        printf("MUTEX_FAIL proc0 抢空锁却失败\n");
        mok = 0;
    }
    if (in_cs > 1) {
        printf("DOUBLE_ENTER_FAIL 同时 %d 个在临界区\n", in_cs);
        mok = 0;
    }
    if (try_lock(&lock)) { /* proc1 持锁期间抢锁，必须失败 */
        in_cs++;
        if (in_cs > 1) {
            printf("DOUBLE_ENTER_FAIL proc1 闯入临界区，同时 %d 个\n", in_cs);
            mok = 0;
        }
    }
    lock = unlock_op();
    in_cs--;
    if (try_lock(&lock)) { /* proc1 重试，这次抢到 */
        in_cs++;
    } else {
        printf("MUTEX_FAIL proc1 在锁释放后仍抢不到\n");
        mok = 0;
    }
    if (in_cs > 1) {
        printf("DOUBLE_ENTER_FAIL 同时 %d 个在临界区\n", in_cs);
        mok = 0;
    }
    lock = unlock_op();
    in_cs--;
    if (in_cs != 0) {
        printf("MUTEX_FAIL 收尾时临界区计数=%d 应=0\n", in_cs);
        mok = 0;
    }
    (void)lock;

    printf("NAIVE_RACE 非原子读改写丢更新: got=%d expected=2\n", naive_lost_update());

    if (mok)
        printf("MUTEX_PASS\n");
    return tok && mok;
}

static int check_sem(void) {
    int ok = 1;
    int out;

    int ok2 = down_sem(2, &out);
    if (!ok2 || out != 1) {
        printf("SEM_FAIL down(2)=(%d,%d) 应=(1,1)\n", out, ok2);
        ok = 0;
    }
    int ok1 = down_sem(1, &out);
    if (!ok1 || out != 0) {
        printf("SEM_FAIL down(1)=(%d,%d) 应=(0,1)\n", out, ok1);
        ok = 0;
    }
    int ok0 = down_sem(0, &out);
    if (ok0) {
        printf("SEM_FAIL down(0) 空仓却返回 ok=1（应阻塞）\n");
        ok = 0;
    }
    if (up_sem(0) != 1 || up_sem(2) != 3) {
        printf("SEM_FAIL up(0)=%d up(2)=%d 应=1/3\n", up_sem(0), up_sem(2));
        ok = 0;
    }

    /* 不变式：2 个资源恰好发放 2 次；仅当 ok 才更新 count。 */
    int count = 2, grants = 0;
    for (int i = 0; i < 3; i++) {
        int c;
        if (down_sem(count, &c)) {
            count = c;
            grants++;
        }
    }
    if (grants != 2) {
        printf("SEM_FAIL 2 个资源却发放了 %d 次\n", grants);
        ok = 0;
    }
    count = up_sem(count);
    int c2;
    if (!down_sem(count, &c2)) {
        printf("SEM_FAIL up 之后队首仍拿不到资源\n");
        ok = 0;
    } else {
        count = c2;
    }
    (void)count;

    if (ok)
        printf("SEM_PASS\n");
    return ok;
}

static int check_orch(void) {
    int ok = 1;
    uint32_t jobs[4] = {7u, 21u, 100u, 3u};
    uint32_t ctrl = 0, phase = 0;

    for (int r = 0; r < 4; r++) {
        uint32_t job = jobs[r];
        uint32_t np, post;

        ctrl = a_step(ctrl, phase, &np, &post); /* A 按门铃 */
        phase = np;
        if (!(ctrl & START) || phase != 1) {
            printf("ORCH_FAIL r%d A 没按门铃(START 未置/相位错)\n", r);
            ok = 0;
        }
        /* A 提前轮询：B 还没干完，A 不得推进 */
        uint32_t ctrl2 = a_step(ctrl, phase, &np, &post);
        if (ctrl2 != ctrl || np != 1 || post != 0) {
            printf("ORCH_FAIL r%d A 在 B 完成前就推进了(乱序)\n", r);
            ok = 0;
        }
        /* B 干活、置位 */
        ctrl = b_step(ctrl, job);
        if (!(ctrl & DONE) || (ctrl & START) || (ctrl & RESULT_MASK) != job) {
            printf("ORCH_FAIL r%d B 未正确置位 ctrl=0x%08x\n", r, ctrl);
            ok = 0;
        }
        /* A 检测到 DONE，做后续 post=result*2，清 DONE 进下一轮 */
        ctrl = a_step(ctrl, phase, &np, &post);
        phase = np;
        if (post != job * 2 || (ctrl & DONE) || phase != 0) {
            printf("ORCH_FAIL r%d 后续值/收尾错 post=%u 应=%u\n", r, post, job * 2);
            ok = 0;
        }
    }

    if (ok)
        printf("ORCH_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_handshake();
    all &= check_tas_mutex();
    all &= check_sem();
    all &= check_orch();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
