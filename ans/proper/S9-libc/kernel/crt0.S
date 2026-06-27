# S9 · 用户程序的 C 运行时启动（crt0，给定）。
# 这是 U 态程序的真正入口：内核 run_user(user_start, ustack) 后，CPU 在 U 态从这里开始。
# 职责（普通进程里 _start 干的事的最小版）：
#   1) 调 main()                —— 返回值（int）在 a0
#   2) 把返回值当退出码交给 exit() —— exit 经 SYS_EXIT 陷入内核，不返回
# 本实验无命令行参数/环境变量/全局构造器，故 crt0 极简。
    .section .text
    .globl user_start
    .align 2
user_start:
    call  main          # int main(void);  返回值留在 a0
    call  exit          # void exit(int code);  以 main 返回值为退出码，不返回
1:  j     1b            # 保险：exit 理应不返回
