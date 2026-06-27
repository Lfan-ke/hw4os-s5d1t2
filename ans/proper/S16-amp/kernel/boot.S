# S16 AMP：从核（小核）入口。hart0 经 SBI HSM sbi_hart_start 唤醒目标 hart，
# 它从 S 态在此开始执行：a0=hartid, a1=opaque。设各自栈，调 secondary_main。
    .section .text
    .globl hart_entry
hart_entry:
    la      sp, sec_stack_top      # 从核独立内核栈（与 boot_stack 分离）
    call    secondary_main          # secondary_main(hartid)  —— a0 已是 hartid
1:  wfi                             # 任务完成后停在这里，等 hart0 关机
    j       1b

    .section .bss
    .align 12
    .globl sec_stack
sec_stack:
    .space 4096 * 4
    .globl sec_stack_top
sec_stack_top:
