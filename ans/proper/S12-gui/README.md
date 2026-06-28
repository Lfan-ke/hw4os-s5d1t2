# 正经·S12 · 简易 GUI（软件 framebuffer 与 html/css 子集转译绘制）

> 承接 S06（驱动）/S10（用户态）。本课不接真显示器，而是在内核里实现一块**软件 framebuffer**，写齐"画点/矩形/字符"三件套，再把一段**极简标记文档**（类 html/css 子集）**转译绘制**到 framebuffer，最后用确定性**校验和**证明每个像素都画对了。

## 0. 这节课在讲什么

GUI 的最底层只有一件事：往一块**显存（framebuffer）**里写像素，硬件把这块内存周期性扫描到屏幕。所以"图形"本质是对一个二维字节数组做内存写。本实验：

- framebuffer = 内核里的 `uint8_t fb[FB_H*FB_W]`，每像素 1 字节调色板索引（`0` 黑底，`1` 红 …）。真机里它会被 virtio-GPU/显存映射出去；这里我们**只在内存里画**，不接外设，用校验和代替肉眼看图（错一个像素，FNV-1a 校验和就变）。
- 绘图原语：`fb_point`（带越界裁剪）、`fb_fill_rect`（实心矩形）、`fb_char`（8x8 字模）。
- 转译绘制：把一段文档解析成绘制命令——`{rect x y w h color}`（类 CSS 盒子）和 `<div style="color:NAME">TEXT</div>`（类 html 标签）——渲染成另一帧。

两个子测试：

1. **DRAW**：直接用原语画一帧（清屏 + 矩形 + 点 + 字符），校验和比对 → `DRAW_PASS`。
2. **RENDER**：解析文档渲染一帧，校验和比对 → `RENDER_PASS`。

都过 → `ALL_PASS`。

## 1. 你要实现的（两处 TODO）

unikernel 式单镜像，无 U 态、无 trap，纯软件绘制逻辑。

- **TODO(1) `kernel/gui.c` 的 `fb_fill_rect(x,y,w,h,color)`**：实心矩形。
  ```c
  for (int dy = 0; dy < h; dy++)
      for (int dx = 0; dx < w; dx++)
          fb_point(x + dx, y + dy, color);   /* fb_point 自带裁剪 */
  ```
- **TODO(2) `kernel/render.c` 的 `parse_div(&p)`**：解析 `<div style="color:NAME">TEXT</div>`。
  - 在 `>` 之前定位 `color:`，读出颜色名 → `color_by_name()` 取索引；
  - 越过 `>`，从这里**逐字符** `fb_char(x, div_y, ch, col)` 直到遇到下一个 `<`（即 `</div>`），每字符 `x += 8`；
  - `div_y += 9` 让下一个 `<div>` 另起一行，再跳过 `</div>`。

`{rect ...}` 命令的解析（`parse_rect`）、`fb_point` / `fb_char` / 8x8 字模 / 校验和 / 颜色名表都已给好。

## 2. 跑

```
labctl run proper/S12-gui
make -C kernel run     # 手动跑（OpenSBI banner 后见内核输出）
```

参考解输出（节选）：

```
[S12] simple GUI: software framebuffer 64x32 (no real display)
[DRAW] checksum=0x...f77f2ab5 expect=0x...f77f2ab5
DRAW_PASS
[RENDER] checksum=0x...b7524475 expect=0x...b7524475
RENDER_PASS
ALL_PASS
```

## 3. 完成标准 (DoD)

- [ ] `fb_fill_rect` 用双重循环逐像素调 `fb_point`，矩形部分出界时靠裁剪不崩。
- [ ] `parse_div` 能在 `>` 前定位 `color:` 取色、从 `>` 后逐字符 `fb_char` 绘制到下一个 `<`，每字符 `x += 8`、每行 `div_y += 9`。
- [ ] `make -C kernel run` 输出 `DRAW_PASS`、`RENDER_PASS`、`ALL_PASS`，两个 FNV-1a 校验和与期望值**逐位**相符。
- [ ] 全程无 `FAIL` / `panic` / `UNEXPECTED`。

## 4. 引申

本实验为聚焦“绘制与转译的逻辑正确性”做了大量简化：framebuffer 只是内存数组、每像素 1 字节调色板索引、字模写死 8x8、布局是固定坐标、看图用校验和代替。想做得更真实，可按兴趣往这些方向深入：

1. **接真显示：virtio-GPU**。把 `fb[]` 当资源在 host 侧建好（`RESOURCE_CREATE_2D` + `RESOURCE_ATTACH_BACKING`），`SET_SCANOUT` 绑到扫描输出，每帧 `TRANSFER_TO_HOST_2D` + `RESOURCE_FLUSH` 把脏区刷出去——这时校验和就换成肉眼可见的真画面。对照 Linux 的 **DRM/KMS** 与 fbdev/fbcon。
2. **像素格式与双缓冲**。1 字节调色板索引升级成 **RGB565 / RGBA8888** 真彩色；加 **double buffering**（前后台帧切换）消除撕裂，再加**脏矩形**只刷变化区域而非整帧。
3. **完整 CSS 盒模型与排版**。现在 `<div>` 只是固定行高换行；扩成 margin/padding/border + 正常流（flow）/换行/嵌套，做一个最小布局引擎，把 html/css 子集真正“排版”而非定点画。
4. **真字体渲染**。8x8 位图字模 → 接 **TrueType**（FreeType 风格）+ 抗锯齿 + UTF-8/CJK 字形，处理字间距与基线。
5. **上层 GUI 栈**。在原语之上封装控件库（按钮/文本框）、事件循环与窗口**合成器（compositor）**，对照 **Wayland** 的合成模型或 X11 的绘图模型。
6. **输入与交互**。接 virtio-input（键盘/鼠标），把“画一帧”变成“响应事件重绘”，形成最小交互式 GUI。

## 文件

- `kernel/main.c`：测试 harness（DRAW / RENDER + 期望校验和）。**勿改**。
- `kernel/gui.c`：framebuffer + 绘图原语 + 8x8 字模 + 校验和（含 TODO(1)）。
- `kernel/render.c`：极简标记解析与渲染（含 TODO(2)）。
- `kernel/gui.h`：共享声明。
