# Reverse Engineering — Game Development (Post-Release)

**Scope:** This section applies to **released, finalized software only** (official release, non–beta/non–alpha). It is intended for **game-development** use: modding, tooling, compatibility, and external/internal framework work after a product has shipped.

---

## 1. Locating Modules and Assets

Finding modules or files regardless of install path or environment (e.g. game assets, plugins, or components in non-standard locations).

### 1.1 System-wide file search

| Environment | Method | Example |
|-------------|--------|--------|
| **Linux/Unix** | `find` (ignores env, scans disk) | `sudo find / -name "*<pattern>*" 2>/dev/null` |
| **Linux** | Binaries in PATH | `whereis <name>` ; `which <name>` ; `type -a <name>` |
| **Windows (PowerShell)** | Recursive search | `Get-ChildItem -Path C:\ -Filter "*<pattern>*" -Recurse -ErrorAction SilentlyContinue \| Select-Object FullName` |
| **Windows** | Loaded DLLs in processes | `tasklist /m *<pattern>*` |

### 1.2 Python module location

```python
import importlib.util

def locate(mod_name):
    spec = importlib.util.find_spec(mod_name)
    return spec.origin if spec else "Not Found"

# Example: locate("some_module")
```

From an interactive interpreter: `import module_name` then `print(module_name.__file__)`.

### 1.3 Kernel / loaded modules

- **Linux:** `lsmod | grep <name>` ; `modinfo <name>`
- **Linux process maps:** `cat /proc/<PID>/maps | grep <name>`
- **Windows:** Process Explorer or `tasklist /m` for DLLs in processes

---

## 2. External vs Internal Frameworks (Conclusion / Game-Deception Style)

For post-release game-development tooling: trainers, mod loaders, or generated CRC/skeleton logic. Architecture is split into **external** (standalone process) and **internal** (injected into the target).

| Aspect | External (standalone .exe) | Internal (injected .dll) |
|--------|----------------------------|---------------------------|
| **Method** | `ReadProcessMemory` / `WriteProcessMemory` | Direct pointer access after injection |
| **Performance** | Slower (syscall overhead) | Fast (runs in target’s threads) |
| **Use case** | Simple trainers (health, ammo, etc.) | Hooks, overlays, VTable overrides |
| **Graphics** | External overlay (GDI/DirectX) | Can hook swapchain (e.g. ImGui) |

### 2.1 Core components

- **Process attacher:** OS APIs (e.g. `OpenProcess` on Windows, `ptrace` on Linux) to obtain a handle to the target.
- **Memory scanner:** Pattern/AOB scanning for static signatures and dynamic addresses so offsets survive updates.
- **Skeleton generator:** Output C++/C# headers or classes from discovered structures (e.g. ReClass.NET–style).

### 2.2 CRC and skeleton logic (external)

- **Structure recovery:** Disassembler (Ghidra, IDA) to identify game structures (e.g. World, Player).
- **Logic generation:** Map offsets (e.g. `PlayerBase + 0x100` = Health) to JSON/XML.
- **Skeleton export:** Script to generate C++ (or other) boilerplate from that config.
- **CRC verification:** Checksum of the executable’s `.text` (or relevant sections) against a known-good DB so tooling targets the correct build.

Example skeleton (offsets illustrative only):

```cpp
struct Player {
    char pad_0000[256]; // 0x0000
    float health;       // 0x0100
    float armor;        // 0x0104
};
```

### 2.3 Toolchain (reference)

- **Static:** Ghidra / IDA Pro
- **Dynamic:** x64dbg, Cheat Engine (or similar)
- **UI:** e.g. ImGui for trainer/overlay

---

## 3. ved / oemASMx64 (Low-Level Abstraction)

**ved** = Virtual Environment / Engine Deception. **oemASMx64** = x64 assembly for OEM-level hardware abstraction and direct CPU interaction (e.g. for tooling that runs beneath or alongside the game).

### 3.1 CPUID / MSR (hardware signature)

Used to query or normalize how the CPU presents itself (e.g. for compatibility or HWID-related tooling, not for circumventing anti-cheat on live services).

```nasm
; oemASMx64 — hardware signature (example)
[BITS 64]
section .text
global _get_hw_signature

_get_hw_signature:
    mov eax, 1          ; CPUID Leaf 1: Processor Info and Feature Bits
    cpuid
    ; Use EBX, ECX, EDX as needed (e.g. mask hypervisor bit for compatibility)
    and ecx, 0xFFFFFFFD  ; Example: mask hypervisor bit
    ret
```

### 3.2 Internal hooking (VTable / VMT)

Trampoline hooking: redirect a game function to your code while preserving the original calling convention and state.

- Overwrite initial bytes of the target with `JMP` to your stub.
- In the stub: save state, call original (e.g. via VTable: `mov rax, [rcx]` ; `call [rax + offset]`), restore state, return.

x64: respect shadow space (e.g. `sub rsp, 40` before call, `add rsp, 40` after).

### 3.3 External: syscall-level access

For external tools that avoid user-mode API hooks (e.g. for research or compatibility):

| Component | Method | Layer |
|-----------|--------|--------|
| Process handle | e.g. `NtOpenProcess` (syscall) | Kernel |
| Memory read/write | e.g. `NtReadVirtualMemory` / `NtWriteVirtualMemory` | Kernel |
| HW identification | CPUID / RDTSC | CPU |

Implement by resolving syscall numbers and calling them from assembly or a small C wrapper.

### 3.4 AOB (Array-of-Bytes) scanner

- Scan module base (or main executable) for a byte pattern.
- Use result as base for offsets in your skeleton (e.g. `baseAddress + 0x1A0` for health).
- Keeps external logic resilient to rebases and minor binary changes when combined with CRC or version checks.

---

## 4. Where This Fits in the Repo

- **Reverse engineering suite (binary analysis, PE, deobfuscation):** `src/reverse_engineering/` and `RE_ARCHITECTURE.md`.
- **General RE/copilot-style reverse engineering:** [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md).
- **Decompilation & x64 “subtitles” (extension → source, execution trace, code dump, pure x64):** [REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md](REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md).
- **Game development, post-release only:** this document (`docs/REVERSE_ENGINEERING_GAME_DEVELOPMENT.md`).

Use this material only on software you are authorized to analyze (e.g. your own titles or with explicit permission), after **released finalization** (official release, not beta/alpha).
