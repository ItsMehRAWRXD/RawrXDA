# Fortress IDE: E-Drive Audit & Migration to D:\RawrXD

**Purpose:** Bring all compiler/linker and source assets from E: into D:\RawrXD so the IDE is a self-contained "fortress" with its own toolchain and sources.

---

## 1. E-Drive Audit Summary

### 1.1 Compilers & linkers (real, functional on E:)

| Location | Contents | Migrated to D:\rawrxd |
|---------|----------|------------------------|
| **E:\nasm\nasm-2.16.01\** | `nasm.exe` (1.6 MB), `ndisasm.exe` (1.1 MB), LICENSE | **Yes** → `D:\rawrxd\toolchain\nasm\` |
| **E:\masm\** | No binaries; uses system **MSVC** (ml64, link) + **Windows Kits** | Scripts/samples → `D:\rawrxd\toolchain\masm\` |
| **E:\** (root) | `dxcompiler.dll`, `dxil.dll` (DirectX shader compiler) | Not copied (optional; can add to toolchain later) |

**Note:** MASM (ml64/link) are not on E:; they come from Visual Studio (e.g. `C:\VS2022Enterprise\VC\Tools\MSVC\...\bin\Hostx64\x64\`). The fortress uses **system MSVC + Windows Kits** for linking; only **NASM** is fully bundled under D:\rawrxd.

### 1.2 Build & compile scripts (E: → D:\rawrxd)

| E: path | Description | D:\rawrxd |
|--------|-------------|-----------|
| E:\masm\Unified-PowerShell-Compiler-Working.ps1 | MASM/NASM → .exe via ml64 + link | Copied + **Unified-PowerShell-Compiler-RawrXD.ps1** (uses `D:\rawrxd\toolchain\nasm\nasm.exe`) |
| E:\masm\Unified-PowerShell-Compiler.ps1 | Variant | In toolchain\masm |
| E:\masm\Unified-PowerShell-Compiler-Fixed.ps1 | Variant | In toolchain\masm |
| E:\masm\Build-MASM-IDE.ps1 | Build MASM64 GUI skeleton | In toolchain\masm |
| E:\masm\README.md | Usage for compiler scripts | In toolchain\masm |

### 1.3 ASM source files (E: root → D:\rawrxd\asm-sources)

| File (E:\*.asm) | Size (bytes) | Description |
|-----------------|-------------|-------------|
| advanced_ai_ide.asm | 1,478,522 | AI IDE in assembly |
| custom_asm_compiler.asm | 957,638 | Custom ASM compiler |
| full_working_asm_ide.asm | 1,919,882 | Full working ASM IDE |
| massive_asm_ide.asm | 1,860,586 | Large ASM IDE |
| NEON_VULKAN_FABRIC.asm | 54,006 | NEON/Vulkan fabric |
| Phase3_Master_Complete.asm | 90,327 | Phase 3 master |
| Phase4_Master_Complete.asm | 54,691 | Phase 4 master |
| Phase4_Test_Harness.asm | 18,114 | Phase 4 test harness |
| Phase5_Master_Complete.asm | 65,551 | Phase 5 master |
| Phase5_Test_Harness.asm | 16,998 | Phase 5 test harness |
| pure_assembly_directx_studio.asm | 2,937,141 | Pure ASM DirectX studio |
| ultimate_ide.asm | 1,922,487 | Ultimate IDE |
| ultimate_multilang_ide.asm | 62,855 | Multilang IDE |
| Week2_3_Master_Complete.asm | 45,354 | Week 2/3 master |
| working_assembly_ide.asm | 16,722 | Working ASM IDE |
| working_ide.asm | 17,107 | Working IDE |

**All 16 files** copied to **D:\rawrxd\asm-sources\**.

### 1.4 MASM samples & binaries (E:\masm)

| Item | Migrated |
|------|----------|
| E:\masm\samples\hello_masm.asm, hello_nasm.asm | Yes → toolchain\masm\samples |
| E:\masm\working_ide_masm.asm | Yes → toolchain\masm |
| E:\masm\bin\working_ide_masm.exe, .obj | Yes → toolchain\masm\bin |

### 1.5 Other E: folders (audited, not merged into fortress)

| E: path | Contents | Action |
|--------|----------|--------|
| E:\RawrXD | Full repo clone (kernels, plugins, src, etc.) | Use D:\rawrxd as canonical; sync/copy only if you need specific files |
| E:\RawrXD-ModelLoader | ModelLoader src (empty or minimal in spot check) | D:\rawrxd already has RawrXD-ModelLoader |
| E:\build | CMake build artifacts (5 files) | Not needed in fortress |
| E:\src\qtapp | Qt app (CMakeLists.txt + 104 files under subtree) | Qt removed from D:\rawrxd IDE; skip unless re-adding Qt |
| E:\generic | qtuiotouchplugin.dll | Qt; skip for zero-Qt fortress |
| E:\Backup | User backups (Cursor, Desktop, etc.) | Not compiler/toolchain |
| E:\semantic-analysis, E:\plugin-system | Empty or minimal | Nothing to bring |
| E:\ci-cd, cloud-infrastructure, crypto, etc. | Various | Audit per need; not required for compiler/linker fortress |

---

## 2. What the IDE gained (fortress integration)

### 2.1 Directory layout under D:\rawrxd

```
D:\rawrxd\
├── toolchain\
│   ├── nasm\
│   │   ├── nasm.exe
│   │   ├── ndisasm.exe
│   │   └── LICENSE
│   └── masm\
│       ├── Unified-PowerShell-Compiler-RawrXD.ps1   ← D:\rawrxd-aware (uses toolchain\nasm)
│       ├── Unified-PowerShell-Compiler-Working.ps1
│       ├── Unified-PowerShell-Compiler.ps1
│       ├── Unified-PowerShell-Compiler-Fixed.ps1
│       ├── Build-MASM-IDE.ps1
│       ├── README.md
│       ├── working_ide_masm.asm
│       ├── bin\
│       └── samples\
│           ├── hello_masm.asm
│           └── hello_nasm.asm
└── asm-sources\
    ├── advanced_ai_ide.asm
    ├── custom_asm_compiler.asm
    ├── ... (all 16 E:\*.asm files)
    └── working_ide.asm
```

### 2.2 IDE code changes (Ship)

- **Run (.asm):** Build_Run() now handles `.asm`: invokes `Unified-PowerShell-Compiler-RawrXD.ps1` with `-Tool masm -SubSystem console -OutDir <file_dir>`, then runs the produced `.exe`.
- **Build (.asm):** Build_Build() single-file path now builds `.asm` via the same script (build only, no run).

Compiler script path hardcoded: `D:\rawrxd\toolchain\masm\Unified-PowerShell-Compiler-RawrXD.ps1`. Override NASM location with env **RAWRXD_ROOT** (e.g. `D:\rawrxd`) if needed.

### 2.3 Toolchain script behavior (Unified-PowerShell-Compiler-RawrXD.ps1)

- **NASM:** Uses `D:\rawrxd\toolchain\nasm\nasm.exe` (or `%RAWRXD_ROOT%\toolchain\nasm\nasm.exe`). Fallback: `E:\nasm\nasm-2.16.01\nasm.exe` if fortress copy missing.
- **MASM/link:** Resolved from system (MSVC + Windows Kits). Tries VS2022 Enterprise path first, then Community/Professional, then `C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC`.

---

## 3. Full effect: “Your own compiler and linker” in the IDE

| Aspect | Effect |
|--------|--------|
| **Assembler (NASM)** | Bundled under D:\rawrxd\toolchain\nasm. IDE can build NASM sources without E: or external NASM install. |
| **Assembler (MASM)** | Not bundled; uses system ml64/link. Scripts and samples are under D:\rawrxd\toolchain\masm; IDE invokes them so “your” build flow is in-repo. |
| **Linker** | Same as MASM: system link.exe (MSVC). No copy on E: to bring. |
| **C/C++** | Already in IDE: `cl` for single-file .c/.cpp. Unchanged. |
| **ASM sources** | All E:\*.asm and E:\masm samples/working_ide_masm.asm are under D:\rawrxd (asm-sources + toolchain\masm). |
| **Single tree** | Compiler scripts, NASM, and ASM sources live under D:\rawrxd; E: no longer required for ASM build/run. |

So: the IDE now has **its own** in-repo toolchain (NASM + scripts + ASM sources). MASM/link remain system-provided; the “fortress” is that all **sources and script-driven toolchain** are under D:\RawrXD.

---

## 4. Optional next steps

1. **DX compiler (E:\dxcompiler.dll, dxil.dll):** Copy into `D:\rawrxd\toolchain\dx` and wire shader build if you want in-IDE shader compilation.
2. **E:\RawrXD vs D:\rawrxd:** Diff specific dirs (e.g. kernels, plugins, src) and copy over any missing or newer files you care about.
3. **NASM vs MASM detection:** Extend Run/Build to choose `-Tool nasm` when the .asm file uses NASM syntax (e.g. by heuristic or project option).
4. **RAWRXD_ROOT:** Resolve at runtime in the IDE (e.g. from exe directory or config) and pass to the script so the same binary works from different install paths.

---

## 5. Checklist: E → D migration status

- [x] E:\nasm → D:\rawrxd\toolchain\nasm (full copy)
- [x] E:\masm (scripts, samples, bin) → D:\rawrxd\toolchain\masm
- [x] D:\rawrxd-aware compiler script (Unified-PowerShell-Compiler-RawrXD.ps1)
- [x] E:\*.asm (16 files) → D:\rawrxd\asm-sources
- [x] IDE Run/Build support for .asm in RawrXD_Win32_IDE.cpp
- [x] FORTRESS_E_DRIVE_AUDIT.md (this document)
- [ ] Optional: dxcompiler/dxil into toolchain
- [ ] Optional: sync selected E:\RawrXD or E:\RawrXD-ModelLoader paths into D:\rawrxd

**The IDE is now a fortress: all audited E: compiler/linker and ASM sources have been brought back under D:\RawrXD.**
