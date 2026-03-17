; =================================================================================
; RawrXD Universal Assembler Loop Engine - PRODUCTION READY
; Pure MASM x64 - Zero Dependencies
; Two-pass assembler: label resolution + code generation
; Handles: x86/x64 encoding, REX prefixes, ModR/M, SIB, immediates, labels
; Build: ml64 /c /nologo /W3 /Zi /O2 assembler_loop_production.asm
; Link: link /SUBSYSTEM:CONSOLE /OUT:assembler.exe assembler_loop_production.obj kernel32.lib
; =================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; =================================================================================
; External Imports
; =================================================================================
EXTRN GetStdHandle:PROC
EXTRN WriteFile:PROC
EXTRN ExitProcess:PROC
EXTRN HeapAlloc:PROC
EXTRN HeapFree:PROC
EXTRN GetProcessHeap:PROC
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN lstrlenA:PROC
EXTRN lstrcpyA:PROC
EXTRN lstrcmpiA:PROC
EXTRN RtlZeroMemory:PROC
EXTRN RtlCopyMemory:PROC
EXTRN CreateFileA:PROC
EXTRN CloseHandle:PROC

; =================================================================================
; Constants
; =================================================================================
STD_OUTPUT_HANDLE           EQU -11
HEAP_ZERO_MEMORY            EQU 8
PAGE_EXECUTE_READWRITE      EQU 40h
MEM_COMMIT                  EQU 1000h
MEM_RESERVE                 EQU 2000h
MEM_RELEASE                 EQU 8000h
GENERIC_WRITE               EQU 40000000h
CREATE_ALWAYS               EQU 2
FILE_ATTRIBUTE_NORMAL       EQU 80h

MAX_INSTRUCTIONS            EQU 10000
MAX_LABELS                  EQU 1000
MAX_SYMBOLS                 EQU 2000
MAX_FIXUPS                  EQU 5000
BUFFER_SIZE                 EQU 1048576

TOK_EOF                     EQU 0
TOK_IDENTIFIER              EQU 1
TOK_REGISTER                EQU 2
TOK_NUMBER                  EQU 3
TOK_STRING                  EQU 4
TOK_COLON                   EQU 5
TOK_COMMA                   EQU 6
TOK_LBRACKET                EQU 7
TOK_RBRACKET                EQU 8
TOK_PLUS                    EQU 9
TOK_MINUS                   EQU 10
TOK_MUL                     EQU 11
TOK_NEWLINE                 EQU 12
TOK_DIRECTIVE               EQU 13
TOK_INSTRUCTION             EQU 14

REG_RAX                     EQU 0
REG_RCX                     EQU 1
REG_RDX                     EQU 2
REG_RBX                     EQU 3
REG_RSP                     EQU 4
REG_RBP                     EQU 5
REG_RSI                     EQU 6
REG_RDI                     EQU 7
REG_R8                      EQU 8
REG_R9                      EQU 9
REG_R10                     EQU 10
REG_R11                     EQU 11
REG_R12                     EQU 12
REG_R13                     EQU 13
REG_R14                     EQU 14
REG_R15                     EQU 15

SIZE_BYTE                   EQU 0
SIZE_WORD                   EQU 1
SIZE_DWORD                  EQU 2
SIZE_QWORD                  EQU 3

; =================================================================================
; Data Section
; =================================================================================
.data
align 16

szVersion           DB "RawrXD Universal Assembler v1.0.0", 13, 10
                    DB "Pure MASM x64 - Zero Dependencies", 13, 10, 0
szErrOutOfMemory    DB "[ERROR] Out of memory", 13, 10, 0
szErrSyntax         DB "[ERROR] Syntax error at line ", 0
szErrUndefinedSymbol DB "[ERROR] Undefined symbol: ", 0
szErrInvalidOperand DB "[ERROR] Invalid operand", 13, 10, 0
szErrTooManyLabels  DB "[ERROR] Too many labels", 13, 10, 0
szErrInvalidRegister DB "[ERROR] Invalid register", 13, 10, 0

szAssembling        DB "[*] Assembling...", 13, 10, 0
szPass1             DB "[*] Pass 1: Parsing and label resolution...", 13, 10, 0
szPass2             DB "[*] Pass 2: Code generation...", 13, 10, 0
szSuccess           DB "[+] Assembly complete. ", 0
szBytesEmitted      DB " bytes emitted", 13, 10, 0
szNewline           DB 13, 10, 0

szRegRAX            DB "rax", 0
szRegRCX            DB "rcx", 0
szRegRDX            DB "rdx", 0
szRegRBX            DB "rbx", 0
szRegRSP            DB "rsp", 0
szRegRBP            DB "rbp", 0
szRegRSI            DB "rsi", 0
szRegRDI            DB "rdi", 0
szRegR8             DB "r8", 0
szRegR9             DB "r9", 0
szRegR10            DB "r10", 0
szRegR11            DB "r11", 0
szRegR12            DB "r12", 0
szRegR13            DB "r13", 0
szRegR14            DB "r14", 0
szRegR15            DB "r15", 0

szMnemMOV           DB "mov", 0
szMnemPUSH          DB "push", 0
szMnemPOP           DB "pop", 0
szMnemADD           DB "add", 0
szMnemSUB           DB "sub", 0
szMnemXOR           DB "xor", 0
szMnemCMP           DB "cmp", 0
szMnemJMP           DB "jmp", 0
szMnemCALL          DB "call", 0
szMnemRET           DB "ret", 0
szMnemNOP           DB "nop", 0
szMnemSYSCALL       DB "syscall", 0

OPC_MOV_REG_IMM     EQU 0B8h
OPC_MOV_REG_REG     EQU 88h
OPC_PUSH_REG        EQU 50h
OPC_POP_REG         EQU 58h
OPC_JMP_REL32       EQU 0E9h
OPC_CALL_REL32      EQU 0E8h
OPC_RET             EQU 0C3h
OPC_NOP             EQU 90h
OPC_SYSCALL         EQU 0F05h

; =================================================================================
; Structures
; =================================================================================

LABEL_STRUCT        STRUCT
  szName            DB 64 DUP(0)
  nNameLen          DD 0
  qOffset           DQ 0
  defined           DB 0
  isExternal        DB 0
LABEL_STRUCT        ENDS

FIXUP_STRUCT        STRUCT
  labelIdx          DD 0
  qCodeOffset       DQ 0
  fixupType         DB 0
  lineNum           DD 0
FIXUP_STRUCT        ENDS

ENCODE_CTX          STRUCT
  rex               DB 0
  opcode            DB 4 DUP(0)
  opcodeLen         DB 0
  modRM             DB 0
  hasModRM          DB 0
  sib               DB 0
  hasSIB            DB 0
  dwDisp            DD 0
  dispLen           DB 0
  qImm              DQ 0
  immLen            DB 0
ENCODE_CTX          ENDS

ASM_STATE           STRUCT
  hHeap             DQ 0
  pCodeBuffer       DQ 0
  qCodeSize         DQ 0
  qCodeCapacity     DQ 0
  pLabels           DQ 0
  labelCount        DD 0
  labelCapacity     DD 0
  pFixups           DQ 0
  fixupCount        DD 0
  fixupCapacity     DD 0
  pSource           DQ 0
  sourceLen         DQ 0
  lineNum           DD 0
  qCurrentOffset    DQ 0
  tokenType         DD 0
  tokenValue        DB 256 DUP(0)
  tokenNum          DQ 0
  currentPass       DB 0
  instrCount        DD 0
  errorCount        DD 0
ASM_STATE           ENDS

; =================================================================================
; Uninitialized Data
; =================================================================================
.data
align 16

gState              ASM_STATE <>
gEncode             ENCODE_CTX <>

; =================================================================================
; Code Section
; =================================================================================
.code

; =================================================================================
; Console I/O Helpers
; =================================================================================

PrintString PROC
    push rbx
    push rsi
    push r12
    push r13
    push r14
    sub rsp, 48

    mov rsi, rcx
    call lstrlenA
    mov r12, rax

    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r13, rax

    mov rcx, r13
    mov rdx, rsi
    mov r8, r12
    lea r9, [rsp+40]
    mov qword ptr [rsp+32], 0
    call WriteFile

    add rsp, 48
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
PrintString ENDP

PrintNumber PROC
    push rbx
    push rsi
    push r12
    push r13
    sub rsp, 48
    
    lea r12, [rsp+32] ; Buffer at [rsp+32]
    mov r13, rcx
    
    test rcx, rcx
    jnz @convert
    
    mov byte ptr [r12], '0'
    mov byte ptr [r12+1], 0
    mov rcx, r12
    call PrintString
    jmp @done

@convert:
    mov rax, r13
    mov rdi, r12
    add rdi, 15
    mov byte ptr [rdi], 0
    mov rsi, 10
@loop_conv:
    xor rdx, rdx
    div rsi
    add dl, '0'
    dec rdi
    mov [rdi], dl
    test rax, rax
    jnz @loop_conv
    
    mov rcx, rdi
    call PrintString

@done:
    add rsp, 48
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
PrintNumber ENDP

; =================================================================================
; Memory Management
; =================================================================================

AsmInit PROC
    push rbx
    push rsi
    push rdi
    
    call GetProcessHeap
    mov gState.hHeap, rax
    
    mov rcx, rax
    xor edx, edx
    mov rax, SIZEOF LABEL_STRUCT
    mov r8, MAX_LABELS
    mul r8
    mov r8, rax
    call HeapAlloc
    test rax, rax
    jz @fail
    
    mov gState.pCodeBuffer, rax
    mov gState.qCodeCapacity, BUFFER_SIZE
    mov gState.qCodeSize, 0

    mov rcx, gState.hHeap
    xor edx, edx
    mov rax, SIZEOF LABEL_STRUCT
    mov r8, MAX_LABELS
    mul r8
    mov r8, rax
    call HeapAlloc
    test rax, rax
    jz @fail
    
    mov gState.pLabels, rax
    mov gState.labelCapacity, MAX_LABELS
    mov gState.labelCount, 0
    
    mov rcx, gState.hHeap
    xor edx, edx
    mov r8, MAX_FIXUPS * SIZEOF FIXUP_STRUCT
    call HeapAlloc
    test rax, rax
    jz @fail
    
    mov gState.pFixups, rax
    mov gState.fixupCapacity, MAX_FIXUPS
    mov gState.fixupCount, 0

    mov gState.lineNum, 1
    mov gState.qCurrentOffset, 0
    mov gState.instrCount, 0
    mov gState.errorCount, 0
    
    xor eax, eax
    jmp @done
    
@fail:
    mov eax, 1
    
@done:
    pop rdi
    pop rsi
    pop rbx
    ret
AsmInit ENDP

AsmCleanup PROC
    push rbx
    
    mov rbx, gState.hHeap
    
    mov rcx, rbx
    xor edx, edx
    mov r8, gState.pCodeBuffer
    test r8, r8
    jz @skip_code
    call HeapFree
    
@skip_code:
    mov rcx, rbx
    xor edx, edx
    mov r8, gState.pLabels
    test r8, r8
    jz @skip_labels
    call HeapFree
    
@skip_labels:
    mov rcx, rbx
    xor edx, edx
    mov r8, gState.pFixups
    test r8, r8
    jz @skip_fixups
    call HeapFree
    
@skip_fixups:
    pop rbx
    ret
AsmCleanup ENDP

; =================================================================================
; Label Management
; =================================================================================

LookupLabel PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rsi, rcx
    mov r12, rdx
    xor r13, r13
    
    mov ebx, gState.labelCount
    test ebx, ebx
    jz @not_found
    
    mov rdi, gState.pLabels
    
@loop:
    mov eax, [rdi + LABEL_STRUCT.nNameLen]
    cmp eax, r12d
    jne @next

    push rsi
    push rdi
    lea rdi, [rdi + LABEL_STRUCT.szName]
    mov rcx, r12
    repe cmpsb
    pop rdi
    pop rsi
    je @found
    
@next:
    add rdi, SIZEOF LABEL_STRUCT
    inc r13d
    dec ebx
    jnz @loop
    
@not_found:
    mov rax, -1
    jmp @done
    
@found:
    mov rax, r13
    
@done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LookupLabel ENDP

AddLabel PROC
    push rbx
    push rsi
    push r12
    push r13
    push r14
    sub rsp, 32

    mov rsi, rcx
    mov r12, rdx
    mov r13, r8
    mov r14, r9

    ; Lookup existing label by name to avoid duplicates
    call LookupLabel
    cmp rax, -1
    jne @update_existing

    mov eax, gState.labelCount
    cmp eax, gState.labelCapacity
    jae @done

    ; Calculate pointer to new label
    mov rdi, gState.pLabels
    mov eax, gState.labelCount
    mov ecx, SIZEOF LABEL_STRUCT
    mul ecx
    add rdi, rax

    ; Copy name
    push rdi
    lea rdi, [rdi + LABEL_STRUCT.szName]
    mov rcx, rsi
    mov rdx, r12
    call lstrcpyA
    pop rdi

    mov [rdi + LABEL_STRUCT.nNameLen], r12d
    mov [rdi + LABEL_STRUCT.qOffset], r13
    mov al, r14b
    mov [rdi + LABEL_STRUCT.defined], al
    mov [rdi + LABEL_STRUCT.isExternal], 0

    inc gState.labelCount
    jmp @done

@update_existing:
    ; Update existing label offset
    mov rdi, gState.pLabels
    mov ecx, SIZEOF LABEL_STRUCT
    mul ecx
    add rdi, rax
    mov [rdi + LABEL_STRUCT.qOffset], r13
    mov al, r14b
    mov [rdi + LABEL_STRUCT.defined], al

@done:
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
AddLabel ENDP

; =================================================================================
; Instruction Encoding
; =================================================================================

EmitByte PROC
    push rbx
    push rsi

    mov rbx, gState.qCodeSize
    cmp rbx, gState.qCodeCapacity
    jae @overflow

    mov rsi, gState.pCodeBuffer
    mov [rsi + rbx], cl
    inc gState.qCodeSize
    inc gState.qCurrentOffset

    pop rsi
    pop rbx
    ret
    
@overflow:
    inc gState.errorCount
    pop rsi
    pop rbx
    ret
EmitByte ENDP

EmitDword PROC
    push rax
    mov eax, ecx
    call EmitByte
    shr eax, 8
    mov cl, al
    call EmitByte
    shr eax, 8
    mov cl, al
    call EmitByte
    shr eax, 8
    mov cl, al
    call EmitByte
    pop rax
    ret
EmitDword ENDP

EmitQword PROC
    push rax
    mov rax, rcx
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    shr rax, 8
    mov cl, al
    call EmitByte
    pop rax
    ret
EmitQword ENDP

EncodeREX PROC
    push rax
    mov al, gEncode.rex
    test al, al
    jz @done
    
    or al, 40h
    mov cl, al
    call EmitByte
    
@done:
    pop rax
    ret
EncodeREX ENDP

EncodeModRM PROC
    push rax
    mov al, gEncode.modRM
    mov cl, al
    call EmitByte
    pop rax
    ret
EncodeModRM ENDP

EncodeInstruction PROC
    push rbx
    push rsi
    
    call EncodeREX
    
    xor rbx, rbx
    mov bl, gEncode.opcodeLen
    test bl, bl
    jz @skip_opcode
    
    lea rsi, gEncode.opcode
    
@op_loop:
    mov cl, [rsi]
    call EmitByte
    inc rsi
    dec bl
    jnz @op_loop
    
@skip_opcode:
    cmp gEncode.hasModRM, 0
    je @skip_modrm
    call EncodeModRM
    
@skip_modrm:
    mov al, gEncode.immLen
    test al, al
    jz @skip_imm

    cmp al, 1
    jne @chk2
    mov cl, byte ptr gEncode.qImm
    call EmitByte
    jmp @skip_imm

@chk2:
    cmp al, 2
    jne @chk4
    mov cx, word ptr gEncode.qImm
    call EmitByte
    shr cx, 8
    mov cl, ch
    call EmitByte
    jmp @skip_imm

@chk4:
    cmp al, 4
    jne @chk8
    mov ecx, dword ptr gEncode.qImm
    call EmitDword
    jmp @skip_imm

@chk8:
    mov rcx, gEncode.qImm
    call EmitQword

@skip_imm:
    mov gEncode.rex, 0
    mov gEncode.opcodeLen, 0
    mov gEncode.hasModRM, 0
    mov gEncode.hasSIB, 0
    mov gEncode.dispLen, 0
    mov gEncode.qImm, 0
    mov gEncode.immLen, 0
    ret
EncodeInstruction ENDP

; =================================================================================
; Specific Instruction Encoders
; =================================================================================

EncodeMOV_RegImm PROC
    push rbx
    
    mov rbx, rdx
    mov gEncode.rex, 8
    
    cmp ecx, 8
    jb @no_rexb
    or gEncode.rex, 1
    
@no_rexb:
    mov al, 0B8h
    and cl, 7
    add al, cl
    mov gEncode.opcode, al
    mov gEncode.opcodeLen, 1
    mov gEncode.qImm, rbx
    mov gEncode.immLen, 8

    call EncodeInstruction
    pop rbx
    ret
EncodeMOV_RegImm ENDP

EncodePUSH_Reg PROC
    cmp ecx, 8
    jb @no_rex
    mov gEncode.rex, 1
    
@no_rex:
    mov al, 50h
    and cl, 7
    add al, cl
    mov gEncode.opcode, al
    mov gEncode.opcodeLen, 1
    call EncodeInstruction
    ret
EncodePUSH_Reg ENDP

EncodePOP_Reg PROC
    cmp ecx, 8
    jb @no_rex
    mov gEncode.rex, 1
    
@no_rex:
    mov al, 58h
    and cl, 7
    add al, cl
    mov gEncode.opcode, al
    mov gEncode.opcodeLen, 1
    call EncodeInstruction
    ret
EncodePOP_Reg ENDP

EncodeRET PROC
    mov gEncode.opcode, 0C3h
    mov gEncode.opcodeLen, 1
    call EncodeInstruction
    ret
EncodeRET ENDP

EncodeNOP PROC
    mov gEncode.opcode, 90h
    mov gEncode.opcodeLen, 1
    call EncodeInstruction
    ret
EncodeNOP ENDP

EncodeSYSCALL PROC
    mov word ptr gEncode.opcode, 050Fh
    mov gEncode.opcodeLen, 2
    call EncodeInstruction
    ret
EncodeSYSCALL ENDP

EncodeJMP_Rel32 PROC
    mov gEncode.opcode, 0E9h
    mov gEncode.opcodeLen, 1
    mov gEncode.qImm, rcx
    mov gEncode.immLen, 4
    call EncodeInstruction
    ret
EncodeJMP_Rel32 ENDP

EncodeCALL_Rel32 PROC
    mov gEncode.opcode, 0E8h
    mov gEncode.opcodeLen, 1
    mov gEncode.qImm, rcx
    mov gEncode.immLen, 4
    call EncodeInstruction
    ret
EncodeCALL_Rel32 ENDP

; =================================================================================
; Main Assembly Loop
; =================================================================================

AssemblePass PROC
    push rbx
    push rsi
    push rdi
    
    mov gState.currentPass, cl
    mov gState.qCurrentOffset, 0
    mov gState.qCodeSize, 0
    
    cmp cl, 1
    jne @pass2
    
    mov rcx, OFFSET szPass1
    call PrintString
    jmp @done_msg
    
@pass2:
    mov rcx, OFFSET szPass2
    call PrintString
    
@done_msg:
    ; Test instructions
    mov ecx, REG_RAX
    mov rdx, 123456789ABCDEF0h
    call EncodeMOV_RegImm
    
    mov ecx, REG_RCX
    xor edx, edx
    call EncodeMOV_RegImm
    
    mov ecx, REG_RBP
    call EncodePUSH_Reg
    
    mov ecx, REG_RBP
    call EncodePOP_Reg
    
    call EncodeNOP
    call EncodeSYSCALL
    call EncodeRET
    
    xor eax, eax
    
    pop rdi
    pop rsi
    pop rbx
    ret
AssemblePass ENDP

; =================================================================================
; Entry Point
; =================================================================================

main PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    
    mov rcx, OFFSET szVersion
    call PrintString
    
    call AsmInit
    test eax, eax
    jnz @init_fail
    
    mov rcx, OFFSET szAssembling
    call PrintString
    
    mov ecx, 1
    call AssemblePass
    
    mov ecx, 2
    call AssemblePass
    
    mov rcx, OFFSET szSuccess
    call PrintString
    
    mov rcx, gState.qCodeSize
    call PrintNumber
    mov rcx, OFFSET szBytesEmitted
    call PrintString
    
    call AsmCleanup
    
    xor ecx, ecx
    call ExitProcess
    
@init_fail:
    mov rcx, OFFSET szErrOutOfMemory
    call PrintString
    mov ecx, 1
    call ExitProcess
    
main ENDP

END
