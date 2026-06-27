# S8 · U 态进入 / syscall 返回内核（给定）。
#   run_user(entry=a0, ustack=a1)：把内核 callee-saved 现场存入 kctx，
#       清 sstatus.SPP(bit8) 使 sret 落到 U 态，置 sepc=entry、sp=ustack，sret 进 U 态。
#   return_to_kernel()：从 kctx 恢复现场并 ret —— 等价于 longjmp 回 kmain 中 run_user 的调用点。
#       sys_exit 在 trap 处理中调它，直接丢弃 U 态/trap 栈帧、回到内核主线。
    .section .text
    .globl run_user
    .globl return_to_kernel
    .align 2
run_user:
    la    t0, kctx
    sd    ra, 0*8(t0)
    sd    sp, 1*8(t0)
    sd    s0, 2*8(t0)
    sd    s1, 3*8(t0)
    sd    s2, 4*8(t0)
    sd    s3, 5*8(t0)
    sd    s4, 6*8(t0)
    sd    s5, 7*8(t0)
    sd    s6, 8*8(t0)
    sd    s7, 9*8(t0)
    sd    s8, 10*8(t0)
    sd    s9, 11*8(t0)
    sd    s10, 12*8(t0)
    sd    s11, 13*8(t0)
    # sstatus.SPP=0 -> sret 返回 U 态
    li    t1, 1 << 8
    csrc  sstatus, t1
    csrw  sepc, a0           # U 态入口
    mv    sp, a1             # U 态栈顶
    sret

return_to_kernel:
    la    t0, kctx
    ld    ra, 0*8(t0)
    ld    sp, 1*8(t0)
    ld    s0, 2*8(t0)
    ld    s1, 3*8(t0)
    ld    s2, 4*8(t0)
    ld    s3, 5*8(t0)
    ld    s4, 6*8(t0)
    ld    s5, 7*8(t0)
    ld    s6, 8*8(t0)
    ld    s7, 9*8(t0)
    ld    s8, 10*8(t0)
    ld    s9, 11*8(t0)
    ld    s10, 12*8(t0)
    ld    s11, 13*8(t0)
    ret
