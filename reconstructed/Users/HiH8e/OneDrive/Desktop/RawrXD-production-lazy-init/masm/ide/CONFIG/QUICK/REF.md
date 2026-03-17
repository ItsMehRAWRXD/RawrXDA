# RawrXD Enterprise Config & Beacon - Quick Reference

## One-Line Build

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\build_production.ps1
```

## Artifacts

| File | Purpose | Required |
|------|---------|----------|
| `AgenticIDEWin.exe` | IDE executable | ✅ Yes |
| `gguf_beacon_spoof.dll` | Wirecap DLL | ❌ Optional |
| `config.ini` | Settings file | ❌ Optional (defaults used if missing) |

## config.ini Template

```ini
; RawrXD Enterprise Config
EnableBeacon=1
CaptureMode=GGUF_STREAM
BufferLimit=8192
```

## Key Settings

| Setting | Type | Default | Effect |
|---------|------|---------|--------|
| `EnableBeacon` | Int | 0 | 1 = Load DLL at startup |
| `CaptureMode` | String | (none) | Future use |
| `BufferLimit` | Int | (none) | Future use |

## Runtime Flow

```
app_start
    ├─ LoadConfig() → read config.ini
    ├─ GetConfigInt("EnableBeacon", 0) → check setting
    ├─ if EnableBeacon==1 → LoadLibraryA("gguf_beacon_spoof.dll")
    │  └─ DllMain(DLL_PROCESS_ATTACH) → GGUFStream_EnableWirecap()
    └─ MainWindow_Create() → show UI
```

## Wirecap Output

**File**: `gguf_wirecap.bin` (in working directory)  
**Format**: Raw binary (no headers)  
**Size**: Grows monotonically during GGUF streaming

## Troubleshooting

| Problem | Check | Solution |
|---------|-------|----------|
| config.ini not found | Any | Create with template above |
| EnableBeacon=1 but no wirecap | DLL present? | Copy `gguf_beacon_spoof.dll` to same folder |
| Config value not read | Format correct? | Use `KEY=VALUE` (no spaces around `=`) |
| App won't start | DLL missing? | Not required; app works without it |

## Config Parser Properties

- **Parse Time**: ~1 ms (static buffer, single-pass)
- **Memory**: 4 KB file buffer + 256 B symbol table
- **Allocations**: Zero (in-place null termination)
- **Robustness**: Handles comments, empty lines, CRLF/LF, missing file

## Build System

**Main Script**: `build_production.ps1`  
**DLL Builder**: `build_beacon_spoof.ps1` (called automatically)  
**Output Dir**: `build/`  
**Modules**: 9 MASM files + DLL components

## Enterprise Features

✅ Zero-allocation config parser  
✅ Optional DLL loading (soft dependency)  
✅ Graceful failure modes  
✅ Pure MASM (no C/C++ runtime)  
✅ In-place tokenization  
✅ Comment & whitespace support  

## Quick Deploy

```powershell
# Copy to destination
cp build/AgenticIDEWin.exe "C:\Program Files\RawrXD\"
cp build/gguf_beacon_spoof.dll "C:\Program Files\RawrXD\"
cp build/config.ini "C:\Program Files\RawrXD\"

# Run
& "C:\Program Files\RawrXD\AgenticIDEWin.exe"
```

## Verify

```powershell
# Check artifacts exist
Test-Path .\build\AgenticIDEWin.exe
Test-Path .\build\gguf_beacon_spoof.dll
Test-Path .\build\config.ini
```

---

**Status**: ✅ Production Ready | **Build**: MASM32 | **Date**: December 21, 2025
