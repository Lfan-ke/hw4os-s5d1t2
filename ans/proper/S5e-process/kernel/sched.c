/* S5e · 进程管理主体（给定）：trap 分发 + fork/exec/wait/exit + 协作式调度。
 * 调度模型：陷入即处理；不切进程的 syscall 走正常 __restore 返回当前进程，
 * 切进程的（exit / wait 阻塞 / exec）走 swtch_to_user / schedule，不返回。 */
#include "kernel.h"
#include "proc.h"
#include "riscv.h"

struct proc ptable[NPROC];
struct proc *current = 0;
volatile int g_fork = 0, g_cow = 0, g_exec = 0, g_wait = 0;

static int next_pid = 1;
static int g_steps = 0;       /* 有界守卫：trap 次数上限，防学生版死循环 */
#define STEP_GUARD 400

void trap_init(void) { w_stvec((uint64_t)__alltraps); }

int proc_index(struct proc *p) { return (int)(p - ptable); }

struct proc *proc_alloc(void) {
    for (int i = 0; i < NPROC; i++) {
        if (!ptable[i].used) {
            struct proc *p = &ptable[i];
            uint64_t *slots = (uint64_t *)&p->tf;
            for (int j = 0; j < 34; j++) slots[j] = 0;
            p->used = 1;
            p->pid = next_pid++;
            p->state = P_RUNNABLE;
            p->parent = -1;
            p->exit_code = 0;
            p->root = 0;
            p->satp = 0;
            return p;
        }
    }
    return 0;
}

/* 建一个全新的用户地址空间：内核恒等映射 + 新栈（4 页）+ 新数据页。 */
uint64_t *uvm_create(void) {
    uint64_t *root = (uint64_t *)frame_alloc();
    map_kernel(root);
    for (uint64_t va = USTACK_BASE; va < USTACK_TOP; va += PAGE_SIZE) {
        void *f = frame_alloc();
        map_one(root, va, (uint64_t)f, PTE_R | PTE_W | PTE_U);
    }
    void *d = frame_alloc();
    map_one(root, DATA_VA, (uint64_t)d, PTE_R | PTE_W | PTE_U);
    return root;
}

/* __alltraps 不保存 x2(sp)：陷入跑在用户栈上，原用户 sp = ctx 指针 + 34*8。 */
static void save_context(struct proc *p, struct TrapContext *ctx) {
    kmemcpy(&p->tf, ctx, sizeof(struct TrapContext));
    p->tf.x[2] = (uint64_t)ctx + 34 * 8;
}

/* ===== 调度：选一个 RUNNABLE 切入；都不可运行则回内核主线 ===== */
void schedule(void) {
    for (int i = 0; i < NPROC; i++) {
        if (ptable[i].used && ptable[i].state == P_RUNNABLE) {
            current = &ptable[i];
            current->state = P_RUNNING;
            swtch_to_user(&current->tf, current->satp); /* 不返回 */
        }
    }
    return_to_kernel(); /* 无可运行进程：所有进程已结束 → longjmp 回 kmain */
}

/* ===== syscall 处理体 ===== */
static long sys_write(long fd, const char *buf, long len) {
    (void)fd;
    for (long i = 0; i < len; i++) console_putchar((unsigned char)buf[i]);
    return len;
}

static void do_fork(struct TrapContext *ctx) {
    struct proc *child = proc_alloc();
    if (!child) {
        ctx->x[10] = (uint64_t)-1;
        ctx->sepc += 4;
        return;
    }
    child->parent = proc_index(current);
    child->root = (uint64_t *)frame_alloc();
    map_kernel(child->root); /* 子进程也要恒等映射内核 + 用户代码页 */

    if (fork_copy_uvm(current, child) != 0) {
        /* 复制失败（含学生占位返回 -1）：撤销子进程，fork 返回 -1 */
        child->used = 0;
        ctx->x[10] = (uint64_t)-1;
        ctx->sepc += 4;
        return;
    }
    child->satp = make_satp(child->root);

    /* 子进程的陷入帧 = 父进程当前上下文的快照，仅改返回值与 sepc。 */
    kmemcpy(&child->tf, ctx, sizeof(struct TrapContext));
    child->tf.x[2] = (uint64_t)ctx + 34 * 8; /* 用户 sp（与父同 VA，落到复制后的私有栈帧） */
    child->tf.x[10] = 0;                      /* 子进程 fork 返回 0 */
    child->tf.sepc += 4;                      /* 跨过 ecall，从 fork 之后继续 */
    child->state = P_RUNNABLE;

    /* 父进程：返回子 pid，正常回到 fork 之后。 */
    ctx->x[10] = (uint64_t)child->pid;
    ctx->sepc += 4;

    g_fork = 1;
    kputs("FORK_PASS parent=");
    kputdec((uint64_t)current->pid);
    kputs(" child=");
    kputdec((uint64_t)child->pid);
    console_putchar('\n');
}

static void do_exec(struct TrapContext *ctx) {
    (void)ctx;
    /* exec：用第二个内嵌程序替换当前进程的地址空间（全新页表 + 新栈 + 新数据页）。 */
    uint64_t *nroot = uvm_create();
    current->root = nroot; /* 旧地址空间此处不回收（简化；见 essay 引申） */
    current->satp = make_satp(nroot);

    uint64_t *slots = (uint64_t *)&current->tf;
    for (int j = 0; j < 34; j++) slots[j] = 0;
    current->tf.sepc = TO_USER(user_b);    /* 新程序入口（用户别名区地址） */
    current->tf.x[2] = USTACK_TOP;         /* 全新用户栈顶 */
    current->tf.sstatus = (1UL << 18);     /* SUM=1, SPP=0 → sret 落 U 态 */

    g_exec = 1;
    kputs("EXEC_PASS loaded second program pid=");
    kputdec((uint64_t)current->pid);
    console_putchar('\n');

    swtch_to_user(&current->tf, current->satp); /* 立即切入新映像，不返回 */
}

static void do_exit(struct TrapContext *ctx, long code) {
    (void)ctx;
    current->exit_code = code;
    current->state = P_ZOMBIE; /* 变僵尸，等父 wait 回收 */
    kputs("[kernel] pid=");
    kputdec((uint64_t)current->pid);
    kputs(" exited code=");
    kputdec((uint64_t)code);
    console_putchar('\n');

    /* 唤醒在 wait 中睡眠的父进程 */
    if (current->parent >= 0) {
        struct proc *par = &ptable[current->parent];
        if (par->used && par->state == P_SLEEPING) par->state = P_RUNNABLE;
    }
    schedule(); /* 切到其它进程；不返回 */
}

static void do_wait(struct TrapContext *ctx) {
    int me = proc_index(current);

    /* ① 已有僵尸子进程：回收、取退出码、写回用户缓冲、返回子 pid。 */
    for (int i = 0; i < NPROC; i++) {
        if (ptable[i].used && ptable[i].parent == me && ptable[i].state == P_ZOMBIE) {
            long code = ptable[i].exit_code;
            int cpid = ptable[i].pid;
            ptable[i].used = 0; /* reap：彻底回收槽位（页表此处不回收，简化） */
            ptable[i].state = P_UNUSED;

            long *ucode = (long *)ctx->x[10]; /* a0：用户传入的 int* 缓冲 */
            if (ucode) *ucode = code;
            ctx->x[10] = (uint64_t)cpid; /* wait 返回被回收子进程 pid */
            ctx->sepc += 4;

            g_wait = 1;
            kputs("WAIT_PASS reaped child=");
            kputdec((uint64_t)cpid);
            kputs(" code=");
            kputdec((uint64_t)code);
            console_putchar('\n');
            return;
        }
    }

    /* ② 还有活着的子进程：睡眠等待，先存好现场（保持 sepc 在 ecall 上，醒来重试 wait）。 */
    int has_child = 0;
    for (int i = 0; i < NPROC; i++)
        if (ptable[i].used && ptable[i].parent == me && ptable[i].state != P_ZOMBIE)
            has_child = 1;
    if (has_child) {
        save_context(current, ctx);
        current->state = P_SLEEPING;
        schedule(); /* 切去跑子进程；不返回 */
    } else {
        /* ③ 没有子进程：wait 返回 -1。 */
        ctx->x[10] = (uint64_t)-1;
        ctx->sepc += 4;
    }
}

/* ===== trap 分发（被 common/trap.S 的 __alltraps 调用） ===== */
void trap_handler(struct TrapContext *ctx) {
    uint64_t scause = r_scause();

    if (++g_steps > STEP_GUARD) { /* 有界守卫：异常多到离谱 → 安全退回内核 */
        kputs("GUARD_ABORT too many traps\n");
        return_to_kernel();
    }

    int is_exc = !(scause & SCAUSE_INT_BIT);

    if (is_exc && scause == SCAUSE_U_ECALL) {
        long n = (long)ctx->x[17];
        long a0 = (long)ctx->x[10];
        long a1 = (long)ctx->x[11];
        long a2 = (long)ctx->x[12];
        switch (n) {
        case SYS_WRITE:
            ctx->x[10] = (uint64_t)sys_write(a0, (const char *)a1, a2);
            ctx->sepc += 4;
            return; /* 正常回当前进程 */
        case SYS_GETPID:
            ctx->x[10] = (uint64_t)current->pid;
            ctx->sepc += 4;
            return;
        case SYS_FORK:
            do_fork(ctx);
            return; /* 父进程正常返回 */
        case SYS_WAIT:
            do_wait(ctx); /* 可能阻塞（不返回）或回收后返回 */
            return;
        case SYS_EXEC:
            do_exec(ctx); /* 不返回 */
            return;
        case SYS_EXIT:
            do_exit(ctx, a0); /* 不返回 */
            return;
        default:
            kputs("UNEXPECTED_SYSCALL n=");
            kputdec((uint64_t)n);
            console_putchar('\n');
            ctx->x[10] = (uint64_t)-1;
            ctx->sepc += 4;
            return;
        }
    } else if (is_exc && (scause == EXC_STORE_PAGE_FAULT ||
                          scause == EXC_LOAD_PAGE_FAULT ||
                          scause == EXC_INST_PAGE_FAULT)) {
        /* 缺页：CoW 分裂后重执行出错指令（不前移 sepc）。 */
        cow_fault(r_stval());
        return;
    } else {
        kputs("UNEXPECTED_TRAP scause=");
        kputhex(scause);
        kputs(" stval=");
        kputhex(r_stval());
        console_putchar('\n');
        ctx->sepc += 4;
        return;
    }
}
