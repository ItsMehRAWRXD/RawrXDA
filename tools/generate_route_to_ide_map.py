#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SSOT = ROOT / "src" / "core" / "ssot_handlers.cpp"
SSOT_EXT = ROOT / "src" / "core" / "ssot_handlers_ext_isolated.cpp"
REG = ROOT / "src" / "core" / "command_registry.hpp"
IDE_H = ROOT / "src" / "win32app" / "Win32IDE.h"
IDE_CMD = ROOT / "src" / "win32app" / "Win32IDE_Commands.cpp"
OUT = ROOT / "reports" / "route_to_ide_map.md"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_routes(text: str, pattern: str) -> list[tuple[int, str]]:
    return [(int(m.group(1)), m.group(2)) for m in re.finditer(pattern, text)]


def parse_registry(text: str) -> dict[int, tuple[str, str]]:
    mapping: dict[int, tuple[str, str]] = {}
    pat = re.compile(
        r'X\(\s*(\d+)\s*,\s*[\w_]+\s*,\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*[\w_]+\s*,\s*"[^"]*"\s*,\s*([\w_]+)\s*,'
    )
    for m in pat.finditer(text):
        cmd_id = int(m.group(1))
        canonical = m.group(2)
        handler = m.group(3)
        mapping[cmd_id] = (canonical, handler)
    return mapping


def parse_idm_values(*texts: str) -> dict[int, str]:
    out: dict[int, str] = {}
    pat = re.compile(r"#define\s+(IDM_[A-Za-z0-9_]+)\s+(\d+)")
    for text in texts:
        for m in pat.finditer(text):
            out[int(m.group(2))] = m.group(1)
    return out


def parse_handled_macros(text: str) -> set[str]:
    return set(re.findall(r"case\s+(IDM_[A-Za-z0-9_]+)\s*:", text))


def main() -> int:
    ssot = read(SSOT)
    ssot_ext = read(SSOT_EXT)
    reg = read(REG)
    ide_h = read(IDE_H)
    ide_cmd = read(IDE_CMD)

    route_entries = parse_routes(ssot, r'routeToIde\(ctx,\s*(\d+),\s*"([^"]+)"\)')
    route_entries += parse_routes(ssot_ext, r'delegateToGui\(ctx,\s*(\d+),\s*"([^"]+)"\)')
    route_entries = sorted(set(route_entries), key=lambda x: x[0])

    reg_map = parse_registry(reg)
    idm_by_value = parse_idm_values(ide_h, ide_cmd)
    handled_macros = parse_handled_macros(ide_cmd)

    lines = [
        "# RouteToIde Command Map",
        "",
        "| cmdId | canonical | ssot handler | Win32IDE macro | status | proof note |",
        "|---:|---|---|---|---|---|",
    ]

    for cmd_id, fallback_name in route_entries:
        canonical, handler = reg_map.get(cmd_id, (fallback_name, "(unknown)"))
        macro = idm_by_value.get(cmd_id, "")
        wired = bool(macro) and macro in handled_macros
        status = "wired" if wired else "unverified"
        proof = (
            f"{macro} case exists in Win32IDE_Commands.cpp"
            if wired
            else "no matching IDM_* case found in Win32IDE_Commands.cpp"
        )
        lines.append(
            f"| {cmd_id} | {canonical} | {handler} | {macro or '(none)'} | {status} | {proof} |"
        )

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {OUT}")
    print(f"Entries: {len(route_entries)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
