/* S01b-c · 测试驱动（给定）。emit() 走原始 SBI（不经 _write），这样桩没实现也能报告。 */
#include "minlibc.h"

static long sbi(long eid, long a0) {
    register long r0 asm("a0") = a0;
    register long r7 asm("a7") = eid;
    asm volatile("ecall" : "+r"(r0) : "r"(r7) : "memory");
    return r0;
}
static void emit(const char *s) { while (*s) sbi(1, (unsigned char)*s++); }
static int write_selftest(void) { return _write(1, "WR ", 3) == 3; }

int main(void) {
    emit("[S01b-c] minlibc lives on newlib-style syscall stubs\n");

    /* ① _write 桩 → printf/puts 的出口 */
    if (write_selftest()) emit("WRITE_PASS\n");
    else { emit("WRITE_MISS (implement _write?)\n"); return 0; }

    /* ② _sbrk 桩 → malloc 的堆来源 */
    char *p = (char *)malloc(64);
    if (p) { strcpy(p, "heap-ok"); emit("SBRK_PASS\nMALLOC_PASS\n"); }
    else { emit("SBRK_MISS (implement _sbrk?)\n"); return 0; }

    /* ③ 完整 libc：printf 经 _write 出去 */
    printf("[minlibc] %s d=%d x=%x\n", p, 42, 255);
    emit("PRINTF_PASS\n");
    emit("ALL_PASS\n");
    return 0;
}
