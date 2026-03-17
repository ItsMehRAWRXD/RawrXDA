# Phase 2 Integration: Step-by-Step Implementation

## Quick Start

This guide walks through integrating `macro_substitution_engine.asm` into `masm_nasm_universal.asm`.

## Prerequisites

- **Files needed:**
  - `masm_nasm_universal.asm` (main compiler, 3072 lines)
  - `macro_substitution_engine.asm` (new engine, ~500 lines)
  - `macro_tests.asm` (test suite, 16 test cases)

- **Working knowledge:**
  - MASM64 x86-64 syntax
  - Token-based preprocessing
  - Stack frame management
  - Error code conventions

## Step 1: Locate Integration Points

**File:** `masm_nasm_universal.asm`

**Key locations:**
- Line ~300: `MacroEntry` structure definition
- Line ~315: `ARGV` / `ArgVec` structure definition
- Line ~2100: `preprocess_macros` main loop
- Line ~2600: Stub `expand_macro` function
- Line ~2900: Macro table management functions

**Action:** Open file and search for these landmarks.

## Step 2: Update Structures

**Current (Line ~300):**
```asm
MacroEntry struct
    name_ptr qword
    body_ptr qword
    body_len dword
    param_count dword
    ; ... other fields
MacroEntry ends
```

**Updated (add these fields):**
```asm
MacroEntry struct
    name_ptr qword          ; +0
    body_ptr qword          ; +8
    body_len dword          ; +16
    param_count dword       ; +20
    min_required dword      ; +24
    max_params dword        ; +28
    defaults_ptr qword      ; +32  (NEW)
    depth_guard dword       ; +40  (NEW)
    ; padding to 64 bytes
    _reserved word 10
MacroEntry ends

; ArgVec structure (16 bytes)
ArgVec struct
    ptr qword               ; +0
    len dword               ; +8
    is_default dword        ; +12
ArgVec ends

; ExpandCtx on stack (~256 bytes)
; (allocate in ExpandMacroWithArgs via sub rsp, 256)
```

**Action:** Use `replace_string_in_file` to update MacroEntry structure.

## Step 3: Copy Expansion Engine

**Source:** `macro_substitution_engine.asm`

**Target:** Insert after line 2600 in `masm_nasm_universal.asm`

**Functions to copy:**
1. `ParseMacroArguments` (~100 lines)
   - Input: rsi=token_ptr, rdi=ArgVec array, rcx=max_slots
   - Output: rax=argc or error
   
2. `ExpandMacroWithArgs` (~300 lines)
   - Main expansion loop
   - %N substitution
   - %* variadic handling
   - %=/%0 arg count
   - %% escaped percent
   
3. `CopyArgTokens` (~50 lines)
   - Copies argument into output buffer
   
4. `EmitDecimal` (~30 lines)
   - Converts argc to ASCII digits
   
5. Error reporting helpers

**Action:** Copy all functions from macro_substitution_engine.asm into masm_nasm_universal.asm after line 2600.

## Step 4: Wire Macro Invocation Detection

**Location:** `preprocess_macros` loop, around line 2100

**Before:**
```asm
preprocess_macros:
    ; ... setup code ...
    loop_main:
        ; ... token reading ...
        if current_token == "%macro"
            ; Handle definition
        else if is_operator(current_token, ";")
            ; Skip comment
        else
            ; ???  MISSING: macro invocation detection
```

**After:**
```asm
preprocess_macros:
    ; ... setup code ...
    mov r10d, 0             ; depth_level = 0
    
    loop_main:
        ; ... token reading ...
        if current_token == "%macro"
            ; Handle definition (unchanged)
        
        else if is_macro_name(current_token)
            ; *** NEW: Macro invocation ***
            
            ; Step 1: Save position
            mov rbx, rsi    ; saved_pos = current position
            
            ; Step 2: Parse arguments
            lea rdi, [rsp - 256]  ; ArgVec array on stack
            mov rcx, MACRO_MAX_ARGS
            call ParseMacroArguments
            test rax, rax
            js error_macro_args   ; if (rax < 0) error
            mov r8d, eax    ; argc = rax
            
            ; Step 3: Lookup macro
            call MacroTableLookup    ; returns ptr in rax
            test rax, rax
            jz error_undefined_macro
            mov r9, rax     ; macro_entry = rax
            
            ; Step 4: Validate arg count
            mov eax, [r9 + 24]      ; min_required
            cmp r8d, eax
            jl error_too_few_args
            mov eax, [r9 + 28]      ; max_params (-1 = unlimited)
            cmp eax, -1
            je skip_max_check
            cmp r8d, eax
            jg error_too_many_args
        skip_max_check:
            
            ; Step 5: Call expansion engine
            lea rdi, [rsp - 256]    ; arg_vec
            mov rcx, r8d            ; argc
            lea rdx, [rsp - 64*1024]; expand_buf (64KB on stack)
            mov r8, 64*1024         ; expand_buf_size
            mov r9d, r10d           ; depth_level
            call ExpandMacroWithArgs
            test rax, rax
            js error_expansion      ; if (rax < 0) error
            
            ; Step 6: Token injection
            mov rcx, rax            ; expanded_len = rax
            lea rsi, [rsp - 64*1024]; expand_buf
            call InjectTokens       ; inserts into main stream
            
            ; Continue processing
            jmp loop_main
        
        else if is_operator(current_token, ";")
            ; Skip comment
        
        else
            ; Regular instruction
```

**Action:** Edit preprocess_macros to add this macro invocation block.

## Step 5: Implement Token Injection

**Location:** Add new function after ExpandMacroWithArgs

```asm
InjectTokens:
    ; Input: rsi -> expanded text
    ;        rcx = expanded_len
    ;        rdi -> insertion point in main stream
    ;
    ; This is tricky: we need to insert expanded text without
    ; overwriting the following tokens
    
    ; For now, simplified version (append to output):
    mov r10, 0              ; i = 0
    
inject_loop:
    cmp r10, rcx
    jge inject_done
    
    mov al, [rsi + r10]
    mov [rdi + r10], al
    inc r10
    jmp inject_loop
    
inject_done:
    mov rsi, rdi
    add rsi, rcx            ; Update rsi for next iteration
    ret
```

**Note:** Full token injection is complex (requires memmove of following tokens). For initial testing, you can:
1. Use append-to-output strategy (simpler)
2. Or use temporary buffer and reassemble (safer)

**Action:** Create InjectTokens function as shown.

## Step 6: Add Error Handling

**Location:** Error handlers at end of preprocess_macros

```asm
error_undefined_macro:
    mov rsi, szErrMacroUndef
    mov rdx, current_token
    call error_report
    jmp fatal_error

error_too_few_args:
    mov rsi, szErrMacroArgs
    mov rdx, [r9]            ; macro name
    mov r8d, [r9 + 24]       ; min_required
    mov r9d, r8d             ; argc
    call error_report_3args
    jmp fatal_error

error_too_many_args:
    mov rsi, szErrMacroArgs
    mov rdx, [r9]            ; macro name
    mov r8d, [r9 + 28]       ; max_params
    mov r9d, r10d            ; argc
    call error_report_3args
    jmp fatal_error

error_expansion:
    ; rax contains negative error code
    mov rcx, rax            ; error_code
    ; Log error with context
    call error_report_expansion
    jmp fatal_error
```

**Error messages to add (around line 2500):**
```asm
szErrMacroUndef: db "Error: Undefined macro: ", 0
szErrMacroArgs:  db "Error: Macro %s expects %d args, got %d", 0dh, 0ah, 0
szErrMacroRec:   db "Error: Macro recursion depth exceeded (>32 levels)", 0dh, 0ah, 0
szErrMacroSubst: db "Error: Invalid parameter substitution: %%%c", 0dh, 0ah, 0
```

**Action:** Add error handling section.

## Step 7: Test Compilation

**Command:**
```powershell
# Compile main assembler
ml64.exe /c /Fo masm_nasm_universal.obj masm_nasm_universal.asm
if ($LASTEXITCODE -eq 0) {
    echo "Compilation successful"
} else {
    echo "Compilation failed"
}
```

**Expected:** Zero errors, zero warnings.

**If errors:** Check for:
- Undefined labels (typo in function names)
- Structure size mismatches (verify all offsets)
- Syntax errors (missing colons, commas)

**Action:** Fix compilation errors.

## Step 8: Link and Create Executable

**Command:**
```powershell
link /SUBSYSTEM:CONSOLE /OUT:masm_nasm_universal.exe masm_nasm_universal.obj
```

**Action:** Create executable.

## Step 9: Run Basic Test

**Command:**
```powershell
# Create simple test input
$test_asm = @"
%macro add 2
  mov rax, %1
  add rax, %2
%endmacro

test:
  add rcx, rdx
  ret
"@

$test_asm | Out-File -Encoding ASCII test_basic.asm

# Compile with our assembler
.\masm_nasm_universal.exe test_basic.asm -o test_basic.obj
```

**Expected output:**
- `test_basic.obj` is created
- No error messages
- Macro expansion is correct

**If fails:**
- Check error messages
- Verify ParseMacroArguments is finding arguments
- Check token stream walking

**Action:** Debug and iterate.

## Step 10: Run Full Test Suite

**Command:**
```powershell
# Use macro_tests.asm (16 test cases)
.\masm_nasm_universal.exe macro_tests.asm -o macro_tests.obj

if ($LASTEXITCODE -ne 0) {
    echo "Tests failed"
    exit 1
}

# Link to executable
link /SUBSYSTEM:CONSOLE /OUT:macro_tests.exe macro_tests.obj

# Run
.\macro_tests.exe
```

**Expected:** All 16 tests pass without errors.

**Test cases:**
1. ✓ Basic substitution (%1, %2)
2. ✓ Argument count (%0, %=)
3. ✓ Variadic (%*)
4. ✓ Escaped percent (%%)
5. ✓ Default parameters
6. ✓ Nested macros
7. ✓ Multiple parameters (%1-%9)
8. ✓ Argument validation
9. ✓ Complex expressions (%1 in address)
10. ✓ No parameters
11. ✓ Macro redefinition
12. ✓ Deep nesting (3 levels)
13. ✓ Preserve grouping (parens)
14. ✓ Single variadic
15. ✓ Conditional expansion
16. ✓ Recursion under limit

**Action:** Verify all tests pass.

## Step 11: Stress Testing

**Command:**
```powershell
# Test 1: Recursion limit (should fail at depth 32)
$stress_asm = @"
%macro r1 0
  r2
%endmacro
%macro r2 0
  r3
%endmacro
... (generate 32 nested macros) ...
%macro r32 0
  nop
%endmacro

test:
  r1
  ret
"@

.\masm_nasm_universal.exe stress_recursion.asm 2>&1 | Select-String "recursion"
# Should find: "Macro recursion depth exceeded (>32 levels)"

# Test 2: Variadic with 20 arguments
$variadic_asm = @"
%macro concat 1+
  db %*
%endmacro

test:
  concat 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20
  ret
"@

.\masm_nasm_universal.exe stress_variadic.asm
# Should expand all 20 args joined with commas

# Test 3: Deep nesting (5 levels of macro calling macro)
# Should work fine (well under 32 limit)

# Test 4: Mixed features (%1, %*, %0, %%, all in one macro)
```

**Expected:** Recursion caught, variadic expanded correctly, nesting works, mixed features functional.

**Action:** Verify stress tests.

## Step 12: Performance Validation

**Command:**
```powershell
# Measure expansion time on complex macro
# Create test with 100 macro invocations
$perf_asm = @"
%macro test 2
  mov %1, %2
%endmacro

test:
  test rax, rbx
  test rcx, rdx
  test rsi, rdi
  ... (repeat 100 times)
  ret
"@

# Time compilation
Measure-Command { .\masm_nasm_universal.exe perf_test.asm -o perf_test.obj }
# Should be < 100ms for 100 macro invocations
```

**Expected:** < 1ms per macro expansion.

**Action:** Verify performance acceptable.

## Troubleshooting

### Issue: "Undefined macro" error on valid macro

**Cause:** Macro not found in table

**Fix:**
1. Verify `MacroTableLookup` is finding the macro
2. Check that macro name is stored correctly (no case sensitivity issue)
3. Add debug output to see what's in the table:
   ```asm
   mov rsi, szDbgMacroName
   mov rdx, macro_name
   call printf
   ```

### Issue: "Too few arguments" error

**Cause:** `ParseMacroArguments` is not extracting all arguments

**Fix:**
1. Add debug dump of ArgVec array
2. Check comma/paren handling
3. Verify argument boundaries are correct

### Issue: Expansion produces wrong output

**Cause:** Token substitution loop bug

**Fix:**
1. Enable detailed logging in `ExpandMacroWithArgs`
2. Print each token as it's copied
3. Check %N detection logic
4. Verify argument tokens are copied exactly

### Issue: Buffer overflow on large macro

**Cause:** `expand_buf` (64KB) too small

**Fix:**
1. Increase buffer size: `mov r8, 256*1024` (256KB)
2. Or implement streaming output
3. Validate output_ptr never exceeds buffer_end

## Success Criteria

✓ All 16 macro_tests.asm cases pass
✓ Recursion limit (32) enforced
✓ Error messages clear and helpful
✓ Expansion time < 1ms per invocation
✓ No memory leaks (validate with debugging tools)
✓ Nested macros work (3+ levels)
✓ Variadic arguments expanded correctly
✓ Default parameters work
✓ %0, %=, %%, %* all functional

## Next Steps

After successful integration:

1. **Phase 2.5: Conditional Assembly**
   - Implement `%if`, `%else`, `%endif`
   - Support expressions like `%if %0 > 2`

2. **Phase 3: VTable Stress Testing**
   - Multiple inheritance diamond patterns
   - Generic template instantiation

3. **Optimization Phase**
   - Cache frequently used macros
   - Pre-expand common patterns
   - Reduce per-invocation overhead

---

**Status:** Ready for integration
**Difficulty:** Moderate (lots of moving parts, but well-scoped)
**Time estimate:** 4-6 hours for full integration + testing
