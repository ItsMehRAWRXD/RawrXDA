# ASSEMBLY VALIDATION CHECKLIST

**File:** `encoder_host_final.asm`  
**Location:** `D:\RawrXD-Compilers\encoder_host_final.asm`  
**Status:** Ready for ml64.exe assembly

---

## Pre-Assembly Verification

### ✅ File Integrity
- [x] File exists at correct path
- [x] File size: ~15KB (496 lines)
- [x] Character encoding: UTF-8 (MASM compatible)
- [x] Line endings: CRLF (Windows standard)
- [x] Last line: `END main` (entry point defined)

### ✅ Syntax Validation (Code Review)
- [x] OPTION CASEMAP:NONE (case-sensitive)
- [x] OPTION WIN64:11 (64-bit target)
- [x] All EXTERN declarations present:
  - GetProcessHeap
  - HeapAlloc
  - GetStdHandle
  - WriteFile
  - ExitProcess
- [x] All constants defined (REX_, MOD_, TOK_, MACRO_MAX_*)
- [x] State variables declared (.data?, .data sections)
- [x] All procedures have PROC FRAME ... ENDP
- [x] All labels are valid (no reserved keywords)
- [x] No duplicate procedure names
- [x] Call targets valid (all named procedures exist)

### ✅ Calling Convention Compliance
- [x] All public procs: PROC FRAME keyword
- [x] Shadow space: Allocated where needed (sub rsp, 28h typically)
- [x] Parameter passing: RCX/RDX/R8/R9 only
- [x] Register preservation: push/pop balanced
- [x] Stack alignment: 16-byte boundaries maintained
- [x] Return values: AL (byte), RAX (qword), or RDX:RAX (128-bit)

### ✅ Instruction Correctness
- [x] Emit_MOV_R64_R64: Uses opcode `8B` (not 89)
- [x] Emit_MOV_R64_IMM64: Uses opcode `B8` + reg
- [x] Emit_ADD_R64_R64: Uses opcode `01`
- [x] REX prefix calculation: Correct bit operations
- [x] ModRM encoding: Correct field alignment
- [x] No invalid x64 instructions (no 32-bit opcodes for 64-bit ops)

---

## Assembly Command

### Primary Command (Recommended)
```powershell
cd D:\RawrXD-Compilers
& "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\ml64.exe" /c encoder_host_final.asm
```

### Fallback Command (If path changed)
```powershell
Get-ChildItem -Path "C:\Program Files*" -Recurse -Filter "ml64.exe" -EA SilentlyContinue | Select-Object -First 1 | ForEach-Object { & $_.FullName /c D:\RawrXD-Compilers\encoder_host_final.asm }
```

### Alternative: Check if ml64 in PATH
```powershell
where.exe ml64.exe
```

---

## Expected Assembly Output

### ✅ Success Scenario
```
Output:
  (No error messages)
  
Files Created:
  D:\RawrXD-Compilers\encoder_host_final.obj          (object file)
  (Optional: encoder_host_final.asm.lst if /B flag used)

Exit Code: 0 (success)
```

### ❌ Failure Scenarios & Recovery

#### Error A2008: Syntax Error
```
Possible causes:
  - Invalid mnemonic (check opcode names: mov, add, ret, nop)
  - Malformed instruction (check operands)
  - Reserved keyword used as label/variable
  
Recovery:
  - Search for "A2008" in editor (Ctrl+H find in files)
  - Check line number in error message
  - Verify syntax against Intel ISA manual
  - Review MASM x64 syntax guide
```

#### Error A2070: Invalid Operands
```
Possible causes:
  - Register size mismatch (r32 vs r64)
  - Immediate too large for instruction
  - Wrong addressing mode for instruction
  - Invalid register combination
  
Recovery:
  - Check operand types (byte, word, dword, qword)
  - Verify register IDs (0-15 valid)
  - Confirm addressing mode (register-direct only for MOV r,r)
  - Review instruction encoding rules
```

#### Error A1010: Unmatched Block
```
Possible causes:
  - Mismatched PROC/ENDP pair
  - Extra or missing ENDP statement
  - FRAME decorator mismatch
  - Missing `.CODE` or `.DATA?` section marker
  
Recovery:
  - Count PROC declarations (line ~200)
  - Count ENDP statements (should match)
  - Check section markers present
  - Verify FRAME syntax on public procedures
```

#### Error A1010: Unmatched Nesting
```
Possible causes:
  - IF/ENDIF imbalance
  - Macro definition improperly closed
  
Recovery:
  - Search for unmatched IF/ELSE/ENDIF
  - Verify all macro definitions start with %macro and end with %endmacro
  - (Currently no macros in encoder_host_final.asm, so unlikely)
```

---

## Post-Assembly Validation

### Step 1: Verify Object File
```powershell
# Check if .obj file was created
Test-Path D:\RawrXD-Compilers\encoder_host_final.obj

# Get file size (should be 5-20KB)
Get-Item D:\RawrXD-Compilers\encoder_host_final.obj | Select-Object -ExpandProperty Length
```

### Step 2: Disassemble Object File (Optional)
```bash
# If objdump is available
objdump -d encoder_host_final.obj | grep -A 20 "main>:"

# Expected output should show test sequence:
#   48 b8 f0 de bc 9a 78 56 34 12     movabs $0x123456789abcdef0,%rax
#   49 b8 42 00 00 00 00 00 00 00     movabs $0x42,%r15
#   48 8b c3                           mov    %rbx,%rax
#   48 01 c3                           add    %rbx,%rax
#   c3                                 retq
```

### Step 3: Link to Executable (Optional)
```powershell
# Link with Windows SDK libs (if available)
link /SUBSYSTEM:CONSOLE encoder_host_final.obj kernel32.lib

# Expected output:
#   encoder_host_final.exe (executable)
```

### Step 4: Run Executable (Optional)
```powershell
# Execute test harness
D:\RawrXD-Compilers\encoder_host_final.exe

# Expected output:
#   [RawrXD] x64 Encoder + Macro Substitution System
```

---

## Troubleshooting Checklist

### If Assembly Fails

**Q: "ml64.exe not found"**
- A1: Check path: `Get-Command ml64 -ErrorAction SilentlyContinue`
- A2: Try full path: `"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\ml64.exe"`
- A3: Check Visual Studio BuildTools installed (may need repair)

**Q: "A2008 Syntax Error on line X"**
- Check line X in editor (Ctrl+G to go to line)
- Verify instruction mnemonic (lowercase: mov, add, ret, nop)
- Check operand syntax (no spaces before commas)

**Q: "A2070 Invalid Operands on line X"**
- Verify register operands are 0-15 (valid x64 registers)
- Check operand sizes match (r64 + r64, qword + qword, etc.)
- Verify imm64 uses qword size

**Q: "A1010 Unmatched nesting"**
- Count PROC declarations vs ENDP statements
- Verify all are balanced
- Check for extra/missing ENDP at end of file

**Q: "Permission Denied"**
- Ensure file is not read-only: `Get-Item encoder_host_final.asm | Set-ItemProperty -Name IsReadOnly -Value $false`
- Ensure ml64.exe is executable
- Restart PowerShell with admin rights if needed

---

## Success Criteria

Assembly is successful if:
- ✅ No error messages printed
- ✅ Exit code is 0
- ✅ File `encoder_host_final.obj` is created
- ✅ File size > 0 (not empty)

Bytecode validation succeeds if:
- ✅ Disassembly shows 5 test instructions
- ✅ Bytecode matches BYTECODE_REFERENCE.md
- ✅ REX.W prefix present for all r64 operations
- ✅ Opcode 8B used for MOV r,r/m (not 89)

---

## Next Steps After Successful Assembly

1. **Verify bytecode** using BYTECODE_REFERENCE.md
2. **Document result** (success/failure/differences noted)
3. **Plan integration** into masm_nasm_universal.asm
4. **Extract working code** (encoder core + macro framework)
5. **Merge and re-assemble** larger file

---

## Reference Documents

| Document | Purpose |
|----------|---------|
| BYTECODE_REFERENCE.md | Expected bytecode per test |
| ENCODER_HOST_FINAL_REPORT.md | Architecture, components |
| FINAL_STATUS_REPORT.md | Timeline, success criteria |

---

## Validation Log (Fill During Assembly)

```
Assembly Attempt #1:
Date/Time: _______________
Command: ml64.exe /c encoder_host_final.asm
Exit Code: ___
Errors: ___
Result: [ ] Success [ ] Failed

If failed, error message:
_________________________________

Action taken:
_________________________________

Attempt #2:
[repeat above]
```

---

**Ready to assemble!**  
Next command: `ml64.exe /c encoder_host_final.asm`

