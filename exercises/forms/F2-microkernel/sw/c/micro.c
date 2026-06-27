/* 形态认知 · F2 微内核（microkernel）—— C。
 *
 * 本质权衡：把 fs / 驱动 / 网络全部赶出内核态，内核只剩三件事——
 *   IPC（消息路由）+ 调度 + 能力（capability）管理。
 * 用户态服务彼此隔离，只能「持有一张能力券 + 发一条消息」来互相访问；
 * 内核是唯一的中间人：校验能力、转发消息、把崩掉的服务标记下线。
 *
 * 三个判据：
 *   MICRO_PASS   —— 经消息往返，echo/store 语义正确（IPC 通了）。
 *   CAP_PASS     —— 没有能力 / 缺权利位的访问，被内核拒绝（不可绕过内核）。
 *   ISOLATE_PASS —— 一个服务「崩溃」被标记下线，内核与另一个服务仍存活。
 *
 * 你只需填两个函数：cap_check（能力校验）与 dispatch（消息分发，标 TODO 处）。
 * 下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>

/* ── 服务身份（CSpace 里 target 字段的取值）── */
#define SVC_ECHO  0 /* 回显服务 */
#define SVC_STORE 1 /* 键值存储服务 */
#define SVC_NONE  2 /* 哨兵：空能力槽（指向「无」） */
#define N_SVC     2

/* ── 权利位（rights）：能力券上盖的章 ── */
#define RIGHT_SEND (1u << 0) /* 允许向 target 发消息 */

/* ── 消息 opcode ── */
#define OP_ECHO 0
#define OP_PUT  1
#define OP_GET  2

#define STORE_SLOTS 4

/* 能力：不可伪造的对象引用 + 权利位。fd 只是进程私有下标，cap 自带 rights。 */
typedef struct {
    uint32_t rights;
    int target;
} Cap;

/* 一条 IPC 消息。 */
typedef struct {
    uint32_t op;
    uint32_t key;
    uint32_t val;
} Msg;

/* 内核回复状态码（ST_OK 放行；其余为内核拒绝理由）。 */
typedef enum {
    ST_OK = 0,
    ST_NOCAP,   /* 空能力槽 */
    ST_NORIGHT, /* 缺 SEND 权利位 */
    ST_OFFLINE  /* 目标服务已崩溃下线 */
} Status;

typedef struct {
    Status status;
    uint32_t val;
} Reply;

/* 「世界」状态：服务存活标记 + store 的内部数据。 */
typedef struct {
    int alive[N_SVC];
    uint32_t store[STORE_SLOTS];
} World;

static const char *st_name(Status s) {
    switch (s) {
    case ST_OK:      return "Ok";
    case ST_NOCAP:   return "DenyNoCap";
    case ST_NORIGHT: return "DenyNoRight";
    case ST_OFFLINE: return "DenyOffline";
    }
    return "?";
}

static World fresh_world(void) {
    World w;
    for (int i = 0; i < N_SVC; i++)
        w.alive[i] = 1;
    for (int i = 0; i < STORE_SLOTS; i++)
        w.store[i] = 0;
    return w;
}

/* ════════════════════════════════════════════════════════════════
 * 给定的用户态服务实现（harness 的一部分，勿改）
 * ════════════════════════════════════════════════════════════════ */

/* echo 服务：回显 val。 */
static uint32_t echo_service(const Msg *msg) {
    return msg->val;
}

/* store 服务：私有小存储，PUT 写槽、GET 读槽。 */
static uint32_t store_service(const Msg *msg, World *world) {
    unsigned k = msg->key % STORE_SLOTS;
    if (msg->op == OP_PUT) {
        world->store[k] = msg->val;
        return msg->val;
    }
    return world->store[k]; /* OP_GET / 其它一律当读 */
}

/* ════════════════════════════════════════════════════════════════
 * 学生填空区：能力校验 + 消息分发（微内核的两件核心活）
 * ════════════════════════════════════════════════════════════════ */

/* 内核能力校验：返回 ST_OK 表示放行，否则给出拒绝理由。
 * 规则（依次判定，顺序固定）：
 *   ① cap->target == SVC_NONE（空能力槽）          → ST_NOCAP
 *   ② cap->rights 缺 RIGHT_SEND 位                 → ST_NORIGHT
 *   ③ world->alive[cap->target] == 0（服务下线）   → ST_OFFLINE
 *   ④ 以上都过                                      → ST_OK
 */
static Status cap_check(const Cap *cap, const World *world) {
    /* TODO: 依次做上面四步判定。
     * HINT:
     *   if (cap->target == SVC_NONE) return ST_NOCAP;
     *   if ((cap->rights & RIGHT_SEND) == 0) return ST_NORIGHT;
     *   if (!world->alive[cap->target]) return ST_OFFLINE;
     *   return ST_OK;
     */
    (void)cap;
    (void)world;
    return ST_OK; /* ← 占位：恒放行 → CAP / ISOLATE 都过不了 */
}

/* 消息分发：把已放行的消息路由到对应用户态服务，返回服务回复的值。
 *   SVC_ECHO  → echo_service(msg)
 *   SVC_STORE → store_service(msg, world)
 *   其它      → 0（理论上不可达）
 */
static uint32_t dispatch(int target, const Msg *msg, World *world) {
    /* TODO: 按 target 路由到对应服务。
     * HINT:
     *   switch (target) {
     *   case SVC_ECHO:  return echo_service(msg);
     *   case SVC_STORE: return store_service(msg, world);
     *   default:        return 0;
     *   }
     */
    (void)target;
    (void)msg;
    (void)world;
    return 0; /* ← 占位：恒返回 0 → MICRO 过不了 */
}

/* ════════════════════════════════════════════════════════════════
 * 内核入口（给定，勿改）：能力校验在前，消息分发在后。
 * 任何对服务的访问都必须穿过内核这一关——微内核的核心不变量。
 * ════════════════════════════════════════════════════════════════ */
static Reply kernel_call(const Cap *cap, const Msg *msg, World *world) {
    Reply rep;
    Status verdict = cap_check(cap, world);
    if (verdict != ST_OK) {
        rep.status = verdict;
        rep.val = 0; /* 拒绝：根本不碰服务，副作用为零 */
        return rep;
    }
    rep.status = ST_OK;
    rep.val = dispatch(cap->target, msg, world);
    return rep;
}

/* ════════════════════════════════════════════════════════════════
 * 测试 harness（给定，勿改）
 * ════════════════════════════════════════════════════════════════ */

static int check_micro(void) {
    int ok = 1;
    World w = fresh_world();
    Cap echo_cap = {RIGHT_SEND, SVC_ECHO};
    Cap store_cap = {RIGHT_SEND, SVC_STORE};

    /* (a) echo 往返。 */
    Msg em = {OP_ECHO, 0, 0xABCD};
    Reply r = kernel_call(&echo_cap, &em, &w);
    if (r.status != ST_OK || r.val != 0xABCD) {
        printf("micro: echo 往返对不上 status=%s val=0x%04x 应=(Ok,0xABCD)\n",
               st_name(r.status), r.val);
        ok = 0;
    }

    /* (b) store 往返：PUT 槽 2，再 GET 槽 2。 */
    Msg pm = {OP_PUT, 2, 0x0055};
    Msg gm = {OP_GET, 2, 0};
    Reply p = kernel_call(&store_cap, &pm, &w);
    Reply g = kernel_call(&store_cap, &gm, &w);
    if (p.status != ST_OK || g.status != ST_OK || g.val != 0x0055) {
        printf("micro: store 往返对不上 put=%s get=(%s,0x%04x) 应 get=(Ok,0x0055)\n",
               st_name(p.status), st_name(g.status), g.val);
        ok = 0;
    }

    if (ok)
        printf("MICRO_PASS\n");
    return ok;
}

static int check_cap(void) {
    int ok = 1;
    World w = fresh_world();
    Msg em = {OP_ECHO, 0, 0x1234};

    /* (a) 空能力槽：必须拒。 */
    Cap nocap = {0, SVC_NONE};
    Reply r1 = kernel_call(&nocap, &em, &w);
    if (r1.status != ST_NOCAP) {
        printf("cap: 空能力却被放行 status=%s 应=DenyNoCap\n", st_name(r1.status));
        ok = 0;
    }

    /* (b) 有券但缺 SEND 权利位：必须拒。 */
    Cap weak = {0, SVC_ECHO};
    Reply r2 = kernel_call(&weak, &em, &w);
    if (r2.status != ST_NORIGHT) {
        printf("cap: 缺权利位却被放行 status=%s 应=DenyNoRight\n", st_name(r2.status));
        ok = 0;
    }

    /* (c) 关键不变量：被拒的 PUT 绝不能改到 store。 */
    Cap weak_store = {0, SVC_STORE};
    w.store[1] = 0;
    Msg pm = {OP_PUT, 1, 0x99};
    Reply r3 = kernel_call(&weak_store, &pm, &w);
    if (r3.status != ST_NORIGHT) {
        printf("cap: 无权 PUT 却被放行 status=%s 应=DenyNoRight\n", st_name(r3.status));
        ok = 0;
    }
    if (w.store[1] != 0) {
        printf("cap: 越权写穿透了内核，store[1]=0x%x 应=0（能力门形同虚设）\n", w.store[1]);
        ok = 0;
    }

    if (ok)
        printf("CAP_PASS\n");
    return ok;
}

static int check_isolate(void) {
    int ok = 1;
    World w = fresh_world();
    Cap echo_cap = {RIGHT_SEND, SVC_ECHO};
    Cap store_cap = {RIGHT_SEND, SVC_STORE};

    /* 崩溃前 echo 正常。 */
    Msg em = {OP_ECHO, 0, 0x7};
    Reply pre = kernel_call(&echo_cap, &em, &w);
    if (pre.status != ST_OK) {
        printf("isolate: 崩溃前 echo 就不正常 status=%s\n", st_name(pre.status));
        ok = 0;
    }

    /* —— echo 服务「崩溃」：内核标记下线 —— */
    w.alive[SVC_ECHO] = 0;

    /* (a) 再调 echo：内核存活，平稳返回拒绝。 */
    Reply r = kernel_call(&echo_cap, &em, &w);
    if (r.status != ST_OFFLINE) {
        printf("isolate: echo 下线后调用 status=%s 应=DenyOffline（内核应平稳拒绝）\n",
               st_name(r.status));
        ok = 0;
    }

    /* (b) 邻居 store 不受牵连。 */
    Msg pm = {OP_PUT, 3, 0x0042};
    Msg gm = {OP_GET, 3, 0};
    Reply p = kernel_call(&store_cap, &pm, &w);
    Reply g = kernel_call(&store_cap, &gm, &w);
    if (p.status != ST_OK || g.status != ST_OK || g.val != 0x0042) {
        printf("isolate: 邻居 store 被连累 put=%s get=(%s,0x%04x) 应 get=(Ok,0x0042)\n",
               st_name(p.status), st_name(g.status), g.val);
        ok = 0;
    }

    if (ok)
        printf("ISOLATE_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_micro();
    all &= check_cap();
    all &= check_isolate();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
