#!/usr/bin/env python3
# ============================================================================
# sovereign_check_no_deps.py — Lab PE heuristic: "minimal import / no reloc"
# ============================================================================
# Purpose: Inspect a PE32+ image for **lab** characteristics sometimes associated
# with hand-rolled / minimal emitters:
#   - No **base relocation** directory (DataDirectory[5] empty), OR optional
#     section named `.reloc` with no mapped data dir (report both).
#   - No imports from **common MSVC CRT / UCRT** DLLs (heuristic list).
#
# This does **NOT** prove security, absence of malware, or "sovereignty" in any
# certification sense. It does **NOT** replace §6/§7 policy: production RawrXD
# still builds with **cl + link.exe + CI**. See:
#   docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md
#
# Usage:
#   python tools/sovereign_check_no_deps.py path\\to\\file.exe
#   python tools/sovereign_check_no_deps.py file.exe --json --strict
#
# Exit codes: 0 = heuristics pass (or --warn-only), 1 = heuristic fail or parse error, 2 = bad args
# ============================================================================

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Any

IMAGE_DIRECTORY_ENTRY_IMPORT = 1
IMAGE_DIRECTORY_ENTRY_BASERELOC = 5


def _rva_to_offset(
    sections: list[dict[str, Any]], rva: int, image_size: int
) -> int | None:
    """
    Map RVA to file offset. Uses VirtualSize (or SizeOfRawData) per section.
    """
    for s in sections:
        va = s["VirtualAddress"]
        vsize = s["VirtualSize"] if s["VirtualSize"] else s["SizeOfRawData"]
        ptr = s["PointerToRawData"]
        if va <= rva < va + vsize and ptr != 0:
            return ptr + (rva - va)
    return None


def _read_utf8_z(data: bytes, off: int) -> str:
    end = data.find(b"\0", off)
    if end < 0:
        end = len(data)
    return data[off:end].decode("utf-8", errors="replace")


def _parse_pe64(data: bytes) -> dict[str, Any]:
    if len(data) < 64 or data[0:2] != b"MZ":
        return {"error": "not_mz"}
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if e_lfanew + 24 > len(data):
        return {"error": "bad_e_lfanew"}
    if data[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        return {"error": "not_pe"}

    coff_off = e_lfanew + 4
    num_sec = struct.unpack_from("<H", data, coff_off + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff_off + 16)[0]
    opt_off = coff_off + 20
    sect_off = opt_off + opt_size

    if opt_off + 112 + 8 * 16 > len(data):
        return {"error": "bad_optional"}

    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x20B:
        return {"error": "not_pe32_plus", "magic": magic}

    dd_import_rva = struct.unpack_from("<I", data, opt_off + 112 + 8 * 1)[0]
    dd_import_size = struct.unpack_from("<I", data, opt_off + 112 + 8 * 1 + 4)[0]
    dd_reloc_rva = struct.unpack_from("<I", data, opt_off + 112 + 8 * 5)[0]
    dd_reloc_size = struct.unpack_from("<I", data, opt_off + 112 + 8 * 5 + 4)[0]

    sections: list[dict[str, Any]] = []
    for i in range(num_sec):
        o = sect_off + i * 40
        if o + 40 > len(data):
            break
        name = data[o : o + 8].split(b"\0", 1)[0].decode("ascii", errors="replace")
        vs = struct.unpack_from("<I", data, o + 8)[0]
        va = struct.unpack_from("<I", data, o + 12)[0]
        raw_size = struct.unpack_from("<I", data, o + 16)[0]
        ptr_raw = struct.unpack_from("<I", data, o + 20)[0]
        sections.append(
            {
                "name": name.strip(),
                "VirtualSize": vs,
                "VirtualAddress": va,
                "SizeOfRawData": raw_size,
                "PointerToRawData": ptr_raw,
            }
        )

    imports: list[str] = []
    if dd_import_rva and dd_import_size:
        idt_off = _rva_to_offset(sections, dd_import_rva, len(data))
        if idt_off is None:
            imports = ["<unmapped_import_directory>"]
        else:
            p = idt_off
            while p + 20 <= len(data):
                oft, _ts, _fc, name_rva, ft = struct.unpack_from("<IIIII", data, p)
                if oft == 0 and name_rva == 0 and ft == 0:
                    break
                if name_rva:
                    no = _rva_to_offset(sections, name_rva, len(data))
                    if no is not None and no < len(data):
                        dll = _read_utf8_z(data, no).lower()
                        imports.append(dll)
                p += 20

    has_reloc_section = any(s["name"].lower() == ".reloc" for s in sections)

    return {
        "ok": True,
        "dd_import_rva": dd_import_rva,
        "dd_import_size": dd_import_size,
        "dd_reloc_rva": dd_reloc_rva,
        "dd_reloc_size": dd_reloc_size,
        "has_reloc_section": has_reloc_section,
        "imports": imports,
    }


# Heuristic: CRT / MSVC runtime-related DLLs (not exhaustive).
_CRIT_CRT_PATTERNS: tuple[str, ...] = (
    "ucrtbase.dll",
    "msvcrt.dll",
    "msvcp140.dll",
    "msvcp140d.dll",
    "vcruntime140.dll",
    "vcruntime140d.dll",
    "vcruntime140_1.dll",
    "vcruntime140_1d.dll",
    "concrt140.dll",
    "concrt140d.dll",
    "msvcp140_atomic_wait.dll",
    "msvcp140_codecvt_ids.dll",
    "vcamp140.dll",
    "vcomp140.dll",
    "msvcp140_1.dll",
    "msvcp140_2.dll",
)


def _crt_hits(imports: list[str]) -> list[str]:
    hits: list[str] = []
    for dll in imports:
        d = dll.lower()
        if d in _CRIT_CRT_PATTERNS:
            hits.append(dll)
            continue
        if d.startswith("api-ms-win-crt-"):
            hits.append(dll)
    return hits


def analyze(data: bytes, allow_dot_reloc_section: bool = False) -> dict[str, Any]:
    p = _parse_pe64(data)
    if "error" in p:
        return p

    crt = _crt_hits(p["imports"])
    reloc_dir_empty = p["dd_reloc_rva"] == 0 and p["dd_reloc_size"] == 0
    section_ok = allow_dot_reloc_section or (not p["has_reloc_section"])

    out: dict[str, Any] = {
        "parse": "pe32_plus",
        "base_relocation_directory_empty": bool(reloc_dir_empty),
        "has_dot_reloc_section": p["has_reloc_section"],
        "import_dlls": p["imports"],
        "crt_like_imports": crt,
        "allow_dot_reloc_section_flag": allow_dot_reloc_section,
    }
    out["heuristic_pass"] = bool(
        reloc_dir_empty and section_ok and len(crt) == 0
    )
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Lab heuristic: PE has no base reloc dir + no common CRT DLL imports."
    )
    ap.add_argument("exe", type=Path, help="Path to PE32+ executable")
    ap.add_argument("--json", action="store_true")
    ap.add_argument(
        "--warn-only",
        action="store_true",
        help="Always exit 0 when the file parses (even if heuristic fails); parse errors still exit 1",
    )
    ap.add_argument(
        "--allow-dot-reloc",
        action="store_true",
        help="Do not fail on presence of a .reloc section if DataDirectory[5] is empty",
    )
    args = ap.parse_args()
    path: Path = args.exe
    if not path.is_file():
        print(f"Error: not a file: {path}", file=sys.stderr)
        return 2

    data = path.read_bytes()
    report = analyze(data, allow_dot_reloc_section=args.allow_dot_reloc)
    report["path"] = str(path.resolve())

    if "error" in report:
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            print(f"Parse error: {report!r}")
        return 1

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"File: {path}")
        print(
            f"  Base reloc directory (DD[5]): "
            f"{'empty' if report['base_relocation_directory_empty'] else 'present'}"
        )
        print(f"  .reloc section present: {report['has_dot_reloc_section']}")
        print(f"  Import DLLs ({len(report['import_dlls'])}):")
        for d in report["import_dlls"][:40]:
            print(f"    - {d}")
        if len(report["import_dlls"]) > 40:
            print("    ...")
        print(f"  CRT-like imports (heuristic): {report['crt_like_imports'] or 'none'}")
        print(f"  Heuristic pass: {report['heuristic_pass']}")

    ok = bool(report.get("heuristic_pass"))
    if args.warn_only:
        return 0
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
