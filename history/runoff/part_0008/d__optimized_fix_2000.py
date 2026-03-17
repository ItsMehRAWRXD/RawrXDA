#!/usr/bin/env python3
r"""
Optimized Fix for RawrXD (D:\RawrXD) — Maximum Performance
Uses: multiprocessing, mmap, compiled regex, hash-based dedup, lazy I/O.
"""

import os
import re
import json
import time
import shutil
import hashlib
import mmap
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from functools import lru_cache
from typing import Dict, Set, List, Tuple

# ─── Config ──────────────────────────────────────────────────────────────────
ROOT = Path(r"D:\RawrXD")
SRC  = ROOT / "src"
CMAKE = ROOT / "CMakeLists.txt"
ARCH = ROOT / ".archived_orphans"
OUT  = ROOT / "optimized_audit.json"
WORKERS = 8

# Pre-compiled regex (faster than re.compile at runtime)
CMAKE_RE = re.compile(rb'[\w/\\.-]+\.(?:cpp|c|asm|h|hpp)\b', re.IGNORECASE)
INCLUDE_RE = re.compile(rb'#\s*include\s*[<"]([^">]+)[">]')
FUNC_RE = re.compile(rb'^[ \t]*(?:static\s+)?(?:inline\s+)?(?:\w[\w*& ]*)\s+(\w+)\s*\(', re.MULTILINE)

# ─── Fast CMake Parsing (mmap) ───────────────────────────────────────────────
def parse_cmake_fast() -> Set[str]:
    """Memory-mapped CMake parsing — zero-copy."""
    refs = set()
    with open(CMAKE, 'rb') as f:
        with mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as mm:
            for m in CMAKE_RE.finditer(mm):
                refs.add(os.path.basename(m.group().decode('utf-8', 'replace')))
    return refs

# ─── Parallel File Scanner ───────────────────────────────────────────────────
def scan_sources() -> Dict[str, Path]:
    """Walk src/ once, build basename -> path map."""
    db = {}
    for ext in ('*.cpp', '*.c', '*.asm', '*.h', '*.hpp'):
        for p in SRC.rglob(ext):
            db[p.name] = p
    return db

# ─── Fast Hash (first 4KB + size) ────────────────────────────────────────────
@lru_cache(maxsize=8192)
def fast_hash(path: Path) -> str:
    """Quick hash using first 4KB + file size for dedup."""
    try:
        sz = path.stat().st_size
        with open(path, 'rb') as f:
            head = f.read(4096)
        return hashlib.md5(head + str(sz).encode()).hexdigest()
    except Exception:
        return ""

# ─── Parallel Include Graph ──────────────────────────────────────────────────
def extract_includes(path: Path) -> Tuple[str, Set[str]]:
    """Extract #include directives from a single file."""
    includes = set()
    try:
        with open(path, 'rb') as f:
            content = f.read(65536)  # First 64KB only
        for m in INCLUDE_RE.finditer(content):
            includes.add(os.path.basename(m.group(1).decode('utf-8', 'replace')))
    except Exception:
        pass
    return path.name, includes

def build_include_graph(files: List[Path]) -> Dict[str, Set[str]]:
    """Parallel include graph construction."""
    graph: Dict[str, Set[str]] = {}
    with ThreadPoolExecutor(max_workers=WORKERS) as ex:
        futures = {ex.submit(extract_includes, p): p for p in files}
        for fut in as_completed(futures):
            name, includes = fut.result()
            if includes:
                graph[name] = includes
    return graph

# ─── Parallel Symbol Extraction ──────────────────────────────────────────────
def extract_symbols(path: Path) -> Tuple[str, Set[str]]:
    """Extract function definitions from a single file."""
    defs = set()
    try:
        sz = path.stat().st_size
        if sz > 524288:  # Skip > 512KB
            return path.name, defs
        with open(path, 'rb') as f:
            content = f.read()
        for m in FUNC_RE.finditer(content):
            defs.add(m.group(1).decode('utf-8', 'replace'))
    except Exception:
        pass
    return path.name, defs

def build_symbol_map(files: List[Path]) -> Dict[str, Set[str]]:
    """Parallel symbol extraction."""
    sym_map: Dict[str, Set[str]] = {}
    with ThreadPoolExecutor(max_workers=WORKERS) as ex:
        futures = {ex.submit(extract_symbols, p): p for p in files}
        for fut in as_completed(futures):
            name, defs = fut.result()
            if defs:
                sym_map[name] = defs
    return sym_map

# ─── Reverse Index Builder ───────────────────────────────────────────────────
def build_includers_index(graph: Dict[str, Set[str]]) -> Dict[str, Set[str]]:
    """Who includes whom? (reverse index)"""
    rev: Dict[str, Set[str]] = {}
    for src, includes in graph.items():
        for inc in includes:
            rev.setdefault(inc, set()).add(src)
    return rev

# ─── Classification Engine ───────────────────────────────────────────────────
def classify(cmake_bases: Set[str], disk: Dict[str, Path],
             includers: Dict[str, Set[str]], symbols: Dict[str, Set[str]]) -> dict:
    """
    Classify files:
      ACTIVE     — in CMake
      NEEDED     — included by active OR defines symbols used elsewhere
      ORPHAN     — neither
      MISSING    — in CMake but not on disk
    """
    active = {k for k in disk if k in cmake_bases}
    
    # Transitive includers
    needed = set()
    frontier = active.copy()
    visited = set()
    while frontier:
        cur = frontier.pop()
        if cur in visited:
            continue
        visited.add(cur)
        for inc in includers.get(cur, ()):
            if inc in disk and inc not in active:
                needed.add(inc)
                frontier.add(inc)
    
    # Symbol cross-ref: if file defines symbols that active files might call
    active_symbols = set()
    for a in active:
        active_symbols.update(symbols.get(a, ()))
    for name, defs in symbols.items():
        if name in disk and name not in active and name not in needed:
            if defs & active_symbols:
                needed.add(name)
    
    orphan = {k for k in disk if k not in active and k not in needed}
    missing = cmake_bases - set(disk.keys())
    
    return {
        'active': sorted(active),
        'needed': sorted(needed),
        'orphan': sorted(orphan),
        'missing': sorted(missing)
    }

# ─── Duplicate Detector ──────────────────────────────────────────────────────
def find_duplicates(files: List[Path]) -> List[Tuple[str, str]]:
    """Hash-based duplicate detection."""
    by_hash: Dict[str, List[str]] = {}
    for p in files:
        h = fast_hash(p)
        if h:
            by_hash.setdefault(h, []).append(p.name)
    return [(v[0], v[1]) for v in by_hash.values() if len(v) > 1]

# ─── Orphan Archiver ─────────────────────────────────────────────────────────
def archive_orphans(orphan_names: List[str], disk: Dict[str, Path]) -> int:
    """Move orphans to archive folder."""
    ARCH.mkdir(exist_ok=True)
    moved = 0
    for name in orphan_names:
        src = disk.get(name)
        if src and src.exists():
            dst = ARCH / name
            if not dst.exists():
                shutil.move(str(src), str(dst))
                moved += 1
    return moved

# ─── Main ────────────────────────────────────────────────────────────────────
def main():
    t0 = time.perf_counter()
    
    print("=" * 60)
    print("  RawrXD Optimized Fix (Parallel + mmap + Hash)")
    print("=" * 60)
    
    # Phase 1: CMake parse (mmap)
    cmake_bases = parse_cmake_fast()
    print(f"[1] CMake refs: {len(cmake_bases)}")
    
    # Phase 2: Disk scan
    disk = scan_sources()
    print(f"[2] Disk files: {len(disk)}")
    
    # Phase 3: Parallel include graph
    all_files = list(disk.values())
    inc_graph = build_include_graph(all_files)
    includers = build_includers_index(inc_graph)
    print(f"[3] Include edges: {sum(len(v) for v in inc_graph.values())}")
    
    # Phase 4: Parallel symbol extraction
    symbols = build_symbol_map(all_files)
    print(f"[4] Files with symbols: {len(symbols)}")
    
    # Phase 5: Classification
    result = classify(cmake_bases, disk, includers, symbols)
    print(f"[5] Active: {len(result['active'])} | Needed: {len(result['needed'])} | Orphan: {len(result['orphan'])} | Missing: {len(result['missing'])}")
    
    # Phase 6: Duplicate detection
    dupes = find_duplicates(all_files)
    print(f"[6] Duplicate pairs: {len(dupes)}")
    
    # Phase 7: Archive orphans
    archived = archive_orphans(result['orphan'], disk)
    print(f"[7] Archived: {archived}")
    
    elapsed = round(time.perf_counter() - t0, 3)
    
    # Save report
    report = {
        'optimized': True,
        'workers': WORKERS,
        'elapsed_sec': elapsed,
        'cmake_refs': len(cmake_bases),
        'disk_files': len(disk),
        'include_edges': sum(len(v) for v in inc_graph.values()),
        'active': len(result['active']),
        'needed': len(result['needed']),
        'orphan': len(result['orphan']),
        'missing': result['missing'],
        'duplicate_pairs': len(dupes),
        'archived': archived
    }
    with open(OUT, 'w') as f:
        json.dump(report, f, indent=2)
    
    print("=" * 60)
    print(f"  DONE in {elapsed}s → {OUT.name}")
    print("=" * 60)

if __name__ == "__main__":
    main()
