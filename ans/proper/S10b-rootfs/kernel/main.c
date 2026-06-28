/* S10b · 内核入口/测试驱动（给定）：内核如何把棒子交给用户态。
 *
 * 主线：mkfs 一张空 RAM 根 → 解开内嵌 initramfs(cpio) 灌进去 →
 *       在 fs 里定位 /init → 把它读进可执行缓冲、跌入 U 态运行 → 回收报告。
 * 四道判据：CPIO_PARSE / POPULATE / INIT_FOUND / USERSPACE，全过打印 ALL_PASS。
 *
 * 复用：fs.*（承 S07 的块设备 + inode/目录 RAM-fs）、uentry.S/syscall.c（承 S08 的 U 态往返）。 */
#include "app.h"
#include "fs.h"
#include "initramfs.h"

/* 内核 callee-saved 保存区（被 uentry.S 使用）。 */
uint64_t kctx[14];
/* 进程退出状态（被 syscall.c 的 sys_exit 写）。 */
volatile long g_exit_code = -1;
volatile long g_proc_done = 0;

/* /init 的可执行落地缓冲 + U 态栈（与内核同地址空间，仅特权级隔离）。 */
static uint8_t  init_exec[4096]   __attribute__((aligned(16)));
static uint64_t user_stack[1024]  __attribute__((aligned(16)));

/* —— 内嵌 initramfs 里几个文本文件的期望内容（用于 POPULATE 校验）—— */
static const char *g_names[3] = { "README", "etc_config", "motd" };
static const char *g_body[3]  = {
    "Welcome to the initramfs root filesystem.\n"
    "This tiny RAM root holds /init and a few config files.\n",
    "hostname=riscv-lab\nmode=batch\n",
    "the quick brown fox jumps over the lazy dog\n",
};

static uint32_t cstrlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static int mem_eq(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}

/* 校验三个文本文件都已被建进 RAM-fs，且内容逐字节读得回来。 */
static int check_populate(void) {
    for (int k = 0; k < 3; k++) {
        int ino = fs_lookup(g_names[k]);
        if (ino < 0) return 0;
        uint8_t buf[BSIZE * 2];
        uint32_t len = cstrlen(g_body[k]);
        int n = fs_read((uint32_t)ino, buf, sizeof(buf));
        if (n != (int)len) return 0;
        if (!mem_eq(buf, g_body[k], len)) return 0;
    }
    return 1;
}

/* =========================================================================
 * 学生填空点 2：在 RAM-fs 里找到 /init，把它读进可执行缓冲，并在 U 态交棒运行。
 *   1) ino = fs_lookup("init")；找不到（ino<0）就直接返回（USERSPACE 不会通过）。
 *   2) n = fs_read(ino, init_exec, sizeof init_exec)；读到 0 字节也直接返回。
 *   3) run_user((uint64_t)init_exec, 用户栈顶)；跌入 U 态执行 /init。
 *      /init（机器码，承 S08 的 ecall 约定）会 sys_write 打印 banner、再 sys_exit(0)，
 *      经 return_to_kernel() longjmp 回到这里 run_user 调用点之后。
 * HINT：用户栈顶 = (uint64_t)((char*)user_stack + sizeof(user_stack))。
 * ========================================================================= */
static void load_and_run_init(void) {
    int ino = fs_lookup("init");
    if (ino < 0) return;
    int n = fs_read((uint32_t)ino, init_exec, sizeof(init_exec));
    if (n <= 0) return;
    run_user((uint64_t)init_exec,
             (uint64_t)((char *)user_stack + sizeof(user_stack)));
}

void kmain(void) {
    kputs("\n[S10b] initramfs: unpack cpio -> RAM-fs -> exec /init in U mode\n");
    trap_init();          /* stvec -> __alltraps（U 态 ecall 也走它）*/
    fs_mkfs();            /* 一张空的 RAM 根文件系统 */

    /* —— 判据 1：解析内嵌 cpio 归档，常规文件数应为 INITRAMFS_NFILES —— */
    int nfiles = cpio_unpack(initramfs_cpio, initramfs_cpio_len);
    kputs("  cpio members unpacked = ");
    kputdec((uint64_t)(uint32_t)nfiles);
    console_putchar('\n');
    if (nfiles == INITRAMFS_NFILES) kputs("CPIO_PARSE_PASS\n");
    else                            kputs("CPIO_PARSE_MISS\n");

    /* —— 判据 2：文件确实灌进 RAM-fs，内容可读回 —— */
    if (check_populate()) kputs("POPULATE_PASS\n");
    else                  kputs("POPULATE_MISS\n");

    /* —— 判据 3：在 fs 里定位 /init —— */
    int init_ino = fs_lookup("init");
    if (init_ino >= 0) {
        kputs("  found /init at inode ");
        kputdec((uint64_t)(uint32_t)init_ino);
        console_putchar('\n');
        kputs("INIT_FOUND_PASS\n");
    } else {
        kputs("INIT_FOUND_MISS\n");
    }

    /* —— 判据 4：把棒子交给用户态——/init 在 U 态跑起来并经 syscall 打印 —— */
    kputs("  handing off to /init (U mode)...\n");
    load_and_run_init();  /* 学生填空点 2 */
    kputs("  kernel reclaimed /init, exit code = ");
    kputdec((uint64_t)g_exit_code);
    console_putchar('\n');
    if (g_proc_done && g_exit_code == 0) kputs("USERSPACE_PASS\n");
    else                                 kputs("USERSPACE_MISS\n");

    if (nfiles == INITRAMFS_NFILES && check_populate() &&
        init_ino >= 0 && g_proc_done && g_exit_code == 0)
        kputs("ALL_PASS\n");
}
