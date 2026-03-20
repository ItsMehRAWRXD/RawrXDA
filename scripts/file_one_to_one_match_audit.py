#!/usr/bin/env python3
"""
One-to-one file matching audit across source trees.

Goals:
1) Match exact file pairs (agentchat -> agentchat via exact path/name).
2) Detect renamed/misnamed correspondences (different filename/path).
3) Produce machine-readable full mapping outputs.

Outputs:
  - source_audit/file_one_to_one_match_audit.json
  - source_audit/file_one_to_one_match_audit.md
  - source_audit/file_match_maps/<pair_name>.csv
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import time
from dataclasses import dataclass
from difflib import SequenceMatcher
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


PAIR_CONFIGS: List[Tuple[str, str]] = [
    ("src", "history/reconstructed/src"),
    ("src", "Full Source/src"),
    ("history/reconstructed/src", "Full Source/src"),
]


@dataclass(frozen=True)
class FileRec:
    rel: str
    abs_path: Path
    ext: str
    basename: str
    basename_lower: str
    stem_norm: str
    tokens: Tuple[str, ...]
    parent_tokens: Tuple[str, ...]
    sha256: str


def sha256_of_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tokenize_name(name: str) -> List[str]:
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name)
    parts = re.split(r"[^A-Za-z0-9]+", name.lower())
    return [p for p in parts if p]


def normalize_stem(stem: str) -> str:
    return "".join(ch for ch in stem.lower() if ch.isalnum())


def collect_records(base_dir: Path) -> List[FileRec]:
    records: List[FileRec] = []
    if not base_dir.exists():
        return records

    for root, _, files in os.walk(base_dir):
        root_path = Path(root)
        for filename in files:
            file_path = root_path / filename
            rel = file_path.relative_to(base_dir).as_posix()
            ext = file_path.suffix.lower()
            basename = file_path.name
            stem = file_path.stem
            tokens = tuple(tokenize_name(stem))
            parent_tokens = tuple(tokenize_name(file_path.parent.as_posix()))
            records.append(
                FileRec(
                    rel=rel,
                    abs_path=file_path,
                    ext=ext,
                    basename=basename,
                    basename_lower=basename.lower(),
                    stem_norm=normalize_stem(stem),
                    tokens=tokens,
                    parent_tokens=parent_tokens,
                    sha256=sha256_of_file(file_path),
                )
            )
    return records


def jaccard(a: Iterable[str], b: Iterable[str]) -> float:
    sa = set(a)
    sb = set(b)
    if not sa and not sb:
        return 1.0
    if not sa or not sb:
        return 0.0
    return len(sa & sb) / len(sa | sb)


def safe_ratio(a: str, b: str) -> float:
    if not a and not b:
        return 1.0
    return SequenceMatcher(None, a, b).ratio()


def score_name_similarity(left: FileRec, right: FileRec) -> float:
    stem_ratio = safe_ratio(left.stem_norm, right.stem_norm)
    token_score = jaccard(left.tokens, right.tokens)
    parent_score = jaccard(left.parent_tokens, right.parent_tokens)
    return (0.55 * stem_ratio) + (0.30 * token_score) + (0.15 * parent_score)


def build_index(records: List[FileRec], attr: str) -> Dict[str, List[int]]:
    index: Dict[str, List[int]] = {}
    for idx, rec in enumerate(records):
        key = getattr(rec, attr)
        index.setdefault(key, []).append(idx)
    return index


def make_pair_name(left: str, right: str) -> str:
    return f"{left}__vs__{right}".replace("/", "_").replace(" ", "_")


def match_pair(left_records: List[FileRec], right_records: List[FileRec]) -> Dict[str, object]:
    right_by_rel = {rec.rel: idx for idx, rec in enumerate(right_records)}
    right_by_hash = build_index(right_records, "sha256")
    right_by_basename = build_index(right_records, "basename_lower")
    right_by_stem_norm = build_index(right_records, "stem_norm")
    token_to_right: Dict[str, List[int]] = {}
    for idx, rec in enumerate(right_records):
        for token in rec.tokens:
            token_to_right.setdefault(token, []).append(idx)

    unmatched_left: Set[int] = set(range(len(left_records)))
    unmatched_right: Set[int] = set(range(len(right_records)))
    matches: List[Dict[str, object]] = []

    def apply_match(
        li: int,
        ri: int,
        match_type: str,
        score: Optional[float] = None,
    ) -> None:
        if li not in unmatched_left or ri not in unmatched_right:
            return
        lrec = left_records[li]
        rrec = right_records[ri]
        unmatched_left.remove(li)
        unmatched_right.remove(ri)
        matches.append(
            {
                "left": lrec.rel,
                "right": rrec.rel,
                "match_type": match_type,
                "score": round(score, 4) if score is not None else None,
                "same_content": lrec.sha256 == rrec.sha256,
                "name_mismatch": lrec.basename_lower != rrec.basename_lower,
            }
        )

    # Pass 1: exact path
    for li in list(unmatched_left):
        lrec = left_records[li]
        ri = right_by_rel.get(lrec.rel)
        if ri is not None and ri in unmatched_right:
            apply_match(li, ri, "exact_path")

    # Pass 2: exact content renamed/relocated
    for li in list(unmatched_left):
        lrec = left_records[li]
        candidates = [ri for ri in right_by_hash.get(lrec.sha256, []) if ri in unmatched_right]
        if not candidates:
            continue
        if len(candidates) == 1:
            apply_match(li, candidates[0], "exact_content")
            continue
        # Prefer same basename among hash-identical candidates.
        same_base = [ri for ri in candidates if right_records[ri].basename_lower == lrec.basename_lower]
        if len(same_base) == 1:
            apply_match(li, same_base[0], "exact_content")
            continue
        # Fallback: best relative-path similarity.
        best_ri = max(candidates, key=lambda ri: safe_ratio(lrec.rel, right_records[ri].rel))
        apply_match(li, best_ri, "exact_content")

    # Pass 3: unique basename match
    for li in list(unmatched_left):
        lrec = left_records[li]
        candidates = [ri for ri in right_by_basename.get(lrec.basename_lower, []) if ri in unmatched_right]
        if len(candidates) == 1:
            apply_match(li, candidates[0], "basename")

    # Pass 4: unique normalized-stem match (same extension preferred)
    for li in list(unmatched_left):
        lrec = left_records[li]
        candidates = [
            ri
            for ri in right_by_stem_norm.get(lrec.stem_norm, [])
            if ri in unmatched_right and right_records[ri].ext == lrec.ext
        ]
        if len(candidates) == 1:
            apply_match(li, candidates[0], "normalized_stem")

    # Pass 5: fuzzy renamed matches (greedy one-to-one).
    proposals: List[Tuple[float, int, int]] = []
    for li in unmatched_left:
        lrec = left_records[li]
        candidate_set: Set[int] = set()
        for token in lrec.tokens:
            for ri in token_to_right.get(token, []):
                if ri in unmatched_right:
                    candidate_set.add(ri)
        if not candidate_set:
            continue
        for ri in candidate_set:
            rrec = right_records[ri]
            if rrec.ext != lrec.ext:
                continue
            score = score_name_similarity(lrec, rrec)
            if score >= 0.72:
                proposals.append((score, li, ri))

    proposals.sort(reverse=True)
    for score, li, ri in proposals:
        if li in unmatched_left and ri in unmatched_right:
            apply_match(li, ri, "fuzzy_name", score=score)

    unmatched_left_rows = [{"left": left_records[li].rel} for li in sorted(unmatched_left)]
    unmatched_right_rows = [{"right": right_records[ri].rel} for ri in sorted(unmatched_right)]

    counts: Dict[str, int] = {}
    for row in matches:
        counts[row["match_type"]] = counts.get(row["match_type"], 0) + 1

    renamed = [m for m in matches if m["name_mismatch"]]
    content_drift_on_exact_path = [
        m for m in matches if m["match_type"] == "exact_path" and not m["same_content"]
    ]

    summary = {
        "left_total": len(left_records),
        "right_total": len(right_records),
        "matched_total": len(matches),
        "unmatched_left_total": len(unmatched_left_rows),
        "unmatched_right_total": len(unmatched_right_rows),
        "match_type_counts": counts,
        "renamed_or_differently_named_total": len(renamed),
        "content_drift_on_exact_path_total": len(content_drift_on_exact_path),
    }

    return {
        "summary": summary,
        "matches": matches,
        "renamed_samples": renamed[:200],
        "content_drift_on_exact_path_samples": content_drift_on_exact_path[:200],
        "unmatched_left": unmatched_left_rows,
        "unmatched_right": unmatched_right_rows,
    }


def write_pair_csv(path: Path, matches: List[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "left",
                "right",
                "match_type",
                "score",
                "same_content",
                "name_mismatch",
            ],
        )
        writer.writeheader()
        for row in matches:
            writer.writerow(row)


def build_markdown(report: Dict[str, object]) -> str:
    lines: List[str] = []
    lines.append("# One-to-One File Match Audit")
    lines.append("")
    lines.append(f"Generated: {report['generated_at']}")
    lines.append("")
    lines.append("## Pair Summary")
    lines.append("")
    lines.append("| Pair | Left Total | Right Total | Matched | Unmatched Left | Unmatched Right | Renamed/Mismatched Names |")
    lines.append("|------|------------|-------------|---------|----------------|-----------------|--------------------------|")
    for pair_name, pair_data in report["pairs"].items():
        summary = pair_data["summary"]
        lines.append(
            f"| {pair_name} | {summary['left_total']} | {summary['right_total']} | "
            f"{summary['matched_total']} | {summary['unmatched_left_total']} | "
            f"{summary['unmatched_right_total']} | {summary['renamed_or_differently_named_total']} |"
        )

    for pair_name, pair_data in report["pairs"].items():
        summary = pair_data["summary"]
        lines.append("")
        lines.append(f"## {pair_name}")
        lines.append("")
        lines.append("Match types:")
        for match_type, count in sorted(summary["match_type_counts"].items()):
            lines.append(f"- {match_type}: {count}")
        lines.append(
            f"- content drift on exact-path matches: {summary['content_drift_on_exact_path_total']}"
        )
        lines.append("")
        lines.append("Renamed / differently named samples:")
        renamed = pair_data["renamed_samples"]
        if not renamed:
            lines.append("- None")
        else:
            for row in renamed[:25]:
                lines.append(
                    f"- `{row['left']}` -> `{row['right']}` "
                    f"(type={row['match_type']}, same_content={row['same_content']})"
                )

    lines.append("")
    lines.append("## Artifacts")
    lines.append("")
    lines.append("- Full machine-readable mapping is in `file_one_to_one_match_audit.json`.")
    lines.append("- Per-pair CSV maps are in `source_audit/file_match_maps/`.")
    return "\n".join(lines) + "\n"


def run(repo_root: Path, out_dir: Path) -> Dict[str, object]:
    pair_results: Dict[str, object] = {}
    csv_paths: List[str] = []

    for left_rel, right_rel in PAIR_CONFIGS:
        left_dir = repo_root / left_rel
        right_dir = repo_root / right_rel
        if not left_dir.exists() or not right_dir.exists():
            continue
        left_records = collect_records(left_dir)
        right_records = collect_records(right_dir)
        pair_name = make_pair_name(left_rel, right_rel)
        pair_result = match_pair(left_records, right_records)
        pair_results[pair_name] = pair_result
        csv_path = out_dir / "file_match_maps" / f"{pair_name}.csv"
        write_pair_csv(csv_path, pair_result["matches"])
        csv_paths.append(csv_path.as_posix())

    report = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repo_root": repo_root.as_posix(),
        "pair_configs": PAIR_CONFIGS,
        "pairs": pair_results,
        "csv_maps": csv_paths,
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "file_one_to_one_match_audit.json"
    md_path = out_dir / "file_one_to_one_match_audit.md"
    json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    md_path.write_text(build_markdown(report), encoding="utf-8")
    return {
        "json_path": json_path.as_posix(),
        "md_path": md_path.as_posix(),
        "report": report,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="1:1 file matching audit.")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("source_audit"),
        help="Output directory.",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    out_dir = args.output_dir
    if not out_dir.is_absolute():
        out_dir = (repo_root / out_dir).resolve()

    result = run(repo_root, out_dir)
    print(f"1:1 match audit JSON: {result['json_path']}")
    print(f"1:1 match audit Markdown: {result['md_path']}")


if __name__ == "__main__":
    main()
