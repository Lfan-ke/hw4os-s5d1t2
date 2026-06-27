/* 正经赛道共享：时钟。virt 机 10MHz，约 10ms 一拍。 */
#include "kernel.h"
#include "riscv.h"

#define TIMER_INTERVAL 100000UL

uint64_t get_time(void) { return r_time(); }

void set_next_trigger(void) { sbi_set_timer(get_time() + TIMER_INTERVAL); }
