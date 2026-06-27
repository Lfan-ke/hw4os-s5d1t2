/* S18 · mini-TCG: shared types for the guest decoder / host translator.
 *
 * A "TCG" (Tiny Code Generator, the heart of QEMU's emulation mode) works in
 * three stages, which this header models:
 *   guest insn word  --decode-->  IR (struct Insn)  --translate-->  host op
 *                                                                 (struct HostOp)
 * then a dispatch loop *executes* the translated host ops against a guest
 * register file. We "generate" host C micro-ops instead of real machine code,
 * but the pipeline (decode -> IR -> host code -> run) is the same idea.
 */
#ifndef S18_TCG_H
#define S18_TCG_H
#include <stdint.h>

/* Decoded guest opcode (a tiny RV64I subset). */
enum Op {
    OP_ADDI = 0,   /* I-type: rd = rs1 + imm   (li rd,imm == addi rd,x0,imm) */
    OP_ADD,        /* R-type: rd = rs1 + rs2 */
    OP_SUB,        /* R-type: rd = rs1 - rs2 */
    OP_AND,        /* R-type: rd = rs1 & rs2 */
    OP_OR,         /* R-type: rd = rs1 | rs2 */
    OP_UNKNOWN
};

/* Decoded intermediate representation of one guest instruction. */
struct Insn {
    enum Op op;
    int  rd, rs1, rs2;
    long imm;       /* sign-extended I-immediate (R-type leaves it 0) */
};

/* A translated host micro-op: a host function plus its already-decoded
 * operands. Running prog[0..n) is "executing the translated block". */
struct HostOp;
typedef void (*host_fn)(uint64_t *regs, const struct HostOp *op);
struct HostOp {
    host_fn fn;
    int  rd, rs1, rs2;
    long imm;
};

/* —— student-implemented (tcg.c) —— */
struct Insn   tcg_decode(uint32_t raw);                 /* guest word -> IR    */
struct HostOp tcg_translate(struct Insn in);            /* IR -> host micro-op */
void          tcg_run(const struct HostOp *prog, int n, uint64_t *regs);

#endif
