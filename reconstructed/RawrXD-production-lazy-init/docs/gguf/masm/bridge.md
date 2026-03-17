# GGUF MASM Bridge

This bridge dynamically loads `RawrXD_GGUF_RobustTools.dll` and uses the MASM metadata parser when available.

## Environment Flags

- `RAWRXD_GGUF_MASM_METADATA=1` enables the MASM metadata path.
- `RAWRXD_GGUF_MASM_DLL` overrides the DLL path.
- `RAWRXD_GGUF_ROBUST_METADATA=0` disables the C++ robust fallback.
- `RAWRXD_GGUF_ROBUST_VERBOSE=1` enables verbose logs.

## Smoke Test

Build a tiny loader that only verifies the DLL can be found.

```pwsh
cl /EHsc /std:c++20 /I D:\RawrXD-production-lazy-init\include D:\RawrXD-production-lazy-init\tools\gguf_masm_bridge_smoke.cpp
```

Run:

```pwsh
.\gguf_masm_bridge_smoke.exe
```
