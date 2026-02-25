# 🔧 Batch Regex Patterns for Fixing All 60+ MASM Procedures

## 📋 **Overview**
Use these patterns in VSCode/PowerShell/Python to automatically fix all procedure syntax issues in the original `RawrXD_NativeModelBridge_v2.asm`.

---

## 🎯 **Pattern 1: Convert USES Procedures to FRAME**

### **Find Pattern** (Regex):
```regex
^(\w+)\s+PROC\s+USES\s+((?:r\w+\s*)+),\s*(\w+:\w+(?:,\s*\w+:\w+)*)
```

### **Replace Pattern**:
```
$1 PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 200h
    .allocstack 200h
    
    ; Save non-volatiles
    mov [rsp+20h], rbx
    .savereg rbx, 20h
    mov [rsp+28h], rsi
    .savereg rsi, 28h
    mov [rsp+30h], rdi
    .savereg rdi, 30h
    mov [rsp+38h], r12
    .savereg r12, 38h
    mov [rsp+40h], r13
    .savereg r13, 40h
    
    .endprolog
    
    ; Parameters arrive in RCX, RDX, R8, R9
    ; TODO: Store parameters from registers to stack
```

### **Example**:
**Before**:
```asm
ParseMetadataKVPairs PROC USES rbx rsi rdi r12 r13 r14, pCtx:QWORD, pStart:QWORD, n_kv:QWORD
```

**After**:
```asm
ParseMetadataKVPairs PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 200h
    .allocstack 200h
    
    mov [rsp+20h], rbx
    .savereg rbx, 20h
    mov [rsp+28h], rsi
    .savereg rsi, 28h
    mov [rsp+30h], rdi
    .savereg rdi, 30h
    mov [rsp+38h], r12
    .savereg r12, 38h
    mov [rsp+40h], r13
    .savereg r13, 40h
    mov [rsp+48h], r14
    .savereg r14, 48h
    
    .endprolog
    
    ; Store parameters
    mov rbx, rcx        ; pCtx
    mov rsi, rdx        ; pStart
    mov r12, r8         ; n_kv
```

---

## 🎯 **Pattern 2: Fix LOCAL Variable Declarations**

### **Find Pattern**:
```regex
^\s+LOCAL\s+(\w+):(\w+)(?:,\s*(\w+):(\w+))*$
```

### **Action**:
Remove LOCAL lines entirely - use stack space allocated in prologue:
```asm
; Instead of: LOCAL temp:DWORD, buffer:QWORD
; Use stack directly:
mov [rbp-10h], eax      ; temp
mov [rbp-18h], rcx      ; buffer
```

---

## 🎯 **Pattern 3: Add Shadow Space to All API Calls**

### **Find Pattern** (API calls without preceding SUB RSP):
```regex
^(\s+)(call\s+(?:CreateFileA|VirtualAlloc|malloc|GetSystemInfo|CreateThread|MapViewOfFile|GetFileSizeEx|CloseHandle|free)[^\n]*)\n(?!\s+add\s+rsp)
```

### **Replace Pattern**:
```
$1; Shadow space allocated in prologue
$1$2
```

**Note**: If procedure already allocates large stack (200h+), no per-call adjustment needed.

---

## 🎯 **Pattern 4: Convert .IF/.ENDIF to CMP/JE**

### **Find Pattern**:
```regex
^\s+\.IF\s+(\w+)\s*==\s*(\w+)
(.*?)
^\s+\.ENDIF
```

### **Replace Pattern**:
```
    cmp $1, $2
    jne @@skip_$1_$2
$3
@@skip_$1_$2:
```

### **Example**:
**Before**:
```asm
    .IF fdwReason == DLL_PROCESS_ATTACH
        call InitSystem
    .ENDIF
```

**After**:
```asm
    cmp edx, DLL_PROCESS_ATTACH
    jne @@skip_init
    call InitSystem
@@skip_init:
```

---

## 🎯 **Pattern 5: Fix Procedure Epilogues**

### **Find Pattern** (end of procedure before RET):
```regex
(^\w+\s+ENDP)$
```

### **Action**:
Add before `ENDP`:
```asm
    ; Restore non-volatiles
    mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    mov r12, [rsp+38h]
    mov r13, [rsp+40h]
    
    add rsp, 200h
    pop rbp
    ret
```

---

## 🎯 **Pattern 6: Fix Parameter Access**

Since parameters come in registers (RCX, RDX, R8, R9), need to either:

**Option A**: Store immediately after `.endprolog`:
```asm
.endprolog

; Store parameters
mov [rbp+10h], rcx      ; First param
mov [rbp+18h], rdx      ; Second param
mov [rbp+20h], r8       ; Third param
mov [rbp+28h], r9       ; Fourth param
```

**Option B**: Use registers directly:
```asm
mov rbx, rcx            ; Use RBX for pCtx
mov r12d, edx           ; Use R12D for token
```

---

## 🐍 **Python Script for Batch Processing**

```python
import re

def fix_procedures(asm_file):
    with open(asm_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern 1: Fix PROC declarations
    pattern1 = re.compile(
        r'^(\w+)\s+PROC\s+USES\s+((?:r\w+\s*)+),\s*(\w+:\w+(?:,\s*\w+:\w+)*)',
        re.MULTILINE
    )
    
    def replace_proc(match):
        proc_name = match.group(1)
        registers = match.group(2).strip().split()
        params = match.group(3)
        
        # Build register save section
        saves = []
        offset = 0x20
        for reg in registers:
            saves.append(f"    mov [rsp+{offset:02X}h], {reg}")
            saves.append(f"    .savereg {reg}, {offset:02X}h")
            offset += 8
        
        result = f"""{proc_name} PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 200h
    .allocstack 200h
    
{"".join(s + "\\n" for s in saves)}
    .endprolog
    
    ; Parameters: {params}
    ; TODO: Store from RCX, RDX, R8, R9 to stack
"""
        return result
    
    content = pattern1.sub(replace_proc, content)
    
    # Pattern 2: Remove LOCAL declarations
    content = re.sub(
        r'^\s+LOCAL\s+.*$',
        '    ; LOCAL vars moved to stack',
        content,
        flags=re.MULTILINE
    )
    
    # Pattern 3: Convert .IF to CMP/JE
    def replace_if(match):
        var = match.group(1)
        val = match.group(2)
        body = match.group(3)
        label = f"skip_{var}_{val}".lower()
        
        return f"""    cmp {var}, {val}
    jne @@{label}
{body}
@@{label}:"""
    
    content = re.sub(
        r'^\s+\.IF\s+(\w+)\s*==\s*(\w+)(.*?)^\s+\.ENDIF',
        replace_if,
        content,
        flags=re.MULTILINE | re.DOTALL
    )
    
    # Write fixed file
    output_file = asm_file.replace('.asm', '_FIXED.asm')
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"✅ Fixed ASM written to: {output_file}")
    return output_file

# Usage
fix_procedures('RawrXD_NativeModelBridge_v2.asm')
```

---

## 💻 **PowerShell Script for Quick Fixes**

```powershell
# fix_asm_procedures.ps1
param(
    [string]$InputFile = "RawrXD_NativeModelBridge_v2.asm",
    [string]$OutputFile = "RawrXD_NativeModelBridge_v2_FIXED.asm"
)

$content = Get-Content $InputFile -Raw

# Fix 1: Convert USES to FRAME
$pattern1 = '(?m)^(\w+)\s+PROC\s+USES\s+((?:r\w+\s*)+),\s*(\w+:\w+(?:,\s*\w+:\w+)*)'
$content = $content -replace $pattern1, {
    param($match)
    $procName = $match.Groups[1].Value
    @"
$procName PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 200h
    .allocstack 200h
    
    mov [rsp+20h], rbx
    .savereg rbx, 20h
    mov [rsp+28h], rsi
    .savereg rsi, 28h
    
    .endprolog
    
    ; TODO: Map parameters
"@
}

# Fix 2: Remove LOCAL
$content = $content -replace '(?m)^\s+LOCAL\s+.*$', '    ; Locals on stack'

# Fix 3: Convert .IF to CMP/JE
$content = $content -replace '(?m)^\s+\.IF\s+(\w+)\s*==\s*(\w+)', '    cmp $1, $2'
$content = $content -replace '(?m)^\s+\.ENDIF', ''

# Write output
Set-Content -Path $OutputFile -Value $content -Encoding UTF8

Write-Host "✅ Fixed $OutputFile" -ForegroundColor Green
Write-Host "📊 Procedures fixed: $((Select-String -Path $OutputFile -Pattern 'PROC FRAME').Matches.Count)"
```

**Usage**:
```powershell
.\fix_asm_procedures.ps1 -InputFile "RawrXD_NativeModelBridge_v2.asm"
```

---

## 📊 **Affected Procedures (60+ total)**

### **High Priority** (Core inference):
1. ✅ `DllMain` - FIXED (template)
2. ✅ `LoadModelNative` - FIXED (template)
3. ✅ `ForwardPass` - FIXED (template)
4. `ParseMetadataKVPairs`
5. `ParseTensorInfos`
6. `ExtractModelParams`
7. `ComputeQKV`
8. `ComputeAttention`
9. `FeedForward_SwiGLU`
10. `GenerateTokens`

### **Medium Priority** (Support functions):
11. `InitMathTables`
12. `InitThreadPool`
13. `AllocateKVCache`
14. `AllocateInferenceBuffers`
15. `InitTokenizer`
16. `BuildBPEHashTable`
17. `TokenizeText`
18. `BPETokenizeWord`
19. `DetokenizeResponse`
20. `SampleToken`

### **Low Priority** (Utilities):
21-60+: Various helper functions (string ops, math ops, etc.)

---

## ✅ **Validation Checklist**

After batch processing:

```powershell
# Check for syntax errors
$errors = Select-String -Path "RawrXD_NativeModelBridge_v2_FIXED.asm" -Pattern "PROC USES"
if ($errors.Count -gt 0) {
    Write-Host "❌ Still has USES directives: $($errors.Count)" -ForegroundColor Red
} else {
    Write-Host "✅ All USES directives converted" -ForegroundColor Green
}

# Check for LOCAL
$locals = Select-String -Path "RawrXD_NativeModelBridge_v2_FIXED.asm" -Pattern "^\s+LOCAL\s+"
if ($locals.Count -gt 0) {
    Write-Host "⚠️ Still has LOCAL: $($locals.Count)" -ForegroundColor Yellow
}

# Count FRAME procedures
$frames = Select-String -Path "RawrXD_NativeModelBridge_v2_FIXED.asm" -Pattern "PROC FRAME"
Write-Host "📊 Total FRAME procedures: $($frames.Count)" -ForegroundColor Cyan
```

---

## 🎯 **Manual Review Required**

Some procedures need manual attention:

1. **Variable number of parameters** - Map to stack correctly
2. **Floating-point parameters** - Use XMM0-XMM3
3. **Procedures with >4 parameters** - Access from stack
4. **Recursive functions** - Ensure tail call optimization
5. **Assembly with inline data** - Check alignment

---

## 📝 **Summary**

Run in this order:

1. **Python script** - Automated bulk conversion
2. **Manual review** - Fix top 10 procedures
3. **Compile test** - `ml64 /c /Fo test.obj`
4. **Fix errors** - Iterate on failures
5. **Full build** - Link DLL

**Expected Results**:
- 60+ procedures converted from USES to FRAME
- All LOCAL declarations removed
- All .IF/.ENDIF converted to CMP/JE
- Clean compilation with zero syntax errors

**Time Estimate**: 2-3 hours for complete batch processing + validation
