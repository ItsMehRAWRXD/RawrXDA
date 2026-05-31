# Enumerate instruction mnemonics from MASM-style .asm sources.
# Heuristic: first token on lines with >=2 tokens, excluding data defs and common directives,
# then intersect with iced-x86's Mnemonic enum (pip install iced-x86).
from __future__ import annotations

import re
from pathlib import Path

try:
    import iced_x86

    ICED_NAMES = {
        name.lower()
        for name in dir(iced_x86.Mnemonic)
        if len(name) > 1 and name[0].isupper() and name.isalnum()
    }
except ImportError:
    ICED_NAMES = set()

ROOT = Path(r"D:\rawrxd")
DIRS = [
    ROOT / "src" / "asm",
    ROOT / "src" / "kernels",
    ROOT / "src" / "editor_bridge",
    ROOT / "src" / "win32app",
    ROOT / "src" / "direct_io",
    ROOT / "src" / "core",
    ROOT / "src" / "ai",
    ROOT / "src" / "memory",
    ROOT / "Ship",
]

DATA_SECOND = {
    "DD", "DQ", "DB", "DW", "DT", "DF", "DP", "DUP", "EQU", "PROC", "ENDP", "ENDS",
    "STRUCT", "ENDSTRUC", "RECORD", "TYPEDEF", "PROTO", "EXTERNDEF", "EXTERN", "PUBLIC",
    "COMM", "ALIGN", "EVEN", "ORG", "GROUP", "SEGMENT", "ASSUME", "OPTION", "INCLUDE",
    "INCLUDELIB", "IF", "ELSEIF", "ELSE", "ENDIF", "MACRO", "ENDM", "REPEAT", "ENDR",
    "FOR", "ENDFOR", "WHILE", "ENDW", "GOTO", "EXITM", "PURGE", "TITLE", "SUBTITLE",
    "PAGE", "COMMENT", "RADIX", "END", "INVOKE", "RESB", "RESW", "RESD", "RESQ", "REST",
    "RESO", "REAL4", "REAL8", "REAL10",
}

DIRECTIVE_FIRST = {
    "if", "elseif", "else", "endif", "macro", "endm", "include", "includelib", "assume",
    "option", "segment", "ends", "group", "radix", "title", "page", "comment", "repeat",
    "endr", "for", "endfor", "while", "endw", "goto", "exitm", "purge",
    # MASM / gas-style lines that are not CPU mnemonics but pass the two-token heuristic
    "align", "invoke", "extern", "extrn", "externdef", "public", "private", "proto",
    "section", "segment", "global", "local", "stack", "db", "dw", "dd", "dq", "dt", "df",
    "real4", "real8", "real10", "textequ", "catstr",
}

TOKEN_RE = re.compile(r"^[A-Za-z@_][A-Za-z0-9@_]*$")


def main() -> None:
    from collections import Counter

    counts: Counter[str] = Counter()
    nfiles = 0
    for d in DIRS:
        if not d.is_dir():
            continue
        for p in d.rglob("*.asm"):
            nfiles += 1
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for line in text.splitlines():
                t = line.lstrip()
                if not t or t[0] == ";":
                    continue
                parts = t.split()
                if len(parts) < 2:
                    continue
                w0, w1 = parts[0], parts[1]
                w0_lower = w0.lower()
                if len(w0) < 2 or len(w0) > 16:
                    continue
                if not TOKEN_RE.match(w0):
                    continue
                if w0.endswith(":"):
                    continue
                if w1.upper() in DATA_SECOND:
                    continue
                if w0_lower in DIRECTIVE_FIRST:
                    continue
                if w0.isupper() and len(w0) >= 2 and w0.isalpha():
                    continue
                counts[w0_lower] += 1

    # Rare tokens are usually struct fields / identifiers in odd-shaped lines; real mnemonics repeat.
    min_hits = 3
    rough = {k for k, v in counts.items() if v >= min_hits}
    if ICED_NAMES:
        found = {k for k in rough if k in ICED_NAMES}
        extras = sorted(rough - found)
    else:
        found = set(rough)
        extras = []

    out = ROOT / "tools" / "_asm_mnemonics_refined.txt"
    out_freq = ROOT / "tools" / "_asm_mnemonics_refined_freq.txt"
    out_x = ROOT / "tools" / "_asm_mnemonics_noniced.txt"
    out.write_text("\n".join(sorted(found)) + "\n", encoding="utf-8")
    out_freq.write_text(
        "\n".join(f"{c:5d}  {m}" for m, c in sorted(counts.items(), key=lambda x: (-x[1], x[0])))
        + "\n",
        encoding="utf-8",
    )
    if extras:
        out_x.write_text("\n".join(extras) + "\n", encoding="utf-8")
    print(f"FILES={nfiles} ICED_MNEMONICS={len(ICED_NAMES)} ROUGH(min_hits={min_hits})={len(rough)}")
    print(f"INTERSECT_ICED={len(found)} -> {out}")
    print(f"FREQ_TABLE -> {out_freq}")
    if extras:
        print(f"NON_ICED_ROUGH({len(extras)}) -> {out_x}")


if __name__ == "__main__":
    main()
