#!/bin/bash
# Cross-build a STATIC vvp (iverilog runtime) with the system VPI module embedded,
# for StarryOS. $1 = arch (x86_64|aarch64|riscv64|loongarch64)
set -e
ARCH="$1"
SRC=/tmp/iverilog-src
X=/opt/$ARCH-linux-musl-cross/bin/$ARCH-linux-musl
CC="$X-gcc"; CXX="$X-g++"
OBJ=/tmp/vvp-build-$ARCH; rm -rf "$OBJ"; mkdir -p "$OBJ"
CFLAGS="-O2 -DHAVE_CONFIG_H -DVVP_STATIC_SYSTEM -no-pie -fno-pie -w"
CXXFLAGS="$CFLAGS -std=c++11"

# ---- vvp/*.cc (exclude draw_tt.c host tool) ----
cd "$SRC/vvp"
ls *.cc | xargs -P8 -I{} sh -c "$CXX $CXXFLAGS -I. -I.. -c {} -o $OBJ/vvp_\$(basename {} .cc).o" 

# ---- minimal system module + libvpi (vpi/*.c) ----
cd "$SRC/vpi"
SYSC="sys_convert sys_countdrivers sys_darray sys_deposit sys_display sys_fileio sys_finish sys_plusargs sys_queue sys_random sys_random_mti sys_readmem sys_readmem_lex sys_scanf sys_time sys_priv stringheap mt19937int vams_simparam sys_table_static readline_stub libvpi"
for b in $SYSC; do $CC $CFLAGS -I. -I.. -c "$b.c" -o "$OBJ/vpi_$b.o"; done

# ---- static link ----
cd "$OBJ"
$CXX -static -no-pie -o "/tmp/vvp-$ARCH" *.o -lm 2>&1 | grep -iE "undefined|cannot find|error:" | sort -u | head -20 || true
if [ -x "/tmp/vvp-$ARCH" ]; then
  echo "[$ARCH] LINKED $(du -h /tmp/vvp-$ARCH | cut -f1)  $(file /tmp/vvp-$ARCH | grep -oE 'ELF 64-bit[^,]*,[^,]*')"
else
  echo "[$ARCH] LINK FAILED"; exit 1
fi
