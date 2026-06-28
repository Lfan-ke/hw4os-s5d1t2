/* S06 · 扁平设备树(FDT/dtb)最小解析器（已给，无需修改）。
 *
 * dtb 布局：fdt_header（大端 u32 字段）+ 内存保留块 + 结构块 + 字符串块。
 * 结构块由 token 流构成（大端 u32）：
 *   FDT_BEGIN_NODE=1  后跟以 NUL 结尾、4 字节对齐的节点名
 *   FDT_PROP=3        后跟 u32 len, u32 nameoff(指向字符串块), 再跟 len 字节值(4 对齐)
 *   FDT_END_NODE=2    节点结束
 *   FDT_NOP=4 / FDT_END=9
 * 所有多字节整数都是大端，这里用逐字节读取，规避对齐问题。 */
#include "dev.h"

#define FDT_MAGIC        0xd00dfeedu
#define FDT_BEGIN_NODE   1u
#define FDT_END_NODE     2u
#define FDT_PROP         3u
#define FDT_NOP          4u
#define FDT_END          9u

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
static uint64_t be64(const uint8_t *p) {
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}
static uint32_t s_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}
static int s_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b ? 1 : 0;
}

int fdt_check_magic(const uint8_t *dtb) {
    return be32(dtb) == FDT_MAGIC ? 1 : 0;
}

int fdt_scan(const uint8_t *dtb, struct dt_device *out, int max) {
    if (!fdt_check_magic(dtb)) return 0;

    uint32_t off_struct  = be32(dtb + 8);
    uint32_t off_strings = be32(dtb + 12);
    const uint8_t *strs  = dtb + off_strings;
    const uint8_t *p     = dtb + off_struct;

    /* 用栈记录当前节点路径，节点闭合时若有 reg/compatible 则收集。 */
    struct dt_device stack[16];
    int sp = 0;
    int count = 0;

    for (;;) {
        uint32_t tok = be32(p);
        p += 4;
        if (tok == FDT_END) break;

        if (tok == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            uint32_t l = s_strlen(name) + 1;
            p += (l + 3) & ~3u;
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp].name = name;
                stack[sp].compatible = 0;
                stack[sp].reg_addr = 0;
                stack[sp].has_reg = 0;
            }
            sp++;
        } else if (tok == FDT_PROP) {
            uint32_t plen = be32(p); p += 4;
            uint32_t noff = be32(p); p += 4;
            const char *pname = (const char *)(strs + noff);
            const uint8_t *pval = p;
            p += (plen + 3) & ~3u;
            if (sp >= 1 && sp <= (int)(sizeof(stack) / sizeof(stack[0]))) {
                struct dt_device *cur = &stack[sp - 1];
                if (s_streq(pname, "reg") && plen >= 8) {
                    cur->reg_addr = be64(pval); /* #address-cells=2：前 8 字节即地址 */
                    cur->has_reg = 1;
                } else if (s_streq(pname, "compatible") && plen >= 1) {
                    cur->compatible = (const char *)pval; /* 首个 NUL 结尾字符串 */
                }
            }
        } else if (tok == FDT_END_NODE) {
            if (sp >= 1) {
                sp--;
                struct dt_device *cur = &stack[sp];
                if (sp < (int)(sizeof(stack) / sizeof(stack[0])) &&
                    (cur->compatible || cur->has_reg) && count < max) {
                    out[count++] = *cur;
                }
            }
        } else if (tok == FDT_NOP) {
            /* 跳过 */
        } else {
            break; /* 未知 token，停止 */
        }
    }
    return count;
}
