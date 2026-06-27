# S1b-c · 极简 crt0：清 bss、设栈、call main、return 后 _exit
    .section .text.entry
    .globl _start
_start:
    la   sp, _stack_top
    la   t0, sbss
    la   t1, ebss
1:  bgeu t0, t1, 2f
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    1b
2:  call main
    call _exit
    .section .bss.stack
    .space 4096*16
    .globl _stack_top
_stack_top:
