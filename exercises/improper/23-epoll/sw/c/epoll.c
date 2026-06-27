/* I/O 多路复用：select / epoll / 边沿触发 / O(ready) 伸缩性 —— C。
 *
 * 心智模型：N 个「fd」各有一个就绪态(可读/不可读)。问「哪些 fd 现在能读」？
 *   · select —— 把全部 fd 从头扫到尾，凡可读就收集。O(n)。
 *   · epoll  —— epoll_ctl 注册兴趣集，内核在 fd「变就绪那一刻」挂上就绪链表；
 *               epoll_wait 只遍历就绪链表，O(ready)。
 *
 * 四段：1 SELECT 扫描全部找就绪集（已给好）；2 EPOLL 只返回已注册且就绪、未注册的不返回；
 *       3 EDGE 边沿触发 ET 只通知一次 vs 水平触发 LT 持续通知；
 *       4 SCALE 1 就绪/1000 fd，epoll O(ready) vs select O(n)。
 *
 * 你只需填 2 处（标 TODO）：epoll_wait 的「只收集就绪兴趣 fd」与 ET 的「边沿判定」。
 * 下方测试 harness 勿改。
 */
#include <stdio.h>
#include <string.h>

#define NMAX 1024

typedef struct {
    int readable;
} Fd;

/* 极简 epoll 实例：
 *   interest[fd] = -1 未注册；0 水平触发 LT；1 边沿触发 ET
 *   ready/rlen   = 就绪链表
 *   on_list      = 去重位图
 *   scan         = epoll_wait 检视过的 fd 数（证明 O(ready)）
 */
typedef struct {
    int interest[NMAX];
    int ready[NMAX];
    int rlen;
    int on_list[NMAX];
    long scan;
    int n;
} Epoll;

static void ep_init(Epoll *ep, int n) {
    ep->n = n;
    ep->rlen = 0;
    ep->scan = 0;
    for (int i = 0; i < n; i++) {
        ep->interest[i] = -1;
        ep->on_list[i] = 0;
    }
}

/* epoll_ctl(ADD)：et=1 边沿触发，et=0 水平触发。 */
static void ep_ctl_add(Epoll *ep, int fd, int et) {
    ep->interest[fd] = et;
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：2 处
 * ════════════════════════════════════════════════════════════════ */

/* 【填空 1 / ET 边沿判定】仅当「上一刻不可读、此刻可读」才算一次新事件(上升沿)。 */
static int edge_ready(int prev, int cur) {
    /* TODO: 返回「上升沿」——上一刻不可读、此刻可读。
     *   HINT: return !prev && cur;
     *   注意：true->true 不算沿(ET 静默)，false->true 才算沿(ET 通知一次)。 */
    (void)prev;
    (void)cur;
    return 0; /* ← 占位 */
}

/* 【填空 2 / epoll_wait 收集】只遍历就绪链表，收集「已注册兴趣 且 当前可读」的 fd，
 * 写入 out[]，返回个数。未注册的、或此刻不可读的都不收。 */
static int epoll_wait_op(Epoll *ep, Fd *fds, int *out) {
    int olen = 0;
    int keep[NMAX];
    int klen = 0;
    int pend[NMAX];
    int plen = ep->rlen;
    memcpy(pend, ep->ready, sizeof(int) * (size_t)plen);
    ep->rlen = 0;

    for (int i = 0; i < plen; i++) {
        int fd = pend[i];
        ep->scan++; /* 只数就绪链表长度 → O(ready) */
        ep->on_list[fd] = 0;
        int registered = ep->interest[fd] >= 0;
        int readable = fds[fd].readable;

        /* TODO: 只把「registered && readable」的 fd 收进 out（out[olen++]=fd）。
         *   未注册的、或此刻不可读的，都不收。 */
        (void)registered;
        (void)readable;

        /* LT 重新武装：仍可读就挂回；ET 不武装(等下次上升沿)。（已给好） */
        if (ep->interest[fd] == 0 && readable) {
            keep[klen++] = fd;
            ep->on_list[fd] = 1;
        }
    }
    memcpy(ep->ready, keep, sizeof(int) * (size_t)klen);
    ep->rlen = klen;
    return olen;
}

/* ════════════════════════════════════════════════════════════════
 * 内核侧 / 对照实现（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* 内核侧：设 fd 可读态为 val；若构成上升沿且已注册，挂上就绪链表。 */
static void set_readable(Fd *fds, Epoll *ep, int fd, int val) {
    int prev = fds[fd].readable;
    fds[fd].readable = val;
    if (ep->interest[fd] >= 0 && edge_ready(prev, val) && !ep->on_list[fd]) {
        ep->ready[ep->rlen++] = fd;
        ep->on_list[fd] = 1;
    }
}

/* select：扫描全部 n 个 fd，凡可读即收集。O(n)。 */
static int select_scan(Fd *fds, int n, int *out, long *counter) {
    int olen = 0;
    for (int i = 0; i < n; i++) {
        (*counter)++;
        if (fds[i].readable) {
            out[olen++] = i;
        }
    }
    return olen;
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int contains(const int *a, int n, int v) {
    for (int i = 0; i < n; i++) {
        if (a[i] == v) {
            return 1;
        }
    }
    return 0;
}

static int check_select(void) {
    int ok = 1;
    Fd fds[8];
    for (int i = 0; i < 8; i++) {
        fds[i].readable = 0;
    }
    fds[1].readable = fds[4].readable = fds[7].readable = 1;

    int out[8];
    long cnt = 0;
    int m = select_scan(fds, 8, out, &cnt);
    if (!(m == 3 && out[0] == 1 && out[1] == 4 && out[2] == 7)) {
        printf("SELECT_MISS 就绪集 len=%d 应=[1,4,7]\n", m);
        ok = 0;
    }
    if (cnt != 8) {
        printf("SELECT_BAD 扫描计数=%ld 应=8(全表都得看)\n", cnt);
        ok = 0;
    }

    Fd e[3];
    for (int i = 0; i < 3; i++) {
        e[i].readable = 0;
    }
    int o2[3];
    long c2 = 0;
    int m2 = select_scan(e, 3, o2, &c2);
    if (m2 != 0) {
        printf("SELECT_MISS 空集却返回 %d 个\n", m2);
        ok = 0;
    }

    if (ok) {
        printf("SELECT_PASS\n");
    }
    return ok;
}

static int check_epoll(void) {
    int ok = 1;
    Fd fds[8];
    for (int i = 0; i < 8; i++) {
        fds[i].readable = 0;
    }
    Epoll ep;
    ep_init(&ep, 8);
    ep_ctl_add(&ep, 2, 0);
    ep_ctl_add(&ep, 3, 0);
    ep_ctl_add(&ep, 5, 0);

    set_readable(fds, &ep, 3, 1);
    set_readable(fds, &ep, 5, 1);
    set_readable(fds, &ep, 6, 1); /* 未注册 */

    int out[8];
    int m = epoll_wait_op(&ep, fds, out);
    for (int i = 0; i < m; i++) {
        for (int j = i + 1; j < m; j++) {
            if (out[j] < out[i]) {
                int t = out[i];
                out[i] = out[j];
                out[j] = t;
            }
        }
    }
    if (!(m == 2 && out[0] == 3 && out[1] == 5)) {
        printf("EPOLL_MISS 返回 len=%d 应=[3,5]（只返回已注册且就绪）\n", m);
        ok = 0;
    }
    if (contains(out, m, 6)) {
        printf("EPOLL_BAD 未注册的 fd6 被错误返回\n");
        ok = 0;
    }
    if (contains(out, m, 2)) {
        printf("EPOLL_BAD 已注册但不可读的 fd2 被错误返回\n");
        ok = 0;
    }

    if (ok) {
        printf("EPOLL_PASS\n");
    }
    return ok;
}

static int check_edge(void) {
    int ok = 1;
    Fd fds[2];
    fds[0].readable = 0;
    fds[1].readable = 0;
    Epoll ep;
    ep_init(&ep, 2);
    ep_ctl_add(&ep, 0, 1); /* ET */
    ep_ctl_add(&ep, 1, 0); /* LT */

    int out[2];
    int m;

    /* (1) 首个上升沿：ET、LT 都通知。 */
    set_readable(fds, &ep, 0, 1);
    set_readable(fds, &ep, 1, 1);
    m = epoll_wait_op(&ep, fds, out);
    if (!(contains(out, m, 0) && contains(out, m, 1))) {
        printf("EDGE_MISS 首个上升沿 ET/LT 都应通知\n");
        ok = 0;
    }

    /* (2) 无新沿、仍可读：ET 静默，LT 继续通知。 */
    m = epoll_wait_op(&ep, fds, out);
    if (contains(out, m, 0)) {
        printf("EDGE_BAD ET 在无新沿时重复通知\n");
        ok = 0;
    }
    if (!contains(out, m, 1)) {
        printf("EDGE_MISS LT 仍可读时应持续通知\n");
        ok = 0;
    }

    /* (3) true->true 不构成沿：ET 仍静默，LT 仍通知。 */
    set_readable(fds, &ep, 0, 1);
    set_readable(fds, &ep, 1, 1);
    m = epoll_wait_op(&ep, fds, out);
    if (contains(out, m, 0)) {
        printf("EDGE_BAD ET 把 true->true 误判为沿\n");
        ok = 0;
    }
    if (!contains(out, m, 1)) {
        printf("EDGE_MISS LT 持续通知失效\n");
        ok = 0;
    }

    /* (4) 落沿再起沿：ET 又得到一次新沿，应再通知一次。 */
    set_readable(fds, &ep, 0, 0);
    set_readable(fds, &ep, 1, 0);
    set_readable(fds, &ep, 0, 1);
    set_readable(fds, &ep, 1, 1);
    m = epoll_wait_op(&ep, fds, out);
    if (!contains(out, m, 0)) {
        printf("EDGE_MISS 新上升沿后 ET 应再通知一次\n");
        ok = 0;
    }
    if (!contains(out, m, 1)) {
        printf("EDGE_MISS LT 应通知\n");
        ok = 0;
    }

    if (ok) {
        printf("EDGE_PASS\n");
    }
    return ok;
}

static Fd g_fds[NMAX];
static int g_out[NMAX];

static int check_scale(void) {
    int ok = 1;
    int n = 1000;
    for (int i = 0; i < n; i++) {
        g_fds[i].readable = 0;
    }
    Epoll ep;
    ep_init(&ep, n);
    for (int fd = 0; fd < n; fd++) {
        ep_ctl_add(&ep, fd, 1);
    }

    int target = 617;
    set_readable(g_fds, &ep, target, 1);

    long before = ep.scan;
    int m = epoll_wait_op(&ep, g_fds, g_out);
    long epoll_steps = ep.scan - before;
    if (!(m == 1 && g_out[0] == target)) {
        printf("SCALE_MISS epoll 返回 len=%d 应=[%d]\n", m, target);
        ok = 0;
    }
    if (epoll_steps != 1) {
        printf("SCALE_BAD epoll 检视了 %ld 个 fd 应=1\n", epoll_steps);
        ok = 0;
    }

    long sc = 0;
    int ms = select_scan(g_fds, n, g_out, &sc);
    if (!(ms == 1 && g_out[0] == target)) {
        printf("SCALE_MISS select 返回 len=%d 应=[%d]\n", ms, target);
        ok = 0;
    }
    if (sc != n) {
        printf("SCALE_BAD select 扫描了 %ld 应=%d\n", sc, n);
        ok = 0;
    }

    if (epoll_steps >= sc) {
        printf("SCALE_BAD epoll(%ld) 未优于 select(%ld)\n", epoll_steps, sc);
        ok = 0;
    }
    printf("SCALE_INFO epoll检视=%ld vs select扫描=%ld（O(ready) 完胜 O(n)）\n",
           epoll_steps, sc);

    if (ok) {
        printf("SCALE_PASS\n");
    }
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_select();
    all &= check_epoll();
    all &= check_edge();
    all &= check_scale();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
