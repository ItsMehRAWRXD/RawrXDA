OPTION CASEMAP:NONE
public DllMain
public PeWriter_CreateMinimalExe
public PeWriter_Init
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
; r8: size (passed in r9)
Emitter_Init PROC
    mov [rcx], r8
    mov QWORD PTR [rcx+8], 0
    mov [rcx+16], r9
    mov DWORD PTR [rcx+24], 0 ; error
    ret
Emitter_Init ENDP

; Emitter_EmitByte
; rcx: EmitterContext*
; dl: byte
Emitter_EmitByte PROC
    mov rax, [rcx+8] ; position
    cmp rax, [rcx+16] ; size
    jae overflow
    mov rax, [rcx] ; buffer
    add rax, [rcx+8]
    mov [rax], dl
    inc QWORD PTR [rcx+8]
    ret
overflow:
    mov DWORD PTR [rcx+24], 1
    ret
Emitter_EmitByte ENDP

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
    mov r10d, [rbp+30h] ; ImportRVA (passed at [rsp+28h] in call, but [rbp+30h] here)
    mov DWORD PTR [rax+144], r10d ; DataDirectory[1].VirtualAddress
    mov r10d, [rbp+38h] ; ImportSize (passed at [rsp+30h] in call, but [rbp+38h] here)
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
    mov r11d, [rbp+30h] ; From stack parameter
    mov DWORD PTR [rax+16], r11d

    ; PointerToRawData
    mov r11d, [rbp+38h] ; From stack parameter
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
    mov r11d, [rbp+40h] ; From stack parameter
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
END
