/* S18 · mini-TCG: the decoder + host code generator (student version).
 *
 * Fill the TODOs:
 *   1) tcg_decode    : crack a 32-bit guest RISC-V word into fields, set op.
 *   2) two host micro-ops (h_addi, h_add): the "translated host code".
 * The translate pass (tcg_translate) and the run loop (tcg_run) are given.
 *
 * Placeholders compile and run, but the program will NOT reach ALL_PASS until
 * you implement them.
 */
#include "tcg.h"

/* ============================ 1. decoder =============================== */
/* RV instruction field layout (bit 0 = LSB):
 *   opcode = raw[6:0]   rd = raw[11:7]   funct3 = raw[14:12]
 *   rs1    = raw[19:15] rs2 = raw[24:20] funct7 = raw[31:25]
 * I-immediate = sign-extend(raw[31:20])  ==  (int32_t)raw >> 20  (arith shift).
 *
 * Opcode table:
 *   opcode=0x13, funct3=0           -> OP_ADDI   (addi / li)
 *   opcode=0x33, funct3=0, funct7=0x00 -> OP_ADD
 *   opcode=0x33, funct3=0, funct7=0x20 -> OP_SUB
 *   opcode=0x33, funct3=7, funct7=0x00 -> OP_AND
 *   opcode=0x33, funct3=6, funct7=0x00 -> OP_OR
 */
struct Insn tcg_decode(uint32_t raw) {
    struct Insn in = { OP_UNKNOWN, 0, 0, 0, 0 };
    /* TODO: extract opcode/funct3/funct7/rd/rs1/rs2/imm from raw and set in.op.
     *       Leave in.op = OP_UNKNOWN for anything not in the table above.
     * (placeholder below returns a fully-unknown insn so the build runs.) */
    (void)raw;
    return in;
}

/* ======================= 2. host micro-ops ============================ */
/* Each is the "translated host code" for one guest opcode. x0 is wired to
 * zero: writes to rd==0 must be dropped. */
static void h_addi(uint64_t *r, const struct HostOp *o) {
    /* TODO: r[rd] = r[rs1] + (uint64_t)imm   (skip when rd==0) */
    (void)r; (void)o;
}
static void h_add(uint64_t *r, const struct HostOp *o) {
    /* TODO: r[rd] = r[rs1] + r[rs2]          (skip when rd==0) */
    (void)r; (void)o;
}
static void h_sub(uint64_t *r, const struct HostOp *o) {
    if (o->rd) r[o->rd] = r[o->rs1] - r[o->rs2];
}
static void h_and(uint64_t *r, const struct HostOp *o) {
    if (o->rd) r[o->rd] = r[o->rs1] & r[o->rs2];
}
static void h_or(uint64_t *r, const struct HostOp *o) {
    if (o->rd) r[o->rd] = r[o->rs1] | r[o->rs2];
}

/* ======================= 3. code generator (given) =================== */
struct HostOp tcg_translate(struct Insn in) {
    struct HostOp h = { 0, in.rd, in.rs1, in.rs2, in.imm };
    switch (in.op) {
        case OP_ADDI: h.fn = h_addi; break;
        case OP_ADD:  h.fn = h_add;  break;
        case OP_SUB:  h.fn = h_sub;  break;
        case OP_AND:  h.fn = h_and;  break;
        case OP_OR:   h.fn = h_or;   break;
        default:      h.fn = 0;      break;   /* untranslatable */
    }
    return h;
}

/* ========================= 4. run the block (given) ================== */
void tcg_run(const struct HostOp *prog, int n, uint64_t *regs) {
    for (int i = 0; i < n; i++)
        if (prog[i].fn) prog[i].fn(regs, &prog[i]);
    regs[0] = 0;                              /* x0 is always 0 */
}
