#!/usr/bin/env python3
"""
Scan repository source files for quantization type identifiers and update
MULTIMODAL_ENGINE_FOLDER_AUDIT.md with a one-line summary.

Usage:
    python update_quant_types.py [--src-root <path>] [--dry-run]
"""

import argparse
import os
import re
from pathlib import Path

QUANT_MARKER_START = "<!-- QUANT_TYPES -->"
QUANT_MARKER_END = "<!-- END_QUANT_TYPES -->"

# Conservative regex to capture common quant names like Q2_K, Q4_K_M, Q5_K, IQ4_NL, F32, Q3_K_S, etc.
QUANT_RE = re.compile(r"\b(IQ\d+_NL|F32|Q\d+(?:_[A-Z0-9]+){0,2})\b")

DEFAULT_SRC_ROOT = Path(__file__).resolve().parents[1]  # src/multimodal_engine/.. -> src/
AUDIT_FILE = Path(__file__).resolve().parents[0] / ".." / "MULTIMODAL_ENGINE_FOLDER_AUDIT.md"

EXCLUDE_DIRS = {".git", "build", "bin", "obj", "venv", "third_party", "3rdparty"}


def find_quant_types(src_root: Path):
    types = set()
    for root, dirs, files in os.walk(src_root):
        # skip excluded dirs
        if any(part in EXCLUDE_DIRS for part in Path(root).parts):
            continue
        for fname in files:
            if not fname.lower().endswith(('.cpp', '.h', '.hpp', '.c', '.cc', '.asm', '.s', '.ps1', '.py', '.md')):
                continue
            fpath = Path(root) / fname
            try:
                text = fpath.read_text(errors='ignore')
            except Exception:
                continue
            for m in QUANT_RE.finditer(text):
                types.add(m.group(1))
    return sorted(types)


def build_one_liner(types):
    if not types:
        return "Supported quantization types: (none detected)"
    return "Supported quantization types: " + ", ".join(sorted(types))


def update_audit_file(audit_file: Path, one_liner: str, dry_run=False):
    content = audit_file.read_text()
    if QUANT_MARKER_START in content and QUANT_MARKER_END in content:
        before, rest = content.split(QUANT_MARKER_START, 1)
        _, after = rest.split(QUANT_MARKER_END, 1)
        new_section = f"{QUANT_MARKER_START}\n\n{one_liner}\n\n{QUANT_MARKER_END}"
        new_content = before + new_section + after
    else:
        # Insert a new section near the top under Summary
        insert_after = "### Summary"
        if insert_after in content:
            new_content = content.replace(insert_after, insert_after + "\n\n" + QUANT_MARKER_START + "\n\n" + one_liner + "\n\n" + QUANT_MARKER_END, 1)
        else:
            new_content = content + "\n\n" + QUANT_MARKER_START + "\n\n" + one_liner + "\n\n" + QUANT_MARKER_END

    if dry_run:
        print(new_content)
        return

    audit_file.write_text(new_content)
    print(f"Updated {audit_file} with: {one_liner}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--src-root', type=str, default=str(DEFAULT_SRC_ROOT), help='Root path of source to scan')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()

    src_root = Path(args.src_root)
    types = find_quant_types(src_root)
    one_liner = build_one_liner(types)
    update_audit_file(Path(__file__).resolve().parents[0] / '..' / 'MULTIMODAL_ENGINE_FOLDER_AUDIT.md', one_liner, dry_run=args.dry_run)
