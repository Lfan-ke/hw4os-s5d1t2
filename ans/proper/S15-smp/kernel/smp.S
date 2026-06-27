# S15 · 副 hart 入口（多核启动）。
# SBI HSM hart_start(EID=0x48534D, fid=0, a0=hartid, a1=start_addr, a2=opaque)
# 唤醒的 hart 在 S 态、satp=0 跳到这里，a0=hartid、a1=opaque。
# OpenSBI 只把引导 hart 送进 _start（用单一 boot_stack），副 hart 必须在这里
# 自备 per-hart 栈，否则几个核共用一个栈会互相踩烂。
    .equ STACK_SIZE, 16384      # 每核 16KiB 栈

    .section .text
    .globl secondary_entry
secondary_entry:
    # sp = sec_stack_base + (hartid+1) * STACK_SIZE  （取该核栈顶）
    la      t0, sec_stack_base
    addi    t1, a0, 1
    slli    t1, t1, 14          # *16384
    add     sp, t0, t1
    call    secondary_main      # a0 仍为 hartid
1:  wfi                         # 返回后停泊；引导 hart 关机时一起被收走
    j       1b

    # per-hart 栈区（.bss，引导 hart 在 _start 已清零）。NHART=4 → 4 个槽。
    .section .bss
    .balign 16
    .globl sec_stack_base
sec_stack_base:
    .space STACK_SIZE * 4
