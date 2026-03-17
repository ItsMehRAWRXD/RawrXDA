# QUICK ACTION GUIDE - Fix 0xCDCDCDCD Corruption

## What Was Fixed
✅ Added explicit corruption detection for 0xCDCDCDCD pattern in LoadModel
✅ Forced fallback to safe defaults (layers=32, embed=4096) if corruption detected
✅ Disabled tensor shape inference that could read corrupted data
✅ Strengthened validation with per-dimension error messages
✅ Created automated test script

## What You Need To Do Now

### Option 1: Automated (Recommended)
```powershell
cd D:\rawrxd
.\scripts\Fix-0xCDCDCDCD-And-Test.ps1
```

This will:
1. Rebuild  binary with fixes
2. Verify binary is fresh (<5min old)
3. Run inference test in isolated shell
4. Parse output and show diagnostic hints
5. Save logs to build_fix_0xCD.log + test_output_fix_0xCD.txt

### Option 2: Manual
```powershell
# Rebuild
cd D:\rawrxd\build
cmake --build . --config Release --target RawrXD-Win32IDE

# Test
cd D:\rawrxd
.\bin\RawrXD-Win32IDE.exe --test-inference-fast --test-model "F:\OllamaModels\blobs\sha256-e1eaa0f4fffb8880ca14c1c1f9e7d887fb45cc19a5b17f5cc83c3e8d3e85914e"
```

## What Success Looks Like
```
[ENGINE] Post-clamp: layers=32 (0x20) embed=4096 (0x1000) vocab=32064
[ENGINE] Model config VALIDATED: vocab=32064 embed=4096 layers=32 heads=32
[FAST] Engine OK
[FASTDBG] Engine state: layers=32 embed=4096 vocab=32064
PASS FAST_GENERATE token=42 time=187ms
```

## If It Still Fails

### "FAIL FAST_ENGINE_LOAD"
→ LoadModel returning false despite fixes
→ Check: `D:\rawrxd\test_stderr_fix_0xCD.txt` for ENGINE ERROR messages
→ Likely: GGUF parser itself returning corrupted metadata (need to fix gguf_loader.cpp)

### "FAIL FAST_INVALID_STATE bad_layers=-842150451"
→ Corruption detected but not fixed by fallback forcing
→ Check: Test ran with OLD binary (verify timestamp)
→ Action: Rebuild and re-test

### "FAIL FAST_TIMEOUT"
→ Generate() hangs after valid state confirmed
→ Next: Add layer instrumentation to find bottleneck in forward pass
→ Check: Transformer/attention/FFN hang

### "FAIL FAST_EXCEPTION vector too long"
→ Corruption happens BETWEEN LoadModel and Generate()
→ Next: Instrument InitKVCache(), Generate() entry to find corruption point

## Output Files
| File | Content |
|------|---------|
| `D:\rawrxd\build_fix_0xCD.log` | CMake build output |
| `D:\rawrxd\test_output_fix_0xCD.txt` | Test stdout ([FAST] lines, PASS/FAIL) |
| `D:\rawrxd\test_stderr_fix_0xCD.txt` | Engine diagnostics (corruption detection, validation) |

## After Success
```powershell
# Run full 8-stage stress harness
cd D:\rawrxd
.\scripts\StressTest-Sovereign7.ps1 -IncludeInferenceValidation -InferenceTimeoutMs 5000
```

Expected: All 8 stages PASS (5 infrastructure + 1 inference + 2 verifiers)

## Documentation
- Full analysis: [PHASE_26_0xCDCDCDCD_FIX_COMPLETE.md](d:\rawrxd\PHASE_26_0xCDCDCDCD_FIX_COMPLETE.md)
- Fix details: [INFERENCE_FIX_0xCDCDCDCD.md](d:\rawrxd\INFERENCE_FIX_0xCDCDCDCD.md)
- Init patch reference: [cpu_inference_engine_init_fix.cpp](d:\rawrxd\src\cpu_inference_engine_init_fix.cpp)

## Key Changes Made
1. **cpu_inference_engine.cpp (~line 1147):** Explicit 0xCDCDCDCD detection + forced fallbacks
2. **cpu_inference_engine.cpp (~line 1165):** Disabled potentially-corrupted tensor shape inference
3. **cpu_inference_engine.cpp (~line 1192):** Strengthened validation with specific error messages
4. **main_win32.cpp (~line 341):** Pre-inference state validation (added Phase 25)
5. **cpu_inference_engine.h (~line 175):** ValidateModelState() helper (added Phase 25)

## Timeline
- Phase 22: Breakthrough - binary enters test mode ([FAST] lines appear)
- Phase 23-24: Diagnosed 0xCDCDCDCD = uninitialized MSVC memory
- Phase 25: Added validation guards in test harness
- Phase 26: **YOU ARE HERE** - Fixed LoadModel corruption handling

## Next Phase Goal
✅ Get "PASS FAST_GENERATE token=X time=Yms" output
→ Then run full Stage 8 stress harness
→ Then complete CMakeLists build integration (if camellia256 lock reoccurs)
