#!/bin/bash
# fetch-resources.sh — re-fetch the slimmed-away, re-downloadable resources for the
# `celery-0` StarryOS case so that prep-celery-rootfs.sh can build the rootfs again.
#
# The delivery repo was slimmed: every artifact re-downloadable from an upstream
# registry was removed; only build/prep scripts were kept. This script restores them at
# the EXACT paths/filenames prep-celery-rootfs.sh expects and verifies each by sha256:
#   ./wheels/<name>-<ver>-*.whl                            (pure-python closure)
#   ./tornado-musllinux/<arch>/tornado-6.5.5-cp39-abi3-musllinux_1_2_<arch>.whl
#   ./tornado-sdist/tornado-6.5.5.tar.gz                   (pure-python fallback, riscv/loong)
#
# Requires: network access + a host python3 with pip. Run this FIRST, then run
#   bash prep-celery-rootfs.sh <arch>
#
# celery/flower + 18 deps are pure-python (py3-none-any / py2.py3-none-any, arch-independent)
# PyPI wheels. tornado 6.5.5 is the only package with a native part: x86_64/aarch64 use the
# PyPI musllinux wheel (carries tornado/speedups.abi3.so); riscv64/loongarch64 reuse the PyPI
# sdist's pure-python tree (no .so -> tornado auto-falls-back). redis is vendored for
# completeness (a celery result-backend dep; the default filesystem:// broker case does not
# install it). All resources come from PyPI; the sha256 values are the digests of the
# artifacts that originally shipped here (authoritative ground truth; no SOURCES.md hashes).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WHEELS="$HERE/wheels"

# pip_fetch <dest-file> <sha256> <pip-download-args...>
#   skip if dest present with matching sha256; else `pip download` into a temp dir,
#   require the exact basename, verify sha256, then move into place.
pip_fetch() {
  local dest="$1" want="$2"; shift 2
  local base; base="$(basename "$dest")"
  if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$want" ]; then
    echo "  [skip] $base (sha256 ok)"; return 0
  fi
  if [ -f "$dest" ]; then echo "  [stale] $base -> re-download"; rm -f "$dest"; fi
  mkdir -p "$(dirname "$dest")"
  local td; td="$(mktemp -d)"
  if ! python3 -m pip download --no-deps --disable-pip-version-check -d "$td" "$@"; then
    rm -rf "$td"; echo "  [ERR] pip download failed: $*" >&2; return 1
  fi
  if [ ! -f "$td/$base" ]; then
    echo "  [ERR] expected '$base' not produced by: pip download $*" >&2
    echo "        produced: $(cd "$td" && ls)" >&2
    rm -rf "$td"; return 1
  fi
  local got; got="$(sha256sum "$td/$base" | awk '{print $1}')"
  if [ "$got" != "$want" ]; then
    echo "  [ERR] sha256 mismatch for $base: got $got want $want" >&2
    rm -rf "$td"; return 1
  fi
  mv "$td/$base" "$dest"; rm -rf "$td"
  echo "  [ok] $base"
}

echo "=== fetch celery/flower pure-python closure (PyPI) -> $WHEELS ==="
# rows: <dest-basename> <sha256> <pip-spec>
WHEEL_ROWS=(
  "amqp-5.3.1-py3-none-any.whl                     43b3319e1b4e7d1251833a93d672b4af1e40f3d632d479b98661a95f117880a2 amqp==5.3.1"
  "billiard-4.2.4-py3-none-any.whl                 525b42bdec68d2b983347ac312f892db930858495db601b5836ac24e6477cde5 billiard==4.2.4"
  "celery-5.5.3-py3-none-any.whl                   0b5761a07057acee94694464ca482416b959568904c9dfa41ce8413a7d65d525 celery==5.5.3"
  "click-8.4.1-py3-none-any.whl                    482be17c6991b8c19c5429a1e995d9b0efdbb63172824c41f99965dc0ade8ec2 click==8.4.1"
  "click_didyoumean-0.3.1-py3-none-any.whl         5c4bb6007cfea5f2fd6583a2fb6701a22a41eb98957e63d0fac41c10e7c3117c click-didyoumean==0.3.1"
  "click_plugins-1.1.1.2-py2.py3-none-any.whl      008d65743833ffc1f5417bf0e78e8d2c23aab04d9745ba817bd3e71b0feb6aa6 click-plugins==1.1.1.2"
  "click_repl-0.3.0-py3-none-any.whl               fb7e06deb8da8de86180a33a9da97ac316751c094c6899382da7feeeeb51b812 click-repl==0.3.0"
  "flower-2.0.1-py2.py3-none-any.whl               9db2c621eeefbc844c8dd88be64aef61e84e2deb29b271e02ab2b5b9f01068e2 flower==2.0.1"
  "humanize-4.15.0-py3-none-any.whl                b1186eb9f5a9749cd9cb8565aee77919dd7c8d076161cf44d70e59e3301e1769 humanize==4.15.0"
  "kombu-5.5.4-py3-none-any.whl                    a12ed0557c238897d8e518f1d1fdf84bd1516c5e305af2dacd85c2015115feb8 kombu==5.5.4"
  "packaging-26.2-py3-none-any.whl                 5fc45236b9446107ff2415ce77c807cee2862cb6fac22b8a73826d0693b0980e packaging==26.2"
  "prometheus_client-0.25.0-py3-none-any.whl       d5aec89e349a6ec230805d0df882f3807f74fd6c1a2fa86864e3c2279059fed1 prometheus-client==0.25.0"
  "prompt_toolkit-3.0.52-py3-none-any.whl          9aac639a3bbd33284347de5ad8d68ecc044b91a762dc39b7c21095fcd6a19955 prompt-toolkit==3.0.52"
  "python_dateutil-2.9.0.post0-py2.py3-none-any.whl a8b2bc7bffae282281c8140a97d3aa9c14da0b136dfe83f850eea9a5f7470427 python-dateutil==2.9.0.post0"
  "pytz-2026.2-py2.py3-none-any.whl                04156e608bee23d3792fd45c94ae47fae1036688e75032eea2e3bf0323d1f126 pytz==2026.2"
  "redis-7.4.0-py3-none-any.whl                    a9c74a5c893a5ef8455a5adb793a31bb70feb821c86eccb62eebef5a19c429ec redis==7.4.0"
  "six-1.17.0-py2.py3-none-any.whl                 4721f391ed90541fddacab5acf947aa0d3dc7d27b2e1e8eda2be8970586c3274 six==1.17.0"
  "tzdata-2026.2-py2.py3-none-any.whl              bbe9af844f658da81a5f95019480da3a89415801f6cc966806612cc7169bffe7 tzdata==2026.2"
  "vine-5.1.0-py3-none-any.whl                     40fdf3c48b2cfe1c38a49e9ae2da6fda88e4794c810050a728bd7413811fb1dc vine==5.1.0"
  "wcwidth-0.7.0-py3-none-any.whl                  5d69154c429a82910e241c738cd0e2976fac8a2dd47a1a805f4afed1c0f136f2 wcwidth==0.7.0"
)
for row in "${WHEEL_ROWS[@]}"; do
  read -r f sha spec <<<"$row"
  pip_fetch "$WHEELS/$f" "$sha" --only-binary=:all: "$spec"
done

echo "=== fetch tornado 6.5.5 native musllinux wheels (PyPI) -> tornado-musllinux/ ==="
pip_fetch "$HERE/tornado-musllinux/x86_64/tornado-6.5.5-cp39-abi3-musllinux_1_2_x86_64.whl" \
  36abed1754faeb80fbd6e64db2758091e1320f6bba74a4cf8c09cd18ccce8aca \
  --only-binary=:all: --platform musllinux_1_2_x86_64 --implementation cp --python-version 3.9 --abi abi3 "tornado==6.5.5"
pip_fetch "$HERE/tornado-musllinux/aarch64/tornado-6.5.5-cp39-abi3-musllinux_1_2_aarch64.whl" \
  3f54aa540bdbfee7b9eb268ead60e7d199de5021facd276819c193c0fb28ea4e \
  --only-binary=:all: --platform musllinux_1_2_aarch64 --implementation cp --python-version 3.9 --abi abi3 "tornado==6.5.5"

echo "=== fetch tornado 6.5.5 sdist (PyPI, pure-python fallback) -> tornado-sdist/ ==="
pip_fetch "$HERE/tornado-sdist/tornado-6.5.5.tar.gz" \
  192b8f3ea91bd7f1f50c06955416ed76c6b72f96779b962f07f911b91e8d30e9 \
  --no-binary=:all: "tornado==6.5.5"

echo "fetch-resources: celery OK"
