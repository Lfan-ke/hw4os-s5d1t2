#!/usr/bin/env python3
# Multi-snapshot pyte render: feed the raw serial stream incrementally and dump the
# screen at several byte checkpoints, so we see the TUI at different moments (default
# view, after F5 tree, after arrows, after toggle-back) and can spot post-operation
# display corruption.
import sys, pyte

path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/htop-cap2.raw"
COLS, ROWS = 120, 40
raw = open(path, "rb").read()
n = len(raw)
# checkpoints across the stream (skip the boot-log prefix region by starting at 35%)
checks = [int(n*f) for f in (0.45, 0.62, 0.78, 0.92, 1.0)]

screen = pyte.Screen(COLS, ROWS)
stream = pyte.ByteStream(screen)
prev = 0
for ci, c in enumerate(checks):
    stream.feed(raw[prev:c]); prev = c
    print("\n========== SNAPSHOT %d  (@%d%% of stream) ==========" % (ci+1, int(c*100/n)))
    nonempty = 0
    for line in screen.display:
        t = line.rstrip()
        if t: nonempty += 1
        print("|" + t + "|")
    print("---- nonempty=%d/%d ----" % (nonempty, ROWS))
