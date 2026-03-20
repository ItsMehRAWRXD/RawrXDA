#!/usr/bin/env python3
"""
Audit completed implementation claims against current source/build reality.

Outputs:
  - source_audit/completed_claims_build_audit.json
  - source_audit/completed_claims_build_audit.md
"""

from __future__ import annotations

import argparse
import json
import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple


CLAIM_DOCS = [
    "docs/FULL_AUDIT_MASTER.md",
    "UNFINISHED_FEATURES.md",
]

ROOT_CMAKE = "CMakeLists.txt"
EXCLUDED_CMAKE_PATH_PARTS = (
    "history/",
    "reconstructed/",
    "runoff/",
    "Full Source/",
    ".archived_orphans/",
)

NOT_IMPLEMENTED_MARKERS = [
    "not implemented",
    "placeholder",
    "todo",
    "stub mode",
]


@dataclass(frozen=True)
class Claim:
    source_doc: str
    line: int
    claim_type: str  # file|symbol
    token: str
    context: str


def read_lines(path: Path) -> List[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def extract_code_spans(text: str) -> List[str]:
    return re.findall(r"`([^`]+)`", text)


def extract_claims_from_doc(path: Path, repo_root: Path) -> List[Claim]:
    claims: List[Claim] = []
    lines = read_lines(path)
    rel_doc = path.relative_to(repo_root).as_posix()

    for idx, line in enumerate(lines, start=1):
        lowered = line.lower()
        completion_signal = (
            "added to win32ide_sources" in lowered
            or "done:" in lowered
            or "✅" in line
            or "| yes | yes |" in lowered
            or "completed this pass" in lowered
        )
        if not completion_signal:
            continue

        spans = extract_code_spans(line)
        for span in spans:
            token = span.strip()
            if not token:
                continue
            if "/" in token or token.endswith((".cpp", ".h", ".hpp", ".asm", ".c", ".ps1", ".md")):
                claims.append(
                    Claim(
                        source_doc=rel_doc,
                        line=idx,
                        claim_type="file",
                        token=token,
                        context=line.strip(),
                    )
                )
            elif "::" in token or token.endswith("()"):
                claims.append(
                    Claim(
                        source_doc=rel_doc,
                        line=idx,
                        claim_type="symbol",
                        token=token,
                        context=line.strip(),
                    )
                )

        # Table rows can include raw paths without backticks.
        if "| yes | yes |" in lowered:
            for raw_path in re.findall(r"((?:src|Ship|include)/[A-Za-z0-9_./-]+)", line):
                claims.append(
                    Claim(
                        source_doc=rel_doc,
                        line=idx,
                        claim_type="file",
                        token=raw_path.strip().rstrip(".,;"),
                        context=line.strip(),
                    )
                )

    # Explicitly parse the FULL_AUDIT_MASTER list of files claimed added to WIN32IDE_SOURCES.
    if rel_doc == "docs/FULL_AUDIT_MASTER.md":
        for idx, line in enumerate(lines, start=1):
            if "Added to WIN32IDE_SOURCES" not in line:
                continue
            for span in extract_code_spans(line):
                token = span.strip()
                if token.endswith(".cpp"):
                    claims.append(
                        Claim(
                            source_doc=rel_doc,
                            line=idx,
                            claim_type="file",
                            token=token,
                            context=line.strip(),
                        )
                    )

    # De-duplicate claim entries from overlapping extraction rules.
    unique: Dict[Tuple[str, int, str, str], Claim] = {}
    for claim in claims:
        key = (claim.source_doc, claim.line, claim.claim_type, claim.token)
        unique[key] = claim
    return list(unique.values())


def collect_repo_files(repo_root: Path) -> Tuple[Set[str], Dict[str, List[str]]]:
    all_rel_paths: Set[str] = set()
    by_basename: Dict[str, List[str]] = {}
    for path in repo_root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(repo_root).as_posix()
        all_rel_paths.add(rel)
        by_basename.setdefault(path.name, []).append(rel)
    return all_rel_paths, by_basename


def parse_win32ide_and_asm_sources(cmake_text: str) -> Tuple[Set[str], Set[str]]:
    win32_sources: Set[str] = set()
    asm_sources: Set[str] = set()

    lines = cmake_text.splitlines()
    current_mode = None  # WIN32|ASM|None
    paren_depth = 0

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("set(WIN32IDE_SOURCES") or stripped.startswith("list(APPEND WIN32IDE_SOURCES"):
            current_mode = "WIN32"
            paren_depth = stripped.count("(") - stripped.count(")")
        elif stripped.startswith("set(ASM_KERNEL_SOURCES") or stripped.startswith("list(APPEND ASM_KERNEL_SOURCES"):
            current_mode = "ASM"
            paren_depth = stripped.count("(") - stripped.count(")")
        elif current_mode:
            paren_depth += stripped.count("(") - stripped.count(")")

        if current_mode:
            for match in re.findall(r"(src/[^\s\)#]+|Ship/[^\s\)#]+|include/[^\s\)#]+)", line):
                normalized = match.strip().rstrip(")")
                if current_mode == "WIN32":
                    win32_sources.add(normalized)
                elif current_mode == "ASM":
                    asm_sources.add(normalized)
            if paren_depth <= 0:
                current_mode = None
                paren_depth = 0

    return win32_sources, asm_sources


def collect_active_cmake_files(repo_root: Path) -> List[Path]:
    files: List[Path] = []
    for path in repo_root.rglob("CMakeLists.txt"):
        rel = path.relative_to(repo_root).as_posix()
        if any(part in rel for part in EXCLUDED_CMAKE_PATH_PARTS):
            continue
        files.append(path)
    return files


def resolve_file_token(token: str, all_rel_paths: Set[str], by_basename: Dict[str, List[str]]) -> List[str]:
    token = token.replace("\\", "/").strip()
    if token in all_rel_paths:
        return [token]
    basename = token.split("/")[-1]
    paths = by_basename.get(basename, [])
    # Prefer active tree paths over reconstructed/history/archive copies.
    def sort_key(rel: str) -> Tuple[int, str]:
        if rel.startswith("src/"):
            return (0, rel)
        if rel.startswith("Ship/"):
            return (1, rel)
        if rel.startswith("include/"):
            return (2, rel)
        if rel.startswith("docs/"):
            return (3, rel)
        return (9, rel)
    return sorted(paths, key=sort_key)


def symbol_exists(repo_root: Path, symbol_token: str) -> bool:
    probe = symbol_token.replace("()", "").strip()
    if "::" in probe:
        short_probe = probe.split("::")[-1]
    else:
        short_probe = probe

    pattern = re.compile(re.escape(short_probe))
    for path in (repo_root / "src").rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".cpp", ".h", ".hpp", ".c", ".asm"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if pattern.search(text):
            return True
    return False


def file_marker_hits(path: Path) -> Dict[str, int]:
    text = path.read_text(encoding="utf-8", errors="replace").lower()
    hits: Dict[str, int] = {}
    for marker in NOT_IMPLEMENTED_MARKERS:
        count = text.count(marker)
        if count:
            hits[marker] = count
    return hits


def run(repo_root: Path, out_dir: Path) -> Dict[str, object]:
    all_rel_paths, by_basename = collect_repo_files(repo_root)
    cmake_path = repo_root / ROOT_CMAKE
    cmake_text = cmake_path.read_text(encoding="utf-8", errors="replace")
    cmake_lines = cmake_text.splitlines()
    win32_sources, asm_sources = parse_win32ide_and_asm_sources(cmake_text)
    active_cmake_files = collect_active_cmake_files(repo_root)
    active_cmake_texts: Dict[str, List[str]] = {
        path.relative_to(repo_root).as_posix(): read_lines(path) for path in active_cmake_files
    }
    all_active_cmake_text = "\n".join("\n".join(lines) for lines in active_cmake_texts.values())

    claims: List[Claim] = []
    for rel_doc in CLAIM_DOCS:
        doc_path = repo_root / rel_doc
        if doc_path.exists():
            claims.extend(extract_claims_from_doc(doc_path, repo_root))

    file_claim_rows: List[Dict[str, object]] = []
    symbol_claim_rows: List[Dict[str, object]] = []
    mismatches: Dict[str, List[Dict[str, object]]] = {
        "claimed_file_missing": [],
        "claimed_file_missing_but_referenced_by_win32ide_sources": [],
        "claimed_file_not_in_expected_build_graph": [],
        "claimed_symbol_missing": [],
        "claimed_completed_with_not_implemented_markers": [],
    }

    for claim in claims:
        if claim.claim_type == "file":
            resolved = resolve_file_token(claim.token, all_rel_paths, by_basename)
            token_basename = Path(claim.token.replace("\\", "/")).name
            in_win32 = False
            in_any_cmake = False
            marker_summary: Dict[str, int] = {}
            cmake_line_hits: List[int] = []
            cmake_file_hits: List[str] = []
            expected_scope = "win32ide" if "win32ide_sources" in claim.context.lower() else "any"

            # Evaluate claim token itself even when the file is missing.
            if token_basename in all_active_cmake_text:
                in_any_cmake = True
                cmake_line_hits = [
                    idx
                    for idx, line in enumerate(cmake_lines, start=1)
                    if token_basename in line
                ][:20]
                for cmake_rel, lines in active_cmake_texts.items():
                    if any(token_basename in line for line in lines):
                        cmake_file_hits.append(cmake_rel)
                cmake_file_hits = cmake_file_hits[:20]
            if any(
                entry == token_basename
                or entry.endswith("/" + token_basename)
                or entry.replace("\\", "/").endswith("/" + token_basename)
                for entry in win32_sources
            ):
                in_win32 = True

            for rel in resolved:
                basename = Path(rel).name
                if rel in win32_sources or any(entry.endswith("/" + basename) or entry == basename for entry in win32_sources):
                    in_win32 = True
                if basename in cmake_text:
                    in_any_cmake = True
            # Marker analysis uses the best-match path only to avoid archive noise.
            if resolved:
                primary = repo_root / resolved[0]
                if primary.suffix.lower() in {".cpp", ".h", ".hpp", ".c"}:
                    marker_summary = file_marker_hits(primary)

            row = {
                "source_doc": claim.source_doc,
                "line": claim.line,
                "token": claim.token,
                "resolved_count": len(resolved),
                "resolved_paths_sample": resolved[:10],
                "exists": len(resolved) > 0,
                "expected_scope": expected_scope,
                "in_win32ide_sources": in_win32,
                "in_any_cmake": in_any_cmake,
                "cmake_line_hits": cmake_line_hits,
                "cmake_file_hits": cmake_file_hits,
                "not_implemented_markers": marker_summary,
                "context": claim.context,
            }
            file_claim_rows.append(row)

            if not row["exists"]:
                mismatches["claimed_file_missing"].append(row)
            if (not row["exists"]) and row["in_win32ide_sources"]:
                mismatches["claimed_file_missing_but_referenced_by_win32ide_sources"].append(row)
            token_ext = Path(claim.token).suffix.lower()
            if row["exists"] and token_ext in {".cpp", ".c", ".asm"}:
                if row["expected_scope"] == "win32ide" and not row["in_win32ide_sources"]:
                    mismatches["claimed_file_not_in_expected_build_graph"].append(row)
                if row["expected_scope"] == "any" and not row["in_any_cmake"]:
                    mismatches["claimed_file_not_in_expected_build_graph"].append(row)
            if row["exists"] and marker_summary.get("not implemented", 0) > 0:
                mismatches["claimed_completed_with_not_implemented_markers"].append(row)

        else:
            exists = symbol_exists(repo_root, claim.token)
            row = {
                "source_doc": claim.source_doc,
                "line": claim.line,
                "token": claim.token,
                "exists_in_src": exists,
                "context": claim.context,
            }
            symbol_claim_rows.append(row)
            if not exists:
                mismatches["claimed_symbol_missing"].append(row)

    # De-duplicate mismatch rows by source location + token.
    for key in mismatches:
        deduped: Dict[Tuple[str, int, str], Dict[str, object]] = {}
        for row in mismatches[key]:
            deduped[(str(row.get("source_doc")), int(row.get("line", 0)), str(row.get("token")))] = row
        mismatches[key] = list(deduped.values())

    report = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repo_root": repo_root.as_posix(),
        "docs_scanned": CLAIM_DOCS,
        "win32ide_source_count": len(win32_sources),
        "asm_kernel_source_count": len(asm_sources),
        "active_cmake_files_scanned": len(active_cmake_files),
        "claims_total": len(claims),
        "claims_file_total": len(file_claim_rows),
        "claims_symbol_total": len(symbol_claim_rows),
        "mismatch_counts": {key: len(values) for key, values in mismatches.items()},
        "mismatches": mismatches,
        "file_claims": file_claim_rows,
        "symbol_claims": symbol_claim_rows,
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "completed_claims_build_audit.json"
    md_path = out_dir / "completed_claims_build_audit.md"
    json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    md_path.write_text(build_markdown(report), encoding="utf-8")

    return {
        "json_path": json_path.as_posix(),
        "md_path": md_path.as_posix(),
        "report": report,
    }


def build_markdown(report: Dict[str, object]) -> str:
    lines: List[str] = []
    lines.append("# Completed Claims vs Current Build Audit")
    lines.append("")
    lines.append(f"Generated: {report['generated_at']}")
    lines.append("")
    lines.append("## Scope")
    lines.append("")
    for doc in report["docs_scanned"]:
        lines.append(f"- {doc}")
    lines.append("")
    lines.append(f"- WIN32IDE_SOURCES entries parsed: **{report['win32ide_source_count']}**")
    lines.append(f"- ASM_KERNEL_SOURCES entries parsed: **{report['asm_kernel_source_count']}**")
    lines.append(f"- Active CMakeLists scanned: **{report['active_cmake_files_scanned']}**")
    lines.append(f"- Claims scanned: **{report['claims_total']}**")
    lines.append("")
    lines.append("## Mismatch Summary")
    lines.append("")
    lines.append("| Mismatch Type | Count |")
    lines.append("|---------------|-------|")
    for key, value in report["mismatch_counts"].items():
        lines.append(f"| {key} | {value} |")

    mismatches = report["mismatches"]

    def emit_rows(title: str, rows: Sequence[Dict[str, object]], max_rows: int = 40) -> None:
        lines.append("")
        lines.append(f"## {title}")
        lines.append("")
        if not rows:
            lines.append("- None")
            return
        for row in rows[:max_rows]:
            token = row.get("token", "<unknown>")
            source = f"{row.get('source_doc')}:{row.get('line')}"
            lines.append(f"- **{token}** ({source})")
            if row.get("resolved_paths_sample"):
                for p in row["resolved_paths_sample"][:4]:
                    lines.append(f"  - path: `{p}`")
            if row.get("context"):
                lines.append(f"  - claim: {row['context']}")
            if row.get("cmake_line_hits"):
                lines.append(f"  - CMake lines: {row['cmake_line_hits']}")
            if row.get("cmake_file_hits"):
                lines.append(f"  - CMake files: {row['cmake_file_hits']}")
            marker_hits = row.get("not_implemented_markers")
            if marker_hits:
                lines.append(f"  - markers: {marker_hits}")
        if len(rows) > max_rows:
            lines.append(f"- ... and {len(rows) - max_rows} more")

    emit_rows("Claimed file missing", mismatches["claimed_file_missing"])
    emit_rows(
        "Claimed file missing but still referenced by WIN32IDE_SOURCES",
        mismatches["claimed_file_missing_but_referenced_by_win32ide_sources"],
    )
    emit_rows(
        "Claimed complete but not in expected build graph",
        mismatches["claimed_file_not_in_expected_build_graph"],
    )
    emit_rows("Claimed symbol missing", mismatches["claimed_symbol_missing"])
    emit_rows(
        "Claimed complete but file still contains 'not implemented'",
        mismatches["claimed_completed_with_not_implemented_markers"],
    )

    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- This audit compares current repository state, not historical build artifacts.")
    lines.append("- Inclusion checks are against current root `CMakeLists.txt` WIN32IDE source graph.")
    lines.append("- Marker hits are textual signals and should be manually triaged before code changes.")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit completed claims vs build inclusion.")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("source_audit"),
        help="Output directory",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    output_dir = args.output_dir
    if not output_dir.is_absolute():
        output_dir = (repo_root / output_dir).resolve()

    result = run(repo_root, output_dir)
    print(f"Completed claims audit JSON: {result['json_path']}")
    print(f"Completed claims audit Markdown: {result['md_path']}")


if __name__ == "__main__":
    main()
