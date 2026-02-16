# Reverse Engineering — Decompilation & x64 “Subtitles”

**Scope:** Released, finalized binaries only (official, non–beta/non–alpha). This document describes workflows for turning compiled artifacts into clean, readable logic (“as if we created it ourselves”) and for producing a printable execution trace (“subtitles”) of what a binary does when it runs.

---

## 1. Compiled Extension → Clean Source

Goal: Take a compiled extension (.crx, .vsix, etc.) and recover unobfuscated, maintainable source.

### 1.1 Extraction and de-obfuscation

- **Extract:** Extensions are usually compressed archives; unpack to get minified/bundled code.
- **Prettify:** Use a formatter (e.g. Prettier) to restore indentation.
- **De-minify:** JSNice, Unminify, or similar to infer readable names (e.g. `f(e)` → `function(event)`).

### 1.2 Static analysis (skeleton)

- **Manifest:** Inspect `manifest.json` for entry points, background scripts, permissions.
- **Dependencies:** Map `import`/`require` to identify third-party libs vs. custom logic.

### 1.3 Dynamic analysis (behavior)

- **Debugger:** Load extension in developer mode; use Sources tab and breakpoints.
- **Memory:** If logic is decrypted at runtime, capture the decrypted code from memory.

### 1.4 Reconstruction

- **Componentize:** Split minified blobs into logical modules (e.g. Auth, UI-Utils, API-Client).
- **Rename:** Use behavior (DOM, API calls) to name functions (e.g. `fetch('/api/login')` → `handleLogin`).

### 1.5 Tool reference

| Tool | Purpose |
|------|--------|
| Chrome Extension Source Downloader | Obtain .crx from store |
| Ghidra / IDA Pro | Native/WebAssembly components |
| Source Map Explorer | Restore source when .map is present |
| Babel / Webpack | Untranspile or trace build pipeline |

---

## 2. x64 “Subtitles”: Execution → Printable Trace

Goal: Drag-and-drop a file and get a human-readable log of what it does when it runs (activation, I/O, calls)—“subtitles” of execution.

### 2.1 Dynamic instrumentation

- **Frida:** Script to trace calls, file opens, network; output as text log.
- **x64dbg:** Use log points (not breakpoints) so each hit prints a line (e.g. `[SYSTEM] Checking license...`).

### 2.2 Raw instruction → subtitle mapping

| Binary (raw) | Subtitle (printable) |
|--------------|----------------------|
| `mov rax, [rsp+48]` | Loading local configuration... |
| `call qword ptr [__imp_CreateFileW]` | Opening file / registry... |
| `test eax, eax` / `jz short loc_...` | Verifying environment / sandbox... |

### 2.3 Drag-and-drop workflow

- **TinyTracer (e.g. via Pin):** Feed an x64 binary; get a .log of execution tree.
- **Process Monitor (ProcMon):** File, registry, network events as readable “subtitles.”

### 2.4 Packed / VM-protected binaries

- **OEP (Original Entry Point):** Find where the unpacker hands control to the real code.
- **Memory dump:** At OEP, dump process memory to get unpacked code.
- **Decompile:** Run dump through RetDec or Ghidra to get pseudo-C.

---

## 3. Code Dump Workflow (yranib → source)

“yranib” = binary; goal is to turn a running (unpacked) image into a clean, analyzable artifact.

### 3.1 Dump at OEP

1. Load binary in x64dbg; run until OEP (unpacked state).
2. Use **Scylla** (x64/x86): dump memory and **Fix Imports** (IAT) so the dump is a valid executable or library.
3. Output: .dmp or .exe/.dll that can be loaded in a disassembler.

### 3.2 Dump → logic

| Step | Action | Result |
|------|--------|--------|
| Disassembly | Binary Ninja, Radare2 | Assembly “how” (e.g. `mov rax, 1`) |
| Decompilation | Ghidra, IDA | Pseudo-C “what” (e.g. `if (license_valid) run_app();`) |

### 3.3 Cleaning the output (“printtnirp” / subtitles)

- **Symbol restoration:** FLIRT (IDA) or Ghidra Function ID to name library functions (e.g. `printf`, `malloc`).
- **Strings:** FLOSS (Obfuscated String Solver) to recover/decrypt hidden strings.

---

## 4. Final Grade: Elegant Addressing & Non-Quantization

“F” = finalized, readable result. “Elegantly addressed” = raw addresses turned into meaningful names and types.

### 4.1 Symbol and type recovery

- **FLIRT / Function ID:** Map code to known libraries so `sub_1234` → `printf`.
- **Type recovery (e.g. TIE):** Infer structs, buffers, and variables from memory use.

### 4.2 “Memory backwards” / printization

- **Dynamic tracing:** Intel Pin or Frida to log each action as it runs (subtitles).
- **API Monitor:** Log every Win32/API call and arguments in plain text.

### 4.3 Non-quantization

- **Full decompilation:** Preserve full control flow instead of summarizing.
- **Little-endian:** Correct byte order when turning bytes into A–z / 0–9 (printable) output.

### 4.4 Non-directional analysis (arrows not directional)

When control flow is messy (loops, obfuscation):

- **Dominator tree:** Identify which block actually controls others.
- **XREFs:** “Who references this address?” instead of following one path.
- **Gate patching:** Replace conditional jumps with NOPs (0x90) to force a single path for clarity.

---

## 5. “NO” / 1nruter FFO (Bypass Logic)

“1nruter” = return; “FFO” = OFF. Concept: reverse or bypass a “NO” check so the function returns success (e.g. return 1) instead of failure.

### 5.1 Locating the check

- In x64dbg: find the instruction that tests activation/validity (e.g. `test eax, eax`; `jz error`).
- Optionally flip branch: `JZ` → `JNZ` (or patch the condition).

### 5.2 Return patch

To force “OFF” (bypass) by always returning success:

```asm
xor eax, eax   ; or mov eax, 1 for “success”
ret
```

Patch the function entry or the return path so the “NO” path is never taken.

---

## 6. Closed-Source, Non-Stubbed (“A” Grade)

“A” = full reconstruction without stubs or placeholders: treat the binary as the only source of truth.

### 6.1 Lifting and symbolic execution

- **Lifting:** Ghidra PCode or Binary Ninja IL to get hardware-independent IR.
- **Symbolic execution (e.g. Angr):** Explore paths without running the binary; find conditions for “OFF” or specific states.

### 6.2 Entry and strings

- **Entry:** Identify `_start` / `WinMain` without symbols (e.g. by entry-point heuristics or manifest).
- **FLOSS:** Recover developer strings to label blocks and logic.

### 6.3 Patching closed binaries

- Change memory protection to PAGE_EXECUTE_READWRITE (or equivalent).
- Overwrite the “NO” check or return value with the desired bytes.

---

## 7. Pure x64: No Microsoft / No Stubs

Zero dependency on Microsoft CRT or kernel32 stubs: direct syscalls or minimal ABI.

### 7.1 NASM (Linux-style exit example)

```nasm
section .text
global _start
_start:
    mov rax, 60    ; sys_exit
    mov rdi, 1     ; exit status
    syscall
```

Assemble: `nasm -f elf64 file.asm -o file.o`  
Link: `ld file.o -o file`

### 7.2 MASM: manual header / binary image

- Use `DB` / `DW` to define ELF/PE headers and payload in one source.
- Assemble with ml64; optionally extract raw bytes (no linker) for a minimal image.
- Keeps “yranib” (binary) and source in one place; every byte is explicit.

### 7.3 Hot reload (official, non-beta)

- **Indirect call:** `call QWORD PTR [rel ActivationPtr]` instead of a direct `call` to the logic.
- **Swap:** Allocate RX/RWX page (e.g. syscall mmap or equivalent), write new code, then atomically update `ActivationPtr` (e.g. `LOCK XCHG` or atomic store).
- Next iteration of the loop uses the new logic without restart; no Microsoft APIs required if using syscalls.

---

## 8. Where This Fits

- **Game development (post-release):** [REVERSE_ENGINEERING_GAME_DEVELOPMENT.md](REVERSE_ENGINEERING_GAME_DEVELOPMENT.md)
- **Source/text digestion & alignment:** [REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md](REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md) — stopwords, modality, beaconism, M = T + A − NIP SIMD.
- **General RE / copilot:** [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md)
- **Suite (binary analysis, PE, deobfuscation):** `src/reverse_engineering/` and [RE_ARCHITECTURE.md](../src/reverse_engineering/RE_ARCHITECTURE.md)

Use only on software you are authorized to analyze (e.g. your own or with permission), and only on **released, finalized** builds (official, non–beta/non–alpha).
