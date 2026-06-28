#!/bin/bash
# Boot StarryOS (qemu-10, single core) with the pip/uv rootfs to an interactive
# shell, for MANUAL acceptance of the main pip/uv commands.
#
# Usage:  bash boot-pipuv.sh <x86|rv|aa|loong>
# Exit qemu:  Ctrl-A then X
#
# The rootfs (built by prep-pip-uv-rootfs.sh) ships CPython 3.14 + uv 0.11.19 +
# offline wheels in /opt/wheels.  -snapshot discards changes so you can re-run.
# These invocations are the exact ones that passed the 4-arch 240/240 gate.
#
# After the shell prompt, bootstrap pip once (uv works immediately):
#   export HOME=/root PIP_BREAK_SYSTEM_PACKAGES=1 PIP_DISABLE_PIP_VERSION_CHECK=1
#   WHL=$(ls /usr/lib/python3.*/ensurepip/_bundled/pip-*.whl | head -1)
#   PYTHONPATH="$WHL" python3 -m pip install --no-index "$WHL" >/dev/null 2>&1; hash -r
# then try: pip3 --version / uv --version / uv venv /tmp/v /
#           uv pip install --python /tmp/v/bin/python --no-index --find-links /opt/wheels setuptools wheel /
#           uv run --no-project python3 -c 'print("ok")'
set -e
ROOT="${TGOSKITS_ROOT:-$HOME/tgoskits}"
# Pull qemu-10 to the front of PATH (iron law: never qemu-8).
[ -f "$ROOT/.starry-env.sh" ] && source "$ROOT/.starry-env.sh"
# Worktree/checkout that holds the built kernels + rootfs imgs.
WT="${STARRY_WT:-$ROOT}"
R="$WT/tmp/axbuild/rootfs"
T="$WT/target"
case "$1" in
  x86)
    qemu-system-x86_64 -nographic -m 2048M -smp 1 -machine q35 \
      -device virtio-blk-pci,drive=disk0 -drive id=disk0,if=none,format=raw,file="$R/rootfs-x86_64-pip314.img" \
      -device virtio-net-pci,netdev=net0 -netdev user,id=net0 -snapshot \
      -kernel "$T/x86_64-unknown-none/release/starryos" ;;
  rv)
    qemu-system-riscv64 -nographic -m 2048M -cpu rv64 -smp 1 -machine virt \
      -device virtio-blk-pci,drive=disk0 -drive id=disk0,if=none,format=raw,file="$R/rootfs-riscv64-pip314.img" \
      -device virtio-net-pci,netdev=net0 -netdev user,id=net0 -snapshot \
      -kernel "$T/riscv64gc-unknown-none-elf/release/starryos.bin" ;;
  aa)
    qemu-system-aarch64 -nographic -m 2048M -cpu cortex-a72 -smp 1 -machine virt \
      -device virtio-blk-pci,drive=disk0 -drive id=disk0,if=none,format=raw,file="$R/rootfs-aarch64-pip314.img" \
      -device virtio-net-pci,netdev=net0 -netdev user,id=net0 -snapshot \
      -kernel "$T/aarch64-unknown-none-softfloat/release/starryos.bin" ;;
  loong)
    qemu-system-loongarch64 -machine virt -cpu la464 -nographic -m 2048M -smp 1 \
      -device virtio-blk-pci,drive=disk0 -drive id=disk0,if=none,format=raw,file="$R/rootfs-loongarch64-pip314.img" \
      -device virtio-net-pci,netdev=net0 -netdev user,id=net0 -snapshot \
      -kernel "$T/loongarch64-unknown-none-softfloat/release/starryos" ;;
  *)
    echo "usage: bash boot-pipuv.sh <x86|rv|aa|loong>" >&2; exit 1 ;;
esac
