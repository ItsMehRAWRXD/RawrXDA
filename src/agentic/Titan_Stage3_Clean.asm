; RawrXD Monolithic PE Emitter - Stage 3 (Imports)
; Full reconstruction for stability

IMAGE_DOS_HEADER STRUCT
    e_magic         WORD    ?
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
    e_lfanew        DWORD   ?
IMAGE_DOS_HEADER ENDS

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
    isize           DWORD   ?
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
    Signature           DWORD   ?
    FileHeader          IMAGE_FILE_HEADER <>
    OptionalHeader      IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

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

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    Name                DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

extern CreateFileA : proc
extern WriteFile  : proc
extern CloseHandle : proc
extern ExitProcess : proc

EmitterContext STRUCT
    pBuffer     QWORD   ?
    position    QWORD   ?
    isize       QWORD   ?
    error       DWORD   ?
EmitterContext ENDS

PeWriterContext STRUCT
    pBuffer     QWORD   ?
    position    QWORD   ?
    isize       QWORD   ?
    error       DWORD   ?
    emitter     EmitterContext <>
PeWriterContext ENDS

.data
g_PEBuffer      DB 1000000 DUP(?)
szKernel32      DB 'KERNEL32.dll', 0
szExitProcess   DB 'ExitProcess', 0
section_name_text  DB '.text', 0, 0, 0
section_name_idata DB '.idata', 0, 0, 0
absolute_output_path DB 'd:\rawrxd\bin\titan_verified.exe', 0
g_Ctx           PeWriterContext <>

.code

PeWriter_Init PROC
    mov [rcx], r8
    mov QWORD PTR [rcx+8], 0
    mov [rcx+16], r9
    push rcx
    lea rcx, [rcx+24]
    call Emitter_Init
    pop rcx
    ret
PeWriter_Init ENDP

Emitter_Init PROC
    mov [rcx], r8
    mov QWORD PTR [rcx+8], 0
    mov [rcx+16], r9
    mov DWORD PTR [rcx+24], 0
    ret
Emitter_Init ENDP

Emitter_EmitByte PROC
    mov rax, [rcx+8]
    cmp rax, [rcx+16]
    jae @ov
    mov r8, [rcx]
    add r8, rax
    mov [r8], dl
    inc QWORD PTR [rcx+8]
    ret
@ov: mov DWORD PTR [rcx+24], 1
    ret
Emitter_EmitByte ENDP

PeWriter_WriteDosHeader PROC
    mov rax, [rcx]
    mov rdx, [rcx+8]
    mov WORD PTR [rax+rdx], 5A4Dh
    mov DWORD PTR [rax+rdx+3Ch], 40h
    add QWORD PTR [rcx+8], 40h
    ret
PeWriter_WriteDosHeader ENDP

PeWriter_WriteNtHeaders PROC
    push rbp
    mov rbp, rsp
    mov rax, [rcx]
    mov r10, [rcx+8]
    lea r11, [rax+r10]
    
    mov DWORD PTR [r11], 00004550h ; 'PE\0\0'
    mov WORD PTR [r11+4], 8664h   ; x64
    mov WORD PTR [r11+6], 2       ; 2 sections
    mov WORD PTR [r11+20], 0F0h   ; OptionalHeader size
    mov WORD PTR [r11+22], 0022h  ; EXECUTABLE | LARGE_ADDRESS_AWARE

    mov WORD PTR [r11+24], 020Bh  ; PE32+
    mov DWORD PTR [r11+40], edx   ; EntryPoint
    mov QWORD PTR [r11+48], 400000h ; ImageBase
    mov DWORD PTR [r11+56], 1000h ; SectionAlignment
    mov DWORD PTR [r11+60], 200h  ; FileAlignment
    mov DWORD PTR [r11+64], 6     ; MajorOS (Win10+)
    mov DWORD PTR [r11+80], r8d   ; SizeOfImage
    mov DWORD PTR [r11+84], r9d   ; SizeOfHeaders
    mov WORD PTR [r11+92], 3      ; Subsystem (Console) - actually using 3 for console robustness

    mov DWORD PTR [r11+116], 16   ; NumberOfRvaAndSizes
    
    ; DataDirectory[1] - Import Table
    mov eax, [rbp+48] ; ImportRVA
    mov [r11+128], eax
    mov eax, [rbp+56] ; ImportSize
    mov [r11+132], eax

    add QWORD PTR [rcx+8], 40h + 18h + 0F0h ; Sig + FileHeader + OptionalHeader
    pop rbp
    ret
PeWriter_WriteNtHeaders ENDP

PeWriter_WriteSectionHeader PROC
    push rbp
    mov rbp, rsp
    push rdi ; Save RDI
    push rsi ; Save RSI
    
    mov rax, [rcx]
    mov r10, [rcx+8]
    lea r11, [rax+r10]
    
    mov rsi, rdx
    lea rdi, [r11]
    mov rcx, 8
    rep movsb
    
    mov eax, r8d
    mov [r11+8], eax ; VirtualSize
    mov eax, r9d
    mov [r11+12], eax ; VirtualAddress
    
    mov eax, [rbp+48] ; SizeOfRawData
    mov [r11+16], eax
    mov eax, [rbp+56] ; PointerToRawData
    mov [r11+20], eax
    mov eax, [rbp+64] ; Characteristics
    mov [r11+36], eax
    
    pop rsi ; Restore
    pop rdi
    pop rbp
    ret
PeWriter_WriteSectionHeader ENDP

PeWriter_AddImportTable PROC
    mov rax, [rcx]
    add rax, [rcx+8]
    
    ; Descriptor
    mov DWORD PTR [rax], 2028h      ; ILT RVA
    mov DWORD PTR [rax+12], 2050h   ; Name RVA
    mov DWORD PTR [rax+16], 2038h   ; IAT RVA
    
    ; ILT
    mov QWORD PTR [rax+40], 2060h   ; Hint/Name
    ; IAT
    mov QWORD PTR [rax+56], 2060h   ; Hint/Name
    
    ; Name "KERNEL32.dll"
    lea rdi, [rax+80]
    lea rsi, szKernel32
    mov rcx, 13
    rep movsb
    
    ; Hint/Name "ExitProcess"
    mov WORD PTR [rax+96], 0
    lea rdi, [rax+98]
    lea rsi, szExitProcess
    mov rcx, 12
    rep movsb
    
    add QWORD PTR [rcx+8], 110
    ret
PeWriter_AddImportTable ENDP

PeWriter_FinalizeToFile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    
    ; CreateFileA(rdx, ...) - RCX is already RDX from call
    mov rcx, [rbp+18h]
    mov rdx, 40000000h
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp+20h], 2
    mov QWORD PTR [rsp+28h], 80h
    mov QWORD PTR [rsp+30h], 0
    call CreateFileA
    
    cmp rax, -1
    je @f_err
    mov r12, rax
    
    mov rcx, r12
    mov rax, [rbp+10h]
    mov rdx, [rax]
    mov r8, [rax+8]
    lea r9, [rbp-8]
    mov QWORD PTR [rsp+20h], 0
    call WriteFile
    
    mov rcx, r12
    call CloseHandle
@f_err:
    add rsp, 40h
    pop rbp
    ret
PeWriter_FinalizeToFile ENDP

PeWriter_CreateMinimalExe PROC
    push rbp
    mov rbp, rsp
    sub rsp, 200h
    
    lea rcx, g_Ctx
    mov r8, OFFSET g_PEBuffer
    mov r9, 1000000
    call PeWriter_Init

    lea rcx, g_Ctx
    call PeWriter_WriteDosHeader

    lea rcx, g_Ctx
    mov rdx, 1000h
    mov r8, 3000h
    mov r9, 400h
    mov DWORD PTR [rsp+28h], 2000h
    mov DWORD PTR [rsp+30h], 110
    call PeWriter_WriteNtHeaders

    lea rcx, g_Ctx
    lea rdx, section_name_text
    mov r8, 200h
    mov r9, 1000h
    mov DWORD PTR [rsp+20h], 200h
    mov DWORD PTR [rsp+28h], 400h
    mov DWORD PTR [rsp+30h], 60000020h
    call PeWriter_WriteSectionHeader
    add QWORD PTR [g_Ctx+8], 28h

    lea rcx, g_Ctx
    lea rdx, section_name_idata
    mov r8, 110
    mov r9, 2000h
    mov DWORD PTR [rsp+20h], 200h
    mov DWORD PTR [rsp+28h], 600h
    mov DWORD PTR [rsp+30h], 0C0000040h
    call PeWriter_WriteSectionHeader
    add QWORD PTR [g_Ctx+8], 28h

    mov QWORD PTR [g_Ctx+8], 400h
    lea rcx, [g_Ctx.emitter]
    mov dl, 48h
    call Emitter_EmitByte
    mov dl, 83h
    call Emitter_EmitByte
    mov dl, 0ECh
    call Emitter_EmitByte
    mov dl, 28h ; 40 bytes
    call Emitter_EmitByte

    mov dl, 31h
    call Emitter_EmitByte
    mov dl, 0C9h
    call Emitter_EmitByte

    mov dl, 0FFh
    call Emitter_EmitByte
    mov dl, 15h
    call Emitter_EmitByte
    mov dl, 2Ch
    call Emitter_EmitByte
    mov dl, 10h
    call Emitter_EmitByte
    mov dl, 00h
    call Emitter_EmitByte
    mov dl, 00h
    call Emitter_EmitByte

    mov QWORD PTR [g_Ctx+8], 600h
    lea rcx, g_Ctx
    call PeWriter_AddImportTable

    lea rcx, g_Ctx
    lea rdx, absolute_output_path
    call PeWriter_FinalizeToFile

    leave
    ret
PeWriter_CreateMinimalExe ENDP

END
