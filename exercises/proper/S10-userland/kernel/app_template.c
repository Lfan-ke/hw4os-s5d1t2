/* S10b · 用户态极简模板引擎：把模板里的 {{key}} 替换成 env 表里对应 val（填空版）。
 * 你来实现 render() 的替换扫描循环；env/模板/驱动已给。 */
#include "app.h"

struct kv { const char *key; const char *val; };

/* 渲染：扫描 tmpl，遇到 "{{key}}" 就在 env(nenv 项) 里查 key 并把其 val 拷进 out，
 * 普通字符原样拷贝。out 容量 cap（含结尾 '\0'）。返回输出长度。
 * 简化：未知 key 直接丢弃（本测试不触发）。 */
static int render(const char *tmpl, const struct kv *env, int nenv,
                  char *out, int cap) {
    int oi = 0;
    (void)env;
    (void)nenv;
    /* TODO: 实现替换扫描循环。
     *   while (tmpl[i]):
     *     若 tmpl[i]=='{' 且 tmpl[i+1]=='{':
     *        i+=2; 记 ks=i; 扫到 "}}" 处记 ke=i; 若是 "}}" 则 i+=2;
     *        在 env 里按 (key 长度 == ke-ks) 且逐字符相等 查到该项，
     *        把 env[e].val 逐字符拷进 out（注意 oi < cap-1）;
     *     否则：out[oi++] = tmpl[i]; i++;
     *   末尾 out[oi]='\0'; return oi;
     * 占位：原样照抄整个模板（不做替换）→ 输出里残留字面量 {{name}}，
     *       与期望串不符，跑不出 TMPL_PASS。 */
    for (int i = 0; tmpl[i] && oi < cap - 1; i++) out[oi++] = tmpl[i];
    out[oi] = '\0';
    return oi;
}

int app_template(void) {
    struct kv env[] = { {"name", "Ada"}, {"os", "NanoOS"}, {"ver", "3"} };
    const char *tmpl   = "Hi {{name}}, welcome to {{os}} v{{ver}}!";
    const char *expect = "Hi Ada, welcome to NanoOS v3!";
    char out[256];

    render(tmpl, env, 3, out, sizeof(out));

    if (u_strcmp(out, expect) != 0) {
        u_puts("[tmpl] mismatch: ");
        u_puts(out);
        u_puts("\n");
        return 0;
    }
    u_puts("[tmpl] ");
    u_puts(out);
    u_puts("\n");
    u_puts("TMPL_PASS\n");
    return 1;
}
