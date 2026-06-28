/* 16c · 核内中断 - C（参考解，规矩·volatile+fence）。
 * 软件 CLINT 模型：mtime 自走、mtimecmp 比较器拉起 timer 中断；msip 写 1 拉起软件中断。
 * env=gcc-rv64：整程序编成 riscv64 静态 ELF 跑在 qemu-user，故 volatile 读写=真 lw/sw、fence=真屏障。
 * 固定场景：PERIOD=5、NTICK=16 → timer 触发 3 次（mtime=5/10/15）；软件中断拉起 2 次。
 */
#include <stdio.h>
#include <stdint.h>

#define PERIOD    5u
#define NTICK     16u
#define EXP_TIMER 3
#define EXP_SOFT  2

typedef struct {
    volatile uint32_t msip;
    uint32_t _pad;
    volatile uint64_t mtimecmp;
    volatile uint64_t mtime;
} Clint;
static Clint clint;

static inline void w32(volatile uint32_t *p, uint32_t v) {
    *p = v;
    __asm__ volatile("fence rw,rw" ::: "memory");
}
static inline uint32_t r32(volatile uint32_t *p) {
    __asm__ volatile("fence rw,rw" ::: "memory");
    return *p;
}
static inline void w64(volatile uint64_t *p, uint64_t v) {
    *p = v;
    __asm__ volatile("fence rw,rw" ::: "memory");
}
static inline uint64_t r64(volatile uint64_t *p) {
    __asm__ volatile("fence rw,rw" ::: "memory");
    return *p;
}

static void clint_tick(void) { clint.mtime = r64(&clint.mtime) + 1u; }

static int mtip_pending(void) { return r64(&clint.mtime) >= r64(&clint.mtimecmp); }
static int msip_pending(void) { return r32(&clint.msip) & 1u; }

static int phase_timer(void) {
    uint64_t cmp = PERIOD;
    w64(&clint.mtimecmp, cmp);
    int fires = 0;
    for (unsigned k = 0; k < NTICK; k++) {
        if (mtip_pending()) {
            fires++;
            cmp += PERIOD;
            w64(&clint.mtimecmp, cmp);
        }
        clint_tick();
    }
    if (fires != EXP_TIMER || r64(&clint.mtime) != NTICK) return 0;
    printf("TIMER_PASS fires=%d mtime=%llu\n", fires, (unsigned long long)r64(&clint.mtime));
    return 1;
}

static int phase_soft(void) {
    int handled = 0;
    for (int i = 0; i < EXP_SOFT; i++) {
        w32(&clint.msip, 1u);
        if (msip_pending()) {
            handled++;
            w32(&clint.msip, 0u);
        }
    }
    if (handled != EXP_SOFT || msip_pending()) return 0;
    printf("SOFT_PASS  ipi=%d\n", handled);
    return 1;
}

int main(void) {
    clint.msip = 0;
    clint.mtimecmp = 0;
    clint.mtime = 0;
    int ok = 1;
    ok &= phase_timer();
    ok &= phase_soft();
    if (ok) {
        printf("ALL_PASS\n");
        return 0;
    }
    printf("SOME_FAIL\n");
    return 1;
}
