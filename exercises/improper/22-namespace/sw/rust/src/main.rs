//! 容器隔离：namespace 切视图 + cgroup 卡配额 —— Rust。
//!
//! 心智模型：一颗内核，养出 N 个互相看不见的「小天地」(容器)。
//! 隔离不是魔法——只是给每个容器换一套**视图**，再给它记一本**配额账**：
//!
//!   1. PID namespace  —— 每个 ns 有独立 pid 编号。容器内 init 永远是 1 号；
//!                        宿主看到的同一个全局进程，在不同容器里编号不同。
//!   2. Mount namespace—— 每个容器各持一张挂载表；A 挂的盘 B 根本看不见。
//!   3. cgroup         —— 给容器记内存配额；申请超额就被拒，用量统计要准。
//!
//! 你只需填 4 个纯函数（标 TODO 处）；下方测试 harness（向量+断言+PASS 打印）勿改。
#![allow(unused_variables, dead_code)]

// ════════════════════════════════════════════════════════════════
// 学生填空区：4 个纯函数
// ════════════════════════════════════════════════════════════════

// ── 1. PID namespace：全局 pid ↔ ns 局部 pid 映射 ────────────────
// 约定：一个 PID namespace 持有「按加入顺序排的全局 pid 列表」globals。
//       globals[0] 是该 ns 的 init，容器内局部 pid 恒为 1；其后依次 2,3,...

/// 容器内视角：某全局 pid 在本 ns 看到的局部 pid（1 起）。不在本 ns 返回 None。
fn ns_local_pid(globals: &[u32], global: u32) -> Option<u32> {
    // TODO: 顺序扫 globals，找到等于 global 的下标 i，返回 Some(i+1)；扫完没有返回 None。
    // HINT: for (i, &g) in globals.iter().enumerate() { if g == global { return Some(i as u32 + 1); } }
    None // ← 占位
}

/// 逆映射：容器内局部 pid（1 起）→ 宿主全局 pid。越界返回 None。
fn ns_global_pid(globals: &[u32], local: u32) -> Option<u32> {
    // TODO: local 在 [1, globals.len()] 内则返回 Some(globals[local-1])，否则 None。
    // HINT: 注意局部 pid 从 1 起，对应下标 local-1。
    None // ← 占位
}

// ── 2. Mount namespace：每容器一张挂载表 ─────────────────────────

/// 在本 ns 的挂载表 paths 里查 target 的下标；未挂载(=不可见)返回 -1。
fn mount_lookup(paths: &[&str], target: &str) -> i32 {
    // TODO: 顺序扫 paths，p == target 时返回该下标(i as i32)；扫完没有返回 -1。
    -1 // ← 占位
}

// ── 3. cgroup：内存配额检查 ──────────────────────────────────────

/// 给容器记一笔内存申请：当前用量 used、配额 quota、本次申请 request。
/// 放得下(used+request <= quota)：批准，返回(used+request, true)；
/// 放不下：拒绝，返回(used, false)——用量原封不动（超额绝不能偷偷涨）。
fn cgroup_charge(used: u64, quota: u64, request: u64) -> (u64, bool) {
    // TODO: 比较 used+request 与 quota；放得下批准+计费，放不下拒绝且用量不变。
    // HINT: if used + request <= quota { (used + request, true) } else { (used, false) }
    (used, false) // ← 占位（永远拒绝，跑不出 CGROUP_PASS）
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

fn check_pidns() -> bool {
    let mut ok = true;

    // 宿主里有一票进程。两个容器各自圈一批进入自己的 PID namespace：
    //   ns_a：init=1000，再加 1001、7777          → 局部 1,2,3
    //   ns_b：init=2000，再加 7777                 → 局部 1,2
    // 注意全局 7777「同时」属于两个 ns（嵌套/共享场景）。
    let ns_a = [1000u32, 1001, 7777];
    let ns_b = [2000u32, 7777];

    // (a) 每个容器的 init 局部 pid 恒为 1。
    if ns_local_pid(&ns_a, 1000) != Some(1) {
        println!("PIDNS_BAD ns_a 的 init(1000) 局部 pid 应=1，得={:?}", ns_local_pid(&ns_a, 1000));
        ok = false;
    }
    if ns_local_pid(&ns_b, 2000) != Some(1) {
        println!("PIDNS_BAD ns_b 的 init(2000) 局部 pid 应=1，得={:?}", ns_local_pid(&ns_b, 2000));
        ok = false;
    }

    // (b) 关键：同一个全局进程 7777，在两个容器里看到的 pid **不同**。
    let la = ns_local_pid(&ns_a, 7777);
    let lb = ns_local_pid(&ns_b, 7777);
    if la != Some(3) {
        println!("PIDNS_BAD 全局 7777 在 ns_a 应=局部 3，得={:?}", la);
        ok = false;
    }
    if lb != Some(2) {
        println!("PIDNS_BAD 全局 7777 在 ns_b 应=局部 2，得={:?}", lb);
        ok = false;
    }
    if la == lb {
        println!("PIDNS_FAIL 隔离失效：同一全局进程在两 ns 看到相同 pid {:?}", la);
        ok = false;
    }

    // (c) 不在本 ns 的全局进程不可见。
    if ns_local_pid(&ns_a, 9999) != None {
        println!("PIDNS_BAD 不属于 ns_a 的 9999 不应可见");
        ok = false;
    }

    // (d) 逆映射自洽：局部 1 → init 全局 pid；越界 → None。
    if ns_global_pid(&ns_a, 1) != Some(1000) || ns_global_pid(&ns_b, 2) != Some(7777) {
        println!("PIDNS_BAD 逆映射错 ns_a[1]={:?} ns_b[2]={:?}", ns_global_pid(&ns_a, 1), ns_global_pid(&ns_b, 2));
        ok = false;
    }
    if ns_global_pid(&ns_a, 0) != None || ns_global_pid(&ns_b, 3) != None {
        println!("PIDNS_BAD 越界局部 pid 应=None");
        ok = false;
    }

    // (e) 往返一致：globals[i] 的全局 pid，局部化再全局化应回到自身。
    for ns in [&ns_a[..], &ns_b[..]] {
        for &g in ns {
            let l = ns_local_pid(ns, g);
            if l.is_none() || ns_global_pid(ns, l.unwrap()) != Some(g) {
                println!("PIDNS_FAIL 映射往返不一致 global={} local={:?}", g, l);
                ok = false;
            }
        }
    }

    if ok {
        println!("PIDNS_PASS");
    }
    ok
}

fn check_mountns() -> bool {
    let mut ok = true;

    // 两个容器各持一张挂载表。公共部分(/、/proc)都有，私有盘各不相同。
    let cont_a = ["/", "/proc", "/data-a"];
    let cont_b = ["/", "/proc", "/data-b"];

    // (a) 公共挂载点两边都可见。
    if mount_lookup(&cont_a, "/") < 0 || mount_lookup(&cont_b, "/") < 0 {
        println!("MOUNTNS_BAD 根挂载点 / 两容器都应可见");
        ok = false;
    }
    if mount_lookup(&cont_a, "/proc") < 0 || mount_lookup(&cont_b, "/proc") < 0 {
        println!("MOUNTNS_BAD /proc 两容器都应可见");
        ok = false;
    }

    // (b) 隔离：A 的私有盘 /data-a 在 A 可见、在 B 不可见；B 的 /data-b 反之。
    if mount_lookup(&cont_a, "/data-a") < 0 {
        println!("MOUNTNS_BAD /data-a 在容器 A 内应可见");
        ok = false;
    }
    if mount_lookup(&cont_b, "/data-a") != -1 {
        println!("MOUNTNS_FAIL 隔离失效：容器 B 看见了 A 的私有挂载 /data-a");
        ok = false;
    }
    if mount_lookup(&cont_b, "/data-b") < 0 {
        println!("MOUNTNS_BAD /data-b 在容器 B 内应可见");
        ok = false;
    }
    if mount_lookup(&cont_a, "/data-b") != -1 {
        println!("MOUNTNS_FAIL 隔离失效：容器 A 看见了 B 的私有挂载 /data-b");
        ok = false;
    }

    if ok {
        println!("MOUNTNS_PASS");
    }
    ok
}

fn check_cgroup() -> bool {
    let mut ok = true;

    // 配额 100 字节。依次申请 [60, 30, 20, 10]：
    //   60  → 0+60=60   <=100 批准，用量 60
    //   30  → 60+30=90  <=100 批准，用量 90
    //   20  → 90+20=110 >100  拒绝，用量仍 90（超额被拒）
    //   10  → 90+10=100 <=100 批准（刚好放下），用量 100
    let quota = 100u64;
    let reqs = [60u64, 30, 20, 10];
    let expect_grant = [true, true, false, true];
    let mut used = 0u64;
    let mut granted_total = 0u64;
    let mut rejections = 0;

    for (i, (&r, &eg)) in reqs.iter().zip(expect_grant.iter()).enumerate() {
        let before = used;
        let (nu, ok_grant) = cgroup_charge(used, quota, r);
        if ok_grant != eg {
            println!("CGROUP_BAD 第 {} 笔申请 {} 批准与否={} 应={}", i, r, ok_grant, eg);
            ok = false;
        }
        if ok_grant {
            // 批准：用量应精确加上申请额。
            if nu != before + r {
                println!("CGROUP_BAD 批准后用量={} 应={}", nu, before + r);
                ok = false;
            }
            granted_total += r;
        } else {
            // 拒绝：用量绝不能变（超额不得偷偷计费）。
            if nu != before {
                println!("CGROUP_FAIL 申请被拒用量却变了 {}→{}", before, nu);
                ok = false;
            }
            rejections += 1;
        }
        used = nu;
        // 任何时刻用量都不得越过配额。
        if used > quota {
            println!("CGROUP_FAIL 用量 {} 越过配额 {}", used, quota);
            ok = false;
        }
    }

    // 统计要准：恰好发生 1 次拒绝；最终用量 = 累计批准额 = 100（用满配额）。
    if rejections != 1 {
        println!("CGROUP_BAD 超额申请应恰好被拒 1 次，实际={}", rejections);
        ok = false;
    }
    if used != granted_total || used != 100 {
        println!("CGROUP_BAD 用量统计错 used={} granted={} 应=100", used, granted_total);
        ok = false;
    }

    if ok {
        println!("CGROUP_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_pidns();
    all &= check_mountns();
    all &= check_cgroup();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
