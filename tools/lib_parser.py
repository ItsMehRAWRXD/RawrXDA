#!/usr/bin/env python3
# ============================================================================
# lib_parser.py — COFF archive (.lib / import library) inspector
# ============================================================================
# Purpose: Educational / lab artifact inspection — see what link.exe ingests
# from MSVC-style import libraries before PE emission. This does NOT replace
# link.exe, symbol resolution, or LTCG; it lists archive members and peeks at
# embedded COFF object headers when present.
#
# Usage:
#   python tools/lib_parser.py path\to\library.lib
#   python tools/lib_parser.py path\to\library.lib --json
#   python tools/lib_parser.py path\to\library.lib --dump-first 4096
#
# References: PE Format — Archive (Library) File Format (Microsoft Learn)
# ============================================================================

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Any

ARCHIVE_MAGIC = b"!<arch>\n"
MEMBER_HEADER_SIZE = 60
IMAGE_FILE_MACHINE_AMD64 = 0x8664
IMAGE_FILE_MACHINE_I386 = 0x014C
IMAGE_FILE_MACHINE_ARM64 = 0xAA64


def _parse_member_header(raw: bytes) -> dict[str, Any] | None:
    if len(raw) < MEMBER_HEADER_SIZE:
        return None
    name = raw[0:16].decode("ascii", errors="replace").strip()
    size_ascii = raw[48:58].decode("ascii", errors="replace").strip()
    try:
        size = int(size_ascii, 10)
    except ValueError:
        size = -1
    end = raw[58:60]
    return {"name_field": name, "size": size, "end_marker": end}


def _coff_machine_name(machine: int) -> str:
    return {
        IMAGE_FILE_MACHINE_AMD64: "AMD64",
        IMAGE_FILE_MACHINE_I386: "I386",
        IMAGE_FILE_MACHINE_ARM64: "ARM64",
    }.get(machine, f"0x{machine:04X}")


def _peek_coff_header(obj_bytes: bytes) -> dict[str, Any] | None:
    if len(obj_bytes) < 20:
        return None
    machine = struct.unpack_from("<H", obj_bytes, 0)[0]
    # COFF object: machine should be plausible
    if machine not in (
        IMAGE_FILE_MACHINE_AMD64,
        IMAGE_FILE_MACHINE_I386,
        IMAGE_FILE_MACHINE_ARM64,
    ):
        return None
    return {"coff_machine": machine, "coff_machine_name": _coff_machine_name(machine)}


def parse_coff_archive(data: bytes) -> dict[str, Any]:
    if len(data) < len(ARCHIVE_MAGIC) or data[: len(ARCHIVE_MAGIC)] != ARCHIVE_MAGIC:
        return {"error": "not_a_coff_archive", "magic": data[:16].hex()}

    members: list[dict[str, Any]] = []
    off = len(ARCHIVE_MAGIC)
    idx = 0
    long_names: bytes | None = None

    while off + MEMBER_HEADER_SIZE <= len(data):
        hdr_raw = data[off : off + MEMBER_HEADER_SIZE]
        meta = _parse_member_header(hdr_raw)
        if not meta or meta["size"] < 0:
            break
        body_off = off + MEMBER_HEADER_SIZE
        body_end = body_off + meta["size"]
        if body_end > len(data):
            members.append(
                {
                    "index": idx,
                    "error": "truncated_member",
                    "name_field": meta["name_field"],
                    "declared_size": meta["size"],
                }
            )
            break
        body = data[body_off:body_end]
        name_field = meta["name_field"]
        display_name = name_field
        if name_field.startswith("/") and name_field[1:].isdigit() and long_names:
            try:
                i = int(name_field[1:])
                nul = long_names.find(b"\x00", i)
                if nul > i:
                    display_name = long_names[i:nul].decode("ascii", errors="replace")
            except Exception:
                pass

        entry: dict[str, Any] = {
            "index": idx,
            "header_offset": off,
            "name_field": name_field,
            "display_name": display_name,
            "size": meta["size"],
            "body_offset": body_off,
        }
        coff = _peek_coff_header(body)
        if coff:
            entry["coff"] = coff
        if name_field.startswith("//") or name_field.strip() == "//":
            long_names = body
            entry["role"] = "long_names_dictionary"
        elif name_field.strip() == "/":
            entry["role"] = "first_linker_member"
        elif name_field.startswith("/") and name_field not in ("//", "/"):
            entry["role"] = "import_or_named_member"
        else:
            entry["role"] = "object_or_data"

        members.append(entry)
        idx += 1
        # Members are 2-byte aligned after data
        off = body_end
        if off & 1:
            off += 1

    return {
        "format": "coff_archive",
        "total_size": len(data),
        "member_count": len(members),
        "members": members,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Inspect MSVC COFF .lib (archive) structure.")
    ap.add_argument("lib", type=Path, help="Path to .lib file")
    ap.add_argument("--json", action="store_true", help="Emit JSON only")
    ap.add_argument(
        "--dump-first",
        type=int,
        metavar="N",
        help="Hexdump first N bytes of file (after parse)",
    )
    args = ap.parse_args()
    path: Path = args.lib
    if not path.is_file():
        print(f"Error: not a file: {path}", file=sys.stderr)
        return 2

    data = path.read_bytes()
    report = parse_coff_archive(data)
    report["path"] = str(path.resolve())

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        if "error" in report:
            print(f"Not a COFF archive: {report.get('error')} magic={report.get('magic')}")
            return 1
        print(f"File: {path}")
        print(f"COFF archive: OK ({report['member_count']} members, {report['total_size']} bytes)\n")
        for m in report["members"]:
            line = (
                f"  [{m['index']:4d}] off=0x{m['header_offset']:08X} "
                f"size={m['size']:6d} name={m.get('display_name', m['name_field'])!r}"
            )
            if "coff" in m:
                line += f"  COFF machine={m['coff']['coff_machine_name']}"
            if "error" in m:
                line += f"  ERROR={m['error']}"
            print(line)

    if args.dump_first and args.dump_first > 0:
        n = min(args.dump_first, len(data))
        print("\n-- hexdump first", n, "bytes --")
        for i in range(0, n, 16):
            chunk = data[i : i + 16]
            hx = " ".join(f"{b:02X}" for b in chunk)
            print(f"{i:08X}  {hx}")

    return 0 if "error" not in report else 1


if __name__ == "__main__":
    raise SystemExit(main())
