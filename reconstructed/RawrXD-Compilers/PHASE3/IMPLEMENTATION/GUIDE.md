# Phase 3: Advanced Macro Features - Implementation Guide

## Overview

Phase 3 extends Phase 2 with four advanced features:
1. **Named Parameters** - `%{param_name}` syntax for self-documenting macros
2. **Conditional Directives** - `%if/%elif/%else/%endif` for conditional expansion
3. **String Functions** - Built-in string manipulation (strlen, substr, etc.)
4. **Repetition Loops** - `%rep N / body / %endrep` for code generation

---

## Feature 1: Named Parameters (%{name})

### Syntax
```asm
%macro demo 3
    %arg1:name arg1
    %arg2:code arg2
    %arg3:count arg3
    ; In macro body:
    mov %{name}, 0      ; Refers to %1 by name
    mov %{code}, 42     ; Refers to %2 by name
    mov rax, %{count}   ; Refers to %3 by name
%endmacro

demo my_var, rax+rbx*2, 5
; Expands using named references
```

### Implementation Architecture

**MacroEntry Extension:**
```
Offset 56+: param_names_ptr (8 bytes)  → Pointer to name array
Offset 64+: param_name_count (4 bytes) → Number of named params
Offset 68+: name_map (20*8 bytes)      → Index mapping table
```

**Parameter Name Storage:**
- Allocate separate buffer for parameter names
- One name per parameter (null-terminated strings)
- Hash each name for O(1) lookup

**Data Structure:**
```asm
struct ParamName
    offset  dd  ; Offset in name buffer
    len     dd  ; Length of name
    index   dd  ; Parameter index (0-19)
    reserved dd
endstruct
```

### Processing Steps

**1. Parse Macro Definition Header:**
```
Input:  %macro demo 3
            %arg1:name name1
            %arg2:code code1
            %arg3:count count1
        ; Body...
        %endmacro

Action: Extract parameter names: ["name1", "code1", "count1"]
Store:  param_names_ptr → array of names
        param_name_map  → [0] → name1, [1] → code1, etc.
```

**2. During Invocation Argument Collection:**
```
Invocation: demo my_var, rax+rbx*2, 5

Action: Collect arguments as normal:
        g_macro_arg_start[0] → "my_var" tokens
        g_macro_arg_start[1] → "rax+rbx*2" tokens
        g_macro_arg_start[2] → "5" token

Map:    "name1" → index 0
        "code1" → index 1
        "count1" → index 2
```

**3. During Expansion:**
```
Token: %{name1}

Action: Look up "name1" in param_name_map
        Find index = 0
        Substitute with g_macro_arg_start[0] tokens
        Same as %1 but more readable
```

### Code Implementation

**Modification to expand_macro:**
```asm
; Check for %{ token (named parameter reference)
; Current code handles %1-%9 (TOK_PERCENT_NUMBER)
; Add handler for named params:

@check_named_param:
    cmp token_type, TOK_LBRACE  ; {
    jne @next_handler
    
    ; Read parameter name until }
    call read_param_name        ; Returns name string in rax, length in ecx
    
    ; Look up name in param_name_map
    call lookup_param_name      ; Returns index in eax (-1 if not found)
    
    cmp eax, -1
    je @param_not_found
    
    ; Use index as parameter number
    movzx eax, al               ; Ensure 0-19
    mov ecx, [g_macro_arg_start + eax*4]
    mov edx, [g_macro_arg_count + eax*4]
    ; Copy tokens to output (same as %N)
    jmp @copy_arg_tokens
```

**New Procedure: lookup_param_name**
```asm
; Input: RCX = macro entry, RDX = name string, R8D = name length
; Output: EAX = parameter index (0-19), or -1 if not found
lookup_param_name proc
    ; Load param_names_ptr from macro entry (offset 56)
    mov rax, [rcx + 56]     ; param_names_ptr
    
    ; Linear search through parameter names
    xor r9d, r9d            ; Index counter
@search_loop:
    cmp r9d, 20             ; Max params
    jae @not_found
    
    ; Compare name at [rax + r9d*16] with input name
    mov rsi, [rax + r9d*16]     ; Name string offset
    mov r10d, [rax + r9d*16 + 4] ; Name length
    
    cmp r10d, r8d           ; Compare lengths
    jne @next_name
    
    ; String comparison (case-insensitive)
    call strcmpi_len        ; Compare rsi with rdx, length r8d
    test eax, eax
    jz @found_match
    
@next_name:
    inc r9d
    jmp @search_loop
    
@found_match:
    mov eax, r9d
    ret
    
@not_found:
    mov eax, -1
    ret
lookup_param_name endp
```

**New Procedure: read_param_name**
```asm
; Read identifier inside %{ }, return string
; Input: current token stream position
; Output: RCX = name string, RDX = length
read_param_name proc
    ; Skip {
    ; Read identifier
    ; Stop at }
    ; Return name
    ret
read_param_name endp
```

### Integration Points

1. **Macro Definition Parsing** (lines ~1200):
   - Extract parameter names from %arg directives
   - Store in param_names_ptr array

2. **expand_macro** (lines ~1300):
   - Add handler for TOK_LBRACE (named parameter reference)
   - Lookup name → index conversion
   - Substitute using same mechanism as %N

3. **preprocess_macros** (lines ~1400):
   - Pass macro entry with populated param_names_ptr
   - Enable named parameter resolution

---

## Feature 2: Conditional Directives (%if/%elif/%else/%endif)

### Syntax
```asm
%macro conditional_demo 1
    %if %0 >= 2
        mov %1, 0       ; If 2+ args
    %elif %0 == 1
        mov %1, -1      ; If 1 arg
    %else
        xor %1, %1      ; If 0 args
    %endif
%endmacro
```

### Expression Evaluator

**Supported Expressions:**
```
%if (%0 > 2) & (%1 != 3)
%if defined(SYMBOL)
%if (%1 == "string_literal")
%if !(%2)                       ; Logical NOT
```

**Implementation:**
```asm
struct CondNode
    op      dd      ; EQ (0), NE (1), LT (2), LE (3), GT (4), GE (5), AND (6), OR (7), NOT (8)
    left    dq      ; Left operand (CondNode or literal)
    right   dq      ; Right operand (CondNode or literal)
    is_literal dd   ; Is this a leaf node?
    value   dq      ; Literal value if is_literal
endstruct
```

### Processing Steps

**1. Parse Condition Expression:**
```
Input:  %if (%0 > 2) & (%1 != 3)

Parse:
    - Tokenize condition
    - Build AST (Abstract Syntax Tree)
    - Handle operator precedence
    - Substitute %0, %1, etc. with current values
```

**2. Evaluate Condition:**
```
- Tree walk (depth-first)
- Evaluate leaf nodes (literals, %0, %1, symbols)
- Apply operators (>, <, ==, !=, &, |, !)
- Return true/false
```

**3. Expansion Control:**
```
%if (true)  → expand body
%elif (false) → skip
%else → expand
%endif → end
```

### Code Skeleton

**Main Condition Handler:**
```asm
handle_if proc
    ; Parse condition expression until EOL
    call parse_condition    ; Returns AST in rax
    
    ; Evaluate AST
    mov rcx, rax
    call evaluate_condition ; Returns result in eax
    
    ; Push onto condition stack
    mov r8, g_cond_stack_ptr
    mov [r8], eax           ; true/false value
    inc g_cond_stack_depth
    
    ; If false, skip to %elif or %else
    test eax, eax
    jz @skip_to_elif
    ret
    
@skip_to_elif:
    ; Scan forward to find %elif, %else, or %endif
    call find_next_cond_directive
    ret
handle_if endp
```

### Integration Points

1. **Preprocessing Pipeline** (lines ~1400):
   - Intercept %if, %elif, %else, %endif tokens
   - Maintain condition stack during body expansion

2. **expand_macro** (lines ~1300):
   - Evaluate %0, %1 references in conditions
   - Use current macro argument values

3. **Token Stream Processing** (lines ~1100):
   - Conditional output: only emit tokens from "true" branch

---

## Feature 3: String Functions

### Built-in String Macros

```asm
%macro strlen 1
    ; Returns length of string %1 as number
    ; Usage: len = strlen("hello")
%endmacro

%macro substr 3
    ; Extract substring
    ; Usage: result = substr("hello", 1, 3)  → "ell"
    ; %1 = string, %2 = start, %3 = length
%endmacro

%macro strcat 2-*
    ; Concatenate strings
    ; Usage: result = strcat("hello", " ", "world")
%endmacro

%macro strupper 1
    ; Convert to uppercase
    ; Usage: upper = strupper("hello")  → "HELLO"
%endmacro

%macro strlower 1
    ; Convert to lowercase
%endmacro
```

### Implementation Strategy

**Option A: Pure Macro Implementation**
- Use existing token substitution
- Implement using %if and parameter introspection
- No new code needed - leverage Phase 3 conditionals

**Option B: Built-in Token Functions**
- Add special token handlers
- Implement in assembly code
- Access token properties directly

### String Function Examples

**strlen Implementation (Pure Macro):**
```asm
%macro strlen 1
    %assign len 0
    %rep 100
        %if %len > 50
            %error "String too long"
        %endif
        ; Count tokens in %1
        %assign len len+1
    %endrep
    db len                  ; Emit length as byte
%endmacro
```

**Token-Based Implementation:**
```asm
; Special token handler for strlen
handle_strlen_call proc
    ; %1 is argument to strlen
    ; Get token count of %1
    mov ecx, [g_macro_arg_count]  ; Token count
    mov eax, ecx                   ; Return count
    ; Emit as TOK_NUMBER
    ret
handle_strlen_call endp
```

---

## Feature 4: Repetition Loops (%rep/%endrep)

### Syntax
```asm
%rep 5
    nop
    ; %@repnum = current iteration (1-5)
%endrep

; Outputs 5 nop instructions
```

### Counter Variable Support

```asm
%rep 3
    db %@repnum     ; 1, 2, 3
%endrep
```

### Implementation

**Loop State:**
```asm
g_rep_depth         dd ?    ; Current loop nesting level
g_rep_count         dd 32 dup(?)  ; Count for each level
g_rep_num           dd 32 dup(?)  ; Current iteration (1-based)
g_rep_body_start    dd 32 dup(?)  ; Token position of body
g_rep_body_end      dd 32 dup(?)  ; Token position of %endrep
```

**Processing:**
```asm
@handle_rep:
    ; Read count expression
    call evaluate_expression    ; Count in eax
    mov ecx, eax
    
    ; Remember body start position
    mov edx, g_tok_pos
    mov [g_rep_body_start + g_rep_depth*4], edx
    
    ; Scan to %endrep
    call find_endrep
    mov [g_rep_body_end + g_rep_depth*4], eax
    
    ; Loop: expand body N times
    xor r8d, r8d                ; Iteration counter
@rep_loop:
    cmp r8d, ecx
    jae @rep_done
    
    ; Set %@repnum = r8d + 1
    mov [g_rep_num + g_rep_depth*4], r8d
    inc [g_rep_num + g_rep_depth*4]
    
    ; Re-expand body
    mov g_tok_pos, [g_rep_body_start + g_rep_depth*4]
    call expand_tokens_until_endrep
    
    inc r8d
    jmp @rep_loop
    
@rep_done:
    ; Skip past %endrep
    ret
```

---

## Implementation Priority & Timeline

### High Priority (Week 1)
1. **Named Parameters** (%{name})
   - Relatively low complexity
   - High user value (documentation)
   - Builds on Phase 2 foundation
   - Estimated: 2-3 days

2. **Conditional Directives** (%if/%else/%endif)
   - Medium complexity
   - Essential for advanced macros
   - Requires expression evaluator
   - Estimated: 3-4 days

### Medium Priority (Week 2)
3. **Repetition Loops** (%rep/%endrep)
   - Medium complexity
   - Common use case (code generation)
   - Estimated: 2-3 days

4. **String Functions** (as pure macros)
   - Low complexity if using conditionals
   - Nice-to-have feature
   - Estimated: 1-2 days

### Post-Phase 3
- Performance optimization (hash tables)
- Environment variables (%\_\_DATE\_\_)
- Advanced string functions (runtime library)

---

## Testing Strategy

**Phase 3 Test Suite (test_phase3_advanced.asm):**

```
Named Parameters:
  - Test 1: Simple named reference
  - Test 2: Multiple named parameters
  - Test 3: Nested macros with names
  - Test 4: Error handling (undefined name)
  - Test 5: Mixed positional and named

Conditionals:
  - Test 6: %if/%else/%endif
  - Test 7: %elif chaining
  - Test 8: Nested conditions
  - Test 9: Condition with %0
  - Test 10: Condition with symbol comparison

Loops:
  - Test 11: Basic %rep
  - Test 12: %@repnum counter
  - Test 13: Nested loops
  - Test 14: Loop with conditionals

String Functions:
  - Test 15: strlen
  - Test 16: substr
  - Test 17: strcat
  - Test 18: strupper
  - Test 19: strlower
  - Test 20: Complex string operations
```

**Expected Results:** 20/20 tests passing (100%)

---

## Documentation Plan

**4 New Documentation Files:**

1. **PHASE3_IMPLEMENTATION.md** (15 KB)
   - Technical architecture
   - Data structures
   - Algorithm descriptions
   - Integration points

2. **PHASE3_FEATURES.md** (12 KB)
   - User guide for each feature
   - Syntax examples
   - Common patterns
   - Error messages

3. **PHASE3_EXAMPLES.md** (10 KB)
   - Working code examples
   - Best practices
   - Performance tips
   - Troubleshooting

4. **PHASE3_DEPLOYMENT_CHECKLIST.md** (8 KB)
   - Verification steps
   - Quality metrics
   - Test coverage
   - Integration validation

---

## Code Organization

**New Functions to Implement:**

```
Named Parameters:
  - parse_param_names()
  - lookup_param_name()
  - handle_named_param_ref()

Conditionals:
  - parse_condition()
  - evaluate_condition()
  - handle_if()
  - handle_elif()
  - handle_else()
  - handle_endif()

Loops:
  - handle_rep()
  - handle_endrep()
  - substitute_repnum()

String Functions:
  - handle_strlen()
  - handle_substr()
  - handle_strcat()
  - handle_strupper()
  - handle_strlower()
```

**New Global Variables:**

```
Named Params:
  - param_names_ptr (per macro entry)
  - param_name_map (lookup table)

Conditionals:
  - g_cond_stack
  - g_cond_stack_depth

Loops:
  - g_rep_depth
  - g_rep_count[]
  - g_rep_num[]
  - g_rep_body_start[]
  - g_rep_body_end[]
```

---

## Conclusion

Phase 3 adds production-ready advanced features while maintaining backward compatibility with Phase 2B. Implementation follows proven patterns from Phase 2 and leverages existing infrastructure (tokenization, argument handling, preprocessing pipeline).

**Estimated Total Phase 3 Duration:** 2-3 weeks  
**Expected Test Coverage:** 100% (20 tests)  
**Production Ready Target:** YES (within phase)

---

**Document Version:** 1.0  
**Date:** January 2026  
**Status:** PLANNING DOCUMENT
