# Macro Argument Substitution - Quick Reference Card

## Basic Syntax

```asm
; Definition
%macro NAME param_count
    ; Use %1, %2, ..., %param_count
%endmacro

; Invocation
NAME arg1, arg2, arg3
```

## Parameter Types

| Syntax | Description | Example |
|--------|-------------|---------|
| `%1`-`%20` | Positional parameters | `mov %1, %2` |
| `%0` | Argument count | `cmp %0, 3` |
| `%*` | All variadic args | `push %*` |

## Macro Definition Variants

### Fixed Parameters
```asm
%macro FIXED 3
    ; Requires exactly 3 arguments
    mov %1, %2
    add %1, %3
%endmacro

FIXED rax, rbx, rcx
```

### Variadic (Unbounded)
```asm
%macro VARIADIC 1+
    ; 1 required, unlimited additional
    mov rax, %1
    ; %* = all args after %1
%endmacro

VARIADIC first, second, third, fourth
```

### Default Parameters
```asm
%macro DEFAULTS 2-4 0, 8
    ; 2 required, 2 optional with defaults
    mov %1, %2      ; Required
    add %1, %3      ; Default: 0
    shr %1, %4      ; Default: 8
%endmacro

DEFAULTS rax, 100           ; Uses defaults: 0, 8
DEFAULTS rbx, 200, 5        ; Uses default: 8
DEFAULTS rcx, 300, 10, 4    ; All explicit
```

### Bounded Variadic
```asm
%macro BOUNDED 2-5
    ; 2-5 arguments accepted
    mov %1, %2      ; Always present
%if %0 > 2
    add %1, %3      ; Optional
%endif
%endmacro

BOUNDED rax, 10             ; Min 2 args
BOUNDED rbx, 20, 30         ; 3 args
BOUNDED rcx, 5, 10, 15, 20  ; Max 5 args
```

## Special Argument Handling

### Brace Delimiters (Comma Preservation)
```asm
%macro ARRAY 2
    %1: dd %2
%endmacro

; Comma inside braces = single argument
ARRAY my_data, {1, 2, 3, 4}
; Expands to: my_data: dd 1, 2, 3, 4
```

### Empty Arguments
```asm
%macro OPTIONAL 3
    mov %1, [%2%3]
%endmacro

OPTIONAL rax, rbx       ; %3 empty: mov rax, [rbx]
OPTIONAL rcx, rdx, +8   ; %3 is +8: mov rcx, [rdx+8]
```

## Argument Features

### Using %0 (Count)
```asm
%macro PUSH_ALL 0+
%rep %0
    push %1
    %rotate 1
%endrep
%endmacro

PUSH_ALL rax, rbx, rcx, rdx
; Expands to 4 push instructions
```

### Using %* (Variadic Blob)
```asm
%macro CALL_WITH 1+
    ; Forward all args to function
    call %1
    ; %* would contain: %2, %3, %4, ...
%endmacro
```

## Nested Macros
```asm
%macro INNER 2
    mov %1, %2
%endmacro

%macro OUTER 3
    INNER %1, %2        ; Nested call
    add %1, %3
%endmacro

OUTER rax, rbx, rcx
; Expands to:
;   mov rax, rbx
;   add rax, rcx
```

## Common Patterns

### Function Prologue/Epilogue
```asm
%macro PROLOGUE 1-2 rsp
    push rbp
    mov rbp, %1
    sub %1, %2
%endmacro

%macro EPILOGUE 1
    mov rsp, %1
    pop rbp
    ret
%endmacro

my_func:
    PROLOGUE rsp, 32
    ; ... function body ...
    EPILOGUE rsp
```

### Register Save/Restore
```asm
%macro SAVE_REGS 0+
%rep %0
    push %1
    %rotate 1
%endrep
%endmacro

%macro RESTORE_REGS 0+
%rep %0
    %rotate -1
    pop %1
%endrep
%endmacro

SAVE_REGS rax, rbx, rcx
; ... code ...
RESTORE_REGS rax, rbx, rcx
```

### Conditional Operations
```asm
%macro COND_MOV 2-3 0
%if %3
    mov %1, %2
%else
    xor %1, %1
%endif
%endmacro

COND_MOV rax, rbx       ; %3=0: xor rax, rax
COND_MOV rcx, rdx, 1    ; %3=1: mov rcx, rdx
```

## Error Prevention

### Check Argument Count
```asm
%macro CHECKED 2-4
%if %0 < 2
    %error "Need at least 2 arguments"
%endif
    ; Macro body
%endmacro
```

### Type Validation (Naming Convention)
```asm
; Use prefixes to document expected types
%macro MOVE_REG 2
    ; %1 = dest register
    ; %2 = source register/immediate
    mov %1, %2
%endmacro
```

## Debugging Macros

### Print Expansion (Conceptual)
```asm
%macro DEBUG_EXPAND 1+
    ; During development, use %warning to see expansion
    %warning "Expanding: %1"
%endmacro
```

### Trace Arguments
```asm
%macro TRACE_ARGS 0+
    ; %0 tells you how many args were passed
    ; Useful for variadic debugging
    %if %0 == 0
        %warning "No arguments provided"
    %endif
%endmacro
```

## Performance Tips

### Token-Stream Advantage
- ✅ Arguments stored as **token arrays** (not text)
- ✅ **No re-lexing** on expansion (O(1) copy per token)
- ✅ Preserves line/column info for error reporting
- ✅ Safe nested expansion (isolated buffers)

### Avoid Anti-Patterns
- ❌ **Don't:** Create macros that expand to incomplete syntax
- ❌ **Don't:** Use macro names that conflict with instructions
- ❌ **Don't:** Exceed 32 recursion depth (hard limit)
- ✅ **Do:** Use clear, descriptive macro names
- ✅ **Do:** Document expected argument types
- ✅ **Do:** Provide defaults for optional parameters

## Limits

| Limit | Value | Notes |
|-------|-------|-------|
| Max parameters | 20 | NASM standard |
| Max recursion depth | 32 | Enforced by guard |
| Max macro definitions | 512 | Configurable |
| Token size | 32 bytes | Fixed structure |
| Substitution buffer | 64K tokens | 2MB total |

## Implementation Details

### Token Types (Internal)
```asm
TOK_PERCENT_NUMBER  equ 30  ; %1-%20
TOK_PARAM_REF       equ 31  ; Generic param
TOK_VARIADIC_MARKER equ 32  ; Marks variadic start
TOK_STRINGIFY       equ 81h ; %"N (future)
TOK_CONCAT          equ 82h ; %+ (future)
TOK_VARIADIC_ALL    equ 83h ; %*
```

### Global State (Per Expansion)
```asm
g_macro_arg_start[20]   ; Token start indices
g_macro_arg_count[20]   ; Token counts per arg
g_variadic_start        ; First variadic index
g_macro_depth           ; Current recursion level
```

## Future Features (Roadmap)

### Stringification (%"N)
```asm
%macro DEBUG 1
    db %"1, ": ", 0     ; Convert %1 to string
    dq %1
%endmacro

DEBUG my_variable
; Will expand to: db "my_variable: ", 0
```

### Concatenation (%+)
```asm
%macro GEN_HANDLER 1
    %1%+_handler:       ; Paste tokens
        ret
%endmacro

GEN_HANDLER test
; Will expand to: test_handler:
```

### Named Parameters (%{name})
```asm
%macro FUNC x:req, y:req, z:0
    mov rax, %{x}
    add rax, %{y}
    sub rax, %{z}
%endmacro

FUNC y=10, z=5, x=20    ; Order-independent
```

## Quick Reference: Syntax Summary

```asm
; ========== MACRO DEFINITION ==========
%macro NAME count               ; Fixed count
%macro NAME min+                ; Variadic (min required)
%macro NAME min-max             ; Bounded range
%macro NAME min-max d1, d2      ; With defaults
%endmacro

; ========== SUBSTITUTION ==========
%1, %2, ..., %20                ; Positional (1-based)
%0                              ; Argument count
%*                              ; All variadic args (future)

; ========== DELIMITERS ==========
{arg, with, commas}             ; Brace-delimited single arg
(expr)                          ; Parens preserved in args
[memory]                        ; Brackets preserved

; ========== INVOCATION ==========
NAME arg1, arg2, ...            ; Normal call
NAME {a,b,c}, d                 ; With brace arg
NAME a, , c                     ; Empty middle arg
```

## Example: Complete Macro System Usage

```asm
; Define macros with various features
%macro SETUP 2-3 default_size
    mov %1, %2
    add %1, %3
%endmacro

%macro LOOP_COPY 3+
    mov rcx, %3
%%loop:
    mov al, [%1]
    mov [%2], al
    inc %1
    inc %2
    loop %%loop
%endmacro

%macro ARRAY_INIT 2
    %1: dd %2
%endmacro

; Use macros
SETUP rax, 100              ; Uses default for %3
SETUP rbx, 200, 50          ; All explicit

LOOP_COPY src, dst, count   ; Variadic usage

ARRAY_INIT data, {1,2,3,4}  ; Brace delimiter
```

## Error Messages Reference

| Error | Cause | Solution |
|-------|-------|----------|
| "Macro argument count mismatch" | Wrong number of args | Check definition min-max |
| "Too many macro arguments" | > 20 params | Reduce parameter count |
| "Macro recursion depth exceeded" | > 32 levels | Check for infinite recursion |
| "Unmatched delimiter" | Unclosed brace/paren | Balance delimiters |
| "Parameter %N out of range" | %N where N > count | Check argument count |

## Testing Checklist

Before deploying macros:
- ✅ Test with minimum argument count
- ✅ Test with maximum argument count
- ✅ Test with defaults (if applicable)
- ✅ Test with brace-delimited args (if used)
- ✅ Test nested invocations (if applicable)
- ✅ Verify error handling for invalid args

---

**See Also:**
- `MACRO_ARGUMENT_SUBSTITUTION_GUIDE.md` - Full implementation details
- `test_macro_arg_substitution.asm` - Comprehensive test suite
- `masm_nasm_universal.asm` - Source implementation

**Version:** 1.0 | **Date:** 2026-01-27
