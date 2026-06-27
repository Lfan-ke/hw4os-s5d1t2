/* S12 · 极简标记渲染：把一段"文档"解析后绘制到 framebuffer。
 *
 * 这是 html/css→GUI 转译的丐版心智模型。文档里混排两类命令：
 *   1) {rect x y w h color}                       —— 类 CSS 盒子，画实心矩形（已给）
 *   2) <div style="color:NAME">TEXT</div>         —— 类 html 标签，按 color 画文字（学生补）
 * 解析器顺序扫描，遇 '{' 走矩形命令、遇 '<' 走标签命令，其余字符跳过。
 */
#include "kernel.h"
#include "gui.h"

/* —— 小工具：纯解析，不依赖 libc —— */
static int is_space(char c) { return c == ' ' || c == '\n' || c == '\t' || c == '\r'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }

static void skip_space(const char **p) {
    while (**p && is_space(**p)) (*p)++;
}

/* 读一个十进制整数，前移指针。 */
static int parse_int(const char **p) {
    skip_space(p);
    int v = 0;
    while (is_digit(**p)) { v = v * 10 + (**p - '0'); (*p)++; }
    return v;
}

/* 比较 [name,name+len) 是否等于以 0 结尾的 lit。 */
static int eq(const char *name, int len, const char *lit) {
    int i = 0;
    for (; i < len; i++) {
        if (lit[i] == 0 || name[i] != lit[i]) return 0;
    }
    return lit[i] == 0;
}

/* 颜色名 -> 调色板索引，未知返回 -1。 */
int color_by_name(const char *name, int len) {
    if (eq(name, len, "black"))  return COL_BLACK;
    if (eq(name, len, "red"))    return COL_RED;
    if (eq(name, len, "green"))  return COL_GREEN;
    if (eq(name, len, "blue"))   return COL_BLUE;
    if (eq(name, len, "yellow")) return COL_YELLOW;
    if (eq(name, len, "cyan"))   return COL_CYAN;
    if (eq(name, len, "white"))  return COL_WHITE;
    return -1;
}

/* 渲染游标：每个 <div> 文字另起一行。 */
static int div_y;

/* —— 命令 1：{rect x y w h color} —— 已给参考 —— */
static void parse_rect(const char **p) {
    (*p)++;                       /* 跳过 '{' */
    skip_space(p);
    /* 跳过关键字 "rect" */
    while (**p && !is_space(**p)) (*p)++;
    int x = parse_int(p), y = parse_int(p);
    int w = parse_int(p), h = parse_int(p);
    skip_space(p);
    const char *cs = *p;          /* 颜色名起点 */
    while (**p && **p != '}' && !is_space(**p)) (*p)++;
    int col = color_by_name(cs, (int)(*p - cs));
    if (col < 0) col = COL_WHITE;
    fb_fill_rect(x, y, w, h, (uint8_t)col);
    while (**p && **p != '}') (*p)++;
    if (**p == '}') (*p)++;       /* 跳过 '}' */
}

/* —— 命令 2：<div style="color:NAME">TEXT</div> —— 学生补完 —— */
static void parse_div(const char **p) {
    /* TODO(2): 解析 <div style="color:NAME">TEXT</div> 并把 TEXT 画到 framebuffer。
     *
     * 期望做法：
     *   1) 在 '>' 之前找到 "color:"，读出颜色名 -> color_by_name() 得到调色板索引 col
     *      （找不到就用默认 COL_WHITE）。
     *   2) 把 *p 前进到 '>' 之后；从这里逐字符 fb_char(x, div_y, **p, col)，
     *      每画一个字符 x += 8，直到遇见下一个 '<'（即 </div>）。
     *   3) div_y += 9 让下一个 <div> 另起一行。
     *
     * 下面的占位代码"只跳过整个标签、不画任何字"，保证不死循环、能编译运行，
     * 但 RENDER 子测试会因没画文字而不通过——这是预期的占位状态。
     * 提示：col 由 color_by_name 决定；越过结束标签同样要把 *p 推到 '>' 之后。
     */
    int col = COL_WHITE;
    (void)col;
    /* 跳过开标签 */
    while (**p && **p != '>') (*p)++;
    if (**p == '>') (*p)++;
    /* 跳过文字（占位：不绘制） */
    while (**p && **p != '<') (*p)++;
    div_y += 9;
    /* 跳过结束标签 </div> */
    while (**p && **p != '>') (*p)++;
    if (**p == '>') (*p)++;
}

void render_doc(const char *src) {
    div_y = 1;
    const char *p = src;
    while (*p) {
        if (*p == '{')      parse_rect(&p);
        else if (*p == '<') parse_div(&p);
        else                p++;
    }
}
