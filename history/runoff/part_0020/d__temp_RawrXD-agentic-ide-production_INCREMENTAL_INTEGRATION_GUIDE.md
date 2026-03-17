# ============================================================================
# Incremental MASM Module Integration Guide
# ============================================================================

## Current Status: Phase 1 Complete ✅

**Working:** Minimal production stub with all 26 exports
**DLL Size:** 11 KB
**Dependencies:** Zero (pure x64 MASM)

---

## Integration Phases

### ✅ Phase 1: Minimal Stub (COMPLETE)
**File:** `src/masm_pure/agentic_core_minimal.asm`
**Status:** Production-ready, all exports functional
**Functions:** 26 (IDEMaster, BrowserAgent, HotPatch, AgenticIDE)

---

### 🔄 Phase 2: Macro Conversion Required

The 18 converted MASM files in `src/masm_pure/` use **MASM32 macros** that aren't supported by ml64:

**Problematic Patterns:**
- `invoke FunctionName, arg1, arg2` → MASM32 macro (not in ml64)
- `.if eax == 0` / `.endif` → MASM32 flow control macros
- `CRITICAL_SECTION` structures → MASM32 SDK types

**Solution:** Manual conversion to raw x64 MASM

---

## Conversion Requirements for Each Module

### Pattern 1: Replace `invoke` with direct calls

**Before (MASM32):**
```asm
invoke InitializeCriticalSection, ADDR g_csAgent
invoke EnterCriticalSection, ADDR g_csAgent
```

**After (Pure x64):**
```asm
lea rcx, g_csAgent
call InitializeCriticalSection

lea rcx, g_csAgent
call EnterCriticalSection
```

### Pattern 2: Replace `.if/.endif` with labels

**Before (MASM32):**
```asm
.if eax == 0
    ; error handling
.endif
```

**After (Pure x64):**
```asm
test eax, eax
jnz @F
    ; error handling
@@:
```

### Pattern 3: Replace structure types with raw data

**Before (MASM32):**
```asm
g_csAgent CRITICAL_SECTION <>
```

**After (Pure x64):**
```asm
; CRITICAL_SECTION is 40 bytes on x64
g_csAgent db 40 dup(0)
```

---

## Module Integration Order (Recommended)

### Batch 1: Core Infrastructure
1. ✅ `agentic_core_minimal.asm` - Working stub
2. ⏳ `error_logging_enhanced.asm` - Convert first (no dependencies)
3. ⏳ `agent_system_core.asm` - Simple, few externals

### Batch 2: Tool System
4. ⏳ `tool_registry_full.asm` - Tool definitions
5. ⏳ `tool_dispatcher_complete.asm` - Tool execution

### Batch 3: Browser Automation
6. ⏳ `autonomous_browser_agent.asm` - Browser control
7. ⏳ `autonomous_agent_system.asm` - Agent orchestration

### Batch 4: Model Management
8. ⏳ `model_hotpatch_engine.asm` - Hot-patching
9. ⏳ `gguf_loader_unified.asm` - GGUF loading
10. ⏳ `gguf_chain_loader_unified.asm` - Chain loading

### Batch 5: Advanced Features
11. ⏳ `inference_backend_selector.asm` - Backend selection
12. ⏳ `piram_compress.asm` - Compression
13. ⏳ `reverse_quant.asm` - Quantization
14. ⏳ `action_executor_enhanced.asm` - Action execution

### Batch 6: UI Integration
15. ⏳ `qt_ide_integration.asm` - Qt bindings
16. ⏳ `qt_pane_system.asm` - Pane management
17. ⏳ `ui_gguf_integration.asm` - UI components

### Batch 7: Master Integration
18. ⏳ `ide_master_integration.asm` - Full orchestration
19. ⏳ `agentic_ide_full_control.asm` - Complete control

---

## Automated Macro Converter (Future Enhancement)

Create `scripts/convert_masm32_to_pure_x64.ps1`:

```powershell
# Convert MASM32 macros to pure x64 MASM
param([string]$File)

$content = Get-Content $File -Raw

# Convert invoke (2-arg)
$content = $content -replace 'invoke\s+(\w+),\s*(\w+)', 'mov rcx, $2`r`ncall $1'

# Convert .if/.endif
$content = $content -replace '\.if\s+(\w+)\s*==\s*0', 'test $1, $1`r`njnz @F'
$content = $content -replace '\.endif', '@@:'

# Remove CRITICAL_SECTION types
$content = $content -replace 'CRITICAL_SECTION\s+<>', 'db 40 dup(0)'

$content | Out-File $File -Encoding ASCII
```

---

## Current Build Command

```powershell
cmake --build "D:\temp\RawrXD-agentic-ide-production\build" `
    --config Release `
    --target RawrXD-SovereignLoader-Agentic
```

**Output:** `build/bin/Release/RawrXD-SovereignLoader-Agentic.dll` (11 KB)

---

## Testing Incremental Changes

### Step 1: Convert one module
```powershell
# Example: Convert error_logging_enhanced.asm
.\scripts\convert_masm32_to_pure_x64.ps1 `
    -File "src\masm_pure\error_logging_enhanced.asm"
```

### Step 2: Add to CMakeLists.txt
```cmake
set(AGENTIC_MASM_SOURCES
    ${CMAKE_SOURCE_DIR}/src/masm_pure/agentic_core_minimal.asm
    ${CMAKE_SOURCE_DIR}/src/masm_pure/error_logging_enhanced.asm  # ← Add
    ${CMAKE_SOURCE_DIR}/kernels/qt-bridge/qt_bridge.asm
)
```

### Step 3: Build and test
```powershell
cmake --build build --config Release --target RawrXD-SovereignLoader-Agentic
```

### Step 4: Verify exports
```powershell
Get-Item "build\bin\Release\RawrXD-SovereignLoader-Agentic.dll" | 
    Select Name, Length
```

---

## Success Criteria Per Module

- ✅ Compiles without errors
- ✅ No linker warnings
- ✅ DLL size increases appropriately
- ✅ All previous exports still present
- ✅ New functions callable from C/C++

---

## Quick Reference: x64 Calling Convention

**Parameters (first 4):** RCX, RDX, R8, R9  
**Parameters (5+):** Stack (right-to-left)  
**Return value:** RAX  
**Shadow space:** 32 bytes (caller allocates)  
**Stack alignment:** 16-byte boundary before call

**Example:**
```asm
; void MyFunction(int a, int b, int c, int d, int e)
MyFunction PROC
    ; RCX = a, RDX = b, R8 = c, R9 = d
    ; [rsp+32] = e (after shadow space)
    
    mov eax, ecx        ; Use parameter a
    add eax, edx        ; Add parameter b
    
    ret
MyFunction ENDP
```

---

## Current Limitations

1. **MASM32 macros** - Not supported by ml64, need manual conversion
2. **Structure definitions** - Must use raw byte arrays
3. **External dependencies** - All modules must be self-contained
4. **Calling conventions** - Must use x64 (not stdcall)

---

## When Fully Integrated

**Expected outcome:**
- All 18 modules compiled
- Full agentic capabilities active
- DLL size: ~100-200 KB
- 58+ tool functions available
- Browser automation working
- Model hot-swapping enabled
- GGUF loading operational

---

**Current recommendation:** Use the working minimal stub in production while incrementally converting and testing each module.
