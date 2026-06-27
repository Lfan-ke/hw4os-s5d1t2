# S5d · __switch(cur, next)：协作式上下文切换（rcore ch3 / S5 TaskContext 风格，给定）。
#   a0 = cur  (struct TaskContext*)：把“当前任务”的 callee-saved 整存进去
#   a1 = next (struct TaskContext*)：从“下一个任务”的 callee-saved 整载回来
# 偏移：ra=0, sp=8, s0=16, s1=24, ... s11=104（每个 8 字节）。
# ret 跳到 next.ra —— 落回 next 上次让出/阻塞 __switch 的下一条指令（或新任务的入口）。
    .section .text
    .globl __switch
    .align 2
__switch:
    sd ra,  0*8(a0)
    sd sp,  1*8(a0)
    sd s0,  2*8(a0)
    sd s1,  3*8(a0)
    sd s2,  4*8(a0)
    sd s3,  5*8(a0)
    sd s4,  6*8(a0)
    sd s5,  7*8(a0)
    sd s6,  8*8(a0)
    sd s7,  9*8(a0)
    sd s8, 10*8(a0)
    sd s9, 11*8(a0)
    sd s10,12*8(a0)
    sd s11,13*8(a0)
    ld ra,  0*8(a1)
    ld sp,  1*8(a1)
    ld s0,  2*8(a1)
    ld s1,  3*8(a1)
    ld s2,  4*8(a1)
    ld s3,  5*8(a1)
    ld s4,  6*8(a1)
    ld s5,  7*8(a1)
    ld s6,  8*8(a1)
    ld s7,  9*8(a1)
    ld s8, 10*8(a1)
    ld s9, 11*8(a1)
    ld s10,12*8(a1)
    ld s11,13*8(a1)
    ret
