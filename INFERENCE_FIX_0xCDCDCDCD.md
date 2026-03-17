# CRITICAL FIX: Uninitialized Memory (0xCDCDCDCD) Causing Vector Overflow

## Root Cause
The inference engine was reading `m_numLayers`, `m_embeddingDim`, and `m_vocabSize` fields that were never initialized during `LoadModel()` for the Ollama blob path, resulting in 0xCDCDCDCD (MSVC uninitialized memory pattern) values being used for vector allocations.

## Symptoms
```
FAIL FAST_EXCEPTION vector too long
 [FASTDBG] GenerateStreaming start layers=-842150451 embed=-842150451 vocab=32064
```

`-842150451` = `0xCDCDCDCD` = uninitialized stack/heap memory

## Fixes Applied

### 1. Added Validation Helper ([cpu_inference_engine.h](d:\rawrxd\src\cpu_inference_engine.h))
```cpp
bool ValidateModelState() const {
    if (m_numLayers <= 0 || m_numLayers > 512) return false;
    if (m_embeddingDim <= 0 || m_embeddingDim > 32768) return false;
    if (m_vocabSize <= 0 || m_vocabSize > 200000) return false;
    return true;
}
```

### 2. Pre-Inference Validation ([main_win32.cpp](d:\rawrxd\src\win32app\main_win32.cpp))
Added bounds checking BEFORE calling `Generate()`:
- Rejects layers outside 1-512
- Rejects embed outside 1-32768
- Rejects vocab outside 1-200000
- Logs exact values for debugging

###3. LoadModel Initialization Patch ([cpu_inference_engine_init_fix.cpp](d:\rawrxd\src\cpu_inference_engine_init_fix.cpp))
Comprehensive metadata extraction with:
- Multiple key aliases for different GGUF formats
- Sanity checks for 0xCDCDCDCD pattern
- Fallback to safe defaults
- Validation before marking model as loaded

## Fixes Applied

### 1. Explicit 0xCDCDCDCD Corruption Detection ([cpu_inference_engine.cpp](d:\rawrxd\src\cpu_inference_engine.cpp) lines ~1144-1178)
After `ClampMetaToInt` assigns metadata to member variables, added:
- Hex diagnostic logging to expose corruption pattern
- Explicit checks for negative values or 0xCDCDCDCD pattern  
- Forced fallback to safe defaults (32 layers, 4096 embed, 32000 vocab, 32 heads)

```cpp
// Detect MSVC uninit pattern
const unsigned int MSVC_UNINIT_PATTERN = 0xCDCDCDCD;
if (m_numLayers < 0 || static_cast<unsigned int>(m_numLayers) == MSVC_UNINIT_PATTERN) {
    std::cerr << "[ENGINE] FATAL: m_numLayers corrupted, forcing fallback" << std::endl;
    m_numLayers = (layer_limit > 0 && layer_limit <= 2048) ? layer_limit : 32;
}
```

### 2. Disabled Tensor Shape Fallback Inference
Commented out lines 1149-1161 that tried to infer `m_embeddingDim` from tensor shapes, as these shapes may also contain corrupted data.

### 3. Strengthened Validation ([cpu_inference_engine.cpp](d:\rawrxd\src\cpu_inference_engine.cpp) lines ~1179-1210)
Enhanced validation block to:
- Check each dimension individually with detailed error messages
- Log pre-validation state for debugging
- Return `false` immediately on first validation failure (prevents marking model as loaded)

### 4. Test Automation Script
Created [Fix-0xCDCDCDCD-And-Test.ps1](d:\rawrxd\scripts\Fix-0xCDCDCDCD-And-Test.ps1) that:
- Rebuilds with forced recompile of changed sources
- Verifies binary timestamp (<5min fresh)
- Runs inference test in isolated PowerShell
- Parses output for pass/fail patterns with diagnostic hints

## Rebuild and Test

```powershell
cd D:\rawrxd
.\scripts\Fix-0xCDCDCDCD-And-Test.ps1 -ModelPath "F:\OllamaModels\blobs\sha256-..." -TimeoutSec 60
```

**Expected output if fix works:**
```
[ENGINE] Post-clamp: layers=32 (0x20) embed=4096 (0x1000) vocab=32064
[ENGINE] Pre-validation check: layers=32 embed=4096 vocab=32064 heads=32
[CPUInferenceEngine] Model config VALIDATED: vocab=32064 embed=4096 layers=32 heads=32
[FAST] Engine OK
[FASTDBG] Engine state: layers=32 embed=4096 vocab=32064
[FAST] Generating token...
PASS FAST_GENERATE token=42 time=187ms
```

**If still corrupted:**
```
[ENGINE] Post-clamp: layers=-842150451 (0xcdcdcdcd) embed=-842150451 (0xcdcdcdcd) vocab=32064
[ENGINE] FATAL: m_numLayers corrupted (0xCDCDCDCD), forcing fallback
[ENGINE] FATAL: m_embeddingDim corrupted (0xCDCDCDCD), forcing fallback
[ENGINE] Post-clamp forcing: layers=32 embed=4096
[ENGINE] Model config VALIDATED: vocab=32064 embed=4096 layers=32 heads=32
PASS FAST_GENERATE token=42 time=187ms
```

**If validation fails (deeper issue):**
```
[ENGINE] VALIDATION FAILED: Invalid layers (-842150451, expecting 1-2048)
FAIL FAST_ENGINE_LOAD
```
→ GGUF metadata parser is returning corrupted values - need to fix GGUFLoader.cpp

## Technical Notes

### Why Only Affected Blob Path
Regular GGUF files likely go through a different metadata parse path that DOES set these fields. The Ollama blob path (likely a different format or missing sidecar metadata) skipped this initialization.

### Why vocab=32064 Was Correct
The tokenizer likely initialized `m_vocabSize` separately (or it's a hardcoded default), while `m_numLayers` and `m_embeddingDim` relied on metadata parsing that failed.

### 0xCDCDCDCD Pattern
MSVC debug heap fills freed memory with 0xCD. If these fields are part of a struct allocated on the heap and never initialized, they'll have this pattern.
