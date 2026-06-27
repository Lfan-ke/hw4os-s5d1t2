/* S10b · cpio(newc) 解包器：把内嵌 initramfs 逐条灌进 RAM-fs（填空版）。
 *
 * newc 归档 = 若干「记录」首尾相接，每条记录：
 *   ┌──────────────┬───────────────┬──────────┬───────────┬──────────┐
 *   │ 110B ASCII 头 │ name(namesize) │ pad→4B   │ data(size) │ pad→4B   │
 *   └──────────────┴───────────────┴──────────┴───────────┴──────────┘
 * 头全是定长 ASCII 十六进制字段（每字段 8 个字符），布局（字节偏移）：
 *   0  magic "070701"   6  c_ino     14 c_mode    22 c_uid     30 c_gid
 *   38 c_nlink         46 c_mtime   54 c_filesize 62 c_devmajor 70 c_devminor
 *   78 c_rdevmajor     86 c_rdevminor 94 c_namesize 102 c_check
 * 名字紧跟在 110B 头后（含结尾 0）；(头+名字) 补齐到 4 字节边界后才是数据；
 * 数据再补齐到 4 字节边界即下一条头。归档以名为 "TRAILER!!!" 的空记录收尾。 */
#include "app.h"
#include "fs.h"

/* 8 个 ASCII 十六进制字符 → uint32（已给）。 */
uint32_t cpio_hex8(const uint8_t *p) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t c = p[i];
        uint32_t d = 0;
        if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        v = (v << 4) | d;
    }
    return v;
}

/* 4 字节向上对齐。 */
static uint32_t align4(uint32_t x) { return (x + 3u) & ~3u; }

/* =========================================================================
 * 填空点 1：解析归档中偏移 off 处的一条 newc 头。
 *   - 越界/魔数不符：返回 0（已给守卫，勿删）。
 *   - 读出 namesize(头+94)、filesize(头+54) 两个 8 字符 ASCII 十六进制字段：
 *       uint32_t namesize = cpio_hex8(h + 94);
 *       uint32_t filesize = cpio_hex8(h + 54);
 *   - name 紧跟 110B 头之后：const char *name = (const char *)(h + 110);
 *   - 数据起始（绝对偏移）= off + align4(110 + namesize)；
 *     下一条头偏移      = 数据起始 + align4(filesize)。
 *   - 若 kstreq(name, "TRAILER!!!")（归档结束哨兵）：返回 0。
 *   - 否则填好 v->name / v->namesize / v->filesize / v->data(=arc+数据偏移) /
 *     v->next，返回 1。
 * HINT：cpio_hex8 / align4 / kstreq 已给；偏移用「绝对偏移」算，data 用 arc+偏移。
 * ========================================================================= */
static int cpio_parse_one(const uint8_t *arc, uint32_t len,
                          uint32_t off, struct cpio_hdr_view *v) {
    /* —— 守卫（已给，勿删）：至少要容得下一个 110 字节头 —— */
    if (off + 110u > len) return 0;
    const uint8_t *h = arc + off;
    if (!(h[0] == '0' && h[1] == '7' && h[2] == '0' &&
          h[3] == '7' && h[4] == '0' && h[5] == '1'))
        return 0; /* 魔数必须是 "070701" */

    (void)align4;
    (void)v;
    /* TODO: 解析这条头（见上）。下面占位让解包得不到任何文件：*/
    return 0;
}

/* —— 逐条解开归档、灌进 RAM-fs（已给：循环骨架调用上面的解析）。 —— */
int cpio_unpack(const uint8_t *arc, uint32_t len) {
    uint32_t off = 0;
    int count = 0;
    struct cpio_hdr_view v;
    /* count 上限做安全护栏：解析填错也不会无限循环。 */
    while (count < 64 && cpio_parse_one(arc, len, off, &v)) {
        int inum = fs_create(v.name);          /* 在根目录建文件 */
        if (inum >= 0)
            fs_write((uint32_t)inum, v.data, v.filesize); /* 写入内容 */
        count++;
        off = v.next;
    }
    return count;
}
