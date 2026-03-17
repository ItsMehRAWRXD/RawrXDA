# Macro Substitution Engine - Quick Reference Card

## Macro Syntax

### Definition
```asm
%macro name argc [min] [max] [default1] [default2] ...
  ; macro body with %1, %2, %*, %0, etc.
%endmacro
```

### Invocation
```asm
name arg1, arg2, ...
; OR with parens (optional)
name(arg1, arg2, ...)
```

## Parameter Substitution

| Syntax | Meaning | Example |
|--------|---------|---------|
| `%1` | 1st argument | `mov rax, %1` |
| `%2-%9` | 2nd-9th arguments | `add %1, %2` |
| `%0` | Argument count (decimal) | `db %0` |
| `%=` | Alt for arg count | Same as `%0` |
| `%*` | All args comma-separated | `func %*` |
| `%%` | Literal percent | `db "%%d"` |

## Examples

### Simple Macro
```asm
%macro mov_imm 2
  mov %1, %2
%endmacro

mov_imm rax, 0x1234        ; → mov rax, 0x1234
mov_imm rcx, rbx           ; → mov rcx, rbx
```

### Variadic Arguments
```asm
%macro print 1+
  db "Args: ", %*
%endmacro

print "a", "b", "c"        ; → db "Args: ", "a", "b", "c"
```

### Default Parameters
```asm
%macro exit_code 0,1 0
  mov rax, 60
  mov rdi, %1
  syscall
%endmacro

exit_code                  ; → mov rdi, 0 (default)
exit_code 1                ; → mov rdi, 1 (override)
```

### Nested Macros
```asm
%macro outer 1
  inner %1
%endmacro

%macro inner 1
  mov rax, %1
%endmacro

outer 42                   ; → → mov rax, 42
```

### Argument Count
```asm
%macro show_count 0,5
  db %0
%endmacro

show_count                 ; db 0
show_count 1,2,3           ; db 3
```

## Signature Formats

### Exact Number of Arguments
```asm
%macro test 2              ; Exactly 2 arguments required
%endmacro
```

### Optional Arguments
```asm
%macro test 0,2            ; 0-2 arguments
%endmacro
```

### Minimum Arguments
```asm
%macro test 1,5            ; 1-5 arguments
%endmacro
```

### Variadic (1 or more)
```asm
%macro test 1+             ; 1 or more arguments
%endmacro
```

### Defaults
```asm
%macro test 0,2 0 0        ; Default values for params 1 and 2
%endmacro
```

## Recursion Rules

- **Maximum depth:** 32 levels of nested macro calls
- **Detection:** Automatic via `depth_guard` counter
- **Error:** "Macro recursion depth exceeded (>32 levels)"
- **Self-call:** Allowed if depth < 32

## Error Messages

| Error | Cause | Fix |
|-------|-------|-----|
| `Undefined macro: X` | Macro not defined | Define it with `%macro` |
| `Too few arguments` | Not enough args provided | Provide minimum required |
| `Too many arguments` | Too many args provided | Provide <= maximum |
| `Macro recursion depth exceeded` | Nesting > 32 levels | Reduce nesting depth |
| `Invalid parameter: %X` | Bad substitution pattern | Use %1-%9, %0, %*, %% |

## Limitations

1. **No recursive self-calls** beyond 32 levels
2. **No string manipulation** (%defstr, %deftok)
3. **No local labels** (all labels global)
4. **Argument size** limited to ~4KB each
5. **Body size** limited to ~64KB per macro

## Integration Checklist

- [ ] MacroEntry structure in place (64 bytes)
- [ ] ParseMacroArguments function implemented
- [ ] ExpandMacroWithArgs core logic in place
- [ ] Macro invocation detection in preprocess_macros
- [ ] Token injection working
- [ ] Recursion guard functional (32-level limit)
- [ ] All 8 test cases passing
- [ ] Error reporting clear and helpful

## Code Locations

| Component | Location |
|-----------|----------|
| Structures | `masm_nasm_universal.asm:300` |
| Preprocess loop | `masm_nasm_universal.asm:2100` |
| Stub expand_macro | `masm_nasm_universal.asm:2600` |
| Test suite | `macro_tests.asm` |
| Integration guide | `INTEGRATION_STEPS.md` |
| Engine code | `macro_substitution_engine.asm` |

## Testing

**Minimal test:**
```asm
%macro test 1
  nop
  mov rax, %1
%endmacro

test 42            ; Should expand to: nop / mov rax, 42
```

**Full test suite:** `macro_tests.asm` (16 test cases)

**Stress tests:**
- Recursion at 32 levels → expect error
- Variadic with 20+ args → all args present
- Mixed features (%1, %*, %0, %%) → all work correctly

## Performance Targets

- **Expansion time:** < 1ms per invocation
- **Compile time for 100 macros:** < 100ms
- **Memory per invocation:** ~256 bytes (stack frame)
- **Buffer size:** 64KB per macro expansion

## Debugging Tips

1. **Enable debug output:**
   ```asm
   DEBUG_DUMP_EXPANSION equ 1
   ```

2. **Check ArgVec array:**
   - Verify {ptr, len} pairs are correct
   - Look for off-by-one errors

3. **Trace token walking:**
   - Add print statements in expansion loop
   - Check %N detection logic

4. **Validate recursion guard:**
   - depth_guard incremented on entry
   - depth_guard checked against 32
   - depth_guard decremented on exit

5. **Buffer overflow detection:**
   - Canary values around expand_buf
   - Validate output_ptr stays in bounds
   - Monitor for stack corruption

## Common Mistakes

1. **Forgetting `%` in substitution**
   - ❌ `mov rax, 1` (just literal)
   - ✓ `mov rax, %1` (parameter)

2. **Mixing up %0 and %%0**
   - ❌ `%0` as literal zero
   - ✓ `%%0` for literal "%0"

3. **Wrong signature format**
   - ❌ `%macro test 2 3` (not valid)
   - ✓ `%macro test 2` or `%macro test 0,3`

4. **Forgetting commas in variadic**
   - ❌ `db %*` when args are "a" "b" → `db "a""b"`
   - ✓ `db %*` already adds commas

5. **Exceeding recursion limit**
   - Caught automatically at 32 levels
   - Error message is clear
   - Rewrite macro to avoid deep nesting

## Quick Fixes

**Macro not expanding?**
- Check if defined before use
- Verify argument count matches
- Look for recursion depth limit error

**Wrong output?**
- Verify %N substitution is working
- Check token boundaries
- Look for off-by-one errors

**Performance slow?**
- Reduce macro complexity
- Cache commonly used expansions
- Profile token walking loop

---

**Version:** 1.0 (Phase 2)
**Last Updated:** Current session
**Confidence:** 95%
