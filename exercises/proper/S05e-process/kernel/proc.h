/* S05e · 进程：fork / exec / wait + 写时复制（CoW）的共享声明。
 * 建在 S05c（SV39 分页/帧分配）与 S08（U 态 sret + ecall）之上。 */
#ifndef S5E_PROC_H
#define S5E_PROC_H
#include <stdint.h>
#include "kernel.h"

/* ===== SV39 页 / PTE 位 ===== */
#define PAGE_SIZE 4096UL
#define PTE_V (1UL << 0) /* Valid */
#define PTE_R (1UL << 1) /* Readable */
#define PTE_W (1UL << 2) /* Writable */
#define PTE_X (1UL << 3) /* eXecutable */
#define PTE_U (1UL << 4) /* User 可访问 */
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
#define PTE_COW (1UL << 8) /* RSW 软件位：此页为写时复制共享（写要先分裂） */

/* ===== 每进程用户地址空间布局（固定 VA，便于教学，免 ELF 加载） =====
 * 用户程序代码本身嵌在内核镜像里、按内核链接地址（0x8020_0000+）恒等映射执行；
 * 真正「每进程私有」的写区是下面两块：栈（fork 时整页复制）+ 数据页（fork 时 CoW 共享）。 */
#define DATA_VA     0x0000000020000000UL /* 1 页：被 fork CoW 共享的「共享变量」所在页 */
#define USTACK_TOP  0x0000000030000000UL /* 用户栈顶（高地址） */
#define USTACK_PAGES 4UL
#define USTACK_BASE (USTACK_TOP - USTACK_PAGES * PAGE_SIZE)

/* 内核恒等映射区间下界（覆盖 OpenSBI 之上的内核镜像 + 帧池 + 栈，上界用 ekernel）。 */
#define KMEM_LO 0x0000000080000000UL

/* 用户态执行别名区：把整块内核镜像再映射一份到这里，带 U+X。
 * 关键约束：S 态（trap 处理）取指必须落在「无 U」的内核页上（否则 S 态取指缺页）；
 * 而 U 态执行嵌在镜像里的用户代码又必须落在「带 U」的页上。同一物理页两套 VA、两套权限：
 *   - 内核 VA = 物理地址本身（恒等、无 U）：S 态跑内核/陷入；
 *   - 用户 VA = 物理地址 - KMEM_LO + USER_BASE（带 U、可执行）：U 态跑用户代码。
 * 整块别名 → PC 相对寻址在别名内自洽（代码/字面量/调用都落在别名页）。 */
#define USER_BASE 0x0000000040000000UL
/* 把内核镜像里的某地址(addr)换算成它在用户别名区的 VA（用作 U 态入口/sepc）。 */
#define TO_USER(addr) ((uint64_t)(addr) - KMEM_LO + USER_BASE)

/* ===== 系统调用号（取 RV64 Linux 风格号，保持 GNU 规范味） ===== */
#define SYS_WRITE  64
#define SYS_EXIT   93
#define SYS_GETPID 172
#define SYS_FORK   220
#define SYS_EXEC   221
#define SYS_WAIT   260

/* ===== scause 关键 code ===== */
#define SCAUSE_U_ECALL       8UL
#define EXC_INST_PAGE_FAULT  12UL
#define EXC_LOAD_PAGE_FAULT  13UL
#define EXC_STORE_PAGE_FAULT 15UL

/* ===== 进程表 ===== */
enum { P_UNUSED = 0, P_RUNNABLE, P_RUNNING, P_SLEEPING, P_ZOMBIE };

#define NPROC 8
struct proc {
    int used;
    int pid;
    int state;
    int parent;            /* 父进程在 ptable 中的下标，-1 表示无 */
    long exit_code;
    uint64_t *root;        /* 该进程的 SV39 根页表 */
    uint64_t satp;         /* (8<<60)|ppn(root)，切换地址空间用 */
    struct TrapContext tf; /* 保存的用户上下文（陷入/切换时存取） */
};
extern struct proc ptable[NPROC];
extern struct proc *current;

/* ===== mm.c（给定）：帧分配 / 建表 / 翻译 / 引用计数 / 内核恒等映射 ===== */
void *frame_alloc(void);
void  map_one(uint64_t *root, uint64_t va, uint64_t pa, uint64_t flags);
uint64_t va2pa(uint64_t *root, uint64_t va); /* 走表得 VA 对应的物理页基址，无映射返回 0 */
void  map_kernel(uint64_t *root);            /* 恒等映射内核运行区（含嵌入的用户代码页，带 U+X） */
void  kmemcpy(void *d, const void *s, uint64_t n);
void  ref_inc(uint64_t pa);
void  ref_dec(uint64_t pa);
int   ref_get(uint64_t pa);
static inline uint64_t make_satp(uint64_t *root) {
    return (8UL << 60) | ((uint64_t)root >> 12);
}

/* ===== proc.c（学生实现两处）===== */
int  fork_copy_uvm(struct proc *parent, struct proc *child); /* 填空①：复制地址空间用户页 */
void cow_fault(uint64_t fault_va);                            /* 填空②：缺页时 CoW 分裂 */

/* ===== sched.c（给定）：trap 分发 / fork / exec / wait / exit / 调度 ===== */
void trap_init(void);
void schedule(void);          /* 选下一个 RUNNABLE 进程切入；无则 longjmp 回内核 */
struct proc *proc_alloc(void);
int  proc_index(struct proc *p);
uint64_t *uvm_create(void);   /* 建全新用户地址空间（内核映射+新栈+新数据页） */

/* ===== swtch.S（给定）：地址空间切换 + sret 入用户 / 回内核主线 ===== */
void swtch_to_user(struct TrapContext *tf, uint64_t satp); /* 写 satp + 载 tf + sret，不返回 */
void sched_enter(void);                                    /* 存内核现场后 call schedule */
void return_to_kernel(void);                               /* longjmp 回 sched_enter 的调用点 */
extern uint64_t kctx[14];

/* ===== user.c（给定）：两个内嵌 U 态程序 ===== */
void user_a(void); /* 第一个程序：设共享变量 → fork → 子写变量(CoW) → exec；父 wait */
void user_b(void); /* 第二个程序：被 exec 载入运行，打印后 exit(7) */

/* ===== 判据标志（main.c 汇总成 ALL_PASS） ===== */
extern volatile int g_fork, g_cow, g_exec, g_wait;

#endif
