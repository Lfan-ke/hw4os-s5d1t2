/* S6 · 本实验本地头：UART/FDT/驱动表的最小接口。 */
#ifndef S6_DEV_H
#define S6_DEV_H
#include <stdint.h>

/* —— NS16550 UART 寄存器偏移（qemu virt：字节对齐，reg-shift=0）—— */
#define UART0_BASE 0x10000000UL
#define UART_RBR 0 /* 读：接收保持寄存器 */
#define UART_THR 0 /* 写：发送保持寄存器 */
#define UART_IER 1
#define UART_FCR 2 /* 写：FIFO 控制 */
#define UART_LCR 3 /* 线路控制（bit7=DLAB） */
#define UART_MCR 4 /* modem 控制（bit4=LOOP 回环） */
#define UART_LSR 5 /* 线路状态 */
#define UART_MSR 6
#define UART_SCR 7 /* 便签寄存器 */

#define LSR_DR   0x01 /* 收到数据 */
#define LSR_THRE 0x20 /* 发送保持寄存器空 */
#define MCR_LOOP 0x10 /* 回环自测 */

/* —— UART MMIO 驱动（uart.c；学生填寄存器读写）—— */
uint8_t uart_reg_read(uint64_t base, int off);
void    uart_reg_write(uint64_t base, int off, uint8_t v);
void    uart_putc(uint64_t base, char c);
void    uart_puts(uint64_t base, const char *s);
int     uart_loopback_selftest(uint64_t base); /* 1=回环字节一致 */

/* —— FDT/设备树解析（fdt.c；已给）—— */
struct dt_device {
    const char *name;        /* 节点名，如 "uart@10000000" */
    const char *compatible;  /* compatible 首串，如 "ns16550a"，无则 0 */
    uint64_t    reg_addr;    /* reg 首地址（#address-cells=2 ⇒ 取前 8 字节 BE） */
    int         has_reg;
};
int fdt_check_magic(const uint8_t *dtb);                       /* 1=魔数正确 */
int fdt_scan(const uint8_t *dtb, struct dt_device *out, int max); /* 返回采集到的设备节点数 */

/* —— 驱动表 + compatible 匹配 + probe（driver.c；学生填匹配）—— */
int driver_match_and_probe(const char *compatible, uint64_t base); /* 1=匹配且 probe 成功；0=匹配但失败；-1=无驱动 */

/* 内嵌的 dtb（dtb_blob.S）。 */
extern unsigned char device_dtb[];
extern unsigned char device_dtb_end[];

#endif
