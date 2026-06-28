# S05 · __switch(cur, next)：协作式上下文切换（rcore ch3 TaskContext 风格）。
#   a0 = cur  (struct TaskContext*)：把“当前任务”的 callee-saved 整存进去
#   a1 = next (struct TaskContext*)：从“下一个任务”的 callee-saved 整载回来
# 偏移：ra=0, sp=8, s0=16, s1=24, ... s11=104（每个 8 字节）。
    .section .text
    .globl __switch
    .align 2
__switch:
    # TODO: 实现上下文切换：
    #   1) 把 ra、sp、s0..s11 依次 sd 到 cur(a0) 的对应偏移；
    #   2) 再从 next(a1) 的对应偏移把 ra、sp、s0..s11 依次 ld 回来；
    #   3) ret —— 跳到刚载入的 next.ra，控制流落回 next 上次让出处。
    # 例：sd ra, 0*8(a0) ; ... ; sd s11, 13*8(a0)
    #     ld ra, 0*8(a1) ; ... ; ld s11, 13*8(a1)
    #
    # 占位：直接 ret（未实现切换 → 自检失败、不死循环、不会 panic）。
    ret
