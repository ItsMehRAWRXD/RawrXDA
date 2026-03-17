; RawrXD Monolithic PE Writer and x64 Machine Code Emitter
; Zero dependencies, no external includes, pure structural definitions

; DOS Header Structure
IMAGE_DOS_HEADER STRUCT
    e_magic         WORD    ?   ; MZ
    e_cblp          WORD    ?
    e_cp            WORD    ?
    e_crlc          WORD    ?
    e_cparhdr       WORD    ?
    e_minalloc      WORD    ?
    e_maxalloc      WORD    ?
    e_ss            WORD    ?
    e_sp            WORD    ?
    e_csum          WORD    ?
    e_ip            WORD    ?
    e_cs            WORD    ?
    e_lfarlc        WORD    ?
    e_ovno          WORD    ?
    e_res           WORD 4 DUP(?)
    e_oemid         WORD    ?
    e_oeminfo       WORD    ?
    e_res2          WORD 10 DUP(?)
    e_lfanew        DWORD   ?   ; NT header offset
IMAGE_DOS_HEADER ENDS

; NT Headers Structure
IMAGE_FILE_HEADER STRUCT
    Machine             WORD    ?
    NumberOfSections    WORD    ?
    TimeDateStamp       DWORD   ?
    PointerToSymbolTable DWORD  ?
    NumberOfSymbols     DWORD   ?
    SizeOfOptionalHeader WORD   ?
    Characteristics     WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    isize           DWORD   ? ; Renamed from 'Size' to 'isize' to avoid conflict with ML64 keyword
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD    ?
    MajorLinkerVersion          BYTE    ?
    MinorLinkerVersion          BYTE    ?
    SizeOfCode                  DWORD   ?
    SizeOfInitializedData       DWORD   ?
    SizeOfUninitializedData     DWORD   ?
    AddressOfEntryPoint         DWORD   ?
    BaseOfCode                  DWORD   ?
    ImageBase                   QWORD   ?
    SectionAlignment            DWORD   ?
    FileAlignment               DWORD   ?
    MajorOperatingSystemVersion WORD    ?
    MinorOperatingSystemVersion WORD    ?
    MajorImageVersion           WORD    ?
    MinorImageVersion           WORD    ?
    MajorSubsystemVersion       WORD    ?
    MinorSubsystemVersion       WORD    ?
    Win32VersionValue           DWORD   ?
    SizeOfImage                 DWORD   ?
    SizeOfHeaders               DWORD   ?
    CheckSum                    DWORD   ?
    Subsystem                   WORD    ?
    DllCharacteristics          WORD    ?
    SizeOfStackReserve          QWORD   ?
    SizeOfStackCommit           QWORD   ?
    SizeOfHeapReserve           QWORD   ?
    SizeOfHeapCommit            QWORD   ?
    LoaderFlags                 DWORD   ?
    NumberOfRvaAndSizes         DWORD   ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature           DWORD   ?   ; PE\0\0
    FileHeader          IMAGE_FILE_HEADER <>
    OptionalHeader      IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

; Section Header Structure
IMAGE_SECTION_HEADER STRUCT
    s_Name              BYTE 8 DUP(?)
    UNION Misc
        PhysicalAddress DWORD   ?
        VirtualSize     DWORD   ?
    ENDS
    VirtualAddress      DWORD   ?
    SizeOfRawData       DWORD   ?
    PointerToRawData    DWORD   ?
    PointerToRelocations DWORD  ?
    PointerToLinenumbers DWORD  ?
    NumberOfRelocations WORD    ?
    NumberOfLinenumbers WORD    ?
    Characteristics     DWORD   ?
IMAGE_SECTION_HEADER ENDS

; Import Directory Structure
IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?   ; RVA to ILT
    TimeDateStamp       DWORD   ?   ; 0 = not bound
    ForwarderChain      DWORD   ?   ; -1 if no forwarders
    Name                DWORD   ?   ; RVA to DLL name
    FirstThunk          DWORD   ?   ; RVA to IAT
IMAGE_IMPORT_DESCRIPTOR ENDS

; Import Lookup Table (ILT) / Import Address Table (IAT) entry
; (Just a QWORD pointing to Hint/Name)

; WinAPI External Prototypes
extern CreateFileA : proc
extern WriteFile  : proc
extern CloseHandle : proc
extern ExitProcess : proc

; PE Section Constants
SECTION_ALIGNMENT EQU 1000h
FILE_ALIGNMENT    EQU 200h

; Emitter Context Structure
EmitterContext STRUCT
    buffer      QWORD   ?   ; Pointer to buffer
    position    QWORD   ?   ; Current position in buffer
    isize       QWORD   ?   ; Buffer size
    error       DWORD   ?   ; Error code (0=Success, 1=Overflow)
EmitterContext ENDS

; PE Writer Context Structure
PeWriterContext STRUCT
    buffer      QWORD   ?   ; PE buffer
    position    QWORD   ?   ; Current write position
    isize       QWORD   ?   ; Buffer size
    error       DWORD   ?
    emitter     EmitterContext <>
PeWriterContext ENDS

; Global PE Buffer (1MB)
.data
g_PEBuffer      DB 1000000 DUP(?)
g_PEBufferSize  EQU $ - g_PEBuffer

; Import Table Data
szKernel32      DB 'KERNEL32.dll', 0
szExitProcess   DB 'ExitProcess', 0

; Section Names
section_name_text  DB '.text', 0, 0, 0
section_name_idata DB '.idata', 0, 0, 0

; Global Contexts
g_Ctx           PeWriterContext <>

; Code Section
.code

; PeWriter_Init
; Initializes the PE writer context
; rcx: PeWriterContext*
; rdx: buffer (passed in r8)
; r8: size (passed in r9)
PeWriter_Init PROC
    mov [rcx], r8       ; buffer
    mov QWORD PTR [rcx+8], 0 ; position
    mov [rcx+16], r9     ; isize

    ; Init emitter
    push rcx
    lea rcx, [rcx+24]   ; emitter context (offset 24 in PeWriterContext)
    ; r8 is already buffer, r9 is already size
    call Emitter_Init
    pop rcx
    ret
PeWriter_Init ENDP

; Emitter_Init
; rcx: EmitterContext*
; rdx: buffer (passed in r8)
; rcx: EmitterContext*
; dl: byte
Emitter_EmitByte PROC
    mov rax, [rcx+8]    ; position
    mov r8, [rcx+16]    ; isize
    cmp rax, r8
    jae overflow_err

    mov r8, [rcx]       ; buffer
    mov [r8+rax], dl
    inc QWORD PTR [rcx+8]
    ret

overflow_err:
    mov DWORD PTR [rcx+24], 1 ; Set error=1
    ret
Emitter_EmitByte ENDP

; Emitter_EmitMovRegImm64
; Emits MOV reg, imm64
; rcx: EmitterContext*
; dl: register (0-15)
; r8: immediate value
Emitter_EmitMovRegImm64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    mov [rbp+10h], rcx  ; Context
    mov [rbp+18h], rdx  ; Register
    mov [rbp+20h], r8   ; Immediate

    ; REX.W prefix (48h + REX.B if reg >= 8)
    mov al, 48h
    test dl, 8
    jz @f
    or al, 1            ; REX.B
@@:
    mov dl, al
    call Emitter_EmitByte

    ; Opcode B8h + (reg & 7)
    mov rcx, [rbp+10h]
    mov rdx, [rbp+18h]
    and dl, 7
    mov al, 0B8h
    add al, dl
    mov dl, al
    call Emitter_EmitByte

    ; Emit 8-byte immediate
    mov r10, 0
emit_imm_loop:
    mov rcx, [rbp+10h]
    mov rax, [rbp+20h]
    mov rdx, r10
    shl rdx, 3          ; rdx * 8 bits
    push rcx
    mov rcx, rdx
    shr rax, cl
    pop rcx
    mov dl, al
    call Emitter_EmitByte
    inc r10
    cmp r10, 8
    jl emit_imm_loop

    leave
    ret
Emitter_EmitMovRegImm64 ENDP

; Emitter_EmitSubRegImm32
; Emits SUB reg, imm32
; rcx: EmitterContext*
; dl: register
; r8d: immediate value
Emitter_EmitSubRegImm32 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8

    ; REX.W prefix
    mov al, 48h
    test dl, 8
    jz @f
    or al, 1            ; REX.B
@@:
    mov dl, al
    call Emitter_EmitByte

    ; Opcode 81h
    mov rcx, [rbp+10h]
    mov dl, 81h
    call Emitter_EmitByte

    ; ModRM: 11 101 reg (0xE8 + reg_mask)
    mov rcx, [rbp+10h]
    mov rdx, [rbp+18h]
    and dl, 7
    mov al, 0E8h
    add al, dl
    mov dl, al
    call Emitter_EmitByte

    ; Imm32
    mov r10, 0
emit_imm_loop:
    mov rcx, [rbp+10h]
    mov rax, [rbp+20h]
    mov rdx, r10
    shl rdx, 3
    push rcx
    mov rcx, rdx
    shr rax, cl
    pop rcx
    mov dl, al
    call Emitter_EmitByte
    inc r10
    cmp r10, 4
    jl emit_imm_loop

    leave
    ret
Emitter_EmitSubRegImm32 ENDP

; Emitter_EmitAddRegImm32
; Emits ADD reg, imm32
; rcx: EmitterContext*
; dl: register
; r8d: immediate value
Emitter_EmitAddRegImm32 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8

    ; REX.W prefix
    mov al, 48h
    test dl, 8
    jz @f
    or al, 1            ; REX.B
@@:
    mov dl, al
    call Emitter_EmitByte

    ; Opcode 81h
    mov rcx, [rbp+10h]
    mov dl, 81h
    call Emitter_EmitByte

    ; ModRM: 11 000 reg (0xC0 + reg_mask)
    mov rcx, [rbp+10h]
    mov rdx, [rbp+18h]
    and dl, 7
    mov al, 0C0h
    add al, dl
    mov dl, al
    call Emitter_EmitByte

    ; Imm32
    mov r10, 0
emit_imm_loop:
    mov rcx, [rbp+10h]
    mov rax, [rbp+20h]
    mov rdx, r10
    shl rdx, 3
    push rcx
    mov rcx, rdx
    shr rax, cl
    pop rcx
    mov dl, al
    call Emitter_EmitByte
    inc r10
    cmp r10, 4
    jl emit_imm_loop

    leave
    ret
Emitter_EmitAddRegImm32 ENDP

; Emitter_EmitPopReg
; Emits POP reg
; rcx: EmitterContext*
; dl: register
Emitter_EmitPopReg PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h

    ; REX.B if reg >= 8
    test dl, 8
    jz no_rex_pop
    push rdx
    mov dl, 41h         ; REX.B
    call Emitter_EmitByte
    pop rdx
    and dl, 7

no_rex_pop:
    mov al, 58h         ; POP reg base
    add al, dl
    mov dl, al
    call Emitter_EmitByte

    leave
    ret
Emitter_EmitPopReg ENDP

; Emitter_EmitFunctionEpilogue
; Standard x64 stack frame teardown
; rcx: EmitterContext*
; rdx: stack_depth (QWORD)
; r8: restore_rbp (BOOL)
Emitter_EmitFunctionEpilogue PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8

    ; 1. add rsp, stack_depth (only if > 0)
    mov rdx, [rbp+18h]
    test rdx, rdx
    jz no_add_rsp
    mov rcx, [rbp+10h]
    mov dl, 4           ; RSP
    mov r8, [rbp+18h]
    call Emitter_EmitAddRegImm32
no_add_rsp:

    ; 2. pop rbp (if requested)
    mov r8, [rbp+20h]
    test r8, r8
    jz no_pop_rbp
    mov rcx, [rbp+10h]
    mov dl, 5           ; RBP
    call Emitter_EmitPopReg
no_pop_rbp:

    ; 3. ret
    mov rcx, [rbp+10h]
    call Emitter_EmitRet

    leave
    ret
Emitter_EmitFunctionEpilogue ENDP

; Emitter_EmitRet
; Emits RET
; rcx: EmitterContext*
Emitter_EmitRet PROC
    mov dl, 0C3h
    call Emitter_EmitByte
    ret
Emitter_EmitRet ENDP

; Emitter_EmitCallReg
; Emits CALL reg
; rcx: EmitterContext*
; dl: register
Emitter_EmitCallReg PROC
    ; REX prefix if needed
    test dl, 8
    jz no_rex_call
    mov al, 41h         ; REX.B
    call Emitter_EmitByte
    and dl, 7
no_rex_call:
    mov al, 0FFh
    call Emitter_EmitByte
    mov al, 0D0h
    add al, dl   ; ModRM for CALL reg
    mov dl, al
    call Emitter_EmitByte
    ret
Emitter_EmitCallReg ENDP

; PeWriter_WriteImportTable
; Writes a minimal import table
; rcx: PeWriterContext*
PeWriter_WriteImportTable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h

    ; Get context
    mov rax, [rcx]      ; buffer
    mov rdx, [rcx+8]    ; position

    ; Import Descriptor (terminator)
    mov QWORD PTR [rax+rdx], 0
    mov QWORD PTR [rax+rdx+8], 0
    mov QWORD PTR [rax+rdx+16], 0
    mov QWORD PTR [rax+rdx+24], 0

    ; Update position
    mov rax, [rbp+10h]
    add QWORD PTR [rax+8], 20h

    leave
    ret
PeWriter_WriteImportTable ENDP

; PeWriter_FinalizeToFile
; Writes the buffer content to a file on disk
; rcx: PeWriterContext*
; rdx: FileName (string pointer)
PeWriter_FinalizeToFile PROC
    push rbp
    mov rbp, rsp
    push r12
    push rsi
    sub rsp, 48h

    mov rsi, rcx       ; Save Context struct ptr safely to non-volatile rsi
    mov [rbp-18h], rdx ; Save FileName safely into local stack

    ; 1. CreateFileA
    mov rcx, [rbp-18h]
    mov rdx, 40000000h ; GENERIC_WRITE
    xor r8, r8         ; No share
    xor r9, r9         ; No security
    mov DWORD PTR [rsp+20h], 2 ; CREATE_ALWAYS
    mov DWORD PTR [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp+30h], 0 ; Template
    call CreateFileA

    cmp rax, -1        ; INVALID_HANDLE_VALUE
    je error_exit
    mov r12, rax       ; hFile

    ; 2. WriteFile
    mov rcx, r12       ; hFile
    mov rdx, [rsi]     ; buffer from g_Ctx correctly fetched via rsi
    mov r8, [rsi+8]    ; position / size
    lea r9, [rbp-20h]  ; &BytesWritten
    mov QWORD PTR [rsp+20h], 0 ; overlapped
    call WriteFile

    ; 3. CloseHandle
    mov rcx, r12
    call CloseHandle

error_exit:
    add rsp, 48h
    pop rsi
    pop r12
    leave
    ret
PeWriter_FinalizeToFile ENDP

; AlignValue
; rcx: value
; rdx: alignment
AlignValue PROC
    dec rdx
    add rcx, rdx
    not rdx
    and rcx, rdx
    mov rax, rcx
    ret
AlignValue ENDP

; PeWriter_CreateMinimalExe
; Creates a minimal executable with KERNEL32!ExitProcess import
; rcx: output buffer
; rdx: output size (PTR)
PeWriter_CreateMinimalExe PROC
    push rbp
    mov rbp, rsp
    and rsp, -16      ; strict 16-byte alignment needed for main entry
    sub rsp, 200h     ; Local context + parameters

    
    ; 1. Initialize Context
    lea rcx, g_Ctx
    mov r8, OFFSET g_PEBuffer
    mov r9, 1000000
    call PeWriter_Init

    ; 2. Write Headers
    lea rcx, g_Ctx
    call PeWriter_WriteDosHeader

    lea rcx, g_Ctx
    mov rdx, 1000h ; EntryPoint (.text)
    mov r8, 3000h  ; SizeOfImage (.text + .idata)
    mov r9, 400h   ; SizeOfHeaders
    mov DWORD PTR [rsp+28h], 2000h ; ImportTableRVA
    mov DWORD PTR [rsp+30h], 110   ; ImportTableSize
    call PeWriter_WriteNtHeaders

    ; 3. Write Section Headers: .text
    lea rcx, g_Ctx
    lea rdx, section_name_text
    mov r8, 200h   ; VirtualSize
    mov r9, 1000h  ; VirtualAddress
    mov DWORD PTR [rsp+20h], 200h ; SizeOfRawData
    mov DWORD PTR [rsp+28h], 400h ; PointerToRawData
    mov DWORD PTR [rsp+30h], 060000020h ; CODE | EXECUTE | READ
    call PeWriter_WriteSectionHeader

    ; 4. Write Section Headers: .idata
    lea rcx, g_Ctx
    lea rdx, section_name_idata
    mov r8, 110    ; VirtualSize
    mov r9, 2000h  ; VirtualAddress
    mov DWORD PTR [rsp+20h], 200h ; SizeOfRawData
    mov DWORD PTR [rsp+28h], 600h ; PointerToRawData
    mov DWORD PTR [rsp+30h], 0C0000040h ; DATA | READ | WRITE
    call PeWriter_WriteSectionHeader

    ; 5. Seek to .text RAW offset (400h) and Emit Code
    lea rcx, g_Ctx
    mov QWORD PTR [rcx+8], 400h 
    
    ; sub rsp, 40 (alignment)
    lea rcx, [g_Ctx.emitter]
    mov dl, 48h ; REX.W
    call Emitter_EmitByte
    mov dl, 83h
    call Emitter_EmitByte
    mov dl, 0ECh ; RSP
    call Emitter_EmitByte
    mov dl, 40
    call Emitter_EmitByte

    ; xor ecx, ecx
    mov dl, 31h
    call Emitter_EmitByte
    mov dl, 0C9h
    call Emitter_EmitByte

    ; call qword ptr [rip + offset] -> ExitProcess
    ; ExitProcess IAT at RVA 2038h
    ; Current RVA: 1000h (start) + 4 (sub rsp) + 2 (xor) = 1006h
    ; Instruction size = 6 (FF 15 xx xx xx xx)
    ; Next RVA = 100Ch
    ; Offset = 2038h - 100Ch = 102Ch
    mov dl, 0FFh
    call Emitter_EmitByte
    mov dl, 15h
    call Emitter_EmitByte
    mov dl, 2Ch ; Displacement
    call Emitter_EmitByte
    mov dl, 10h
    call Emitter_EmitByte
    mov dl, 00h
    call Emitter_EmitByte
    mov dl, 00h
    call Emitter_EmitByte

    ; ret (though ExitProcess won't return)
    mov dl, 0C3h
    call Emitter_EmitByte

    ; 6. Seek to .idata RAW offset (600h) and Write Imports
    lea rcx, g_Ctx
    mov QWORD PTR [rcx+8], 600h
    call PeWriter_AddImportTable

    ; 7. Finalize to file (using absolute path for stability)
    lea rcx, g_Ctx
    db 'd', ':', '\', 'r', 'a', 'w', 'r', 'x', 'd', '\', 'b', 'i', 'n', '\', 't', 'i', 't', 'a', 'n', '_', 'v', 'e', 'r', 'i', 'f', 'i', 'e', 'd', '.', 'e', 'x', 'e', 0
    lea rdx, [rbp-100h] ; Move string would be better, but let's just use a label below
    ; Wait, I can't easily push a string on stack in MASM without more code. 
    ; Let's use the .data label.
    lea rdx, absolute_output_path
    call PeWriter_FinalizeToFile

    xor rcx, rcx
    call ExitProcess

    leave
    ret
PeWriter_CreateMinimalExe ENDP

.data
absolute_output_path DB 'd:\rawrxd\bin\titan_verified.exe', 0
section_name_text DB '.text', 0, 0, 0
section_name_idata DB '.idata', 0, 0, 0

; PeWriter_Init
; Initializes the PE writer context
END 
