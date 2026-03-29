#!/usr/bin/env python3
# =============================================================================
# auto_register_commands.py — Automated GUI→CLI Command Registration Generator
# =============================================================================
# Architecture: Build-time source scanner + C++ code generator
# Purpose: Scans all IDM_* defines + handler declarations, cross-references
#          with existing feature_registration.cpp, generates auto_feature_registry.cpp
#          for ALL unregistered GUI commands (250+ entries).
#
# Run: python scripts/auto_register_commands.py [--src-root D:\rawrxd\src]
# Output: src/core/auto_feature_registry.cpp  (compiled into both RawrEngine + Win32IDE)
#         src/core/auto_feature_registry.hpp  (header with stub declarations)
#         reports/command_coverage_report.md   (audit report)
#
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

import re
import os
import json
import hashlib
from datetime import datetime
from dataclasses import dataclass
from typing import Dict, List, Set, Optional, Tuple

# =============================================================================
# CONFIGURATION
# =============================================================================

DEFAULT_SRC_ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")

# Files to scan for IDM_* defines
IDM_SCAN_FILES = [
    "win32app/Win32IDE.h",
    "win32app/Win32IDE.cpp",
    "win32app/Win32IDE_Commands.cpp",
    "win32app/Win32IDE_VoiceAutomation.cpp",
    "win32app/Win32IDE_DecompilerView.cpp",
    "core/shared_feature_dispatch.h",
    "ide_constants.h",
    "vscode_extension_api.h",
]

# Files containing handler declarations
HANDLER_SCAN_FILES = [
    "core/feature_handlers.h",
]

# Existing registration files (to detect what's already registered)
EXISTING_REG_FILES = [
    "core/feature_registration.cpp",
]

# Output paths (relative to src root)
OUTPUT_HEADER = "core/auto_feature_registry.hpp"
OUTPUT_CPP    = "core/auto_feature_registry.cpp"
OUTPUT_REPORT = "../reports/command_coverage_report.md"
OUTPUT_JSON   = "../reports/command_registry.json"

# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass
class IDMDefine:
    """Represents a #define IDM_* constant found in source."""
    name: str           # e.g., "IDM_ROUTER_ENABLE"
    value: int          # e.g., 5048
    file: str           # source file where defined
    line: int           # line number
    
    @property
    def category(self) -> str:
        """Extract category from IDM name: IDM_ROUTER_ENABLE -> ROUTER"""
        parts = self.name.replace("IDM_", "").split("_")
        return parts[0] if parts else "UNKNOWN"
    
    @property
    def action(self) -> str:
        """Extract action: IDM_ROUTER_ENABLE -> ENABLE"""
        parts = self.name.replace("IDM_", "").split("_", 1)
        return parts[1] if len(parts) > 1 else parts[0]

@dataclass
class HandlerDecl:
    """Represents a handler function declaration."""
    name: str           # e.g., "handleRouterEnable"
    file: str
    line: int

@dataclass  
class RegisteredFeature:
    """Represents an already-registered feature."""
    feature_id: str     # e.g., "router.enable"
    command_id: int     # IDM_* value or 0
    cli_command: str    # "!router_enable"
    handler: str        # "handleRouterEnable"

@dataclass
class AutoRegistration:
    """Generated registration entry."""
    feature_id: str
    name: str
    description: str
    group: str          # FeatureGroup enum value
    command_id: str     # IDM_* define name (or 0)
    command_value: int
    cli_command: str
    handler: str
    gui_supported: bool
    cli_supported: bool
    asm_hot_path: bool
    source_idm: Optional[IDMDefine] = None
    has_existing_handler: bool = False

# =============================================================================
# CATEGORY → FeatureGroup MAPPING
# =============================================================================

CATEGORY_TO_GROUP = {
    # Direct mappings
    "FILE": "FeatureGroup::FileOps",
    "EDIT": "FeatureGroup::Editing",
    "VIEW": "FeatureGroup::View",
    "TERMINAL": "FeatureGroup::Terminal",
    "AGENT": "FeatureGroup::Agent",
    "SUBAGENT": "FeatureGroup::SubAgent",
    "AUTONOMY": "FeatureGroup::Autonomy",
    "AI": "FeatureGroup::AIMode",
    "REVENG": "FeatureGroup::ReverseEng",
    "DECOMP": "FeatureGroup::Decompiler",
    "DEBUG": "FeatureGroup::Debug",
    "DBG": "FeatureGroup::Debug",
    "BREAKPOINT": "FeatureGroup::Debug",
    "HOTPATCH": "FeatureGroup::Hotpatch",
    "VOICE": "FeatureGroup::Voice",
    "THEME": "FeatureGroup::Themes",
    "TRANSPARENCY": "FeatureGroup::Themes",
    "BACKEND": "FeatureGroup::LLMRouter",
    "ROUTER": "FeatureGroup::LLMRouter",
    "LSP": "FeatureGroup::LSP",
    "HYBRID": "FeatureGroup::LSP",
    "ASM": "FeatureGroup::ReverseEng",
    "SWARM": "FeatureGroup::Swarm",
    "MULTI": "FeatureGroup::Tools",
    "GOV": "FeatureGroup::Tools",
    "SAFETY": "FeatureGroup::Security",
    "REPLAY": "FeatureGroup::Tools",
    "CONFIDENCE": "FeatureGroup::Tools",
    "PLUGIN": "FeatureGroup::Tools",
    "PDB": "FeatureGroup::Debug",
    "TELEMETRY": "FeatureGroup::Performance",
    "AUDIT": "FeatureGroup::Security",
    "GAUNTLET": "FeatureGroup::Tools",
    "QW": "FeatureGroup::Tools",
    "VSCEXT": "FeatureGroup::Tools",
    "GIT": "FeatureGroup::Git",
    "SERVER": "FeatureGroup::Server",
    "SETTINGS": "FeatureGroup::Settings",
    "HELP": "FeatureGroup::Help",
    "TOOLS": "FeatureGroup::Tools",
    "MODULES": "FeatureGroup::Modules",
    "EDITOR": "FeatureGroup::Editing",
    "MONACO": "FeatureGroup::View",
}

# Categories with known MASM hot-paths 
ASM_HOT_PATH_CATEGORIES = {"HOTPATCH", "ASM", "REVENG", "DECOMP"}

# =============================================================================
# HANDLER NAME → IDM NAME MAPPING HEURISTIC
# =============================================================================

def idm_to_handler_name(idm_name: str) -> str:
    """Convert IDM_ROUTER_ENABLE → handleRouterEnable"""
    parts = idm_name.replace("IDM_", "").split("_")
    camel = "".join(p.capitalize() for p in parts)
    return f"handle{camel}"

def idm_to_feature_id(idm_name: str) -> str:
    """Convert IDM_ROUTER_ENABLE → router.enable"""
    parts = idm_name.replace("IDM_", "").lower().split("_", 1)
    if len(parts) == 2:
        return f"{parts[0]}.{parts[1]}"
    return parts[0]

def idm_to_cli_command(idm_name: str) -> str:
    """Convert IDM_ROUTER_ENABLE → !router_enable"""
    action = idm_name.replace("IDM_", "").lower()
    return f"!{action}"

def idm_to_display_name(idm_name: str) -> str:
    """Convert IDM_ROUTER_ENABLE → Router Enable"""
    parts = idm_name.replace("IDM_", "").split("_")
    return " ".join(p.capitalize() for p in parts)

def idm_to_description(idm_name: str) -> str:
    """Generate a description from IDM name."""
    category = idm_name.replace("IDM_", "").split("_")[0].lower()
    action = "_".join(idm_name.replace("IDM_", "").split("_")[1:]).lower().replace("_", " ")
    if not action:
        action = category
    return f"{action.capitalize()} ({category} system)"

# =============================================================================
# SCANNER: Parse source files for IDM_* defines
# =============================================================================

def scan_idm_defines(src_root: str) -> Dict[str, IDMDefine]:
    """Scan all source files for #define IDM_* patterns."""
    idm_defines = {}
    pattern = re.compile(r'^\s*#define\s+(IDM_\w+)\s+(\d+|0x[0-9a-fA-F]+)', re.MULTILINE)
    
    # Scan specified files
    for rel_path in IDM_SCAN_FILES:
        full_path = os.path.join(src_root, rel_path)
        if not os.path.exists(full_path):
            continue
        with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        for match in pattern.finditer(content):
            name = match.group(1)
            val_str = match.group(2)
            value = int(val_str, 0)  # handles both decimal and 0x hex
            line = content[:match.start()].count('\n') + 1
            if name not in idm_defines:
                idm_defines[name] = IDMDefine(name=name, value=value, file=rel_path, line=line)
    
    # Also scan ALL .h and .cpp files under src/ for any IDM_* we missed
    # EXCLUDE auto-generated files to prevent circular scanning
    EXCLUDE_FILES = {"auto_feature_registry.cpp", "auto_feature_registry.hpp"}
    for dirpath, _, filenames in os.walk(src_root):
        for fname in filenames:
            if not (fname.endswith('.h') or fname.endswith('.hpp') or fname.endswith('.cpp')):
                continue
            if fname in EXCLUDE_FILES:
                continue  # skip auto-generated output
            full_path = os.path.join(dirpath, fname)
            rel_path = os.path.relpath(full_path, src_root)
            if rel_path in IDM_SCAN_FILES:
                continue  # already scanned
            try:
                with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
            except:
                continue
            for match in pattern.finditer(content):
                name = match.group(1)
                val_str = match.group(2)
                value = int(val_str, 0)
                line = content[:match.start()].count('\n') + 1
                if name not in idm_defines:
                    idm_defines[name] = IDMDefine(name=name, value=value, file=rel_path, line=line)
    
    return idm_defines

# =============================================================================
# SCANNER: Parse handler declarations
# =============================================================================

def scan_handler_declarations(src_root: str) -> Dict[str, HandlerDecl]:
    """Scan headers and source files for handler declarations/definitions."""
    handlers: Dict[str, HandlerDecl] = {}
    # Match both declarations and definitions:
    #   CommandResult handleFoo(...);
    #   CommandResult handleFoo(...) { ... }
    pattern = re.compile(r'\bCommandResult\s+(handle\w+)\s*\(')

    scan_files: List[str] = []

    # Keep explicit scan files first for stable source attribution.
    for rel_path in HANDLER_SCAN_FILES:
        full_path = os.path.join(src_root, rel_path)
        if os.path.exists(full_path):
            scan_files.append(full_path)

    # Also scan the whole source tree for real handler definitions.
    for root, _, files in os.walk(src_root):
        for filename in files:
            if not (filename.endswith(".cpp") or filename.endswith(".h") or filename.endswith(".hpp")):
                continue
            full_path = os.path.join(root, filename)
            if full_path not in scan_files:
                scan_files.append(full_path)

    for full_path in scan_files:
        rel_path = os.path.relpath(full_path, src_root).replace("\\", "/")
        with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        for match in pattern.finditer(content):
            name = match.group(1)
            line = content[:match.start()].count('\n') + 1
            handlers[name] = HandlerDecl(name=name, file=rel_path, line=line)

    return handlers

# =============================================================================
# SCANNER: Parse existing registrations
# =============================================================================

def scan_existing_registrations(src_root: str) -> Tuple[Set[int], Set[str], Set[str]]:
    """Scan feature_registration.cpp to find already-registered command IDs, feature IDs, and CLI commands."""
    registered_cmd_ids = set()
    registered_feature_ids = set()
    registered_cli_cmds = set()
    
    # Pattern for reg() calls
    reg_pattern = re.compile(r'reg\(\s*"([^"]+)"')
    # Pattern for command ID references (IDM_*_R defines mapped to actual values)
    idm_r_pattern = re.compile(r'#define\s+(IDM_\w+_R)\s+(\d+|0x[0-9a-fA-F]+)')
    # Pattern for CLI command in reg() calls
    cli_pattern = re.compile(r'reg\([^)]*"([!][^"]+)"')
    
    for rel_path in EXISTING_REG_FILES:
        full_path = os.path.join(src_root, rel_path)
        if not os.path.exists(full_path):
            continue
        with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        
        # Extract all _R define values (these are the actual command IDs used)
        for match in idm_r_pattern.finditer(content):
            val_str = match.group(2)
            val = int(val_str, 0)
            if val != 0:
                registered_cmd_ids.add(val)
        
        # Extract feature IDs from reg() first argument
        for match in reg_pattern.finditer(content):
            registered_feature_ids.add(match.group(1))
        
        # Extract CLI commands
        for match in cli_pattern.finditer(content):
            registered_cli_cmds.add(match.group(1))
    
    return registered_cmd_ids, registered_feature_ids, registered_cli_cmds

# =============================================================================
# GENERATOR: Produce auto-registration entries
# =============================================================================

# IDM_* defines to SKIP (internal markers, range boundaries, not actual commands)
SKIP_PATTERNS = {
    "IDM_THEME_BASE", "IDM_THEME_END",  # Range markers, not commands
}
SKIP_SUFFIXES = {"_BASE", "_END", "_FIRST", "_LAST", "_COUNT", "_MAX", "_MIN"}

def should_skip_idm(name: str) -> bool:
    """Determine if an IDM define is a range marker/internal, not an actual command."""
    if name in SKIP_PATTERNS:
        return True
    for suffix in SKIP_SUFFIXES:
        if name.endswith(suffix) and name != "IDM_AI_MODE_MAX":  # MAX is a real command
            return True
    return False

def generate_registrations(
    all_idm: Dict[str, IDMDefine],
    all_handlers: Dict[str, HandlerDecl],
    registered_ids: Set[int],
    registered_features: Set[str],
    registered_cli: Set[str]
) -> List[AutoRegistration]:
    """Generate AutoRegistration entries for all unregistered IDM_* commands."""
    registrations = []
    
    for idm_name, idm_def in sorted(all_idm.items(), key=lambda x: x[1].value):
        # Skip internal/range markers
        if should_skip_idm(idm_name):
            continue
        
        # Skip if already registered by value
        if idm_def.value in registered_ids:
            continue
        
        # Skip value 0 (CLI-only placeholder defines from feature_registration.cpp)
        if idm_def.value == 0:
            continue
        
        # Skip _R defines (those are registration aliases, not source of truth)
        if idm_name.endswith("_R"):
            continue
        
        # Generate registration data
        feature_id = idm_to_feature_id(idm_name)
        
        # Skip if feature ID already registered
        if feature_id in registered_features:
            continue
        
        handler_name = idm_to_handler_name(idm_name)
        cli_cmd = idm_to_cli_command(idm_name)
        
        # Check if CLI command already exists
        if cli_cmd in registered_cli:
            continue
        
        # Determine category and group
        category = idm_def.category
        group = CATEGORY_TO_GROUP.get(category, "FeatureGroup::Tools")
        
        # Prefer explicit handler symbols when available; missing ones are emitted
        # through the generated stub path in auto_feature_registry.
        has_handler = handler_name in all_handlers
        
        # Check for MASM hot-path
        asm_hot = category in ASM_HOT_PATH_CATEGORIES
        
        reg = AutoRegistration(
            feature_id=feature_id,
            name=idm_to_display_name(idm_name),
            description=idm_to_description(idm_name),
            group=group,
            command_id=idm_name,
            command_value=idm_def.value,
            cli_command=cli_cmd,
            handler=handler_name,
            gui_supported=True,
            cli_supported=True,
            asm_hot_path=asm_hot,
            source_idm=idm_def,
            has_existing_handler=has_handler,
        )
        registrations.append(reg)
    
    return registrations

# =============================================================================
# CODE GENERATOR: Produce C++ header
# =============================================================================

def generate_header(registrations: List[AutoRegistration], all_handlers: Dict[str, HandlerDecl]) -> str:
    """Generate auto_feature_registry.hpp with handler declarations."""

    # Declare every handler referenced by generated registration code so the
    # translation unit compiles even when declarations live outside feature_handlers.h.
    all_declared_handlers = sorted(set(reg.handler for reg in registrations))
    new_handlers = set()
    for reg in registrations:
        if not reg.has_existing_handler:
            new_handlers.add(reg.handler)
    
    header = f"""\
// ============================================================================
// auto_feature_registry.hpp — AUTO-GENERATED Command Registration Header
// ============================================================================
// Generated by: scripts/auto_register_commands.py
// Generated at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
// Total new registrations: {len(registrations)}
// New stub handlers: {len(new_handlers)}
//
// Architecture: C++20, Win32, no Qt, no exceptions
// This file is auto-generated. Do NOT edit manually.
// Re-run: python scripts/auto_register_commands.py
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_AUTO_FEATURE_REGISTRY_HPP
#define RAWRXD_AUTO_FEATURE_REGISTRY_HPP

#include "shared_feature_dispatch.h"

// ============================================================================
// INITIALIZATION — Call once at startup (after FeatureRegistrar static init)
// ============================================================================

void initAutoFeatureRegistry();

// ============================================================================
// COVERAGE QUERY — Runtime introspection
// ============================================================================

struct RegistryCoverage {{
    size_t totalIdmDefines;       // All IDM_* found in source
    size_t registeredBefore;      // Already in feature_registration.cpp
    size_t autoRegistered;        // Added by this auto-registration
    size_t totalAfter;            // Grand total
    size_t stubHandlers;          // Handlers using generic stub
    size_t realHandlers;          // Handlers with real implementation
    float  coveragePercent;       // totalAfter / totalIdmDefines * 100
}};

RegistryCoverage getRegistryCoverage();
const char* getRegistryVersionHash();

"""
    
    if all_declared_handlers:
        header += "// ============================================================================\n"
        header += f"// HANDLER DECLARATIONS — {len(all_declared_handlers)} referenced handlers\n"
        header += "// ============================================================================\n"
        header += "// Declarations are emitted here so generated registration code can reference\n"
        header += "// handlers regardless of where their definitions are implemented.\n"
        header += "// ============================================================================\n\n"

        for handler in all_declared_handlers:
            header += f"CommandResult {handler}(const CommandContext& ctx);\n"
        header += "\n"
    
    header += "#endif // RAWRXD_AUTO_FEATURE_REGISTRY_HPP\n"
    return header

# =============================================================================
# CODE GENERATOR: Produce C++ implementation
# =============================================================================

def generate_cpp(registrations: List[AutoRegistration], all_handlers: Dict[str, HandlerDecl],
                 all_idm: Dict[str, IDMDefine], registered_ids: Set[int]) -> str:
    """Generate auto_feature_registry.cpp with all registrations + stub handlers."""
    
    # Separate registrations by whether they have existing handlers
    with_handler = [r for r in registrations if r.has_existing_handler]
    without_handler = [r for r in registrations if not r.has_existing_handler]
    new_handler_names = sorted(set(r.handler for r in without_handler))
    
    # Compute content hash for version tracking
    content_hash = hashlib.md5(
        json.dumps([r.feature_id for r in registrations]).encode()
    ).hexdigest()[:12]
    
    cpp = f"""\
// ============================================================================
// auto_feature_registry.cpp — AUTO-GENERATED Command Registration
// ============================================================================
// Generated by: scripts/auto_register_commands.py
// Generated at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
// Content hash: {content_hash}
// Total auto-registrations: {len(registrations)}
//   With existing handler: {len(with_handler)}
//   With stub handler: {len(without_handler)}
//
// Architecture: C++20, Win32, no Qt, no exceptions
// This file is auto-generated. Re-run the script to update.
// DO NOT edit manually — changes will be overwritten.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "auto_feature_registry.hpp"
#include "feature_handlers.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// IDM_* DEFINES — Canonical values from Win32IDE.h and related headers
// ============================================================================
// These are included directly to avoid header dependency on Win32IDE.h
// in the CLI build target.
// ============================================================================

"""
    
    # Emit IDM defines for all registrations
    seen_defines = set()
    for reg in registrations:
        if reg.command_id not in seen_defines and reg.command_value != 0:
            cpp += f"#define {reg.command_id:<45s} {reg.command_value}\n"
            seen_defines.add(reg.command_id)
    
    cpp += "\n"
    
    # ── Stub handler implementations ──
    if new_handler_names:
        cpp += "// ============================================================================\n"
        cpp += f"// STUB HANDLERS — {len(new_handler_names)} auto-generated command handlers\n"
        cpp += "// ============================================================================\n"
        cpp += "// Each stub reports the command via ctx.output() and returns OK.\n"
        cpp += "// Replace with real implementations as features are built.\n"
        cpp += "// The stub pattern allows full CLI dispatch even before real code exists.\n"
        cpp += "// ============================================================================\n\n"
        
        # Generic stub helper
        cpp += """\
static CommandResult autoStub(const CommandContext& ctx, const char* cmdName, const char* featureId) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[AutoReg] %s: %s (args: %s)\\n",
             cmdName, featureId, ctx.args ? ctx.args : "(none)");
    ctx.output(buf);
    
    if (ctx.isGui) {
        // GUI: Show in status bar or output panel
        snprintf(buf, sizeof(buf), "Command '%s' registered but not yet implemented", cmdName);
        ctx.output(buf);
    }
    return CommandResult::ok("stub");
}

"""
        
        for handler in new_handler_names:
            # Reverse-engineer feature ID from handler name for diagnostic
            # handleRouterEnable → "router.enable"
            stripped = handler.replace("handle", "")
            parts = re.findall(r'[A-Z][a-z0-9]*', stripped) if stripped else [stripped]
            feat_id = ".".join(p.lower() for p in parts[:2]) if len(parts) >= 2 else stripped.lower()
            
            cpp += f'CommandResult {handler}(const CommandContext& ctx) {{\n'
            cpp += f'    return autoStub(ctx, "{handler}", "{feat_id}");\n'
            cpp += f'}}\n\n'
    
    # ── Registration function ──
    cpp += "// ============================================================================\n"
    cpp += f"// AUTO-REGISTRATION — {len(registrations)} commands\n"
    cpp += "// ============================================================================\n\n"
    
    cpp += """\
static void autoReg(const char* id, const char* name, const char* desc,
                    FeatureGroup group, uint32_t cmdId,
                    const char* cliCmd, const char* shortcut,
                    FeatureHandler handler, bool gui, bool cli, bool asmFast = false) {
    FeatureDescriptor fd{};
    fd.id = id;
    fd.name = name;
    fd.description = desc;
    fd.group = group;
    fd.commandId = cmdId;
    fd.cliCommand = cliCmd;
    fd.shortcut = shortcut;
    fd.handler = handler;
    fd.guiSupported = gui;
    fd.cliSupported = cli;
    fd.asmHotPath = asmFast;
    SharedFeatureRegistry::instance().registerFeature(fd);
}

"""
    
    cpp += "void initAutoFeatureRegistry() {\n"
    cpp += f"    // Auto-registering {len(registrations)} commands...\n\n"
    
    # Group registrations by category for readability
    by_group: Dict[str, List[AutoRegistration]] = {}
    for reg in registrations:
        cat = reg.source_idm.category if reg.source_idm else "MISC"
        by_group.setdefault(cat, []).append(reg)
    
    for cat in sorted(by_group.keys()):
        regs = by_group[cat]
        cpp += f"    // ══════════════ {cat} ({len(regs)} commands) ══════════════\n"
        for reg in regs:
            asm_str = "true" if reg.asm_hot_path else "false"
            gui_str = "true" if reg.gui_supported else "false"
            cli_str = "true" if reg.cli_supported else "false"
            cpp += f'    autoReg("{reg.feature_id}", "{reg.name}", "{reg.description}",\n'
            cpp += f'        {reg.group}, {reg.command_id}, "{reg.cli_command}", "",\n'
            cpp += f'        {reg.handler}, {gui_str}, {cli_str}, {asm_str});\n'
        cpp += "\n"
    
    cpp += "}\n\n"
    
    # ── Coverage query implementation ──
    cpp += "// ============================================================================\n"
    cpp += "// COVERAGE INTROSPECTION\n"
    cpp += "// ============================================================================\n\n"
    
    total_idm = len(all_idm)
    already_registered = len(registered_ids)
    auto_count = len(registrations)
    
    cpp += f"""\
static const char* s_registryVersionHash = "{content_hash}";

RegistryCoverage getRegistryCoverage() {{
    RegistryCoverage cov{{}};
    cov.totalIdmDefines = {total_idm};
    cov.registeredBefore = {already_registered};
    cov.autoRegistered = {auto_count};
    cov.totalAfter = SharedFeatureRegistry::instance().totalRegistered();
    cov.stubHandlers = {len(new_handler_names)};
    cov.realHandlers = {len(with_handler)};
    cov.coveragePercent = (cov.totalAfter > 0 && cov.totalIdmDefines > 0)
        ? (float(cov.totalAfter) / float(cov.totalIdmDefines)) * 100.0f
        : 0.0f;
    return cov;
}}

const char* getRegistryVersionHash() {{
    return s_registryVersionHash;
}}
"""
    
    return cpp

# =============================================================================
# REPORT GENERATOR: Markdown coverage report
# =============================================================================

def generate_report(
    all_idm: Dict[str, IDMDefine],
    all_handlers: Dict[str, HandlerDecl],
    registered_ids: Set[int],
    registrations: List[AutoRegistration]
) -> str:
    """Generate comprehensive coverage report."""
    
    report = f"""\
# RawrXD Command Registry Coverage Report

Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

## Summary

| Metric | Count |
|---|---|
| Total IDM_* defines found | {len(all_idm)} |
| Already registered (manual) | {len(registered_ids)} |
| Auto-registered (this run) | {len(registrations)} |
| **Total after auto-reg** | **{len(registered_ids) + len(registrations)}** |
| Handler declarations found | {len(all_handlers)} |
| With existing handler | {len([r for r in registrations if r.has_existing_handler])} |
| With stub handler | {len([r for r in registrations if not r.has_existing_handler])} |
| Coverage | {((len(registered_ids) + len(registrations)) / max(len(all_idm), 1)) * 100:.1f}% |

## Auto-Registered Commands

"""
    
    # Group by category
    by_group: Dict[str, List[AutoRegistration]] = {}
    for reg in registrations:
        cat = reg.source_idm.category if reg.source_idm else "MISC"
        by_group.setdefault(cat, []).append(reg)
    
    for cat in sorted(by_group.keys()):
        regs = by_group[cat]
        report += f"### {cat} ({len(regs)} commands)\n\n"
        report += "| Feature ID | CLI Command | IDM_* | Value | Handler | Status |\n"
        report += "|---|---|---|---|---|---|\n"
        for reg in regs:
            status = "REAL" if reg.has_existing_handler else "STUB"
            report += f"| {reg.feature_id} | `{reg.cli_command}` | {reg.command_id} | {reg.command_value} | {reg.handler} | {status} |\n"
        report += "\n"
    
    # Remaining unregistered (if any)
    auto_values = {r.command_value for r in registrations}
    still_unregistered = []
    for name, idm in sorted(all_idm.items(), key=lambda x: x[1].value):
        if idm.value == 0:
            continue
        if idm.value in registered_ids:
            continue
        if idm.value in auto_values:
            continue
        if should_skip_idm(name) or name.endswith("_R"):
            continue
        still_unregistered.append(idm)
    
    if still_unregistered:
        report += f"## Still Unregistered ({len(still_unregistered)} commands)\n\n"
        report += "These were skipped due to duplicate feature IDs, CLI command collisions, or filter rules:\n\n"
        report += "| IDM_* | Value | Source File | Reason |\n"
        report += "|---|---|---|---|\n"
        for idm in still_unregistered:
            report += f"| {idm.name} | {idm.value} | {idm.file} | Filtered |\n"
        report += "\n"
    
    report += "## How to Update\n\n"
    report += "1. Add new `IDM_*` defines in `Win32IDE.h` (or any scanned header)\n"
    report += "2. Optionally add handler declarations in `feature_handlers.h`\n"
    report += "3. Run: `python scripts/auto_register_commands.py`\n"
    report += "4. The system automatically generates registrations + stubs\n"
    report += "5. Replace stubs with real handlers as you implement features\n"
    report += "6. CMake pre-build step does this automatically on every build\n"
    
    return report

# =============================================================================
# JSON MANIFEST
# =============================================================================

def generate_json_manifest(registrations: List[AutoRegistration], registered_ids: Set[int]) -> str:
    """Generate JSON manifest of all auto-registered commands."""
    entries = []
    for reg in registrations:
        entries.append({
            "featureId": reg.feature_id,
            "name": reg.name,
            "description": reg.description,
            "group": reg.group,
            "commandId": reg.command_value,
            "idmDefine": reg.command_id,
            "cliCommand": reg.cli_command,
            "handler": reg.handler,
            "guiSupported": reg.gui_supported,
            "cliSupported": reg.cli_supported,
            "asmHotPath": reg.asm_hot_path,
            "hasRealHandler": reg.has_existing_handler,
        })
    
    manifest = {
        "generated": datetime.now().isoformat(),
        "totalAutoRegistered": len(registrations),
        "totalManualRegistered": len(registered_ids),
        "entries": entries,
    }
    return json.dumps(manifest, indent=2)

# =============================================================================
# MAIN
# =============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Auto-generate CLI registrations for GUI-only commands")
    parser.add_argument("--src-root", default=DEFAULT_SRC_ROOT, help="Source root directory")
    parser.add_argument("--dry-run", action="store_true", help="Print stats without writing files")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()
    
    src_root = os.path.abspath(args.src_root)
    print(f"[AutoReg] Source root: {src_root}")
    
    # Phase 1: Scan
    print("[AutoReg] Phase 1: Scanning IDM_* defines...")
    all_idm = scan_idm_defines(src_root)
    print(f"  Found {len(all_idm)} IDM_* defines")
    
    print("[AutoReg] Phase 1: Scanning handler declarations...")
    all_handlers = scan_handler_declarations(src_root)
    print(f"  Found {len(all_handlers)} handler declarations")
    
    print("[AutoReg] Phase 1: Scanning existing registrations...")
    registered_ids, registered_features, registered_cli = scan_existing_registrations(src_root)
    print(f"  Found {len(registered_ids)} registered command IDs")
    print(f"  Found {len(registered_features)} registered feature IDs")
    
    # Phase 2: Generate
    print("[AutoReg] Phase 2: Generating auto-registrations...")
    registrations = generate_registrations(
        all_idm, all_handlers, registered_ids, registered_features, registered_cli
    )
    print(f"  Generated {len(registrations)} new registrations")
    print(f"    With existing handler: {len([r for r in registrations if r.has_existing_handler])}")
    print(f"    With stub handler: {len([r for r in registrations if not r.has_existing_handler])}")
    
    if args.verbose:
        for reg in registrations:
            status = "REAL" if reg.has_existing_handler else "STUB"
            print(f"    [{status}] {reg.feature_id} → {reg.cli_command} (IDM={reg.command_value})")
    
    if args.dry_run:
        total = len(registered_ids) + len(registrations)
        coverage = (total / max(len(all_idm), 1)) * 100
        print(f"\n[AutoReg] DRY RUN — Would register {len(registrations)} commands")
        print(f"  Coverage: {total}/{len(all_idm)} = {coverage:.1f}%")
        return
    
    # Phase 3: Write output files
    print("[AutoReg] Phase 3: Writing output files...")
    
    # Header
    header_path = os.path.join(src_root, OUTPUT_HEADER)
    header_content = generate_header(registrations, all_handlers)
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    print(f"  Wrote: {header_path} ({len(header_content)} bytes)")
    
    # C++ implementation
    cpp_path = os.path.join(src_root, OUTPUT_CPP)
    cpp_content = generate_cpp(registrations, all_handlers, all_idm, registered_ids)
    with open(cpp_path, 'w', encoding='utf-8') as f:
        f.write(cpp_content)
    print(f"  Wrote: {cpp_path} ({len(cpp_content)} bytes)")
    
    # Reports directory
    report_dir = os.path.join(src_root, "..", "reports")
    os.makedirs(report_dir, exist_ok=True)
    
    # Markdown report
    report_path = os.path.join(src_root, OUTPUT_REPORT)
    report_content = generate_report(all_idm, all_handlers, registered_ids, registrations)
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(report_content)
    print(f"  Wrote: {report_path}")
    
    # JSON manifest
    json_path = os.path.join(src_root, OUTPUT_JSON)
    json_content = generate_json_manifest(registrations, registered_ids)
    with open(json_path, 'w', encoding='utf-8') as f:
        f.write(json_content)
    print(f"  Wrote: {json_path}")
    
    # Summary
    total = len(registered_ids) + len(registrations)
    coverage = (total / max(len(all_idm), 1)) * 100
    print(f"\n[AutoReg] COMPLETE")
    print(f"  Manual registrations:  {len(registered_ids)}")
    print(f"  Auto registrations:    {len(registrations)}")
    print(f"  Total features:        {total}")
    print(f"  Coverage:              {coverage:.1f}%")
    print(f"  Version hash:          {hashlib.md5(json.dumps([r.feature_id for r in registrations]).encode()).hexdigest()[:12]}")

if __name__ == "__main__":
    main()
