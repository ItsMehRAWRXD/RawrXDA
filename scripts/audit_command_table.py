#!/usr/bin/env python3
# =============================================================================
# audit_command_table.py — COMMAND_TABLE Coverage Auditor
# =============================================================================
# Architecture: Build-time source validator for the SSOT COMMAND_TABLE system
# Purpose: Cross-references COMMAND_TABLE entries in command_registry.hpp against
#          Win32IDE.h IDM_* defines and feature_handlers.h declarations to detect:
#          - IDM_* defines not covered by COMMAND_TABLE
#          - Handlers declared but not referenced in COMMAND_TABLE
#          - Duplicate command IDs or CLI aliases
#          - Legacy ID collisions
#
# Run: python scripts/audit_command_table.py [--src-root D:\rawrxd\src]
# Output: reports/command_table_audit.md   (human-readable coverage report)
#         reports/command_table_audit.json  (machine-readable registry snapshot)
#
# The SSOT for command registration is:
#   src/core/command_registry.hpp  → COMMAND_TABLE X-macro
#   src/core/unified_command_dispatch.cpp → AutoRegistrar (populates SharedFeatureRegistry)
#
# To add a new command: Add ONE line to COMMAND_TABLE in command_registry.hpp.
# Then re-run this script to verify coverage.
#
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

import re
import os
import sys
import json
import hashlib
from datetime import datetime
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Set, Optional, Tuple

# =============================================================================
# CONFIGURATION
# =============================================================================

DEFAULT_SRC_ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")

# Files to scan for IDM_* defines (Win32 GUI surface)
IDM_SCAN_FILES = [
    "win32app/Win32IDE.h",
    "win32app/Win32IDE.cpp",
    "win32app/Win32IDE_Commands.cpp",
    "win32app/Win32IDE_VoiceAutomation.cpp",
    "win32app/Win32IDE_DecompilerView.cpp",
    "ide_constants.h",
    "vscode_extension_api.h",
]

# The Single Source of Truth
COMMAND_TABLE_FILE = "core/command_registry.hpp"

# Handler declaration files
HANDLER_SCAN_FILES = [
    "core/feature_handlers.h",
    "core/command_registry.hpp",   # EXPAND_CMD_DECL forward-declares all handlers
]

# Files to exclude from broad scans
EXCLUDE_FILES = {"auto_feature_registry.cpp", "auto_feature_registry.hpp"}

# Range markers / internal defines that are not real commands
SKIP_SUFFIXES = {"_BASE", "_END", "_FIRST", "_LAST", "_COUNT", "_MAX", "_MIN"}
SKIP_NAMES = {"IDM_THEME_END"}  # Known non-command defines

# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass
class IDMDefine:
    """A #define IDM_* found in Win32IDE.h or similar."""
    name: str
    value: int
    file: str
    line: int

    @property
    def category(self) -> str:
        parts = self.name.replace("IDM_", "").split("_")
        return parts[0] if parts else "UNKNOWN"

@dataclass
class CommandTableEntry:
    """A single COMMAND_TABLE X-macro row."""
    id: int
    symbol: str
    canonical_name: str
    cli_alias: str
    exposure: str
    category: str
    handler: str
    flags: str
    line: int

@dataclass
class HandlerDecl:
    """A handler function declaration."""
    name: str
    file: str
    line: int

@dataclass
class AuditResult:
    """Full audit output."""
    total_idm_defines: int = 0
    total_table_entries: int = 0
    total_handlers_declared: int = 0
    gui_routable: int = 0          # COMMAND_TABLE entries with ID != 0
    cli_accessible: int = 0        # COMMAND_TABLE entries with CLI_ONLY or BOTH
    coverage_pct: float = 0.0
    missing_from_table: List[dict] = field(default_factory=list)
    orphan_handlers: List[str] = field(default_factory=list)
    duplicate_ids: List[dict] = field(default_factory=list)
    duplicate_aliases: List[dict] = field(default_factory=list)
    categories: Dict[str, int] = field(default_factory=dict)
    entries: List[dict] = field(default_factory=list)
    is_clean: bool = True

# =============================================================================
# SCANNER: IDM_* defines in Win32IDE.h
# =============================================================================

def scan_idm_defines(src_root: str) -> Dict[str, IDMDefine]:
    """Scan Win32IDE.h and related files for #define IDM_* patterns."""
    defines = {}
    pattern = re.compile(r'#define\s+(IDM_\w+)\s+(\d+|0x[0-9a-fA-F]+)')

    for rel_path in IDM_SCAN_FILES:
        full_path = os.path.join(src_root, rel_path)
        if not os.path.exists(full_path):
            continue
        with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
            for lineno, line in enumerate(f, 1):
                m = pattern.search(line)
                if m:
                    name = m.group(1)
                    val = int(m.group(2), 0)
                    if name not in defines:  # first definition wins
                        defines[name] = IDMDefine(name=name, value=val, file=rel_path, line=lineno)

    # Also scan broadly for any IDM_* in other source files
    for root, dirs, files in os.walk(src_root):
        dirs[:] = [d for d in dirs if d not in {'build', '.git', '__pycache__'}]
        for fname in files:
            if fname in EXCLUDE_FILES:
                continue
            if not fname.endswith(('.h', '.hpp')):
                continue
            rel = os.path.relpath(os.path.join(root, fname), src_root).replace('\\', '/')
            if rel in IDM_SCAN_FILES:
                continue
            full = os.path.join(root, fname)
            try:
                with open(full, 'r', encoding='utf-8', errors='replace') as f:
                    for lineno, line in enumerate(f, 1):
                        m = pattern.search(line)
                        if m:
                            name = m.group(1)
                            if name not in defines:
                                val = int(m.group(2), 0)
                                defines[name] = IDMDefine(name=name, value=val, file=rel, line=lineno)
            except (OSError, UnicodeDecodeError):
                pass

    return defines

# =============================================================================
# SCANNER: COMMAND_TABLE entries
# =============================================================================

def scan_command_table(src_root: str) -> List[CommandTableEntry]:
    """Parse COMMAND_TABLE X-macro entries from command_registry.hpp."""
    entries = []
    full_path = os.path.join(src_root, COMMAND_TABLE_FILE)
    if not os.path.exists(full_path):
        print(f"[ERROR] {COMMAND_TABLE_FILE} not found at {full_path}", file=sys.stderr)
        return entries

    # Match X(ID, SYMBOL, "canonical", "cli", EXPOSURE, "category", handler, FLAGS)
    # The X-macro rows can have complex flags like CMD_REQUIRES_FILE | CMD_ASYNC
    x_pattern = re.compile(
        r'X\(\s*(\d+)\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*'
        r'(\w+)\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,\s*([^)]+)\)',
        re.MULTILINE
    )

    with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    for m in x_pattern.finditer(content):
        lineno = content[:m.start()].count('\n') + 1
        entries.append(CommandTableEntry(
            id=int(m.group(1)),
            symbol=m.group(2),
            canonical_name=m.group(3),
            cli_alias=m.group(4),
            exposure=m.group(5).strip(),
            category=m.group(6),
            handler=m.group(7),
            flags=m.group(8).strip(),
            line=lineno,
        ))

    return entries

# =============================================================================
# SCANNER: Handler declarations
# =============================================================================

def scan_handler_declarations(src_root: str) -> Dict[str, HandlerDecl]:
    """Scan for CommandResult handler(const CommandContext&) declarations."""
    handlers = {}
    pattern = re.compile(r'^CommandResult\s+(handle\w+)\s*\(', re.MULTILINE)

    for rel_path in HANDLER_SCAN_FILES:
        full_path = os.path.join(src_root, rel_path)
        if not os.path.exists(full_path):
            continue
        with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        for m in pattern.finditer(content):
            name = m.group(1)
            line = content[:m.start()].count('\n') + 1
            if name not in handlers:
                handlers[name] = HandlerDecl(name=name, file=rel_path, line=line)

    return handlers

# =============================================================================
# AUDITOR: Cross-reference everything
# =============================================================================

def should_skip_idm(name: str) -> bool:
    """Skip range markers and internal defines."""
    if name in SKIP_NAMES:
        return True
    for suffix in SKIP_SUFFIXES:
        if name.endswith(suffix):
            return True
    return False

def run_audit(src_root: str) -> AuditResult:
    """Run full cross-reference audit."""
    result = AuditResult()

    # 1. Scan IDM_* defines from Win32 headers
    idm_defines = scan_idm_defines(src_root)
    # Filter to non-zero, non-range-marker defines
    real_idm = {n: d for n, d in idm_defines.items() if d.value != 0 and not should_skip_idm(n)}
    result.total_idm_defines = len(real_idm)

    # 2. Scan COMMAND_TABLE
    table_entries = scan_command_table(src_root)
    result.total_table_entries = len(table_entries)

    # 3. Scan handler declarations
    handlers = scan_handler_declarations(src_root)
    result.total_handlers_declared = len(handlers)

    # 4. Build lookup sets from COMMAND_TABLE
    table_ids: Dict[int, List[CommandTableEntry]] = {}
    table_aliases: Dict[str, List[CommandTableEntry]] = {}
    table_handlers: Set[str] = set()

    for entry in table_entries:
        # Count GUI-routable
        if entry.id != 0:
            result.gui_routable += 1
        # Count CLI-accessible
        if entry.exposure in ("BOTH", "CLI_ONLY"):
            result.cli_accessible += 1
        # Track duplicates
        if entry.id != 0:
            table_ids.setdefault(entry.id, []).append(entry)
        table_aliases.setdefault(entry.cli_alias, []).append(entry)
        table_handlers.add(entry.handler)
        # Category stats
        result.categories[entry.category] = result.categories.get(entry.category, 0) + 1
        # Store for JSON export
        result.entries.append({
            "id": entry.id,
            "symbol": entry.symbol,
            "canonical": entry.canonical_name,
            "cli": entry.cli_alias,
            "exposure": entry.exposure,
            "category": entry.category,
            "handler": entry.handler,
            "flags": entry.flags,
        })

    # 5. Find IDM_* defines missing from COMMAND_TABLE
    table_id_set = {entry.id for entry in table_entries if entry.id != 0}
    for name, defn in sorted(real_idm.items(), key=lambda x: x[1].value):
        if defn.value not in table_id_set:
            result.missing_from_table.append({
                "name": name,
                "value": defn.value,
                "file": defn.file,
                "line": defn.line,
                "category": defn.category,
            })

    # 6. Find orphan handlers (declared but not in COMMAND_TABLE)
    for hname in sorted(handlers.keys()):
        if hname not in table_handlers:
            result.orphan_handlers.append(hname)

    # 7. Find duplicate IDs
    for cmd_id, entries in table_ids.items():
        if len(entries) > 1:
            result.duplicate_ids.append({
                "id": cmd_id,
                "entries": [e.canonical_name for e in entries],
            })

    # 8. Find duplicate CLI aliases (excluding intentional legacy)
    for alias, entries in table_aliases.items():
        if len(entries) > 1:
            result.duplicate_aliases.append({
                "alias": alias,
                "entries": [e.canonical_name for e in entries],
            })

    # 9. Coverage calculation
    if result.total_idm_defines > 0:
        covered = result.total_idm_defines - len(result.missing_from_table)
        result.coverage_pct = (covered / result.total_idm_defines) * 100
    else:
        result.coverage_pct = 100.0

    # 10. Is it clean?
    result.is_clean = (
        len(result.missing_from_table) == 0 and
        len(result.duplicate_ids) == 0
    )

    return result

# =============================================================================
# REPORT GENERATORS
# =============================================================================

def generate_markdown_report(result: AuditResult, src_root: str) -> str:
    """Generate a Markdown audit report."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = []
    lines.append("# COMMAND_TABLE Coverage Audit Report")
    lines.append(f"Generated: {now}")
    lines.append(f"Source root: `{src_root}`")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"| Metric | Count |")
    lines.append(f"|--------|-------|")
    lines.append(f"| IDM_* defines (non-zero, non-range) | {result.total_idm_defines} |")
    lines.append(f"| COMMAND_TABLE entries | {result.total_table_entries} |")
    lines.append(f"| Handler declarations | {result.total_handlers_declared} |")
    lines.append(f"| GUI-routable (ID ≠ 0) | {result.gui_routable} |")
    lines.append(f"| CLI-accessible (BOTH + CLI_ONLY) | {result.cli_accessible} |")
    lines.append(f"| **Coverage** | **{result.coverage_pct:.1f}%** |")
    lines.append(f"| Status | {'✅ CLEAN' if result.is_clean else '⚠️ ISSUES FOUND'} |")
    lines.append("")

    # Categories
    lines.append("## Categories")
    lines.append("")
    lines.append("| Category | Commands |")
    lines.append("|----------|----------|")
    for cat, count in sorted(result.categories.items(), key=lambda x: -x[1]):
        lines.append(f"| {cat} | {count} |")
    lines.append("")

    # Missing from table
    if result.missing_from_table:
        lines.append("## ⚠️ IDM_* Defines Missing from COMMAND_TABLE")
        lines.append("")
        lines.append("These Win32 GUI command IDs have no entry in COMMAND_TABLE.")
        lines.append("To fix: add an X(...) row in `command_registry.hpp`.")
        lines.append("")
        lines.append("| Define | Value | File | Line |")
        lines.append("|--------|-------|------|------|")
        for m in result.missing_from_table:
            lines.append(f"| `{m['name']}` | {m['value']} | {m['file']} | {m['line']} |")
        lines.append("")
    else:
        lines.append("## ✅ Full Coverage")
        lines.append("")
        lines.append("All IDM_* defines from Win32IDE.h are represented in COMMAND_TABLE.")
        lines.append("")

    # Duplicate IDs
    if result.duplicate_ids:
        lines.append("## ⚠️ Duplicate Command IDs")
        lines.append("")
        for dup in result.duplicate_ids:
            entries_str = ", ".join(f"`{e}`" for e in dup["entries"])
            lines.append(f"- **ID {dup['id']}**: {entries_str}")
        lines.append("")

    # Duplicate aliases
    if result.duplicate_aliases:
        lines.append("## ℹ️ Duplicate CLI Aliases")
        lines.append("")
        lines.append("These may be intentional (legacy aliases).")
        lines.append("")
        for dup in result.duplicate_aliases:
            entries_str = ", ".join(f"`{e}`" for e in dup["entries"])
            lines.append(f"- **`{dup['alias']}`**: {entries_str}")
        lines.append("")

    # Orphan handlers
    if result.orphan_handlers:
        lines.append("## ℹ️ Declared Handlers Not in COMMAND_TABLE")
        lines.append("")
        lines.append("These handlers exist in `feature_handlers.h` but aren't")
        lines.append("referenced by any COMMAND_TABLE entry. They may be called")
        lines.append("directly or are waiting to be wired up.")
        lines.append("")
        for h in result.orphan_handlers:
            lines.append(f"- `{h}`")
        lines.append("")

    return "\n".join(lines)


def generate_json_report(result: AuditResult) -> str:
    """Generate JSON registry snapshot."""
    data = {
        "generated": datetime.now().isoformat(),
        "summary": {
            "total_idm_defines": result.total_idm_defines,
            "total_table_entries": result.total_table_entries,
            "total_handlers": result.total_handlers_declared,
            "gui_routable": result.gui_routable,
            "cli_accessible": result.cli_accessible,
            "coverage_pct": round(result.coverage_pct, 2),
            "is_clean": result.is_clean,
        },
        "categories": result.categories,
        "missing_from_table": result.missing_from_table,
        "orphan_handlers": result.orphan_handlers,
        "duplicate_ids": result.duplicate_ids,
        "duplicate_aliases": result.duplicate_aliases,
        "entries": result.entries,
    }
    return json.dumps(data, indent=2)

# =============================================================================
# COMMAND_TABLE ENTRY GENERATOR
# =============================================================================

def generate_new_entries(result: AuditResult) -> str:
    """Generate COMMAND_TABLE X-macro lines for missing IDM_* defines.
    These can be pasted directly into command_registry.hpp."""
    if not result.missing_from_table:
        return "// All IDM_* defines are covered. No new entries needed.\n"

    lines = []
    lines.append("// ═══════════════════ AUTO-GENERATED ENTRIES ═══════════════════")
    lines.append("// Paste these into COMMAND_TABLE in command_registry.hpp")
    lines.append("// Then implement the handler stubs or map to existing handlers.")
    lines.append("")

    for m in result.missing_from_table:
        name = m["name"]
        value = m["value"]
        cat = m["category"]
        # Generate reasonable defaults
        symbol = name.replace("IDM_", "")
        canonical = f"{cat.lower()}.{symbol.lower().replace('_', '.')}"
        cli_alias = f"!{symbol.lower().replace('_', ' ')}"
        handler = f"handle{symbol.title().replace('_', '')}"
        lines.append(
            f'    X({value}, {symbol:30s} "{canonical}", '
            f'"{cli_alias}", BOTH, "{cat}", {handler}, CMD_NONE) \\\\'
        )

    return "\n".join(lines)

# =============================================================================
# MAIN
# =============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Audit COMMAND_TABLE coverage")
    parser.add_argument("--src-root", default=DEFAULT_SRC_ROOT,
                        help="Path to src/ directory")
    parser.add_argument("--generate", action="store_true",
                        help="Generate X-macro lines for missing entries")
    parser.add_argument("--json-only", action="store_true",
                        help="Output only JSON to stdout")
    parser.add_argument("--quiet", action="store_true",
                        help="Suppress console output (still writes report files)")
    args = parser.parse_args()

    src_root = os.path.abspath(args.src_root)
    if not os.path.isdir(src_root):
        print(f"[ERROR] Source root not found: {src_root}", file=sys.stderr)
        sys.exit(1)

    # Run audit
    result = run_audit(src_root)

    # Console output
    if not args.quiet and not args.json_only:
        print(f"╔══════════════════════════════════════════════════════════╗")
        print(f"║         COMMAND_TABLE Coverage Audit                    ║")
        print(f"╠══════════════════════════════════════════════════════════╣")
        print(f"║  IDM_* defines (Win32IDE.h):      {result.total_idm_defines:>4d}                 ║")
        print(f"║  COMMAND_TABLE entries:            {result.total_table_entries:>4d}                 ║")
        print(f"║  Handler declarations:             {result.total_handlers_declared:>4d}                 ║")
        print(f"║  GUI-routable (ID ≠ 0):           {result.gui_routable:>4d}                 ║")
        print(f"║  CLI-accessible:                   {result.cli_accessible:>4d}                 ║")
        print(f"║  Coverage:                      {result.coverage_pct:>6.1f}%                ║")
        print(f"║  Missing from table:              {len(result.missing_from_table):>4d}                 ║")
        print(f"║  Orphan handlers:                 {len(result.orphan_handlers):>4d}                 ║")
        print(f"║  Duplicate IDs:                   {len(result.duplicate_ids):>4d}                 ║")
        print(f"║  Status: {'✅ CLEAN' if result.is_clean else '⚠️  ISSUES'}                                   ║")
        print(f"╚══════════════════════════════════════════════════════════╝")

    if args.json_only:
        print(generate_json_report(result))
        return 0 if result.is_clean else 1

    # Write reports
    reports_dir = os.path.join(os.path.dirname(src_root), "reports")
    os.makedirs(reports_dir, exist_ok=True)

    md_path = os.path.join(reports_dir, "command_table_audit.md")
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(generate_markdown_report(result, src_root))
    if not args.quiet:
        print(f"\n  Report: {md_path}")

    json_path = os.path.join(reports_dir, "command_table_audit.json")
    with open(json_path, 'w', encoding='utf-8') as f:
        f.write(generate_json_report(result))
    if not args.quiet:
        print(f"  JSON:   {json_path}")

    # Optionally generate new entries
    if args.generate and result.missing_from_table:
        entries = generate_new_entries(result)
        entries_path = os.path.join(reports_dir, "new_command_table_entries.txt")
        with open(entries_path, 'w', encoding='utf-8') as f:
            f.write(entries)
        if not args.quiet:
            print(f"  New entries: {entries_path}")
    elif args.generate and not args.quiet:
        print(f"\n  ✅ No missing entries — nothing to generate.")

    return 0 if result.is_clean else 1


if __name__ == "__main__":
    sys.exit(main())
