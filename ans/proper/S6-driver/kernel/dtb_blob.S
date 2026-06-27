/* 把 dtc 生成的 device.dtb 原样嵌入内核 .rodata。
 * 汇编时 .incbin 从当前目录读取 device.dtb（Makefile 保证它已生成）。 */
    .section .rodata
    .balign 8
    .globl device_dtb
device_dtb:
    .incbin "device.dtb"
    .globl device_dtb_end
device_dtb_end:
