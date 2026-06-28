#!/bin/bash
# fetch-resources.sh — re-fetch the re-downloadable resources removed during slimming.
# Needs network access only if anything must be re-fetched.
#
# For java/complex-demo: NOTHING was removed during slimming. The diff between the full
# and slim trees is empty. This is a self-written, zero-external-dependency Java 17
# program; everything it needs is retained in-tree:
#   src/main/java/demo/{App,Shapes}.java   the program source
#   pom.xml, build.gradle, settings.gradle the offline maven/gradle build files
#   complex-demo.jar                       our pre-built product (host javac; also
#                                          rebuildable offline on starry — see README.md)
#
# Hence there is nothing to download here. complex-demo.jar is a build product (not a
# third-party download) and is kept in-tree. It can be rebuilt with zero network using
# the maven/gradle restored by ../toolchain/fetch-resources.sh, or directly:
#   javac --release 17 -d out src/main/java/demo/*.java
set -euo pipefail

echo "  [info] java/complex-demo: no resources were removed during slimming; nothing to fetch."
echo "fetch-resources: complex-demo OK"
