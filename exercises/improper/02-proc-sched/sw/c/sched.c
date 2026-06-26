/* 进程管理（软件模拟调度）—— C。
 * 调度 = 存储结构（谁在就绪）+ 选择策略（选谁）。三段递进。
 * 你只需填三个函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

#define GHOST 1u
#define TAG_A 2u
#define TAG_B 3u
#define MAXP  16

typedef struct { uint32_t pid, prio, tag; } Proc;

/* ── 三段核心逻辑（学生填）── */

/* FIFO：按到达顺序写入 out，返回个数。 */
static int run_fifo(const Proc *p, int n, uint32_t *out) {
    (void)p; (void)n; (void)out;
    /* TODO: for i in [0,n): out[i] = p[i].pid; 返回 n。 */
    return 0; /* ← 占位 */
}

/* 约束调度：去掉 GHOST；保证 B 恒在 A 之后；其余各一次。返回个数。 */
static int run_constrained(const Proc *p, int n, uint32_t *out) {
    (void)p; (void)n; (void)out;
    /* TODO: 先把 tag!=GHOST 的 pid 收进 out；再保证 TAG_B 的 pid 排在 TAG_A 的 pid 之后。
     * HINT: 若 B 在 A 前，删 B 再插到 A 之后。 */
    return 0; /* ← 占位 */
}

/* 优先级调度：高优先先出队，平级按到达顺序。返回个数。 */
static int run_priority(const Proc *p, int n, uint32_t *out) {
    (void)p; (void)n; (void)out;
    /* TODO: 对下标做稳定排序（prio 降序，平级保持到达顺序），按序写 out[i]=p[idx[i]].pid。 */
    return 0; /* ← 占位 */
}

/* ── 测试 harness（勿改）── */

static int arrival_index(const Proc *p, int n, uint32_t pid) {
    for (int i = 0; i < n; i++) if (p[i].pid == pid) return i;
    return 1 << 30;
}

static int check_fifo(const Proc *p, int n, const uint32_t *out, int m) {
    if (m != n) { printf("FIFO_FAIL 个数=%d 应=%d\n", m, n); return 0; }
    for (int i = 0; i < n; i++)
        if (out[i] != p[i].pid) { printf("FIFO_FAIL pos%d=%u 应=%u\n", i, out[i], p[i].pid); return 0; }
    printf("FIFO_PASS\n"); return 1;
}

static int check_constrained(const Proc *p, int n, const uint32_t *out, int m) {
    int ok = 1;
    for (int i = 0; i < n; i++) if (p[i].tag == GHOST)
        for (int j = 0; j < m; j++) if (out[j] == p[i].pid) { printf("SCHED_FAIL ghost pid=%u\n", p[i].pid); ok = 0; }
    for (int i = 0; i < n; i++) if (p[i].tag != GHOST) {
        int c = 0; for (int j = 0; j < m; j++) if (out[j] == p[i].pid) c++;
        if (c != 1) { printf("SCHED_FAIL pid=%u 出现 %d 次\n", p[i].pid, c); ok = 0; }
    }
    uint32_t a = 0, b = 0; int hasA = 0, hasB = 0;
    for (int i = 0; i < n; i++) { if (p[i].tag == TAG_A){a=p[i].pid;hasA=1;} if (p[i].tag == TAG_B){b=p[i].pid;hasB=1;} }
    if (hasA && hasB) {
        int pa = -1, pb = -1;
        for (int j = 0; j < m; j++) { if (out[j]==a) pa=j; if (out[j]==b) pb=j; }
        if (!(pa >= 0 && pb >= 0 && pa < pb)) { printf("SCHED_FAIL B(pid=%u) 必须在 A(pid=%u) 之后\n", b, a); ok = 0; }
    }
    if (ok) printf("SCHED_PASS\n");
    return ok;
}

static int check_priority(const Proc *p, int n, const uint32_t *out, int m) {
    int ok = 1;
    if (m != n) { printf("PRIO_FAIL 应调度全部 %d 个\n", n); ok = 0; }
    for (int i = 1; i < m; i++) {
        uint32_t pa = 0, pb = 0;
        for (int k = 0; k < n; k++) { if (p[k].pid==out[i-1]) pa=p[k].prio; if (p[k].pid==out[i]) pb=p[k].prio; }
        if (pb > pa) { printf("PRIO_FAIL 优先级非单调 pid=%u 在 pid=%u 后\n", out[i], out[i-1]); ok = 0; }
        else if (pb == pa && arrival_index(p,n,out[i]) < arrival_index(p,n,out[i-1])) {
            printf("PRIO_FAIL 平级未按到达顺序 pid=%u 应在 pid=%u 前\n", out[i], out[i-1]); ok = 0;
        }
    }
    if (ok) printf("PRIO_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    uint32_t out[MAXP]; int m;

    Proc fifo[] = { {10,0,0},{20,0,0},{30,0,0},{40,0,0} };
    m = run_fifo(fifo, 4, out); all &= check_fifo(fifo, 4, out, m);

    Proc con[] = { {1,0,0},{2,0,GHOST},{3,0,TAG_B},{4,0,TAG_A},{5,0,0} };
    m = run_constrained(con, 5, out); all &= check_constrained(con, 5, out, m);

    Proc pri[] = { {10,1,0},{20,3,0},{30,2,0},{40,3,0} };
    m = run_priority(pri, 4, out); all &= check_priority(pri, 4, out, m);

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
