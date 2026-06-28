# S05e · 地址空间切换 + sret 入用户 / longjmp 回内核主线（给定）。
#
#   swtch_to_user(tf=a0, satp=a1)：写 satp 换地址空间 + sfence，
#       从 *tf 载入全部 GPR/sstatus/sepc，sret 落到 U 态。不返回。
#   sched_enter()：把内核 callee-saved 存入 kctx，再 call schedule()。
#   return_to_kernel()：从 kctx 恢复现场并 ret —— 等价 longjmp 回 sched_enter 的调用点（kmain）。
#       当再无可运行进程时由 schedule() 调用，丢弃当前（用户栈上的）trap 现场、回内核主线。
    .section .text
    .globl swtch_to_user
    .globl sched_enter
    .globl return_to_kernel
    .align 2

swtch_to_user:
    # a0 = struct TrapContext*, a1 = satp
    csrw  satp, a1
    sfence.vma
    # 先把 sstatus/sepc 装进 CSR（用 t0/t1，随后会被 tf 里的 x5/x6 覆盖）
    ld    t0, 32*8(a0)
    ld    t1, 33*8(a0)
    csrw  sstatus, t0
    csrw  sepc, t1
    # 载入通用寄存器（x10/a0 是基址，最后载）；x0 不载。
    ld    x1,  1*8(a0)
    ld    x3,  3*8(a0)
    ld    x4,  4*8(a0)
    ld    x5,  5*8(a0)
    ld    x6,  6*8(a0)
    ld    x7,  7*8(a0)
    ld    x8,  8*8(a0)
    ld    x9,  9*8(a0)
    ld    x11, 11*8(a0)
    ld    x12, 12*8(a0)
    ld    x13, 13*8(a0)
    ld    x14, 14*8(a0)
    ld    x15, 15*8(a0)
    ld    x16, 16*8(a0)
    ld    x17, 17*8(a0)
    ld    x18, 18*8(a0)
    ld    x19, 19*8(a0)
    ld    x20, 20*8(a0)
    ld    x21, 21*8(a0)
    ld    x22, 22*8(a0)
    ld    x23, 23*8(a0)
    ld    x24, 24*8(a0)
    ld    x25, 25*8(a0)
    ld    x26, 26*8(a0)
    ld    x27, 27*8(a0)
    ld    x28, 28*8(a0)
    ld    x29, 29*8(a0)
    ld    x30, 30*8(a0)
    ld    x31, 31*8(a0)
    ld    x2,  2*8(a0)   # sp（用户栈）
    ld    x10, 10*8(a0)  # a0：最后载，丢弃基址
    sret

sched_enter:
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
    call  schedule
    # schedule 不会正常返回；万一返回，按现场恢复回调用者。
    la    t0, kctx
    ld    ra, 0*8(t0)
    ld    sp, 1*8(t0)
    ret

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

    .section .bss
    .globl kctx
    .align 3
kctx:
    .space 14*8
