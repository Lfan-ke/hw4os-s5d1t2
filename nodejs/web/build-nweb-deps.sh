#!/bin/bash
# build-nweb-deps.sh — rebuild the node-web carpet dependency closure (CI-like, no bundled
# node_modules). Produces the assets/ + programs/ layout that apps-starry/prebuild.sh stages.
#
# Inputs (co-located in this delivery repo + one fetched dependency set):
#   - a Node.js + npm toolchain (the carpets target node v22.22.2; any recent npm can resolve
#     the pinned versions in package.json)
#   - package.json pinning express@4.21.2 + pug@3.0.3 (npm fetches their closure from the
#     registry; see SOURCES.md for offline-mirror notes)
#   - the carpet sources under carpets/ (PugCarpet.js / ExpressCarpet.js / KotlinJsCarpet.js)
#     and the host-precompiled Kotlin/JS module carpets/kotlin-app.js (+ kotlin-REF.out golden)
# Output: an out/ tree mirroring the upstream apps/starry/node-web app —
#   out/assets/node_modules           (the pug/express closure)
#   out/assets/kotlin-app.js + kotlin-REF.out
#   out/programs/carpets/*.js + out/programs/run-nweb.sh
#   out/{prebuild.sh,*.toml,README.md}  (copied from apps-starry/)
# which is then placed at tgoskits apps/starry/node-web/ to run on-target.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NPM="${NPM:-npm}"
OUT="$HERE/out"
rm -rf "$OUT"; mkdir -p "$OUT/assets" "$OUT/programs/carpets"

# 1. resolve the pug/express closure from package.json into a clean node_modules
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cp "$HERE/package.json" "$TMP/"
( cd "$TMP" && "$NPM" install --no-audit --no-fund --omit=dev --loglevel=error )
mv "$TMP/node_modules" "$OUT/assets/node_modules"
echo "build-nweb: node_modules closure = $(find "$OUT/assets/node_modules" -maxdepth 1 -mindepth 1 -type d | wc -l) packages, $(du -sh "$OUT/assets/node_modules" | cut -f1)"

# 2. carpets + run gate + Kotlin/JS module (kotlin-app.js lives next to KotlinJsCarpet via __dirname)
install -m0644 "$HERE/carpets/PugCarpet.js" "$HERE/carpets/ExpressCarpet.js" "$HERE/carpets/KotlinJsCarpet.js" "$OUT/programs/carpets/"
install -m0644 "$HERE/carpets/kotlin-app.js"  "$OUT/assets/kotlin-app.js"
install -m0644 "$HERE/carpets/kotlin-REF.out" "$OUT/assets/kotlin-REF.out"
install -m0755 "$HERE/run-nweb.sh" "$OUT/programs/run-nweb.sh"

# 3. app config (4-arch build/qemu toml + prebuild.sh + README)
cp "$HERE/apps-starry/"*.toml "$HERE/apps-starry/prebuild.sh" "$HERE/apps-starry/README.md" "$OUT/"

echo "build-nweb: out/ ready — place at tgoskits apps/starry/node-web/ (see SOURCES.md)"
