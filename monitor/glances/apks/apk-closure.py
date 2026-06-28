#!/usr/bin/env python3
"""
apk-closure.py — resolve an Alpine apk transitive closure WITHOUT the `apk` tool,
download every .apk in the closure into an output dir.

Why: this host has no `apk`, so the prep scripts' `apk fetch -R` path is unavailable.
Alpine APKINDEX carries full dependency metadata (D: deps incl so:/cmd:/pc: tokens,
p: provides), so we can resolve the closure ourselves with deterministic BFS.

Usage:
  apk-closure.py --arch x86_64 --out DIR \
     --repo v3.23/main python3 py3-pip \
     --repo v3.23/community py3-numpy py3-scikit-learn py3-opencv

  Multiple --repo groups are allowed; each is "<branch>/<repo>" followed by its roots.
  All repos are loaded into one provides-index so cross-repo deps (community->main) resolve.

Mirrors: pass --mirror to override the default dl-cdn base.
Prints the resolved package list and downloads each missing .apk into --out.
Exit non-zero if any root or any hard dependency cannot be satisfied.
"""
import argparse
import os
import sys
import tarfile
import io
import urllib.request
import urllib.error

DEFAULT_MIRROR = "https://dl-cdn.alpinelinux.org/alpine"
# Fallback mirrors tried in order if the primary fails for a given URL.
FALLBACK_MIRRORS = [
    "https://dl-cdn.alpinelinux.org/alpine",
    "https://mirrors.ustc.edu.cn/alpine",
    "https://mirrors.tuna.tsinghua.edu.cn/alpine",
    "https://mirrors.aliyun.com/alpine",
]


def fetch(url, timeout=120, tries=3):
    last = None
    for _ in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "apk-closure/1.0"})
            with urllib.request.urlopen(req, timeout=timeout) as r:
                return r.read()
        except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as e:
            last = e
    raise last


def fetch_with_mirrors(path, mirrors, timeout=120):
    """path is like 'v3.23/main/x86_64/APKINDEX.tar.gz'; try each mirror."""
    last = None
    for m in mirrors:
        url = f"{m}/{path}"
        try:
            return fetch(url, timeout=timeout), url
        except Exception as e:  # noqa
            last = e
    raise last


def parse_apkindex(raw_index_bytes):
    """Parse the APKINDEX text into a list of package dicts.
    Each record: P,V,A and lists D (deps), p (provides)."""
    pkgs = []
    cur = {}
    for line in raw_index_bytes.decode("utf-8", "replace").splitlines():
        if line == "":
            if cur:
                pkgs.append(cur)
                cur = {}
            continue
        if len(line) < 2 or line[1] != ":":
            continue
        k, v = line[0], line[2:]
        if k in ("D", "p"):
            cur[k] = v.split()
        else:
            cur[k] = v
    if cur:
        pkgs.append(cur)
    return pkgs


def strip_dep_version(tok):
    """A dependency token may carry a version constraint: name>=1, name=1, name~1, name<1.
    Return the bare provide-name (so:..., cmd:..., pc:..., /path, or pkgname)."""
    for op in (">=", "<=", "><", "=", ">", "<", "~"):
        i = tok.find(op)
        if i > 0:
            return tok[:i]
    return tok


def build_indexes(repos, arch, mirrors):
    """repos: list of (branch_repo, [roots]). Returns:
    provides: dict provide-name -> pkgname (first wins)
    pkginfo:  dict pkgname -> {V, A, repo (branch/repo), D:[...]}
    """
    provides = {}
    pkginfo = {}
    for branch_repo, _roots in repos:
        path = f"{branch_repo}/{arch}/APKINDEX.tar.gz"
        raw, used = fetch_with_mirrors(path, mirrors)
        tf = tarfile.open(fileobj=io.BytesIO(raw), mode="r:gz")
        member = tf.extractfile("APKINDEX")
        idx_bytes = member.read()
        pkgs = parse_apkindex(idx_bytes)
        sys.stderr.write(f"[idx] {used}  ({len(pkgs)} pkgs)\n")
        for p in pkgs:
            name = p.get("P")
            if not name:
                continue
            ver = p.get("V", "")
            # later branches should not clobber earlier; first definition wins per name
            if name not in pkginfo:
                pkginfo[name] = {
                    "V": ver,
                    "A": p.get("A", arch),
                    "repo": branch_repo,
                    "D": p.get("D", []),
                }
            # the package provides its own name
            provides.setdefault(name, name)
            for prov in p.get("p", []):
                pname = strip_dep_version(prov)
                provides.setdefault(pname, name)
    return provides, pkginfo


def resolve(repos, arch, mirrors):
    provides, pkginfo = build_indexes(repos, arch, mirrors)
    selected = {}
    missing = []
    queue = []
    for _br, roots in repos:
        queue.extend(roots)
    seen_dep = set()
    # Resolve roots first (they are package names)
    while queue:
        tok = queue.pop(0)
        if tok in seen_dep:
            continue
        seen_dep.add(tok)
        if tok.startswith("!"):
            continue  # conflict / negation -> ignore
        bare = strip_dep_version(tok)
        pkgname = provides.get(bare)
        if pkgname is None:
            # try the raw token too (some deps are exact provide names)
            pkgname = provides.get(tok)
        if pkgname is None:
            missing.append(tok)
            continue
        if pkgname not in selected:
            info = pkginfo.get(pkgname)
            if info is None:
                missing.append(tok)
                continue
            selected[pkgname] = info
            for d in info["D"]:
                queue.append(d)
    return selected, missing


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mirror", default=None,
                    help="primary mirror base (default dl-cdn); fallbacks always appended")
    ap.add_argument("--print-only", action="store_true",
                    help="resolve + print list, do not download")
    # --repo BRANCH/REPO root1 root2 ... ; repeatable. argparse can't do nargs groups
    # cleanly, so we parse the remaining argv manually.
    args, rest = ap.parse_known_args()

    # rest looks like: --repo v3.23/main python3 py3-pip --repo v3.23/community ...
    repos = []
    i = 0
    cur_repo = None
    cur_roots = []
    while i < len(rest):
        t = rest[i]
        if t == "--repo":
            if cur_repo is not None:
                repos.append((cur_repo, cur_roots))
            cur_repo = rest[i + 1]
            cur_roots = []
            i += 2
            continue
        cur_roots.append(t)
        i += 1
    if cur_repo is not None:
        repos.append((cur_repo, cur_roots))
    if not repos:
        sys.stderr.write("no --repo groups given\n")
        return 2

    mirrors = []
    if args.mirror:
        mirrors.append(args.mirror.rstrip("/"))
    for m in FALLBACK_MIRRORS:
        if m not in mirrors:
            mirrors.append(m)

    selected, missing = resolve(repos, args.arch, mirrors)

    # Map each selected pkg to a download path. A package's repo is where its index
    # record came from; download from <branch/repo>/<arch>/<P>-<V>.apk
    sys.stderr.write(f"\n[closure] {len(selected)} packages for arch={args.arch}\n")
    for name in sorted(selected):
        info = selected[name]
        sys.stderr.write(f"  {name}-{info['V']}  [{info['repo']}]\n")
    if missing:
        # de-dup, drop pure so: that are actually base-image provided? No: report all.
        uniq = sorted(set(missing))
        sys.stderr.write(f"\n[WARN] {len(uniq)} unsatisfied dep tokens (may be base-image provided):\n")
        for m in uniq:
            sys.stderr.write(f"  ? {m}\n")

    if args.print_only:
        return 0

    os.makedirs(args.out, exist_ok=True)
    n_dl = 0
    n_cached = 0
    n_fail = 0
    for name in sorted(selected):
        info = selected[name]
        fn = f"{name}-{info['V']}.apk"
        dst = os.path.join(args.out, fn)
        if os.path.exists(dst) and os.path.getsize(dst) > 0:
            n_cached += 1
            continue
        path = f"{info['repo']}/{args.arch}/{fn}"
        try:
            data, used = fetch_with_mirrors(path, mirrors)
            with open(dst, "wb") as f:
                f.write(data)
            n_dl += 1
            sys.stderr.write(f"  + {fn} ({len(data)} B)\n")
        except Exception as e:  # noqa
            n_fail += 1
            sys.stderr.write(f"  ! {fn} download FAIL: {e}\n")
    sys.stderr.write(f"\n[done] downloaded={n_dl} cached={n_cached} fail={n_fail}\n")
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
