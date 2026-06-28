#!/bin/bash
# prep-jupyter-rootfs.sh — build rootfs-<arch>-jupyter.img for the `jupyter-lab-0`
# StarryOS stress case (#764 jupyter <!-- lab --> = the JupyterLab PRODUCT:
# correct startup + URL curl/wget reachable).  ALL 4 ARCHES, uniform JupyterLab 4.5.7.
#
# CLOSURE STRATEGY (see ../core/apks/SOURCES-python-apks.md §9):
#   * The make-or-break NATIVE pieces come from Alpine *edge* musl apks (py3.14.3),
#     ALL PRE-BUILT by Alpine (no in-guest compilation): py3-pyzmq (cython
#     _zmq.cpython-314-*.so) + libzmq.so.5 + libsodium, py3-ipykernel / ipython /
#     py3-jupyter_client / py3-jupyter_core / jupyter-server + nbconvert/nbformat/
#     nbclient / py3-tornado / py3-traitlets / py3-jinja2, plus the jupyterlab-server
#     deps that ARE apks (babel/json5/jsonschema/requests/packaging/async-lru/
#     notebook-shim).
#   * The JupyterLab FRONT-END (`jupyterlab` apk, 296 bundled static JS) exists as an
#     apk ONLY for x86_64+aarch64; riscv64+loongarch64 DON'T ship the `jupyterlab`
#     apk. To get IDENTICAL JupyterLab 4.5.7 on ALL FOUR arches we therefore install
#     the front-end from the **py3-none-any** PyPI wheels (arch-INDEPENDENT, bundle
#     the 257-asset static front-end): jupyterlab + jupyterlab-server + jupyter-lsp.
#     This avoids two code paths and the riscv/loongarch apk gap.  (jupyter_lsp /
#     jupyterlab_server are not apks on any arch; jupyterlab apk only on x86/aarch.)
#
# Usage:   bash prep-jupyter-rootfs.sh <arch>   # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: a bare global `sync` D-state-deadlocks this host.
# This script uses `debugfs -w` (writes the UNMOUNTED ext4 image directly) so it
# never mounts and never calls sync -> no deadlock. It only touches the jupyter
# image it creates and downloads apks/wheels into ./jupyter/ if missing.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable layout (offline-reproducible delivery): TGOSKITS_ROOT points at the maintainer's
# tgoskits checkout (rootfs imgs under tmp/axbuild/rootfs/); the per-arch jupyter apk closure,
# the front-end wheels, and apk-closure.py ship alongside this script under ./apks/. No abs paths.
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DL="$HERE/apks"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-alpine.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-jupyter.img
APKDIR=$DL/$ARCH
WHEELDIR=$DL/wheels
STAGE=/tmp/jl-stage-$ARCH
PYVER=3.14

[ -f "$BASE" ] || { echo "missing base $BASE"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. resolve+download the jupyter-server native apk closure ----------------
# Roots: jupyter-server (pulls nbconvert/nbformat/nbclient/tornado/traitlets/jinja2/
# jupyter_client/jupyter_core/anyio/argon2/jupyter-events/overrides/prometheus/
# send2trash/terminado/websocket-client + the Qt5 dead-weight nbconvert drags in),
# py3-ipykernel (pulls ipython + py3-pyzmq + libzmq + libsodium), and the
# jupyterlab[-server] runtime deps that exist as apks.
echo "=== [$ARCH] resolve+download jupyter-server native closure -> $APKDIR ==="
mkdir -p "$APKDIR"
python3 "$DL/apk-closure.py" --arch "$ARCH" --out "$APKDIR" \
  --repo edge/main python3 py3-jinja2 py3-requests py3-packaging py3-babel \
  --repo edge/community jupyter-server py3-ipykernel py3-async-lru \
         jupyter-notebook-shim py3-json5 py3-jsonschema py3-pyzmq py3-httpx
rc=$?
[ "$rc" -ge 2 ] && { echo "  RESOLVER FATAL ($rc)"; exit 2; }
N=$(ls "$APKDIR"/*.apk 2>/dev/null | wc -l)
echo "  closure apks: $N"
[ "$N" -ge 120 ] || { echo "  closure too small ($N) — aborting"; exit 2; }

# --- 1b. vendor the arch-independent front-end wheels (once) ------------------
mkdir -p "$WHEELDIR"
if [ -z "$(ls -A "$WHEELDIR"/jupyterlab-*.whl 2>/dev/null)" ]; then
  echo "=== vendor py3-none-any front-end wheels into $WHEELDIR ==="
  python3 -m pip download --only-binary=:all: --no-deps -d "$WHEELDIR" \
    "jupyterlab==4.5.7" "jupyterlab-server>=2.28.0" "jupyter-lsp>=2.0.0" \
    2>&1 | grep -iE 'Saved|ERROR' || echo "  WARN wheel download failed"
else
  echo "=== front-end wheels already vendored ==="; ls "$WHEELDIR"/*.whl | xargs -n1 basename
fi

# --- 2. copy base image, grow to 4G (Qt5 dead-weight from nbconvert is bulky) -
echo "=== [$ARCH] copy base -> $IMG (resize 4G) ==="
cp -f "$BASE" "$IMG"
truncate -s 4G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 3. stage: extract every apk payload into one tree (later apks win) -------
echo "=== [$ARCH] stage apk payloads into $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
shopt -s nullglob
for apk in "$APKDIR"/*.apk; do
  tar xzf "$apk" -C "$STAGE" \
    --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.pre-install' \
    --exclude='.post-install' --exclude='.pre-upgrade' --exclude='.post-upgrade' \
    --exclude='.trigger' 2>/dev/null && echo "  + $(basename "$apk")" || echo "  ! $(basename "$apk") (partial)"
done

# --- 3b. install the front-end wheels into the staged guest site-packages -----
# py3-none-any -> host pip can --target them straight into the guest tree; --no-deps
# + --no-index because every transitive dep is already provided by the apks above.
SP="$STAGE/usr/lib/python$PYVER/site-packages"
mkdir -p "$SP"
if [ -n "$(ls -A "$WHEELDIR"/*.whl 2>/dev/null)" ]; then
  echo "=== install front-end wheels into $SP (offline, no-deps) ==="
  TMPT=/tmp/jl-wheels-$ARCH; rm -rf "$TMPT"; mkdir -p "$TMPT"
  python3 -m pip install --no-index --no-deps --find-links "$WHEELDIR" --target "$TMPT" \
    jupyterlab jupyterlab-server jupyter-lsp 2>&1 | tail -4
  cp -a "$TMPT"/. "$SP"/ && echo "  front-end wheels merged into staged site-packages"
  # pip --target dumps the wheels' .data/data/{share,bin} into the target ROOT, so the
  # cp above lands them under site-packages/{share,bin}. But JupyterLab serves its built
  # front-end from <prefix>/share/jupyter/lab (= /usr/share/jupyter/lab), NOT
  # site-packages/share — leaving them there makes /lab return a 329-byte error page
  # ("JupyterLab application assets not found in /usr/share/jupyter/lab"). The x86/aarch64
  # `jupyterlab` apk places them correctly; for the riscv64/loongarch64 wheel path we must
  # relocate them ourselves into the guest /usr/share + /usr/bin.
  if [ -d "$SP/share" ]; then mkdir -p "$STAGE/usr/share"; cp -a "$SP/share/." "$STAGE/usr/share/"; rm -rf "$SP/share"; fi
  if [ -d "$SP/bin" ];   then mkdir -p "$STAGE/usr/bin";   cp -a "$SP/bin/."   "$STAGE/usr/bin/";   rm -rf "$SP/bin";   fi
  rm -rf "$TMPT"
else
  echo "  WARN no wheels -> JupyterLab front-end MISSING (server-only)"
fi
# musl loader search path for the guest
printf '/lib\n/usr/lib\n' > "$STAGE/etc-ld-musl-$ARCH.path"

# --- 4. write the staged tree into the UNMOUNTED ext4 via debugfs -------------
echo "=== [$ARCH] inject tree into $IMG via debugfs -w (no mount, no sync) ==="
DBG=/tmp/jl-debugfs-$ARCH.cmds
: > "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /$rel" >> "$DBG"
  echo "write $STAGE/$rel /$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
  rel="${l#./}"; tgt=$(readlink "$STAGE/$rel")
  echo "rm /$rel" >> "$DBG"
  echo "symlink /$rel $tgt" >> "$DBG"
done
echo "rm /etc/ld-musl-$ARCH.path" >> "$DBG"
echo "write $STAGE/etc-ld-musl-$ARCH.path /etc/ld-musl-$ARCH.path" >> "$DBG"

debugfs -w -f "$DBG" "$IMG" >/tmp/jl-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/jl-debugfs-$ARCH.log | grep -viE 'File not found.*rm|rm:.*not found' | head

# --- 5. verify the jupyterlab stack landed -------------------------------------
echo "=== [$ARCH] verify jupyter stack in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /usr/bin/python3"   "$IMG" 2>/dev/null | head -2
debugfs -R "ls /usr/lib/python$PYVER/site-packages/jupyterlab/static" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -c '\.js' | sed 's/^/  jupyterlab front-end static .js count: /'
debugfs -R "ls /usr/lib/python$PYVER/site-packages/zmq/backend/cython" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -i '_zmq' && echo "  pyzmq native .so OK"
debugfs -R "ls /usr/lib" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -i 'libzmq.so.5' | head -1 && echo "  libzmq OK"
debugfs -R "ls /usr/lib/python$PYVER/site-packages/ipykernel" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -i '__init__' && echo "  ipykernel OK"
debugfs -R "ls /usr/lib/python$PYVER/site-packages/jupyter_server" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -i 'serverapp' && echo "  jupyter_server OK"
debugfs -R "ls /usr/lib/python$PYVER/site-packages/jupyterlab_server" "$IMG" 2>/dev/null | tr ' ' '\n' | grep -i '__init__' && echo "  jupyterlab_server OK"
echo "[$ARCH] DONE -> $IMG"
