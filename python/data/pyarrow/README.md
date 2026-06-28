# pyarrow (python-arrow-0) — Apache Arrow / Parquet I/O 正确性 (DoD on StarryOS, 四架构)

## 这是什么
[pyarrow](https://arrow.apache.org/docs/python/) 21.0.0（musl-native）跑在 python3 运行时上，
驱动 Apache Arrow / Parquet 的完整 C++ 栈：`libarrow` / `libparquet` / `libthrift` /
`libprotobuf` + Snappy/Zstd 压缩编解码器 + 列式 Parquet 文件的 mmap/threads/file I/O。
对应 #764 的 `pyarrow <!-- parquet -->` 条目。

## 测试方法（DoD，非 exit-0）
在内存中构造一个 Arrow Table（int32 / int64 / string / float64 / 含空串的 string 共 5 列 5 行），
用 `pyarrow.parquet.write_table` 写入磁盘上真实的 Parquet 文件，再用 `read_table` 读回，断言：
1. **schema 完全相等**（字段名 + 类型）；
2. **全表值相等**（`table.equals(back)`）；
3. **逐列值抽查相等**（独立于 `.equals()` 的纵深校验）；
4. 写出的 Parquet 文件字节数 > 0。

四者全真才由 shell 受控打印 `PYARROW_OK=1`（`success_regex = ["(?m)^PYARROW_OK=1"]`，
末尾单条 printf 防 success_regex 假阳性）。

## 四架构验证结果（qemu-10 单核 starry）
| arch | 结果 | 备注 |
|------|------|------|
| x86_64 | √ PYARROW_OK=1 / RT_OK / SEGV=0 | **需内核 AVX/XCR0 修复**（见下）+ `-cpu Haswell` |
| aarch64 | √ PYARROW_OK=1 / RT_OK / SEGV=0 | `-cpu cortex-a72` |
| riscv64 | √ PYARROW_OK=1 / RT_OK / SEGV=0 | `-cpu rv64` |
| loongarch64 | √ PYARROW_OK=1 / RT_OK / SEGV=0 | `-machine virt -cpu la464` |

= 真 4/4。所有架构 Parquet round-trip 精确相等，无 SIGSEGV。

## 依赖的内核修复（x86 关键）
x86 此前崩在 "illegal instruction"：StarryOS x86 启动从不设 `CR4.OSXSAVE`、从未写 `XCR0`，
导致用户态第一条 VEX 编码的 AVX 指令触发 `#UD`（Arrow/abseil RANDEN 探测后跑 AVX/AVX2/AES-NI）。
根因修复（与 Linux x86 启动规范一致）位于 tgoskits fork：
- `components/axplat_crates/platforms/axplat-x86-pc/src/boot.rs`：CR4 fp-simd 分支加 `OSXSAVE`(bit18)。
- `.../src/init.rs`：`enable_xsave_features()` 在每核 `init_trap()` 后经 CPUID guard 写
  `XCR0.{X87,SSE,[AVX]}`（仅 CPU 支持 XSAVE 才写，支持 AVX 才附加 AVX 位）。

对应提交 `fix(axplat-x86-pc): enable XCR0 AVX/SSE state so userspace AVX doesn't #UD`
（Refs rcore-os/tgoskits#250 #764）。这是所有 x86 AVX 应用的 foundational 修复。
其余三架构 qemu 默认 `-cpu` 已带向量扩展，无需此修复。

## 文件
- `case/build-<arch>.toml` — 四架构 starry 内核 build 描述（target/env/log/features/plat）。
- `case/python-arrow-0/qemu-<arch>.toml` — 四架构 qemu 运行 + 探针（单核 `-smp 1`；
  x86 显式 `-cpu Haswell`，含 AVX2+AES-NI+XSAVE 的最小 stock model）。
- `prep-python-arrow-rootfs.sh` — rootfs 构建：在 `rootfs-<arch>-python.img` 基础上用
  debugfs 注入（不 mount，WSL2-safe）pyarrow + Arrow/Parquet C++ 闭包
  （Alpine v3.23 musl apks：`py3-pyarrow` + `libarrow*`/`libparquet` + 闭包）。
  产物 `tmp/axbuild/rootfs/rootfs-<arch>-python-arrow.img`。
  资产来源见同目录 `SOURCES.md`（素材已随附本目录 Git LFS）。

## 在四架构 starry 上运行（qemu-10）
```bash
# 0) 必须用 qemu-10（loong 硬性要求；默认 qemu-8 不可）
export TGOSKITS_ROOT=$HOME/tgoskits   # 你的 tgoskits checkout
source "$TGOSKITS_ROOT/.starry-env.sh"
# 1) 构建 rootfs（每架构一次；素材随附本目录, 离线可复现）
bash prep-python-arrow-rootfs.sh <arch>   # arch ∈ x86_64|aarch64|riscv64|loongarch64
# 2) 跑（内核含 AVX/XCR0 修复）
cargo xtask starry test qemu --arch <arch> -g stress -c python-arrow-0
# 3) 判定：标准输出出现 ^PYARROW_OK=1 即通过
```
