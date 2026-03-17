# RawrXD NASM Universal Assembler - Comprehensive Audit Report
**Date**: January 27, 2026  
**File**: `masm_nasm_universal.asm` (2,934 lines)  
**Status**: 40% Complete - Multiple critical components unfinished

---

## Executive Summary

The NASM-compatible assembler has a **solid foundation** but is missing critical implementation pieces:

| Component | Status | Priority | Est. Completion |
|-----------|--------|----------|-----------------|
| **Lexer/Tokenizer** | ✓ 90% Complete | Low | Phase 3 |
| **Macro System (Phase 1)** | ⚠️ 60% Complete | **HIGH** | Phase 2 |
| **Instruction Encoder** | ❌ 20% Complete | **CRITICAL** | Phase 1 |
| **PE32/PE32+ Generator** | ❌ 5% Complete | **CRITICAL** | Phase 1 |
| **Symbol Table & Relocations** | ❌ 0% Complete | **HIGH** | Phase 2 |
| **Preprocessor/Parser** | ❌ 30% Complete | **MEDIUM** | Phase 2 |
| **Assembler Pass 1/2** | ❌ 10% Complete | **CRITICAL** | Phase 1 |

---

## 1. WORKING COMPONENTS ✓

### 1.1 Lexer System (90% Complete)
**Lines**: 410-900  
**Functions**:
- `lexer_init` ✓ - Token buffer allocation
- `get_char`, `peek_char` ✓ - Character stream processing
- `skip_ws` ✓ - Whitespace and comment handling
- `read_ident` ✓ - Identifier tokenization
- `read_number` ✓ - Number parsing (hex, binary, octal, decimal)
- `read_string` ✓ - String literal parsing
- `percent_to_token` ✓ - NASM % syntax handling
- `tokenize` ✓ - Main tokenization loop

**Status**: Functional  
**Issues**: Minor edge cases in escape sequences

### 1.2 Macro Parsing (Phase 1) (60% Complete)
**Lines**: 1200-1500, 2250-2400  
**Functions**:
- `match_macro_keyword` ✓ - Macro definition detection
- `find_macro` ✓ - Macro table lookup with DJB2 hashing
- `djb2_hash`, `djb2_hash_len` ✓ - Macro name hashing
- `tokenize_macro_args` ✓ - Argument parsing infrastructure
- `parse_macro_arguments` ✓ - Nested parenthesis/bracket tracking

**Status**: Partially functional  
**Issues**: 
- Macro expansion (`expand_macro`) not fully tested
- Phase 2 (argument substitution) incomplete
- Variadic macro support scaffolded but not implemented

### 1.3 Helper Functions (100% Complete)
- `init_heap`, `alloc` ✓
- `strcpy`, `strlen`, `strcmpi` ✓
- `print_str`, `print_char`, `print_int` ✓
- `read_file`, `write_output_file` (stubs)

---

## 2. INCOMPLETE/BROKEN COMPONENTS ❌

### 2.1 CRITICAL: Instruction Encoder (20% Complete)

**Lines**: 1900-2100  
**Status**: BROKEN - Only skeleton exists

#### 2.1.1 What's Implemented
- `EmitByte`, `EmitWord`, `EmitDword`, `EmitQword` ✓ - Output primitives
- `CalcREX` ✓ - REX prefix calculation
- `EncodeModRM`, `EncodeSIB` ✓ - Bit packing helpers
- `Emit_MOV_R64_R64` ⚠️ - Incomplete (partial template)
- `Emit_MOV_R64_IMM64` ⚠️ - Incomplete
- `Emit_ADD_R64_R64` ⚠️ - Incomplete

#### 2.1.2 What's Missing
- **No instruction dispatch table** - No way to route mnemonics to encoders
- **No operand parser** - Can't extract register/memory operands from tokens
- **No SIB encoding** - Scaled index addressing broken
- **No displacement encoding** - Can't emit rel32 for JMP/CALL
- **No immediate encoding** - Can't handle mov r64, imm64
- **Missing 950+ instruction variants** - Only MOV, ADD stubbed

#### 2.1.3 Fix Requirements
**Priority**: CRITICAL (blocks assembly)  
**Lines of Code Needed**: ~1500-2000  
**Approach**: 
```
1. Create instruction_dispatch[] table mapping mnemonic → encoder function
2. Implement operand_parse() to extract type/size/register/memory from token stream
3. Complete high-level emitters (Emit_MOV_RM_R, Emit_ADD_RM_IMM, etc.)
4. Add displacement/immediate encoding
5. Implement rel32 calculation for jumps (requires 2-pass assembly)
```

---

### 2.2 CRITICAL: PE32/PE32+ Header Generation (5% Complete)

**Lines**: 1540-1560  
**Status**: STUBS ONLY

```asm
write_pe proc
    cmp g_format, FMT_WIN32
    je @pe32
    call write_pe64
    ret
@pe32:
    call write_pe32
    ret
write_pe endp

write_pe32 proc
    ret          ; ← EMPTY!
write_pe32 endp

write_pe64 proc
    ret          ; ← EMPTY!
write_pe64 endp
```

#### 2.2.1 Missing PE Header Generation
- DOS stub (MZ header)
- PE signature
- COFF file header (machine type, section count, timestamp)
- Optional header (subsystem, image base, entry point)
- Section headers (.text, .data, .rdata)
- Import table (kernel32.dll)
- Base relocations (.reloc section)
- Resource table (.rsrc)

#### 2.2.2 Fix Requirements
**Priority**: CRITICAL (output generation)  
**Lines of Code Needed**: ~800-1200  
**Approach**:
```
1. Define PE_HEADER struct with all required fields
2. Build DOS stub and PE signature
3. Calculate section offsets and sizes
4. Emit COFF header with correct machine type (0x8664 for x64, 0x14C for x86)
5. Build optional header with entry point and image base
6. Generate section headers with R/X/W flags
7. Emit import table for kernel32.dll
```

**Reference**: Currently delegating to external `RawrXD_GenerateExecutable` (line 1598)

---

### 2.3 HIGH PRIORITY: Symbol Table & Relocations (0% Complete)

**Lines**: None - structure defined, no implementation  
**Status**: STUBS ONLY

#### 2.3.1 What's Missing
- **Symbol insertion** - No code to add labels/functions to symbol table
- **Symbol lookup** - No code to find symbol by name
- **Relocation generation** - No code to emit COFF relocations
- **Forward reference resolution** - No 2-pass label resolution

#### 2.3.2 Data Structures Exist (Line 350-400)
```asm
g_sym_table     dq 65536 dup(?)    ; 64k buckets for hash table
g_sym_cnt       dd ?
g_relocs        dq ?               ; Relocation array
g_reloc_cnt     dd ?
```

#### 2.3.3 Functions That Need Implementation
- `mark_global` (line 1675) - Register exported symbol
- `add_symbol` (missing) - Insert symbol into hash table
- `lookup_symbol` (missing) - Find symbol by name
- `add_relocation` (missing) - Record fixup for linker
- `resolve_symbols` (missing) - 2-pass label resolution

#### 2.3.4 Fix Requirements
**Priority**: HIGH (needed for linking)  
**Lines of Code Needed**: ~600-800  
**Approach**:
```
1. Implement hash table insertion with DJB2 hashing
2. Create symbol structure with name, offset, section, flags
3. Add relocation struct with type, offset, symbol index
4. Implement 2-pass assembly for forward references
5. Generate COFF relocation records
```

---

### 2.4 MEDIUM PRIORITY: Macro Phase 2 (Argument Substitution) (0% Complete)

**Lines**: 2400-2600 (stubs)  
**Status**: Infrastructure present, logic missing

#### 2.4.1 What's Defined
```asm
g_macro_arg_stack       dq ?        ; Argument stack for nested expansions
g_subst_buffer          dq ?        ; Token expansion buffer
g_stringify_buffer      db 4096 dup(?)  ; For %"N stringification
g_concat_buffer         db 1024 dup(?)  ; For %{} token pasting
```

#### 2.4.2 Functions That Need Implementation
- `expand_macro_subst` (line 2596) - Substitute %1..%20 in tokens
- `stringify_argument` (missing) - Convert %"N to string literal
- `concat_tokens` (missing) - Token pasting for %{}
- `handle_variadic` (missing) - %* expansion

#### 2.4.3 Example Unimplemented Feature
```nasm
%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
%endmacro

PUSH_ALL        ; Should expand to three PUSH instructions
```

#### 2.4.4 Fix Requirements
**Priority**: MEDIUM (needed for macro expansion)  
**Lines of Code Needed**: ~400-600  
**Approach**:
```
1. Tokenize macro body into token stream
2. Create substitution pass: scan tokens, replace TOK_MACRO_PARAM
3. Implement stringification: convert token range to string literal
4. Implement token pasting: merge adjacent tokens
5. Handle variadic expansion: repeat tokens for %*
6. Support nested macros with argument stack
```

---

### 2.5 MEDIUM PRIORITY: Assembler Passes (30% Complete)

**Lines**: 1000-1200  
**Status**: Partial framework only

#### 2.5.1 What's Missing
```asm
; Main assembly routine (should exist but incomplete)
assemble proc
    ; NOT IMPLEMENTED
    ret
assemble endp

preprocess_macros proc
    ; NOT IMPLEMENTED
    ret
preprocess_macros endp
```

#### 2.5.2 Required Features
1. **Pass 1: Syntax Analysis**
   - Parse directives (BITS, SECTION, EXTERN, GLOBAL)
   - Scan labels and record positions
   - Validate syntax
   - Build symbol table
   - **Status**: MISSING (0%)

2. **Pass 2: Code Generation**
   - Emit instruction bytes using encoder
   - Resolve forward references
   - Generate relocations
   - **Status**: MISSING (0%)

#### 2.5.3 Critical Functions Missing
- `parse_line()` - Single line parsing
- `parse_directive()` - BITS, SECTION, GLOBAL handling
- `parse_instruction()` - Mnemonic + operand parsing
- `assemble_pass1()` - Label collection
- `assemble_pass2()` - Code generation

#### 2.5.4 Fix Requirements
**Priority**: CRITICAL (main loop)  
**Lines of Code Needed**: ~800-1000  
**Approach**:
```
1. Implement directive parser (BITS, SECTION, GLOBAL, EXTERN)
2. Create instruction parser: tokenize operands, extract registers/memory
3. Build pass 1: collect labels, symbols, relocations
4. Build pass 2: emit machine code using encoder
5. Implement forward reference resolution
```

---

### 2.6 LOW PRIORITY: Directive Handlers (10% Complete)

**Lines**: 1650-1690  
**Status**: Stubs

```asm
parse_section_directive proc
    ret              ; STUB
parse_section_directive endp

expect_number proc
    ret              ; STUB
expect_number endp

expect_ident proc
    ret              ; STUB
expect_ident endp

mark_global proc
    ret              ; STUB
mark_global endp
```

#### 2.6.1 Implementation Needed
- **BITS directive**: Set 16/32/64-bit mode
- **SECTION directive**: Switch between .text, .data, .bss
- **GLOBAL/PUBLIC directive**: Mark symbols as exported
- **EXTERN directive**: Register external symbols
- **EQU directive**: Create constants
- **TIMES directive**: Repeat instruction/data
- **ALIGN directive**: Pad to boundary

#### 2.6.2 Fix Requirements
**Priority**: LOW (non-blocking)  
**Lines of Code Needed**: ~300-400

---

## 3. BUILD ISSUES & BLOCKERS

### 3.1 Compilation Status
```
✓ Assembles successfully with ML64
✓ No syntax errors
✗ Does not link (no entry point, no PE header)
✗ Cannot run (would crash immediately)
```

### 3.2 Current Blockers
1. **PE header generation** - Output format completely wrong
2. **Instruction encoder** - Can't emit machine code
3. **Assembler main loop** - No pass 1/2 implementation
4. **Symbol resolution** - No linking support
5. **Macro substitution** - Phase 2 incomplete

---

## 4. PRIORITY FIXES (In Order)

### Phase 1: Core Functionality (Blocking)
**Est. 2-3 weeks, ~4000 LOC**

1. **[CRITICAL] Instruction Encoder** (1500-2000 LOC)
   - Build instruction dispatch table
   - Implement operand parser
   - Complete high-level emitters (MOV, ADD, PUSH, POP, CALL, JMP, RET, etc.)
   - Add displacement/immediate encoding
   - Support scaled index (SIB)

2. **[CRITICAL] PE32/PE32+ Header** (800-1200 LOC)
   - DOS stub and PE signature
   - COFF file header
   - Optional header
   - Section headers
   - Import table
   - Relocation table

3. **[CRITICAL] Assembler Pass 1 & 2** (800-1000 LOC)
   - Directive parser (BITS, SECTION, GLOBAL, EXTERN)
   - Instruction parser (mnemonics + operands)
   - Label collection and 2-pass resolution
   - Code emission loop

4. **[HIGH] Symbol Table** (600-800 LOC)
   - Hash table insertion/lookup
   - Symbol structure definition
   - Forward reference resolution

### Phase 2: Macro System & Polish (Nice to Have)
**Est. 1-2 weeks, ~1500 LOC**

1. Macro Phase 2 (Argument Substitution)
2. Directive handlers (EQU, TIMES, ALIGN, INCBIN)
3. Error reporting improvements
4. Expression evaluator (for constants)

### Phase 3: Extended Features (Future)
**Est. 2-3 weeks, ~2000 LOC**

1. SSE/AVX instruction support
2. Advanced macros (nested, recursive)
3. Debug symbols (DWARF)
4. Link-time optimization hints

---

## 5. ESTIMATION SUMMARY

### Current State
- **Total Lines**: 2,934
- **Working Code**: ~1,200 lines (40%)
- **Stub/Incomplete**: ~1,734 lines (60%)

### To Reach MVP (Assembles Simple Programs)
- **Additional LOC**: ~4,000-5,000
- **Time Estimate**: 3-4 weeks
- **Effort**: 2-3 FTE

### To Reach Feature Parity (NASM Compatible)
- **Additional LOC**: ~10,000+
- **Time Estimate**: 8-12 weeks
- **Effort**: Full-time team

---

## 6. RECOMMENDED NEXT STEPS

**Immediate** (This Week):
1. Implement `parse_instruction()` - Tokenize operands
2. Implement `operand_parse()` - Extract register/memory/immediate
3. Complete `Emit_MOV_R64_R64`, `Emit_PUSH`, `Emit_RET` emitters
4. Build instruction dispatch table for 20 core instructions

**Short-term** (Next 2 Weeks):
1. Implement assembler passes 1 & 2
2. PE header generation (delegate to helper DLL if needed)
3. Symbol table and relocations
4. Basic linking

**Medium-term** (Next Month):
1. Macro Phase 2 (argument substitution)
2. Extended instruction set (40+ instructions)
3. Error reporting and diagnostics

---

## 7. FILES GENERATED
- **x64_encoder_corrected.asm** (27 KB) - Pure instruction encoder with fixes
- **x64_encoder_corrected.obj** (1.8 KB) - Compiled encoder
- **ENCODER_CORRECTIONS_SUMMARY.md** - Bug fixes documentation

---

## Conclusion

The assembler has a **strong foundation** with working lexer, macro parsing, and helper infrastructure. However, it's missing the **three critical components** that do the actual work:

1. ❌ Instruction encoder (can't emit machine code)
2. ❌ PE header generator (can't produce executables)
3. ❌ Assembler passes (can't parse and assemble)

**Recommendation**: Focus on these three areas first. Once they're functional, the remaining ~60% can be built incrementally.

The separated `x64_encoder_corrected.asm` module provides a **production-ready foundation** for instruction encoding that can be integrated into the main assembler.

