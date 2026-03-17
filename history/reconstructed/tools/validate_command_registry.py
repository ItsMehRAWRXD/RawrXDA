#!/usr/bin/env python3
# ============================================================================
# validate_command_registry.py — Build-Time Registry Integrity Validator
# ============================================================================
# Purpose: Ensures COMMAND_TABLE in command_registry.hpp is the SSOT by:
#   1. Extracting all IDM_* from Win32IDE.h
#   2. Extracting all entries from COMMAND_TABLE
#   3. Cross-referencing: every IDM_* must be in COMMAND_TABLE
#   4. Detecting duplicate IDs, duplicate CLI aliases
#   5. Validating ID ranges per category
#
# Usage:
#   python validate_command_registry.py
#   python validate_command_registry.py --strict  (fail on warnings)
#   python validate_command_registry.py --json     (machine-readable output)
#
# Exit codes:
#   0 = clean (all IDM_* accounted for, no duplicates)
#   1 = errors found (missing commands, collisions)
#   2 = file not found
# ============================================================================

import re
import sys
import os
import json
from pathlib import Path
from collections import defaultdict

# Paths relative to project root
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
WIN32IDE_H = PROJECT_ROOT / "src" / "win32app" / "Win32IDE.h"
WIN32IDE_CPP = PROJECT_ROOT / "src" / "win32app" / "Win32IDE.cpp"
COMMAND_REGISTRY = PROJECT_ROOT / "src" / "core" / "command_registry.hpp"
COMMAND_RANGES = PROJECT_ROOT / "src" / "core" / "command_ranges.hpp"

# ── Regex patterns ──
IDM_PATTERN = re.compile(r'#define\s+(IDM_\w+)\s+(\d+)')
XTABLE_PATTERN = re.compile(
    r'X\(\s*(\d+)\s*,'            # ID
    r'\s*(\w+)\s*,'                # SYMBOL  
    r'\s*"([^"]+)"\s*,'            # canonical_name
    r'\s*"([^"]+)"\s*,'            # cli_alias
    r'\s*(\w+)\s*,'                # exposure
    r'\s*"([^"]+)"\s*,'            # category
    r'\s*(\w+)\s*,'                # handler
    r'\s*([^)]+)\s*\)'             # flags
)

RANGE_PATTERN = re.compile(
    r'constexpr\s+uint32_t\s+(\w+)\s*=\s*(\d+)\s*;'
)


def extract_idm_constants(*paths):
    """Extract all #define IDM_* constants from header files."""
    idm_map = {}
    for path in paths:
        if not path.exists():
            continue
        content = path.read_text(encoding='utf-8', errors='replace')
        for match in IDM_PATTERN.finditer(content):
            name = match.group(1)
            value = int(match.group(2))
            if name in idm_map and idm_map[name] != value:
                print(f"  [WARN] Duplicate IDM: {name} = {idm_map[name]} vs {value}")
            idm_map[name] = value
    return idm_map


def extract_command_table(path):
    """Extract all X(...) entries from COMMAND_TABLE macro."""
    if not path.exists():
        return []
    content = path.read_text(encoding='utf-8', errors='replace')
    entries = []
    for match in XTABLE_PATTERN.finditer(content):
        entries.append({
            'id': int(match.group(1)),
            'symbol': match.group(2),
            'canonical': match.group(3),
            'cli_alias': match.group(4),
            'exposure': match.group(5),
            'category': match.group(6),
            'handler': match.group(7),
            'flags': match.group(8).strip(),
        })
    return entries


def extract_ranges(path):
    """Extract category range constraints."""
    if not path.exists():
        return {}
    content = path.read_text(encoding='utf-8', errors='replace')
    ranges = {}
    for match in RANGE_PATTERN.finditer(content):
        name = match.group(1)
        value = int(match.group(2))
        ranges[name] = value
    return ranges


def validate(strict=False):
    """Run full validation. Returns (errors, warnings)."""
    errors = []
    warnings = []
    
    # ── Step 1: Extract IDM_* from Win32IDE.h and Win32IDE.cpp ──
    print("── Extracting IDM_* constants ──")
    idm_constants = extract_idm_constants(WIN32IDE_H, WIN32IDE_CPP)
    print(f"  Found {len(idm_constants)} IDM_* constants")
    
    # ── Step 2: Extract COMMAND_TABLE entries ──
    print("── Extracting COMMAND_TABLE entries ──")
    table_entries = extract_command_table(COMMAND_REGISTRY)
    print(f"  Found {len(table_entries)} entries in COMMAND_TABLE")
    
    if not table_entries:
        errors.append("COMMAND_TABLE is empty or unparseable")
        return errors, warnings
    
    # ── Step 3: Build lookup maps ──
    table_ids = {}  # id → entry
    table_symbols = {}  # symbol → entry
    table_cli = {}  # cli_alias → entry
    table_canonical = {}  # canonical → entry
    
    for entry in table_entries:
        eid = entry['id']
        if eid != 0:
            if eid in table_ids:
                errors.append(
                    f"DUPLICATE ID: {eid} used by both "
                    f"{table_ids[eid]['symbol']} and {entry['symbol']}")
            table_ids[eid] = entry
        
        sym = entry['symbol']
        if sym in table_symbols:
            errors.append(f"DUPLICATE SYMBOL: {sym}")
        table_symbols[sym] = entry
        
        cli = entry['cli_alias']
        if cli in table_cli:
            errors.append(
                f"DUPLICATE CLI ALIAS: '{cli}' used by both "
                f"{table_cli[cli]['symbol']} and {entry['symbol']}")
        table_cli[cli] = entry
        
        canon = entry['canonical']
        if canon in table_canonical:
            errors.append(
                f"DUPLICATE CANONICAL NAME: '{canon}' used by both "
                f"{table_canonical[canon]['symbol']} and {entry['symbol']}")
        table_canonical[canon] = entry
    
    # ── Step 4: Cross-reference IDM_* → COMMAND_TABLE ──
    print("── Cross-referencing IDM_* vs COMMAND_TABLE ──")
    
    # Filter out non-command IDM_* (theme end markers, base markers, etc.)
    skip_patterns = ['IDM_THEME_END', 'IDM_THEME_BASE']
    
    missing_in_table = []
    matched = []
    
    for idm_name, idm_value in sorted(idm_constants.items()):
        if any(idm_name.startswith(p) for p in skip_patterns):
            continue
        
        if idm_value in table_ids:
            matched.append((idm_name, idm_value, table_ids[idm_value]['symbol']))
        else:
            missing_in_table.append((idm_name, idm_value))
    
    if missing_in_table:
        print(f"\n  ⚠ {len(missing_in_table)} IDM_* NOT in COMMAND_TABLE:")
        for name, value in missing_in_table:
            msg = f"  MISSING: {name} = {value}"
            print(f"    {msg}")
            warnings.append(msg)
        
        if strict:
            for name, value in missing_in_table:
                errors.append(f"IDM not in COMMAND_TABLE: {name} = {value}")
    else:
        print("  ✓ All IDM_* constants are in COMMAND_TABLE")
    
    print(f"  Matched: {len(matched)} / {len(idm_constants)}")
    
    # ── Step 5: Validate ID ranges ──
    print("── Validating ID ranges ──")
    ranges = extract_ranges(COMMAND_RANGES)
    
    # Build category → (min, max) map
    range_map = {}
    for rname, rval in ranges.items():
        # Parse names like FILE_MIN, FILE_MAX, EDIT_MIN, EDIT_MAX
        parts = rname.rsplit('_', 1)
        if len(parts) == 2 and parts[1] in ('MIN', 'MAX'):
            cat_key = parts[0]
            if cat_key not in range_map:
                range_map[cat_key] = [0, 0]
            if parts[1] == 'MIN':
                range_map[cat_key][0] = rval
            else:
                range_map[cat_key][1] = rval
    
    # Map COMMAND_TABLE categories → range keys
    cat_to_range = {
        'File': 'FILE', 'Edit': 'EDIT', 'View': 'VIEW', 'Git': 'GIT',
        'Theme': 'THEME', 'Transparency': 'TRANS', 'Terminal': 'TERM',
        'Agent': 'AGENT', 'SubAgent': 'SUBAGENT', 'Autonomy': 'AUTO',
        'AIMode': 'AIMODE', 'AIContext': 'AICTX', 'ReverseEng': 'REVENG',
        'Backend': 'BACKEND', 'Router': 'ROUTER', 'LSP': 'LSP',
        'ASM': 'ASM', 'Hybrid': 'HYBRID', 'MultiResp': 'MULTI',
        'Governor': 'GOV', 'Safety': 'SAFETY', 'Replay': 'REPLAY',
        'Confidence': 'CONF', 'Swarm': 'SWARM', 'Debug': 'DBG',
        'Plugin': 'PLUGIN', 'Hotpatch': 'HOTPATCH', 'Monaco': 'MONACO',
        'LSPServer': 'LSPSRV', 'Editor': 'EDITOR', 'PDB': 'PDB',
        'Audit': 'AUDIT', 'Gauntlet': 'GAUNTLET', 'Voice': 'VOICE',
        'QW': 'QW', 'Telemetry': 'TELEM',
    }
    
    range_violations = 0
    for entry in table_entries:
        if entry['id'] == 0:
            continue  # CLI-only, skip range check
        
        cat = entry['category']
        rkey = cat_to_range.get(cat)
        if rkey and rkey in range_map:
            rmin, rmax = range_map[rkey]
            if entry['id'] < rmin or entry['id'] > rmax:
                msg = (f"RANGE VIOLATION: {entry['symbol']} (ID={entry['id']}) "
                       f"not in {cat} range [{rmin}, {rmax}]")
                errors.append(msg)
                range_violations += 1
    
    if range_violations == 0:
        print("  ✓ All IDs within category ranges")
    else:
        print(f"  ✗ {range_violations} range violations")
    
    # ── Step 6: Summary ──
    categories = defaultdict(int)
    gui_count = 0
    cli_count = 0
    both_count = 0
    
    for entry in table_entries:
        categories[entry['category']] += 1
        if entry['exposure'] == 'GUI_ONLY':
            gui_count += 1
        elif entry['exposure'] == 'CLI_ONLY':
            cli_count += 1
        elif entry['exposure'] == 'BOTH':
            both_count += 1
    
    print("\n── Registry Summary ──")
    print(f"  Total entries:    {len(table_entries)}")
    print(f"  GUI-only:         {gui_count}")
    print(f"  CLI-only:         {cli_count}")
    print(f"  Both GUI+CLI:     {both_count}")
    print(f"  Categories:       {len(categories)}")
    print()
    for cat, count in sorted(categories.items()):
        print(f"    {cat:20s} {count:3d} commands")
    
    return errors, warnings


def main():
    strict = '--strict' in sys.argv
    as_json = '--json' in sys.argv
    
    print("=" * 60)
    print("RawrXD Command Registry Validator")
    print("=" * 60)
    
    # Verify files exist
    for path, name in [(WIN32IDE_H, "Win32IDE.h"), 
                        (COMMAND_REGISTRY, "command_registry.hpp"),
                        (COMMAND_RANGES, "command_ranges.hpp")]:
        if not path.exists():
            print(f"ERROR: {name} not found at {path}")
            sys.exit(2)
    
    errors, warnings = validate(strict)
    
    if as_json:
        result = {
            'errors': errors,
            'warnings': warnings,
            'clean': len(errors) == 0,
        }
        print(json.dumps(result, indent=2))
    
    print()
    if errors:
        print(f"✗ FAILED: {len(errors)} error(s), {len(warnings)} warning(s)")
        for e in errors:
            print(f"  ERROR: {e}")
        sys.exit(1)
    elif warnings:
        print(f"⚠ PASSED with {len(warnings)} warning(s)")
        sys.exit(0)
    else:
        print("✓ CLEAN: Registry is complete and consistent")
        sys.exit(0)


if __name__ == '__main__':
    main()
