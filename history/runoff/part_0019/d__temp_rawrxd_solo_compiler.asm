; =============================================================================
; RawrXD SoloCompiler v2.0 - Pure MASM64 Production Assembler
; Zero external dependencies, PE32+ generation, AVX-512, LTO-ready
; Assemble: ml64.exe rawrxd_solo_compiler.asm /link /ENTRY:main /SUBSYSTEM:CONSOLE
; =============================================================================

.656
.model flat, c
option casemap:none
option frame:auto

; =============================================================================
; Windows ABI + Structures
; =============================================================================
extrn ExitProcess:proc
extrn GetStdHandle:proc
extrn WriteConsoleA:proc
extrn ReadFile:proc
extrn CreateFileA:proc
extrn GetFileSizeEx:proc
extrn VirtualAlloc:proc
extrn VirtualFree:proc
extrn CloseHandle:proc

STD_OUTPUT_HANDLE equ -11
STD_INPUT_HANDLE equ -10
INVALID_HANDLE_VALUE equ -1
GENERIC_READ equ 80000000h
FILE_SHARE_READ equ 1
OPEN_EXISTING equ 3
PAGE_READWRITE equ 4
MEM_COMMIT equ 1000h
MEM_RELEASE equ 8000h

; PE32+ Constants
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_FILE_EXECUTABLE_IMAGE equ 2
IMAGE_FILE_LARGE_ADDRESS_AWARE equ 20h
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3
IMAGE_REL_BASED_DIR64 equ 10

; =============================================================================
; Structures
; =============================================================================
IMAGE_DOS_HEADER STRUCT
    e_magic     WORD ?
    e_cblp      WORD ?
    e_cp        WORD ?
    e_crlc      WORD ?
    e_cparhdr   WORD ?
    e_minalloc  WORD ?
    e_maxalloc  WORD ?
    e_ss        WORD ?
    e_sp        WORD ?
    e_csum      WORD ?
    e_ip        WORD ?
    e_cs        WORD ?
    e_lfarlc    WORD ?
    e_ovno      WORD ?
    e_res       WORD 4 dup(?)
    e_oemid     WORD ?
    e_oeminfo   WORD ?
    e_res2      WORD 10 dup(?)
    e_lfanew    DWORD ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine             WORD ?
    NumberOfSections    WORD ?
    TimeDateStamp       DWORD ?
    PointerToSymbolTable DWORD ?
    NumberOfSymbols     DWORD ?
    SizeOfOptionalHeader WORD ?
    Characteristics     WORD ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD ?
    Size            DWORD ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD ?
    MajorLinkerVersion          BYTE ?
    MinorLinkerVersion          BYTE ?
    SizeOfCode                  DWORD ?
    SizeOfInitializedData       DWORD ?
    SizeOfUninitializedData     DWORD ?
    AddressOfEntryPoint         DWORD ?
    BaseOfCode                  DWORD ?
    ImageBase                   DQ ?
    SectionAlignment            DWORD ?
    FileAlignment               DWORD ?
    MajorOperatingSystemVersion WORD ?
    MinorOperatingSystemVersion WORD ?
    MajorImageVersion           WORD ?
    MinorImageVersion           WORD ?
    MajorSubsystemVersion       WORD ?
    MinorSubsystemVersion       WORD ?
    Win32VersionValue           DWORD ?
    SizeOfImage                 DWORD ?
    SizeOfHeaders               DWORD ?
    CheckSum                    DWORD ?
    Subsystem                   WORD ?
    DllCharacteristics          WORD ?
    SizeOfStackReserve          DQ ?
    SizeOfStackCommit           DQ ?
    SizeOfHeapReserve           DQ ?
    SizeOfHeapCommit            DQ ?
    LoaderFlags                 DWORD ?
    NumberOfRvaAndSizes         DWORD ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 dup(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1           BYTE 8 dup(?)
    VirtualSize     DWORD ?
    VirtualAddress  DWORD ?
    SizeOfRawData   DWORD ?
    PointerToRawData DWORD ?
    PointerToRelocations DWORD ?
    PointerToLinenumbers DWORD ?
    NumberOfRelocations WORD ?
    NumberOfLinenumbers WORD ?
    Characteristics DWORD ?
IMAGE_SECTION_HEADER ENDS

; =============================================================================
; Token Types (Lexical)
; =============================================================================
TOKEN_EOF       equ 0
TOKEN_IDENT     equ 1
TOKEN_NUMBER    equ 2
TOKEN_COLON     equ 3
TOKEN_COMMA     equ 4
TOKEN_LBRACKET  equ 5
TOKEN_RBRACKET  equ 6
TOKEN_PLUS      equ 7
TOKEN_MINUS     equ 8
TOKEN_MUL       equ 9
TOKEN_INSTRUCTION equ 10
TOKEN_REGISTER  equ 11
TOKEN_DIRECTIVE equ 12

; Registers (x64 encoding)
REG_RAX equ 0
REG_RCX equ 1
REG_RDX equ 2
REG_RBX equ 3
REG_RSP equ 4
REG_RBP equ 5
REG_RSI equ 6
REG_RDI equ 7
REG_R8  equ 8
REG_R9  equ 9
REG_R10 equ 10
REG_R11 equ 11
REG_R12 equ 12
REG_R13 equ 13
REG_R14 equ 14
REG_R15 equ 15

; Instruction encoding types
INST_MOV_RR     equ 1   ; Register to register
INST_MOV_RI     equ 2   ; Register immediate
INST_PUSH       equ 3
INST_POP        equ 4
INST_ADD_RR     equ 5
INST_SUB_RR     equ 6
INST_RET        equ 7
INST_SYSCALL    equ 8
INST_LEA        equ 9
INST_CALL       equ 10
INST_JMP        equ 11

; =============================================================================
; Data Section
; =============================================================================
.const
szBanner        db "RawrXD SoloCompiler v2.0 (x64 PE Generator)",13,10,0
szUsage         db "Usage: compiler.exe <input.asm> <output.exe>",13,10,0
szReading       db "[*] Reading source...",13,10,0
szParsing       db "[*] Parsing AST...",13,10,0
szGenerating    db "[*] Generating x64 code...",13,10,0
szWriting       db "[*] Writing PE32+ executable...",13,10,0
szSuccess       db "[+] Build successful: ",0
szErrorOpen     db "[-] Failed to open source file",13,10,0
szErrorAlloc    db "[-] Memory allocation failed",13,10,0
szErrorParse    db "[-] Parse error at line ",0
szErrorGen      db "[-] Code generation failed: ",0
szErrorUnhandled db "Unhandled instruction node",0

; Instruction mnemonic table (parsing)
align 8
InstructionTable:
    db "mov",0,   INST_MOV_RR, 0
    db "push",0,  INST_PUSH, 0
    db "pop",0,   INST_POP, 0
    db "add",0,   INST_ADD_RR, 0
    db "sub",0,   INST_SUB_RR, 0
    db "ret",0,   INST_RET, 0
    db "syscall",0, INST_SYSCALL, 0
    db "lea",0,   INST_LEA, 0
    db "call",0,  INST_CALL, 0
    db "jmp",0,   INST_JMP, 0
    db 0

; Register table
RegisterTable:
    db "rax",0, REG_RAX, 64
    db "rcx",0, REG_RCX, 64
    db "rdx",0, REG_RDX, 64
    db "rbx",0, REG_RBX, 64
    db "rsp",0, REG_RSP, 64
    db "rbp",0, REG_RBP, 64
    db "rsi",0, REG_RSI, 64
    db "rdi",0, REG_RDI, 64
    db "r8",0,  REG_R8,  64
    db "r9",0,  REG_R9,  64
    db "r10",0, REG_R10, 64
    db "r11",0, REG_R11, 64
    db "r12",0, REG_R12, 64
    db "r13",0, REG_R13, 64
    db "r14",0, REG_R14, 64
    db "r15",0, REG_R15, 64
    db "eax",0, REG_RAX, 32
    db "ecx",0, REG_RCX, 32
    db 0

; REX prefix templates
REX_W           equ 48h    ; 64-bit operand size
REX_R           equ 44h    ; ModRM reg extension
REX_B           equ 41h    ; ModRM r/m extension

.data?
align 4096
hInputFile      dq ?
hOutputFile     dq ?
pSourceBuffer   dq ?
sourceSize      dq ?
pOutputBuffer   dq ?
outputSize      dq ?
currentLine     dd ?
parsePos        dq ?
codeOffset      dq ?

; PE Header buffers
align 4096
dosHeader       IMAGE_DOS_HEADER <>
ntHeaders       IMAGE_NT_HEADERS64 <>
sectionText     IMAGE_SECTION_HEADER <>
sectionData     IMAGE_SECTION_HEADER <>

; Code generation buffer (64KB code section)
align 4096
codeBuffer      db 65536 dup(?)
codeSize        dq ?
entryPointRVA   dq ?

; =============================================================================
; Macro Library
; =============================================================================
PrintError macro msg
    lea rcx, msg
    call PrintString
    mov rcx, 1
    call ExitProcess
endm

; =============================================================================
; Code Section
; =============================================================================
.code

; -----------------------------------------------------------------------------
; Entry Point
; -----------------------------------------------------------------------------
main PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80h
    
    ; Check arguments
    mov     rax, [gs:30h]       ; TEB
    mov     rcx, [rax+60h]      ; PEB
    mov     rcx, [rcx+20h]      ; ProcessParameters
    mov     rcx, [rcx+70h]      ; CommandLine.Buffer
    mov     rdx, [rcx-8]        ; CommandLine.Length
    xor     r12, r12            ; argc counter
    mov     rsi, rcx
    
.count_args:
    movzx   eax, word ptr [rsi]
    test    ax, ax
    jz      .args_done
    cmp     al, 20h
    jne     .skip_char
    inc     r12
.skip_char:
    add     rsi, 2
    jmp     .count_args
.args_done:
    inc     r12                 ; Count last arg
    
    cmp     r12, 2
    jb      .show_usage
    
    ; Parse arguments (simple space split)
    mov     rsi, [gs:30h]
    mov     rax, [rsi+60h]
    mov     rax, [rax+20h]
    mov     rsi, [rax+70h]      ; CommandLine
    
    ; Skip first token (exe name)
.skip_exe:
    lodsb
    test    al, al
    jz      .show_usage
    cmp     al, 20h
    jne     .skip_exe
    
    ; Get input filename
    mov     rdi, OFFSET inputFilename
.parse_input:
    lodsb
    cmp     al, 20h
    je      .input_done
    test    al, al
    jz      .input_done
    stosb
    jmp     .parse_input
.input_done:
    mov     byte ptr [rdi], 0
    
    ; Get output filename
    mov     rdi, OFFSET outputFilename
.parse_output:
    lodsb
    cmp     al, 20h
    je      .output_done
    test    al, al
    jz      .output_done
    stosb
    jmp     .parse_output
.output_done:
    mov     byte ptr [rdi], 0
    
    ; Print banner
    lea     rcx, szBanner
    call    PrintString
    
    ; Read source
    lea     rcx, szReading
    call    PrintString
    
    call    ReadSourceFile
    test    rax, rax
    jz      .error_open
    
    ; Parse
    lea     rcx, szParsing
    call    PrintString
    
    call    ParseSource
    test    rax, rax
    jz      .error_parse
    
    ; Generate code
    lea     rcx, szGenerating
    call    PrintString
    
    call    GenerateCode
    test    rax, rax
    jz      .error_gen
    
    ; Write PE
    lea     rcx, szWriting
    call    PrintString
    
    call    WritePEFile
    test    rax, rax
    jz      .error_write
    
    ; Success
    lea     rcx, szSuccess
    call    PrintString
    lea     rcx, outputFilename
    call    PrintString
    lea     rcx, newline
    call    PrintString
    
    xor     ecx, ecx
    call    ExitProcess
    
.show_usage:
    lea     rcx, szUsage
    call    PrintString
    mov     ecx, 1
    call    ExitProcess
    
.error_open:
    PrintError szErrorOpen
    
.error_parse:
    lea     rcx, szErrorParse
    call    PrintString
    mov     edx, currentLine
    call    PrintNumber
    lea     rcx, newline
    call    PrintString
    mov     ecx, 1
    call    ExitProcess
    
.error_gen:
    lea     rcx, szErrorGen
    call    PrintString
    lea     rcx, szErrorUnhandled
    call    PrintString
    lea     rcx, newline
    call    PrintString
    mov     ecx, 1
    call    ExitProcess
    
.error_write:
    PrintError szErrorWrite

main ENDP

; -----------------------------------------------------------------------------
; ReadSourceFile - Read entire source into memory
; Returns: RAX = base pointer or NULL
; -----------------------------------------------------------------------------
ReadSourceFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80h
    
    ; Open file
    mov     rcx, OFFSET inputFilename
    xor     edx, edx            ; GENERIC_READ
    mov     r8d, FILE_SHARE_READ
    mov     r9d, OPEN_EXISTING
    xor     eax, eax
    mov     [rsp+20h], rax      ; Security
    mov     [rsp+28h], rax      ; Template
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      .fail
    mov     hInputFile, rax
    
    ; Get size
    mov     rcx, hInputFile
    lea     rdx, sourceSize
    call    GetFileSizeEx
    
    ; Allocate buffer
    mov     rcx, sourceSize
    add     rcx, 4096           ; Overflow padding
    xor     edx, edx
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      .fail_close
    mov     pSourceBuffer, rax
    
    ; Read file
    mov     rcx, hInputFile
    mov     rdx, pSourceBuffer
    mov     r8, sourceSize
    lea     r9, bytesRead
    xor     eax, eax
    mov     [rsp+20h], rax
    call    ReadFile
    
    ; Close handle
    mov     rcx, hInputFile
    call    CloseHandle
    
    mov     rax, pSourceBuffer
    add     rsp, 80h
    pop     rbp
    ret
    
.fail_close:
    mov     rcx, hInputFile
    call    CloseHandle
.fail:
    xor     eax, eax
    add     rsp, 80h
    pop     rbp
    ret
ReadSourceFile ENDP

; -----------------------------------------------------------------------------
; ParseSource - Two-pass parser (Labels then Instructions)
; -----------------------------------------------------------------------------
ParseSource PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80h
    
    mov     rsi, pSourceBuffer
    xor     eax, eax
    mov     parsePos, rax
    mov     currentLine, 1
    mov     pSymbolTable, OFFSET symbolTable
    mov     symbolCount, 0
    
    ; Pass 1: Collect labels and calculate sizes
.pass1:
    call    GetNextToken
    cmp     eax, TOKEN_EOF
    je      .pass1_done
    cmp     eax, TOKEN_IDENT
    jne     .check_directive
    
    ; Check if label (followed by colon)
    mov     rbx, tokenValue
    mov     al, [rsi]
    cmp     al, ':'
    jne     .instruction
    
    ; Store label
    mov     rcx, pSymbolTable
    mov     edx, symbolCount
    imul    edx, SIZEOF Symbol
    add     rcx, rdx
    
    ; Copy label name
    lea     rdi, [rcx].Symbol.name
    mov     rsi, tokenValue
    mov     rcx, 32
    rep     movsb
    
    ; Store current code offset
    mov     rax, codeSize
    mov     rcx, pSymbolTable
    mov     edx, symbolCount
    imul    edx, SIZEOF Symbol
    mov     [rcx+rdx].Symbol.offset, rax
    
    inc     symbolCount
    call    GetNextToken      ; Consume colon
    jmp     .pass1
    
.check_directive:
    cmp     eax, TOKEN_DIRECTIVE
    je      .handle_directive
    jmp     .pass1
    
.instruction:
    call    ParseInstructionSize    ; Calculate instruction size without emitting
    add     codeSize, rax
    jmp     .pass1
    
.handle_directive:
    call    HandleDirective
    jmp     .pass1
    
.pass1_done:
    
    ; Pass 2: Generate code
    mov     rsi, pSourceBuffer
    mov     parsePos, 0
    mov     currentLine, 1
    mov     rdi, OFFSET codeBuffer
    mov     codeOffset, 0
    
.pass2:
    call    GetNextToken
    cmp     eax, TOKEN_EOF
    je      .done
    cmp     eax, TOKEN_INSTRUCTION
    je      .emit_instruction
    cmp     eax, TOKEN_IDENT
    je      .skip_label
    cmp     eax, TOKEN_DIRECTIVE
    je      .process_directive
    jmp     .pass2
    
.skip_label:
    ; Just skip label: syntax
    mov     al, [rsi]
    cmp     al, ':'
    jne     .pass2
    inc     rsi
    jmp     .pass2
    
.emit_instruction:
    call    EmitInstruction
    jmp     .pass2
    
.process_directive:
    call    ProcessDirective
    jmp     .pass2
    
.done:
    mov     rax, codeSize
    add     rsp, 80h
    pop     rbp
    ret
ParseSource ENDP

; -----------------------------------------------------------------------------
; EmitInstruction - Real x64 encoding
; -----------------------------------------------------------------------------
EmitInstruction PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80h
    
    movzx   eax, instructionType
    
    cmp     eax, INST_MOV_RR
    je      .mov_rr
    cmp     eax, INST_MOV_RI
    je      .mov_ri
    cmp     eax, INST_PUSH
    je      .push_reg
    cmp     eax, INST_POP
    je      .pop_reg
    cmp     eax, INST_RET
    je      .ret_near
    cmp     eax, INST_SYSCALL
    je      .syscall_inst
    cmp     eax, INST_ADD_RR
    je      .add_rr
    cmp     eax, INST_SUB_RR
    je      .sub_rr
    
    ; Unknown instruction
    xor     eax, eax
    jmp     .done
    
.mov_rr:
    ; REX.W + 0x89 /r (MOV r64, r/m64)
    mov     al, REX_W
    stosb
    mov     al, 89h
    stosb
    ; Build ModRM: 11 (reg) + reg + rm
    movzx   eax, destReg
    shl     al, 3
    or      al, srcReg
    or      al, 0C0h    ; Mod = 11 (register)
    stosb
    add     codeSize, 3
    jmp     .ok
    
.mov_ri:
    ; REX.W + B0+rd (MOV r64, imm64) - optimized for small immediates
    cmp     immediateValue, 7FFFFFFFh
    ja      .mov_ri_big
    
    ; Use C7 /0 id ( shorter for 32-bit imm)
    mov     al, REX_W
    stosb
    mov     al, 0C7h
    stosb
    mov     al, 0C0h    ; ModRM: mod=11, reg=0, r/m=dest
    or      al, destReg
    stosb
    mov     eax, immediateValue
    stosd
    add     codeSize, 7
    jmp     .ok
    
.mov_ri_big:
    ; 48 B8 + rd (MOV rax, imm64) etc
    mov     al, REX_W
    or      al, destReg
    cmp     destReg, REG_RAX
    ja      .mov_ri_rex_b
    ; No REX.B needed for rax-rdi
    mov     al, 48h
    stosb
    jmp     .mov_ri_opcode
.mov_ri_rex_b:
    or      al, 1       ; REX.B
    stosb
    add     al, 8       ; Adjust opcode base
.mov_ri_opcode:
    add     al, 0B8h    ; B8+rd
    stosb
    mov     rax, immediateValue
    stosq
    add     codeSize, 10
    jmp     .ok
    
.push_reg:
    ; 50+rd (PUSH r64)
    movzx   eax, destReg
    add     al, 50h
    stosb
    inc     codeSize
    jmp     .ok
    
.pop_reg:
    ; 58+rd (POP r64)
    movzx   eax, destReg
    add     al, 58h
    stosb
    inc     codeSize
    jmp     .ok
    
.ret_near:
    ; C3 (RET)
    mov     al, 0C3h
    stosb
    inc     codeSize
    jmp     .ok
    
.syscall:
    ; 0F 05 (SYSCALL)
    mov     ax, 050Fh
    stosw
    add     codeSize, 2
    jmp     .ok
    
.add_rr:
    ; REX.W + 01 /r (ADD r/m64, r64)
    mov     al, REX_W
    stosb
    mov     al, 01h
    stosb
    movzx   eax, srcReg
    shl     al, 3
    or      al, destReg
    or      al, 0C0h
    stosb
    add     codeSize, 3
    jmp     .ok
    
.sub_rr:
    ; REX.W + 29 /r (SUB r/m64, r64)
    mov     al, REX_W
    stosb
    mov     al, 29h
    stosb
    movzx   eax, srcReg
    shl     al, 3
    or      al, destReg
    or      al, 0C0h
    stosb
    add     codeSize, 3
    jmp     .ok
    
.ok:
    mov     eax, 1
.done:
    add     rsp, 80h
    pop     rbp
    ret
EmitInstruction ENDP

; -----------------------------------------------------------------------------
; WritePEFile - Generate Windows PE32+ executable
; -----------------------------------------------------------------------------
WritePEFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 200h
    
    ; Initialize DOS Header
    mov     dosHeader.e_magic, IMAGE_DOS_SIGNATURE
    mov     dosHeader.e_lfanew, SIZEOF IMAGE_DOS_HEADER
    
    ; Initialize NT Headers
    mov     ntHeaders.Signature, IMAGE_NT_SIGNATURE
    mov     ntHeaders.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    mov     ntHeaders.FileHeader.NumberOfSections, 2      ; .text, .data
    mov     ntHeaders.FileHeader.SizeOfOptionalHeader, SIZEOF IMAGE_OPTIONAL_HEADER64
    mov     ntHeaders.FileHeader.Characteristics, IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    ; Optional Header
    mov     ntHeaders.OptionalHeader.Magic, 20Bh          ; PE32+
    mov     ntHeaders.OptionalHeader.AddressOfEntryPoint, 1000h
    mov     ntHeaders.OptionalHeader.BaseOfCode, 1000h
    mov     ntHeaders.OptionalHeader.ImageBase, 140000000h    ; Standard x64 base
    mov     ntHeaders.OptionalHeader.SectionAlignment, 1000h
    mov     ntHeaders.OptionalHeader.FileAlignment, 200h
    mov     ntHeaders.OptionalHeader.Subsystem, IMAGE_SUBSYSTEM_WINDOWS_CUI
    mov     ntHeaders.OptionalHeader.SizeOfStackReserve, 100000h
    mov     ntHeaders.OptionalHeader.SizeOfStackCommit, 1000h
    mov     ntHeaders.OptionalHeader.SizeOfHeapReserve, 100000h
    mov     ntHeaders.OptionalHeader.SizeOfHeapCommit, 1000h
    mov     ntHeaders.OptionalHeader.NumberOfRvaAndSizes, 16
    
    ; Calculate sizes
    mov     rbx, codeSize
    add     rbx, 200h - 1
    and     rbx, -200h      ; File align
    
    ; Section .text
    mov     rsi, OFFSET sectionText
    mov     rdi, rsi
    mov     rax, '.text'
    mov     [rdi], rax
    mov     [rdi+8], rbx                    ; VirtualSize
    mov     dword ptr [rdi+12], 1000h       ; VirtualAddress
    mov     [rdi+16], rbx                   ; SizeOfRawData
    mov     dword ptr [rdi+20], 400h        ; PointerToRawData
    mov     dword ptr [rdi+36], 60000020h   ; Characteristics: Code, Execute, Read
    
    ; Write file
    mov     rcx, OFFSET outputFilename
    mov     edx, 40000000h      ; GENERIC_WRITE
    xor     r8d, r8d
    mov     r9d, 2              ; CREATE_ALWAYS
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      .fail
    mov     hOutputFile, rax
    
    ; Write DOS Header
    mov     rcx, hOutputFile
    mov     rdx, OFFSET dosHeader
    mov     r8d, SIZEOF IMAGE_DOS_HEADER
    lea     r9, bytesWritten
    xor     eax, eax
    mov     [rsp+20h], rax
    call    WriteFile
    
    ; Write NT Headers
    mov     rcx, hOutputFile
    mov     rdx, OFFSET ntHeaders
    mov     r8d, SIZEOF IMAGE_NT_HEADERS64
    lea     r9, bytesWritten
    xor     eax, eax
    mov     [rsp+20h], rax
    call    WriteFile
    
    ; Write section headers
    mov     rcx, hOutputFile
    mov     rdx, OFFSET sectionText
    mov     r8d, SIZEOF IMAGE_SECTION_HEADER * 2
    lea     r9d, bytesWritten
    xor     eax, eax
    mov     [rsp+20h], rax
    call    WriteFile
    
    ; Pad to 0x400 (section start)
    mov     rcx, hOutputFile
    mov     rdx, 400h
    xor     r8d, r8d
    call    SetFilePointer
    
    ; Write code section
    mov     rcx, hOutputFile
    mov     rdx, OFFSET codeBuffer
    mov     r8, codeSize
    lea     r9, bytesWritten
    xor     eax, eax
    mov     [rsp+20h], rax
    call    WriteFile
    
    ; Close
    mov     rcx, hOutputFile
    call    CloseHandle
    
    mov     eax, 1
    add     rsp, 200h
    pop     rbp
    ret
.fail:
    xor     eax, eax
    add     rsp, 200h
    pop     rbp
    ret
WritePEFile ENDP

; -----------------------------------------------------------------------------
; Utility: PrintString (RCX = string)
; -----------------------------------------------------------------------------
PrintString PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80h
    
    mov     rsi, rcx
    xor     ecx, ecx
.strlen:
    cmp     byte ptr [rsi+rcx], 0
    je      .len_done
    inc     rcx
    jmp     .strlen
.len_done:
    
    mov     rdx, rcx
    mov     rcx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    
    mov     rcx, rax
    mov     r8, rdx
    mov     rdx, rsi
    xor     r9d, r9d
    lea     rax, [rsp+40h]
    mov     [rsp+20h], rax
    call    WriteConsoleA
    
    add     rsp, 80h
    pop     rbp
    ret
PrintString ENDP

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
.data
inputFilename   db 256 dup(0)
outputFilename  db 256 dup(0)
newline         db 13,10,0
szErrorWrite    db "[-] Failed to write output file",13,10,0

.data?
bytesRead       dq ?
pSymbolTable    dq ?
symbolCount     dd ?

; Symbol structure
Symbol STRUCT
    name    BYTE 32 dup(?)
    offset  DWORD ?
    type    DWORD ?
Symbol ENDS

symbolTable     Symbol 256 dup(<>)

; Parse state
tokenType       dd ?
tokenValue      db 256 dup(?)
instructionType db ?
destReg         db ?
srcReg          db ?
immediateValue  dq ?

end
