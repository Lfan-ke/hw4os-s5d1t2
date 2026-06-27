# 正经赛道共享：S 态 trap 入口/出口。保存 32 GPR + sstatus/sepc 到栈上的 TrapContext。
# 内核态自陷（如时钟中断）使用同一内核栈，sp 由 addi 管理（不保存/恢复 x2）。
.altmacro
.macro SAVE_GP n
    sd x\n, \n*8(sp)
.endm
.macro LOAD_GP n
    ld x\n, \n*8(sp)
.endm

    .section .text
    .globl __alltraps
    .globl __restore
    .align 2
__alltraps:
    addi sp, sp, -34*8
    sd   x1, 1*8(sp)
    sd   x3, 3*8(sp)
    .set n, 4
    .rept 28
        SAVE_GP %n
        .set n, n+1
    .endr
    csrr t0, sstatus
    csrr t1, sepc
    sd   t0, 32*8(sp)
    sd   t1, 33*8(sp)
    mv   a0, sp
    call trap_handler

    .globl __restore
__restore:
    ld   t0, 32*8(sp)
    ld   t1, 33*8(sp)
    csrw sstatus, t0
    csrw sepc, t1
    ld   x1, 1*8(sp)
    ld   x3, 3*8(sp)
    .set n, 4
    .rept 28
        LOAD_GP %n
        .set n, n+1
    .endr
    addi sp, sp, 34*8
    sret
