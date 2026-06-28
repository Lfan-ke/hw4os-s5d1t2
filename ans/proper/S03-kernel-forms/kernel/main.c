/* S03 · kernel-forms harness (given, do not change).
 *
 * One S-mode kernel image demonstrates THREE classic kernel "forms":
 *   1) unikernel  : the "application" is just a kernel function, reached by a
 *                   direct call -- no syscall / no trap.            -> UNIKERNEL_PASS
 *   2) RTOS       : a static set of tasks share the CPU by cooperative yield
 *                   (round-robin scheduler, see rtos.c).            -> RTOS_PASS
 *   3) exokernel  : the kernel only does SECURE MULTIPLEXING (hands out resource
 *                   handles after a safety check); the abstraction lives in a
 *                   libOS function.                                 -> EXOKERNEL_PASS
 * All three pass -> ALL_PASS.
 */
#include "kernel.h"

/* ---- form 1: unikernel (library OS) --------------------------------- */
/* This is the "application". In a unikernel the app is linked into the same
 * image and address space as the kernel, so it reaches a kernel service by an
 * ordinary function call -- there is no privilege boundary, no syscall ABI. */
static int app_add(int a, int b) { return a + b; }

static int unikernel_demo(void) {
    kputs("[unikernel] app linked into kernel image; calls kernel fn directly\n");
    int r = app_add(20, 22);                 /* pure application logic */
    kputs("[unikernel] app uses kputdec() (a kernel routine) as its 'write': ");
    kputdec((uint64_t)r);
    console_putchar('\n');
    if (r == 42) { kputs("UNIKERNEL_PASS\n"); return 1; }
    return 0;
}

/* ---- form 3: exokernel ---------------------------------------------- */
/* The exokernel core owns the raw resource and does ONLY secure multiplexing:
 * it grants a disjoint slice (an opaque handle = offset) after a bounds check.
 * It imposes no abstraction. The abstraction (here: a string writer) lives in
 * a libOS function that runs on top of the granted region. */
#define EXO_POOL 64
static char exo_pool[EXO_POOL];
static int  exo_watermark;

/* kernel primitive: bounds-checked grant of a region -> handle, or -1 */
static int exo_grant(int len) {
    if (len <= 0 || exo_watermark + len > EXO_POOL) return -1; /* safety only */
    int h = exo_watermark;
    exo_watermark += len;                    /* watermark keeps grants disjoint */
    return h;
}

/* libOS: builds an abstraction (a string writer) over the raw granted region. */
static int libos_write_str(int handle, const char *s) {
    char *region = &exo_pool[handle];
    int n = 0;
    while (s[n]) { region[n] = s[n]; n++; }
    return n;
}

static int exokernel_demo(void) {
    kputs("[exokernel] kernel grants handles; libOS implements the abstraction\n");
    int hA = exo_grant(8);
    int hB = exo_grant(8);
    libos_write_str(hA, "libA");
    libos_write_str(hB, "libB");
    int disjoint = (hA >= 0 && hB >= hA + 8);            /* safe multiplexing */
    int intact   = (exo_pool[hA] == 'l' && exo_pool[hA + 3] == 'A' &&
                    exo_pool[hB] == 'l' && exo_pool[hB + 3] == 'B');
    int refused  = (exo_grant(1000) == -1);             /* safety check refuses */
    kputs("[exokernel] grants disjoint, regions intact, over-grant refused\n");
    if (disjoint && intact && refused) { kputs("EXOKERNEL_PASS\n"); return 1; }
    return 0;
}

/* ---- form 2: RTOS lives in rtos.c ----------------------------------- */
int rtos_demo(void);

void kmain(void) {
    kputs("\n[S03] kernel forms: unikernel / RTOS / exokernel (one S-mode image)\n");
    int ok = 1;
    ok &= unikernel_demo();
    ok &= rtos_demo();
    ok &= exokernel_demo();
    if (ok) kputs("ALL_PASS\n");
}
