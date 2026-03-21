#!/usr/bin/env python3
"""
Audit unresolved linker symbols for RawrXD-Win32IDE production build.

Outputs:
  - JSON: full machine-readable symbol classification
  - Markdown: concise human report
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List


UNDEF_RE = re.compile(r"objects\.a\(([^)]+)\).*undefined reference to `([^`]+)'")
MORE_UNDEF_RE = re.compile(r"more undefined references to `([^`]+)' follow")
OBJ_RULE_RE = re.compile(
    r"^CMakeFiles/RawrXD-Win32IDE\.dir/(.+?\.obj): (/workspace/.+\.(?:c|cc|cpp|cxx|h|hpp|asm|rc))$"
)

SEARCH_GLOBS = [
    "*.c",
    "*.cc",
    "*.cpp",
    "*.cxx",
    "*.h",
    "*.hpp",
    "*.asm",
    "*.inc",
]

STUB_TOKENS = ("stub", "stubs", "fallback", "shim", "mock", "fake", "placeholder")


@dataclass
class RefEntry:
    line: int
    obj: str
    symbol: str


def parse_unresolved(log_path: Path) -> List[RefEntry]:
    refs: List[RefEntry] = []
    for i, raw in enumerate(log_path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
        m = UNDEF_RE.search(raw)
        if m:
            refs.append(RefEntry(line=i, obj=m.group(1), symbol=m.group(2)))
            continue
        m2 = MORE_UNDEF_RE.search(raw)
        if m2:
            refs.append(RefEntry(line=i, obj="(grouped)", symbol=m2.group(1)))
    return refs


def parse_build_sources(build_make: Path) -> Dict[str, List[str]]:
    """
    Returns mapping:
      obj basename -> list of absolute source paths mapped to that object basename.
    """
    by_obj: Dict[str, List[str]] = defaultdict(list)
    txt = build_make.read_text(encoding="utf-8", errors="ignore")
    for line in txt.splitlines():
        m = OBJ_RULE_RE.match(line)
        if not m:
            continue
        obj_rel = m.group(1)
        src_abs = m.group(2)
        by_obj[Path(obj_rel).name].append(src_abs)
    return by_obj


def parse_non_msvc_removed(cmake_path: Path) -> List[str]:
    removed: List[str] = []
    lines = cmake_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    in_not_msvc = False
    in_remove = False

    for raw in lines:
        line = raw.strip()
        if line.startswith("if(NOT MSVC)"):
            in_not_msvc = True
            continue
        if in_not_msvc and line.startswith("endif()"):
            in_not_msvc = False
            in_remove = False
            continue
        if not in_not_msvc:
            continue
        if line.startswith("list(REMOVE_ITEM WIN32IDE_SOURCES"):
            in_remove = True
            continue
        if in_remove and line.startswith(")"):
            in_remove = False
            continue
        if in_remove:
            cand = line.split("#", 1)[0].strip()
            if cand:
                removed.append(cand)
    return removed


def symbol_token(symbol: str) -> str:
    base = symbol.split("(", 1)[0]
    return base.strip()


def rg_hits(token: str, repo_root: Path) -> List[dict]:
    cmd = ["rg", "--line-number", "--no-heading", "--fixed-strings", token, str(repo_root / "src"), str(repo_root / "include"), str(repo_root / "Ship")]
    for g in SEARCH_GLOBS:
        cmd.extend(["--glob", g])
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode not in (0, 1):
        return []
    hits: List[dict] = []
    for row in p.stdout.splitlines():
        # path:line:text (split only first 2 separators)
        parts = row.split(":", 2)
        if len(parts) != 3:
            continue
        path, line_no, text = parts
        hits.append({"path": path, "line": int(line_no), "text": text})
    return hits


def looks_like_definition(token: str, hit: dict) -> bool:
    text = hit["text"]
    stripped = text.strip()
    if not stripped or stripped.startswith("//") or stripped.startswith("*"):
        return False
    p = Path(hit["path"])
    is_source = p.suffix.lower() in {".c", ".cc", ".cpp", ".cxx", ".asm"}

    # Namespace function style
    if "::" in token and "(" not in token:
        if f"{token}(" in text and ";" not in stripped:
            return True

    # C/C++ function definitions
    m_fn = re.search(rf"(^|[^A-Za-z0-9_]){re.escape(token)}\s*\(", text)
    if m_fn and "extern " not in stripped:
        if is_source:
            return True
        # Header inline/template definitions
        if "inline " in stripped or "constexpr " in stripped:
            return True

    # Variable definitions
    m_var = re.search(rf"(^|[^A-Za-z0-9_]){re.escape(token)}([^A-Za-z0-9_]|$)", text)
    if m_var and "=" in text and "extern " not in stripped:
        return True

    return False


def stublike_path(path: str) -> bool:
    lower = path.lower()
    return any(tok in lower for tok in STUB_TOKENS)


def classify_symbol(
    symbol: str,
    refs_for_symbol: List[RefEntry],
    repo_root: Path,
    build_obj_to_src: Dict[str, List[str]],
    non_msvc_removed: List[str],
) -> dict:
    token = symbol_token(symbol)
    hits = rg_hits(token, repo_root)
    defs = [h for h in hits if looks_like_definition(token, h)]

    wired_defs = []
    unwired_defs = []
    for d in defs:
        dpath = d["path"]
        # "Wired" in this concrete build = source appears in build.make object rules.
        is_wired = False
        for srcs in build_obj_to_src.values():
            if dpath in srcs:
                is_wired = True
                break
        row = {
            "path": dpath,
            "line": d["line"],
            "text": d["text"].strip(),
            "stublike_path": stublike_path(dpath),
            "removed_by_non_msvc_block": dpath.replace(str(repo_root) + "/", "") in non_msvc_removed,
        }
        if is_wired:
            wired_defs.append(row)
        else:
            unwired_defs.append(row)

    if not defs:
        classification = "dissolved_or_external"
        reason = "No definition candidate found in src/include/Ship."
    elif wired_defs:
        classification = "unlinked_despite_wired"
        reason = "Definition candidates exist in current target sources; unresolved likely ABI/lib closure mismatch."
    else:
        unwired_nonstub = [d for d in unwired_defs if not d["stublike_path"]]
        if unwired_nonstub:
            classification = "unwired_non_stub"
            reason = "Definition candidates exist but are not wired into current Win32IDE build."
        else:
            classification = "stub_or_fallback_only_unwired"
            reason = "Only stub/fallback-like definition candidates found, and they are not wired."

    ref_objs = Counter(r.obj for r in refs_for_symbol)
    top_ref_objs = [{"obj": k, "count": v, "mapped_sources": build_obj_to_src.get(k, [])} for k, v in ref_objs.most_common(5)]

    return {
        "symbol": symbol,
        "token": token,
        "occurrences": len(refs_for_symbol),
        "classification": classification,
        "reason": reason,
        "top_referencing_objects": top_ref_objs,
        "definition_candidates_wired": wired_defs[:10],
        "definition_candidates_unwired": unwired_defs[:10],
        "all_definition_candidates_count": len(defs),
        "all_search_hits_count": len(hits),
        "reference_lines": [r.line for r in refs_for_symbol[:20]],
    }


def render_markdown(result: dict) -> str:
    lines: List[str] = []
    lines.append("# Win32IDE Unlinked + Dissolved Symbol Audit")
    lines.append("")
    lines.append(f"- Log: `{result['log_path']}`")
    lines.append(f"- Total unresolved references: **{result['total_reference_lines']}**")
    lines.append(f"- Unique unresolved symbols: **{result['unique_symbols']}**")
    lines.append("")
    lines.append("## Classification Summary")
    lines.append("")
    lines.append("| Classification | Count |")
    lines.append("|---|---:|")
    for k, v in sorted(result["classification_counts"].items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"| `{k}` | {v} |")
    lines.append("")

    lines.append("## Unwired Non-Stub Symbols")
    lines.append("")
    uw = [s for s in result["symbols"] if s["classification"] == "unwired_non_stub"]
    if not uw:
        lines.append("_None found._")
    else:
        lines.append("| Symbol | Occurrences | Top ref object | Example unwired definition |")
        lines.append("|---|---:|---|---|")
        for s in sorted(uw, key=lambda x: (-x["occurrences"], x["symbol"])):
            top_obj = s["top_referencing_objects"][0]["obj"] if s["top_referencing_objects"] else "-"
            example = s["definition_candidates_unwired"][0]["path"] if s["definition_candidates_unwired"] else "-"
            lines.append(f"| `{s['symbol']}` | {s['occurrences']} | `{top_obj}` | `{example}` |")
    lines.append("")

    lines.append("## Dissolved or External Symbols")
    lines.append("")
    de = [s for s in result["symbols"] if s["classification"] == "dissolved_or_external"]
    if not de:
        lines.append("_None found._")
    else:
        lines.append("| Symbol | Occurrences | Top ref object |")
        lines.append("|---|---:|---|")
        for s in sorted(de, key=lambda x: (-x["occurrences"], x["symbol"])):
            top_obj = s["top_referencing_objects"][0]["obj"] if s["top_referencing_objects"] else "-"
            lines.append(f"| `{s['symbol']}` | {s['occurrences']} | `{top_obj}` |")
    lines.append("")

    lines.append("## Top 40 Unresolved Symbols")
    lines.append("")
    lines.append("| Symbol | Class | Occurrences | Top ref object |")
    lines.append("|---|---|---:|---|")
    top = sorted(result["symbols"], key=lambda x: (-x["occurrences"], x["symbol"]))[:40]
    for s in top:
        top_obj = s["top_referencing_objects"][0]["obj"] if s["top_referencing_objects"] else "-"
        lines.append(f"| `{s['symbol']}` | `{s['classification']}` | {s['occurrences']} | `{top_obj}` |")
    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo-root", default="/workspace")
    ap.add_argument("--log-path", default="/workspace/source_audit/win32ide_production_link_latest.log")
    ap.add_argument("--output-dir", default="/workspace/source_audit")
    args = ap.parse_args()

    repo_root = Path(args.repo_root).resolve()
    log_path = Path(args.log_path).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    build_make = repo_root / "build_completed_win" / "CMakeFiles" / "RawrXD-Win32IDE.dir" / "build.make"
    cmake_path = repo_root / "CMakeLists.txt"

    if not log_path.exists():
        raise SystemExit(f"Log not found: {log_path}")
    if not build_make.exists():
        raise SystemExit(f"Build make not found: {build_make}")

    refs = parse_unresolved(log_path)
    by_symbol: Dict[str, List[RefEntry]] = defaultdict(list)
    for r in refs:
        by_symbol[r.symbol].append(r)

    build_obj_to_src = parse_build_sources(build_make)
    non_msvc_removed = parse_non_msvc_removed(cmake_path)

    symbols_out = []
    for sym in sorted(by_symbol):
        symbols_out.append(
            classify_symbol(
                symbol=sym,
                refs_for_symbol=by_symbol[sym],
                repo_root=repo_root,
                build_obj_to_src=build_obj_to_src,
                non_msvc_removed=non_msvc_removed,
            )
        )

    class_counts = Counter(s["classification"] for s in symbols_out)
    result = {
        "repo_root": str(repo_root),
        "log_path": str(log_path),
        "build_make": str(build_make),
        "total_reference_lines": len(refs),
        "unique_symbols": len(symbols_out),
        "classification_counts": dict(class_counts),
        "non_msvc_removed_sources_count": len(non_msvc_removed),
        "non_msvc_removed_sources": non_msvc_removed,
        "symbols": symbols_out,
    }

    json_path = output_dir / "unlinked_symbols_production_audit.json"
    md_path = output_dir / "unlinked_symbols_production_audit.md"
    json_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    md_path.write_text(render_markdown(result), encoding="utf-8")

    print(f"Wrote: {json_path}")
    print(f"Wrote: {md_path}")
    print(f"Unique unresolved symbols: {len(symbols_out)}")
    print(f"Classification counts: {dict(class_counts)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
