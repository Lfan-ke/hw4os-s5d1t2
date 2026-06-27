/* 简化 VLAN Tag 处理 —— 软件路径（C 参考解）。
 * 包字: [31]VALID [30]HAS_TAG [29]DROP [28]DIR(in) [21:16]VID(6b) [15:0]PAYLOAD
 *
 * 学生版只需填 process() 函数体；下方测试 harness（向量 + PASS 打印）给好。
 */
#include <stdio.h>
#include <stdint.h>

#define VALID    (1u << 31)
#define HAS_TAG  (1u << 30)
#define DROP     (1u << 29)
#define DIR      (1u << 28)
#define VID_SH   16u
#define VID_MASK 0x3Fu

#define ACCESS 0u
#define TRUNK  1u
#define HYBRID 2u

static uint32_t strip(uint32_t in)               { return VALID | (in & 0xFFFFu); }
static uint32_t insert(uint32_t in, uint32_t pv) { return VALID | HAS_TAG | ((pv & VID_MASK) << VID_SH) | (in & 0xFFFFu); }
static uint32_t keep(uint32_t in)                { return VALID | (in & (HAS_TAG | (VID_MASK << VID_SH) | 0xFFFFu)); }
static uint32_t drop_pkt(void)                   { return VALID | DROP; }

/* 同一逻辑的“软件 if-else”实现（硬件路径是同一函数的组合逻辑）。 */
static uint32_t process(uint32_t mode, uint32_t pvid, uint64_t allow, uint64_t untag, uint32_t in) {
    int has_tag  = (in & HAS_TAG) != 0;
    int egress   = (in & DIR) != 0;
    uint32_t vid = (in >> VID_SH) & VID_MASK;
    if (!egress) {
        if (mode == ACCESS) return has_tag ? strip(in) : insert(in, pvid);
        if (!has_tag) return drop_pkt();
        if (((allow >> vid) & 1u) == 0) return drop_pkt();
        return keep(in);
    } else {
        if (mode == ACCESS) return strip(in);
        if (mode == TRUNK)  return keep(in);
        return (has_tag && ((untag >> vid) & 1u)) ? strip(in) : keep(in);
    }
}

/* ─────────────────── 测试 harness（给好，勿改）─────────────────── */

static uint32_t mk(uint32_t dir, uint32_t tag, uint32_t vid, uint32_t pl) {
    return VALID | (dir ? DIR : 0) | (tag ? HAS_TAG : 0) | ((vid & VID_MASK) << VID_SH) | (pl & 0xFFFFu);
}

typedef struct { uint32_t in, exp; } Case;

static int run_group(const char *name, uint32_t mode, uint32_t pvid,
                     uint64_t allow, uint64_t untag, const Case *cases, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        uint32_t got = process(mode, pvid, allow, untag, cases[i].in);
        if (got != cases[i].exp) {
            printf("%s_FAIL case#%d in=0x%08X exp=0x%08X got=0x%08X\n",
                   name, i, cases[i].in, cases[i].exp, got);
            ok = 0;
        }
    }
    if (ok) printf("%s_PASS\n", name);
    return ok;
}

int main(void) {
    uint64_t allow = (1ull << 10) | (1ull << 20);
    uint64_t untag = (1ull << 10);
    int all = 1;

    Case access_cases[] = {
        { mk(0,1,10,0x1234), VALID | 0x1234 },
        { mk(0,0,0,0x1234),  VALID | HAS_TAG | (5u << 16) | 0x1234 },
        { mk(1,1,10,0x1234), VALID | 0x1234 },
    };
    all &= run_group("ACCESS", ACCESS, 5, 0, 0, access_cases, 3);

    Case trunk_cases[] = {
        { mk(0,1,10,0xABCD), VALID | HAS_TAG | (10u << 16) | 0xABCD },
        { mk(0,1,30,0x1111), VALID | DROP },
        { mk(0,0,0,0x2222),  VALID | DROP },
        { mk(1,1,10,0xABCD), VALID | HAS_TAG | (10u << 16) | 0xABCD },
    };
    all &= run_group("TRUNK", TRUNK, 0, allow, 0, trunk_cases, 4);

    Case hybrid_cases[] = {
        { mk(0,1,20,0x0F0F), VALID | HAS_TAG | (20u << 16) | 0x0F0F },
        { mk(0,1,30,0x3333), VALID | DROP },
        { mk(1,1,10,0x0F0F), VALID | 0x0F0F },
        { mk(1,1,20,0x0F0F), VALID | HAS_TAG | (20u << 16) | 0x0F0F },
    };
    all &= run_group("HYBRID", HYBRID, 0, allow, untag, hybrid_cases, 4);

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
