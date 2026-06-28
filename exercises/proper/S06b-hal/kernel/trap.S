# S06b · CTE 的 S 态陷入入口（给定，无需修改）。
# 这是 YSYX AM 里 __am_asm_trap 的 S 态版本：
#   1) 在“当前任务自己的栈”上压一份完整 Context（gpr + scause/sstatus/sepc）；
#   2) mv a0, sp 把 Context* 传给 __am_irq_handle，由它分发事件、返回“下一个 Context*”；
#   3) mv sp, a0 切到下一个 Context 所在的栈，整体恢复后 sret。
# 关键：x2(sp) 不进通用寄存器循环——它由 sp 自身管理（addi/mv），
#       于是“每个任务一根栈、Context 就建在该栈顶”的 AM 模型自然成立。
    .section .text
    .globl __am_trap
    .align 2
__am_trap:
    addi sp, sp, -35*8
    sd x1,  1*8(sp)
    sd x3,  3*8(sp)
    sd x4,  4*8(sp)
    sd x5,  5*8(sp)
    sd x6,  6*8(sp)
    sd x7,  7*8(sp)
    sd x8,  8*8(sp)
    sd x9,  9*8(sp)
    sd x10, 10*8(sp)
    sd x11, 11*8(sp)
    sd x12, 12*8(sp)
    sd x13, 13*8(sp)
    sd x14, 14*8(sp)
    sd x15, 15*8(sp)
    sd x16, 16*8(sp)
    sd x17, 17*8(sp)
    sd x18, 18*8(sp)
    sd x19, 19*8(sp)
    sd x20, 20*8(sp)
    sd x21, 21*8(sp)
    sd x22, 22*8(sp)
    sd x23, 23*8(sp)
    sd x24, 24*8(sp)
    sd x25, 25*8(sp)
    sd x26, 26*8(sp)
    sd x27, 27*8(sp)
    sd x28, 28*8(sp)
    sd x29, 29*8(sp)
    sd x30, 30*8(sp)
    sd x31, 31*8(sp)
    csrr t0, scause
    csrr t1, sstatus
    csrr t2, sepc
    sd t0, 32*8(sp)
    sd t1, 33*8(sp)
    sd t2, 34*8(sp)

    mv a0, sp
    call __am_irq_handle
    mv sp, a0

    ld t1, 33*8(sp)
    ld t2, 34*8(sp)
    csrw sstatus, t1
    csrw sepc, t2
    ld x1,  1*8(sp)
    ld x3,  3*8(sp)
    ld x4,  4*8(sp)
    ld x5,  5*8(sp)
    ld x6,  6*8(sp)
    ld x7,  7*8(sp)
    ld x8,  8*8(sp)
    ld x9,  9*8(sp)
    ld x10, 10*8(sp)
    ld x11, 11*8(sp)
    ld x12, 12*8(sp)
    ld x13, 13*8(sp)
    ld x14, 14*8(sp)
    ld x15, 15*8(sp)
    ld x16, 16*8(sp)
    ld x17, 17*8(sp)
    ld x18, 18*8(sp)
    ld x19, 19*8(sp)
    ld x20, 20*8(sp)
    ld x21, 21*8(sp)
    ld x22, 22*8(sp)
    ld x23, 23*8(sp)
    ld x24, 24*8(sp)
    ld x25, 25*8(sp)
    ld x26, 26*8(sp)
    ld x27, 27*8(sp)
    ld x28, 28*8(sp)
    ld x29, 29*8(sp)
    ld x30, 30*8(sp)
    ld x31, 31*8(sp)
    addi sp, sp, 35*8
    sret
