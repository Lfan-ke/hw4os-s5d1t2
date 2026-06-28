/* S06b · 迷你 Abstract Machine（HAL）头：TRM / IOE / CTE 三层可移植 API。
 *
 * 这是 YSYX AM 思想的极简 S 态移植：程序只调本头里的 API，不碰具体寄存器；
 * 换一套硬件只需换 *.c 实现，程序源码一字不改 —— 这就是“硬件抽象层(HAL)”。
 *
 * 对照 common 内核：那里直接 csrr/csrw、直接 SBI；这里把它们藏进 TRM/IOE/CTE 三层。 */
#ifndef S6B_AM_H
#define S6B_AM_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 内存区间 [start, end) */
typedef struct {
    void *start, *end;
} Area;

/* ---------- 事件：CTE 把“硬件陷入原因”翻译成与 arch 无关的事件 ---------- */
typedef struct {
    enum {
        EVENT_NULL = 0,
        EVENT_YIELD,   /* 主动让出（自陷）*/
        EVENT_ERROR,   /* 其它未预期陷入 */
    } event;
    uintptr_t cause;
} Event;

/* ---------- arch 相关的上下文：trap 入口保存的一整套现场 ----------
 * 布局必须与 trap.S 一一对应（gpr[0..31] + scause/sstatus/sepc）。
 * 注意：gpr[2]（sp）不由通用寄存器循环保存/恢复，而由 sp 自身管理
 * （AM 经典做法：trap 帧就建在“被陷入任务自己的栈”上）。 */
typedef struct Context {
    uintptr_t gpr[32];   /* x0..x31；a0=gpr[10], a1=gpr[11] ... */
    uintptr_t scause;
    uintptr_t sstatus;
    uintptr_t sepc;
} Context;

/* ===================== TRM：图灵机 / 最小计算 ===================== */
extern Area heap;                 /* 一块可用 RAM */
void putch(char ch);              /* 经 SBI 控制台输出一个字符 */
void halt(int code);              /* 关机（经 SBI）*/

/* ===================== IOE：设备 I/O 抽象 ===================== */
/* 统一接口：所有设备访问都走 ioe_read/ioe_write(reg, buf)，reg 是设备寄存器枚举。 */
bool ioe_init(void);
void ioe_read(int reg, void *buf);
void ioe_write(int reg, void *buf);

/* 设备寄存器枚举 + 各自的数据结构（仿 AM amdev.h，仅取本课所需）。 */
enum {
    AM_UART_TX = 2,       /* 串口发送 */
    AM_TIMER_UPTIME = 6,  /* 开机至今微秒数（经 rdtime）*/
};
typedef struct { char data; } AM_UART_TX_T;
typedef struct { uint64_t us; } AM_TIMER_UPTIME_T;

/* ===================== CTE：上下文 / 陷入 / 事件 ===================== */
/* cte_init：登记事件处理函数并设好陷入入口（stvec→trap.S）。 */
bool cte_init(Context *(*handler)(Event ev, Context *ctx));
/* yield：自陷一次，进入 handler；handler 返回的 Context 即下一个被恢复的现场。 */
void yield(void);
/* kcontext：在 kstack 上“凭空”造一份内核任务初始上下文（entry(arg) 作为入口）。 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg);

/* trap.S 提供的汇编入口与 C 分发器。 */
void __am_trap(void);
Context *__am_irq_handle(Context *c);

#endif
