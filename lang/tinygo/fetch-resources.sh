#!/bin/bash
# fetch-resources.sh - lang/tinygo
#
# 用途: 重新获取「瘦身」时从交付仓库移除的、可重新下载的资源, 使
# prep-tinygo-rootfs.sh 之后可正常构建 rootfs。需联网; 运行后再跑 prep-tinygo-rootfs.sh。
#
# 据实情况 (已核对 prep-tinygo-rootfs.sh + SOURCES.md):
#   prep 脚本只消费随交付一起发布、且瘦身时【保留】的文件:
#     - testbin/tinygo-<arch>   TinyGo 0.40.0 交叉编译产物 (自建难复现静态二进制)
#                               x86_64 / aarch64 各一; riscv64 / loong64 上游未支持 -> 无产物
#     - golden.txt              host 黄金输出 (11 行, 结尾 TINYGO_OK)
#   prep 脚本【不】从任何 packages/bins/apks 目录解包, 也【不】下载任何包/apk/wheel。
#   因此瘦身时【没有删除任何 prep 所需的可重下载资源】。本脚本无需下载, 只校验上述
#   自建产物确实在位。
#
# (可选, 仅当需要从源码重建 testbin 时) TinyGo 0.40.0 工具链:
#   - github tinygo-org/tinygo release 0.40.0 (内置 go1.22.2 + LLVM 20.1.1 + 自带 musl)。
#   - 用法: GOOS=linux GOARCH=arm64 tinygo build (aarch64 静态); x86_64 默认 musl 动态。
#   逐文件 URL / 版本 / sha256 见下载侧 SOURCES (本仓库 SOURCES.md 未收录 sha256)。
#   该工具链不是 prep 的输入 (产物已随交付发布), 故此处不下载; 如需重建请按 SOURCES.md
#   手动获取 TinyGo 0.40.0 后用它把 main.go 编译为各 arch 二进制。
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
# 仅 x86_64 / aarch64 有产物 (riscv64 / loong64 上游 TinyGo 未支持)。
# 缺失说明交付不完整 -> 请按上方注释从源码重建 (本环境无法代下载)。
missing=0
for a in x86_64 aarch64; do
  if [ ! -f "$HERE/testbin/tinygo-$a" ]; then
    echo "MISSING self-built product (rebuild per SOURCES.md): testbin/tinygo-$a" >&2
    missing=1
  fi
done
if [ ! -f "$HERE/golden.txt" ]; then
  echo "MISSING: golden.txt" >&2
  missing=1
fi
if [ "$missing" -ne 0 ]; then
  echo "fetch-resources: tinygo incomplete (see above)" >&2
  exit 1
fi

echo "fetch-resources: tinygo OK"
