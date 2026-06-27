/* S10c · 用户态丐版 TUI 组件：MD → ANSI。
 * 读一段 Markdown（行首 "# " 标题 / 行内 **粗体**），渲染成带 ANSI 转义的文本。
 * 全给定（学生只需完成排序与模板两题）。 */
#include "app.h"

#define ESC "\x1b"

static int append(char *o, int oi, int cap, const char *s) {
    for (int i = 0; s[i] && oi < cap - 1; i++) o[oi++] = s[i];
    return oi;
}

/* 极简 MD→ANSI：
 *   行首 "# X" → 整行标题：加粗 + 下划线 + 青色（ESC[1;4;36m ... ESC[0m）
 *   行内 **X** → 加粗（ESC[1m ... ESC[0m） */
static int render_md(const char *md, char *out, int cap) {
    int oi = 0;
    int i = 0;
    int at_line_start = 1;
    while (md[i]) {
        if (at_line_start && md[i] == '#' && md[i + 1] == ' ') {
            i += 2;
            oi = append(out, oi, cap, ESC "[1;4;36m");
            while (md[i] && md[i] != '\n') { if (oi < cap - 1) out[oi++] = md[i]; i++; }
            oi = append(out, oi, cap, ESC "[0m");
            if (md[i] == '\n') { if (oi < cap - 1) out[oi++] = '\n'; i++; }
            at_line_start = 1;
            continue;
        }
        if (md[i] == '*' && md[i + 1] == '*') {
            i += 2;
            oi = append(out, oi, cap, ESC "[1m");
            while (md[i] && !(md[i] == '*' && md[i + 1] == '*')) {
                if (oi < cap - 1) out[oi++] = md[i];
                i++;
            }
            if (md[i] == '*') i += 2; /* 跳过结尾 ** */
            oi = append(out, oi, cap, ESC "[0m");
            at_line_start = 0;
            continue;
        }
        if (oi < cap - 1) out[oi++] = md[i];
        at_line_start = (md[i] == '\n');
        i++;
    }
    out[oi] = '\0';
    return oi;
}

static int contains(const char *hay, const char *needle) {
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && hay[i + j] == needle[j]) j++;
        if (!needle[j]) return 1;
    }
    return 0;
}

int app_tui(void) {
    const char *md = "# NanoOS Status\nCore is **online** and **stable**.\n";
    char out[512];

    render_md(md, out, sizeof(out));

    u_puts("[tui] rendered:\n");
    u_puts(out);

    /* 自检：标题转义与粗体转义都已生成。 */
    if (!contains(out, ESC "[1;4;36m") ||
        !contains(out, ESC "[1m") ||
        !contains(out, ESC "[0m")) {
        u_puts("[tui] missing ANSI escapes\n");
        return 0;
    }
    u_puts("TUI_PASS\n");
    return 1;
}
