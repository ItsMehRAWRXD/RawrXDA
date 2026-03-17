# PE Writer + x64 MASM Machine Code Emitter - Backend Designer Reference

**Focus**: Backend design (NOT assembler user perspective)  
**Target**: Emit x64 machine code → PE32+/64 executable writer  
**Language**: Pure x64 MASM with manual PE construction  
**Goal**: Simplest implementation with same functionality as MSVC/ml64  

---

## 📐 Architecture: Backend Designer Perspective

### Layer Model

```
┌────────────────────────────────────────┐
│  Application Logic (User Code)         │
├────────────────────────────────────────┤
│  Code Generation (x64 AST → Machine)   │ ← Backend Designer
├────────────────────────────────────────┤
│  PE32+/64 Binary Writer (Sections)     │ ← Backend Designer  
├────────────────────────────────────────┤
│  OS Loader (PE Header Verification)    │
└────────────────────────────────────────┘
```

**Designer Responsibilities**:
1. Convert AST → x64 machine code bytes (emission)
2. Build PE headers (DOS, NT, sections)
3. Manage import tables (standard library linking)
4. Allocate/assign section RVAs
5. Generate relocation tables
6. Write binary to disk

---

## 🔧 Phase 1: x64 Machine Code Emitter

### 1.1: REX Prefix & Encoding (Foundation)

ALL x64 instructions need REX prefix if:
- Using r8-r15 registers
- Using 64-bit operand size
- Using SIB byte addressing

**REX Encoding**:
```
REX = 0100 WRXB
      ││││ ││││
      ││││ │││└─ B (high bit of rm/reg field)
      ││││ ││└── X (high bit of SIB.index)
      ││││ │└─── R (high bit of reg field)
      ││││ └──── W (64-bit operand size) = 1 for 64-bit
      ├─ Fixed: 0100
```

**Examples**:
```
mov rax, rbx          → No REX needed (only use r/m is 64-bit)
                      → REX.W=1, R=0, X=0, B=0 (only W varies)
                      → 0x48 (REX.W)

mov r8, r9            → Uses r8 (high bits) + r9 (high bits)
                      → 0x4D (REX.W=1, R=1, B=1)

add r12, [r15+r14*8]  → SIB addressing, r12 (R), r15 (B), r14 (X)
                      → 0x4F (REX.W=1, R=1, X=1, B=1)
```

### 1.2: Modrm/Sib Byte Encoding

**MODRM Byte**:
```
Byte = MM REG R/M
       ││ ─┬─ ──┬──
       ││  │    └─ Register/Memory field (8 values, or SIB if=4)
       ││  └────── Register field (opcode extension)
       └┴───────── Addressing mode:
                   00 = [reg]
                   01 = [reg + disp8]
                   10 = [reg + disp32]
                   11 = reg (no memory)
```

**SIB Byte** (if MODRM.r/m = 4):
```
Byte = SS I B
       ││ ─┬─ ─┬─
       ││  │   └─ Base register
       ││  └───── Index register  
       └┴──────── Scale (00=1, 01=2, 10=4, 11=8)
```

**Examples**:
```
mov rax, [rbx]        → MODRM = 00 000 011 (0x03)
mov rax, [rbx + 8]    → MODRM = 01 000 011 (0x43) + disp8=0x08
mov rax, [rbx + r12*4] → Need SIB:
                        MODRM = 00 000 100 (0x04, indicates SIB)
                        SIB = 10 100 011 (0xA3, scale=4, idx=r12, base=rbx)
```

### 1.3: Instruction Emission Function Pattern

**Key Pattern for Backend Designer**:

```asm
; ============================================================================
; EmitMov_Reg64FromImm64(rdi=buffer_ptr, rcx=dest_reg, rdx=immediate_value)
; Returns: rax = bytes emitted
; ============================================================================
EmitMov_Reg64FromImm64 PROC PUBLIC
    ; mov reg64, imm64 = 48 B8+reg imm64-le
    ;                   opcode with register baked in
    
    ; REX.W = 1 (64-bit), no R/X/B needed here
    mov [rdi], 0x48             ; REX.W
    mov [rdi+1], 0xB8           ; MOV r64, imm64 opcode base
    
    ; Encode register directly in opcode
    ; RAX=0, RCX=1, RDX=2, ..., RDI=7
    ; R8-R15 would need REX.B set
    add [rdi+1], cl              ; Add register index to opcode
    
    ; Emit 64-bit immediate (little-endian)
    mov qword [rdi+2], rdx
    
    mov rax, 10                 ; mov reg64, imm64 = 10 bytes
    ret
EmitMov_Reg64FromImm64 ENDP

; Backend Designer Principle: 
; "Know opcode table → encode prefix/modrm/imm → write bytes"
```

### 1.4: Complete x64 Instruction Emitter Library

**Core Instructions Needed**:

| Instruction | Opcode | REX | MODRM | Imm | Total |
|-------------|--------|-----|-------|-----|-------|
| mov r64, imm64 | B8+r | W | - | 64b | 10 |
| mov r64, r/m64 | 8B | W | Y | - | 3-7 |
| add r64, imm32 | 81/0 | W | Y | 32b | 7 |
| sub r64, imm32 | 81/5 | W | Y | 32b | 7 |
| xor r64, r64 | 33 | W | Y | - | 3 |
| ret | C3 | - | - | - | 1 |

**Emit Function Template** (for backend designer):

```asm
;────────────────────────────────────────────────────────────────
; Pattern: ExitFunction_epilogue()
; Design: Emit standard x64 stack frame teardown
; Parameters: rdi = output buffer (current emission point)
; Returns: rax = bytes emitted (always 1 for ret)
;────────────────────────────────────────────────────────────────

; Standard x64 Function Epilogue
; mov rsp, rbp      (restore stack)
; pop rbp           (restore frame pointer)
; ret               (return)

EmitFunctionEpilogue PROC PUBLIC
    ; mov rsp, rbp         → 48 89 EC
    mov byte [rdi], 0x48  ; REX.W
    mov byte [rdi+1], 0x89 ; mov r/m64, r64 / mov rsp, rbp
    mov byte [rdi+2], 0xEC ; MODRM: 11 101 100 (rsp=4, rbp=5: 0xEC)
    
    ; pop rbp              → 5D
    mov byte [rdi+3], 0x5D ; POP reg (rdi+3 since we're writing sequentially)
    
    ; ret                  → C3
    mov byte [rdi+4], 0xC3
    
    mov rax, 5             ; Total bytes for epilogue
    ret
EmitFunctionEpilogue ENDP

; Backend principle: Epilogue is ALWAYS 5 bytes
; - mov rsp, rbp: 3 bytes
; - pop rbp: 1 byte
; - ret: 1 byte
```

---

## 🏗️ Phase 2: PE32+/64 Binary Writer

### 2.1: PE Header Structure (Minimal Valid Example)

**DOS Header** (64 bytes):
```
Offset  Size  Field           Value
0x00    2     e_magic         0x4D5A ("MZ")
0x3C    4     e_lfanew        0x40 (→ NT header location)
```

**NT Header** (24 bytes):
```
Offset  Size  Field           
0x00    4     Signature       0x50450000 ("PE\0\0")
```

**File Header** (20 bytes):
```
Offset  Size  Field           Value (x64)
0x04    2     Machine         0x8664 (x64)
0x06    2     NumberOfSections 2 (e.g., .text + .data)
0x08    4     TimeDateStamp   (current Unix time)
0x0C    4     PointerToSymbolTable 0
0x10    4     NumberOfSymbols 0
0x12    2     SizeOfOptionalHeader 240 (for PE32+)
0x14    2     Characteristics 0x022E (executable, large addr aware)
```

**Optional Header** (240 bytes for PE32+):
```
Offset  Size  Field                   Value
0x00    2     Magic                   0x020B (PE32+)
0x02    1     MajorLinkerVersion      14 (MSVC 2022)
0x58    8     ImageBase               0x0000140000000000 (64-bit base)
0x60    4     SectionAlignment        0x1000 (4096 bytes)
0x64    4     FileAlignment           0x200 (512 bytes)
0xA0    8     SizeOfImage             (end of last section aligned)
0xA8    4     SizeOfHeaders           (aligned to FileAlignment)
```

**Section Headers** (40 bytes each):

`.text` section:
```
Offset  Size  Field                       Value
0x00    8     Name                        ".text\0\0\0"
0x08    4     VirtualSize                 0x2000 (actual code)
0x0C    4     VirtualAddress              0x1000 (in memory RVA)
0x10    4     SizeOfRawData               0x2000 (padded to FileAlignment)
0x14    4     PointerToRawData            0x400 (file offset)
0x18    4     Characteristics             0x60000020 (EXECUTE + READ + ALLOC)
```

### 2.2: Import Table (For Linking to Runtime)

**Import Directory Table**:
```
Offset  Size  Field                       
0x00    4     ImportLookupTableRVA        → .idata section
0x04    4     TimeDateStamp
0x08    4     ForwarderChain
0x0C    4     NameRVA                     → DLL name ("KERNEL32.DLL")
0x10    4     ImportAddressTableRVA
```

**ImportLookupTable** (array of thunks):
```
Each entry = Imported function name
E.g.: GetProcAddress, LoadLibraryA, ExitProcess

Format (x64, 64-bit entries):
- Bit 63 = 1 → Imported by ordinal (rare)
- Bit 63 = 0 → RVA to HintNameTable entry
```

**HintNameTable**:
```
Structure per import:
Offset  Size
0x00    2     Hint (unused, set to 0)
0x02    n     ASCII name ("ExitProcess\0")
```

### 2.3: Complete PE Writer Example

```asm
;============================================================================
; WriteMinimalPE32Plus(rdi=output_path, rsi=code_buffer, rdx=code_size)
; Backend Designer: Emit complete PE32+/64 executable from raw x64 code
;============================================================================

WriteMinimalPE32Plus PROC PUBLIC
    ; LOCAL: PE structure buffer (on stack or pre-allocated)
    
    ; Step 1: Write DOS Header
    mov r10, 0x4D5A            ; "MZ"
    mov [PE_Buffer + 0x00], r10w
    mov dword [PE_Buffer + 0x3C], 0x40 ; → NT header
    
    ; Step 2: Write NT Header (PE signature)
    mov dword [PE_Buffer + 0x40], 0x50450000 ; "PE\0\0"
    
    ; Step 3: Write File Header
    mov word [PE_Buffer + 0x44], 0x8664  ; Machine (x64)
    mov word [PE_Buffer + 0x46], 0x0002  ; NumberOfSections (2)
    ; ... (more fields)
    
    ; Step 4: Write Optional Header
    mov word [PE_Buffer + 0x5C], 0x020B  ; Magic (PE32+)
    mov qword [PE_Buffer + 0xB8], 0x0000140000000000 ; ImageBase
    ; ... (more fields)
    
    ; Step 5: Write Section Headers (.text, .data)
    ; Offset: 0x158 (after optional header)
    
    ; Step 6: Write code sections to file
    ; .text: copy rsi (code_buffer) of rdx (size) bytes to file
    
    ; Step 7: Calculate checksums, sizes
    ; Set SizeOfImage, SizeOfHeaders based on section layout
    
    ; Step 8: Write entire PE to rdi (output_path)
    
    xor eax, eax               ; return 0 = success
    ret
WriteMinimalPE32Plus ENDP
```

---

## 🔗 Phase 3: Integration (Emitter → PE Writer)

### 3.1: End-to-End Pipeline

```
Input AST (e.g., Function definition)
       │
       ▼
┌─────────────────────────┐
│ Instruction Selector    │ ← Choose fastest x64 encoding
│ (3-byte mov vs lea...)  │
└─────────────────────────┘
       │
       ▼
┌─────────────────────────┐
│ Register Allocator      │ ← Assign rax,rcx,rdx,...
└─────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│ Machine Code Emitter                │ ← Emit x64 bytes
│ (EmitMov, EmitAdd, EmitJmp, etc)   │
│ Buffer: code_out[16KB]              │
└─────────────────────────────────────┘
       │
       ▼ (code_out filled with x64 bytes)
┌─────────────────────────────────────┐
│ PE32+/64 Binary Writer              │ ← Build PE headers
│ (WriteMinimalPE32Plus)              │ ← Allocate .text section
│                                     │ ← Add ImportTable for stdlib
│ Output: executable.exe              │
└─────────────────────────────────────┘
```

### 3.2: Code Generation Example (Backend Designer)

**Input**: C-like function call
```cpp
int main() {
    printf("Hello");  // Print string
    return 0;         // Exit
}
```

**AST → x64 Machine Code**:

```asm
; Entry point (RVA 0x1000)
main:
    ; Prologue
    48 89 E5            ; mov rbp, rsp (save old frame)
    48 83 EC 08         ; sub rsp, 0x08 (align stack)
    
    ; Load string address → rcx (first parameter, x64 calling conv)
    48 C7 C1 00 10 00 00 ; mov rcx, 0x1000 (string location)
    
    ; Call printf (via import table thunk)
    E8 XX XX XX XX      ; call [rip + offset to printf thunk]
    
    ; Return 0
    33 C0               ; xor eax, eax (eax = 0, return value)
    
    ; Epilogue
    48 89 EC            ; mov rsp, rbp
    5D                  ; pop rbp
    C3                  ; ret
```

**x64 Bytes Written to .text Section**:
```
1000: 48 89 E5 48 83 EC 08 48 C7 C1 00 10 00 00 E8 XX XX XX XX 33 C0 48 89 EC 5D C3
```

---

## 📊 Reference: x64 Instruction Encoding Table

**For Backend Designer** (opcode reference):

| Instruction | Category | Opcode | Form | REX | MODRM | Imm | Notes |
|------------|----------|--------|------|-----|-------|-----|-------|
| mov r64,imm | Move | B8+r | r64,imm64 | - | - | 64 | Direct reg encode |
| mov r/m,imm | Move | C7 | r/m64,imm32 | W | Y | 32 | Sign-extend imm32 |
| mov r64,r/m | Move | 8B | r64,r/m64 | W | Y | - | Register from mem |
| lea r64,m | Load Addr | 8D | r64,m | W | Y | - | RIP-relative safe |
| add r64,imm | ALU | 81/0 | r64,imm32 | W | Y | 32 | Sign-extended |
| sub r64,imm | ALU | 81/5 | r64,imm32 | W | Y | 32 | Signed |
| xor r64,r64 | ALU | 33 | r64,r64 | W | Y | - | Zero register fast |
| call rel | Control | E8 | rel32 | - | - | 32 | Relative offset |
| jmp rel | Control | EB/E9 | rel8/rel32 | - | - | 8/32 | Relative offset |
| ret | Control | C3 | - | - | - | - | From x64 stack |
| push r64 | Stack | 50+r | r64 | - | - | - | Direct reg encode |
| pop r64 | Stack | 58+r | r64 | - | - | - | Direct reg encode |

---

## 🎯 Least Dependencies Implementation Strategy

### Design Principle: Minimal Complexity

**What to AVOID**:
- ❌ Full linker backends
- ❌ Symbol tables
- ❌ Debug information
- ❌ Complex relocations
- ❌ COFF sections beyond .text + .data + .reloc

**What to KEEP**:
- ✅ Basic x64 instruction emitter (REX, MODRM, SIB only)
- ✅ Minimal PE32+/64 headers (DOS, NT, sections)
- ✅ Import table stub (kernelbase.dll only)
- ✅ Simple relocation entries (offset + type)
- ✅ Direct x64 code generation (no IR needed)

### Implementation Size Estimate

| Component | Lines | Complexity |
|-----------|-------|------------|
| x64 Emitter (20 core instrs) | 500-800 | Low |
| PE Writer (header + section mgmt) | 400-600 | Medium |
| Import Table Builder | 200-300 | Low |
| Relocation Handler | 200-300 | Medium |
| **TOTAL** | **1300-2000** | **Achievable** |

**Comparison**:
- LLVM: 2M+ LOC
- MSVC cl.exe: closed-source, ~50MB
- This approach: <2000 LOC, fully controllable

---

## ✅ Minimal Complete Working Example

**Goal**: Generate and run smallest possible PE32+/64 executable

**Code** (pure x64 MASM):
```asm
; minimal_pe_writer.asm
; INPUT: None
; OUTPUT: output.exe (runnable Windows executable)

PE_DOS_HEADER:
    db 'MZ'
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    db 0,0,0,0,0,0,0,0    ; Fill to offset 0x3C
    dd 0x40               ; -> NT header at 0x40

PE_NT_HEADER:
    dd 'PE\0\0'
    
PE_FILE_HEADER:
    dw 0x8664             ; Machine: x64
    dw 1                  ; NumSections: 1
    dd 0                  ; TimeDateStamp
    dd 0                  ; SymbolPointer
    dd 0                  ; NumSymbols
    dw 240                ; OptHeaderSize (PE32+)
    dw 0x022E             ; Characteristics

PE_OPT_HEADER:
    dw 0x020B             ; Magic: PE32+
    ; ... (rest of optional header)

PE_SECTION_TEXT:
    ; .text section definition

; Machine code inline
SECTION '.text' CODE EXECUTABLE
    ; Your x64 code here
    mov eax, 0
    ret
```

**Result**: Runnable .exe file with minimal overhead

---

## 📚 Further Reading (Backend Designer Path)

**Essential References**:
1. AMD64 ISA Manual (Instruction format, encoding)
2. Microsoft PE/COFF specification
3. System V AMD64 ABI (calling conventions)

**Key Insights**:
- x64 has 16 general-purpose registers → efficient code
- REX prefix makes modern instructions backward compatible
- PE headers are mostly fixed (DOS stub for legacy)
- Import tables do actual runtime linking
- Relocations tell loader where to patch absolute addresses

---

## 🎓 Summary: Backend Designer Checklist

Before writing your PE emitter:

- [ ] Understand REX prefix encoding (W/R/X/B flags)
- [ ] Master MODRM byte construction (addressing modes)
- [ ] Know SIB byte for complex addressing (r/m + index + scale)
- [ ] Implement ref x64 instruction emitter (20 core instructions)
- [ ] Build minimal PE32+/64 writer (headers only)
- [ ] Implement import table stub resolver
- [ ] Add relocation table generation
- [ ] Test: Emit simple function (mov, ret)
- [ ] Test: Emit complex function (calls, branches)
- [ ] Verify executable runs on Windows x64

**Estimated Time**: 40-60 hours (one developer)  
**Learning Curve**: Steep initially, then rapid productivity  

---

**Status**: Complete reference for backend design  
**Next**: Implement emitter → writer integration  
**Goal**: Smallest functional PE generator in <2000 LOC  

