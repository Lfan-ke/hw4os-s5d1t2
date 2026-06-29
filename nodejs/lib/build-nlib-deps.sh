#!/bin/bash
# build-nlib-deps.sh — rebuild the node-lib carpet dependency closure (CI-like, no bundled
# node_modules). Produces the assets/ + programs/ layout that apps-starry/prebuild.sh stages.
#
# Inputs: a Node.js + npm toolchain (carpets target node v22.22.2); package.json pinning
#   less / stylus / sass / @babel/core + preset-typescript + preset-react / terser / eslint;
#   the carpet sources under carpets/.
# Output: out/ mirroring the upstream apps/starry/node-lib app (assets/node_modules +
#   programs/carpets/*.js + programs/run-nlib.sh + prebuild.sh + *.toml + README.md).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NPM="${NPM:-npm}"
OUT="$HERE/out"
rm -rf "$OUT"; mkdir -p "$OUT/assets" "$OUT/programs/carpets"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cp "$HERE/package.json" "$TMP/"
( cd "$TMP" && "$NPM" install --no-audit --no-fund --omit=dev --loglevel=error )
# slim the closure: drop docs / sourcemaps / type defs / test dirs (not read at runtime)
find "$TMP/node_modules" -type f \( -iname '*.md' -o -iname '*.markdown' -o -iname 'LICENSE*' -o -iname 'license*' -o -iname '*.map' -o -iname '*.d.ts' -o -iname 'CHANGELOG*' \) -delete 2>/dev/null || true
find "$TMP/node_modules" -type d \( -name test -o -name tests -o -name __tests__ -o -name docs -o -name doc -o -name '.github' -o -name example -o -name examples \) -exec rm -rf {} + 2>/dev/null || true
mv "$TMP/node_modules" "$OUT/assets/node_modules"
echo "build-nlib: node_modules closure = $(find "$OUT/assets/node_modules" -maxdepth 1 -mindepth 1 -type d | wc -l) packages, $(du -sh "$OUT/assets/node_modules" | cut -f1)"
install -m0644 "$HERE"/carpets/*.js "$OUT/programs/carpets/"
install -m0755 "$HERE/run-nlib.sh" "$OUT/programs/run-nlib.sh"
cp "$HERE/apps-starry/"*.toml "$HERE/apps-starry/prebuild.sh" "$HERE/apps-starry/README.md" "$OUT/"
echo "build-nlib: out/ ready — place at tgoskits apps/starry/node-lib/ (see SOURCES.md)"
