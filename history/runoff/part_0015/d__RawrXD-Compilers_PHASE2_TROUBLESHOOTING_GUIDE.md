# Phase 2 Troubleshooting & Developer Guide

## Problem Diagnosis Quick Reference

### Macro Not Expanding

**Symptom:** Macro invocation appears in output unchanged

**Possible Causes:**
1. Macro not defined yet
2. Name mismatch (case-sensitive)
3. Wrong scope (if scoped macros implemented)
4. Preprocessing not run

**Diagnosis Steps:**
```
1. Check macro definition exists in source
2. Verify name spelling exactly matches invocation
3. Check macro defined BEFORE invocation
4. Verify preprocess_macros called in pipeline
```

**Solution:**
```asm
; WRONG - invocation before definition
invoke_it arg1, arg2

%macro invoke_it 2
    mov %1, %2
%endmacro

; CORRECT - definition before invocation
%macro invoke_it 2
    mov %1, %2
%endmacro

invoke_it arg1, arg2
```

**Code Reference:** `find_macro` at line 1070 - checks g_macro_table

---

### Parameter Substitution Failing

**Symptom:** %1, %2 appear in output instead of arguments

**Possible Causes:**
1. Parameters exceeds actual argument count
2. Argument not collected properly
3. Substitution handler not triggered

**Diagnosis Steps:**
```
1. Count actual arguments passed: arg1, arg2, ...
2. Check parameter references don't exceed count
3. Verify argument types valid (not keywords)
```

**Solution:**
```asm
; WRONG - %3 used but only 2 args
%macro swap 2
    mov rax, %1
    mov %1, %2
    mov %2, rax
    mov %3, 0  ; ERROR: no third arg
%endmacro

; CORRECT - only use %1-%2
%macro swap 2
    mov rax, %1
    mov %1, %2
    mov %2, rax
%endmacro
```

**Code Reference:** `expand_macro` lines 1280-1295 - validates param range

---

### Argument Count Wrong (%0)

**Symptom:** %0 not matching actual argument count

**Possible Causes:**
1. Variadic range not properly declared
2. Default parameters confusing count
3. Comma handling inside braces

**Diagnosis Steps:**
```
1. Verify macro declaration: %macro name min-max
2. Count actual invocation arguments
3. Check for braces protecting commas: {1,2,3}
```

**Solution:**
```asm
; WRONG - variadic not declared
%macro count_args 2
    mov rax, %0    ; ERROR: %0 = 2 always
%endmacro

; CORRECT - declare variadic
%macro count_args 1+
    mov rax, %0    ; %0 = actual arg count
%endmacro

; With braces:
%macro array 2
    %1 dd {%2}     ; %2 can contain multiple values
%endmacro

array data, {1,2,3,4,5}  ; %0 = 2, NOT 6
```

**Code Reference:** `expand_macro` lines 1240-1250 - %0 handling

---

### Variadic Expansion Issues

**Symptom:** %* not expanding or missing arguments

**Possible Causes:**
1. Macro not declared with + (variadic)
2. No remaining arguments after first parameter
3. Empty arguments causing issues

**Diagnosis Steps:**
```
1. Verify macro has 1+ in definition
2. Check at least 2 arguments passed
3. Verify no syntax errors in macro body
```

**Solution:**
```asm
; WRONG - not variadic
%macro multi 2
    db %*      ; ERROR: %* undefined
%endmacro

; CORRECT - variadic declared
%macro multi 1+
    mov rax, %1       ; First arg
    ; ... use %* for rest
    db %*             ; All args: arg2,arg3,arg4,...
%endmacro

; Calling with 3+ arguments:
multi first, second, third, fourth
; %1 = first
; %* = second, third, fourth
```

**Code Reference:** `expand_variadic_all` lines 2023+ - handles %*

---

### Stringification Not Working (%"N)

**Symptom:** %"1 not converting to string

**Possible Causes:**
1. Incorrect syntax (should be %"N not %\'N)
2. Parameter out of range
3. Buffer overflow

**Diagnosis Steps:**
```
1. Check syntax: %"1 (with quotes)
2. Verify parameter number valid (1-9)
3. Check argument isn't too complex (4096 byte limit)
```

**Solution:**
```asm
; WRONG - incorrect syntax
%macro log 1
    db %'1, 0      ; ERROR: wrong quote style
%endmacro

; CORRECT - proper syntax
%macro log 1
    db %"1, 0      ; Correct: %"1 with double quotes
%endmacro

log counter        ; Creates: db "counter", 0
```

**Code Reference:** `stringify_arg` lines 1850-2000

---

### Brace Delimiter Problems

**Symptom:** Commas inside braces treated as argument separators

**Possible Causes:**
1. Not using braces: {1,2,3}
2. Mismatched braces
3. Braces inside strings causing issues

**Diagnosis Steps:**
```
1. Verify argument wrapped in braces: {value}
2. Check braces properly paired
3. Look for unclosed braces in previous args
```

**Solution:**
```asm
; WRONG - commas split arguments
%macro array 2
    %1 dd %2
%endmacro

array data, 1,2,3,4,5  ; ERROR: 6 arguments, expected 2

; CORRECT - braces protect commas
%macro array 2
    %1 dd %2
%endmacro

array data, {1,2,3,4,5}  ; CORRECT: 2 arguments
                          ; %2 = {1,2,3,4,5} (7 tokens)
```

**Code Reference:** `parse_macro_args_enhanced` lines 1550-1580

---

### Default Parameters Not Filling

**Symptom:** Missing arguments not replaced with defaults

**Possible Causes:**
1. Defaults not declared in macro definition
2. Too many arguments provided (overriding defaults)
3. Default syntax incorrect

**Diagnosis Steps:**
```
1. Check macro definition has defaults: %macro name 1-3 def1,def2
2. Count arguments: should be less than max
3. Verify default values are valid expressions
```

**Solution:**
```asm
; WRONG - no defaults declared
%macro prologue 1-2
    push %1
    push %2        ; ERROR: no default for %2
%endmacro

prologue rax       ; ERROR: %2 undefined

; CORRECT - defaults declared
%macro prologue 1-2 rbx
    push %1
    push %2        ; %2 defaults to rbx if missing
%endmacro

prologue rax       ; Works: push rax; push rbx
prologue rax, rcx  ; Works: push rax; push rcx
```

**Code Reference:** `verify_and_fill_args` lines 1700-1750

---

### Nested Macro Recursion Error

**Symptom:** "Macro recursion depth exceeded" error

**Possible Causes:**
1. Mutual recursion: A calls B, B calls A
2. Direct recursion: A calls A
3. Depth >32 levels

**Diagnosis Steps:**
```
1. Trace macro call chain
2. Look for circular references
3. Count nesting depth
```

**Solution:**
```asm
; WRONG - circular recursion
%macro a 0
    b
%endmacro

%macro b 0
    a              ; a calls b, b calls a → INFINITE
%endmacro

; CORRECT - break cycle
%macro a_impl 0
    mov rax, 0
%endmacro

%macro a 0
    a_impl         ; Call implementation, not another macro
%endmacro
```

**Code Reference:** `expand_macro` lines 1121-1125 - recursion guard at 32 levels

---

### Token Concatenation Not Working (%{})

**Symptom:** %{token1}%{token2} not merging

**Possible Causes:**
1. Spacing breaking concatenation
2. Special characters in tokens
3. Token boundaries not detected

**Diagnosis Steps:**
```
1. Check spacing: no spaces between tokens
2. Verify tokens are valid identifiers
3. Check no special characters that prevent merge
```

**Solution:**
```asm
; This feature requires conditional assembly
; For now, use simpler concatenation:

%macro make_label 1
    %1_handler:        ; Text concatenation (simpler)
%endmacro

make_label my        ; Creates: my_handler:
```

**Code Reference:** `concat_tokens` lines 2100-2200

---

## Debug Techniques

### Enabling Expansion Tracing

**Option 1: Manual Expansion Check**
```asm
; Define test macro
%macro test_expand 2
    mov %1, %2
    add %1, 0x10
%endmacro

; Test with known values
test_expand rax, rbx

; Expected output:
; mov rax, rbx
; add rax, 0x10

; If output shows %1, %2 → substitution failed
```

**Option 2: Intermediate File Inspection**
```
1. Compile with debugging enabled
2. Dump preprocessed token stream
3. Check macro definitions collected
4. Trace expansion step-by-step
```

### Using %0 for Debugging
```asm
%macro debug_args 0-5
    ; Shows what arguments were passed
    mov rax, %0        ; Count
    ; Now use breakpoint to inspect rax
%endmacro

debug_args a, b, c     ; %0 = 3
```

### Stringification for Output
```asm
%macro trace 1
    db "Arg was: ", %"1, 0
%endmacro

trace counter_value    ; Output: Arg was: counter_value
```

---

## Common Mistakes

### Mistake 1: Forgetting Macro Declaration Parameters
```asm
; WRONG
%macro swap
    mov %1, rax
    mov rax, %2
    mov %2, %1
%endmacro

; CORRECT
%macro swap 2          ; ← Must specify 2 parameters
    mov %1, rax
    mov rax, %2
    mov %2, %1
%endmacro
```

### Mistake 2: Using %0 in Non-Variadic Macro
```asm
; WRONG
%macro fixed 2
    mov rax, %0        ; ERROR: fixed macro, %0 always = 2
%endmacro

; CORRECT
%macro variable 1+
    mov rax, %0        ; Works: %0 = actual arg count
%endmacro
```

### Mistake 3: Commas in Arguments Without Braces
```asm
; WRONG
%macro array 2
    %1 dd %2
%endmacro

array data, 1,2,3      ; ERROR: 4 arguments instead of 2

; CORRECT
array data, {1,2,3}    ; 2 arguments: data and {1,2,3}
```

### Mistake 4: Nested Braces Confusing Parser
```asm
; TRICKY - works but complex
%macro complex 1
    db {%1}
%endmacro

complex {1,2,3}        ; Parses as one argument: {1,2,3}
                       ; Expands to: db {1,2,3}
```

### Mistake 5: Escaping Percent Signs
```asm
; WRONG
%macro perc 1
    mov %1, 100 %      ; ERROR: bare % invalid

; CORRECT
%macro perc 1
    mov %1, 100%%      ; %% = literal %
%endmacro

perc rax               ; Expands to: mov rax, 100%
```

---

## Performance Troubleshooting

### Macro Expansion Too Slow

**Diagnosis:**
1. Count number of macros: if >512, expand capacity
2. Check argument parsing: complex expressions slow
3. Check nested calls: depth multiplies expansion

**Optimization:**
1. Minimize argument count (use simpler parameters)
2. Reduce nesting depth (avoid recursive calls)
3. Simplify macro bodies (fewer tokens)

### Buffer Overflow

**Symptom:** Expansion stops or garbles mid-output

**Causes:**
1. Substitution buffer full (64KB)
2. Stringify buffer full (4KB)
3. Argument count exceeds 20

**Solution:**
1. Check macro expansion size
2. Split large macros
3. Reduce variadic arguments

---

## Testing Your Macros

### Test Template
```asm
; Test: [Feature Name]
%macro test_feature [params]
    [macro body]
%endmacro

; Expected: [what should happen]
; Actual: [what does happen]
; Status: [PASS/FAIL]
```

### Validation Checklist
- [ ] Macro defined before use
- [ ] Parameter count matches declaration
- [ ] Arguments valid expressions
- [ ] Braces used for complex arguments
- [ ] Defaults provided if optional
- [ ] No recursion >32 levels
- [ ] Output matches expectations

---

## Error Messages Reference

| Error Message | Likely Cause | Solution |
|---------------|--------------|----------|
| "Macro not found" | Name mismatch or undefined | Check spelling, ensure macro defined first |
| "Too many arguments" | More args than allowed | Check macro definition max |
| "Too few arguments" | Missing required args | Provide all required args or use defaults |
| "Parameter %N out of range" | %N where N > arg count | Check actual arg count |
| "Recursion depth exceeded" | >32 nested calls | Check for infinite recursion |
| "Unmatched delimiter" | Mismatched braces/parens | Balance delimiters properly |
| "Buffer overflow" | Output too large | Split macro or reduce complexity |

---

## Resources

### Implementation Source
- **Location:** `d:\RawrXD-Compilers\masm_nasm_universal.asm`
- **Lines:** 4686 total, ~1500 for Phase 2
- **Key Procedures:**
  - expand_macro: Line 1073
  - parse_macro_args_enhanced: Line 1496
  - stringify_arg: Line 1850
  - concat_tokens: Line 2100
  - preprocess_macros: Line 2096

### Documentation
- **PHASE2_ARCHITECTURE_SUMMARY.md** - Technical details
- **MACRO_QUICK_REFERENCE.md** - Syntax reference
- **test_phase2_validation.asm** - Working examples
- **PHASE2_DOCUMENTATION_INDEX.md** - Navigation guide

### Test Suite
- **test_phase2_validation.asm** - 15 comprehensive tests
- All tests with expected expansions documented

---

**Troubleshooting Guide Version:** 1.0  
**Last Updated:** January 2026  
**Status:** FINAL ✅

For additional help, see PHASE2_DOCUMENTATION_INDEX.md for full resource list.
