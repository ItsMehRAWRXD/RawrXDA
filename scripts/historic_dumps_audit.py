#!/usr/bin/env python3
"""
Historic source dump audit for the RawrXD repository.

Generates:
  - source_audit/historic_dumps_audit.json
  - source_audit/historic_dumps_audit.md
"""

from __future__ import annotations

import argparse
import hashlib
import json
import time
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


AUDIT_ROOTS = [
    "history",
    "history/runoff",
    "history/reconstructed",
    "history/all_versions",
    "runoff",
    "reconstructed",
    "Full Source/runoff",
]

PARITY_TREES = {
    "src": "src",
    "history_reconstructed_src": "history/reconstructed/src",
    "full_source_src": "Full Source/src",
}


@dataclass(frozen=True)
class FileInfo:
    repo_relative: str
    abs_path: Path
    size: int


def sha256_of_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_files(repo_root: Path, rel_dir: str) -> List[FileInfo]:
    base = repo_root / rel_dir
    if not base.exists():
        return []

    files: List[FileInfo] = []
    for file_path in base.rglob("*"):
        if not file_path.is_file():
            continue
        try:
            size = file_path.stat().st_size
        except OSError:
            continue
        files.append(
            FileInfo(
                repo_relative=file_path.relative_to(repo_root).as_posix(),
                abs_path=file_path,
                size=size,
            )
        )
    return files


def summarize_directory(repo_root: Path, rel_dir: str) -> Dict[str, object]:
    base = repo_root / rel_dir
    if not base.exists():
        return {"path": rel_dir, "exists": False}

    file_count = 0
    dir_count = 0
    total_bytes = 0
    ext_counter: Counter[str] = Counter()

    for current_root, dir_names, file_names in base.walk():
        dir_count += len(dir_names)
        file_count += len(file_names)
        for file_name in file_names:
            file_path = current_root / file_name
            try:
                size = file_path.stat().st_size
            except OSError:
                continue
            total_bytes += size
            ext = file_path.suffix.lower() or "<no_ext>"
            ext_counter[ext] += 1

    top_extensions = [
        {"extension": ext, "count": count}
        for ext, count in ext_counter.most_common(15)
    ]

    return {
        "path": rel_dir,
        "exists": True,
        "files": file_count,
        "directories": dir_count,
        "total_bytes": total_bytes,
        "total_mb": round(total_bytes / (1024 * 1024), 2),
        "top_extensions": top_extensions,
    }


def detect_exact_duplicates(files: Iterable[FileInfo]) -> Dict[str, object]:
    by_size: Dict[int, List[FileInfo]] = defaultdict(list)
    for info in files:
        by_size[info.size].append(info)

    hash_groups: Dict[str, List[FileInfo]] = defaultdict(list)
    for size, sized_files in by_size.items():
        if len(sized_files) < 2:
            continue
        for info in sized_files:
            try:
                digest = sha256_of_file(info.abs_path)
            except OSError:
                continue
            hash_groups[f"{size}:{digest}"].append(info)

    groups = [group for group in hash_groups.values() if len(group) >= 2]
    groups.sort(key=lambda g: (g[0].size * len(g), len(g)), reverse=True)

    duplicate_file_count = sum(len(group) for group in groups)
    duplicate_group_count = len(groups)
    duplicate_reclaimable_bytes = sum((len(group) - 1) * group[0].size for group in groups)

    top_groups = []
    for group in groups[:25]:
        top_groups.append(
            {
                "file_size": group[0].size,
                "instances": len(group),
                "reclaimable_bytes": (len(group) - 1) * group[0].size,
                "sample_paths": [entry.repo_relative for entry in group[:8]],
            }
        )

    return {
        "group_count": duplicate_group_count,
        "file_count": duplicate_file_count,
        "reclaimable_bytes": duplicate_reclaimable_bytes,
        "reclaimable_mb": round(duplicate_reclaimable_bytes / (1024 * 1024), 2),
        "top_groups": top_groups,
    }


def parity_for_tree(repo_root: Path, rel_dir: str) -> Dict[str, str]:
    base = repo_root / rel_dir
    if not base.exists():
        return {}
    mapping: Dict[str, str] = {}
    for file_path in base.rglob("*"):
        if not file_path.is_file():
            continue
        try:
            mapping[file_path.relative_to(base).as_posix()] = sha256_of_file(file_path)
        except OSError:
            continue
    return mapping


def compare_parity_trees(trees: Dict[str, Dict[str, str]]) -> Dict[str, object]:
    names = sorted(trees.keys())
    pairwise: Dict[str, Dict[str, int]] = {}
    for i, left in enumerate(names):
        for right in names[i + 1 :]:
            left_keys = set(trees[left].keys())
            right_keys = set(trees[right].keys())
            common = left_keys & right_keys
            same = sum(1 for rel in common if trees[left][rel] == trees[right][rel])
            pairwise[f"{left}__vs__{right}"] = {
                "common_paths": len(common),
                "same_content": same,
                "different_content": len(common) - same,
                f"only_{left}": len(left_keys - right_keys),
                f"only_{right}": len(right_keys - left_keys),
            }
    return pairwise


def find_cross_tree_missing_examples(trees: Dict[str, Dict[str, str]]) -> Dict[str, List[str]]:
    src = trees.get("src", {})
    full_source = trees.get("full_source_src", {})
    hist_recon = trees.get("history_reconstructed_src", {})

    src_paths = set(src.keys())
    full_paths = set(full_source.keys())
    hist_paths = set(hist_recon.keys())

    in_both_historic_not_src = sorted((full_paths & hist_paths) - src_paths)
    in_src_not_historic_pair = sorted(src_paths - (full_paths & hist_paths))

    return {
        "in_both_historic_not_src_sample": in_both_historic_not_src[:100],
        "in_src_not_historic_pair_sample": in_src_not_historic_pair[:100],
    }


def build_markdown_report(audit: Dict[str, object]) -> str:
    lines: List[str] = []
    lines.append("# Full Source Historic Dumps Audit")
    lines.append("")
    lines.append(f"Generated: {audit['generated_at']}")
    lines.append("")
    lines.append("## Historic Dump Directory Inventory")
    lines.append("")
    lines.append("| Path | Exists | Files | Directories | Size (MB) |")
    lines.append("|------|--------|-------|-------------|-----------|")
    for item in audit["root_inventory"]:
        if not item["exists"]:
            lines.append(f"| {item['path']} | no | 0 | 0 | 0.00 |")
            continue
        lines.append(
            f"| {item['path']} | yes | {item['files']} | {item['directories']} | {item['total_mb']:.2f} |"
        )

    duplicate = audit["exact_duplicates"]
    lines.append("")
    lines.append("## Exact Duplicate Content (Across Historic Roots)")
    lines.append("")
    lines.append(f"- Duplicate groups: **{duplicate['group_count']}**")
    lines.append(f"- Duplicate files participating: **{duplicate['file_count']}**")
    lines.append(f"- Reclaimable size if deduplicated: **{duplicate['reclaimable_mb']:.2f} MB**")
    lines.append("")
    lines.append("Top duplicate groups:")
    for idx, group in enumerate(duplicate["top_groups"][:12], start=1):
        lines.append(
            f"{idx}. size={group['file_size']} bytes, instances={group['instances']}, "
            f"reclaimable={round(group['reclaimable_bytes'] / (1024 * 1024), 2)} MB"
        )
        for sample in group["sample_paths"][:4]:
            lines.append(f"   - {sample}")

    lines.append("")
    lines.append("## Source Tree Parity (Path + Content)")
    lines.append("")
    lines.append("| Pair | Common Paths | Same Content | Different Content |")
    lines.append("|------|--------------|--------------|-------------------|")
    for pair, metrics in audit["parity_pairwise"].items():
        lines.append(
            f"| {pair} | {metrics['common_paths']} | {metrics['same_content']} | {metrics['different_content']} |"
        )

    examples = audit["cross_tree_missing_examples"]
    lines.append("")
    lines.append("## Cross-Tree Drift Samples")
    lines.append("")
    lines.append(
        "- Paths present in both historic trees but not in `src` "
        f"(sample size: {len(examples['in_both_historic_not_src_sample'])}):"
    )
    for path in examples["in_both_historic_not_src_sample"][:20]:
        lines.append(f"  - {path}")
    lines.append("")
    lines.append(
        "- Paths present in `src` but absent from historic intersection "
        f"(sample size: {len(examples['in_src_not_historic_pair_sample'])}):"
    )
    for path in examples["in_src_not_historic_pair_sample"][:20]:
        lines.append(f"  - {path}")

    lines.append("")
    lines.append("## Audit Notes")
    lines.append("")
    lines.append("- This audit is exact-hash based for duplicate detection and parity checks.")
    lines.append("- Samples are truncated for readability; full data is in the JSON report.")
    return "\n".join(lines) + "\n"


def run(repo_root: Path, output_dir: Path) -> Dict[str, object]:
    generated_at = time.strftime("%Y-%m-%d %H:%M:%S")

    root_inventory = [summarize_directory(repo_root, rel) for rel in AUDIT_ROOTS]

    all_historic_files: List[FileInfo] = []
    for rel in AUDIT_ROOTS:
        all_historic_files.extend(collect_files(repo_root, rel))

    # Deduplicate file entries by full path so nested roots do not double count.
    unique_by_path = {info.repo_relative: info for info in all_historic_files}
    unique_historic_files = list(unique_by_path.values())

    exact_duplicates = detect_exact_duplicates(unique_historic_files)

    parity_trees: Dict[str, Dict[str, str]] = {
        name: parity_for_tree(repo_root, rel_dir) for name, rel_dir in PARITY_TREES.items()
    }
    parity_pairwise = compare_parity_trees(parity_trees)
    cross_tree_missing_examples = find_cross_tree_missing_examples(parity_trees)

    audit = {
        "generated_at": generated_at,
        "repo_root": repo_root.as_posix(),
        "audit_roots": AUDIT_ROOTS,
        "root_inventory": root_inventory,
        "historic_unique_files_considered": len(unique_historic_files),
        "exact_duplicates": exact_duplicates,
        "parity_trees": {name: len(tree) for name, tree in parity_trees.items()},
        "parity_pairwise": parity_pairwise,
        "cross_tree_missing_examples": cross_tree_missing_examples,
    }

    output_dir.mkdir(parents=True, exist_ok=True)
    json_path = output_dir / "historic_dumps_audit.json"
    md_path = output_dir / "historic_dumps_audit.md"
    json_path.write_text(json.dumps(audit, indent=2), encoding="utf-8")
    md_path.write_text(build_markdown_report(audit), encoding="utf-8")

    return {
        "json_path": json_path.as_posix(),
        "md_path": md_path.as_posix(),
        "audit": audit,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit historic source dumps.")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path (default: script parent parent)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("source_audit"),
        help="Output directory (default: source_audit)",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    output_dir = args.output_dir
    if not output_dir.is_absolute():
        output_dir = (repo_root / output_dir).resolve()

    result = run(repo_root, output_dir)
    print(f"Historic dumps audit JSON: {result['json_path']}")
    print(f"Historic dumps audit Markdown: {result['md_path']}")


if __name__ == "__main__":
    main()
