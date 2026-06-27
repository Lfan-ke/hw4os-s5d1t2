# 正经赛道共享内核入口：OpenSBI 跳入后，设栈、清 .bss、调 kmain，返回则关机。
    .section .text.entry
    .globl _start
_start:
    la      sp, boot_stack_top
    # 清零 .bss
    la      t0, sbss
    la      t1, ebss
1:  bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:
    call    kmain
    # kmain 返回 → 关机
    call    k_shutdown
3:  wfi
    j       3b

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 * 16
    .globl boot_stack_top
boot_stack_top:
