/* 16 · 驱动入门 —— C（参考解）。
 * 一节贯通四子题：① 裸机 MMIO 手工艺 ② 设备树解析 + compatible 匹配
 * ③ driver derive 可插拔注册（链接段自发现） ④ 平台总线 + 用户态 /dev。
 * 设备/MMIO 全部用软件寄存器模型建模（host 直接跑，纯逻辑）。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ====================================================================
 * 16.1 裸机 MMIO 手工艺人
 * ==================================================================== */
#define REG_ID 0x0u
#define REG_CTRL 0x4u
#define REG_STATUS 0x8u
#define REG_DATA 0xCu
#define DEV_MAGIC 0x426C6E6Bu /* "Blnk" */
#define CTRL_ENABLE 1u
#define STATUS_READY 1u

typedef struct {
    int enabled;
    uint8_t last;
    uint8_t out[64];
    int out_len;
} MmioDevice;

/* 设备对一次 MMIO 写的反应（寄存器状态机）。 */
static void bus_write(MmioDevice *d, unsigned off, uint32_t val) {
    if (off == REG_CTRL) {
        d->enabled = (val & CTRL_ENABLE) != 0;
    } else if (off == REG_DATA) {
        if (d->enabled && d->out_len < (int)sizeof d->out) {
            d->last = (uint8_t)val;
            d->out[d->out_len++] = (uint8_t)val;
        }
    }
    /* ID/STATUS 只读，写被忽略 */
}
static uint32_t bus_read(const MmioDevice *d, unsigned off) {
    switch (off) {
    case REG_ID: return DEV_MAGIC;
    case REG_CTRL: return d->enabled ? CTRL_ENABLE : 0;
    case REG_STATUS: return d->enabled ? STATUS_READY : 0;
    case REG_DATA: return d->last;
    default: return 0;
    }
}
/* 模拟 readl/writel：对“地址”的 volatile 读写。 */
static uint32_t mmio_read(const MmioDevice *d, unsigned off) { return bus_read(d, off); }
static void mmio_write(MmioDevice *d, unsigned off, uint32_t v) { bus_write(d, off, v); }

/* ── 学生填 16.1 ── */

/* probe：volatile 读 ID，与 DEV_MAGIC 比对。 */
static int driver_probe(const MmioDevice *d) {
    return mmio_read(d, REG_ID) == DEV_MAGIC;
}

/* 握手 + 突发收发：① 置 CTRL 使能 ② 轮询 STATUS.ready ③ 逐字节写 DATA。 */
static void driver_io(MmioDevice *d, const uint8_t *msg, int n) {
    /* TODO[a] 忙等轮询 STATUS 单字节握手（本参考解）： */
    mmio_write(d, REG_CTRL, CTRL_ENABLE);
    int spun = 0;
    while ((mmio_read(d, REG_STATUS) & STATUS_READY) == 0) {
        if (++spun > 10000) break; /* 防呆超时 */
    }
    for (int i = 0; i < n; i++) mmio_write(d, REG_DATA, msg[i]);
    /* ELSE[b] 也可：读“可写计数”后一次性突发写——对外输出一致。 */
}

static int sub_mmio(void) {
    MmioDevice dev;
    memset(&dev, 0, sizeof dev);
    int ok = 1;
    if (driver_probe(&dev)) {
        printf("PROBE_PASS 读到 magic 0x%08X\n", DEV_MAGIC);
    } else {
        printf("PROBE_FAIL ID 未读到 magic 0x%08X\n", DEV_MAGIC);
        ok = 0;
    }
    const uint8_t msg[] = {'D', 'R', 'V', '-', 'O', 'K'};
    driver_io(&dev, msg, 6);
    if (dev.enabled && dev.out_len == 6 && memcmp(dev.out, msg, 6) == 0) {
        printf("IO_PASS 设备经握手收到 %d 字节\n", dev.out_len);
    } else {
        printf("IO_FAIL enabled=%d out_len=%d\n", dev.enabled, dev.out_len);
        ok = 0;
    }
    if (ok) printf("MMIO_PASS\n");
    return ok;
}

/* ====================================================================
 * 16.2 设备树：DTS → dtc → dtb → 解析 → compatible 匹配
 * ==================================================================== */
#define FDT_MAGIC 0xd00dfeedu
#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE 0x2u
#define FDT_PROP 0x3u
#define FDT_NOP 0x4u
#define FDT_END 0x9u

typedef struct {
    char name[32];
    char compatible[32];
    uint32_t base, size;
} DevNode;

typedef struct { uint32_t state; } DevFile;
typedef DevFile (*probe_fn)(uint32_t base, uint32_t size);
typedef struct {
    const char *compatible;
    const char *name;
    probe_fn probe;
} Driver;

static void put_be32(uint8_t *b, int *p, uint32_t x) {
    b[*p] = (uint8_t)(x >> 24);
    b[*p + 1] = (uint8_t)(x >> 16);
    b[*p + 2] = (uint8_t)(x >> 8);
    b[*p + 3] = (uint8_t)x;
    *p += 4;
}
static uint32_t get_be32(const uint8_t *b, int o) {
    return ((uint32_t)b[o] << 24) | ((uint32_t)b[o + 1] << 16) |
           ((uint32_t)b[o + 2] << 8) | (uint32_t)b[o + 3];
}

/* 序列化节点表为真正的 FDT 二进制（大端线格式）。返回总字节数。 */
static int build_fdt(const DevNode *nodes, int n, uint8_t *out) {
    uint8_t str[64];
    int sp = 0;
    uint32_t comp_off = (uint32_t)sp;
    memcpy(str + sp, "compatible", 10);
    sp += 10;
    str[sp++] = 0;
    uint32_t reg_off = (uint32_t)sp;
    memcpy(str + sp, "reg", 3);
    sp += 3;
    str[sp++] = 0;

    uint8_t st[768];
    int p = 0;
    put_be32(st, &p, FDT_BEGIN_NODE);
    st[p++] = 0; /* 根名 "" */
    while (p % 4) st[p++] = 0;
    for (int i = 0; i < n; i++) {
        put_be32(st, &p, FDT_BEGIN_NODE);
        int l = (int)strlen(nodes[i].name);
        memcpy(st + p, nodes[i].name, (size_t)l);
        p += l;
        st[p++] = 0;
        while (p % 4) st[p++] = 0;
        int cl = (int)strlen(nodes[i].compatible);
        put_be32(st, &p, FDT_PROP);
        put_be32(st, &p, (uint32_t)(cl + 1));
        put_be32(st, &p, comp_off);
        memcpy(st + p, nodes[i].compatible, (size_t)cl);
        p += cl;
        st[p++] = 0;
        while (p % 4) st[p++] = 0;
        put_be32(st, &p, FDT_PROP);
        put_be32(st, &p, 8);
        put_be32(st, &p, reg_off);
        put_be32(st, &p, nodes[i].base);
        put_be32(st, &p, nodes[i].size);
        put_be32(st, &p, FDT_END_NODE);
    }
    put_be32(st, &p, FDT_END_NODE);
    put_be32(st, &p, FDT_END);

    int off_mem = 40, mem_sz = 16;
    int off_struct = off_mem + mem_sz;
    int off_strings = off_struct + p;
    int total = off_strings + sp;
    int o = 0;
    put_be32(out, &o, FDT_MAGIC);
    put_be32(out, &o, (uint32_t)total);
    put_be32(out, &o, (uint32_t)off_struct);
    put_be32(out, &o, (uint32_t)off_strings);
    put_be32(out, &o, (uint32_t)off_mem);
    put_be32(out, &o, 17);
    put_be32(out, &o, 16);
    put_be32(out, &o, 0);
    put_be32(out, &o, (uint32_t)sp);
    put_be32(out, &o, (uint32_t)p);
    memset(out + o, 0, (size_t)mem_sz);
    o += mem_sz;
    memcpy(out + o, st, (size_t)p);
    o += p;
    memcpy(out + o, str, (size_t)sp);
    o += sp;
    return total;
}

/* 大端遍历 FDT 结构块，取每个设备节点的 compatible / reg。返回节点数。 */
static int parse_fdt(const uint8_t *b, DevNode *out) {
    int off_struct = (int)get_be32(b, 8);
    int off_strings = (int)get_be32(b, 12);
    int pos = off_struct, count = 0;
    DevNode stack[8];
    int sp = 0;
    for (;;) {
        uint32_t tok = get_be32(b, pos);
        pos += 4;
        if (tok == FDT_BEGIN_NODE) {
            DevNode nd;
            memset(&nd, 0, sizeof nd);
            int l = (int)strlen((const char *)b + pos);
            if (l < 32) memcpy(nd.name, b + pos, (size_t)l);
            pos += (l + 1 + 3) & ~3;
            stack[sp++] = nd;
        } else if (tok == FDT_PROP) {
            int len = (int)get_be32(b, pos);
            int nameoff = (int)get_be32(b, pos + 4);
            pos += 8;
            const char *pname = (const char *)b + off_strings + nameoff;
            const uint8_t *val = b + pos;
            if (sp > 0) {
                DevNode *top = &stack[sp - 1];
                if (strcmp(pname, "compatible") == 0) {
                    if (len < 32) memcpy(top->compatible, val, (size_t)len);
                } else if (strcmp(pname, "reg") == 0 && len >= 8) {
                    top->base = get_be32(val, 0);
                    top->size = get_be32(val, 4);
                }
            }
            pos += (len + 3) & ~3;
        } else if (tok == FDT_END_NODE) {
            if (sp > 0) {
                sp--;
                if (stack[sp].name[0] != 0) out[count++] = stack[sp];
            }
        } else if (tok == FDT_NOP) {
            /* skip */
        } else {
            break; /* FDT_END */
        }
    }
    return count;
}

static DevFile probe_blink(uint32_t base, uint32_t size) {
    (void)base; (void)size;
    DevFile f = {0};
    return f;
}
static DevFile probe_gpio(uint32_t base, uint32_t size) {
    (void)base; (void)size;
    DevFile f = {0};
    return f;
}

/* 16.2 阶段固定驱动表：此时只有 blink/gpio（blink-v2 尚未注册）。 */
static const Driver base_table[2] = {
    {"acme,blink", "blink", probe_blink},
    {"acme,gpio", "gpio", probe_gpio},
};

/* 等价于 board.dts 的节点表。学生填 blink/gpio 的 compatible 与 reg。 */
static int board_nodes(DevNode *out) {
    /* blink@10001000：compatible="acme,blink"，reg=<0x10001000 0x1000> */
    strcpy(out[0].name, "blink@10001000");
    strcpy(out[0].compatible, "acme,blink");
    out[0].base = 0x10001000u;
    out[0].size = 0x1000u;
    /* gpio@10002000：compatible="acme,gpio"，reg=<0x10002000 0x1000> */
    strcpy(out[1].name, "gpio@10002000");
    strcpy(out[1].compatible, "acme,gpio");
    out[1].base = 0x10002000u;
    out[1].size = 0x1000u;
    /* lamp@10003000（给定）：16.2 时未知，16.3 注册 blink-v2 后被发现。 */
    strcpy(out[2].name, "lamp@10003000");
    strcpy(out[2].compatible, "acme,blink-v2");
    out[2].base = 0x10003000u;
    out[2].size = 0x1000u;
    return 3;
}

/* ── 学生填 16.2：解析主循环里的匹配 ──
 * 遍历节点，在驱动表里逐条精确字符匹配 compatible；命中即 probe 并记录；
 * 未知项走 fallback 跳过。返回跳过数；命中串 "name@base" 写入 matched[]。 */
static int match_and_probe(const DevNode *nodes, int nn, const Driver *drv, int nd,
                           char matched[][32], int *mcount) {
    int skipped = 0;
    *mcount = 0;
    for (int i = 0; i < nn; i++) {
        int hit = 0;
        for (int j = 0; j < nd; j++) {
            if (strcmp(drv[j].compatible, nodes[i].compatible) == 0) {
                DevFile inst = drv[j].probe(nodes[i].base, nodes[i].size);
                (void)inst;
                printf("PROBE %s@%x\n", drv[j].name, nodes[i].base);
                snprintf(matched[*mcount], 32, "%s@%x", drv[j].name, nodes[i].base);
                (*mcount)++;
                hit = 1;
                break;
            }
        }
        if (!hit) skipped++; /* 未知 compatible：fallback 跳过 */
    }
    return skipped;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static int sub_dtb(void) {
    DevNode nodes[8];
    int n = board_nodes(nodes);
    uint8_t blob[1024];
    build_fdt(nodes, n, blob);
    DevNode parsed[8];
    int np = parse_fdt(blob, parsed);
    int ok = 1;

    uint32_t magic = get_be32(blob, 0);
    if (magic == FDT_MAGIC && np == 3) {
        printf("DTB_PASS magic=0x%08x nodes=%d\n", magic, np);
    } else {
        printf("DTB_FAIL magic=0x%08x nodes=%d\n", magic, np);
        ok = 0;
    }

    char m1[8][32];
    int c1;
    int sk1 = match_and_probe(parsed, np, base_table, 2, m1, &c1);
    /* 隐藏向量：打乱节点顺序后结果应不变（按名片而非位置匹配）。 */
    DevNode shuf[8];
    for (int i = 0; i < np; i++) shuf[i] = parsed[np - 1 - i];
    char m2[8][32];
    int c2;
    int sk2 = match_and_probe(shuf, np, base_table, 2, m2, &c2);

    qsort(m1, (size_t)c1, 32, cmp_str);
    qsort(m2, (size_t)c2, 32, cmp_str);
    int same = (c1 == c2);
    for (int i = 0; i < c1 && same; i++) same = (strcmp(m1[i], m2[i]) == 0);
    int correct = (c1 == 2 && strcmp(m1[0], "blink@10001000") == 0 &&
                   strcmp(m1[1], "gpio@10002000") == 0);
    if (sk1 == 1 && sk2 == 1 && same && correct) {
        printf("MATCH_PASS 命中 blink/gpio、跳过未知 acme,blink-v2、乱序不变\n");
    } else {
        printf("MATCH_FAIL skipped=%d/%d count=%d/%d\n", sk1, sk2, c1, c2);
        ok = 0;
    }
    return ok;
}

/* ====================================================================
 * 16.3 driver derive：链接段自发现注册（加一个驱动 = 加一个标注）
 * ==================================================================== */
/* 注册标注：仿 Linux initcall，把“指向 Driver 的指针”塞进自定义链接段 "drivers"，
 * 框架遍历 __start/__stop 自发现。存指针（而非结构体）可避开段内对齐补位带来的步长问题。 */
#define REGISTER_DRIVER(sym, compat, nm, pfn)                       \
    static const Driver sym##_def = {compat, nm, pfn};              \
    static const Driver *const sym __attribute__((used, section("drivers"))) = &sym##_def;
extern const Driver *const __start_drivers[];
extern const Driver *const __stop_drivers[];

/* 给定的两个驱动登记 */
REGISTER_DRIVER(_drv_blink, "acme,blink", "blink", probe_blink)
REGISTER_DRIVER(_drv_gpio, "acme,gpio", "gpio", probe_gpio)
/* ── 学生填 16.3：新增第三个驱动的注册标注（框架遍历代码一行不改）── */
REGISTER_DRIVER(_drv_blink2, "acme,blink-v2", "blink2", probe_blink)

/* 框架侧：遍历注册段得到驱动表（零改、自发现）。
 * 链接器可能在段内对齐补零，遍历时跳过空洞（compatible==NULL）即可。 */
static int collect_drivers(Driver *out) {
    int c = 0;
    for (const Driver *const *pp = __start_drivers; pp < __stop_drivers; pp++)
        if (*pp != NULL && (*pp)->compatible != NULL) out[c++] = **pp;
    return c;
}

static int sub_derive(void) {
    DevNode nodes[8];
    int n = board_nodes(nodes);
    uint8_t blob[1024];
    build_fdt(nodes, n, blob);
    DevNode parsed[8];
    int np = parse_fdt(blob, parsed);

    Driver all[16];
    int nd = collect_drivers(all); /* 框架侧关键一行：从注册段收集驱动表 */

    char m[8][32];
    int c;
    int skipped = match_and_probe(parsed, np, all, nd, m, &c);
    int found = 0;
    for (int i = 0; i < c; i++)
        if (strcmp(m[i], "blink2@10003000") == 0) found = 1;
    if (skipped == 0 && found) {
        printf("DERIVE_PASS 新驱动 acme,blink-v2 被自动发现并 probe\n");
        printf("PLUG_PASS 框架遍历逻辑零改，加一个标注即接纳新驱动\n");
        return 1;
    }
    printf("DERIVE_FAIL skipped=%d found_v2=%d nd=%d\n", skipped, found, nd);
    return 0;
}

/* ====================================================================
 * 16.4 平台总线（简化）+ 用户态访问
 * ==================================================================== */
typedef struct {
    char path[24];
    DevFile file;
} BusDev;
typedef struct {
    BusDev devs[8];
    int n;
} Bus;

/* DevFile 的 FileLike read/write（复用“设备即文件”抽象）。 */
static uint32_t file_read(const DevFile *f) { return f->state; }
static void file_write(DevFile *f, uint32_t v) { f->state = v; }

/* ── 学生填 16.4：总线枚举 → match → bind → 登记 /dev ── */
static void bind_all(Bus *bus, const DevNode *nodes, int nn, const Driver *drv, int nd) {
    bus->n = 0;
    for (int i = 0; i < nn; i++) {
        for (int j = 0; j < nd; j++) {
            if (strcmp(drv[j].compatible, nodes[i].compatible) == 0) {
                DevFile inst = drv[j].probe(nodes[i].base, nodes[i].size); /* bind=调 probe */
                /* 同名驱动的实例编号 */
                int idx = 0;
                for (int k = 0; k < bus->n; k++) {
                    char pre[24];
                    snprintf(pre, sizeof pre, "/dev/%s", drv[j].name);
                    if (strncmp(bus->devs[k].path, pre, strlen(pre)) == 0) idx++;
                }
                snprintf(bus->devs[bus->n].path, 24, "/dev/%s%d", drv[j].name, idx);
                bus->devs[bus->n].file = inst;
                bus->n++;
                break;
            }
        }
    }
}

/* 用户态 open+read：找到 /dev 路径的 FileLike，转发 read（不直接碰 MMIO）。 */
static int user_read(const Bus *bus, const char *path, uint32_t *out) {
    for (int i = 0; i < bus->n; i++)
        if (strcmp(bus->devs[i].path, path) == 0) {
            *out = file_read(&bus->devs[i].file);
            return 1;
        }
    return 0;
}
static int user_write(Bus *bus, const char *path, uint32_t v) {
    for (int i = 0; i < bus->n; i++)
        if (strcmp(bus->devs[i].path, path) == 0) {
            file_write(&bus->devs[i].file, v);
            return 1;
        }
    return 0;
}

static int sub_bus(void) {
    DevNode nodes[8];
    int n = board_nodes(nodes);
    uint8_t blob[1024];
    build_fdt(nodes, n, blob);
    DevNode parsed[8];
    int np = parse_fdt(blob, parsed);
    Driver all[16];
    int nd = collect_drivers(all);

    Bus bus;
    bind_all(&bus, parsed, np, all, nd);
    int ok = 1;

    int has_b0 = 0, has_g0 = 0, has_b20 = 0;
    for (int i = 0; i < bus.n; i++) {
        if (strcmp(bus.devs[i].path, "/dev/blink0") == 0) has_b0 = 1;
        if (strcmp(bus.devs[i].path, "/dev/gpio0") == 0) has_g0 = 1;
        if (strcmp(bus.devs[i].path, "/dev/blink20") == 0) has_b20 = 1;
    }
    if (bus.n == 3 && has_b0 && has_g0 && has_b20) {
        printf("BIND_PASS 绑定 /dev/blink0 /dev/gpio0 /dev/blink20\n");
    } else {
        printf("BIND_FAIL n=%d\n", bus.n);
        ok = 0;
    }

    uint32_t before = 0xFFFF, after = 0xFFFF;
    int r1 = user_read(&bus, "/dev/blink0", &before);
    int w = user_write(&bus, "/dev/blink0", 0xA5);
    int r2 = user_read(&bus, "/dev/blink0", &after);
    if (r1 && w && r2 && before == 0 && after == 0xA5) {
        printf("USER_PASS open/read/write 转发一致（写改状态、读回一致）\n");
    } else {
        printf("USER_FAIL before=%u after=%u\n", before, after);
        ok = 0;
    }

    if (ok) printf("BUS_PASS\n");
    return ok;
}

int main(void) {
    int all = 1;
    all &= sub_mmio();
    all &= sub_dtb();
    all &= sub_derive();
    all &= sub_bus();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
