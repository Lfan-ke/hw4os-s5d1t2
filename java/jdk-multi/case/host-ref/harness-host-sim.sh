#!/bin/bash
# harness-host-sim.sh — host simulation of the openjdk-multi-0 shell_init_cmd.
# Validates the EXACT gate logic + per-version flags + version-switch sub-test
# that the qemu-*.toml embeds, but against the host JDK homes (musl tars + libz)
# instead of /opt/jdkNN in the guest. NOT shipped into the image; reference only.
#
# Host JDK homes (set up by the validation steps):
#   17 = system glibc /usr/lib/jvm/java-17-openjdk-amd64
#   21 = system glibc /usr/lib/jvm/java-21-openjdk-amd64
#   23 = musl tar     /tmp/jdktest23/jdk-23.0.2   (run w/ libz on LD_LIBRARY_PATH)
#   25 = musl tar     /tmp/jdktest25/jdk-25
set -u
L=/tmp/muslz/usr/lib
J17=/usr/lib/jvm/java-17-openjdk-amd64
J21=/usr/lib/jvm/java-21-openjdk-amd64
J23=/tmp/jdktest23/jdk-23.0.2
J25=/tmp/jdktest25/jdk-25
SRC="${JDK_MULTI_PROGRAMS:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../programs" && pwd)}"

# run_java <jdkhome> <args...> — host helper. The musl tar JDKs (23/25) need libz
# (from $L) on the lib path; the system glibc JDKs (17/21) must NOT see the musl
# libz dir or their glibc loader picks up libc.musl. In the GUEST all four are musl
# and resolve uniformly via /etc/ld-musl-<arch>.path, so this split is host-only.
rj() {
  local h="$1"; shift
  case "$h" in
    /tmp/jdktest*) LD_LIBRARY_PATH="$L:$h/lib:$h/lib/server" "$h/bin/java" "$@" ;;
    *)             "$h/bin/java" "$@" ;;
  esac
}

PASS=0; TOTAL=0
acc(){ TOTAL=$((TOTAL+1)); if [ "$1" = 1 ]; then PASS=$((PASS+1)); else echo "  SUITE FAIL ($2)"; fi; }

echo "=== JDK17 features ==="
rj "$J17" "$SRC/Jdk17Features.java" >/tmp/o17 2>&1
grep -q '^JDK17_OK$' /tmp/o17 && { echo "JDK17_OK printed"; acc 1 J17; } || { tail -8 /tmp/o17; acc 0 J17; }

echo "=== JDK21 features ==="
rj "$J21" "$SRC/Jdk21Features.java" >/tmp/o21 2>&1
grep -q '^JDK21_OK$' /tmp/o21 && { echo "JDK21_OK printed"; acc 1 J21; } || { tail -8 /tmp/o21; acc 0 J21; }

echo "=== JDK23 features (preview) ==="
rj "$J23" --enable-preview --source 23 "$SRC/Jdk23Features.java" >/tmp/o23 2>&1
grep -q '^JDK23_OK$' /tmp/o23 && { echo "JDK23_OK printed"; acc 1 J23; } || { tail -8 /tmp/o23; acc 0 J23; }

echo "=== JDK25 features (preview) ==="
rj "$J25" --enable-preview --source 25 "$SRC/Jdk25Features.java" >/tmp/o25 2>&1
grep -q '^JDK25_OK$' /tmp/o25 && { echo "JDK25_OK printed"; acc 1 J25; } || { tail -8 /tmp/o25; acc 0 J25; }
# compact object headers second run
rj "$J25" -XX:+UseCompactObjectHeaders --enable-preview --source 25 "$SRC/Jdk25Features.java" >/tmp/o25c 2>&1
grep -q 'compact-object-headers flag present = true' /tmp/o25c && grep -q '^JDK25_OK$' /tmp/o25c \
  && { echo "JDK25 compact-headers run OK"; acc 1 J25-compact; } || { tail -8 /tmp/o25c; acc 0 J25-compact; }

echo "=== version switch (update-alternatives-style JAVA_HOME + symlink) ==="
# guest layout simulated with /tmp/optjdk/jdkNN + a retargeted current symlink
rm -rf /tmp/optjdk; mkdir -p /tmp/optjdk
ln -sfn "$J17" /tmp/optjdk/jdk17; ln -sfn "$J21" /tmp/optjdk/jdk21
ln -sfn "$J23" /tmp/optjdk/jdk23; ln -sfn "$J25" /tmp/optjdk/jdk25
SW=0
for V in 17 21 23 25; do
  ln -sfn /tmp/optjdk/jdk$V /tmp/optjdk/current   # the update-alternatives swap
  export JAVA_HOME=/tmp/optjdk/current
  H=$(readlink -f "$JAVA_HOME")
  RAW=$(rj "$H" -version 2>&1 | head -1)
  echo "  switch->$V : $RAW"
  echo "$RAW" | grep -q "\"$V" && SW=$((SW+1)) || echo "    MISMATCH expected $V"
done
[ "$SW" = 4 ] && { echo "SWITCH ok=4/4"; acc 1 SWITCH; } || { echo "SWITCH ok=$SW/4"; acc 0 SWITCH; }

echo "=== sdkman-style candidate switch (offline, pre-seeded) ==="
# emulate ~/.sdkman/candidates/java/<ver>-open + 'current' retarget (what `sdk use` does)
rm -rf /tmp/sdkhome; mkdir -p /tmp/sdkhome/candidates/java
for V in 17 21 23 25; do ln -sfn /tmp/optjdk/jdk$V /tmp/sdkhome/candidates/java/$V-open; done
SD=0
for V in 17 21 23 25; do
  ln -sfn /tmp/sdkhome/candidates/java/$V-open /tmp/sdkhome/candidates/java/current  # `sdk use java $V-open`
  CH=$(readlink -f /tmp/sdkhome/candidates/java/current)
  RAW=$(rj "$CH" -version 2>&1 | head -1)
  echo "  sdk-use $V : $RAW"
  echo "$RAW" | grep -q "\"$V" && SD=$((SD+1))
done
[ "$SD" = 4 ] && { echo "SDK-SWITCH ok=4/4"; acc 1 SDK-SWITCH; } || { echo "SDK-SWITCH ok=$SD/4"; acc 0 SDK-SWITCH; }

echo "AGGREGATE: PASS=$PASS TOTAL=$TOTAL"
if [ "$PASS" = "$TOTAL" ] && [ "$TOTAL" -gt 0 ]; then V=1; else V=0; fi
printf 'JDK_MULTI_OK=%s\n' "$V"
