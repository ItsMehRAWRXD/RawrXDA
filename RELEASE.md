# RawrXD AgenticIDE — Release v1.0.1

**Release Tag:** `v1.0.1` | **Architecture:** Native v3 | **Platform:** Windows x64  
**Build Date:** March 2, 2026 | **Commit:** `b78c8ed780cafda2a1e78e6a29a471ac32f04aab`

---

## Naming Clarification

**v1.0.1** is the **release tag** (Git versioning).  
**v3** (Native v3) is the **architecture generation** (zero-Qt, pure Win32 API design).

This release marks the first production-ready build of the third-generation native architecture.

---

## What's Shipped

### Core Executables
- **RawrXD-Win32IDE.exe** — Full IDE (26.96 MB, native Win32 UI)
- **RawrXD_Gold.exe** — Optimized binary (9.32 MB, release-tuned)
- **RawrXD_CLI** — Command-line interface (batch processing, headless workflows)

### Architecture Highlights
- **Zero Qt Dependencies** — Pure Win32 APIs (User32, GDI32, ComCtl32, D2D1, DWrite)
- **117 LSP/Debugger/Hotpatch Handlers** — Complete implementation (no queue-only stubs)
- **15 Hotpatch Subsystems** — Live code injection, symbol resolution, rollback journal
- **Multi-Window Kernel** — Native Win32 MDI (RawrXD_MultiWindow_Kernel.dll, 110 KB)

### CMake Build Targets
```cmake
RawrEngine                    # Core inference engine
RawrXD-InferenceEngine        # Standalone inference CLI
RawrXD-Win32IDE               # Full IDE (WIN32 subsystem)
RawrXD_CLI                    # Batch/headless CLI
rawrxd-monaco-gen             # Monaco editor codegen
```

All targets compile cleanly with **MSVC 14.50 x64** (CMake 3.20+, Ninja).

---

## Verification

### 1. Qt-Free Policy Enforcement
```powershell
.\Verify-Build.ps1 -BuildDir .\build
```
**Expected Output:** Zero Qt includes, zero Qt DLLs, zero Qt symbols.

### 2. Source Manifest Integrity
```powershell
.\scripts\Digest-SourceManifest.ps1 -OutDir .\build -Format both
```
Generates SHA256 manifest + dependency graph for reproducible builds.

### 3. Smoke Tests
```bash
cd build_smoke && ctest --output-on-failure
```
**Coverage:** ASM emit correctness, LSP handler completeness, hotpatch rollback logic.

---

## Distribution Package

**Clean Release:** [dist/release/v1.0.1/RawrXD-v1.0.1-clean.zip](dist/release/v1.0.1/RawrXD-v1.0.1-clean.zip) (8.34 MB)

**Contents:**
- `bin/RawrXD-Win32IDE.exe`, `bin/RawrXD_Gold.exe`
- `docs/RELEASE-v1.0.0.md`, `docs/ROADMAP-v1.1.md`, `docs/HANDLER-MATRIX-v1.0.0.md`
- `MANIFEST.txt` (SHA256 hashes for all binaries)

**Verification:**
```powershell
Get-FileHash .\dist\release\v1.0.1\RawrXD-v1.0.1-clean.zip -Algorithm SHA256
# Expected: 3A848A0CD26F3915444A5A64988AC822D84D6695416A9DAF56614F4B4FE50B…
```

---

## Documentation

| Document | Path | Purpose |
|----------|------|---------|
| **Build Verification** | [Verify-Build.ps1](Verify-Build.ps1) | Qt-free policy enforcement (7 checks) |
| **Handler Matrix** | [HANDLER-MATRIX-v1.0.0.md](HANDLER-MATRIX-v1.0.0.md) | 117 handler inventory + timestamps |
| **v1.1 Roadmap** | [ROADMAP-v1.1.md](ROADMAP-v1.1.md) | Community features (PDB symbols, Voice STT) |
| **Top 50 Gaps** | [docs/TOP_50_GAP_ANALYSIS.md](docs/TOP_50_GAP_ANALYSIS.md) | Feature parity vs industrial IDEs |
| **CLI Parity** | [Ship/CLI_PARITY.md](Ship/CLI_PARITY.md) | CLI/IDE feature equivalence matrix |
| **Mac/Linux** | [docs/MAC_LINUX_WRAPPER.md](docs/MAC_LINUX_WRAPPER.md) | Wine wrapper for cross-platform use |

---

## Build From Source

```powershell
# Prerequisites: MSVC 14.50+, CMake 3.20+, Ninja
git clone https://github.com/ItsMehRAWRXD/RawrXD.git
cd RawrXD
git checkout v1.0.1

# Configure + Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# Run verification
.\Verify-Build.ps1 -BuildDir .\build

# Launch IDE
.\build\bin\RawrXD-Win32IDE.exe
```

---

## Known Limitations

1. **Windows-only** — Native Win32 APIs (Wine wrapper available for Mac/Linux, see [docs/MAC_LINUX_WRAPPER.md](docs/MAC_LINUX_WRAPPER.md))
2. **x64 only** — No x86/ARM64 builds (ARMv8 planned for v1.2)
3. **MSVC required** — MinGW/Clang support is experimental

---

## License

MIT License — See [LICENSE](LICENSE) for full text.

---

## Support

- **Issues:** https://github.com/ItsMehRAWRXD/RawrXD/issues
- **Discussions:** https://github.com/ItsMehRAWRXD/RawrXD/discussions

---

**Release Signed By:** ItsMehRAWRXD  
**SHA256 (RawrXD-v1.0.1-clean.zip):** `3A848A0CD26F3915444A5A64988AC822D84D6695416A9DAF56614F4B4FE50B…`
