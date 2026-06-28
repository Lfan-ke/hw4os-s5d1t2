/* 动态链接加载器（软件建模）—— C。
 * 母题：静态链接 = 每个程序各塞一份库的拷贝（费空间、改 bug 逐个重编）；
 *   动态链接 = 程序只存「需要谁」(名字)+一张空 GOT，库只一份共享，
 *   ld.so 按【符号名 key】到唯一共享库查表、把「基址+偏移」回填进各自私有 GOT。
 * 逐题意境→现实：E1 静态各塞一份 → E2 动态名字引用+空GOT → E3 ld.so 按 key 解析回填
 *   → E4 一份共享+私有GOT 空间对比 → E5 lazy vs now → E6 dlopen + 缺符号失败。
 * 学生只填 // TODO；下方 harness 勿改。
 */
#include <stdio.h>
#include <string.h>

#define LIB_CODE 1000UL /* 一份库代码大小 */
#define OWN_CODE 200UL   /* 程序自身代码大小 */
#define GOT_SLOT 8UL     /* 一个 GOT 槽=一个指针=8 字节 */
#define MAXSYM 8
#define MAXGOT 8
#define MAXLIB 8

typedef struct { char name[16]; unsigned long off; } Sym;
/* 共享库：按符号名存库内偏移。加载到 base 后，符号真实地址 = base+off。 */
typedef struct { char name[16]; unsigned long base; Sym syms[MAXSYM]; int nsym; } Library;
/* GOT 槽：resolved==0 表示还没回填（库基址 0x4000_0000，真实地址非 0，故 0 可作哨兵）。 */
typedef struct { char sym[16]; unsigned long resolved; } GotSlot;
typedef struct { unsigned long own; Library embedded; } StaticProgram;       /* 各塞一份 */
typedef struct { unsigned long own; char needed[16]; GotSlot got[MAXGOT]; int ngot; } DynProgram;
typedef struct { Library libs[MAXLIB]; int n; } Registry;                     /* 已加载的 .so */

/* ═════════════════════ 学生填空区（六段，意境→现实）═════════════════════ */

/* E3①：按【符号名 key】查表 → 命中返回真实地址 base+off，未命中返回 0。 */
static unsigned long lookup(const Library *lib, const char *sym) {
    /* TODO: 遍历 lib->syms[0..nsym)，strcmp 命中返回 lib->base + off，否则 0。 */
    (void)lib; (void)sym; /* 占位：永远找不到 → 后续解析全失败 */
    return 0;
}

/* E1：n 个静态程序的总空间——各塞一份库。 */
static unsigned long static_total_space(unsigned long n) {
    /* TODO: 每个 = OWN_CODE + 一整份 LIB_CODE；n 个即 n*(OWN_CODE+LIB_CODE)。 */
    (void)n; /* 占位 */
    return 0;
}

/* E1：造一个静态程序——把库整份拷进去（自包含）。 */
static StaticProgram make_static(const Library *lib) {
    StaticProgram p;
    /* TODO: p.own=OWN_CODE; p.embedded = *lib（整份拷贝）。 */
    p.own = 0; /* 占位 */
    p.embedded = *lib;
    return p;
}

/* E2：造一个动态程序——只登记 needed + 给每个外部符号建一个【空】GOT 槽。 */
static DynProgram make_dyn(const char *needed, const char *const *syms, int nsym) {
    DynProgram p;
    /* TODO: p.own=OWN_CODE; strcpy needed; 对每个 sym 建 got[i]={sym, resolved=0}; p.ngot=nsym。 */
    (void)needed; (void)syms; (void)nsym; /* 占位 */
    p.own = 0;
    p.needed[0] = 0;
    p.ngot = 0;
    return p;
}

/* E3②：ld.so 重定位——每个 GOT 空槽拿名字去共享库按 key 查、回填真实地址。 */
static void resolve(DynProgram *prog, const Library *lib) {
    /* TODO: 对 prog->got[0..ngot)：resolved = lookup(lib, sym)。 */
    (void)prog; (void)lib; /* 占位：什么都不回填 */
}

/* E4：n 个动态程序【共享一份】库 + 各自一张小 GOT；返回总空间。 */
static unsigned long dyn_total_space(unsigned long n, unsigned long n_syms) {
    /* TODO: LIB_CODE + n*(OWN_CODE + n_syms*GOT_SLOT)。 */
    (void)n; (void)n_syms; /* 占位 */
    return 0;
}

/* E5：NOW 绑定——加载时一次性解析所有 GOT 槽，返回解析次数。 */
static int now_bind(DynProgram *prog, const Library *lib) {
    /* TODO: 解析全部槽，返回槽数。 */
    (void)prog; (void)lib; /* 占位 */
    return 0;
}

/* E5：LAZY 绑定——只解析 used 里被调用的符号，返回解析次数。 */
static int lazy_bind(DynProgram *prog, const Library *lib, const char *const *used, int nused) {
    /* TODO: 对 used 每个符号，找到其槽；若 resolved==0 → lookup 回填、计数+1。 */
    (void)prog; (void)lib; (void)used; (void)nused; /* 占位 */
    return 0;
}

/* E6：dlopen——运行时把一份库装进注册表。 */
static void dlopen_lib(Registry *reg, Library lib) {
    /* TODO: reg->libs[reg->n++] = lib。 */
    (void)reg; (void)lib; /* 占位：什么都不装 */
}

/* E6：dlsym——按 库名+符号名 查真实地址；缺库或缺符号 → *err=1 返回 0（不崩）。 */
static unsigned long dlsym_addr(const Registry *reg, const char *libname, const char *sym, int *err) {
    /* TODO: 找 reg 里 name==libname 的库；再 lookup(sym)；任一缺失 *err=1 返回 0。 */
    (void)reg; (void)libname; (void)sym; /* 占位 */
    *err = 1;
    return 0;
}

/* ═════════════════════════════ harness（勿改）═════════════════════════════ */

static Library make_libc(void) {
    Library l;
    strcpy(l.name, "libc.so");
    l.base = 0x40000000UL;
    strcpy(l.syms[0].name, "puts");   l.syms[0].off = 0x100;
    strcpy(l.syms[1].name, "printf"); l.syms[1].off = 0x240;
    strcpy(l.syms[2].name, "malloc"); l.syms[2].off = 0x380;
    l.nsym = 3;
    return l;
}

static int check_static(void) {
    Library libc = make_libc();
    unsigned long n = 3, i;
    int ok = 1;
    for (i = 0; i < n; i++) {
        StaticProgram p = make_static(&libc);
        if (lookup(&p.embedded, "puts") != 0x40000100UL) {
            printf("STATIC_FAIL 静态程序的内嵌库查不到 puts（应自包含）\n"); ok = 0;
        }
    }
    unsigned long space = static_total_space(n), want = n * (OWN_CODE + LIB_CODE);
    if (space != want) { printf("STATIC_FAIL 总空间=%lu 应=%lu\n", space, want); ok = 0; }
    if (ok) { printf("STATIC: n=%lu 各塞一份库 → 总空间=%lu 字节\n", n, space); printf("STATIC_PASS\n"); }
    return ok;
}

static int check_dynsym(void) {
    const char *syms[] = {"puts", "printf"};
    DynProgram p = make_dyn("libc.so", syms, 2);
    int ok = 1;
    if (strcmp(p.needed, "libc.so") != 0) { printf("DYNSYM_FAIL needed=%s 应=libc.so\n", p.needed); ok = 0; }
    if (p.ngot != 2) { printf("DYNSYM_FAIL GOT 槽数=%d 应=2\n", p.ngot); ok = 0; }
    for (int i = 0; i < p.ngot; i++)
        if (p.got[i].resolved != 0) { printf("DYNSYM_FAIL 刚建的 GOT 槽必须全未解析(0)\n"); ok = 0; }
    if (p.ngot >= 1 && strcmp(p.got[0].sym, "puts") != 0) { printf("DYNSYM_FAIL GOT[0] 应=puts\n"); ok = 0; }
    if (ok) { printf("DYNSYM: 动态程序只存 needed=libc.so + 2 个空 GOT 槽（未塞库）\n"); printf("DYNSYM_PASS\n"); }
    return ok;
}

static int check_resolve(void) {
    Library libc = make_libc();
    const char *syms[] = {"puts", "printf", "malloc"};
    DynProgram p = make_dyn("libc.so", syms, 3);
    resolve(&p, &libc);
    struct { const char *s; unsigned long a; } want[] = {
        {"puts", 0x40000100UL}, {"printf", 0x40000240UL}, {"malloc", 0x40000380UL}};
    int ok = 1;
    for (int w = 0; w < 3; w++) {
        unsigned long got = 0; int found = 0;
        for (int i = 0; i < p.ngot; i++)
            if (strcmp(p.got[i].sym, want[w].s) == 0) { got = p.got[i].resolved; found = 1; }
        if (!found || got != want[w].a) {
            printf("RESOLVE_FAIL %s 解析=%#lx 应=%#lx\n", want[w].s, got, want[w].a); ok = 0;
        }
    }
    if (ok) { printf("RESOLVE: ld.so 按 key 把 puts/printf/malloc 回填进私有 GOT（base+偏移）\n"); printf("RESOLVE_PASS\n"); }
    return ok;
}

static int check_share(void) {
    unsigned long n = 100, n_syms = 3;
    unsigned long stat = static_total_space(n), dyn = dyn_total_space(n, n_syms);
    unsigned long want_dyn = LIB_CODE + n * (OWN_CODE + n_syms * GOT_SLOT);
    int ok = 1;
    if (dyn != want_dyn) { printf("SHARE_FAIL 动态总空间=%lu 应=%lu\n", dyn, want_dyn); ok = 0; }
    if (!(dyn < stat)) { printf("SHARE_FAIL 动态(%lu) 应远小于 静态(%lu)\n", dyn, stat); ok = 0; }
    if (ok) {
        printf("SHARE: n=%lu → 静态=%luB（各塞一份） vs 动态=%luB（一份共享+各自GOT），省 %luB\n",
               n, stat, dyn, stat - dyn);
        printf("SHARE_PASS\n");
    }
    return ok;
}

static int check_bind(void) {
    Library libc = make_libc();
    const char *syms[] = {"puts", "printf", "malloc"};
    DynProgram pn = make_dyn("libc.so", syms, 3);
    DynProgram pl = make_dyn("libc.so", syms, 3);
    int now_cnt = now_bind(&pn, &libc);
    const char *used[] = {"puts"};
    int lazy_cnt = lazy_bind(&pl, &libc, used, 1);
    int ok = 1;
    if (now_cnt != 3) { printf("BIND_FAIL now 解析次数=%d 应=3\n", now_cnt); ok = 0; }
    if (lazy_cnt != 1) { printf("BIND_FAIL lazy 解析次数=%d 应=1\n", lazy_cnt); ok = 0; }
    for (int i = 0; i < pl.ngot; i++)
        if (strcmp(pl.got[i].sym, "puts") != 0 && pl.got[i].resolved != 0) {
            printf("BIND_FAIL lazy 下未调用的符号不应被解析\n"); ok = 0;
        }
    if (ok) { printf("BIND: now 解析 %d 个；lazy 只解析 %d 个（按需，省解析开销）\n", now_cnt, lazy_cnt); printf("BIND_PASS\n"); }
    return ok;
}

static int check_dlopen(void) {
    Registry reg; reg.n = 0;
    dlopen_lib(&reg, make_libc());
    Library plug;
    strcpy(plug.name, "libplugin.so"); plug.base = 0x50000000UL;
    strcpy(plug.syms[0].name, "greet"); plug.syms[0].off = 0x10; plug.nsym = 1;
    dlopen_lib(&reg, plug);
    int ok = 1, err = 0;
    unsigned long a = dlsym_addr(&reg, "libplugin.so", "greet", &err);
    if (err || a != 0x50000010UL) { printf("DLOPEN_FAIL dlsym(greet)=%#lx err=%d 应=0x50000010\n", a, err); ok = 0; }
    err = 0; dlsym_addr(&reg, "libplugin.so", "nonexist", &err);
    if (!err) { printf("DLOPEN_FAIL dlsym(缺符号) 应 err=1\n"); ok = 0; }
    err = 0; dlsym_addr(&reg, "libmissing.so", "x", &err);
    if (!err) { printf("DLOPEN_FAIL dlsym(缺库) 应 err=1\n"); ok = 0; }
    if (ok) { printf("DLOPEN: 运行时装入 libplugin，dlsym(greet)=Ok；缺符号/缺库=Err（不崩）\n"); printf("DLOPEN_PASS\n"); }
    return ok;
}

int main(void) {
    int all = 1;
    all &= check_static();
    all &= check_dynsym();
    all &= check_resolve();
    all &= check_share();
    all &= check_bind();
    all &= check_dlopen();
    if (all) { printf("ALL_PASS\n"); return 0; }
    return 1;
}
