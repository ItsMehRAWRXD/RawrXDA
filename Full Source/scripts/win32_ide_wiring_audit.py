#!/usr/bin/env python3
# =============================================================================
# win32_ide_wiring_audit.py — Win32 IDE Self-Audit: Wire Manifest
# =============================================================================
# Purpose: Run from inside the repo to audit Win32 IDE wiring automatically.
#          Scans src/win32app (and related) for:
#          - ID definitions (#define IDM_* / IDC_*)
#          - Sources: menu items (AppendMenu*), controls (CreateWindowEx + HMENU id)
#          - Sinks: handlers (case IDM_*, routeCommandUnified, if (id == ...))
#          Produces a manifest of what is CONNECTED vs what is MISSING.
#
# Run: python scripts/win32_ide_wiring_audit.py [--src-root D:\rawrxd] [--out reports]
# Output: reports/win32_ide_wiring_manifest.json
#         reports/win32_ide_wiring_manifest.md
#
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

import re
import os
import sys
import json
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Set, Optional, Tuple
from collections import defaultdict

# -----------------------------------------------------------------------------
# CONFIG
# -----------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_SRC = REPO_ROOT / "src"
DEFAULT_WIN32 = DEFAULT_SRC / "win32app"
DEFAULT_INCLUDE = DEFAULT_SRC.parent / "include"
DEFAULT_OUT = REPO_ROOT / "reports"

# Patterns
RE_DEFINE_ID = re.compile(
    r"#\s*define\s+(IDM_[A-Za-z0-9_]+)\s+(\d+)",
    re.MULTILINE
)
RE_DEFINE_IDC = re.compile(
    r"#\s*define\s+(IDC_[A-Za-z0-9_]+)\s+(\d+)",
    re.MULTILINE
)
# AppendMenuA(hMenu, MF_STRING, IDM_XXX, "label") or AppendMenuA(..., 10200, ...)
RE_APPEND_MENU = re.compile(
    r"AppendMenuA?\s*\(\s*[^,]+,\s*[^,]+,\s*([^,)]+),\s*[^)]+\)",
    re.MULTILINE
)
# CreateWindowEx*(..., (HMENU)IDC_XXX, ...) or (HMENU)id
RE_CREATE_WINDOW_ID = re.compile(
    r"CreateWindowExA?\s*\([^)]*\(\s*HMENU\s*\)\s*([^)]+)\)",
    re.MULTILINE
)
RE_CREATE_WINDOW_ID_ALT = re.compile(
    r"\(HMENU\)\s*(IDC_[A-Za-z0-9_]+)\s*[,)]",
    re.MULTILINE
)
# case IDM_XXX: or case 50001: or case IDC_XXX:
RE_CASE_ID = re.compile(
    r"case\s+(IDM_[A-Za-z0-9_]+|IDC_[A-Za-z0-9_]+|\d+)\s*:",
    re.MULTILINE
)
# routeCommandUnified(IDM_XXX, this) or routeCommandUnified(id,
RE_ROUTE = re.compile(
    r"routeCommandUnified\s*\(\s*([^,)]+)\s*,",
    re.MULTILINE
)
# if (commandId == IDM_... or controlId == IDC_... or idFrom == IDC_...
RE_IF_ID = re.compile(
    r"(?:commandId|controlId|idFrom|id)\s*==\s*(IDM_[A-Za-z0-9_]+|IDC_[A-Za-z0-9_]+|\d+)",
    re.MULTILINE
)
# MAKEWPARAM(2001, 0) or MAKEWPARAM(id,
RE_MAKEWPARAM = re.compile(
    r"MAKEWPARAM\s*\(\s*(\d+)\s*,",
    re.MULTILINE
)
# TrackPopupMenu(..., TPM_RETURNCMD, ...) then if (cmd == IDC_...
RE_TRACKPOPUP_CMD = re.compile(
    r"(?:cmd|command)\s*==\s*(IDC_[A-Za-z0-9_]+|IDM_[A-Za-z0-9_]+|\d+)",
    re.MULTILINE
)

# Exclude from "missing" noise (ranges / internal)
SKIP_NAMES = {"IDM_FILE_RECENT_BASE", "IDM_FILE_RECENT_CLEAR", "IDM_THEME_END"}
SKIP_SUFFIXES = ("_BASE", "_END", "_FIRST", "_LAST", "_COUNT", "_MAX", "_MIN")


def _should_skip_name(name: str) -> bool:
    if name in SKIP_NAMES:
        return True
    for s in SKIP_SUFFIXES:
        if name.endswith(s):
            return True
    return False


# -----------------------------------------------------------------------------
# DATA
# -----------------------------------------------------------------------------

@dataclass
class IdDefine:
    name: str
    value: int
    file: str
    line: int


@dataclass
class SourceRef:
    """A place where an ID is 'emitted' (menu item, control creation)."""
    id_val: int
    id_name: Optional[str]
    file: str
    line: int
    kind: str  # "menu" | "control" | "post" | "trackpopup"


@dataclass
class SinkRef:
    """A place where an ID is 'handled' (case, routeCommandUnified, if)."""
    id_val: int
    id_name: Optional[str]
    file: str
    line: int
    kind: str  # "case" | "route" | "if" | "range"


# -----------------------------------------------------------------------------
# SCANNERS
# -----------------------------------------------------------------------------

def scan_defines(tree: Path, rel_prefix: Optional[Path]) -> Dict[str, IdDefine]:
    name_to_def: Dict[str, IdDefine] = {}
    for ext in ("*.cpp", "*.h", "*.hpp"):
        for path in tree.rglob(ext):
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            rel = path.relative_to(rel_prefix) if rel_prefix else path.name
            for m in RE_DEFINE_ID.finditer(text):
                name = m.group(1)
                if _should_skip_name(name):
                    continue
                try:
                    val = int(m.group(2))
                    name_to_def[name] = IdDefine(name, val, str(rel), text[:m.start()].count("\n") + 1)
                except ValueError:
                    pass
            for m in RE_DEFINE_IDC.finditer(text):
                name = m.group(1)
                if _should_skip_name(name):
                    continue
                try:
                    val = int(m.group(2))
                    name_to_def[name] = IdDefine(name, val, str(rel), text[:m.start()].count("\n") + 1)
                except ValueError:
                    pass
    return name_to_def


def resolve_to_value(token: str, name_to_def: Dict[str, IdDefine]) -> Optional[int]:
    token = token.strip()
    if re.match(r"^\d+$", token):
        return int(token)
    if token in name_to_def:
        return name_to_def[token].value
    return None


def collect_sources(tree: Path, name_to_def: Dict[str, IdDefine], rel_prefix: Path) -> List[SourceRef]:
    out: List[SourceRef] = []
    for ext in ("*.cpp", "*.h"):
        for path in tree.rglob(ext):
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            rel = str(path.relative_to(rel_prefix))
            for m in RE_APPEND_MENU.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SourceRef(val, name, rel, text[:m.start()].count("\n") + 1, "menu"))
            for m in RE_CREATE_WINDOW_ID.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SourceRef(val, name, rel, text[:m.start()].count("\n") + 1, "control"))
            for m in RE_CREATE_WINDOW_ID_ALT.finditer(text):
                tok = m.group(1).strip()
                if tok not in name_to_def:
                    continue
                out.append(SourceRef(name_to_def[tok].value, tok, rel, text[:m.start()].count("\n") + 1, "control"))
            for m in RE_MAKEWPARAM.finditer(text):
                val = int(m.group(1))
                out.append(SourceRef(val, None, rel, text[:m.start()].count("\n") + 1, "post"))
    return out


def collect_sinks(tree: Path, name_to_def: Dict[str, IdDefine], rel_prefix: Path) -> List[SinkRef]:
    out: List[SinkRef] = []
    for ext in ("*.cpp", "*.h"):
        for path in tree.rglob(ext):
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            rel = str(path.relative_to(rel_prefix))
            for m in RE_CASE_ID.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SinkRef(val, name, rel, text[:m.start()].count("\n") + 1, "case"))
            for m in RE_ROUTE.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SinkRef(val, name, rel, text[:m.start()].count("\n") + 1, "route"))
            for m in RE_IF_ID.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SinkRef(val, name, rel, text[:m.start()].count("\n") + 1, "if"))
            for m in RE_TRACKPOPUP_CMD.finditer(text):
                tok = m.group(1).strip()
                val = resolve_to_value(tok, name_to_def)
                if val is None:
                    continue
                name = tok if tok in name_to_def else None
                out.append(SinkRef(val, name, rel, text[:m.start()].count("\n") + 1, "if"))
    return out


def collect_creation_order(tree: Path, rel_prefix: Path) -> List[Dict]:
    """Find create*() calls and CreateWindowEx order in Core/Cpp to infer creation order."""
    order: List[Dict] = []
    create_pat = re.compile(r"\b(create[A-Za-z0-9_]*)\s*\(\s*(?:hwnd|hwndParent|m_hwndMain)?\s*\)", re.MULTILINE)
    for path in sorted(tree.rglob("*.cpp")):
        name = path.name
        if "Win32IDE_Core" not in name and "Win32IDE.cpp" not in name:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        rel = str(path.relative_to(rel_prefix))
        for m in create_pat.finditer(text):
            fn = m.group(1)
            order.append({"function": fn, "file": rel, "line": text[:m.start()].count("\n") + 1})
    return order


# -----------------------------------------------------------------------------
# CROSS-REFERENCE
# -----------------------------------------------------------------------------

def build_manifest(
    name_to_def: Dict[str, IdDefine],
    sources: List[SourceRef],
    sinks: List[SinkRef],
    creation_order: List[Dict],
) -> Dict:
    by_val_sources: Dict[int, List[SourceRef]] = defaultdict(list)
    by_val_sinks: Dict[int, List[SinkRef]] = defaultdict(list)
    val_to_name: Dict[int, str] = {}
    for d in name_to_def.values():
        val_to_name[d.value] = d.name
    for s in sources:
        by_val_sources[s.id_val].append(s)
    for s in sinks:
        by_val_sinks[s.id_val].append(s)

    connected: List[Dict] = []
    missing_handler: List[Dict] = []
    missing_source: List[Dict] = []
    for val, srcs in by_val_sources.items():
        name = val_to_name.get(val) or f"#{val}"
        entry = {"id": val, "name": name, "sources": len(srcs), "sinks": len(by_val_sinks[val])}
        if by_val_sinks[val]:
            connected.append(entry)
        else:
            missing_handler.append(entry)
    for val, snks in by_val_sinks.items():
        if val not in by_val_sources:
            name = val_to_name.get(val) or f"#{val}"
            missing_source.append({"id": val, "name": name, "sinks": len(snks)})

    return {
        "generated": __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat(),
        "summary": {
            "connected": len(connected),
            "missing_handler": len(missing_handler),
            "missing_source": len(missing_source),
            "total_defines": len(name_to_def),
            "total_sources": len(sources),
            "total_sinks": len(sinks),
        },
        "connected": connected,
        "missing_handler": missing_handler,
        "missing_source": missing_source,
        "creation_order": creation_order,
        "id_defines": [asdict(d) for d in list(name_to_def.values())[:500]],
    }


def write_md(manifest: Dict, path: Path) -> None:
    s = manifest["summary"]
    lines = [
        "# Win32 IDE Wiring Audit — Auto-Generated Manifest",
        "",
        "Generated by `scripts/win32_ide_wiring_audit.py`. Run from repo root.",
        "",
        "## Summary",
        "",
        "| Metric | Count |",
        "|--------|-------|",
        f"| **Connected** (has menu/control and handler) | {s['connected']} |",
        f"| **Missing handler** (menu/control exists, no handler) | {s['missing_handler']} |",
        f"| **Missing source** (handler exists, no menu/control) | {s['missing_source']} |",
        f"| Total ID defines scanned | {s['total_defines']} |",
        f"| Total source refs (menu/control) | {s['total_sources']} |",
        f"| Total sink refs (handlers) | {s['total_sinks']} |",
        "",
        "## Connected (wired properly)",
        "",
    ]
    for e in manifest["connected"][:200]:
        lines.append(f"- `{e['name']}` (id={e['id']}) — sources={e['sources']}, sinks={e['sinks']}")
    if len(manifest["connected"]) > 200:
        lines.append(f"- ... and {len(manifest['connected']) - 200} more")
    lines.extend([
        "",
        "## Missing handler (source exists, no sink)",
        "",
    ])
    for e in manifest["missing_handler"][:150]:
        lines.append(f"- `{e['name']}` (id={e['id']}) — has {e['sources']} source(s), 0 handlers")
    if len(manifest["missing_handler"]) > 150:
        lines.append(f"- ... and {len(manifest['missing_handler']) - 150} more")
    lines.extend([
        "",
        "## Missing source (sink exists, no menu/control)",
        "",
    ])
    for e in manifest["missing_source"][:100]:
        lines.append(f"- `{e['name']}` (id={e['id']}) — has {e['sinks']} handler(s), no source")
    if len(manifest["missing_source"]) > 100:
        lines.append(f"- ... and {len(manifest['missing_source']) - 100} more")
    lines.extend([
        "",
        "## Creation order (create* calls in Core/main)",
        "",
    ])
    for o in manifest["creation_order"][:40]:
        lines.append(f"- `{o['function']}` — {o['file']}:{o['line']}")
    path.write_text("\n".join(lines), encoding="utf-8")


# -----------------------------------------------------------------------------
# MAIN
# -----------------------------------------------------------------------------

def main() -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Win32 IDE wiring self-audit; output manifest of connected vs missing.")
    ap.add_argument("--src-root", type=Path, default=REPO_ROOT, help="Repo root (contains src/)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="Output directory for manifest files")
    args = ap.parse_args()
    src_root = args.src_root
    win32_dir = src_root / "src" / "win32app"
    out_dir = args.out
    if not win32_dir.is_dir():
        print(f"Not found: {win32_dir}", file=sys.stderr)
        return 1
    out_dir.mkdir(parents=True, exist_ok=True)
    rel_prefix = src_root / "src"

    print("Scanning ID defines...")
    name_to_def = scan_defines(win32_dir, rel_prefix)
    include_dir = src_root / "include"
    if include_dir.is_dir():
        for k, v in scan_defines(include_dir, src_root).items():
            if k not in name_to_def:
                name_to_def[k] = v
    print(f"  Found {len(name_to_def)} IDM_/IDC_ defines")

    print("Collecting sources (menus, controls)...")
    sources = collect_sources(win32_dir, name_to_def, rel_prefix)
    print(f"  Found {len(sources)} source refs")

    print("Collecting sinks (handlers)...")
    sinks = collect_sinks(win32_dir, name_to_def, rel_prefix)
    print(f"  Found {len(sinks)} sink refs")

    print("Collecting creation order...")
    creation_order = collect_creation_order(win32_dir, rel_prefix)
    print(f"  Found {len(creation_order)} create* calls")

    manifest = build_manifest(name_to_def, sources, sinks, creation_order)
    json_path = out_dir / "win32_ide_wiring_manifest.json"
    md_path = out_dir / "win32_ide_wiring_manifest.md"
    json_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    write_md(manifest, md_path)
    print(f"Wrote {json_path}")
    print(f"Wrote {md_path}")
    print(f"Summary: {manifest['summary']['connected']} connected, {manifest['summary']['missing_handler']} missing handler, {manifest['summary']['missing_source']} missing source")
    return 0


if __name__ == "__main__":
    sys.exit(main())
