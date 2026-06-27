//! 形态认知 · F2 微内核（microkernel）—— Rust。
//!
//! 本质权衡：把 fs / 驱动 / 网络全部赶出内核态，内核只剩三件事——
//!   **IPC（消息路由）+ 调度 + 能力（capability）管理**。
//! 用户态服务彼此隔离，只能「持有一张能力券 + 发一条消息」来互相访问；
//! 内核是唯一的中间人，负责校验能力、转发消息、把崩掉的服务标记下线。
//!
//! 这个 host demo 用最朴素的软件模型把它演出来：
//!   - 一张「能力表」: Cap { rights, target } —— 不可伪造的对象引用 + 权利位。
//!   - 一个「内核」kernel_call(): 先 cap_check 校验，再 dispatch 路由。
//!   - 两个「用户态服务」: echo（回显）/ store（键值存储），只能经内核访问。
//!
//! 三个判据：
//!   MICRO_PASS   —— 经消息往返，echo/store 语义正确（IPC 通了）。
//!   CAP_PASS     —— 没有能力 / 缺权利位的访问，被内核拒绝（不可绕过内核）。
//!   ISOLATE_PASS —— 一个服务「崩溃」被标记下线，内核与另一个服务仍存活。
//!
//! 你只需填两个函数：`cap_check`（能力校验）与 `dispatch`（消息分发，标 TODO 处）。
//! 下方测试 harness 勿改。
#![allow(unused_variables, dead_code)]

// ── 服务身份（CSpace 里 target 字段的取值）─────────────────────────
const SVC_ECHO: usize = 0; // 回显服务
const SVC_STORE: usize = 1; // 键值存储服务
const SVC_NONE: usize = 2; // 哨兵：空能力槽（指向「无」）
const N_SVC: usize = 2;

// ── 权利位（rights）：能力券上盖的章 ───────────────────────────────
const RIGHT_SEND: u32 = 1 << 0; // 允许向 target 发消息（无此位即「只读引用」）

// ── 消息 opcode ────────────────────────────────────────────────────
const OP_ECHO: u32 = 0; // echo：回显 val
const OP_PUT: u32 = 1; // store：写槽 store[key]=val
const OP_GET: u32 = 2; // store：读槽 -> store[key]

const STORE_SLOTS: usize = 4;

/// 能力（capability）：一张「不可伪造的对象引用 + 权利位」。
/// 与 POSIX fd 的关键区别：fd 只是个进程私有的数组下标（环境权威）；
/// cap 自带 rights，且只能由内核派生/传递，用户态伪造不出来。
#[derive(Clone, Copy)]
struct Cap {
    rights: u32,
    target: usize,
}

/// 空能力槽：什么都指不到。
const NO_CAP: Cap = Cap {
    rights: 0,
    target: SVC_NONE,
};

/// 一条 IPC 消息。
#[derive(Clone, Copy)]
struct Msg {
    op: u32,
    key: u32,
    val: u32,
}

/// 内核回复的状态码（Ok 表示放行；其余是内核拒绝的理由）。
#[derive(Clone, Copy, PartialEq, Debug)]
enum Status {
    Ok,
    DenyNoCap,   // 空能力槽：根本没拿到券
    DenyNoRight, // 有券但缺 SEND 权利位
    DenyOffline, // 目标服务已崩溃下线
}

/// 内核给调用方的回复。
#[derive(Clone, Copy)]
struct Reply {
    status: Status,
    val: u32,
}

/// 「世界」状态：服务存活标记 + store 服务的内部数据。
struct World {
    alive: [bool; N_SVC],
    store: [u32; STORE_SLOTS],
}

fn fresh_world() -> World {
    World {
        alive: [true; N_SVC],
        store: [0; STORE_SLOTS],
    }
}

// ════════════════════════════════════════════════════════════════
// 给定的用户态服务实现（harness 的一部分，勿改）
// ════════════════════════════════════════════════════════════════

/// echo 服务：回显 val（最朴素的 RPC）。
fn echo_service(msg: &Msg) -> u32 {
    msg.val
}

/// store 服务：一块私有的小存储，PUT 写槽、GET 读槽。
fn store_service(msg: &Msg, world: &mut World) -> u32 {
    let k = (msg.key as usize) % STORE_SLOTS;
    match msg.op {
        OP_PUT => {
            world.store[k] = msg.val;
            msg.val
        }
        _ => world.store[k], // OP_GET / 其它一律当读
    }
}

// ════════════════════════════════════════════════════════════════
// 学生填空区：能力校验 + 消息分发（微内核的两件核心活）
// ════════════════════════════════════════════════════════════════

/// 内核能力校验：决定一次调用是否放行。返回 Status::Ok 表示放行，否则给出拒绝理由。
/// 规则（依次判定，顺序固定）：
///   ① cap.target == SVC_NONE（空能力槽）            → Status::DenyNoCap
///   ② cap.rights 缺 RIGHT_SEND 位（券上没盖发送章） → Status::DenyNoRight
///   ③ world.alive[cap.target] == false（服务已下线）→ Status::DenyOffline
///   ④ 以上都过                                       → Status::Ok
fn cap_check(cap: &Cap, world: &World) -> Status {
    // TODO: 依次做上面四步判定。
    // HINT:
    //   if cap.target == SVC_NONE { return Status::DenyNoCap; }
    //   if cap.rights & RIGHT_SEND == 0 { return Status::DenyNoRight; }
    //   if !world.alive[cap.target] { return Status::DenyOffline; }
    //   Status::Ok
    Status::Ok // ← 占位：恒放行 → CAP / ISOLATE 都过不了
}

/// 消息分发：把已放行的消息路由到对应用户态服务，返回服务回复的值。
///   SVC_ECHO  → echo_service(msg)
///   SVC_STORE → store_service(msg, world)
///   其它      → 0（理论上不可达，因为已被 cap_check 放行）
fn dispatch(target: usize, msg: &Msg, world: &mut World) -> u32 {
    // TODO: 按 target 路由到对应服务。
    // HINT:
    //   match target {
    //       SVC_ECHO => echo_service(msg),
    //       SVC_STORE => store_service(msg, world),
    //       _ => 0,
    //   }
    0 // ← 占位：恒返回 0 → MICRO 过不了
}

// ════════════════════════════════════════════════════════════════
// 内核入口（给定，勿改）：能力校验在前，消息分发在后。
// 这正是微内核的不变量——任何对服务的访问都必须穿过内核这一关。
// ════════════════════════════════════════════════════════════════
fn kernel_call(cap: &Cap, msg: &Msg, world: &mut World) -> Reply {
    let verdict = cap_check(cap, world);
    if verdict != Status::Ok {
        // 拒绝：根本不碰服务，副作用为零（capability 安全的本质）。
        return Reply {
            status: verdict,
            val: 0,
        };
    }
    let v = dispatch(cap.target, msg, world);
    Reply {
        status: Status::Ok,
        val: v,
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 判据 1：消息往返语义正确。
fn check_micro() -> bool {
    let mut ok = true;
    let mut world = fresh_world();
    let echo_cap = Cap {
        rights: RIGHT_SEND,
        target: SVC_ECHO,
    };
    let store_cap = Cap {
        rights: RIGHT_SEND,
        target: SVC_STORE,
    };

    // (a) echo 往返：发 0xABCD，原样回来。
    let r = kernel_call(
        &echo_cap,
        &Msg {
            op: OP_ECHO,
            key: 0,
            val: 0xABCD,
        },
        &mut world,
    );
    if r.status != Status::Ok || r.val != 0xABCD {
        println!(
            "micro: echo 往返对不上 status={:?} val=0x{:04x} 应=(Ok,0xABCD)",
            r.status, r.val
        );
        ok = false;
    }

    // (b) store 往返：PUT 槽 2 = 0x0055，再 GET 槽 2 应拿回 0x0055。
    let p = kernel_call(
        &store_cap,
        &Msg {
            op: OP_PUT,
            key: 2,
            val: 0x0055,
        },
        &mut world,
    );
    let g = kernel_call(
        &store_cap,
        &Msg {
            op: OP_GET,
            key: 2,
            val: 0,
        },
        &mut world,
    );
    if p.status != Status::Ok || g.status != Status::Ok || g.val != 0x0055 {
        println!(
            "micro: store 往返对不上 put={:?} get=({:?},0x{:04x}) 应 get=(Ok,0x0055)",
            p.status, g.status, g.val
        );
        ok = false;
    }

    if ok {
        println!("MICRO_PASS");
    }
    ok
}

/// 判据 2：无能力 / 缺权利位的访问被内核拒绝，且服务零副作用。
fn check_cap() -> bool {
    let mut ok = true;
    let mut world = fresh_world();
    let echo_msg = Msg {
        op: OP_ECHO,
        key: 0,
        val: 0x1234,
    };

    // (a) 空能力槽：内核必须拒（DenyNoCap），不得放行。
    let r1 = kernel_call(&NO_CAP, &echo_msg, &mut world);
    if r1.status != Status::DenyNoCap {
        println!(
            "cap: 空能力却被放行 status={:?} 应=DenyNoCap",
            r1.status
        );
        ok = false;
    }

    // (b) 有券但缺 SEND 权利位：内核必须拒（DenyNoRight）。
    let weak = Cap {
        rights: 0,
        target: SVC_ECHO,
    };
    let r2 = kernel_call(&weak, &echo_msg, &mut world);
    if r2.status != Status::DenyNoRight {
        println!(
            "cap: 缺权利位却被放行 status={:?} 应=DenyNoRight",
            r2.status
        );
        ok = false;
    }

    // (c) 关键不变量：被拒的 PUT 绝不能改到 store（无能力 = 零副作用）。
    let weak_store = Cap {
        rights: 0,
        target: SVC_STORE,
    };
    world.store[1] = 0;
    let r3 = kernel_call(
        &weak_store,
        &Msg {
            op: OP_PUT,
            key: 1,
            val: 0x99,
        },
        &mut world,
    );
    if r3.status != Status::DenyNoRight {
        println!("cap: 无权 PUT 却被放行 status={:?} 应=DenyNoRight", r3.status);
        ok = false;
    }
    if world.store[1] != 0 {
        println!(
            "cap: 越权写穿透了内核，store[1]=0x{:x} 应=0（能力门形同虚设）",
            world.store[1]
        );
        ok = false;
    }

    if ok {
        println!("CAP_PASS");
    }
    ok
}

/// 判据 3：一个服务崩溃被标记下线，内核与另一个服务仍存活。
fn check_isolate() -> bool {
    let mut ok = true;
    let mut world = fresh_world();
    let echo_cap = Cap {
        rights: RIGHT_SEND,
        target: SVC_ECHO,
    };
    let store_cap = Cap {
        rights: RIGHT_SEND,
        target: SVC_STORE,
    };

    // 崩溃前两个服务都正常。
    let pre = kernel_call(
        &echo_cap,
        &Msg {
            op: OP_ECHO,
            key: 0,
            val: 0x7,
        },
        &mut world,
    );
    if pre.status != Status::Ok {
        println!("isolate: 崩溃前 echo 就不正常 status={:?}", pre.status);
        ok = false;
    }

    // —— echo 服务「崩溃」：内核把它标记下线（用户态故障不拖垮内核）。——
    world.alive[SVC_ECHO] = false;

    // (a) 再调 echo：内核存活、正常返回拒绝（DenyOffline），而不是自己崩。
    let r = kernel_call(
        &echo_cap,
        &Msg {
            op: OP_ECHO,
            key: 0,
            val: 0x7,
        },
        &mut world,
    );
    if r.status != Status::DenyOffline {
        println!(
            "isolate: echo 下线后调用 status={:?} 应=DenyOffline（内核应平稳拒绝）",
            r.status
        );
        ok = false;
    }

    // (b) 另一个服务 store 完全不受牵连，往返照常。
    let p = kernel_call(
        &store_cap,
        &Msg {
            op: OP_PUT,
            key: 3,
            val: 0x0042,
        },
        &mut world,
    );
    let g = kernel_call(
        &store_cap,
        &Msg {
            op: OP_GET,
            key: 3,
            val: 0,
        },
        &mut world,
    );
    if p.status != Status::Ok || g.status != Status::Ok || g.val != 0x0042 {
        println!(
            "isolate: 邻居 store 被连累 put={:?} get=({:?},0x{:04x}) 应 get=(Ok,0x0042)",
            p.status, g.status, g.val
        );
        ok = false;
    }

    if ok {
        println!("ISOLATE_PASS");
    }
    ok
}

fn main() {
    let mut all = true;
    all &= check_micro();
    all &= check_cap();
    all &= check_isolate();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
