/* 三态特权机 —— 软件路径（C）。
 * 状态字 csr[4:0] = { saved_priv[4:3], feat_en[2], cur_priv[1:0] }
 * 操作 op = (kind, arg_priv, arg_en)；纯函数 step(csr, op) → (csr', trap)。
 *
 * 特权级：A(最高,≈M)=2，B(≈S)=1，C(最低,≈U)=0。
 * 你只需填 step() 各 kind 分支；下方测试 harness（向量 + PASS 打印）勿改。
 * 四条路径（rust/c/v/bsv）输出必须逐位一致。
 */
#include <stdio.h>
#include <stdint.h>

/* 特权级编码（数值越大权限越高） */
#define A 2u /* 最高 */
#define B 1u
#define C 0u /* 最低 */

/* 操作 kind */
#define NORMAL  0u
#define DROP    1u
#define ECALL   2u
#define XRET    3u
#define SETFEAT 4u
#define USEFEAT 5u

static uint32_t cur_of(uint32_t csr) { return csr & 0x3u; }
static uint32_t fe_of (uint32_t csr) { return (csr >> 2) & 0x1u; }
static uint32_t sp_of (uint32_t csr) { return (csr >> 3) & 0x3u; }
static uint32_t pack(uint32_t cur, uint32_t fe, uint32_t sp) {
    return ((sp & 0x3u) << 3) | ((fe & 0x1u) << 2) | (cur & 0x3u);
}

/* 核心：与硬件 priv_gate 同构的纯逻辑。把 out_csr 与 out_trap 写出。 */
static void step(uint32_t csr, uint32_t kind, uint32_t arg_priv, uint32_t arg_en,
                 uint32_t *out_csr, int *out_trap) {
    uint32_t cur = cur_of(csr);
    uint32_t fe  = fe_of(csr);
    uint32_t sp  = sp_of(csr);

    /* 默认：csr 不变、不陷入。各分支只改需要改的。 */
    uint32_t ncur = cur, nfe = fe, nsp = sp;
    int trap = 0;
    (void)arg_priv; (void)arg_en; /* 填完逻辑后这两行可删 */

    switch (kind) {
    /* 子实验 1（CMP）：特权比较器——“有没有权限”就是一根 cur < need 的线。 */
    case NORMAL:
        /* TODO[a]（推荐）：一行比较器 —— trap = (cur < arg_priv);
         * ELSE[b]：把 3x3 等级关系展开成显式真值表（C,B)/(C,A)/(B,A) 时 trap=1。 */
        break;

    /* 子实验 2（DROP）：向下放权 = 写低位。A→B→C 自由下行；上行非法。 */
    case DROP:
        /* TODO: arg_priv > cur → trap=1（不许直接提权）；否则 ncur = arg_priv。 */
        break;

    /* 子实验 3（TRAP）：陷入提权 + 返回。 */
    case ECALL:
        /* TODO: 合法陷入 —— nsp = cur（保存前态，同 MPP/SPP）；ncur = A（进最高态）。 */
        break;
    case XRET:
        /* TODO[a]：用 2 位 saved_priv 存完整前态 ——
         *   cur != A → trap=1（非最高态不得 xret）；否则 ncur = sp。
         * ELSE[b]：仿真实 sstatus.SPP 只用 1 位，并在 THINKING.md 讨论三态信息为何不足。 */
        break;

    /* 子实验 4（FEAT）：开启功能也是置位。能力 = 特权够 且 使能位亮。 */
    case SETFEAT:
        /* TODO: cur < B → trap=1（配置使能位至少要 B 态）；否则 nfe = arg_en。 */
        break;
    case USEFEAT:
        /* TODO: 缺一不可 —— trap = (cur < arg_priv) || (fe == 0)。 */
        break;

    default:
        break;
    }

    *out_csr  = pack(ncur, nfe, nsp);
    *out_trap = trap;
}

/* ─────────────────── 测试 harness（勿改）─────────────────── */

static uint32_t mkcsr(uint32_t cur, uint32_t fe, uint32_t sp) { return pack(cur, fe, sp); }

typedef struct {
    uint32_t csr, kind, arg_priv, arg_en, exp_csr;
    int exp_trap;
} Case;

static int run_group(const char *name, const Case *cs, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        uint32_t got_csr; int got_trap;
        step(cs[i].csr, cs[i].kind, cs[i].arg_priv, cs[i].arg_en, &got_csr, &got_trap);
        if (got_csr != cs[i].exp_csr || got_trap != cs[i].exp_trap) {
            printf("%s_FAIL case#%d csr=0x%02X kind=%u ap=%u ae=%u | exp(csr=0x%02X,trap=%d) got(csr=0x%02X,trap=%d)\n",
                   name, i, cs[i].csr, cs[i].kind, cs[i].arg_priv, cs[i].arg_en,
                   cs[i].exp_csr, cs[i].exp_trap, got_csr, got_trap);
            ok = 0;
        }
    }
    if (ok) printf("%s_PASS\n", name);
    return ok;
}

int main(void) {
    int all = 1;

    Case cmp[] = {
        { mkcsr(A,0,0), NORMAL, A, 0, mkcsr(A,0,0), 0 },
        { mkcsr(C,0,0), NORMAL, A, 0, mkcsr(C,0,0), 1 },
        { mkcsr(B,0,0), NORMAL, B, 0, mkcsr(B,0,0), 0 },
        { mkcsr(B,0,0), NORMAL, A, 0, mkcsr(B,0,0), 1 },
        { mkcsr(C,0,0), NORMAL, C, 0, mkcsr(C,0,0), 0 },
        { mkcsr(A,0,0), NORMAL, C, 0, mkcsr(A,0,0), 0 },
    };
    all &= run_group("CMP", cmp, 6);

    Case drop[] = {
        { mkcsr(A,0,0), DROP, B, 0, mkcsr(B,0,0), 0 },
        { mkcsr(A,0,0), DROP, C, 0, mkcsr(C,0,0), 0 },
        { mkcsr(B,0,0), DROP, C, 0, mkcsr(C,0,0), 0 },
        { mkcsr(C,0,0), DROP, A, 0, mkcsr(C,0,0), 1 },
        { mkcsr(B,0,0), DROP, A, 0, mkcsr(B,0,0), 1 },
        { mkcsr(B,0,0), DROP, B, 0, mkcsr(B,0,0), 0 },
    };
    all &= run_group("DROP", drop, 6);

    Case trap[] = {
        { mkcsr(C,0,0), ECALL, 0, 0, mkcsr(A,0,C), 0 },
        { mkcsr(B,0,0), ECALL, 0, 0, mkcsr(A,0,B), 0 },
        { mkcsr(A,0,B), XRET,  0, 0, mkcsr(B,0,B), 0 },
        { mkcsr(C,0,0), XRET,  0, 0, mkcsr(C,0,0), 1 },
        { mkcsr(B,0,0), XRET,  0, 0, mkcsr(B,0,0), 1 },
        { mkcsr(C,1,0), ECALL, 0, 0, mkcsr(A,1,C), 0 },
    };
    all &= run_group("TRAP", trap, 6);

    Case feat[] = {
        { mkcsr(A,0,0), SETFEAT, 0, 1, mkcsr(A,1,0), 0 },
        { mkcsr(B,0,0), SETFEAT, 0, 1, mkcsr(B,1,0), 0 },
        { mkcsr(C,0,0), SETFEAT, 0, 1, mkcsr(C,0,0), 1 },
        { mkcsr(A,1,0), SETFEAT, 0, 0, mkcsr(A,0,0), 0 },
        { mkcsr(C,1,0), USEFEAT, C, 0, mkcsr(C,1,0), 0 },
        { mkcsr(C,0,0), USEFEAT, C, 0, mkcsr(C,0,0), 1 },
        { mkcsr(A,1,0), USEFEAT, B, 0, mkcsr(A,1,0), 0 },
        { mkcsr(C,1,0), USEFEAT, B, 0, mkcsr(C,1,0), 1 },
        { mkcsr(B,1,0), USEFEAT, B, 0, mkcsr(B,1,0), 0 },
    };
    all &= run_group("FEAT", feat, 9);

    /* 子实验 5：三态贯通小程序，csr 串起来逐步校验。 */
    {
        /* (kind, arg_priv, arg_en, exp_csr, exp_trap) */
        Case traj[] = {
            { 0, DROP,    B, 0, mkcsr(B,0,0), 0 },
            { 0, SETFEAT, 0, 1, mkcsr(B,1,0), 0 },
            { 0, DROP,    C, 0, mkcsr(C,1,0), 0 },
            { 0, USEFEAT, B, 0, mkcsr(C,1,0), 1 },
            { 0, ECALL,   0, 0, mkcsr(A,1,C), 0 },
            { 0, XRET,    0, 0, mkcsr(C,1,0), 0 },
        };
        uint32_t csr = mkcsr(A,0,0); /* A 启动 */
        int ok = 1;
        for (int i = 0; i < 6; i++) {
            uint32_t ncsr; int trap;
            step(csr, traj[i].kind, traj[i].arg_priv, traj[i].arg_en, &ncsr, &trap);
            if (ncsr != traj[i].exp_csr || trap != traj[i].exp_trap) {
                printf("CAPSTONE_FAIL step#%d csr=0x%02X kind=%u | exp(csr=0x%02X,trap=%d) got(csr=0x%02X,trap=%d)\n",
                       i, csr, traj[i].kind, traj[i].exp_csr, traj[i].exp_trap, ncsr, trap);
                ok = 0;
            }
            csr = ncsr;
        }
        if (ok) printf("CAPSTONE_PASS\n");
        all &= ok;
    }

    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
