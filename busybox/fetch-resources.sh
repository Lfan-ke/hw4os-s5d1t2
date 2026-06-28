#!/bin/bash
# fetch-resources.sh — re-fetch any redistributable resources that were removed when
# this delivery repo was slimmed down.
#
# For busybox: NOTHING was removed and there is NOTHING to fetch. This test category
# has no prep-*.sh and ships no per-app package files. The busybox applets exercised
# by applets/case/sh/busybox-tests.sh are provided by the busybox already present in
# the base Alpine rootfs image (rootfs-<arch>-alpine.img, Alpine v3.23.4) under the
# maintainer's tgoskits checkout — that base image is a separate build artifact, not
# a file that lives in (or was slimmed out of) this repo.
#
# For reference only (NOT required to run the busybox tests): the busybox package the
# base image is built from is, per SOURCES.md, Alpine v3.23/main:
#   https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/busybox-1.37.0-r30.apk
#   https://dl-cdn.alpinelinux.org/alpine/v3.23/main/<arch>/busybox-binsh-1.37.0-r30.apk
#
# Requires no network. After this, run the tests via the usual StarryOS harness using
# the base Alpine image (no prep step needed for busybox).
set -euo pipefail

echo "fetch-resources: busybox has no removed resources to fetch"
echo "  (applets are provided by the base Alpine rootfs image; see SOURCES.md)"
echo "fetch-resources: busybox OK"
