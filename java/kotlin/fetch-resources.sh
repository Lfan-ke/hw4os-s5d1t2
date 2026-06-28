#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resources removed during slimming.
# Needs network access only if anything must be re-fetched.
#
# For java/kotlin: NOTHING was removed during slimming. The diff between the full and
# slim trees is empty. Every artifact this case needs is retained in-tree:
#   KotlinCarpet.kt          language-carpet source (82 checks)
#   KotlinCarpet.jar         our self-built, self-contained host artifact
#                            (kotlinc 2.0.21 `-include-runtime`, arch-independent;
#                            staged to rootfs and run on starry's JDK17 — see README.md)
#   case/build-*.toml, case/qemu-*.toml   the 4-arch case configs
#
# Hence there is nothing to download here. (KotlinCarpet.jar is a build product, not a
# third-party download, so it is kept rather than re-fetched.) If a from-source rebuild
# of the carpet jar is ever desired, the only external dependency is the Kotlin 2.0.21
# compiler, which `../toolchain/fetch-resources.sh` restores
# (kotlin/kotlin-compiler-2.0.21.zip); then host-compile:
#   kotlinc -include-runtime KotlinCarpet.kt -d KotlinCarpet.jar
set -euo pipefail

echo "  [info] java/kotlin: no resources were removed during slimming; nothing to fetch."
echo "fetch-resources: kotlin OK"
