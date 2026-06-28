#!/bin/bash
# prep-wildfly-rootfs.sh — build rootfs-<arch>-wildfly.img for the `wildfly-0`
# StarryOS stress case (WildFly 40.0.0.Final — full Jakarta EE 10 app server).
#
# BASE = rootfs-<arch>-java.img (already carries the 4-arch musl OpenJDK17 JRE;
# WildFly 40 supports JDK 17+). The case extracts the FULL official binary
# distribution to /opt/wildfly-40.0.0.Final and boots the REAL server via
# bin/standalone.sh (jboss-modules -> org.jboss.as.standalone). This is the
# heaviest java case in the set: a real app-server process, modular classloader,
# the Undertow web subsystem on :8080, and a multi-minute -Xint boot.
#
# bin/standalone.sh + bin/common.sh are POSIX `#!/bin/sh` (verified: no [[, no
# local/arrays/function-keyword) so they run under the base image's busybox ash;
# NO bash needed for WildFly (unlike sdkman). standalone.conf default sizing is
# -Xms64m -Xmx512m; the toml forces -Xint via JAVA_OPTS (#206 JIT instability).
#
# The toml harness: boot standalone.sh in the background -> wait for the
# WFLYSRV0025 "started" banner -> probe :8080 with busybox nc -> assert HTTP 200
# + the welcome-content body ("Welcome to WildFly" / "Your WildFly instance is
# running.") -> assert the 40.0.0.Final version banner -> then `kill $PID`.
#
# WHY `kill $PID` and NOT jboss-cli :shutdown (design decision):
# jboss-cli.sh spins up a SECOND full JVM (doubling the already-huge
# -Xint cost) and its management-port SSL/SASL handshake times out under the
# emulated single-core guest. A plain `kill` on the server PID is the cheap,
# reliable teardown.
#
# Usage:  bash prep-wildfly-rootfs.sh <arch>  # arch in x86_64|aarch64|riscv64|loongarch64
#
# WSL2 NOTE: uses `debugfs -w` on the UNMOUNTED image (no mount, no sync) -> no
# D-state deadlock; only touches the wildfly image it creates.
set -uo pipefail
ARCH="${1:-x86_64}"
# Portable: ROOT from env (your tgoskits checkout); distribution tarball co-located in package/.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TGOSKITS_ROOT:?set TGOSKITS_ROOT to your tgoskits checkout, e.g. export TGOSKITS_ROOT=\$HOME/tgoskits}"
TARBALL="$HERE/package/wildfly-40.0.0.Final.tar.gz"
BASE=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-java.img
IMG=$ROOT/tmp/axbuild/rootfs/rootfs-$ARCH-wildfly.img
STAGE=/tmp/wildfly-stage-$ARCH

[ -f "$BASE" ]    || { echo "missing base $BASE"; exit 2; }
[ -f "$TARBALL" ] || { echo "missing wildfly tarball $TARBALL"; exit 2; }
command -v debugfs >/dev/null 2>&1 || { echo "need debugfs (e2fsprogs)"; exit 2; }

# --- 1. copy base, grow to 5G (WildFly unpacked is ~500 MB; leave room for log) -
echo "=== [$ARCH] copy java base -> $IMG (resize 5G) ==="
cp -f "$BASE" "$IMG"
truncate -s 5G "$IMG"
e2fsck -f -y "$IMG" >/dev/null 2>&1
resize2fs "$IMG" >/dev/null 2>&1

# --- 2. stage: extract the distribution (top dir = wildfly-40.0.0.Final/) ------
echo "=== [$ARCH] stage WildFly -> $STAGE ==="
rm -rf "$STAGE"; mkdir -p "$STAGE"
tar xzf "$TARBALL" -C "$STAGE"
chmod +x "$STAGE/wildfly-40.0.0.Final/bin/"*.sh 2>/dev/null || true

# --- 3. inject the staged tree into /opt via debugfs -w (no mount, no sync) ----
# WildFly has ~thousands of module files; replay mkdir (depth-first) then write.
echo "=== [$ARCH] inject /opt/wildfly-40.0.0.Final into $IMG via debugfs -w ==="
DBG=/tmp/wildfly-debugfs-$ARCH.cmds
: > "$DBG"
echo "mkdir /opt" >> "$DBG"
( cd "$STAGE" && find . -type d | sort ) | while read -r d; do
  rel="${d#./}"; [ "$rel" = "." ] && continue
  echo "mkdir /opt/$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type f | sort ) | while read -r f; do
  rel="${f#./}"
  echo "rm /opt/$rel" >> "$DBG"
  echo "write $STAGE/$rel /opt/$rel" >> "$DBG"
done
( cd "$STAGE" && find . -type l | sort ) | while read -r l; do
  rel="${l#./}"; tgt=$(readlink "$STAGE/$rel")
  echo "rm /opt/$rel" >> "$DBG"
  echo "symlink /opt/$rel $tgt" >> "$DBG"
done
debugfs -w -f "$DBG" "$IMG" >/tmp/wildfly-debugfs-$ARCH.log 2>&1
grep -iE 'error|cannot|fail' /tmp/wildfly-debugfs-$ARCH.log | grep -viE 'File exists.*mkdir|File not found.*rm|rm:.*not found' | head

# --- 4. verify standalone.sh + welcome page landed ----------------------------
echo "=== [$ARCH] verify WildFly in image ==="
e2fsck -f -y "$IMG" >/dev/null 2>&1
debugfs -R "stat /opt/wildfly-40.0.0.Final/bin/standalone.sh" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /opt/wildfly-40.0.0.Final/jboss-modules.jar" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
debugfs -R "stat /opt/wildfly-40.0.0.Final/welcome-content/index.html" "$IMG" 2>/dev/null | grep -iE 'Inode|Size' | head -2
echo "[$ARCH] DONE -> $IMG"
