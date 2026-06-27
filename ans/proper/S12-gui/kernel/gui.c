/* S12 · 绘图原语与 8x8 字模（软件 framebuffer，无真显示）。
 *
 * framebuffer 是内核里一块直接映射的二维像素数组（每像素 1 字节调色板索引）。
 * 真实 GUI 里这块内存会被 virtio-GPU/显存映射出去扫描到屏幕；本实验不接真外设，
 * 只在内核里"画"，最后对像素阵列取校验和与期望比对来证明绘制正确。
 */
#include "kernel.h"
#include "gui.h"

uint8_t fb[FB_H * FB_W];

/* 整屏填同色（清屏）。 */
void fb_clear(uint8_t color) {
    for (int i = 0; i < FB_H * FB_W; i++) fb[i] = color;
}

/* 画一个像素，越界丢弃（裁剪）。 */
void fb_point(int x, int y, uint8_t color) {
    if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    fb[y * FB_W + x] = color;
}

/* 实心矩形：左上角 (x,y)，宽 w 高 h，逐像素调用 fb_point 填充。
 * fb_point 自带裁剪，所以这里无需再判越界。 */
void fb_fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            fb_point(x + dx, y + dy, color);
}

/* —— 8x8 字模：每字符 8 字节，最高位(bit7)对应最左列 —— */
struct Glyph { char ch; uint8_t rows[8]; };
static const struct Glyph FONT[] = {
    { 'A', { 0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00 } },
    { 'H', { 0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00 } },
    { 'I', { 0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00 } },
    { 'O', { 0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00 } },
    { 'X', { 0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x00 } },
    { ' ', { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
};
#define FONT_N (int)(sizeof(FONT) / sizeof(FONT[0]))

static const uint8_t *glyph_of(char ch) {
    for (int i = 0; i < FONT_N; i++)
        if (FONT[i].ch == ch) return FONT[i].rows;
    return FONT[FONT_N - 1].rows; /* 未知字符当空格 */
}

/* 在 (x,y) 处用 8x8 字模画一个字符；置位的点用 color，其余留底色。 */
void fb_char(int x, int y, char ch, uint8_t color) {
    const uint8_t *g = glyph_of(ch);
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if (g[row] & (0x80 >> col))
                fb_point(x + col, y + row, color);
}

/* 对整块 framebuffer 取 FNV-1a 32 位校验和。 */
uint32_t fb_checksum(void) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < FB_H * FB_W; i++) {
        h ^= fb[i];
        h *= 16777619u;
    }
    return h;
}
