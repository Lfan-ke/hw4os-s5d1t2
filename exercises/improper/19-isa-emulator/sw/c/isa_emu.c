/* ISA 模拟器：CPU 不过是一个「取指 → 译码 → 执行」的循环 —— C。
 *
 * 母题：一颗 CPU 的本质，就是反复地
 *   1) 取指 fetch：从 pc 处读 4 字节机器码；
 *   2) 译码 decode：拆出 opcode/funct3/funct7、rd/rs1/rs2、立即数(符号扩展)；
 *   3) 执行 execute：更新寄存器 / 内存 / pc。
 * NEMU、QEMU、Spike 这些模拟器，就是用软件跑这个循环。
 *
 * 本课实现一颗软件 RV64 CPU：u64 regs[32](x0 恒 0) + pc + 一小块字节内存，
 * 支持若干 RV64I：addi/add/sub/lui/ld/sd/beq/bne/jal/jalr。
 * 内嵌小程序求 1+2+...+10 = 55，再调子程序翻倍成 110。
 *
 * 判据：DECODE_PASS(译码字段对) / EXEC_PASS(跑出结果对) /
 *       DIFFTEST_PASS(与黄金参考逐指令对拍一致)。全过 ALL_PASS。
 *
 * 你只需填两处 TODO：① add/sub/beq/bne 的译码语义与执行；② difftest 逐步对拍。
 * 其余(取指框架、内存、黄金参考、机器码、harness)均已给定，勿改。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── 内嵌机器码小程序（RV64I，小端）。地址 = 下标*4。
 *   0x00 addi x5,x0,0   0x04 addi x6,x0,1   0x08 addi x7,x0,11  0x0c lui x8,1
 *   0x10 beq x6,x7,+16  0x14 add x5,x5,x6   0x18 addi x6,x6,1   0x1c jal x0,-12
 *   0x20 sd x5,0(x8)    0x24 ld x9,0(x8)    0x28 sub x12,x7,x6  0x2c jal x1,+16
 *   0x30 bne x13,x0,+8  0x34 addi x14,x0,777 0x38 HALT          0x3c add x13,x5,x5
 *   0x40 jalr x0,0(x1)
 */
static const uint32_t PROG[] = {
    0x00000293, 0x00100313, 0x00b00393, 0x00001437, 0x00730863, 0x006282b3, 0x00130313, 0xff5ff06f,
    0x00543023, 0x00043483, 0x40638633, 0x010000ef, 0x00069463, 0x30900713, 0x00000000, 0x005286b3,
    0x00008067,
};
#define PROG_N ((int)(sizeof(PROG) / sizeof(PROG[0])))

#define MEM_SIZE 0x2000
#define STEP_CAP 10000 /* 防跑飞：模型不收敛也不死循环 */

/* 差分对拍跟踪的寄存器；快照 col0=pc，col1..6 = x5,x6,x7,x9,x12,x13 */
static const int TRACK[6] = {5, 6, 7, 9, 12, 13};
#define SNAP 7

/* ── 译码结果：一条指令拆出的全部字段（取指框架给定） ── */
typedef struct {
    uint32_t opcode, funct3, funct7;
    int rd, rs1, rs2;
    int64_t imm_i, imm_s, imm_b, imm_u, imm_j;
} Decoded;

/* 符号扩展：把 v 的低 n 位当有符号数扩到 int64。 */
static int64_t sext(uint32_t v, int n) {
    int shift = 32 - n;
    return (int64_t)((int32_t)(v << shift) >> shift);
}

/* 译码：把 32-bit 机器码拆成字段 + 各型立即数（给定，勿改）。 */
static Decoded decode(uint32_t inst) {
    Decoded d;
    d.opcode = inst & 0x7f;
    d.rd = (inst >> 7) & 0x1f;
    d.funct3 = (inst >> 12) & 0x7;
    d.rs1 = (inst >> 15) & 0x1f;
    d.rs2 = (inst >> 20) & 0x1f;
    d.funct7 = (inst >> 25) & 0x7f;
    d.imm_i = sext((inst >> 20) & 0xFFF, 12);
    d.imm_s = sext((((inst >> 25) & 0x7f) << 5) | ((inst >> 7) & 0x1f), 12);
    d.imm_b = sext((((inst >> 31) & 1) << 12) | (((inst >> 7) & 1) << 11) |
                       (((inst >> 25) & 0x3f) << 5) | (((inst >> 8) & 0xf) << 1),
                   13);
    d.imm_u = sext((inst >> 12) & 0xFFFFF, 20) << 12;
    d.imm_j = sext((((inst >> 31) & 1) << 20) | (((inst >> 12) & 0xff) << 12) |
                       (((inst >> 20) & 1) << 11) | (((inst >> 21) & 0x3ff) << 1),
                   21);
    return d;
}

/* ── 软件 RV64 CPU ── */
typedef struct {
    uint64_t regs[32];
    uint64_t pc;
    uint8_t mem[MEM_SIZE];
} Cpu;

static void cpu_init(Cpu *c) {
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < PROG_N; i++) {
        memcpy(&c->mem[i * 4], &PROG[i], 4); /* 小端主机直接拷 */
    }
}

static uint32_t fetch(const Cpu *c) {
    uint32_t w;
    memcpy(&w, &c->mem[c->pc], 4);
    return w;
}
static uint64_t load8(const Cpu *c, uint64_t addr) {
    uint64_t v;
    memcpy(&v, &c->mem[addr], 8);
    return v;
}
static void store8(Cpu *c, uint64_t addr, uint64_t val) {
    memcpy(&c->mem[addr], &val, 8);
}
static void wreg(Cpu *c, int rd, uint64_t val) {
    if (rd != 0)
        c->regs[rd] = val; /* x0 硬连线零 */
}

/* 执行一条：取指→译码→执行。返回是否停机。 */
static int step(Cpu *c) {
    uint32_t inst = fetch(c);
    if (inst == 0)
        return 1; /* 取到全 0 字 → 停机 */
    Decoded d = decode(inst);
    uint64_t npc = c->pc + 4; /* 缺省顺序流 */

    switch (d.opcode) {
    case 0x13: /* OP-IMM: addi（给定） */
        if (d.funct3 == 0)
            wreg(c, d.rd, c->regs[d.rs1] + (uint64_t)d.imm_i);
        break;
    case 0x33: /* OP: R 型 add/sub */
        if (d.funct3 == 0) {
            /* ───────────── TODO ① (R 型) ─────────────
             * 用 funct7 区分：0x00=add（rs1+rs2），0x20=sub（rs1-rs2），写 rd。
             * 提示：wreg(c, d.rd, c->regs[d.rs1] + c->regs[d.rs2]); sub 用减法。
             * 当前占位什么都不写 → 求和 x5 会一直是 0。 */
            /* ────────────────────────────────────────── */
        }
        break;
    case 0x37: /* LUI（给定）：imm_u 已 <<12 */
        wreg(c, d.rd, (uint64_t)d.imm_u);
        break;
    case 0x03: /* LOAD: ld（给定） */
        if (d.funct3 == 3)
            wreg(c, d.rd, load8(c, c->regs[d.rs1] + (uint64_t)d.imm_i));
        break;
    case 0x23: /* STORE: sd（给定） */
        if (d.funct3 == 3)
            store8(c, c->regs[d.rs1] + (uint64_t)d.imm_s, c->regs[d.rs2]);
        break;
    case 0x63: { /* BRANCH: beq/bne */
        /* ───────────── TODO ① (分支) ─────────────
         * funct3==0 是 beq（相等跳），==1 是 bne（不等跳）。
         * 目标 = pc + imm_b（相对 pc 的有符号偏移）；条件成立才把 npc 改成目标。
         * 例：if (d.funct3==0 && c->regs[d.rs1]==c->regs[d.rs2]) npc = c->pc + (uint64_t)d.imm_b;
         * 当前占位：永不跳转 → 循环退不出去（撞 STEP_CAP），结果当然不对。 */
        /* ────────────────────────────────────────── */
        break;
    }
    case 0x6f: /* JAL（给定） */
        wreg(c, d.rd, c->pc + 4);
        npc = c->pc + (uint64_t)d.imm_j;
        break;
    case 0x67: { /* JALR（给定） */
        uint64_t t = c->pc + 4;
        npc = (c->regs[d.rs1] + (uint64_t)d.imm_i) & ~(uint64_t)1;
        wreg(c, d.rd, t);
        break;
    }
    default:
        break; /* 未实现指令：当 nop */
    }

    c->regs[0] = 0; /* 兜底 x0 恒 0 */
    c->pc = npc;
    return 0;
}

static void snapshot(const Cpu *c, uint64_t out[SNAP]) {
    out[0] = c->pc;
    for (int i = 0; i < 6; i++)
        out[i + 1] = c->regs[TRACK[i]];
}

/* ── 黄金参考轨迹（GIVEN）：独立可信参考预算的每步 [pc,x5,x6,x7,x9,x12,x13] ── */
static const uint64_t GOLDEN[][SNAP] = {
    {0x4, 0, 0, 0, 0, 0, 0},     {0x8, 0, 1, 0, 0, 0, 0},      {0xc, 0, 1, 11, 0, 0, 0},
    {0x10, 0, 1, 11, 0, 0, 0},   {0x14, 0, 1, 11, 0, 0, 0},    {0x18, 1, 1, 11, 0, 0, 0},
    {0x1c, 1, 2, 11, 0, 0, 0},   {0x10, 1, 2, 11, 0, 0, 0},    {0x14, 1, 2, 11, 0, 0, 0},
    {0x18, 3, 2, 11, 0, 0, 0},   {0x1c, 3, 3, 11, 0, 0, 0},    {0x10, 3, 3, 11, 0, 0, 0},
    {0x14, 3, 3, 11, 0, 0, 0},   {0x18, 6, 3, 11, 0, 0, 0},    {0x1c, 6, 4, 11, 0, 0, 0},
    {0x10, 6, 4, 11, 0, 0, 0},   {0x14, 6, 4, 11, 0, 0, 0},    {0x18, 10, 4, 11, 0, 0, 0},
    {0x1c, 10, 5, 11, 0, 0, 0},  {0x10, 10, 5, 11, 0, 0, 0},   {0x14, 10, 5, 11, 0, 0, 0},
    {0x18, 15, 5, 11, 0, 0, 0},  {0x1c, 15, 6, 11, 0, 0, 0},   {0x10, 15, 6, 11, 0, 0, 0},
    {0x14, 15, 6, 11, 0, 0, 0},  {0x18, 21, 6, 11, 0, 0, 0},   {0x1c, 21, 7, 11, 0, 0, 0},
    {0x10, 21, 7, 11, 0, 0, 0},  {0x14, 21, 7, 11, 0, 0, 0},   {0x18, 28, 7, 11, 0, 0, 0},
    {0x1c, 28, 8, 11, 0, 0, 0},  {0x10, 28, 8, 11, 0, 0, 0},   {0x14, 28, 8, 11, 0, 0, 0},
    {0x18, 36, 8, 11, 0, 0, 0},  {0x1c, 36, 9, 11, 0, 0, 0},   {0x10, 36, 9, 11, 0, 0, 0},
    {0x14, 36, 9, 11, 0, 0, 0},  {0x18, 45, 9, 11, 0, 0, 0},   {0x1c, 45, 10, 11, 0, 0, 0},
    {0x10, 45, 10, 11, 0, 0, 0}, {0x14, 45, 10, 11, 0, 0, 0},  {0x18, 55, 10, 11, 0, 0, 0},
    {0x1c, 55, 11, 11, 0, 0, 0}, {0x10, 55, 11, 11, 0, 0, 0},  {0x20, 55, 11, 11, 0, 0, 0},
    {0x24, 55, 11, 11, 0, 0, 0}, {0x28, 55, 11, 11, 55, 0, 0}, {0x2c, 55, 11, 11, 55, 0, 0},
    {0x3c, 55, 11, 11, 55, 0, 0}, {0x40, 55, 11, 11, 55, 0, 110}, {0x30, 55, 11, 11, 55, 0, 110},
    {0x38, 55, 11, 11, 55, 0, 110},
};
#define GOLDEN_N ((int)(sizeof(GOLDEN) / sizeof(GOLDEN[0])))

static void col_name(int c, char *buf) {
    if (c == 0)
        snprintf(buf, 8, "pc");
    else
        snprintf(buf, 8, "x%d", TRACK[c - 1]);
}

/* ════════════════════ 测试 harness（给定，勿改） ════════════════════ */

static int check_decode(void) {
    int ok = 1;
    Decoded d;

    d = decode(0x00b00393); /* addi x7,x0,11 */
    if (d.opcode != 0x13 || d.rd != 7 || d.rs1 != 0 || d.funct3 != 0 || d.imm_i != 11) {
        printf("DECODE_BAD addi 字段错: op=0x%x rd=%d rs1=%d imm_i=%lld\n", d.opcode, d.rd, d.rs1,
               (long long)d.imm_i);
        ok = 0;
    }
    d = decode(0x00001437); /* lui x8,1 */
    if (d.opcode != 0x37 || d.rd != 8 || d.imm_u != 0x1000) {
        printf("DECODE_BAD lui 字段错: op=0x%x rd=%d imm_u=0x%llx 应=(0x37,8,0x1000)\n", d.opcode,
               d.rd, (long long)d.imm_u);
        ok = 0;
    }
    d = decode(0x40638633); /* sub x12,x7,x6 */
    if (d.opcode != 0x33 || d.funct7 != 0x20 || d.funct3 != 0 || d.rd != 12 || d.rs1 != 7 ||
        d.rs2 != 6) {
        printf("DECODE_BAD sub 字段错: op=0x%x f7=0x%x rd=%d rs1=%d rs2=%d\n", d.opcode, d.funct7,
               d.rd, d.rs1, d.rs2);
        ok = 0;
    }
    d = decode(0x00730863); /* beq x6,x7,+16 */
    if (d.opcode != 0x63 || d.funct3 != 0 || d.rs1 != 6 || d.rs2 != 7 || d.imm_b != 16) {
        printf("DECODE_BAD beq 字段错: op=0x%x rs1=%d rs2=%d imm_b=%lld 应 imm_b=16\n", d.opcode,
               d.rs1, d.rs2, (long long)d.imm_b);
        ok = 0;
    }
    d = decode(0x010000ef); /* jal x1,+16 */
    if (d.opcode != 0x6f || d.rd != 1 || d.imm_j != 16) {
        printf("DECODE_BAD jal 字段错: op=0x%x rd=%d imm_j=%lld 应=(0x6f,1,16)\n", d.opcode, d.rd,
               (long long)d.imm_j);
        ok = 0;
    }
    d = decode(0xff5ff06f); /* jal x0,-12 */
    if (d.imm_j != -12) {
        printf("DECODE_BAD jal 负偏移错: imm_j=%lld 应=-12（符号扩展没做对？）\n",
               (long long)d.imm_j);
        ok = 0;
    }
    d = decode(0x00543023); /* sd x5,0(x8) */
    if (d.opcode != 0x23 || d.rs1 != 8 || d.rs2 != 5 || d.imm_s != 0) {
        printf("DECODE_BAD sd 字段错: op=0x%x rs1=%d rs2=%d imm_s=%lld\n", d.opcode, d.rs1, d.rs2,
               (long long)d.imm_s);
        ok = 0;
    }

    if (ok)
        printf("DECODE_PASS\n");
    return ok;
}

static int check_exec(void) {
    int ok = 1;
    Cpu c;
    cpu_init(&c);
    int steps = 0, halted = 0;
    for (int i = 0; i < STEP_CAP; i++) {
        if (step(&c)) {
            halted = 1;
            break;
        }
        steps++;
    }

    if (!halted) {
        printf("EXEC_BAD 跑了 %d 步还没停机（撞上限，可能分支没跳/陷入死循环）\n", steps);
        ok = 0;
    }
    int wr[7] = {5, 6, 7, 9, 12, 13, 14};
    uint64_t wv[7] = {55, 11, 11, 55, 0, 110, 0};
    for (int i = 0; i < 7; i++) {
        if (c.regs[wr[i]] != wv[i]) {
            printf("EXEC_BAD x%d = %llu 应 = %llu\n", wr[i], (unsigned long long)c.regs[wr[i]],
                   (unsigned long long)wv[i]);
            ok = 0;
        }
    }
    uint64_t m = load8(&c, 0x1000);
    if (m != 55) {
        printf("EXEC_BAD mem[0x1000] = %llu 应 = 55\n", (unsigned long long)m);
        ok = 0;
    }
    if (c.pc != 0x38) {
        printf("EXEC_BAD 停机 pc = 0x%llx 应 = 0x38\n", (unsigned long long)c.pc);
        ok = 0;
    }
    if (c.regs[0] != 0) {
        printf("EXEC_BAD x0 被写脏 = %llu（x0 必须恒 0）\n", (unsigned long long)c.regs[0]);
        ok = 0;
    }

    if (ok)
        printf("EXEC_PASS 1+2+...+10 = %llu（再翻倍 = %llu）\n", (unsigned long long)c.regs[5],
               (unsigned long long)c.regs[13]);
    return ok;
}

static int check_difftest(void) {
    Cpu c;
    cpu_init(&c);
    uint64_t dut[STEP_CAP][SNAP];
    int n = 0;
    for (int i = 0; i < STEP_CAP; i++) {
        if (step(&c))
            break;
        snapshot(&c, dut[n]);
        n++;
    }

    if (n != GOLDEN_N) {
        printf("DIFF_BAD 轨迹步数 dut=%d ref=%d（执行流就没对上）\n", n, GOLDEN_N);
        return 0;
    }

    /* ───────────────── TODO ② 逐步对拍 ─────────────────
     * 从第 0 步起逐步比较 dut[k] 与 GOLDEN[k] 每一列（col0=pc）。
     * 找到第一处不等就打印
     *   "DIFF_BAD 第 {k+1} 条指令 {列名}: ref={..} dut={..}"  然后 return 0。
     * 全程一致则打印 DIFFTEST_PASS 并 return 1。列名用 col_name(col, nm)。
     *
     *   char nm[8];
     *   for k in 0..GOLDEN_N: for col in 0..SNAP:
     *       if dut[k][col] != GOLDEN[k][col] { col_name(col,nm); printf(...); return 0; }
     *   printf("DIFFTEST_PASS ..."); return 1;
     */
    (void)col_name; /* 占位：实现后用 col_name 打印列名 */
    return 0;       /* 还没实现对拍，先当作不通过 */
    /* ──────────────────────────────────────────────────── */
}

int main(void) {
    int all = 1;
    all &= check_decode();
    all &= check_exec();
    all &= check_difftest();
    if (all) {
        printf("ALL_PASS\n");
        return 0;
    }
    return 1;
}
