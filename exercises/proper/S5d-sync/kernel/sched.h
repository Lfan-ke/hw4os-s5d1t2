/* S5d · 协作式两任务运行时（阻塞同步原语的载体，给定）。
 * 复用 S5 的 TaskContext + __switch，在其上加“每任务 READY/RUNNING/BLOCKED/EXITED 态”
 * 与“阻塞/唤醒”两个调度原语——这正是阻塞式同步原语需要的最小底座：
 *   - 一个抢不到资源的任务可以被置 BLOCKED 并 __switch 走人（零 CPU 占用）；
 *   - 持有者释放资源时把它置回 READY，调度器之后再把它切回来。
 * 同步原语（mutex/sem/condvar）只管“何时阻塞、何时唤醒”，不碰这里的切换细节。 */
#ifndef S5D_SCHED_H
#define S5D_SCHED_H
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

#define NTASK 2   /* 本课每个测试用两个协作任务（生产者/消费者，或两个争锁者） */

/* 拉起两个协作任务 f0、f1（f0 先上 CPU），都退出后返回。
 * 返回值：1=两任务都正常跑完退出；0=触发切换上限（疑似活锁/死锁）被强制收尾。
 * 每次调用都把 g_block_events 清零。 */
int  sched_run_pair(task_fn f0, task_fn f1);

/* 当前正在占用 CPU 的任务 id（0..NTASK-1）。 */
int  cur_task(void);

/* 主动让出 CPU，但保持 READY（协作式自旋让出；不算“阻塞”）。 */
void sync_yield(void);

/* 把“当前任务”置为 BLOCKED 并 __switch 走人——真正的阻塞：在被唤醒前
 * 调度器绝不会再选中它，它零 CPU 占用。被 sched_wake 置回 READY、再次被调度时，
 * 控制流从本函数调用点之后返回。 */
void sched_block(void);

/* 唤醒任务 id：若它处于 BLOCKED，置回 READY（重新可被调度）。不立即切换。 */
void sched_wake(int id);

/* 本次运行内“任务进入 BLOCKED 态”的累计次数。
 * 判据用它来区分“真阻塞”与“自旋让出”：自旋（sync_yield）不进 BLOCKED，恒为 0。 */
extern long g_block_events;

#endif
