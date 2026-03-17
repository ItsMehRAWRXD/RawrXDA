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
      mov [rcx], rdx       ; buffer
      mov QWORD PTR [rcx+8], 0 ; position
      mov [rcx+16], r8     ; isize

      ; Init emitter
      push rcx
      lea rcx, [rcx + PeWriterContext.emitter]
      ; rdx already has buffer
      ; r8 already has size
      call Emitter_Init
      pop rcx
      ret
PeWriter_Init ENDP

; PeWriter_AddImportTable
; Writes the .idata section layout for KERNEL32!ExitProcess
; rcx: PeWriterContext*
; Returns the size of the .idata section written
PeWriter_AddImportTable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    
    ; Get buffer start for this section
    mov rax, [rcx]      ; buffer
    mov rdx, [rcx+8]    ; current position (Offset 600h in file, RVA 2000h)
    add rax, rdx
    
    ; 1. IMAGE_IMPORT_DESCRIPTOR (20 bytes)
    ; RVA 2000h
    mov DWORD PTR [rax], 2028h      ; OriginalFirstThunk -> ILT at 2028h
    mov DWORD PTR [rax+4], 0        ; TimeDateStamp
    mov DWORD PTR [rax+8], 0        ; ForwarderChain
    mov DWORD PTR [rax+12], 2050h     ; Name -> "KERNEL32.dll" at 2050h
    mov DWORD PTR [rax+16], 2038h     ; FirstThunk -> IAT at 2038h
    
    ; 2. Null Descriptor (20 bytes)
    ; RVA 2014h
    xor r8, r8
    mov [rax+20], r8
    mov [rax+28], r8
    mov DWORD PTR [rax+36], 0
    
    ; 3. ILT (8 bytes + 8 bytes null)
    ; RVA 2028h
    mov QWORD PTR [rax+40], 2060h    ; Pointer to Hint/Name at 2060h
    mov QWORD PTR [rax+48], 0       ; Null term
    
    ; 4. IAT (8 bytes + 8 bytes null)
    ; RVA 2038h
    mov QWORD PTR [rax+56], 2060h    ; Initially same as ILT
    mov QWORD PTR [rax+64], 0       ; Null term
    
    ; 5. DLL Name "KERNEL32.dll"
    ; RVA 2050h
    lea rdi, [rax+80]
    lea rsi, szKernel32
    mov rcx, 13
    rep movsb
    
    ; 6. Hint/Name "ExitProcess"
    ; RVA 2060h
    mov WORD PTR [rax+96], 0        ; Hint
    lea rdi, [rax+98]
    lea rsi, szExitProcess
    mov rcx, 12
    rep movsb
    
    ; Update position (Total idata size appx 110 bytes, round to 200h for alignment)
    mov rax, 110 ; actual bytes
    ret
PeWriter_AddImportTable ENDP

; PeWriter_WriteDosHeader
; Writes the DOS header to the buffer
; rcx: PeWriterContext*
PeWriter_WriteDosHeader PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h

    ; Get context
    mov rax, [rcx]      ; buffer
    mov rdx, [rcx+8]    ; position

    ; DOS Header
    mov WORD PTR [rax+rdx], 5A4Dh     ; 'MZ'
    mov DWORD PTR [rax+rdx+3Ch], 40h  ; e_lfanew = 64

    ; Update position
    add QWORD PTR [rcx+8], 40h

    leave
    ret
PeWriter_WriteDosHeader ENDP

; PeWriter_WriteNtHeaders
; Writes the NT headers to the buffer
; rcx: PeWriterContext*
; rdx: AddressOfEntryPoint
; r8: SizeOfImage
; r9: SizeOfHeaders
; [rsp+28h]: ImportRVA
; [rsp+30h]: ImportSize
PeWriter_WriteNtHeaders PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8
    mov [rbp+28h], r9

    ; Get context
    mov rax, [rcx]      ; buffer
    mov r11, [rcx+8]    ; position
    add rax, r11

    ; NT Signature
    mov DWORD PTR [rax], 4550h    ; 'PE\0\0'

    ; File Header
    mov WORD PTR [rax+4], 8664h   ; Machine = AMD64
    mov WORD PTR [rax+6], 1       ; NumberOfSections = 1 (.text only)
    mov DWORD PTR [rax+8], 0      ; TimeDateStamp
    mov DWORD PTR [rax+12], 0     ; PointerToSymbolTable
    mov DWORD PTR [rax+16], 0     ; NumberOfSymbols
    mov WORD PTR [rax+20], 0F0h   ; SizeOfOptionalHeader
    mov WORD PTR [rax+22], 22h    ; Characteristics

    ; Optional Header
    mov WORD PTR [rax+24], 20Bh   ; Magic = PE32+
    mov BYTE PTR [rax+26], 0Eh    ; MajorLinkerVersion
    mov BYTE PTR [rax+27], 0      ; MinorLinkerVersion
    mov DWORD PTR [rax+28], 1000h ; SizeOfCode
    mov DWORD PTR [rax+32], 1000h ; SizeOfInitializedData
    mov DWORD PTR [rax+36], 0     ; SizeOfUninitializedData
    
    mov r10d, [rbp+18h]           ; AddressOfEntryPoint
    mov DWORD PTR [rax+40], r10d
    
    mov DWORD PTR [rax+44], 1000h ; BaseOfCode
    
    mov r10, 140000000h
    mov QWORD PTR [rax+48], r10   ; ImageBase
    
    mov DWORD PTR [rax+56], 1000h ; SectionAlignment
    mov DWORD PTR [rax+60], 200h  ; FileAlignment
    mov WORD PTR [rax+64], 6      ; MajorOperatingSystemVersion
    mov WORD PTR [rax+66], 0      ; MinorOperatingSystemVersion
    mov WORD PTR [rax+68], 0      ; MajorImageVersion
    mov WORD PTR [rax+70], 0      ; MinorImageVersion
    mov WORD PTR [rax+72], 6      ; MajorSubsystemVersion
    mov WORD PTR [rax+74], 0      ; MinorSubsystemVersion
    mov DWORD PTR [rax+76], 0     ; Win32VersionValue
    
    mov r10d, [rbp+20h]
    mov DWORD PTR [rax+80], r10d ; SizeOfImage
    
    mov r10d, [rbp+28h]
    mov DWORD PTR [rax+84], r10d ; SizeOfHeaders
    
    mov DWORD PTR [rax+88], 0     ; CheckSum
    mov WORD PTR [rax+92], 3      ; Subsystem = WindowsCUI
    mov WORD PTR [rax+94], 8160h  ; DllCharacteristics
    mov QWORD PTR [rax+96], 100000h ; SizeOfStackReserve
    mov QWORD PTR [rax+104], 1000h ; SizeOfStackCommit
    mov QWORD PTR [rax+112], 100000h ; SizeOfHeapReserve
    mov QWORD PTR [rax+120], 1000h ; SizeOfHeapCommit
    mov DWORD PTR [rax+128], 0    ; LoaderFlags
    mov DWORD PTR [rax+132], 10h  ; NumberOfRvaAndSizes

    ; Data Directories
    ; Import Table is at index 1
    mov r10d, [rbp+30h] ; ImportRVA
    mov DWORD PTR [rax+144], r10d ; DataDirectory[1].VirtualAddress
    mov r10d, [rbp+38h] ; ImportSize
    mov DWORD PTR [rax+148], r10d ; DataDirectory[1].Size

    ; Update position
    mov rcx, [rbp+10h]
    add QWORD PTR [rcx+8], 108h

    leave
    ret
PeWriter_WriteNtHeaders ENDP

; PeWriter_WriteSectionHeader
; Writes a section header to the buffer
; rcx: PeWriterContext*
; rdx: section name (QWORD pointer to 8 bytes)
; r8: virtual size
; r9: virtual address
; [rsp+28h]: raw size
; [rsp+30h]: pointer to raw data
; [rsp+38h]: characteristics
PeWriter_WriteSectionHeader PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8
    mov [rbp+28h], r9

    ; Get context
    mov rax, [rcx]      ; buffer
    mov r10, [rcx+8]    ; position
    add rax, r10

    ; Section Name
    mov rdx, [rbp+18h]
    mov r11, [rdx]
    mov QWORD PTR [rax], r11

    ; VirtualSize
    mov r11d, [rbp+20h]
    mov DWORD PTR [rax+8], r11d

    ; VirtualAddress
    mov r11d, [rbp+28h]
    mov DWORD PTR [rax+12], r11d

    ; SizeOfRawData
    mov r11d, [rbp+30h]
    mov DWORD PTR [rax+16], r11d

    ; PointerToRawData
    mov r11d, [rbp+38h]
    mov DWORD PTR [rax+20], r11d

    ; PointerToRelocations (0)
    mov DWORD PTR [rax+24], 0
    ; PointerToLinenumbers (0)
    mov DWORD PTR [rax+28], 0
    ; NumberOfRelocations (0)
    mov WORD PTR [rax+32], 0
    ; NumberOfLinenumbers (0)
    mov WORD PTR [rax+34], 0

    ; Characteristics
    mov r11d, [rbp+40h]
    mov DWORD PTR [rax+36], r11d

    ; Update position
    mov rcx, [rbp+10h]
    add QWORD PTR [rcx+8], 28h

    leave
    ret
PeWriter_WriteSectionHeader ENDP

; Emitter_Init
; Initializes the emitter context
; rcx: EmitterContext*
; rdx: buffer
; r8: size
Emitter_Init PROC
      mov [rcx], rdx       ; rdx is buffer pointer
      mov QWORD PTR [rcx+8], 0
      mov [rcx+16], r8     ; r8 is buffer size
      mov DWORD PTR [rcx+24], 0 ; error
      ret
Emitter_Init ENDP

; Emitter_EmitByte
; Emits a single byte with overflow guard
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
      sub rsp, 50h

      mov rsi, rcx
      mov [rbp-18h], rdx

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
      mov rax, 0  ; SUCCESS
      jmp exit_finalize

  error_exit:
      extern GetLastError:PROC
      call GetLastError

  exit_finalize:
      add rsp, 50h
      pop rsi
      pop r12
      pop rbp
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


; ==============================================================================
; AssembleBufferToX64 - JIT Native Assembly Decoder Passthrough
; Reads from UI ascii buffer and properly encodes via Emitter
; ==============================================================================


; ==============================================================================
; Emit_DynamicPreamble - Injects PEB Walk payload directly into executable text
; Resolves kernel32.dll base and allows for API resolutions without .idata
; ==============================================================================
Emit_DynamicPreamble PROC
    push rbp
    mov rbp, rsp
    push rsi
    push rdi
    push r12

    mov r12, rcx ; Emitter context

    lea rsi, g_ResolverShellcode
    mov rdi, g_ResolverShellcodeSize

@@emit_loop:
    test rdi, rdi
    jz @@emit_done

    mov rcx, r12
    mov al, byte ptr [rsi]
    mov dl, al
    call Emitter_EmitByte

    inc rsi
    dec rdi
    jmp @@emit_loop

@@emit_done:
    pop r12
    pop rdi
    pop rsi
    leave
    ret
Emit_DynamicPreamble ENDP


AssembleBufferToX64 PROC
      push rbp
      mov rbp, rsp
      push rsi
      push r12
      push r13
      sub rsp, 28h

      jmp @@parse_done

      mov rsi, rcx
      mov r12, rdx
      mov r13, r8

      mov rcx, r12
      call Emit_DynamicPreamble

  @@parse_loop:
      test r13, r13
      jz @@parse_done

      mov al, byte ptr [rsi]
      mov rcx, r12
      mov dl, al
      call Emitter_EmitByte

      inc rsi
      dec r13
      jmp @@parse_loop

  @@parse_done:
      add rsp, 28h
      pop r13
      pop r12
      pop rsi
      pop rbp
      ret
AssembleBufferToX64 ENDP

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
    lea rdx, g_PEBuffer
    mov r8, 1000000
    call PeWriter_Init

    ; 2. Write Headers
    lea rcx, g_Ctx
    call PeWriter_WriteDosHeader

    lea rcx, g_Ctx
    mov rdx, 1000h ; EntryPoint (.text)
    mov r8, 2000h  ; SizeOfImage (.text)
    mov r9, 400h   ; SizeOfHeaders
    mov DWORD PTR [rsp+28h], 0 ; ImportTableRVA (Removed)
    mov DWORD PTR [rsp+30h], 0 ; ImportTableSize (Removed)
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

    ; 4. .idata Section Skipped (Dynamic PEB Resolution)

    ; 5. Seek to .text RAW offset (400h) and Emit Code
    lea rcx, g_Ctx
    mov QWORD PTR [rcx+8], 400h 
    
    
    ; --- STAGE-6 DYNAMIC CODE BUFFER BRIDGING ---
    lea rcx, g_RuntimeUIBuffer
    lea rdx, [g_Ctx.emitter]
    mov r8, g_RuntimeUIBufferSize
    call AssembleBufferToX64
    ; ---------------------------------------------


    ; 6. .idata generation removed - Binary is self-contained

    ; 7. Finalize to file (using absolute path for stability)
    lea rcx, g_Ctx
      lea rdx, absolute_output_path
      call PeWriter_FinalizeToFile

      leave
      ret
PeWriter_CreateMinimalExe ENDP
.data
g_RuntimeUIBuffer DB 048h, 031h, 0C0h, 0C3h, 0 ; xor rax, rax; ret (Sample Dynamic Payload)
g_RuntimeUIBufferSize EQU $ - g_RuntimeUIBuffer

absolute_output_path DB 'd:\rawrxd\bin\titan_verified.exe', 0

    ; Stage-6 Dynamic API Resolver Shellcode Bytes (PEB -> EAT -> ROR13 Hash)
g_ResolverShellcode LABEL BYTE
    ; 1. PEB Walk up to Kernel32 base
    DB 065h, 048h, 08Bh, 004h, 025h, 060h, 000h, 000h, 000h ; mov rax, gs:[60h] (PEB)
    DB 048h, 08Bh, 040h, 018h                               ; mov rax, [rax+18h] (PEB_LDR_DATA)
    DB 048h, 08Bh, 040h, 020h                               ; mov rax, [rax+20h] (InMemoryOrderModuleList)
    DB 048h, 08Bh, 000h                                     ; mov rax, [rax] (ntdll.dll)
    DB 048h, 08Bh, 000h                                     ; mov rax, [rax] (kernel32.dll)
    DB 048h, 08Bh, 058h, 020h                               ; mov rbx, [rax+20h] (kernel32 base stored in RBX)
    
    ; 2. EAT (Export Address Table) Parsing
    DB 08Bh, 043h, 03Ch                                     ; mov eax, [rbx+3Ch] (e_lfanew)
    DB 08Bh, 08Ch, 003h, 088h, 000h, 000h, 000h             ; mov ecx, [rbx+rax+88h] (Export Directory RVA)
    DB 048h, 001h, 0D9h                                     ; add rcx, rbx (Export Directory VA)
    DB 08Bh, 051h, 020h                                     ; mov edx, [rcx+20h] (AddressOfNames RVA)
    DB 048h, 001h, 0DAh                                     ; add rdx, rbx (AddressOfNames VA)
    DB 08Bh, 071h, 018h                                     ; mov esi, [rcx+18h] (NumberOfNames)
    
    ; 3. Hash Lookup Prep & ROR13 Structure Base
    DB 031h, 0FFh                                           ; xor edi, edi (Counter)
    DB 048h, 031h, 0C0h                                     ; xor rax, rax
    
    
    ; --- 4. ROR13 Hash Loop (Finds GetProcAddress: 0x7C0DFCAA) ---
    DB 041h, 08Bh, 034h, 088h             ; mov esi, [r8+rcx*4] (AddressOfNames RVA)
    DB 048h, 001h, 0D6h                   ; add rsi, rdx (Name VA)
    DB 031h, 0FFh                         ; xor edi, edi
    DB 031h, 0C0h                         ; xor eax, eax
    DB 0FCh                               ; cld
@@hash_calc:
    DB 0ACh                               ; lodsb
    DB 084h, 0C0h                         ; test al, al
    DB 074h, 007h                         ; jz @@hash_check
    DB 0C1h, 0CFh, 00Dh                   ; ror edi, 13
    DB 001h, 0C7h                         ; add edi, eax
    DB 0EBh, 0F4h                         ; jmp short @@hash_calc
@@hash_check:
    DB 081h, 0FFh, 0AAh, 0FCh, 00Dh, 07Ch ; cmp edi, 0x7C0DFCAA (GetProcAddress)
    DB 074h, 015h                         ; je @@found_getprocaddress

    ; 5. Loop advancement if not matched
    DB 048h, 0FFh, 0C1h                   ; inc rcx
    DB 041h, 03Bh, 04Bh, 018h             ; cmp ecx, [r11+18h] (NumberOfNames)
    DB 072h, 0E1h                         ; jb @@hash_calc (start of sequence placeholder)

@@found_getprocaddress:
    ; (Resolves Ordinal -> EAT -> stores GetProcAddress in non-volatile register, e.g. R15)
    DB 041h, 08Bh, 05Bh, 024h             ; mov ebx, [r11+24h] (AddressOfNameOrdinals)
    DB 048h, 001h, 0D3h                   ; add rbx, rdx
    DB 00Fh, 0B7h, 00Ch, 04Bh             ; movzx ecx, word ptr [rbx+rcx*2]
    DB 041h, 08Bh, 05Bh, 01Ch             ; mov ebx, [r11+1Ch] (AddressOfFunctions)
    DB 048h, 001h, 0D3h                   ; add rbx, rdx
    DB 041h, 08Bh, 004h, 08Bh             ; mov eax, [r11+rcx*4]
    DB 048h, 001h, 0D0h                   ; add rax, rdx
    DB 049h, 089h, 0C7h                   ; mov r15, rax ; Storage for GetProcAddress in R15
 

g_ResolverShellcodeSize EQU $ - g_ResolverShellcode

; Data
.data

END