# RawrXD Amphibious Hybrid — Integration Summary

## Status: Option C implemented

**Unified hybrid mode**: Try real Ollama inference first (5s timeout), auto-fallback to simulation, report mode in telemetry.

---

## Changes in this repo

### 1. `RawrXD_InferenceAPI.asm` (hybrid real + sim)

- **WinHTTP connect timeout**: `WinHttpSetOption(..., WINHTTP_OPTION_CONNECT_TIMEOUT, 5000)` so connect fails fast if Ollama is down.
- **Init always returns 1**: On connect failure, `g_hHttpConnect = 0`, `g_InferenceModeSim = 1`, still returns 1 so ChatService keeps going.
- **Generate**: If `g_hHttpConnect == 0`, writes simulation message to output buffer and returns; otherwise streams from Ollama.
- **Telemetry**: `OutputDebugStringA` for `[MODE] real (Ollama)` or `[MODE] simulation (Ollama unavailable)`.
- **RawrXD_Inference_GetMode**: New export; returns 0 = real, 1 = simulation (for callers that need mode).
- **Tokenizer stubs**: `RawrXD_Tokenizer_Encode` and `RawrXD_Tokenizer_Decode` added so ChatService can link against InferenceAPI only (no ML_Runtime).
- **Labels**: Dot-prefix local labels replaced with unique names for VS `ml64` compatibility.

### 2. `Build_Amphibious_Hybrid.ps1`

- Builds a hybrid CLI using **RawrXD_InferenceAPI.asm** instead of **RawrXD_ML_Runtime.asm**.
- Links `winhttp.lib` for WinHTTP.
- Output: `build\amphibious\RawrXD_CLI_Hybrid.exe`.
- **Requirement**: Same as AutoHeal — `RawrXD_AgentHost_Sovereign.asm` and others must assemble (e.g. MASM32/masm64rt.inc if used).

---

## Build and run

### Assemble InferenceAPI only (no MASM32)

```powershell
cd D:\rawrxd
$ml64 = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
& $ml64 /nologo /c /Fo build_out\inference_api.obj RawrXD_InferenceAPI.asm
```

### Full hybrid CLI (when AgentHost etc. build)

```powershell
.\Build_Amphibious_Hybrid.ps1
# Run:
.\build\amphibious\RawrXD_CLI_Hybrid.exe
```

### Sovereign-only CLI (no inference, no Ollama)

```powershell
.\Build_Amphibious.ps1
.\build_out\RawrXD_CLI.exe
```

---

## API for telemetry

- **RawrXD_Inference_GetMode**: Call after `RawrXD_Inference_Init`. Returns `EAX = 0` (real) or `1` (simulation).
- **g_InferenceModeSim**: Global DWORD; 0 = real, 1 = simulation.

---

## File manifest (Option C)

| File | Purpose |
|------|---------|
| `RawrXD_InferenceAPI.asm` | HTTP bridge + timeout + sim fallback + GetMode + tokenizer stubs |
| `Build_Amphibious_Hybrid.ps1` | Build script for hybrid CLI (InferenceAPI + winhttp) |
| `Build_Amphibious.ps1` | Sovereign-only CLI (no HTTP) |
| `Build-AutoHeal-Amphibious.ps1` | AutoHeal CLI (ML_Runtime sim only) |

---

## Success criteria

| Criterion | Status |
|-----------|--------|
| Connect timeout (no hang) | Done (5s) |
| Init returns 1 on failure; Generate uses sim | Done |
| [MODE] real / simulation in telemetry | Done |
| RawrXD_Inference_GetMode export | Done |
| Tokenizer_Encode/Decode stubs | Done |
| InferenceAPI assembles with VS ml64 | Done |
