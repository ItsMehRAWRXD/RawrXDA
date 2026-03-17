# Phase 3: Advanced Macro Features - User Guide

## Quick Start

### Named Parameters - Write Self-Documenting Macros

```asm
; Define macro with named parameters
%macro load_context 3
    %arg1:context_ptr   ; Pointer to context structure
    %arg2:offset        ; Field offset
    %arg3:register      ; Destination register
    
    mov %{register}, [%{context_ptr} + %{offset}]
%endmacro

; Use by name - much more readable!
load_context rax, 16, rcx
; Expands to: mov rcx, [rax + 16]
```

**Benefits:**
- Self-documenting code (readers see parameter names)
- Less error-prone (fewer positional mix-ups)
- IDE-friendly (can show hints based on names)
- Backward compatible (%1, %2 still work)

---

### Conditional Expansion - Smart Code Generation

```asm
%macro prologue 1-2
    push rbp
    mov rbp, rsp
    %if %0 > 1          ; If stack frame size provided
        sub rsp, %2
    %endif
%endmacro

prologue                ; No frame → just push/mov
prologue 32             ; With frame → sub rsp, 32
prologue 64             ; Different size → sub rsp, 64
```

**Key Features:**
- `%if`, `%elif`, `%else`, `%endif` support
- Evaluate expressions: `%0 > 1`, `%1 != 3`, etc.
- Use anywhere in macro body
- Perfectly nested support

---

### Repetition Loops - Generate Sequences

```asm
%rep 10
    dd %@repnum         ; 1, 2, 3, ..., 10
%endrep

; Generates: dd 1, dd 2, dd 3, ..., dd 10
```

**Use Cases:**
```asm
; Generate register save sequence
%rep 10
    mov [rsp + %@repnum * 8], r8 + %@repnum - 1
%endrep

; Unroll loop manually
%rep 4
    mov rax, [rsi + %@repnum * 8 - 8]
    mov [rdi + %@repnum * 8 - 8], rax
%endrep
```

---

### String Functions - Text Manipulation

```asm
%macro declare_string 1
    db %{|%1|}, 0       ; |...| = length prefix
    db %1, 0
%endmacro

declare_string "Hello"
; Expands to: db 5, 0, "Hello", 0
```

**Available Functions:**
- `%{|%1|}` - String length (strlen)
- `%{substr(%1, 2, 3)}` - Extract substring
- `%{upper(%1)}` - Convert to uppercase
- `%{lower(%1)}` - Convert to lowercase
- `%{cat(%1, %2, %3)}` - Concatenate strings

---

## Detailed Feature Reference

### 1. Named Parameters

#### Syntax
```asm
%macro name min-max [defaults]
    %arg1:param1        ; Name for %1
    %arg2:param2        ; Name for %2
    ; ... up to %20
    
    ; Body uses:
    mov %{param1}, %{param2}
%endmacro
```

#### Examples

**Simple Named Parameters:**
```asm
%macro swap 2
    %arg1:first
    %arg2:second
    
    mov rax, %{first}
    mov %{first}, %{second}
    mov %{second}, rax
%endmacro

swap rbx, rcx
; vs: swap rbx, rcx (without names - less clear!)
```

**With Defaults:**
```asm
%macro memcpy 3-4 1024
    %arg1:dest
    %arg2:src
    %arg3:count
    %arg4:block_size
    
    mov rcx, %{count}
    cmp rcx, %{block_size}
    jl @small_copy
    
    ; Large copy...
    jmp @done
@small_copy:
    ; Small copy...
@done:
%endmacro

memcpy rdi, rsi, rcx              ; Uses default block_size=1024
memcpy rdi, rsi, rcx, 256         ; Custom block_size
```

**Variadic with Names:**
```asm
%macro multi_push 1+
    %arg1:first
    
    push %{first}
    %rep %0 - 1
        push %%(2 + %@repnum)     ; Remaining args
    %endrep
%endmacro

multi_push rax, rbx, rcx, rdx
; Pushes: rax, rbx, rcx, rdx (in order)
```

#### Guidelines
- Use descriptive names (max 32 chars)
- Names are case-insensitive
- Can mix positional (%1) and named (%{name})
- Names improve readability significantly

---

### 2. Conditional Directives

#### Syntax
```asm
%if expression
    ; Code executed if expression is true
%elif expression
    ; Code executed if previous false, this true
%else
    ; Code executed if all previous false
%endif
```

#### Expressions

**Supported Operators:**
```
Comparison:     == != < <= > >=
Logical:        & (AND) | (OR) ! (NOT)
Parentheses:    ( ) for grouping
Special refs:   %0 (arg count), %1-%9 (args)
```

**Examples:**
```asm
%if %0 == 2
    ; Exactly 2 arguments
%endif

%if (%0 >= 1) & (%1 != 3)
    ; At least 1 arg AND first arg not 3
%endif

%if !(%0)
    ; No arguments provided
%endif
```

#### Real-World Example

```asm
%macro syscall_wrapper 1-3 64
    %arg1:syscall_num
    %arg2:arg_count
    %arg3:stack_size
    
    ; Check syscall number valid
    %if %{syscall_num} > 500
        %error "Syscall number too large"
    %endif
    
    ; Set up arguments based on count
    %if %{arg_count} >= 1
        mov rdi, [rsp + 16]     ; Arg 1
    %endif
    
    %if %{arg_count} >= 2
        mov rsi, [rsp + 24]     ; Arg 2
    %endif
    
    ; Save stack frame
    %if %{stack_size} > 0
        sub rsp, %{stack_size}
    %endif
    
    ; Execute syscall
    syscall
    
    ; Restore stack
    %if %{stack_size} > 0
        add rsp, %{stack_size}
    %endif
%endmacro

syscall_wrapper 1               ; NOP with defaults
syscall_wrapper 60, 2           ; exit() syscall
syscall_wrapper 1, 1, 128       ; write() with stack
```

#### Common Patterns

**Option 1 - Different Code Paths:**
```asm
%if TARGET_X32
    mov eax, %1
%else
    mov rax, %1
%endif
```

**Option 2 - Argument Validation:**
```asm
%if %0 < 2
    %error "Macro requires at least 2 arguments"
%endif
```

**Option 3 - Optional Features:**
```asm
%if USE_AVX
    vmovdqa xmm0, [rsi]
%else
    movdqa xmm0, [rsi]
%endif
```

---

### 3. Repetition Loops

#### Syntax
```asm
%rep count
    ; Body executed 'count' times
    ; Use %@repnum for iteration number (1-based)
%endrep
```

#### Counter Variable
```asm
%rep 5
    db %@repnum     ; 1, 2, 3, 4, 5
%endrep
```

#### Examples

**Generate Index Table:**
```asm
%rep 16
    dq table_entry_%@repnum
%endrep
; Creates: dq table_entry_1, dq table_entry_2, ... dq table_entry_16
```

**Unroll Loop:**
```asm
; Copy 4 qwords at a time
%rep 4
    mov rax, [rsi + %@repnum * 8 - 8]
    mov [rdi + %@repnum * 8 - 8], rax
%endrep
; Expands to 4 separate mov pairs
```

**Nested Loops:**
```asm
%rep 3              ; 3 rows
    %rep 4          ; 4 columns
        db %@repnum + (%@repnum - 1) * 4
    %endrep
%endrep
```

**With Conditionals:**
```asm
%rep 10
    %if %@repnum < 5
        db %@repnum
    %else
        db %@repnum + 10
    %endif
%endrep
; Outputs: 1 2 3 4 5 15 16 17 18 19
```

#### Performance Notes
- Each iteration expands full body
- Use for fixed-size sequences (not dynamic)
- Unrolling 4-8 iterations typically improves performance

---

### 4. String Functions

#### Syntax
```asm
%{strlen(string)}           ; Length of string
%{substr(string, start)}    ; From start to end
%{substr(string, start, len)} ; Substring of length
%{upper(string)}            ; Uppercase
%{lower(string)}            ; Lowercase
%{cat(str1, str2, ...)}     ; Concatenate
```

#### Examples

**String Length:**
```asm
%macro declare_cstring 1
    db %{strlen(%1)}, 0     ; Length prefix
    db %1, 0                ; String
%endmacro

declare_cstring "Hello"
; Expands: db 5, 0, "Hello", 0
```

**Substring:**
```asm
%macro version_string 0
    db %{substr("v1.2.3", 2)}, 0   ; From pos 2: "1.2.3"
    db %{substr("v1.2.3", 2, 3)}, 0 ; Length 3: "1.2"
%endmacro
```

**Case Conversion:**
```asm
section '.data'
uppercase_name db %{upper("my_function")}, 0  ; MY_FUNCTION
lowercase_name db %{lower("MY_FUNCTION")}, 0  ; my_function
%endmacro
```

**Concatenation:**
```asm
%macro full_name 3
    db %{cat(%1, "_", %2, "_", %3)}, 0
%endmacro

full_name "ns", "class", "method"
; Expands: db "ns_class_method", 0
```

---

## Best Practices

### Named Parameters
- ✅ Use descriptive names (avoid single letters)
- ✅ Document parameter purposes in comments
- ✅ Keep parameter count reasonable (< 8)
- ❌ Don't shadow built-in names (%0, %*, etc.)

### Conditionals
- ✅ Use for platform-specific code
- ✅ Validate arguments early with %if
- ✅ Properly nest %if/%elif/%else/%endif
- ❌ Don't nest too deeply (> 5 levels hard to read)

### Loops
- ✅ Use for small fixed-size generations
- ✅ Reference %@repnum for varying behavior
- ✅ Combine with %if for selective emission
- ❌ Don't use for arbitrary dynamic counts (too slow)

### String Functions
- ✅ Use for self-documenting string data
- ✅ Combine with string constants
- ✅ Use for code generation (data tables)
- ❌ Don't expect complex string algorithms

---

## Common Patterns & Recipes

### Pattern 1: Architecture-Specific Macros

```asm
%if BITS == 64
    %macro mov_imm 2
        movabs %1, %2
    %endmacro
%elif BITS == 32
    %macro mov_imm 2
        mov %1, %2
    %endmacro
%endif
```

### Pattern 2: Argument Count Validation

```asm
%macro require_args 2-
    %if %0 < %1
        %error "Too few arguments (need %1, got %0)"
    %endif
    %if %0 > %2
        %error "Too many arguments (max %2, got %0)"
    %endif
%endmacro
```

### Pattern 3: Generate Symbol Table

```asm
%rep 256
    symbol_%@repnum equ 0x%{hex(%@repnum)}
%endrep
```

### Pattern 4: Conditional Feature Check

```asm
%macro require_feature 1
    %if !HAVE_%{upper(%1)}
        %error "Feature %1 not available"
    %endif
%endmacro
```

---

## Troubleshooting

**Q: Named parameter not found?**
A: Check spelling and case. Names are case-insensitive but must match definition.

**Q: Condition not evaluated correctly?**
A: Remember %0 is arg count, %1-%9 are argument values. Parenthesize for clarity.

**Q: Loop counter wrong?**
A: %@repnum is 1-based. For 0-based: `%@repnum - 1`

**Q: String function not working?**
A: Must be inside macro body. Cannot use at assembly-level directives.

---

## Performance Tips

1. **Named Parameters:** No performance cost (same as positional)
2. **Conditionals:** Compile-time only (no runtime cost)
3. **Loops:** Expands at compile-time (exponential code growth possible)
4. **String Functions:** Evaluated at macro expansion time

---

## Migration from Phase 2

**Phase 2 Macros Still Work:**
```asm
; Old style still works
%macro old_style 2
    mov %1, %2
%endmacro

; New style adds clarity
%macro new_style 2
    %arg1:dest
    %arg2:src
    mov %{dest}, %{src}
%endmacro

; Both produce identical output!
```

---

**Phase 3 User Guide Version:** 1.0  
**Date:** January 2026  
**Status:** FEATURE REFERENCE

For technical implementation details, see PHASE3_IMPLEMENTATION_GUIDE.md
