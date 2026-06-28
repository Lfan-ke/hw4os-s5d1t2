# S06e · 副 hart 入口（给定，同 S13/S15 做法）。
# SBI HSM hart_start 把目标 hart 以 S 态、satp=0 跳到这里，约定 a0=hartid、a1=opaque。
# 它不走 _start（那只引导核用、单一 boot_stack），副 hart 必须自备 per-hart 栈，
# 否则多个核共用一个栈会互相覆盖。
    .equ STACK_SIZE, 16384

    .section .text
    .globl secondary_entry
secondary_entry:
    la      t0, sec_stack_base
    addi    t1, a0, 1
    slli    t1, t1, 14          # *16384：取本核栈顶 = base + (hartid+1)*16384
    add     sp, t0, t1
    call    secondary_main      # a0 仍为 hartid
1:  wfi                         # 完成后停泊；引导核关机时一并停机
    j       1b

    .section .bss
    .balign 16
    .globl sec_stack_base
sec_stack_base:
    .space STACK_SIZE * 4       # NHART=4 → 4 个槽
