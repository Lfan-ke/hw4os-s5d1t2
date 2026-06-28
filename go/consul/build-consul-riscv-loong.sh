#!/bin/bash
# build-consul-riscv-loong.sh — cross-compile consul v1.22.7 for riscv64 + loongarch64
# from source (no official release exists for these arches). Outputs bare static
# binaries to ./bins/{riscv64,loongarch64}/consul (override the output root via
# CONSUL_DST). See BUILD-PROVENANCE.md for the full rationale.
# Requires: go >=1.20, git, network.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DST=${CONSUL_DST:-"$HERE/bins"}
SRC=${CONSUL_SRC:-/tmp/consul-src}
BOLT_PATCH=/tmp/bolt-patched
GP_PATCH=/tmp/gopsutil-patched
TAG=v1.22.7; COMMIT=c18bcb9d
GOMODCACHE=$(go env GOMODCACHE)

echo "=== [1/4] clone consul $TAG ==="
[ -d "$SRC/.git" ] || git clone --depth 1 --branch "$TAG" https://github.com/hashicorp/consul "$SRC"

echo "=== [2/4] patch boltdb@v1.3.1 (add riscv64+loong64 arch files + go.mod) ==="
rm -rf "$BOLT_PATCH"; cp -r "$GOMODCACHE/github.com/boltdb/bolt@v1.3.1" "$BOLT_PATCH"; chmod -R u+w "$BOLT_PATCH"
printf 'module github.com/boltdb/bolt\n\ngo 1.12\n' > "$BOLT_PATCH/go.mod"
for a in riscv64 loong64; do cat > "$BOLT_PATCH/bolt_$a.go" <<GO
//go:build $a

package bolt

const maxMapSize = 0xFFFFFFFFFFFF // 256TB
const maxAllocSize = 0x7FFFFFFF
var brokenUnaligned = false
GO
done

echo "=== [3/4] patch gopsutil/v3@v3.22.9 (add host_linux_loong64.go) ==="
rm -rf "$GP_PATCH"; cp -r "$GOMODCACHE/github.com/shirou/gopsutil/v3@v3.22.9" "$GP_PATCH"; chmod -R u+w "$GP_PATCH"
cp "$GP_PATCH/host/host_linux_riscv64.go" "$GP_PATCH/host/host_linux_loong64.go"

echo "=== [4/4] cross-compile ==="
cd "$SRC"
go mod edit -replace github.com/boltdb/bolt="$BOLT_PATCH"
go mod edit -replace github.com/shirou/gopsutil/v3="$GP_PATCH"
LD="-s -w -X github.com/hashicorp/consul/version.GitVersion=1.22.7 -X github.com/hashicorp/consul/version.GitCommit=$COMMIT -X github.com/hashicorp/consul/version.GitDescribe=$TAG"
declare -A MAP=( [riscv64]=riscv64 [loong64]=loongarch64 )
for goarch in riscv64 loong64; do
  starch=${MAP[$goarch]}; mkdir -p "$DST/$starch"
  CGO_ENABLED=0 GOOS=linux GOARCH=$goarch go build -trimpath -ldflags "$LD" -o "$DST/$starch/consul" .
  file "$DST/$starch/consul" | head -1
done
echo "DONE"
