/* 容器隔离：namespace 切视图 + cgroup 卡配额 —— C。
 *
 * 心智模型：一颗内核，养出 N 个互相看不见的「小天地」(容器)。
 * 隔离不是魔法——只是给每个容器换一套视图，再给它记一本配额账：
 *
 *   1. PID namespace  —— 每个 ns 独立 pid 编号；容器内 init 永远是 1 号。
 *   2. Mount namespace—— 每个容器各持一张挂载表；A 挂的盘 B 看不见。
 *   3. cgroup         —— 给容器记内存配额；申请超额就被拒，统计要准。
 *
 * 你只需填 4 个纯函数（标 TODO 处）；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：4 个纯函数
 * ════════════════════════════════════════════════════════════════ */

/* ── 1. PID namespace：globals[0]=init(局部 1)，其后 2,3,... ── */

/* 容器内视角：某全局 pid 在本 ns 看到的局部 pid（1 起）。不在本 ns 返回 0。 */
static int ns_local_pid(const uint32_t *globals, int n, uint32_t global) {
    /* TODO: 顺序扫 globals[0..n)，找到等于 global 的下标 i，返回 i+1；没有返回 0。 */
    (void)globals; (void)n; (void)global;
    return 0; /* ← 占位 */
}

/* 逆映射：局部 pid（1 起）→ 宿主全局 pid 写到 *out；越界返回 0，否则 1。 */
static int ns_global_pid(const uint32_t *globals, int n, int local, uint32_t *out) {
    /* TODO: local 在 [1,n] 内则 *out=globals[local-1] 返回 1；否则返回 0。 */
    (void)globals; (void)n; (void)local; (void)out;
    return 0; /* ← 占位 */
}

/* ── 2. Mount namespace：每容器一张挂载表 ── */

/* 在本 ns 的挂载表 paths 里查 target 的下标；未挂载(不可见)返回 -1。 */
static int mount_lookup(const char *const *paths, int n, const char *target) {
    /* TODO: 顺序扫 paths，strcmp(paths[i],target)==0 时返回 i；没有返回 -1。 */
    (void)paths; (void)n; (void)target;
    return -1; /* ← 占位 */
}

/* ── 3. cgroup：内存配额检查 ── */

/* 内存申请：放得下(used+request<=quota)批准，*new_used=used+request 返回 1；
 * 放不下拒绝，*new_used=used（用量原封不动）返回 0。 */
static int cgroup_charge(uint64_t used, uint64_t quota, uint64_t request, uint64_t *new_used) {
    /* TODO: 比较 used+request 与 quota；放得下批准+计费，放不下拒绝且用量不变。 */
    (void)quota; (void)request;
    *new_used = used;
    return 0; /* ← 占位（永远拒绝，跑不出 CGROUP_PASS） */
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int check_pidns(void) {
    int ok = 1;
    uint32_t ns_a[3] = {1000u, 1001u, 7777u}; /* 局部 1,2,3 */
    uint32_t ns_b[2] = {2000u, 7777u};        /* 局部 1,2   */

    /* (a) 每个容器 init 局部 pid 恒为 1。 */
    if (ns_local_pid(ns_a, 3, 1000u) != 1) {
        printf("PIDNS_BAD ns_a 的 init(1000) 局部 pid 应=1，得=%d\n", ns_local_pid(ns_a, 3, 1000u));
        ok = 0;
    }
    if (ns_local_pid(ns_b, 2, 2000u) != 1) {
        printf("PIDNS_BAD ns_b 的 init(2000) 局部 pid 应=1，得=%d\n", ns_local_pid(ns_b, 2, 2000u));
        ok = 0;
    }

    /* (b) 关键：同一全局进程 7777 在两容器看到的 pid 不同。 */
    int la = ns_local_pid(ns_a, 3, 7777u);
    int lb = ns_local_pid(ns_b, 2, 7777u);
    if (la != 3) {
        printf("PIDNS_BAD 全局 7777 在 ns_a 应=局部 3，得=%d\n", la);
        ok = 0;
    }
    if (lb != 2) {
        printf("PIDNS_BAD 全局 7777 在 ns_b 应=局部 2，得=%d\n", lb);
        ok = 0;
    }
    if (la == lb) {
        printf("PIDNS_FAIL 隔离失效：同一全局进程在两 ns 看到相同 pid %d\n", la);
        ok = 0;
    }

    /* (c) 不在本 ns 的全局进程不可见(=0)。 */
    if (ns_local_pid(ns_a, 3, 9999u) != 0) {
        printf("PIDNS_BAD 不属于 ns_a 的 9999 不应可见\n");
        ok = 0;
    }

    /* (d) 逆映射自洽。 */
    uint32_t g1, g2;
    if (!ns_global_pid(ns_a, 3, 1, &g1) || g1 != 1000u ||
        !ns_global_pid(ns_b, 2, 2, &g2) || g2 != 7777u) {
        printf("PIDNS_BAD 逆映射错\n");
        ok = 0;
    }
    uint32_t dummy;
    if (ns_global_pid(ns_a, 3, 0, &dummy) || ns_global_pid(ns_b, 2, 3, &dummy)) {
        printf("PIDNS_BAD 越界局部 pid 应失败\n");
        ok = 0;
    }

    /* (e) 往返一致：global → local → global 应回到自身。 */
    uint32_t *all[2] = {ns_a, ns_b};
    int lens[2] = {3, 2};
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < lens[k]; i++) {
            uint32_t g = all[k][i];
            int l = ns_local_pid(all[k], lens[k], g);
            uint32_t back;
            if (l == 0 || !ns_global_pid(all[k], lens[k], l, &back) || back != g) {
                printf("PIDNS_FAIL 映射往返不一致 global=%u local=%d\n", g, l);
                ok = 0;
            }
        }
    }

    if (ok)
        printf("PIDNS_PASS\n");
    return ok;
}

static int check_mountns(void) {
    int ok = 1;
    const char *cont_a[3] = {"/", "/proc", "/data-a"};
    const char *cont_b[3] = {"/", "/proc", "/data-b"};

    /* (a) 公共挂载点两边都可见。 */
    if (mount_lookup(cont_a, 3, "/") < 0 || mount_lookup(cont_b, 3, "/") < 0) {
        printf("MOUNTNS_BAD 根挂载点 / 两容器都应可见\n");
        ok = 0;
    }
    if (mount_lookup(cont_a, 3, "/proc") < 0 || mount_lookup(cont_b, 3, "/proc") < 0) {
        printf("MOUNTNS_BAD /proc 两容器都应可见\n");
        ok = 0;
    }

    /* (b) 隔离：私有盘互不可见。 */
    if (mount_lookup(cont_a, 3, "/data-a") < 0) {
        printf("MOUNTNS_BAD /data-a 在容器 A 内应可见\n");
        ok = 0;
    }
    if (mount_lookup(cont_b, 3, "/data-a") != -1) {
        printf("MOUNTNS_FAIL 隔离失效：容器 B 看见了 A 的私有挂载 /data-a\n");
        ok = 0;
    }
    if (mount_lookup(cont_b, 3, "/data-b") < 0) {
        printf("MOUNTNS_BAD /data-b 在容器 B 内应可见\n");
        ok = 0;
    }
    if (mount_lookup(cont_a, 3, "/data-b") != -1) {
        printf("MOUNTNS_FAIL 隔离失效：容器 A 看见了 B 的私有挂载 /data-b\n");
        ok = 0;
    }

    if (ok)
        printf("MOUNTNS_PASS\n");
    return ok;
}

static int check_cgroup(void) {
    int ok = 1;
    uint64_t quota = 100;
    uint64_t reqs[4] = {60, 30, 20, 10};
    int expect_grant[4] = {1, 1, 0, 1};
    uint64_t used = 0, granted_total = 0;
    int rejections = 0;

    for (int i = 0; i < 4; i++) {
        uint64_t before = used, nu;
        int g = cgroup_charge(used, quota, reqs[i], &nu);
        if (g != expect_grant[i]) {
            printf("CGROUP_BAD 第 %d 笔申请 %llu 批准与否=%d 应=%d\n",
                   i, (unsigned long long)reqs[i], g, expect_grant[i]);
            ok = 0;
        }
        if (g) {
            if (nu != before + reqs[i]) {
                printf("CGROUP_BAD 批准后用量=%llu 应=%llu\n",
                       (unsigned long long)nu, (unsigned long long)(before + reqs[i]));
                ok = 0;
            }
            granted_total += reqs[i];
        } else {
            if (nu != before) {
                printf("CGROUP_FAIL 申请被拒用量却变了 %llu->%llu\n",
                       (unsigned long long)before, (unsigned long long)nu);
                ok = 0;
            }
            rejections++;
        }
        used = nu;
        if (used > quota) {
            printf("CGROUP_FAIL 用量 %llu 越过配额 %llu\n",
                   (unsigned long long)used, (unsigned long long)quota);
            ok = 0;
        }
    }

    if (rejections != 1) {
        printf("CGROUP_BAD 超额申请应恰好被拒 1 次，实际=%d\n", rejections);
        ok = 0;
    }
    if (used != granted_total || used != 100) {
        printf("CGROUP_BAD 用量统计错 used=%llu granted=%llu 应=100\n",
               (unsigned long long)used, (unsigned long long)granted_total);
        ok = 0;
    }

    if (ok)
        printf("CGROUP_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_pidns();
    all &= check_mountns();
    all &= check_cgroup();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
