/* S6b · IOE（Input/Output Devices）层实现（参考解）。
 * 统一接口 ioe_read/ioe_write(reg, buf)：程序按“设备寄存器枚举”访问设备，
 * 不必知道它背后是 rdtime、是 MMIO、还是 SBI —— 这正是设备无关性的来源。 */
#include "am.h"
#include "kernel.h"
#include "riscv.h"

bool ioe_init(void) { return true; }

/* 读类设备寄存器：按 reg 分发到具体后端。 */
void ioe_read(int reg, void *buf) {
    switch (reg) {
    case AM_TIMER_UPTIME: {
        AM_TIMER_UPTIME_T *t = (AM_TIMER_UPTIME_T *)buf;
        /* virt 机 mtime 频率约 10MHz：微秒数 = rdtime / 10。 */
        t->us = r_time() / 10;
        break;
    }
    default:
        break;
    }
}

/* 写类设备寄存器：按 reg 分发到具体后端。 */
void ioe_write(int reg, void *buf) {
    switch (reg) {
    case AM_UART_TX: {
        AM_UART_TX_T *u = (AM_UART_TX_T *)buf;
        putch(u->data);   /* 串口发送 → 复用 TRM 的 putch（经 SBI）*/
        break;
    }
    default:
        break;
    }
}
