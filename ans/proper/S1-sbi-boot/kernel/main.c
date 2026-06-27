/* S1 · 内核入口/测试驱动（给定）：经 SBI 打印自检串，再返回（entry 调 k_shutdown）。 */
#include "kernel.h"

void kmain(void) {
    kputs("\n[S1] kernel entered in S-mode\n");
    kputs("SBI_BOOT\n");

    /* 用 SBI console putchar 逐字符打印一段，验证 putchar 通路 */
    kputs("hello from sbi: ");
    for (char c = 'A'; c <= 'E'; c++) console_putchar(c);
    console_putchar('\n');
    kputs("PUTCHAR_PASS\n");

    kputs("ALL_PASS\n");
    /* 返回 → entry.S 调 k_shutdown 关机，qemu 退出 */
}
