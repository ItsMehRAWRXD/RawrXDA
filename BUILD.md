# RawrXD Build Guide

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| MSVC (cl.exe) | 19.x+ | C/C++ compiler |
| ml64.exe | MASM assembler | x64 assembly modules |
| CMake | 3.20+ | Build system generator |
| Windows SDK | 10.0.22621.0+ | Win32 API headers/libs |
| PowerShell | 5.1+ | Build scripts |

## Quick Build

```powershell
# From project root
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Build Variants

### Debug Build
```powershell
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Ninja Build (Faster)
```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Monolithic Build (Single-step)
```powershell
.\tools\fast_monolithic_build.ps1
```

### Orchestrated Build (Full pipeline)
```powershell
.\tools\orchestrate_build.ps1
```

## Project Structure

```
rawrxd/
├── CMakeLists.txt          # Root build config (3400+ lines)
├── cmake/                  # CMake modules
│   ├── MASMCompiler.cmake  # ml64 integration
│   ├── SourceManifest.cmake
│   ├── BuildTypes.cmake
│   └── AutoFeatureRegistry.cmake
├── src/
│   ├── win32app/           # Win32 GUI IDE (117+ files)
│   ├── pe_writer_production/ # PE32+ generator
│   ├── agentic/            # GPU/DMA compute engine (MASM)
│   └── *.asm               # IPC bridge, pipe server
├── include/                # Public headers
├── tools/                  # Build helpers
├── scripts/                # Automation scripts
├── sign.ps1                # Code signing (Authenticode)
└── .github/workflows/      # CI/CD (19 workflows)
```

## MASM Assembly

x64 assembly files (`.asm`) are compiled with `ml64.exe` via the CMake MASM module:
- `GPU_DMA_COMPLETE_IMPLEMENTATIONS.asm` — Core compute engine (179K+ lines)
- `RawrXD_PipeServer.asm` — Named pipe IPC server
- `RawrXD_IPC_Bridge.asm` — Shared memory bridge

## Code Signing

```powershell
# Sign with self-signed cert (development)
.\sign.ps1 -SelfSign

# Sign with PFX certificate (release)
.\sign.ps1 -CertPath "path\to\cert.pfx" -CertPassword "password"
```

## Release Packaging

```powershell
.\scripts\package_release.ps1
# or
.\scripts\package_gold_master.ps1
```

## CI/CD

GitHub Actions workflows in `.github/workflows/`:
- `build.yml` — Primary build pipeline
- `ci.yml` — Continuous integration
- `release.yml` / `self_release.yml` — Release automation
- `tests.yml` — Test suite
- `performance.yml` — Performance regression gates
- `quality.yml` — Code quality checks

## Dependencies

**Zero external dependencies** — the project uses only Win32 API and standard C/C++ runtime. GPU libraries (Vulkan, DirectStorage, ROCm, CUDA) are loaded dynamically at runtime via `LoadLibraryA`/`GetProcAddress`.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `ml64.exe not found` | Run from VS Developer Command Prompt or `.\tools\ensure_vsenv.ps1` |
| Link errors (unresolved externals) | Check `link_stubs_win32ide_methods.cpp` for missing stubs |
| SDK headers not found | Set `CMAKE_SYSTEM_VERSION=10.0.22621.0` |
| RichEdit control missing | Ensure `LoadLibraryA("msftedit.dll")` in main |
