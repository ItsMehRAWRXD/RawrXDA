# 🔧 RawrXD Native Model Bridge - Compilation Guide

## ⚡ Quick Start (Zero to DLL in 30 Minutes)

### **Phase 1: Pre-Flight Check (5 minutes)**

```powershell
# Verify MASM64 installation
ml64 /?
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ MASM64 not found. Install from: https://www.masm32.com/"
    exit 1
}

# Verify linker
link /?
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Link.exe not found. Install Visual Studio Build Tools"
    exit 1
}

Write-Host "✅ Build tools ready"
```

---

### **Phase 2: Critical Fixes (15 minutes)**

#### **Fix 1: Replace Include Directive**

**File**: `RawrXD_NativeModelBridge_v2.asm` (Lines 1-15)

**Before**:
```asm
include \masm64\include64\win64.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc
include \masm64\include64\ntdll.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ntdll.lib
includelib \masm64\lib64\user32.lib
```

**After**:
```asm
; Use our custom include with all API declarations
include win64_api.inc

; Library linkage (handled by linker command)
COMMENT @
includelib kernel32.lib
includelib ntdll.lib
includelib msvcrt.lib
@
```

---

#### **Fix 2: Procedure Syntax (Example: DllMain)**

**Before**:
```asm
DllMain PROC hInst:QWORD, fdwReason:DWORD, lpReserved:QWORD
    LOCAL sysInfo:SYSTEM_INFO
    
    .IF fdwReason == DLL_PROCESS_ATTACH
        ; ... code ...
```

**After**:
```asm
DllMain PROC FRAME
    ; Allocate locals + shadow space
    sub rsp, 100h
    .allocstack 100h
    .endprolog
    
    ; Parameters arrive in RCX, RDX, R8
    mov [rsp+20h], rcx      ; hInst
    mov [rsp+28h], edx      ; fdwReason
    mov [rsp+30h], r8       ; lpReserved
    
    cmp edx, DLL_PROCESS_ATTACH
    jne @@check_detach
    
    ; ... implementation ...
    
@@check_detach:
    cmp edx, DLL_PROCESS_DETACH
    jne @@done
    
    ; ... cleanup ...
    
@@done:
    add rsp, 100h
    ret
DllMain ENDP
```

**Pattern for ALL procedures**:
```asm
MyFunc PROC FRAME
    sub rsp, 80h           ; Allocate stack (must be 16-byte aligned)
    .allocstack 80h
    
    ; Save non-volatiles if needed
    mov [rsp+20h], rbx
    .savereg rbx, 20h
    
    .endprolog
    
    ; === Function body ===
    ; RCX = param1, RDX = param2, R8 = param3, R9 = param4
    ; Additional params at [rsp+stack_offset] after epilogue
    
    ; === Epilogue ===
    mov rbx, [rsp+20h]     ; Restore if saved
    add rsp, 80h
    ret
MyFunc ENDP
```

---

#### **Fix 3: API Calls with Shadow Space**

**Every single API call needs 32 bytes (20h) shadow space:**

**Before**:
```asm
mov rcx, lpPath
call CreateFileA        ; ❌ Stack not aligned
```

**After**:
```asm
mov rcx, lpPath
ALLOC_SHADOW           ; Macro expands to: sub rsp, 20h
call CreateFileA
FREE_SHADOW            ; Macro expands to: add rsp, 20h
```

**OR** use local frame:
```asm
MyFunc PROC FRAME
    sub rsp, 200h      ; Large frame = no need to adjust per call
    .allocstack 200h
    .endprolog
    
    mov rcx, lpPath
    call CreateFileA   ; Shadow space already allocated
    
    ; ... more calls ...
    
    add rsp, 200h
    ret
MyFunc ENDP
```

---

#### **Fix 4: Data Section Alignment**

**Before**:
```asm
.DATA?
ALIGN 64
g_modelCache QWORD 16 DUP(?)
```

**After** (Option A - Static):
```asm
.DATA
ALIGN 64
g_modelCache QWORD 16 DUP(0)   ; Initialize to zero
```

**After** (Option B - Dynamic, RECOMMENDED):
```asm
.DATA?
g_modelCache QWORD ?            ; Pointer to heap-allocated array

; In DllMain:
DllMain PROC FRAME
    ; ...
    cmp edx, DLL_PROCESS_ATTACH
    jne @@done
    
    ; Allocate 16 * 8 = 128 bytes
    mov ecx, 128
    ALLOC_SHADOW
    call malloc
    FREE_SHADOW
    mov g_modelCache, rax
    
@@done:
    ; ...
```

---

### **Phase 3: Build Script (5 minutes)**

Create `build.bat`:

```batch
@echo off
setlocal enabledelayedexpansion

echo ========================================
echo RawrXD Native Model Bridge - BUILD
echo ========================================
echo.

REM Step 1: Clean previous build
if exist *.obj del *.obj
if exist *.dll del *.dll
if exist *.lib del *.lib
if exist *.exp del *.exp
if exist *.pdb del *.pdb

REM Step 2: Assemble
echo [1/3] Assembling...
ml64 /c /Zi /O2 /arch:AVX2 /Fo"RawrXD_NativeModelBridge.obj" RawrXD_NativeModelBridge_v2.asm

if %ERRORLEVEL% neq 0 (
    echo ❌ Assembly failed!
    pause
    exit /b 1
)

echo ✅ Assembly successful

REM Step 3: Link
echo [2/3] Linking...
link /DLL ^
     /OUT:RawrXD_NativeModelBridge.dll ^
     /MACHINE:X64 ^
     /SUBSYSTEM:WINDOWS ^
     /ENTRY:DllMain ^
     /LARGEADDRESSAWARE ^
     /DEBUG:FULL ^
     /OPT:REF ^
     /OPT:ICF ^
     RawrXD_NativeModelBridge.obj ^
     kernel32.lib ^
     ntdll.lib ^
     msvcrt.lib

if %ERRORLEVEL% neq 0 (
    echo ❌ Linking failed!
    pause
    exit /b 1
)

echo ✅ Linking successful

REM Step 4: Verify
echo [3/3] Verifying...
if not exist RawrXD_NativeModelBridge.dll (
    echo ❌ DLL not found!
    exit /b 1
)

for %%F in (RawrXD_NativeModelBridge.dll) do set SIZE=%%~zF
echo ✅ DLL created: %SIZE% bytes

echo.
echo ========================================
echo 🎉 BUILD SUCCESSFUL
echo ========================================
dir RawrXD_NativeModelBridge.*

pause
```

---

### **Phase 4: Test Harness (5 minutes)**

Create `test_dll.ps1`:

```powershell
# Load DLL
Add-Type @"
using System;
using System.Runtime.InteropServices;

public class NativeBridge {
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr LoadModelNative(string path, out IntPtr ctx);
    
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int RunLocalModel(
        string endpoint,
        string prompt,
        IntPtr outBuf,
        int outSize
    );
    
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void UnloadModelNative(IntPtr ctx);
}
"@

# Test 1: DLL loads
Write-Host "✅ DLL loaded successfully"

# Test 2: Call function
$buffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(4096)
try {
    $result = [NativeBridge]::RunLocalModel(
        "test_model.gguf",
        "Hello, world!",
        $buffer,
        4096
    )
    
    if ($result -eq 1) {
        $output = [System.Runtime.InteropServices.Marshal]::PtrToStringAnsi($buffer)
        Write-Host "✅ Output: $output"
    } else {
        Write-Host "❌ Failed to run model"
    }
} finally {
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)
}
```

---

## 🐛 **Common Errors & Solutions**

### **Error: A2008: syntax error in expression**
**Cause**: Incorrect procedure syntax (USES with parameters)

**Solution**: Use FRAME pattern shown in Fix 2

---

### **Error: A2154: SRWLOCK is not defined**
**Cause**: Missing win64_api.inc

**Solution**: Ensure `include win64_api.inc` is first line after OPTION

---

### **Error: unresolved external symbol CreateFileA**
**Cause**: Missing library linkage

**Solution**: Add `kernel32.lib` to link command

---

### **Error: Stack pointer not aligned to 16 bytes**
**Cause**: Missing shadow space before API call

**Solution**: Use ALLOC_SHADOW/FREE_SHADOW macros

---

### **Error: A2070: invalid instruction operands**
**Cause**: Trying to use .IF/.ENDIF (HLL syntax not supported)

**Solution**: Replace with CMP/JE pattern

---

## 📊 **Priority Fix Checklist**

Complete these in order for fastest path to compilation:

- [ ] **P0**: Add `include win64_api.inc` (Line 1)
- [ ] **P0**: Fix `DllMain` procedure syntax (Lines 300-400)
- [ ] **P1**: Fix `LoadModelNative` procedure (Lines 600-900)
- [ ] **P1**: Fix `ForwardPass` procedure (Lines 1500-1700)
- [ ] **P2**: Fix all `USES` procedures (60+ functions)
- [ ] **P2**: Add shadow space to API calls (200+ sites)
- [ ] **P3**: Fix data section alignment (20+ globals)
- [ ] **P3**: Test on real GGUF file

---

## 🎯 **Expected Timeline**

| Task | Time | Cumulative |
|------|------|------------|
| Install includes | 2 min | 2 min |
| Fix DllMain | 5 min | 7 min |
| Fix 3 key procedures | 10 min | 17 min |
| First compile attempt | 2 min | 19 min |
| Fix compilation errors | 5 min | 24 min |
| Link DLL | 1 min | 25 min |
| Test loading | 5 min | 30 min |

---

## ✅ **Success Criteria**

**Compilation**:
```
0 errors
0 warnings
RawrXD_NativeModelBridge.dll (256 KB)
```

**Load Test**:
```powershell
$dll = [System.Reflection.Assembly]::LoadFile("$PWD\RawrXD_NativeModelBridge.dll")
$dll.GetExportedTypes()
# Should show exported functions
```

**Basic Inference**:
```
Input: "What is 2+2?"
Output: "The answer is 4."
Time: <5 seconds
Memory: <1GB
```

---

## 🚀 **Next Steps After Compilation**

1. **Optimize**: Replace stubs with actual AVX-512 kernels
2. **Profile**: Use VTune to find bottlenecks
3. **Benchmark**: Test on 7B/13B/70B models
4. **Stress Test**: 1000 generations without crash
5. **Production**: Deploy with monitoring

---

## 📞 **Support**

Issues? Check:
1. `PRODUCTION_FIXES_REQUIRED.md` - Detailed fixes
2. `win64_api.inc` - All API definitions
3. Assembly log: `ml64 /Fl output.lst`
4. Link map: `link /MAP`

---

**REMEMBER**: Focus on **compiling first**, then optimize. Don't get stuck perfecting code that doesn't build yet!
