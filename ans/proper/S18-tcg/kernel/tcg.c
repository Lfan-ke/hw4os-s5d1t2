/* S18 · mini-TCG: the decoder + host code generator (reference solution).
 *
 * This is the part a "binary translator" actually owns:
 *   1) tcg_decode    : crack a 32-bit guest RISC-V word into fields (the front
 *                      end / decoder).
 *   2) host micro-ops: the host-side primitives the guest ops compile down to.
 *   3) tcg_translate : pick the host micro-op for a decoded guest insn
 *                      (the "code generator").
 *   4) tcg_run       : run a translated block against a guest register file.
 */
#include "tcg.h"

/* ============================ 1. decoder =============================== */
/* RV instruction field layout (little fields, bit 0 = LSB):
 *   opcode = raw[6:0]   rd = raw[11:7]   funct3 = raw[14:12]
 *   rs1    = raw[19:15] rs2 = raw[24:20] funct7 = raw[31:25]
 * I-immediate = sign-extend(raw[31:20]).
 */
struct Insn tcg_decode(uint32_t raw) {
    struct Insn in = { OP_UNKNOWN, 0, 0, 0, 0 };

    uint32_t opcode = raw & 0x7f;
    uint32_t funct3 = (raw >> 12) & 0x7;
    uint32_t funct7 = (raw >> 25) & 0x7f;
    in.rd  = (raw >> 7)  & 0x1f;
    in.rs1 = (raw >> 15) & 0x1f;
    in.rs2 = (raw >> 20) & 0x1f;
    /* arithmetic right shift of a signed value sign-extends the I-immediate */
    in.imm = (long)((int32_t)raw >> 20);

    if (opcode == 0x13 && funct3 == 0x0) {
        in.op = OP_ADDI;                 /* addi / li */
    } else if (opcode == 0x33 && funct3 == 0x0 && funct7 == 0x00) {
        in.op = OP_ADD;
    } else if (opcode == 0x33 && funct3 == 0x0 && funct7 == 0x20) {
        in.op = OP_SUB;
    } else if (opcode == 0x33 && funct3 == 0x7 && funct7 == 0x00) {
        in.op = OP_AND;
    } else if (opcode == 0x33 && funct3 == 0x6 && funct7 == 0x00) {
        in.op = OP_OR;
    }
    return in;
}

/* ======================= 2. host micro-ops ============================ */
/* Each is the "translated host code" for one guest opcode. x0 stays wired to
 * zero: writes to rd==0 are dropped (RISC-V semantics). */
static void h_addi(uint64_t *r, const struct HostOp *o) {
    if (o->rd) r[o->rd] = r[o->rs1] + (uint64_t)o->imm;
}
static void h_add(uint64_t *r, const struct HostOp *o) {
    if (o->rd) r[o->rd] = r[o->rs1] + r[o->rs2];
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

/* ======================= 3. code generator =========================== */
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

/* ========================= 4. run the block ========================== */
void tcg_run(const struct HostOp *prog, int n, uint64_t *regs) {
    for (int i = 0; i < n; i++)
        if (prog[i].fn) prog[i].fn(regs, &prog[i]);
    regs[0] = 0;                              /* x0 is always 0 */
}
