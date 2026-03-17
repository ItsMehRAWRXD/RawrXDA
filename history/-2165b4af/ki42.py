#!/usr/bin/env python3
"""
RawrXD Instruction Formatter - Phase 2
========================================
Converts raw corpus JSONL into instruction-tuning format for QLoRA fine-tuning.

Generates multiple training signal types from the raw code:
  1. Code Completion   - Given partial code, complete it
  2. Code Explanation  - Explain what this code does
  3. Bug/Error Fix     - Given error context + code, suggest fix
  4. Documentation     - Generate docs/comments for code
  5. Refactoring       - Suggest refactored version
  6. Architecture Q&A  - Answer questions about project structure
  7. Build System      - CMake/build config understanding
  8. ASM Translation   - MASM <-> C++ correspondence

Output format (ChatML-style for broad model compatibility):
{
    "conversations": [
        {"role": "system", "content": "You are RawrXD-AI..."},
        {"role": "user", "content": "..."},
        {"role": "assistant", "content": "..."}
    ]
}
"""

import os
import sys
import json
import random
import re
import time
from pathlib import Path
from collections import defaultdict

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "pipeline_config.json")
with open(CONFIG_PATH, "r", encoding="utf-8") as f:
    CONFIG = json.load(f)

CORPUS_CFG = CONFIG["corpus"]
RAW_JSONL = CORPUS_CFG["raw_jsonl"]
INSTRUCTIONS_JSONL = CORPUS_CFG["instructions_jsonl"]
OUTPUT_DIR = CORPUS_CFG["output_dir"]

# System prompt baked into every training example
SYSTEM_PROMPT = """You are RawrXD-AI, an expert AI coding assistant specialized in the RawrXD-Shell IDE project. You have deep knowledge of:
- C++20, MASM64 assembly, and systems programming
- The three-layer hotpatching architecture (memory, byte-level, server)
- GGUF model loading, quantization, and inference
- The agentic failure recovery system (detector, puppeteer, proxy)
- CMake build systems on Windows (MSVC 2022) and POSIX (Clang)
- The RawrXD IDE's refactoring engine, LSP integration, and GPU acceleration
- No-exception error handling with PatchResult patterns
- llama.cpp internals and GGML tensor operations

You write correct, production-quality code. You never use exceptions. You use structured PatchResult returns. You follow the project's coding conventions exactly."""

# ---------------------------------------------------------------------------
# Instruction Generators
# ---------------------------------------------------------------------------

def make_code_completion(record: dict) -> list[dict] | None:
    """Split code at a random point, ask model to complete it."""
    text = record["text"]
    lines = text.split("\n")

    if len(lines) < 10:
        return None

    # Pick a split point between 30-70% of the file
    split = random.randint(int(len(lines) * 0.3), int(len(lines) * 0.7))
    prefix = "\n".join(lines[:split])
    suffix = "\n".join(lines[split:])

    if len(suffix.strip()) < 20:
        return None

    lang = record["language"]
    source = record["source"]

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Complete the following {lang} code from `{Path(source).name}`:\n\n```{lang}\n{prefix}\n```\n\nContinue writing the rest of this file."},
            {"role": "assistant", "content": f"```{lang}\n{suffix}\n```"}
        ]
    }]


def make_code_explanation(record: dict) -> list[dict] | None:
    """Ask model to explain what a code file does."""
    text = record["text"]
    if len(text) < 100 or record["category"] not in ("code", "asm"):
        return None

    lang = record["language"]
    source = record["source"]
    fname = Path(source).name

    # For headers, explain the API surface
    if fname.endswith((".h", ".hpp")):
        prompt = f"Explain the API and purpose of this header file `{fname}`:\n\n```{lang}\n{text}\n```"
        # Generate a synthetic but useful explanation
        explanation = _generate_header_explanation(text, fname)
    else:
        prompt = f"Explain what this {lang} file `{fname}` does:\n\n```{lang}\n{text}\n```"
        explanation = _generate_code_explanation(text, fname, lang)

    if not explanation:
        return None

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": prompt},
            {"role": "assistant", "content": explanation}
        ]
    }]


def make_fill_in_the_middle(record: dict) -> list[dict] | None:
    """FIM-style: give prefix and suffix, ask for the middle."""
    text = record["text"]
    lines = text.split("\n")

    if len(lines) < 20:
        return None

    # Pick a function or block to mask
    start = random.randint(5, len(lines) - 15)
    end = min(start + random.randint(5, 20), len(lines) - 3)

    prefix = "\n".join(lines[:start])
    middle = "\n".join(lines[start:end])
    suffix = "\n".join(lines[end:])

    if len(middle.strip()) < 30:
        return None

    lang = record["language"]
    fname = Path(record["source"]).name

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Fill in the missing code section in `{fname}`.\n\nBefore the gap:\n```{lang}\n{prefix}\n```\n\nAfter the gap:\n```{lang}\n{suffix}\n```\n\nWrite the missing code that goes between these sections."},
            {"role": "assistant", "content": f"```{lang}\n{middle}\n```"}
        ]
    }]


def make_doc_qa(record: dict) -> list[dict] | None:
    """Turn documentation into Q&A pairs."""
    if record["category"] != "doc":
        return None
    text = record["text"]
    if len(text) < 200:
        return None

    fname = Path(record["source"]).name
    # Extract sections (## headers in markdown)
    sections = re.split(r'\n#{1,3}\s+', text)
    results = []

    for section in sections[:3]:  # Max 3 Q&A per doc
        section = section.strip()
        if len(section) < 100:
            continue

        # First line is likely the section title
        lines = section.split("\n", 1)
        if len(lines) < 2:
            continue

        title = lines[0].strip()
        content = lines[1].strip()

        if title and content:
            results.append({
                "conversations": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": f"From the RawrXD project documentation ({fname}): {title}"},
                    {"role": "assistant", "content": content}
                ]
            })

    return results if results else None


def make_build_understanding(record: dict) -> list[dict] | None:
    """Turn CMake/build files into understanding tasks."""
    if record["category"] != "build" and record["language"] != "cmake":
        return None
    text = record["text"]
    if len(text) < 50:
        return None

    fname = Path(record["source"]).name

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Explain this CMake/build configuration from `{fname}`:\n\n```cmake\n{text}\n```"},
            {"role": "assistant", "content": _generate_cmake_explanation(text, fname)}
        ]
    }]


def make_script_explanation(record: dict) -> list[dict] | None:
    """Turn build/automation scripts into explained versions."""
    if record["category"] != "script":
        return None
    text = record["text"]
    if len(text) < 100:
        return None

    lang = record["language"]
    fname = Path(record["source"]).name

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"What does this {lang} script `{fname}` do? Walk through it step by step.\n\n```{lang}\n{text}\n```"},
            {"role": "assistant", "content": _generate_script_explanation(text, fname, lang)}
        ]
    }]


def make_config_understanding(record: dict) -> list[dict] | None:
    """Turn config files into understanding pairs."""
    if record["category"] != "config":
        return None
    text = record["text"]
    if len(text) < 50 or len(text) > 50000:
        return None

    lang = record["language"]
    fname = Path(record["source"]).name

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Explain this {lang} configuration file `{fname}` from the RawrXD project:\n\n```{lang}\n{text}\n```"},
            {"role": "assistant", "content": f"This configuration file `{fname}` defines settings for the RawrXD-Shell IDE project.\n\n{text}"}
        ]
    }]


def make_raw_memorization(record: dict) -> list[dict] | None:
    """Direct file memorization - model learns the codebase verbatim."""
    text = record["text"]
    if len(text) < 50 or len(text) > 32000:
        return None

    lang = record["language"]
    source = record["source"]
    fname = Path(source).name
    rel_path = source.replace("D:\\", "").replace("\\", "/")

    return [{
        "conversations": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Show me the contents of `{rel_path}` from the RawrXD project."},
            {"role": "assistant", "content": f"Here is `{rel_path}`:\n\n```{lang}\n{text}\n```"}
        ]
    }]


# ---------------------------------------------------------------------------
# Synthetic Explanation Generators (heuristic-based)
# ---------------------------------------------------------------------------

def _generate_header_explanation(text: str, fname: str) -> str:
    """Generate a structural explanation of a header file."""
    parts = []
    parts.append(f"`{fname}` is a header file in the RawrXD-Shell project.\n")

    # Find classes
    classes = re.findall(r'(?:class|struct)\s+(\w+)', text)
    if classes:
        parts.append(f"**Defined types:** {', '.join(f'`{c}`' for c in classes)}\n")

    # Find key functions
    funcs = re.findall(r'(?:static\s+)?(?:\w+\s+)+(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?;', text)
    if funcs:
        unique_funcs = list(dict.fromkeys(funcs))[:10]
        parts.append(f"**Key functions:** {', '.join(f'`{f}()`' for f in unique_funcs)}\n")

    # Find enums
    enums = re.findall(r'enum\s+(?:class\s+)?(\w+)', text)
    if enums:
        parts.append(f"**Enumerations:** {', '.join(f'`{e}`' for e in enums)}\n")

    # Find includes
    includes = re.findall(r'#include\s*[<"]([^>"]+)[>"]', text)
    if includes:
        parts.append(f"**Dependencies:** {', '.join(includes[:8])}\n")

    if len(parts) <= 1:
        return ""

    return "\n".join(parts)


def _generate_code_explanation(text: str, fname: str, lang: str) -> str:
    """Generate a basic explanation of a source file."""
    parts = [f"`{fname}` implements functionality for the RawrXD-Shell IDE.\n"]

    # Find function definitions (C/C++)
    if lang in ("cpp", "c", "c_cpp"):
        funcs = re.findall(r'(?:\w+\s+)+(\w+::\w+|\w+)\s*\([^)]*\)\s*\{', text)
        if funcs:
            unique = list(dict.fromkeys(funcs))[:10]
            parts.append(f"**Functions implemented:** {', '.join(f'`{f}()`' for f in unique)}\n")

    # Find key patterns
    if "PatchResult" in text:
        parts.append("Uses the `PatchResult` structured error handling pattern (no exceptions).\n")
    if "VirtualProtect" in text or "mprotect" in text:
        parts.append("Performs memory protection manipulation for hotpatching.\n")
    if "std::mutex" in text or "std::atomic" in text:
        parts.append("Contains thread-safe operations with mutex/atomic synchronization.\n")

    if len(parts) <= 1:
        return ""

    return "\n".join(parts)


def _generate_cmake_explanation(text: str, fname: str) -> str:
    """Explain CMake configuration."""
    parts = [f"This `{fname}` configures the build system.\n"]

    targets = re.findall(r'add_(?:executable|library)\s*\(\s*(\w+)', text)
    if targets:
        parts.append(f"**Build targets:** {', '.join(f'`{t}`' for t in targets)}\n")

    deps = re.findall(r'find_package\s*\(\s*(\w+)', text)
    if deps:
        parts.append(f"**Dependencies:** {', '.join(deps)}\n")

    if "enable_testing" in text.lower() or "add_test" in text.lower():
        parts.append("Includes test configuration.\n")

    return "\n".join(parts)


def _generate_script_explanation(text: str, fname: str, lang: str) -> str:
    """Explain a script file step by step."""
    lines = text.split("\n")
    steps = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("//") or line.startswith("REM"):
            if line.startswith("#") or line.startswith("//") or line.startswith("REM"):
                steps.append(line.lstrip("#/ REM").strip())
    if steps:
        numbered = "\n".join(f"{i+1}. {s}" for i, s in enumerate(steps[:15]) if s)
        return f"This {lang} script `{fname}` performs the following steps:\n\n{numbered}"
    return f"This is a {lang} automation script used in the RawrXD build/deployment pipeline."


# ---------------------------------------------------------------------------
# Main Pipeline
# ---------------------------------------------------------------------------

GENERATORS = [
    ("completion", make_code_completion, 1.0),
    ("explanation", make_code_explanation, 0.5),
    ("fim", make_fill_in_the_middle, 0.8),
    ("doc_qa", make_doc_qa, 1.0),
    ("build", make_build_understanding, 1.0),
    ("script", make_script_explanation, 0.7),
    ("config", make_config_understanding, 0.5),
    ("memorization", make_raw_memorization, 1.0),
]


def run():
    print("=" * 72)
    print("  RawrXD Instruction Formatter v1.0")
    print("  Converting raw corpus to instruction-tuning format...")
    print("=" * 72)

    if not os.path.exists(RAW_JSONL):
        print(f"\n  ERROR: Raw corpus not found at {RAW_JSONL}")
        print(f"  Run 01_extract_corpus.py first!")
        sys.exit(1)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Count input records
    total_input = 0
    with open(RAW_JSONL, "r", encoding="utf-8") as f:
        for _ in f:
            total_input += 1

    print(f"\n  Input records: {total_input:,}")

    # Process
    stats = defaultdict(int)
    total_output = 0
    start = time.time()

    with open(RAW_JSONL, "r", encoding="utf-8") as inp, \
         open(INSTRUCTIONS_JSONL, "w", encoding="utf-8") as out:

        for i, line in enumerate(inp):
            if (i + 1) % 1000 == 0:
                print(f"  [{i+1}/{total_input}] Generated {total_output:,} instructions...")

            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue

            for name, generator, probability in GENERATORS:
                if random.random() > probability:
                    continue

                try:
                    results = generator(record)
                    if results:
                        for r in results:
                            out.write(json.dumps(r, ensure_ascii=False) + "\n")
                            total_output += 1
                            stats[name] += 1
                except Exception as e:
                    continue  # Skip problematic files silently

    elapsed = time.time() - start

    print(f"\n{'=' * 72}")
    print(f"  INSTRUCTION FORMATTING COMPLETE")
    print(f"{'=' * 72}")
    print(f"  Output:             {INSTRUCTIONS_JSONL}")
    print(f"  Instructions:       {total_output:,}")
    print(f"  From {total_input:,} source files")
    print(f"  Time:               {elapsed:.1f} seconds")
    print(f"\n  By generator:")
    for name, count in sorted(stats.items(), key=lambda x: -x[1]):
        print(f"    {name:20s} {count:,}")
    print(f"{'=' * 72}")

    # Write stats
    meta = {
        "created": time.strftime("%Y-%m-%d %H:%M:%S"),
        "input_file": RAW_JSONL,
        "output_file": INSTRUCTIONS_JSONL,
        "total_input": total_input,
        "total_output": total_output,
        "generators": dict(stats),
    }
    meta_path = os.path.join(OUTPUT_DIR, "instructions_metadata.json")
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)


if __name__ == "__main__":
    run()
