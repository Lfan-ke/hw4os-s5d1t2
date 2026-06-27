/* S10 · U 态主程序（给定）：依次跑三个用户程序（排序 / 模板 / TUI），
 * 全过则 exit(0)，否则 exit(非 0)。内核据 exit code 决定是否打印 ALL_PASS。 */
#include "app.h"

void user_main(void) {
    int ok = 1;
    ok &= app_sort();
    ok &= app_template();
    ok &= app_tui();
    u_exit(ok ? 0 : 1);
    for (;;) { } /* exit 不返回；保险死循环 */
}
