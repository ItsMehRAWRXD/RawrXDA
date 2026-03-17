#!/usr/bin/env python3
"""ultra_fix_2000.py  —  Ultra-Optimised RawrXD Source Fixer

NO MIXING: YES
    - Standalone script
    - Uses its own output + archive locations (does not share with other scripts)

Strategy : CMake basename + path token cross-reference with concurrent I/O.
Mode     : Full apply by default — archives orphans, reports missing.
Identity : Does NOT borrow heuristics from instant_fix_2000.py and does NOT run
                     the 9-phase analysis from slow_fix_2000.py.
"""

import argparse
import os
import re
import json
import time
import shutil
import concurrent.futures
from pathlib import Path

# ── Config ────────────────────────────────────────────────────────────────────
DEFAULT_ROOT = Path(r"D:\RawrXD")

SRC_SUBDIR = "src"
CMAKE_FILE = "CMakeLists.txt"
ARCH_DIR   = ".archived_orphans_ultra"
OUT_NAME   = "ultra_audit.json"

SRC_EXTS = frozenset({".cpp", ".c", ".asm", ".h", ".hpp"})
CMAKE_RE  = re.compile(r'[\w/\\${}.-]+\.(?:cpp|c|asm|h|hpp)\b', re.IGNORECASE)


# ── Fast recursive scan using os.scandir ─────────────────────────────────────

def _scandir_files(top: Path) -> list[Path]:
    """Iterative scandir — avoids rglob overhead on large trees."""
    stack, out = [top], []
    while stack:
        with os.scandir(stack.pop()) as it:
            for e in it:
                if e.is_dir(follow_symlinks=False):
                    if e.name not in {".git", ARCH_DIR}:
                        stack.append(Path(e.path))
                elif Path(e.name).suffix.lower() in SRC_EXTS:
                    out.append(Path(e.path))
    return out


# ── CMake parse (single-pass, no line splitting) ──────────────────────────────

def cmake_tokens(cmake_path: Path) -> frozenset[str]:
    """Extract likely source tokens from a CMakeLists file."""
    text = cmake_path.read_text(encoding="utf-8", errors="replace")
    return frozenset(m.group().replace("\\", "/").lstrip("./") for m in CMAKE_RE.finditer(text))


# ── Classify ──────────────────────────────────────────────────────────────────

def classify(root: Path, disk: list[Path], cmake: frozenset[str]):
    by_name: dict[str, list[Path]] = {}
    by_rel: set[str] = set()
    for p in disk:
        by_name.setdefault(p.name, []).append(p)
        by_rel.add(p.relative_to(root).as_posix())

    active: list[Path] = []
    missing: list[str] = []

    for tok in cmake:
        base = Path(tok).name

        # Prefer a path match when token contains a path separator.
        if "/" in tok and tok in by_rel:
            active.append(root / tok)
            continue

        candidates = by_name.get(base)
        if not candidates:
            missing.append(tok)
        else:
            # If basename is ambiguous, count them all as active.
            active.extend(candidates)

    active_set = {p.resolve() for p in active}
    orphan = [p for p in disk if p.resolve() not in active_set]
    return active, orphan, sorted(set(missing))


# ── Concurrent archive ────────────────────────────────────────────────────────

def _move_one(root: Path, archive_root: Path, src: Path, dry_run: bool) -> bool:
    rel = src.relative_to(root).as_posix()
    dst = archive_root / rel
    if dst.exists():
        return False
    try:
        if not dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dst))
        return True
    except OSError:
        return False


def archive_orphans(root: Path, archive_root: Path, orphans: list[Path], dry_run: bool) -> int:
    if not dry_run:
        archive_root.mkdir(exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor() as pool:
        futs = [pool.submit(_move_one, root, archive_root, src, dry_run) for src in orphans]
        moved = 0
        for f in concurrent.futures.as_completed(futs):
            moved += 1 if f.result() else 0
        return moved


# ── Main ──────────────────────────────────────────────────────────────────────

def _count_loc_this_file() -> int:
    try:
        return len(Path(__file__).read_text(encoding="utf-8", errors="replace").splitlines())
    except OSError:
        return -1


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("project_root", nargs="?", default=str(DEFAULT_ROOT))
    ap.add_argument("--dry-run", action="store_true", help="Do not move/write anything")
    args = ap.parse_args()

    root = Path(args.project_root).resolve()
    src_dir = root / SRC_SUBDIR
    cmake_path = root / CMAKE_FILE
    archive_root = root / ARCH_DIR
    out_path = root / OUT_NAME

    t0 = time.perf_counter()

    cmake = cmake_tokens(cmake_path) if cmake_path.is_file() else frozenset()
    disk = _scandir_files(src_dir) if src_dir.is_dir() else []
    active, orphan, missing = classify(root, disk, cmake)
    moved = archive_orphans(root, archive_root, orphan, dry_run=args.dry_run)

    elapsed = round(time.perf_counter() - t0, 3)

    print("=" * 50)
    print("  ultra_fix_2000  |  NO MIXING: YES")
    print(f"  Mode    : {'DRY RUN' if args.dry_run else 'FULL APPLY'}")
    print(f"  Root    : {root}")
    print(f"  Active  : {len(active)}")
    print(f"  Orphan  : {len(orphan)}  (archived: {moved})")
    print(f"  Missing : {len(missing)}")
    print(f"  Time    : {elapsed}s")
    print("=" * 50)
    for f in missing[:10]:
        print(f"  MISSING: {f}")
    if len(missing) > 10:
        print(f"  ... +{len(missing) - 10} more")

    report = {
        "approach": "ultra",
        "no_mixing": "YES",
        "strategy": "cmake tokens (basename+path) cross-ref, concurrent archive",
        "project_root": str(root),
        "mode": "dry_run" if args.dry_run else "full_apply",
        "loc": _count_loc_this_file(),
        "elapsed_sec": elapsed,
        "active": len(active),
        "orphan": len(orphan),
        "archived": moved,
        "missing": missing,
        "archive_dir": str(archive_root),
    }
    if not args.dry_run:
        out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\n  Audit -> {out_path}")
    else:
        print("\n  (dry-run) no audit written")


if __name__ == "__main__":
    main()
