/* S12 · 简易 GUI（软件 framebuffer，无真显示）—— 测试驱动 harness（已给，勿改）。
 *
 * 两个子测试：
 *   DRAW   : 直接用绘图原语（清屏/矩形/字符）画一帧，对像素阵列取校验和比对
 *            -> 依赖 fb_fill_rect 正确                          -> DRAW_PASS
 *   RENDER : 解析一段极简标记文档（{rect ...} 与 <div ...>TEXT</div>）渲染成另一帧
 *            -> 依赖 html 标签解析正确                          -> RENDER_PASS
 * 两个都过 -> ALL_PASS。
 *
 * framebuffer 仅存在于内核内存：真实系统里它会被 virtio-GPU/显存映射扫描到屏幕；
 * 本实验聚焦"绘制逻辑正确"，用确定性校验和代替肉眼看图。
 */
#include "kernel.h"
#include "gui.h"

/* 参考帧的期望校验和（由参考解确定性算出后固化）。 */
#define EXPECT_DRAW   0xf77f2ab5u
#define EXPECT_RENDER 0xb7524475u

/* 文档：两个盒子 + 两行带色文字。 */
static const char DOC[] =
    "{rect 4 2 40 12 blue}"
    "{rect 30 16 20 12 green}"
    "<div style=\"color:red\">HI</div>"
    "<div style=\"color:white\">OX</div>";

static void report(const char *what, uint32_t got, uint32_t want) {
    kputs("[");
    kputs(what);
    kputs("] checksum=");
    kputhex(got);
    kputs(" expect=");
    kputhex(want);
    console_putchar('\n');
}

static int draw_test(void) {
    fb_clear(COL_BLACK);
    fb_fill_rect(10, 5, 20, 10, COL_RED);   /* 实心矩形 */
    fb_point(2, 2, COL_GREEN);              /* 单点 */
    fb_char(40, 4, 'A', COL_WHITE);         /* 8x8 字符 */
    fb_char(48, 4, 'X', COL_YELLOW);
    uint32_t cs = fb_checksum();
    report("DRAW", cs, EXPECT_DRAW);
    if (cs == EXPECT_DRAW) { kputs("DRAW_PASS\n"); return 1; }
    kputs("[DRAW] pixel mismatch (check fb_fill_rect)\n");
    return 0;
}

static int render_test(void) {
    fb_clear(COL_BLACK);
    render_doc(DOC);
    uint32_t cs = fb_checksum();
    report("RENDER", cs, EXPECT_RENDER);
    if (cs == EXPECT_RENDER) { kputs("RENDER_PASS\n"); return 1; }
    kputs("[RENDER] pixel mismatch (check html tag parsing)\n");
    return 0;
}

void kmain(void) {
    kputs("\n[S12] simple GUI: software framebuffer ");
    kputdec(FB_W);
    console_putchar('x');
    kputdec(FB_H);
    kputs(" (no real display)\n");
    int ok = 1;
    ok &= draw_test();
    ok &= render_test();
    if (ok) kputs("ALL_PASS\n");
}
