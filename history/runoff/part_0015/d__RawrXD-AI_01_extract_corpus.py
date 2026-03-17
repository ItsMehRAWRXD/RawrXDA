#!/usr/bin/env python3
"""
RawrXD Corpus Extractor - Phase 1
==================================
Crawls the D: drive and extracts ALL text/code content into a structured JSONL
training corpus. Handles deduplication, size filtering, and metadata tagging.

Output: One JSONL file where each line is:
{
    "text": "<file contents>",
    "source": "<file path>",
    "language": "<detected language>",
    "category": "<code|doc|config|build|log|script>",
    "size_bytes": 12345,
    "lines": 200
}
"""

import os
import sys
import json
import hashlib
import time
import mmap
import re
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import defaultdict

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "pipeline_config.json")

with open(CONFIG_PATH, "r", encoding="utf-8") as f:
    CONFIG = json.load(f)

CORPUS_CFG = CONFIG["corpus"]
SOURCE_ROOT = CORPUS_CFG["source_root"]
OUTPUT_DIR = CORPUS_CFG["output_dir"]
RAW_JSONL = CORPUS_CFG["raw_jsonl"]
SCAN_DIRS = CORPUS_CFG["scan_directories"]
INCLUDE_EXT = set(CORPUS_CFG["include_extensions"])
EXCLUDE_PATTERNS = CORPUS_CFG["exclude_patterns"]
MAX_FILE_MB = CORPUS_CFG["max_file_size_mb"]
MIN_FILE_BYTES = CORPUS_CFG["min_file_size_bytes"]

# Also scan top-level files on D: (markdown docs, scripts, etc.)
SCAN_TOP_LEVEL = True

# ---------------------------------------------------------------------------
# Language & Category Detection
# ---------------------------------------------------------------------------

EXT_TO_LANG = {
    ".cpp": "cpp", ".hpp": "cpp", ".h": "c_cpp", ".c": "c", ".cc": "cpp", ".cxx": "cpp",
    ".asm": "x86asm", ".inc": "x86asm", ".masm": "x86asm",
    ".py": "python", ".pyw": "python",
    ".js": "javascript", ".ts": "typescript", ".jsx": "javascript", ".tsx": "typescript",
    ".rs": "rust", ".go": "go", ".java": "java", ".cs": "csharp", ".swift": "swift",
    ".ps1": "powershell", ".bat": "batch", ".sh": "bash", ".bash": "bash", ".cmd": "batch",
    ".cmake": "cmake", ".make": "makefile", ".makefile": "makefile",
    ".json": "json", ".yaml": "yaml", ".yml": "yaml", ".toml": "toml",
    ".xml": "xml", ".ini": "ini", ".cfg": "ini",
    ".md": "markdown", ".txt": "plaintext", ".rst": "restructuredtext", ".log": "log",
    ".sql": "sql", ".graphql": "graphql",
    ".html": "html", ".css": "css", ".scss": "scss",
    ".dockerfile": "dockerfile",
    ".gitignore": "gitignore", ".editorconfig": "editorconfig",
}

def detect_language(filepath: str) -> str:
    ext = Path(filepath).suffix.lower()
    name = Path(filepath).name.lower()
    if name == "cmakelists.txt":
        return "cmake"
    if name == "makefile":
        return "makefile"
    if name == "dockerfile":
        return "dockerfile"
    if name.startswith("modelfile"):
        return "ollama_modelfile"
    return EXT_TO_LANG.get(ext, "unknown")

def detect_category(filepath: str, lang: str) -> str:
    fp = filepath.lower()
    ext = Path(filepath).suffix.lower()

    if ext in (".md", ".txt", ".rst"):
        return "doc"
    if ext in (".log",):
        return "log"
    if ext in (".json", ".yaml", ".yml", ".toml", ".xml", ".ini", ".cfg",
               ".gitignore", ".editorconfig"):
        return "config"
    if ext in (".cmake",) or "cmakelists" in fp:
        return "build"
    if ext in (".ps1", ".bat", ".sh", ".bash", ".cmd"):
        return "script"
    if ext in (".asm", ".inc", ".masm"):
        return "asm"
    return "code"

# ---------------------------------------------------------------------------
# Exclusion Logic
# ---------------------------------------------------------------------------

def _compile_excludes():
    """Pre-compile exclusion checks for speed."""
    dir_excludes = []
    ext_excludes = []
    for pat in EXCLUDE_PATTERNS:
        if pat.startswith("*."):
            ext_excludes.append(pat[1:].lower())  # e.g. ".gguf"
        elif pat.endswith("/"):
            dir_excludes.append(pat.rstrip("/").lower())
        else:
            dir_excludes.append(pat.lower())
    return dir_excludes, ext_excludes

DIR_EXCLUDES, EXT_EXCLUDES = _compile_excludes()

def should_exclude(filepath: str) -> bool:
    fp_lower = filepath.lower().replace("\\", "/")
    ext = Path(filepath).suffix.lower()
    if ext in EXT_EXCLUDES:
        return True
    for d in DIR_EXCLUDES:
        if f"/{d}/" in fp_lower or fp_lower.endswith(f"/{d}"):
            return True
    return False

# ---------------------------------------------------------------------------
# File Reading (robust encoding handling)
# ---------------------------------------------------------------------------

ENCODINGS = ["utf-8", "utf-8-sig", "latin-1", "cp1252", "ascii"]

def read_file_safe(filepath: str) -> str | None:
    """Try multiple encodings, return text or None."""
    try:
        size = os.path.getsize(filepath)
        if size > MAX_FILE_MB * 1024 * 1024:
            return None
        if size < MIN_FILE_BYTES:
            return None
    except OSError:
        return None

    for enc in ENCODINGS:
        try:
            with open(filepath, "r", encoding=enc, errors="strict") as f:
                text = f.read()
            # Basic binary detection: if >10% non-printable, skip
            if len(text) > 0:
                non_print = sum(1 for c in text[:2000]
                                if not c.isprintable() and c not in "\n\r\t")
                sample_len = min(len(text), 2000)
                if non_print / sample_len > 0.10:
                    return None
            return text
        except (UnicodeDecodeError, UnicodeError):
            continue
        except OSError:
            return None
    return None

# ---------------------------------------------------------------------------
# Content Hash for Dedup
# ---------------------------------------------------------------------------

def content_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8", errors="replace")).hexdigest()

# ---------------------------------------------------------------------------
# Scanner
# ---------------------------------------------------------------------------

class CorpusExtractor:
    def __init__(self):
        self.seen_hashes: set[str] = set()
        self.stats = defaultdict(int)
        self.total_bytes = 0
        self.total_files = 0
        self.skipped_binary = 0
        self.skipped_too_large = 0
        self.skipped_too_small = 0
        self.skipped_excluded = 0
        self.skipped_duplicate = 0

    def scan_directory(self, root_dir: str) -> list[str]:
        """Recursively find all candidate files."""
        candidates = []
        if not os.path.isdir(root_dir):
            print(f"  [SKIP] Directory not found: {root_dir}")
            return candidates

        for dirpath, dirnames, filenames in os.walk(root_dir, topdown=True):
            # Prune excluded directories in-place for speed
            dirnames[:] = [d for d in dirnames
                           if d.lower() not in DIR_EXCLUDES
                           and not d.startswith(".git")]

            for fname in filenames:
                fpath = os.path.join(dirpath, fname)
                ext = Path(fname).suffix.lower()

                # Extension filter
                name_lower = fname.lower()
                if ext not in INCLUDE_EXT and name_lower not in (
                    "cmakelists.txt", "makefile", "dockerfile",
                ) and not name_lower.startswith("modelfile"):
                    continue

                if should_exclude(fpath):
                    self.skipped_excluded += 1
                    continue

                candidates.append(fpath)

        return candidates

    def process_file(self, filepath: str) -> dict | None:
        """Read and process a single file, return JSONL record or None."""
        text = read_file_safe(filepath)
        if text is None:
            return None

        text = text.strip()
        if len(text) < MIN_FILE_BYTES:
            self.skipped_too_small += 1
            return None

        # Dedup
        h = content_hash(text)
        if h in self.seen_hashes:
            self.skipped_duplicate += 1
            return None
        self.seen_hashes.add(h)

        lang = detect_language(filepath)
        cat = detect_category(filepath, lang)
        lines = text.count("\n") + 1

        return {
            "text": text,
            "source": filepath,
            "language": lang,
            "category": cat,
            "size_bytes": len(text.encode("utf-8", errors="replace")),
            "lines": lines,
        }

    def scan_top_level_files(self) -> list[str]:
        """Grab loose files on D: root (docs, scripts, configs)."""
        candidates = []
        try:
            for item in os.scandir(SOURCE_ROOT):
                if item.is_file():
                    ext = Path(item.name).suffix.lower()
                    if ext in INCLUDE_EXT:
                        if not should_exclude(item.path):
                            candidates.append(item.path)
        except OSError:
            pass
        return candidates

    def run(self):
        print("=" * 72)
        print("  RawrXD Corpus Extractor v1.0")
        print("  Scanning D: drive for training data...")
        print("=" * 72)

        # Ensure output directory exists
        os.makedirs(OUTPUT_DIR, exist_ok=True)

        # Collect all candidates
        all_candidates = []

        if SCAN_TOP_LEVEL:
            print(f"\n[SCAN] Top-level files in {SOURCE_ROOT}")
            top = self.scan_top_level_files()
            all_candidates.extend(top)
            print(f"  Found {len(top)} top-level files")

        for scan_dir in SCAN_DIRS:
            print(f"\n[SCAN] {scan_dir}")
            found = self.scan_directory(scan_dir)
            all_candidates.extend(found)
            print(f"  Found {len(found)} candidate files")

        print(f"\n{'=' * 72}")
        print(f"  Total candidates: {len(all_candidates)}")
        print(f"  Processing files...")
        print(f"{'=' * 72}\n")

        # Process files and write JSONL
        written = 0
        start = time.time()

        with open(RAW_JSONL, "w", encoding="utf-8") as out:
            for i, fpath in enumerate(all_candidates):
                if (i + 1) % 500 == 0:
                    elapsed = time.time() - start
                    rate = (i + 1) / elapsed if elapsed > 0 else 0
                    print(f"  [{i+1}/{len(all_candidates)}] "
                          f"{rate:.0f} files/sec | "
                          f"Written: {written} | "
                          f"Dupes: {self.skipped_duplicate}")

                record = self.process_file(fpath)
                if record:
                    out.write(json.dumps(record, ensure_ascii=False) + "\n")
                    self.total_bytes += record["size_bytes"]
                    self.total_files += 1
                    written += 1

                    # Track by category
                    self.stats[record["category"]] += 1

        elapsed = time.time() - start

        # Print summary
        print(f"\n{'=' * 72}")
        print(f"  CORPUS EXTRACTION COMPLETE")
        print(f"{'=' * 72}")
        print(f"  Output:           {RAW_JSONL}")
        print(f"  Files written:    {written:,}")
        print(f"  Total text size:  {self.total_bytes / (1024*1024):.1f} MB")
        print(f"  Duplicates:       {self.skipped_duplicate:,}")
        print(f"  Excluded:         {self.skipped_excluded:,}")
        print(f"  Time:             {elapsed:.1f} seconds")
        print(f"\n  By category:")
        for cat, count in sorted(self.stats.items(), key=lambda x: -x[1]):
            print(f"    {cat:15s} {count:,}")
        print(f"{'=' * 72}")

        # Write metadata
        meta = {
            "created": time.strftime("%Y-%m-%d %H:%M:%S"),
            "source_root": SOURCE_ROOT,
            "total_files": written,
            "total_bytes": self.total_bytes,
            "total_bytes_human": f"{self.total_bytes/(1024*1024):.1f} MB",
            "duplicates_removed": self.skipped_duplicate,
            "categories": dict(self.stats),
            "scan_directories": SCAN_DIRS,
        }
        meta_path = os.path.join(OUTPUT_DIR, "corpus_metadata.json")
        with open(meta_path, "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        print(f"  Metadata: {meta_path}")


if __name__ == "__main__":
    extractor = CorpusExtractor()
    extractor.run()
