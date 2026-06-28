/* S09 · U 态用户程序（给定）。只用 libc（ulib.h），不碰任何内核函数。
 * 三个子测各自打印自己的 *_PASS（经 printf -> write 系统调用）：
 *   CRT0_PASS   —— crt0 成功把控制权交到 main，且 write/printf 的 %s 路径可用。
 *   MALLOC_PASS —— bump malloc 给出对齐、互不重叠、可写的内存。
 *   PRINTF_PASS —— 迷你 printf 的 %d/%x/%s 数字与字符串格式化正确（sprintf 自校验）。
 * 全过 -> main 返回 0 -> crt0 调 exit(0) -> 内核盖章 ALL_PASS。
 * 任一不过 -> 返回非 0 -> 内核不出 ALL_PASS（且不打印 FAIL 字样）。 */
#include "ulib.h"

int main(void) {
    int fails = 0;

    /* —— T1：crt0 + 基本输出路径 —— */
    printf("%s\n", "CRT0_PASS");

    /* —— T2：bump malloc —— */
    {
        unsigned long *p = (unsigned long *)malloc(32);
        unsigned long *q = (unsigned long *)malloc(16);
        int ok = p && q
                 && (((uintptr_t)p & 7u) == 0)          /* 8 字节对齐 */
                 && ((char *)q >= (char *)p + 32);        /* 两块互不重叠 */
        if (ok) {
            p[0] = 0xdeadUL; q[0] = 0xbeefUL;             /* 可写 */
            ok = (p[0] == 0xdeadUL && q[0] == 0xbeefUL);
        }
        if (ok) printf("%s\n", "MALLOC_PASS");
        else  { printf("%s\n", "[user] malloc check mismatch"); fails++; }
    }

    /* —— T3：迷你 printf 的数字/字符串格式化（自校验：sprintf 到缓冲再比对）—— */
    {
        char buf[64];
        sprintf(buf, "[%d|%x|%s]", -42, 0xBEEF, "hi");
        const char *want = "[-42|beef|hi]";
        if (strcmp(buf, want) == 0) {
            printf("fmt -> %s\n", buf);    /* 顺带展示一行真实格式化输出 */
            printf("%s\n", "PRINTF_PASS");
        } else {
            printf("got %s want %s\n", buf, want);
            fails++;
        }
    }

    return fails; /* crt0 用作 exit code */
}
