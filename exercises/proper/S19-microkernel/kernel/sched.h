/* S19 · 协作式两任务运行时（微内核 IPC 的载体，给定，沿用 S05/S14）。
 * 复用 S05 的 TaskContext + __switch：客户/服务两个协作任务在单核上轮流上 CPU，
 * 靠 ipc_yield() 主动让出。微内核路径只关心“客户怎么经 IPC 请求服务”，
 * 这套最小调度器只是把两任务拉起来、轮转、跑完退出。 */
#ifndef S19_SCHED_H
#define S19_SCHED_H
#include <stdint.h>

/* 任务上下文：协作式切换只需 callee-saved（ra/sp/s0-s11）。
 * 偏移：ra=0, sp=8, s0=16, ..., s11=104。switch.S 与此一一对应。 */
struct TaskContext {
    uint64_t ra;
    uint64_t sp;
    uint64_t s[12];
};

void __switch(struct TaskContext *cur, struct TaskContext *next);

typedef void (*task_fn)(void);

/* 拉起两个协作任务 f0、f1（round-robin），两者都退出后返回。
 * f0 先上 CPU。返回值：1=两任务都正常跑完退出；0=触发切换上限（疑似活锁）被强制收尾。 */
int  sched_run_pair(task_fn f0, task_fn f1);

/* 当前任务主动让出 CPU；下次被调度时从这里继续。 */
void ipc_yield(void);

#endif
