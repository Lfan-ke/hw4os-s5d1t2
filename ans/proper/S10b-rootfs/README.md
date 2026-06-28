# 正经·S10b · initramfs：解开内嵌 cpio → 灌进 RAM-fs → 交棒 /init · 参考解

> 与 `exercises/proper/S10b-rootfs` 同构；此处 `kernel/cpio.c` 的 `cpio_parse_one`
> 与 `kernel/main.c` 的 `load_and_run_init` 已实现。
> 把 S07（RAM-fs）与 S08（U 态/syscall）接成「内核启动尾声」：解包 initramfs → 跑 /init。

## 主线（kmain，给定）

```
fs_mkfs()                              一张空 RAM 根文件系统（承 S07）
cpio_unpack(initramfs_cpio, len)       逐条解析 newc 头 → fs_create + fs_write 灌进 RAM-fs
fs_lookup("init")                      在 fs 里定位 /init
load_and_run_init()                    fs_read /init → run_user 跌入 U 态（承 S08）
                                       /init 经 ecall 打印 banner、exit(0)，longjmp 回内核
```

四道判据：`CPIO_PARSE_PASS`（成员数 == `INITRAMFS_NFILES=4`）、`POPULATE_PASS`（文本文件内容
可读回）、`INIT_FOUND_PASS`（fs 定位 /init）、`USERSPACE_PASS`（/init 在 U 态跑通、exit 0），
全过打印 `ALL_PASS`。

## 内嵌 initramfs（`initramfs.h`，给定）

`initramfs_cpio[]` 是用 `cpio -H newc` 真打的归档（已截到 `TRAILER!!!` 收尾），成员：

| 名字         | 内容                                   |
|--------------|----------------------------------------|
| `README`     | 一段欢迎文本                           |
| `etc_config` | `hostname=...\nmode=batch\n`           |
| `motd`       | 一行 the quick brown fox…              |
| `init`       | **/init 的扁平机器码**（96 字节，见下）|

`/init` 由 `init.S` 汇编为扁平二进制（`-Ttext=0 -e _start`，objcopy -O binary）后嵌入：
`sys_write(1, msg, 58)` + `sys_exit(0)`，`msg` 用 `lla`(auipc+addi) PC 相对寻址，
故拷到任意 16 字节对齐缓冲都能跑。源见 `initramfs.h` 顶部注释。

## 两处实现点

- `cpio_parse_one`（`cpio.c`）：读 newc 头 110B ASCII 的 namesize(+94)/filesize(+54)，
  名字 = h+110，数据 = `off+align4(110+namesize)`，下一条 = `数据+align4(filesize)`，
  `TRAILER!!!` 处返回 0。≈ Linux `init/initramfs.c` 的 `do_header/do_name`。
- `load_and_run_init`（`main.c`）：`fs_lookup("init")` → `fs_read` 进 `init_exec` →
  `run_user`。≈ 内核执行第一个用户态进程 `/init`。

## 自验证

```
make -C kernel kernel.elf
timeout --kill-after=3 20 qemu-system-riscv64 -machine virt -smp 4 -nographic -bios default -kernel kernel.elf > /tmp/q.txt 2>&1
grep -aE "CPIO_PARSE_PASS|POPULATE_PASS|INIT_FOUND_PASS|USERSPACE_PASS|ALL_PASS|INIT:" /tmp/q.txt
make -C kernel clean
```

期望依次出现四个 `*_PASS`、`/init` 自己的 `INIT: ...` banner 与 `ALL_PASS`，qemu 经
`k_shutdown` 正常退出。

## 计分

`require=1`：软件 C 变体（`kernel-c`）打全 4 个 `*_PASS` + `ALL_PASS` 即必修达成。
essay 变体独立判，作辅助账。
