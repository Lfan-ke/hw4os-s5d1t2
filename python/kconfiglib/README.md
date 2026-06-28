# kconfiglib-0 — StarryOS stress test (#764 python kconfiglib)

`kconfiglib` is a single-file, fully-headless **pure-Python** Kconfig parser
(`kconfiglib.py` v14.1.0 — no `curses`/`tk` at import time). This case boots
StarryOS on the base CPython 3.12 rootfs with `kconfiglib.py` injected, then
drives an **invariant battery** (assert accumulator, exact-value asserts — NOT
exit-0) covering the full Kconfig semantic surface.

## What is tested

A synthetic `/tmp/Kconfig` is written exercising every symbol kind and relation
(plus a `MODULES` symbol with `option modules`, required for tristate symbols to
remain tristate — without it kconfiglib downgrades every tristate to bool). A
Python script then asserts:

| Group | Checks |
|-------|--------|
| Type introspection | `.type` of `bool` / `tristate` / `int` / `hex` / `string` symbols |
| Value resolution   | exact `.str_value` / `.tri_value` after explicit `set_value` (incl. tristate `m`) |
| `depends on`        | dependent is `y` when parent `y`; drops to `n` when parent `n` |
| `select` (strong)   | selector `y` forces target `y`; `rev_dep` evaluates to `y` |
| `imply` (weak)      | implier `y` raises implied target to `y`; `weak_rev_dep` is `y` |
| `choice`            | exactly one member selected; others `n` (mutual exclusion) |
| `range` on default  | a default below the range is clamped up to the range minimum |
| `range` + user value | a `set_value` is accepted verbatim, ignoring range (range only constrains defaults/menuconfig) |
| `default`           | declared defaults resolve to their exact values |
| `write_config` / `load_config` | roundtrip equality after re-read |

The script prints `PASS <name>` / `FAIL <name>` per check (26 checks total) and
emits the success token `^KCONFIGLIB_OK=1` **only if every check passes** (gated
on `all(...)`).

## Provenance

- Module source: `opensbi/scripts/Kconfiglib/kconfiglib.py` (`VERSION == (14, 1, 0)`).
- Base rootfs: `rootfs-<arch>-python.img` (Alpine musl CPython 3.12).
- Injection: `prep-kconfiglib-rootfs.sh <arch>` copies the base image to
  `rootfs-<arch>-kconfiglib.img` and writes `kconfiglib.py` to
  `/usr/lib/python3.12/site-packages/` via `debugfs -w` (no mount, WSL2-safe).

## Success criterion

`success_regex = ["(?m)^KCONFIGLIB_OK=1"]`, `timeout = 900`.
`fail_regex` trips on `panic` / any `^FAIL ` line / `^KCONFIGLIB_BAD` / `Traceback`.

## DoD (4-arch)

| Arch | qemu cpu / machine | mem | to_bin | rootfs | `^KCONFIGLIB_OK=1` |
|------|--------------------|-----|--------|--------|--------------------|
| x86_64      | (default)         | 2048M | false | rootfs-x86_64-kconfiglib.img      | [ ] |
| aarch64     | cortex-a72        | 2048M | true  | rootfs-aarch64-kconfiglib.img     | [ ] |
| riscv64     | rv64              | 2048M | true  | rootfs-riscv64-kconfiglib.img     | [ ] |
| loongarch64 | la464 / virt      | 4096M | true  | rootfs-loongarch64-kconfiglib.img | [ ] |

> The invariant battery was validated on host CPython 3.12 against the real
> `kconfiglib.py` v14.1.0 before being baked into the qemu tomls. Notes on the
> verified semantics (which corrected several naive first-draft assumptions):
> - A `tristate` symbol stays tristate only when a `MODULES`/`option modules`
>   symbol exists; otherwise its `.type` is `bool` and `m` collapses to `n`.
> - `range` clamps an out-of-range *default* up to the range minimum, but a user
>   `set_value` is accepted verbatim regardless of range.
> - The choice is looked up by membership (order-independent), not by index.
