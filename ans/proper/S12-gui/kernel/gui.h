/* S12 · 简易 GUI：软件 framebuffer + 绘图原语 + 极简标记渲染（共享声明）。 */
#ifndef S12_GUI_H
#define S12_GUI_H
#include <stdint.h>

/* —— framebuffer：直接映射的二维像素数组，每像素 1 字节调色板索引 —— */
#define FB_W 64
#define FB_H 32

/* 调色板（颜色名 -> 索引），0 为背景黑 */
enum {
    COL_BLACK = 0,
    COL_RED   = 1,
    COL_GREEN = 2,
    COL_BLUE  = 3,
    COL_YELLOW= 4,
    COL_CYAN  = 5,
    COL_WHITE = 7,
};

extern uint8_t fb[FB_H * FB_W];

/* —— 绘图原语（gui.c）—— */
void fb_clear(uint8_t color);
void fb_point(int x, int y, uint8_t color);
void fb_fill_rect(int x, int y, int w, int h, uint8_t color);  /* 学生补完 */
void fb_char(int x, int y, char ch, uint8_t color);
uint32_t fb_checksum(void);

/* —— 极简标记渲染（render.c）—— */
int color_by_name(const char *name, int len); /* 颜色名 -> 索引，未知返回 -1 */
void render_doc(const char *src);             /* 解析并绘制整篇文档 */

#endif
