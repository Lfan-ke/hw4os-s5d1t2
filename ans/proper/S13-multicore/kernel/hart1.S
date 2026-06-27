# S13 · hart1 裸入口（给定）：SBI hart_start 把目标 hart 以 S 态/satp=0 跳到此处，
# 约定 a0=hartid、a1=opaque。这里只负责给 hart1 自己的栈，再进 C 主体。
    .section .text
    .globl hart1_entry
hart1_entry:
    la      sp, hart1_stack_top   # hart1 不能复用 hart0 的 boot_stack，自带一块栈
    call    hart1_main            # hart1_main(hartid)：a0 已是 hartid
1:  wfi                           # 收工后停在这里；qemu 由 hart0 关机退出
    j       1b

    .section .bss
    .align 4
    .globl hart1_stack
hart1_stack:
    .space 4096 * 4
    .globl hart1_stack_top
hart1_stack_top:
