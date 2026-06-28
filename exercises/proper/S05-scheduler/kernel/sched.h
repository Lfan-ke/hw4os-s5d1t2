/* S05 · 协作式调度器：任务上下文 + __switch + 就绪任务表 + 轮转调度器。 */
#ifndef S05_SCHED_H
#define S05_SCHED_H
#include <stdint.h>

#define NTASK  3   /* 任务数 */
#define SLICES 3   /* 每个任务让出（yield）的次数 = 它占用 CPU 的时间片数 */

/* 任务上下文：协作式切换只需保存被调用者保存寄存器（callee-saved）。
 * 偏移（字节）：ra=0, sp=8, s0=16, s1=24, ..., s11=104。__switch.S 与此一一对应。 */
struct TaskContext {
    uint64_t ra;      /* 返回地址：决定“切回去后从哪条指令继续” */
    uint64_t sp;      /* 栈指针：每个任务一根独立内核栈 */
    uint64_t s[12];   /* s0..s11 */
};

/* 汇编实现：保存 cur 的 callee-saved，恢复 next 的 callee-saved，ret 到 next.ra。 */
void __switch(struct TaskContext *cur, struct TaskContext *next);

int  switch_selftest(void); /* 独立的上下文切换自检：来回切一趟，验证“回来值对” */
void sched_init(void);      /* 建立任务表与各任务初始上下文 */
void run_tasks(void);       /* 调度循环：拉起首个任务，全部退出后返回 */
void schedule(void);        /* 调度器：轮转选下一个就绪任务并切换（学生实现） */
int  sched_order_ok(void);  /* 校验记录到的运行序是否是正确的轮转序 */

extern int run_log[];       /* 运行序：每片记录占用 CPU 的任务 id */
extern int run_log_n;

#endif
