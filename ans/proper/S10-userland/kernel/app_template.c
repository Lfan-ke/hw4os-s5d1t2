/* S10b · 用户态极简模板引擎：把模板里的 {{key}} 替换成 env 表里对应 val。
 * 学生实现 render() 的替换扫描循环；env/模板/驱动给定。 */
#include "app.h"

struct kv { const char *key; const char *val; };

/* 渲染：扫描 tmpl，遇到 "{{key}}" 就在 env(nenv 项) 里查 key 并把其 val 拷进 out，
 * 普通字符原样拷贝。out 容量 cap（含结尾 '\0'）。返回输出长度。
 * 简化：未知 key 直接丢弃（本测试不触发）。 */
static int render(const char *tmpl, const struct kv *env, int nenv,
                  char *out, int cap) {
    int oi = 0;
    int i = 0;
    while (tmpl[i]) {
        if (tmpl[i] == '{' && tmpl[i + 1] == '{') {
            /* 进入占位符：读 key 直到 "}}" */
            i += 2;
            int ks = i;
            while (tmpl[i] && !(tmpl[i] == '}' && tmpl[i + 1] == '}')) i++;
            int ke = i;             /* key = tmpl[ks..ke) */
            if (tmpl[i] == '}') i += 2; /* 跳过 "}}" */
            /* 在 env 里按 key 查 val 并拷贝 */
            for (int e = 0; e < nenv; e++) {
                const char *k = env[e].key;
                int kl = 0;
                while (k[kl]) kl++;
                if (kl != ke - ks) continue;
                int match = 1;
                for (int t = 0; t < kl; t++) {
                    if (k[t] != tmpl[ks + t]) { match = 0; break; }
                }
                if (match) {
                    const char *v = env[e].val;
                    for (int t = 0; v[t] && oi < cap - 1; t++) out[oi++] = v[t];
                    break;
                }
            }
        } else {
            if (oi < cap - 1) out[oi++] = tmpl[i];
            i++;
        }
    }
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
