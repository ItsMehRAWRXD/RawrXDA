# Sovereign Forge — MASM x64 PE emission (teaching notes)

This document **reverse-engineers** the mechanical requirements for a **pure MASM** tool that writes a **PE32+** file (the **emission layer** — see **`docs/SOVEREIGN_EMISSION_LAYER.md`**). It fixes common issues in “hand-forged” header sketches and points at the **in-repo reference** implementation.

**Tier G** — reusable teaching / tooling. **Tier P** IDE still ships with **`link.exe`** for product binaries — **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** **§7**.

---

## Reference implementation (maintained)

| Artifact | Role |
|----------|------|
| **`tools/pe_emitter.asm`** | Full **BuildImage** + **WritePE**: DOS/PE/optional/sections, import table, `.text` with **`call [rip+…]` → ExitProcess**, **`IMAGE_SIZE`** single write. |
| **`tools/sovereign_macros.inc`** | IAT RVAs / **`SovereignCallImp`** helpers for MASM call sites. |

Build (Developer Prompt / `ml64` + `link` on PATH):

```bat
ml64 /c /nologo /Fo build\pe_emitter.obj tools\pe_emitter.asm
link /nologo /subsystem:console /entry:main build\pe_emitter.obj kernel32.lib /out:build\pe_emitter.exe
```

Convenience: **`scripts/sovereign_forge_build.bat`** → builds the **forge tool** as **`build/sovereign_app.exe`** (same source as `pe_emitter.asm`). When run, the tool still writes **`output.exe`** (see `szOutFile` in `pe_emitter.asm`); rename or edit that string if you want a different emitted PE name.

---

## What a “minimal forge” must get right

1. **`e_lfanew`** must point to the **actual** `PE\0\0` signature (often **`0x80`**, not arbitrary padding counts).
2. **`SizeOfHeaders`** must cover **all** headers through the end of the **section table** (typically **`0x200`** for 2 sections, not `400h` unless you really laid out that many bytes).
3. **Section file layout**: **`PointerToRawData`** / **`SizeOfRawData`** must match what you **`WriteFile`** (usually **`FILE_ALIGN`**-aligned).
4. **Single `WriteFile` of `400h`** is only valid if the **entire** image is exactly **`0x400`** bytes — a normal PE with code + imports is **larger** (see **`IMAGE_SIZE`** in `pe_emitter.asm`).
5. **REL32 on `E8`**: `rel32 = target_rva - (source_rva + 5)` where **`source_rva`** is the **`E8`** byte. For **`call` → next instruction** (e.g. **`ret`** immediately after), **`rel32 = 0`** is correct: next RIP = **`source + 5`**, target = **`source + 5`**.
6. **No imports**: a **`ret`‑only** entry can **crash** or behave oddly depending on stack setup; real minimal user-mode exits use **`kernel32!ExitProcess`** (as in **`pe_emitter.asm`**) or a full CRT.

---

## “Living state” + patching pass (your sketch)

```text
target_code  db 0E8h, 00h, 00h, 00h, 00h, 0C3h   ; call rel32 ; ret
; Patch dword at target_code+1 so rel32 encodes (target - next_rip).
```

For **`call` to the byte at `entry+5`** (the **`ret`**): **`rel32 = 0`**.

---

## Related

- **`docs/SOVEREIGN_EMISSION_LAYER.md`**
- **`docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`** (REL32 patch-site rules)
- **`examples/som_minimal_usage.c`** (same math in C)
