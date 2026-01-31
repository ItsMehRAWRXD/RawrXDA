# Titan Streaming Orchestrator - Build Troubleshooting Guide

## Prerequisites

Before building, ensure you have:
- Windows 10 x64 or later
- Visual Studio 2022 Enterprise/Community with C++ workload
- **Run from "x64 Native Tools Command Prompt for VS 2022"** (important!)

## Common Build Issues & Solutions

### 1. "ml64.exe not found" or "link.exe not found"

**Error Message:**
```
ERROR: ml64.exe not found in PATH
Solution: Run from "x64 Native Tools Command Prompt for VS 2022"
```

**Cause:** Missing MASM64 tools in PATH

**Fix:**
1. Close any current command prompt
2. Open **"x64 Native Tools Command Prompt for VS 2022"** from Windows Start menu
3. Navigate to the source directory
4. Run `titan_build.bat`

**Verify Tools:**
```cmd
where ml64
where link
```

---

### 2. "Undefined symbol" Errors

**Error Message:**
```
Linking error: unresolved external symbol 'GetProcessHeap'
Linking error: unresolved external symbol 'HeapAlloc'
```

**Cause:** Missing EXTERN declarations or wrong function names

**Verification:**
```cmd
REM Check if all EXTERN functions are declared
findstr "EXTERN" Titan_Streaming_Orchestrator_Fixed.asm
```

**Fix:** Ensure these imports exist in the .asm file:
```asm
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetFileSizeEx:PROC
EXTERN GetLastError:PROC
EXTERN SetLastError:PROC
EXTERN CloseHandle:PROC
; ... etc ...

includelib kernel32.lib
includelib ws2_32.lib
```

**Add Missing Libraries:**
If you get errors about socket functions:
```asm
; Add after other EXTERN declarations:
EXTERN WSAStartup:PROC
EXTERN WSACleanup:PROC
EXTERN socket:PROC
EXTERN bind:PROC
EXTERN sendto:PROC
EXTERN recvfrom:PROC
EXTERN closesocket:PROC

; Add after includelib:
includelib ws2_32.lib
```

---

### 3. "Invalid instruction operands" During Assembly

**Error Message:**
```
error A2070: invalid operand type
error A2070: invalid instruction operand
```

**Common Causes:**

#### a) Large Immediates
```asm
; WRONG - Constant too large for immediate:
cmp rax, 7000000000

; RIGHT - Load into register first:
mov r8, CONST_7B        ; From .DATA section
cmp rax, r8
```

**Status:** Already fixed in this version (CONST_7B, CONST_13B, etc. defined in .DATA)

#### b) Memory Operand Errors
```asm
; WRONG:
mov DWORD PTR [rcx], 1000000000    ; 32-bit can't hold this

; RIGHT:
mov r8d, 1000000000
mov [rcx], r8d
```

#### c) MOV Restrictions
```asm
; WRONG - immediate to memory in one instruction:
mov [rbx], 123456789

; RIGHT - use register as intermediate:
mov eax, 123456789
mov [rbx], eax
```

---

### 4. "Unwind info too complex" Errors

**Error Message:**
```
error A4068: unwind code exceeds maximum
error A4069: too many register unwind codes
```

**Cause:** PROC FRAME has too many saved registers or complex stack layout

**Current Fix (Already Applied):**
```asm
Titan_LockScheduler PROC FRAME
    push rbx
    .pushreg rbx           ; Register unwind info
    sub rsp, 8
    .allocstack 8          ; Stack allocation unwind info
    .endprolog             ; Signals end of prologue
    ; ... function body ...
Titan_LockScheduler ENDP
```

**If Still Getting Errors - Simplify:**
```asm
; Remove FRAME for leaf functions that don't need unwind:
MySimpleFunc PROC           ; No FRAME
    push rbx
    push rsi
    ; ... code ...
    pop rsi
    pop rbx
    ret
MySimpleFunc ENDP
```

---

### 5. "Alignment too large" or "Invalid OPTION" Errors

**Error Message:**
```
error A2165: illegal size for ALIGN
error A2011: invalid option in OPTION statement
```

**Current Fix:**
```asm
OPTION ALIGN:64         ; At very top of file
OPTION CASEMAP:NONE
OPTION WIN64:3
```

**Alternative (More Conservative):**
```asm
OPTION ALIGN:16         ; Safer default
OPTION CASEMAP:NONE
```

**In Code:**
```asm
; Use smaller alignments within procedures:
ALIGN 16                ; Not 64
MyVar QWORD 0
```

---

### 6. "Invalid stack offset" During Assembly

**Error Message:**
```
error A4071: unaligned stack
error A4070: rbp relative addressing
```

**Cause:** Stack not properly aligned or FRAME prologue/epilogue mismatch

**Verify Stack Alignment:**
```asm
Titan_InitScheduler PROC FRAME
    push rbx            ; -8 (now 8 bytes pushed)
    .pushreg rbx
    push rsi            ; -8 (now 16 bytes pushed)
    .pushreg rsi
    push rdi            ; -8 (now 24 bytes pushed)
    .pushreg rdi
    sub rsp, 56         ; Allocate 56 bytes
    .allocstack 56      ; Total stack: 56 + 24 = 80 bytes (16-byte aligned)
    .endprolog          ; Important!
    
    ; At entry: RSP is 8 bytes misaligned (80 is multiple of 16 but entry was misaligned)
    ; ... code ...
    
    add rsp, 56
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitScheduler ENDP
```

**Key Rule:**
- Entry RSP = 8 (mod 16)
- Total push count + sub rsp value must result in 16-byte boundary before `call`

---

### 7. LINK Errors - "Undefined External Symbol"

**Error Message:**
```
error LNK2001: unresolved external symbol 'GetProcessHeap'
error LNK2001: unresolved external symbol 'kernel32_something'
```

**Cause:** Missing libraries in link command

**Current Build Script Fix:**
```batch
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"build\bin\titan.exe" ^
      "build\obj\titan.obj" kernel32.lib ws2_32.lib
```

**To Add More Libraries:**
```batch
REM In titan_build.bat, change link line to:
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"build\bin\titan.exe" ^
      "build\obj\titan.obj" kernel32.lib ws2_32.lib user32.lib ^
      advapi32.lib shell32.lib shlwapi.lib
```

**Find Required Libraries:**
```cmd
REM Search online for function names:
REM GetProcessHeap -> kernel32.lib
REM WSAStartup -> ws2_32.lib
REM CreateWindowA -> user32.lib
REM RegOpenKeyA -> advapi32.lib
REM ShellExecuteA -> shell32.lib
```

---

### 8. "Invalid struct field" Errors

**Error Message:**
```
error A2088: structure fields are too complex
error A2134: undefined symbol in expression
```

**Cause:** Struct definitions are incomplete

**Current Fix (Already Applied):**
```asm
SRWLOCK STRUCT
    Ptr QWORD ?         ; Field name required
SRWLOCK ENDS

OVERLAPPED STRUCT
    Internal      QWORD ?
    InternalHigh  QWORD ?
    ; ... more fields ...
OVERLAPPED ENDS
```

**All Fields Must Be Named:**
```asm
; WRONG:
MYSTRUCT STRUCT
    QWORD ?
    DWORD ?
MYSTRUCT ENDS

; RIGHT:
MYSTRUCT STRUCT
    field1 QWORD ?
    field2 DWORD ?
MYSTRUCT ENDS
```

---

### 9. "File Encoding" Issues

**Error Message:**
```
error A1000: syntax error
```

**Or Assembly hangs/crashes mid-file**

**Cause:** File saved with BOM (Byte Order Mark) or wrong line endings

**Fix:**
1. Open file in VS Code
2. Bottom right: Check encoding indicator
3. Click encoding → **"Save with Encoding"**
4. Select **UTF-8 without BOM** or **ASCII**
5. Check line ending: **CRLF** (Windows style)

---

### 10. Runtime Errors (Build Succeeds but .exe Crashes)

**Symptoms:**
```
Segmentation Fault
Access Violation
The program stopped unexpectedly
```

**Common Causes:**

#### a) Stack Corruption
```asm
; WRONG - mismatched push/pop:
push rbx
call SomeFunc
; Missing: pop rbx
ret

; RIGHT:
push rbx
call SomeFunc
pop rbx
ret
```

#### b) Null Pointer Dereference
```asm
; Check for NULL returns:
call GetProcessHeap
test rax, rax           ; Verify heap handle
jz @@error              ; Handle NULL case

mov rbx, rax            ; Only use if non-NULL
mov rcx, rbx
call HeapAlloc
```

#### c) Invalid Register Usage
```asm
; WRONG - using uninitialized register:
mov [r12], rax          ; r12 was never set

; RIGHT:
mov r12, some_address
mov [r12], rax
```

**Debug with GDB (if available):**
```cmd
gdb build\bin\titan.exe
(gdb) run
(gdb) where
(gdb) info registers
```

---

## Build Process Detailed Steps

### Step 1: Verify Environment
```cmd
where ml64
where link
where kernel32.lib
```

### Step 2: Assembly Only (No Link)
```cmd
ml64 /c /Zd /Zi /Fo"build\obj\test.obj" "Titan_Streaming_Orchestrator_Fixed.asm"
```

**Success:** Creates `build\obj\test.obj`
**Failure:** Shows assembly errors

### Step 3: Link
```cmd
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"build\bin\test.exe" ^
     "build\obj\test.obj" kernel32.lib ws2_32.lib
```

**Success:** Creates `build\bin\test.exe`
**Failure:** Shows link errors (undefined symbols)

### Step 4: Verify Executable
```cmd
dir build\bin\test.exe
REM Should show file size > 10KB
```

### Step 5: Test Run
```cmd
build\bin\test.exe
echo %ERRORLEVEL%
REM 0 = success, 1 = init failed, >1 = crash
```

---

## Alternative: No-FRAME Build (Fallback)

If FRAME directives continue causing issues:

**Create `Titan_Streaming_Orchestrator_NoFrame.asm`:**

```asm
OPTION CASEMAP:NONE
OPTION WIN64:3

; Remove all PROC FRAME declarations
; Use regular PROC instead

Titan_LockScheduler PROC            ; No FRAME
    sub rsp, 8                      ; Align stack
    lea rcx, g_LockScheduler
    call AcquireSRWLockExclusive
    add rsp, 8
    ret
Titan_LockScheduler ENDP

; ... rest of functions without .pushreg/.allocstack/.endprolog ...

END
```

**Build:**
```cmd
ml64 /c /FoTest.obj Titan_Streaming_Orchestrator_NoFrame.asm
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:Test.exe Test.obj kernel32.lib ws2_32.lib
```

---

## Verification Checklist

- [ ] Running from "x64 Native Tools Command Prompt"
- [ ] `ml64 /c` command completes without errors
- [ ] `link` command completes without errors  
- [ ] `build\bin\titan.exe` file exists and is > 10KB
- [ ] `build\bin\titan.exe` runs without crashing
- [ ] `echo %ERRORLEVEL%` shows 0 or 1 (expected codes)

---

## Getting Help

### Information to Collect:
1. **Exact error message** (copy full text)
2. **Full command used** (what was typed)
3. **Environment** (VS version, Windows version)
4. **File encoding** (shown in VS Code bottom bar)

### Resources:
- Microsoft MASM64 Documentation: https://docs.microsoft.com/en-us/cpp/assembler/masm/
- x64 Calling Convention: https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention
- Stack Alignment: https://docs.microsoft.com/en-us/cpp/build/stack-frame-layout

---

## Success Indicators

✓ Assembly completes with no "error A" messages  
✓ Linking completes with no "error LNK" messages  
✓ Executable is created  
✓ Executable runs and exits cleanly  
✓ `ERRORLEVEL` is 0 or 1  

**If all these are true, the build succeeded!**
