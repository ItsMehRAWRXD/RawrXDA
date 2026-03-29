#!/usr/bin/env python3
"""
check_agent_parity.py

Enforces parity between CLI/completion server and GUI/local server agent surfaces.
Also enforces strict zero macro wrappers in active first-party IDE sources.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def extract_pairs(text: str, anchor: str) -> set[tuple[str, str]]:
    i = text.find(anchor)
    if i < 0:
        return set()
    j = text.find("};", i)
    if j < 0:
        return set()
    block = text[i:j]
    return set(re.findall(r'\{"([^"]+)",\s*"([^"]+)"\}', block))


def extract_routes(text: str) -> set[str]:
    m = re.search(r'R"\((\{"success":true,"surface":"[^"]+".*?\})\)"', text, re.S)
    if not m:
        return set()
    try:
        payload = json.loads(m.group(1))
        routes = payload.get("routes", {})
        if isinstance(routes, dict):
            return set(routes.keys())
        return set()
    except Exception:
        return set()


def extract_cli_routes(text: str) -> set[str]:
    marker = "agent-capabilities"
    i = text.find(marker)
    if i < 0:
        return set()
    m = re.search(r'R"\((\{"success":true,"surface":"cli_shell".*?\})\)"', text[i:], re.S)
    if not m:
        return set()
    try:
        payload = json.loads(m.group(1))
        routes = payload.get("routes", {})
        if isinstance(routes, dict):
            return set(routes.keys())
        return set()
    except Exception:
        return set()

def extract_cli_family_mappings(text: str) -> set[tuple[str, str]]:
    return extract_pairs(text, 'out["familyMappings"] = {')


def count_wrappers(core_dir: Path) -> dict[str, int]:
    patterns = {
        "DEFINE_AF_STUB(": 0,
        "DEFINE_AF_STUB_CS(": 0,
        "DEFINE_AUTO_MISSING_HANDLER(": 0,
        "DEFINE_LINK_GAP_HANDLER(": 0,
        "WIN32IDE_MISSING_HANDLER(": 0,
    }
    for path in core_dir.rglob("*"):
        if path.suffix.lower() not in {".cpp", ".h", ".hpp"}:
            continue
        if path.name == "agent_capability_audit.hpp":
            continue
        text = read_text(path)
        for key in patterns:
            patterns[key] += text.count(key)
    return patterns


def count_global_wrapper_like_macros(src_dir: Path) -> dict[str, int]:
    include_ext = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}
    excluded_prefixes = {
        "ggml-cpu/",
        "3rdparty/",
        "vendor/",
        "external/",
    }
    excluded_files = set()
    patterns = {
        "STUB": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*STUB[A-Za-z0-9_]*\s*\(", re.M),
        "WRAP": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*WRAP(?:PER)?[A-Za-z0-9_]*\s*\(", re.M),
        "HANDLER": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*HANDLER[A-Za-z0-9_]*\s*\(", re.M),
        "DISPATCH": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*DISPATCH[A-Za-z0-9_]*\s*\(", re.M),
        "ROUTE": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*ROUTE[A-Za-z0-9_]*\s*\(", re.M),
        "GAP": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*GAP[A-Za-z0-9_]*\s*\(", re.M),
    }
    counts = {k: 0 for k in patterns}
    for path in src_dir.rglob("*"):
        if path.suffix.lower() not in include_ext:
            continue
        rel = path.relative_to(src_dir).as_posix()
        if any(rel.startswith(prefix) for prefix in excluded_prefixes):
            continue
        if rel in excluded_files:
            continue
        text = read_text(path)
        for key, regex in patterns.items():
            counts[key] += len(regex.findall(text))
    return counts


def count_global_wrapper_like_macros_anywhere(src_dir: Path) -> dict[str, int]:
    include_ext = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}
    patterns = {
        "STUB": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*STUB[A-Za-z0-9_]*\s*\(", re.M),
        "WRAP": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*WRAP(?:PER)?[A-Za-z0-9_]*\s*\(", re.M),
        "HANDLER": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*HANDLER[A-Za-z0-9_]*\s*\(", re.M),
        "DISPATCH": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*DISPATCH[A-Za-z0-9_]*\s*\(", re.M),
        "ROUTE": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*ROUTE[A-Za-z0-9_]*\s*\(", re.M),
        "GAP": re.compile(r"^\s*#define\s+[A-Za-z_][A-Za-z0-9_]*GAP[A-Za-z0-9_]*\s*\(", re.M),
    }
    counts = {k: 0 for k in patterns}
    for path in src_dir.rglob("*"):
        if path.suffix.lower() not in include_ext:
            continue
        text = read_text(path)
        for key, regex in patterns.items():
            counts[key] += len(regex.findall(text))
    return counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default="D:/rawrxd")
    parser.add_argument("--strict-anywhere", action="store_true",
                        help="Fail if wrapper-like macros exist anywhere in src/, including third-party/vendor code.")
    args = parser.parse_args()

    root = Path(args.repo_root)
    complete = root / "src" / "complete_server.cpp"
    local = root / "src" / "win32app" / "Win32IDE_LocalServer.cpp"
    cli = root / "src" / "rawrxd_cli.cpp"
    core_dir = root / "src" / "core"
    src_dir = root / "src"
    registry = root / "reports" / "command_registry.json"

    complete_text = read_text(complete)
    local_text = read_text(local)
    cli_text = read_text(cli)

    aliases_complete = extract_pairs(complete_text, 'payload["aliases"] = {')
    aliases_local = extract_pairs(local_text, 'payload["aliases"] = {')

    families_complete = extract_pairs(complete_text, 'out["familyMappings"] = {')
    families_local = extract_pairs(local_text, 'out["familyMappings"] = {')

    routes_complete = extract_routes(complete_text)
    routes_local = extract_routes(local_text)
    routes_cli = extract_cli_routes(cli_text)
    families_cli = extract_cli_family_mappings(cli_text)

    wrappers = count_wrappers(core_dir)
    wrapper_total = sum(wrappers.values())
    global_wrappers = count_global_wrapper_like_macros(src_dir)
    global_wrapper_total = sum(global_wrappers.values())
    literal_wrappers = count_global_wrapper_like_macros_anywhere(src_dir)
    literal_wrapper_total = sum(literal_wrappers.values())

    failures: list[str] = []
    registry_stats = {
        "total": 0,
        "hasRealHandlerFalse": 0,
        "guiSupportedFalse": 0,
        "cliSupportedFalse": 0,
        "missingHandlerName": 0,
        "missingCliCommand": 0,
        "agentEntries": 0,
        "nonAgentEntries": 0,
        "agentHasRealHandlerFalse": 0,
        "nonAgentHasRealHandlerFalse": 0,
        "agentGuiSupportedFalse": 0,
        "nonAgentGuiSupportedFalse": 0,
        "agentCliSupportedFalse": 0,
        "nonAgentCliSupportedFalse": 0,
    }

    if aliases_complete != aliases_local:
        failures.append("aliases mismatch")
        failures.append(f"  only in completion: {sorted(aliases_complete - aliases_local)}")
        failures.append(f"  only in local: {sorted(aliases_local - aliases_complete)}")

    if families_complete != families_local:
        failures.append("familyMappings mismatch")
        failures.append(f"  only in completion: {sorted(families_complete - families_local)}")
        failures.append(f"  only in local: {sorted(families_local - families_complete)}")

    if routes_complete != routes_local:
        failures.append("capabilities routes mismatch")
        failures.append(f"  only in completion: {sorted(routes_complete - routes_local)}")
        failures.append(f"  only in local: {sorted(routes_local - routes_complete)}")

    if routes_complete != routes_cli:
        failures.append("CLI/GUI capabilities routes mismatch")
        failures.append(f"  only in completion: {sorted(routes_complete - routes_cli)}")
        failures.append(f"  only in cli: {sorted(routes_cli - routes_complete)}")
    if families_complete != families_cli:
        failures.append("CLI/GUI familyMappings mismatch")
        failures.append(f"  only in completion: {sorted(families_complete - families_cli)}")
        failures.append(f"  only in cli: {sorted(families_cli - families_complete)}")

    # CLI must execute canonical backend tools (same surface as /api/tool),
    # not legacy native-agent wrappers.
    legacy_wrapper_markers = [
        "native_agent->Execute(",
        "SetCompactedConversation(",
        "SetOptimizingToolSelection(",
    ]
    for marker in legacy_wrapper_markers:
        if marker in cli_text:
            failures.append(f"legacy CLI wrapper call present: {marker}")

    required_backend_tools = [
        "compact_conversation",
        "optimize_tool_selection",
        "resolve_symbol",
        "read_lines",
        "plan_code_exploration",
        "search_files",
        "evaluate_integration_audit_feasibility",
        "save_checkpoint",
        "restore_checkpoint",
    ]
    for tool in required_backend_tools:
        if f'runBackendTool("{tool}"' not in cli_text:
            failures.append(f"CLI missing required backend tool route: {tool}")

    # Full-surface registry integrity (agent + non-agent families)
    if not registry.exists():
        failures.append(f"command registry missing: {registry}")
    else:
        try:
            doc = json.loads(read_text(registry))
            entries = doc.get("entries", [])
            registry_stats["total"] = len(entries)
            agent_prefixes = (
                "agent.",
                "subagent.",
                "autonomy.",
                "router.",
                "ai.",
            )
            for entry in entries:
                feature_id = str(entry.get("featureId", "")).strip().lower()
                is_agent_family = feature_id.startswith(agent_prefixes)
                if is_agent_family:
                    registry_stats["agentEntries"] += 1
                else:
                    registry_stats["nonAgentEntries"] += 1

                if not entry.get("hasRealHandler", False):
                    registry_stats["hasRealHandlerFalse"] += 1
                    if is_agent_family:
                        registry_stats["agentHasRealHandlerFalse"] += 1
                    else:
                        registry_stats["nonAgentHasRealHandlerFalse"] += 1
                if not entry.get("guiSupported", False):
                    registry_stats["guiSupportedFalse"] += 1
                    if is_agent_family:
                        registry_stats["agentGuiSupportedFalse"] += 1
                    else:
                        registry_stats["nonAgentGuiSupportedFalse"] += 1
                if not entry.get("cliSupported", False):
                    registry_stats["cliSupportedFalse"] += 1
                    if is_agent_family:
                        registry_stats["agentCliSupportedFalse"] += 1
                    else:
                        registry_stats["nonAgentCliSupportedFalse"] += 1
                if not str(entry.get("handler", "")).strip():
                    registry_stats["missingHandlerName"] += 1
                if not str(entry.get("cliCommand", "")).strip():
                    registry_stats["missingCliCommand"] += 1
        except Exception as ex:
            failures.append(f"failed to parse command registry: {ex}")

    if registry_stats["hasRealHandlerFalse"] != 0:
        failures.append(f"registry has unresolved handlers: {registry_stats['hasRealHandlerFalse']}")
    if registry_stats["guiSupportedFalse"] != 0:
        failures.append(f"registry GUI unsupported entries: {registry_stats['guiSupportedFalse']}")
    if registry_stats["cliSupportedFalse"] != 0:
        failures.append(f"registry CLI unsupported entries: {registry_stats['cliSupportedFalse']}")
    if registry_stats["missingHandlerName"] != 0:
        failures.append(f"registry entries missing handler names: {registry_stats['missingHandlerName']}")
    if registry_stats["missingCliCommand"] != 0:
        failures.append(f"registry entries missing CLI commands: {registry_stats['missingCliCommand']}")
    if registry_stats["nonAgentHasRealHandlerFalse"] != 0:
        failures.append(
            f"non-agent unresolved handlers present: {registry_stats['nonAgentHasRealHandlerFalse']}"
        )
    if registry_stats["nonAgentGuiSupportedFalse"] != 0:
        failures.append(
            f"non-agent GUI unsupported entries present: {registry_stats['nonAgentGuiSupportedFalse']}"
        )
    if registry_stats["nonAgentCliSupportedFalse"] != 0:
        failures.append(
            f"non-agent CLI unsupported entries present: {registry_stats['nonAgentCliSupportedFalse']}"
        )

    if wrapper_total != 0:
        failures.append(f"wrapper macros present: {wrappers}")
    if global_wrapper_total != 0:
        failures.append(f"global wrapper-like macros present: {global_wrappers}")
    if args.strict_anywhere and literal_wrapper_total != 0:
        failures.append(f"literal anywhere wrapper-like macros present: {literal_wrappers}")

    print("Agent parity report")
    print(f"- aliases: {len(aliases_complete)}")
    print(f"- familyMappings: {len(families_complete)}")
    print(f"- routes: {len(routes_complete)}")
    print(f"- cli routes: {len(routes_cli)}")
    print(f"- wrapper macros total: {wrapper_total}")
    print(f"- global wrapper-like macros total: {global_wrapper_total}")
    print(f"- literal anywhere wrapper-like macros total: {literal_wrapper_total}")
    print(f"- registry entries total: {registry_stats['total']}")
    print(f"- registry hasRealHandler=false: {registry_stats['hasRealHandlerFalse']}")
    print(f"- registry guiSupported=false: {registry_stats['guiSupportedFalse']}")
    print(f"- registry cliSupported=false: {registry_stats['cliSupportedFalse']}")
    print(f"- registry missing handler names: {registry_stats['missingHandlerName']}")
    print(f"- registry missing CLI commands: {registry_stats['missingCliCommand']}")
    print(f"- registry agent entries: {registry_stats['agentEntries']}")
    print(f"- registry non-agent entries: {registry_stats['nonAgentEntries']}")
    print(f"- non-agent hasRealHandler=false: {registry_stats['nonAgentHasRealHandlerFalse']}")
    print(f"- non-agent guiSupported=false: {registry_stats['nonAgentGuiSupportedFalse']}")
    print(f"- non-agent cliSupported=false: {registry_stats['nonAgentCliSupportedFalse']}")
    print("- global wrapper-like macro scan excludes: ggml-cpu, 3rdparty, vendor, external")

    if failures:
        print("\nFAIL")
        for item in failures:
            print(f"- {item}")
        return 1

    print("\nPASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

