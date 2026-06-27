/* 04 · 线程管理：进程是线程的资源容器 —— C。
 * 04-1 ctx：一个上下文 = CSR + GPRs；ctx_save / ctx_restore / ctx_switch。
 * 04-2 tcb：PCB→TCB——抽出共享资源 Process，Tcb 只存 ctx + 指向 Process 的指针；
 *           同进程两线程共享同一 fd/内存；调度器轮流 switch 多个 TCB。
 * 你只需填 4 个函数体；下方测试 harness 勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define NGPR 31
#define NRES 8

/* 一个上下文 = 一把寄存器 = 一个执行身份。 */
typedef struct {
    uint64_t gpr[NGPR]; /* x1..x31 */
    uint64_t sepc;      /* 异常返回 PC */
    uint64_t sstatus;   /* 状态 CSR（占位） */
    uint64_t sp;        /* 栈指针（x2 单列） */
} Context;

/* 当前“虚拟 CPU”：寄存器现场 = 正在执行的那套上下文。 */
typedef struct {
    Context regs;
} Vcpu;

/* 进程资源（全线程共享）：地址空间 / fd 表 / 信号量，统统占位。 */
typedef struct {
    uint64_t mem[NRES];
    int64_t  fd[NRES];
    int64_t  sem;
} SharedRes;

/* PCB = 资源容器。 */
typedef struct {
    uint32_t  pid;
    SharedRes res;
} Process;

/* TCB = 一把寄存器（ctx）+ 指向 PCB 的指针。无任何资源副本。 */
typedef struct {
    uint32_t tid;
    Context  ctx;
    Process *proc;
} Tcb;

static void ctx_zero(Context *c) { memset(c, 0, sizeof(*c)); }

/* ── 04-1：上下文存取与切换（学生填）── */

/* 把当前 vCPU 的整套寄存器现场整存进 cur。 */
static void ctx_save(const Vcpu *vcpu, Context *cur) {
    (void)vcpu; (void)cur;
    /* TODO[a]：逐字段搬——memcpy(cur->gpr, vcpu->regs.gpr, sizeof cur->gpr); cur->sepc = ...; cur->sstatus = ...; cur->sp = ...
     * ELSE[b]：整体赋值——*cur = vcpu->regs;
     * 占位：什么也不存 → 判 FAIL */
}

/* 把 next 的整套寄存器整载回 vCPU（与 save 相反）。 */
static void ctx_restore(Vcpu *vcpu, const Context *next) {
    (void)vcpu; (void)next;
    /* TODO: vcpu->regs = *next;（整组载回当前 vCPU）
     * 占位：什么也不载 → 判 FAIL */
}

/* 协作式切换：先存当前现场到 cur，再把 next 载入当前。 */
static void ctx_switch(Vcpu *vcpu, Context *cur, const Context *next) {
    (void)vcpu; (void)cur; (void)next; (void)ctx_save; /* (void)ctx_save 仅为消除“未用”告警 */
    /* TODO: ctx_save(vcpu, cur); ctx_restore(vcpu, next);
     * 占位：什么也不切 → 判 FAIL */
}

/* ── 04-2：让两个 TCB 指向同一个 Process（学生填）── */

/* 造两个线程，让它们共享**同一个** Process p（other 为对照用的无关进程）。 */
static void spawn_shared_pair(Process *p, Process *other, Tcb *t1, Tcb *t2) {
    /* TODO[a]：C 没有 Rc，让两个 TCB 的 proc 指针都指向同一个 p 即可共享。
     * 占位：t2 误指向另一个进程 other → 两线程不共享 → 判 FAIL */
    t1->tid = 1; ctx_zero(&t1->ctx); t1->proc = p;
    t2->tid = 2; ctx_zero(&t2->ctx); t2->proc = other;
}

/* ── 测试 harness（勿改）── */

static Context ctx_demo(uint64_t base) {
    Context c; ctx_zero(&c);
    for (int i = 0; i < NGPR; i++) c.gpr[i] = base + (uint64_t)i;
    c.sepc = base + 0x00E0;
    c.sstatus = base + 0x005A;
    c.sp = base + 0x5000;
    return c;
}

static int ctx_eq(const Context *a, const Context *b) {
    return memcmp(a, b, sizeof(Context)) == 0;
}

static int check_ctx(void) {
    Context a_snapshot = ctx_demo(0xA000);
    Context b_snapshot = ctx_demo(0xB000);
    Context a = a_snapshot, b = b_snapshot;

    Vcpu vcpu; ctx_zero(&vcpu.regs);
    ctx_restore(&vcpu, &a);
    if (!ctx_eq(&vcpu.regs, &a_snapshot)) {
        printf("CTX_SWAP_FAIL restore 后 vCPU 现场 != A（未整组载入）\n");
        return 0;
    }

    ctx_switch(&vcpu, &a, &b);
    if (!ctx_eq(&vcpu.regs, &b_snapshot)) {
        printf("CTX_SWAP_FAIL switch 后 vCPU 现场 != B（上下文未整组迁移）\n");
        return 0;
    }
    if (vcpu.regs.sepc != b_snapshot.sepc) {
        printf("CTX_SWAP_FAIL sepc 未随上下文迁移到 B 执行流\n");
        return 0;
    }
    if (!ctx_eq(&a, &a_snapshot)) {
        printf("CTX_SWAP_FAIL ctx_save 未把切换前的现场整存进 A\n");
        return 0;
    }

    vcpu.regs.gpr[5] = 0xDEAD;
    vcpu.regs.sepc += 4;
    Context b_after = vcpu.regs;
    ctx_switch(&vcpu, &b, &a);
    if (!ctx_eq(&vcpu.regs, &a_snapshot)) {
        printf("CTX_SWAP_FAIL 切回 A 后现场 != A\n");
        return 0;
    }
    if (!ctx_eq(&b, &b_after)) {
        printf("CTX_SWAP_FAIL 切回时未把 B 的最新现场存回 B\n");
        return 0;
    }
    printf("CTX_SWAP_PASS\n");
    return 1;
}

static int check_share(const Tcb *t1, const Tcb *t2) {
    t1->proc->res.fd[3] = 99;
    t1->proc->res.mem[0] = 0xABCD;
    t1->proc->res.sem += 1;
    if (t2->proc->res.fd[3] == 99 && t2->proc->res.mem[0] == 0xABCD && t2->proc->res.sem == 1) {
        printf("SHARE_PASS\n");
        return 1;
    }
    printf("SHARE_FAIL 线程2 看不到线程1 写入的 fd/mem/sem（两线程未共享同一 Process）\n");
    return 0;
}

static int check_sched(void) {
    Process proc; memset(&proc, 0, sizeof(proc)); proc.pid = 100;
    for (int i = 0; i < NRES; i++) proc.res.fd[i] = -1;

    uint64_t starts[3] = { 1000, 2000, 3000 };
    Context ctxs[3];
    uint32_t tids[3];
    for (int k = 0; k < 3; k++) {
        ctx_zero(&ctxs[k]);
        ctxs[k].sepc = starts[k];
        ctxs[k].gpr[0] = 0x1000 + (uint64_t)k;
        tids[k] = (uint32_t)(k + 1);
    }

    Vcpu vcpu; ctx_zero(&vcpu.regs);
    ctx_restore(&vcpu, &ctxs[0]);
    uint32_t trace[16]; int tn = 0;
    trace[tn++] = tids[0];
    int cur = 0;
    for (int r = 0; r < 6; r++) {
        vcpu.regs.sepc += 4;
        int next = (cur + 1) % 3;
        Context nxt = ctxs[next];
        ctx_switch(&vcpu, &ctxs[cur], &nxt);
        cur = next;
        trace[tn++] = tids[cur];
    }

    int ok = 1;
    uint32_t want[7] = { 1, 2, 3, 1, 2, 3, 1 };
    if (tn != 7) { printf("SCHED_FAIL 执行序长度=%d 应=7\n", tn); ok = 0; }
    for (int i = 0; i < tn && i < 7; i++)
        if (trace[i] != want[i]) { printf("SCHED_FAIL 轮转序 pos%d=%u 应=%u\n", i, trace[i], want[i]); ok = 0; }
    for (int k = 0; k < 3; k++) {
        if (ctxs[k].sepc != starts[k] + 8) {
            printf("SCHED_FAIL 线程%d sepc=%llu 应=%llu（上下文未独立推进）\n",
                   k + 1, (unsigned long long)ctxs[k].sepc, (unsigned long long)(starts[k] + 8)); ok = 0;
        }
        if (ctxs[k].gpr[0] != 0x1000 + (uint64_t)k) { printf("SCHED_FAIL 线程%d 标记被污染\n", k + 1); ok = 0; }
    }
    if (ok) printf("SCHED_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;

    all &= check_ctx();

    Process p; memset(&p, 0, sizeof(p)); p.pid = 7;
    Process q; memset(&q, 0, sizeof(q)); q.pid = 8;
    for (int i = 0; i < NRES; i++) { p.res.fd[i] = -1; q.res.fd[i] = -1; }
    Tcb t1, t2;
    spawn_shared_pair(&p, &q, &t1, &t2);
    all &= check_share(&t1, &t2);

    all &= check_sched();

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
