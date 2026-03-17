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
