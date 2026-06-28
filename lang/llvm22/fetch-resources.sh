#!/bin/bash
# fetch-resources.sh - lang/llvm22
#
# 用途: 重新获取「瘦身」时从交付仓库移除的、可重新下载的资源, 使
# prep-llvm22-rootfs.sh 之后可正常构建 rootfs。需联网; 运行后再跑 prep-llvm22-rootfs.sh。
#
# 据实情况 (已核对 prep-llvm22-rootfs.sh + SOURCES.md):
#   prep 脚本只消费随交付一起发布、且瘦身时【保留】的文件:
#     - testbin/llvm22-<arch>   clang-22 / LLVM 22.1.6 AOT 交叉编译产物 (自建难复现静态二进制)
#     - golden.txt              host 黄金输出
#   prep 脚本【不】从任何 packages/bins/apks 目录解包, 也【不】下载任何包/apk/wheel。
#   因此瘦身时【没有删除任何 prep 所需的可重下载资源】。本脚本无需下载, 只校验上述
#   自建产物确实在位。
#
# (可选, 仅当需要从源码重建 testbin 时) clang-22 / LLVM 22.1.6 工具链:
#   - x86_64 / aarch64: LLVM 官方 release LLVM-22.1.6-Linux-{X64,ARM64}.tar.xz
#   - riscv64 / loongarch64: 社区 / 自编 chain
#   逐文件 URL / 版本 / sha256 见下载侧 llvm-bins/SOURCES.md (本仓库 SOURCES.md 未收录 sha256)。
#   该工具链不是 prep 的输入 (产物已随交付发布), 故此处不下载; 如需重建请按 SOURCES.md
#   手动获取工具链后用 clang-22 把 llvm22_test.cpp 交叉编译为各 arch 静态二进制。
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

# fetch <url> <dest> <sha256>: dest 已存在且 sha256 匹配则跳过; 否则下载 (curl -fL --retry 3)
# 并校验 sha256, 不匹配则删除并报错退出。
# 注: 本 app 无直链可重下载资源 (prep 只用随交付保留的自建产物), 故此函数当前无调用,
# 保留以符合 fetch-resources 模板; 若日后新增直链资源, 须用 SOURCES.md 的权威 sha256 调用。
fetch() {
  local url="$1" dest="$2" sha="$3" got
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | cut -d' ' -f1)" = "$sha" ]; then
    echo "skip (already ok): $dest"; return 0
  fi
  mkdir -p "$(dirname "$dest")"
  echo "fetch: $url -> $dest"
  curl -fL --retry 3 -o "$dest" "$url"
  got="$(sha256sum "$dest" | cut -d' ' -f1)"
  if [ "$got" != "$sha" ]; then
    echo "sha256 mismatch for $dest: got $got want $sha" >&2
    rm -f "$dest"; exit 1
  fi
}

# 本 app 无可重下载资源; 校验随交付保留的自建产物 + golden 在位。
# 缺失说明交付不完整 -> 请按上方注释从源码重建 (本环境无法代下载)。
missing=0
for a in x86_64 aarch64 riscv64 loongarch64; do
  if [ ! -f "$HERE/testbin/llvm22-$a" ]; then
    echo "MISSING self-built product (rebuild per SOURCES.md): testbin/llvm22-$a" >&2
    missing=1
  fi
done
if [ ! -f "$HERE/golden.txt" ]; then
  echo "MISSING: golden.txt" >&2
  missing=1
fi
if [ "$missing" -ne 0 ]; then
  echo "fetch-resources: llvm22 incomplete (see above)" >&2
  exit 1
fi

echo "fetch-resources: llvm22 OK"
