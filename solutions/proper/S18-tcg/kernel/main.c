/* S18 · mini-TCG harness (given, do not change).
 *
 * Goal of this stage: tell apart the *virtualization shapes* and then build a
 * dynamic binary translator in miniature.
 *
 *   - A VIRTUAL MACHINE runs a guest of the *same* ISA as the host. The guest
 *     executes natively; only privileged operations trap and are emulated.
 *       * type-1  : bare-metal hypervisor (Xen, ESXi, KVM via RISC-V H-ext) --
 *                   the hypervisor IS the lowest software on the metal.
 *       * type-2  : a hosted VMM that runs as an ordinary application on top of
 *                   a host OS (VirtualBox, VMware Workstation). It is still a
 *                   VIRTUAL MACHINE -- same ISA, hardware-assisted.
 *       * type-1.5: a host kernel module that promotes the host kernel into a
 *                   hypervisor (Linux KVM, bhyve) -- between the two.
 *   - An EMULATOR / SIMULATOR runs a guest of a *different* ISA. No instruction
 *     can run natively, so EVERY guest instruction must be interpreted or
 *     translated. Cross-architecture `qemu-system-*` in TCG mode is an
 *     emulator: its Tiny Code Generator dynamically translates guest machine
 *     code into host machine code, block by block.
 *
 * This lab builds the emulator path in miniature: we take guest RISC-V
 * instruction words, DECODE them, TRANSLATE each into a host micro-op, then
 * EXECUTE the translated block on a guest register file and check the results.
 *   DECODE_PASS  -- the decoder cracks each guest word into the right fields.
 *   EXEC_PASS    -- the translated block computes the right register values.
 *   ALL_PASS     -- both.
 */
#include "kernel.h"
#include "tcg.h"

static const char *opname(enum Op op) {
    switch (op) {
        case OP_ADDI: return "addi";
        case OP_ADD:  return "add";
        case OP_SUB:  return "sub";
        case OP_AND:  return "and";
        case OP_OR:   return "or";
        default:      return "????";
    }
}

/* --------- part A: virtualization taxonomy (narrative, self-checked) ------- */
static void describe_virt(void) {
    kputs("[concept] virtual machine = SAME ISA, guest runs natively, only\n");
    kputs("[concept]   privileged ops trap-and-emulate:\n");
    kputs("[concept]     type-1   bare-metal hypervisor (Xen/ESXi/KVM+H-ext)\n");
    kputs("[concept]     type-2   hosted VMM, an app on a host OS (VirtualBox)\n");
    kputs("[concept]     type-1.5 host-kernel-module hypervisor (Linux KVM)\n");
    kputs("[concept] emulator/simulator = DIFFERENT ISA, every guest insn is\n");
    kputs("[concept]   translated; cross-arch qemu (TCG) is an emulator, and\n");
    kputs("[concept]   THIS lab is a miniature of exactly that translator.\n");
}

/* --------- part B: decode test ------------------------------------------- */
struct DecodeCase {
    uint32_t raw;
    enum Op  op;
    int      rd, rs1, rs2;
    long     imm;
};

/* Hand-assembled RV64I words (verified with binutils `as`). */
static const struct DecodeCase dcases[] = {
    { 0x00500093u, OP_ADDI, 1, 0, 0,  5 },   /* li   x1, 5      */
    { 0x02500113u, OP_ADDI, 2, 0, 0, 37 },   /* li   x2, 37     */
    { 0x002081b3u, OP_ADD,  3, 1, 2,  0 },   /* add  x3, x1, x2 */
    { 0xffe18213u, OP_ADDI, 4, 3, 0, -2 },   /* addi x4, x3, -2 */
    { 0x401182b3u, OP_SUB,  5, 3, 1,  0 },   /* sub  x5, x3, x1 */
    { 0x0021f333u, OP_AND,  6, 3, 2,  0 },   /* and  x6, x3, x2 */
    { 0x0020e3b3u, OP_OR,   7, 1, 2,  0 },   /* or   x7, x1, x2 */
};
#define NDEC ((int)(sizeof(dcases) / sizeof(dcases[0])))

static int decode_test(void) {
    int ok = 1;
    for (int i = 0; i < NDEC; i++) {
        const struct DecodeCase *c = &dcases[i];
        struct Insn in = tcg_decode(c->raw);
        int good = (in.op == c->op) && (in.rd == c->rd) && (in.rs1 == c->rs1);
        /* imm matters for I-type (addi/li); rs2 for R-type. The other field is
         * a don't-care for that format, so we only check the relevant one. */
        if (c->op == OP_ADDI) good = good && (in.imm == c->imm);
        else                  good = good && (in.rs2 == c->rs2);

        kputs("[decode] ");
        kputhex(c->raw);
        kputs(" -> ");
        kputs(opname(in.op));
        kputs(" x");  kputdec((uint64_t)in.rd);
        kputs(",x"); kputdec((uint64_t)in.rs1);
        if (in.op == OP_ADDI) { kputs(",imm="); kputdec((uint64_t)in.imm); }
        else { kputs(",x"); kputdec((uint64_t)in.rs2); }
        kputs(good ? "  [ok]\n" : "  [mismatch]\n");
        ok &= good;
    }
    if (ok) kputs("DECODE_PASS\n");
    else    kputs("decode mismatch: decoder fields wrong\n");
    return ok;
}

/* --------- part C: translate + execute test ------------------------------ */
/* Guest program (same words as above), expected final register file:
 *   x1=5  x2=37  x3=42  x4=40  x5=37  x6=(42&37)=32  x7=(5|37)=37
 */
static const uint32_t guest_prog[] = {
    0x00500093u, 0x02500113u, 0x002081b3u, 0xffe18213u,
    0x401182b3u, 0x0021f333u, 0x0020e3b3u,
};
#define NPROG ((int)(sizeof(guest_prog) / sizeof(guest_prog[0])))

struct ExpReg { int idx; uint64_t val; };
static const struct ExpReg expect[] = {
    { 1, 5 }, { 2, 37 }, { 3, 42 }, { 4, 40 }, { 5, 37 }, { 6, 32 }, { 7, 37 },
};
#define NEXP ((int)(sizeof(expect) / sizeof(expect[0])))

static int exec_test(void) {
    struct HostOp prog[NPROG];
    uint64_t regs[32];
    for (int i = 0; i < 32; i++) regs[i] = 0;

    /* translate guest words -> host micro-ops (the "code generation" pass) */
    for (int i = 0; i < NPROG; i++)
        prog[i] = tcg_translate(tcg_decode(guest_prog[i]));

    /* execute the translated block */
    tcg_run(prog, NPROG, regs);

    int ok = 1;
    for (int i = 0; i < NEXP; i++) {
        uint64_t got = regs[expect[i].idx];
        int good = (got == expect[i].val);
        kputs("[exec] x");
        kputdec((uint64_t)expect[i].idx);
        kputs(" = ");
        kputdec(got);
        kputs(good ? "  [ok]\n" : "  [wrong]\n");
        ok &= good;
    }
    if (ok) kputs("EXEC_PASS\n");
    else    kputs("exec mismatch: translated block computed wrong values\n");
    return ok;
}

void kmain(void) {
    kputs("\n[S18] mini-TCG: decode -> translate -> execute guest RISC-V\n");
    describe_virt();
    int ok = 1;
    ok &= decode_test();
    ok &= exec_test();
    if (ok) kputs("ALL_PASS\n");
}
