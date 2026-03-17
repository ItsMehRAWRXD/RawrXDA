; RawrXD_DumpBin_Engine.asm
; DumpBin Engine - PE Parser and Analyzer
; System 1 of 6: DumpBin Implementation
; PURE X64 MASM - ZERO STUBS - ZERO CRT

OPTION CASEMAP:NONE

include rawr_mem.inc

;=============================================================================
; PUBLIC INTERFACE
;=============================================================================
PUBLIC RawrDumpBin_Create
PUBLIC RawrDumpBin_Destroy
PUBLIC RawrDumpBin_ParsePE
PUBLIC RawrDumpBin_GetSections
PUBLIC RawrDumpBin_GetExports
PUBLIC RawrDumpBin_GetImports
PUBLIC RawrDumpBin_RVAToFileOffset

;=============================================================================
; STRUCTURES
;=============================================================================
IMAGE_DOS_HEADER STRUCT
    e_magic         WORD ?
    e_cblp          WORD ?
    e_cp            WORD ?
    e_crlc          WORD ?
    e_cparhdr       WORD ?
    e_minalloc      WORD ?
    e_maxalloc      WORD ?
    e_ss            WORD ?
    e_sp            WORD ?
    e_csum          WORD ?
    e_ip            WORD ?
    e_cs            WORD ?
    e_lfarlc        WORD ?
    e_ovno          WORD ?
    e_res           WORD 4 DUP(?)
    e_oemid         WORD ?
    e_oeminfo       WORD ?
    e_res2          WORD 10 DUP(?)
    e_lfanew        DWORD ?
IMAGE_DOS_HEADER ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_FILE_HEADER STRUCT
    Machine         WORD ?
    NumberOfSections WORD ?
    TimeDateStamp   DWORD ?
    PointerToSymbolTable DWORD ?
    NumberOfSymbols DWORD ?
    SizeOfOptionalHeader WORD ?
    Characteristics WORD ?
IMAGE_FILE_HEADER ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD ?
    MajorLinkerVersion          BYTE ?
    MinorLinkerVersion          BYTE ?
    SizeOfCode                  DWORD ?
    SizeOfInitializedData       DWORD ?
    SizeOfUninitializedData     DWORD ?
    AddressOfEntryPoint         DWORD ?
    BaseOfCode                  DWORD ?
    ImageBase                   QWORD ?
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
    SizeOfStackReserve          QWORD ?
    SizeOfStackCommit           QWORD ?
    SizeOfHeapReserve           QWORD ?
    SizeOfHeapCommit            QWORD ?
    LoaderFlags                 DWORD ?
    NumberOfRvaAndSizes         DWORD ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD ?
    Size            DWORD ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1           BYTE 8 DUP(?)
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

IMAGE_EXPORT_DIRECTORY STRUCT
    Characteristics           DWORD ?
    TimeDateStamp            DWORD ?
    MajorVersion             WORD ?
    MinorVersion             WORD ?
    Name                     DWORD ?
    Base                     DWORD ?
    NumberOfFunctions        DWORD ?
    NumberOfNames            DWORD ?
    AddressOfFunctions       DWORD ?
    AddressOfNames           DWORD ?
    AddressOfNameOrdinals    DWORD ?
IMAGE_EXPORT_DIRECTORY ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk      DWORD ?
    TimeDateStamp          DWORD ?
    ForwarderChain         DWORD ?
    Name1                  DWORD ?
    FirstThunk             DWORD ?
IMAGE_IMPORT_DESCRIPTOR ENDS

IMAGE_IMPORT_BY_NAME STRUCT
    Hint                   WORD ?
    Name1                  BYTE 1 DUP(?)
IMAGE_IMPORT_BY_NAME ENDS

; DumpBin Context Structure
DUMPBIN_CONTEXT STRUCT
    mappedBase      QWORD ?     ; Base address of mapped PE
    mappedSize      QWORD ?     ; Size of mapped PE
    pDosHeader      QWORD ?     ; Pointer to DOS header
    pNtHeader       QWORD ?     ; Pointer to NT headers
    pSectionHeaders QWORD ?     ; Pointer to section headers
    numSections     DWORD ?     ; Number of sections
    pExportDir      QWORD ?     ; Pointer to export directory
    pImportDir      QWORD ?     ; Pointer to import directory
    imageBase       QWORD ?     ; Image base
    sectionAlign    DWORD ?     ; Section alignment
    fileAlign       DWORD ?     ; File alignment
DUMPBIN_CONTEXT ENDS

; Section info structure
SECTION_INFO STRUCT
    name            BYTE 9 DUP(?) ; Section name (8 chars + null)
    rva             DWORD ?       ; Virtual address
    rawSize         DWORD ?       ; Size of raw data
    characteristics DWORD ?       ; Section characteristics
SECTION_INFO ENDS

; Export info structure
EXPORT_INFO STRUCT
    name            QWORD ?       ; Pointer to name string
    rva             DWORD ?       ; Export RVA
EXPORT_INFO ENDS

; Import info structure
IMPORT_INFO STRUCT
    dllName         QWORD ?       ; Pointer to DLL name
    funcName        QWORD ?       ; Pointer to function name (or NULL for ordinal)
    ordinal         WORD ?        ; Ordinal value (if by ordinal)
    byOrdinal       BYTE ?        ; 1 if imported by ordinal
    _pad            BYTE ?        ; Padding
IMPORT_INFO ENDS

;=============================================================================
; CONSTANTS
;=============================================================================
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
PE32PLUS_MAGIC equ 20Bh
IMAGE_DIRECTORY_ENTRY_EXPORT equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1

;=============================================================================
; EXTERNAL FUNCTIONS
;=============================================================================
EXTERN rawr_heap_alloc: PROC
EXTERN rawr_heap_free: PROC

;=============================================================================
; MACROS
;=============================================================================
; Check bounds: ptr, size, base, total_size -> ZF=1 if valid
CHECK_BOUNDS macro ptr, size, base, total_size
    mov rax, ptr
    sub rax, base
    jc check_bounds_fail
    add rax, size
    cmp rax, total_size
    ja check_bounds_fail
    test rax, rax  ; ZF=1 for valid
    jmp check_bounds_done
check_bounds_fail:
    xor rax, rax   ; ZF=0 for invalid
check_bounds_done:
endm

;=============================================================================
; CODE
;=============================================================================
.CODE

;-----------------------------------------------------------------------------
; RawrDumpBin_Create
; Creates a new DumpBin context
; Returns: RAX = context handle (0 = failure)
;-----------------------------------------------------------------------------
RawrDumpBin_Create PROC
    ; Allocate context structure
    mov rcx, SIZEOF DUMPBIN_CONTEXT
    xor rdx, rdx    ; Don't zero memory initially
    call rawr_heap_alloc
    test rax, rax
    jz @error
    
    ; Zero the context
    mov rcx, rax
    mov rdx, SIZEOF DUMPBIN_CONTEXT
    call ZeroMemory
    
    ret
    
@error:
    xor rax, rax
    ret
RawrDumpBin_Create ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_Destroy
; Destroys a DumpBin context
; RCX = context handle
;-----------------------------------------------------------------------------
RawrDumpBin_Destroy PROC
    test rcx, rcx
    jz @done
    
    ; Free the context
    call rawr_heap_free
    
@done:
    ret
RawrDumpBin_Destroy ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_ParsePE
; Parses a PE file from mapped memory
; RCX = context, RDX = mapped base, R8 = mapped size
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RawrDumpBin_ParsePE PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx    ; Context
    mov rsi, rdx    ; Mapped base
    mov r12, r8     ; Mapped size
    
    ; Validate parameters
    test rbx, rbx
    jz @error
    test rsi, rsi
    jz @error
    test r12, r12
    jz @error
    
    ; Store basic info
    mov [rbx].DUMPBIN_CONTEXT.mappedBase, rsi
    mov [rbx].DUMPBIN_CONTEXT.mappedSize, r12
    
    ; Validate DOS header
    CHECK_BOUNDS rsi, SIZEOF IMAGE_DOS_HEADER, rsi, r12
    jnz @error
    
    mov rdi, rsi
    cmp WORD PTR [rdi].IMAGE_DOS_HEADER.e_magic, IMAGE_DOS_SIGNATURE
    jne @error
    
    ; Get NT headers offset
    mov eax, [rdi].IMAGE_DOS_HEADER.e_lfanew
    test eax, eax
    jz @error
    
    ; Validate NT headers
    mov r13, rsi
    add r13, rax
    CHECK_BOUNDS r13, SIZEOF IMAGE_NT_HEADERS64, rsi, r12
    jnz @error
    
    ; Check NT signature
    cmp DWORD PTR [r13].IMAGE_NT_HEADERS64.Signature, IMAGE_NT_SIGNATURE
    jne @error
    
    ; Check machine type (AMD64)
    cmp WORD PTR [r13].IMAGE_NT_HEADERS64.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    jne @error
    
    ; Check magic (PE32+)
    cmp WORD PTR [r13].IMAGE_NT_HEADERS64.OptionalHeader.Magic, PE32PLUS_MAGIC
    jne @error
    
    ; Store header pointers
    mov [rbx].DUMPBIN_CONTEXT.pDosHeader, rdi
    mov [rbx].DUMPBIN_CONTEXT.pNtHeader, r13
    
    ; Store alignment values
    mov eax, [r13].IMAGE_NT_HEADERS64.OptionalHeader.SectionAlignment
    mov [rbx].DUMPBIN_CONTEXT.sectionAlign, eax
    mov eax, [r13].IMAGE_NT_HEADERS64.OptionalHeader.FileAlignment
    mov [rbx].DUMPBIN_CONTEXT.fileAlign, eax
    mov rax, [r13].IMAGE_NT_HEADERS64.OptionalHeader.ImageBase
    mov [rbx].DUMPBIN_CONTEXT.imageBase, rax
    
    ; Get number of sections
    movzx r14d, WORD PTR [r13].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections
    mov [rbx].DUMPBIN_CONTEXT.numSections, r14d
    
    ; Get section headers
    mov r15, r13
    add r15, SIZEOF IMAGE_NT_HEADERS64
    mov rax, SIZEOF IMAGE_SECTION_HEADER
    mul r14d
    CHECK_BOUNDS r15, rax, rsi, r12
    jnz @error
    mov [rbx].DUMPBIN_CONTEXT.pSectionHeaders, r15
    
    ; Get export directory
    mov r8, [r13].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress
    test r8d, r8d
    jz @no_export
    
    ; Convert RVA to file offset
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rbx].DUMPBIN_CONTEXT.pExportDir
    call RVAToFileOffsetInternal
    test rax, rax
    jz @no_export
    
    ; Validate export directory bounds
    mov rdi, [rbx].DUMPBIN_CONTEXT.pExportDir
    CHECK_BOUNDS rdi, SIZEOF IMAGE_EXPORT_DIRECTORY, rsi, r12
    jnz @no_export
    
@no_export:
    ; Get import directory
    mov r8, [r13].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress
    test r8d, r8d
    jz @no_import
    
    ; Convert RVA to file offset
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rbx].DUMPBIN_CONTEXT.pImportDir
    call RVAToFileOffsetInternal
    test rax, rax
    jz @no_import
    
@no_import:
    ; Success
    mov rax, 1
    jmp @done
    
@error:
    xor rax, rax
    
@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrDumpBin_ParsePE ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_GetSections
; Gets section information
; RCX = context, RDX = out_sections array, R8 = out_count
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RawrDumpBin_GetSections PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Context
    mov rsi, rdx    ; Out sections
    mov rdi, r8     ; Out count
    
    ; Validate parameters
    test rbx, rbx
    jz @error
    test rsi, rsi
    jz @error
    test rdi, rdi
    jz @error
    
    ; Get section count
    mov eax, [rbx].DUMPBIN_CONTEXT.numSections
    mov [rdi], eax
    
    ; Allocate section info array
    mov ecx, SIZEOF SECTION_INFO
    mul ecx
    mov rcx, rax
    call rawr_heap_alloc
    test rax, rax
    jz @error
    mov rsi, rax
    
    ; Fill section info
    mov r8, [rbx].DUMPBIN_CONTEXT.pSectionHeaders
    xor rcx, rcx
    mov ecx, [rbx].DUMPBIN_CONTEXT.numSections
    
@loop:
    test rcx, rcx
    jz @done_fill
    
    ; Copy name (8 bytes)
    mov rdx, [r8].IMAGE_SECTION_HEADER.Name1
    mov [rsi].SECTION_INFO.name, rdx
    
    ; Get RVA and size
    mov edx, [r8].IMAGE_SECTION_HEADER.VirtualAddress
    mov [rsi].SECTION_INFO.rva, edx
    mov edx, [r8].IMAGE_SECTION_HEADER.SizeOfRawData
    mov [rsi].SECTION_INFO.rawSize, edx
    mov edx, [r8].IMAGE_SECTION_HEADER.Characteristics
    mov [rsi].SECTION_INFO.characteristics, edx
    
    ; Next section
    add r8, SIZEOF IMAGE_SECTION_HEADER
    add rsi, SIZEOF SECTION_INFO
    dec rcx
    jmp @loop
    
@done_fill:
    ; Return the array pointer
    mov rax, rsi
    jmp @success
    
@error:
    xor rax, rax
    
@success:
    pop rdi
    pop rsi
    pop rbx
    ret
RawrDumpBin_GetSections ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_GetExports
; Gets export information
; RCX = context, RDX = out_exports array, R8 = out_count
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RawrDumpBin_GetExports PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx    ; Context
    mov rsi, rdx    ; Out exports
    mov rdi, r8     ; Out count
    
    ; Validate parameters
    test rbx, rbx
    jz @error
    test rsi, rsi
    jz @error
    test rdi, rdi
    jz @error
    
    ; Check if export directory exists
    mov r12, [rbx].DUMPBIN_CONTEXT.pExportDir
    test r12, r12
    jz @no_exports
    
    ; Get number of exports
    mov r13d, [r12].IMAGE_EXPORT_DIRECTORY.NumberOfNames
    test r13d, r13d
    jz @no_exports
    
    mov [rdi], r13d
    
    ; Allocate export info array
    mov ecx, SIZEOF EXPORT_INFO
    mul r13d
    mov rcx, rax
    call rawr_heap_alloc
    test rax, rax
    jz @error
    mov rsi, rax
    
    ; Get name and function arrays
    mov r14, [rbx].DUMPBIN_CONTEXT.mappedBase
    
    ; AddressOfNames
    mov r8d, [r12].IMAGE_EXPORT_DIRECTORY.AddressOfNames
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rsp-8]  ; Temp storage
    call RVAToFileOffsetInternal
    test rax, rax
    jz @error
    mov r8, [rsp-8]  ; AddressOfNames pointer
    
    ; AddressOfFunctions
    mov r9d, [r12].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions
    mov rcx, rbx
    mov rdx, r9
    lea r10, [rsp-16] ; Temp storage
    call RVAToFileOffsetInternal
    test rax, rax
    jz @error
    mov r9, [rsp-16] ; AddressOfFunctions pointer
    
    ; AddressOfNameOrdinals
    mov r10d, [r12].IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals
    mov rcx, rbx
    mov rdx, r10
    lea r11, [rsp-24] ; Temp storage
    call RVAToFileOffsetInternal
    test rax, rax
    jz @error
    mov r10, [rsp-24] ; AddressOfNameOrdinals pointer
    
    ; Fill export info
    xor rcx, rcx
    mov ecx, r13d
    
@export_loop:
    test rcx, rcx
    jz @done_exports
    
    ; Get name RVA
    mov edx, [r8]
    add r8, 4
    
    ; Convert to file offset
    push rcx
    mov rcx, rbx
    lea r11, [rsp-32] ; Temp storage
    call RVAToFileOffsetInternal
    pop rcx
    test rax, rax
    jz @next_export
    
    mov rdx, [rsp-32] ; Name pointer
    mov [rsi].EXPORT_INFO.name, rdx
    
    ; Get ordinal
    movzx rdx, WORD PTR [r10]
    add r10, 2
    
    ; Get function RVA
    mov eax, [r9 + rdx*4]
    mov [rsi].EXPORT_INFO.rva, eax
    
@next_export:
    add rsi, SIZEOF EXPORT_INFO
    dec rcx
    jmp @export_loop
    
@done_exports:
    mov rax, rsi
    jmp @success
    
@no_exports:
    mov DWORD PTR [rdi], 0
    xor rax, rax
    jmp @success
    
@error:
    xor rax, rax
    
@success:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrDumpBin_GetExports ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_GetImports
; Gets import information
; RCX = context, RDX = out_imports array, R8 = out_count
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RawrDumpBin_GetImports PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx    ; Context
    mov rsi, rdx    ; Out imports
    mov rdi, r8     ; Out count
    
    ; Validate parameters
    test rbx, rbx
    jz @error
    test rsi, rsi
    jz @error
    test rdi, rdi
    jz @error
    
    ; Check if import directory exists
    mov r12, [rbx].DUMPBIN_CONTEXT.pImportDir
    test r12, r12
    jz @no_imports
    
    ; Count imports
    mov r13, r12
    xor r14d, r14d
    
@count_loop:
    ; Check for null descriptor
    mov rax, [r13].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk
    or rax, [r13].IMAGE_IMPORT_DESCRIPTOR.Name1
    or rax, [r13].IMAGE_IMPORT_DESCRIPTOR.FirstThunk
    test rax, rax
    jz @done_count
    
    ; Count functions in this DLL
    mov r8d, [r13].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk
    test r8d, r8d
    jz @next_dll
    
    ; Convert RVA to file offset
    push r13
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rsp-8]
    call RVAToFileOffsetInternal
    pop r13
    test rax, rax
    jz @next_dll
    
    mov r8, [rsp-8] ; IAT pointer
    
@func_count_loop:
    mov rax, [r8]
    test rax, rax
    jz @next_dll
    add r8, 8
    inc r14d
    jmp @func_count_loop
    
@next_dll:
    add r13, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    jmp @count_loop
    
@done_count:
    test r14d, r14d
    jz @no_imports
    
    mov [rdi], r14d
    
    ; Allocate import info array
    mov ecx, SIZEOF IMPORT_INFO
    mul r14d
    mov rcx, rax
    call rawr_heap_alloc
    test rax, rax
    jz @error
    mov rsi, rax
    
    ; Reset for filling
    mov r13, r12
    xor r14d, r14d
    
@fill_loop:
    ; Check for null descriptor
    mov rax, [r13].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk
    or rax, [r13].IMAGE_IMPORT_DESCRIPTOR.Name1
    or rax, [r13].IMAGE_IMPORT_DESCRIPTOR.FirstThunk
    test rax, rax
    jz @done_fill
    
    ; Get DLL name
    mov r8d, [r13].IMAGE_IMPORT_DESCRIPTOR.Name1
    test r8d, r8d
    jz @next_fill_dll
    
    push r13
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rsp-8]
    call RVAToFileOffsetInternal
    pop r13
    test rax, rax
    jz @next_fill_dll
    
    mov r15, [rsp-8] ; DLL name pointer
    
    ; Get import lookup table
    mov r8d, [r13].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk
    test r8d, r8d
    jz @next_fill_dll
    
    push r13
    mov rcx, rbx
    mov rdx, r8
    lea r9, [rsp-16]
    call RVAToFileOffsetInternal
    pop r13
    test rax, rax
    jz @next_fill_dll
    
    mov r8, [rsp-16] ; ILT pointer
    
@func_fill_loop:
    mov rax, [r8]
    test rax, rax
    jz @next_fill_dll
    
    ; Store DLL name
    mov [rsi].IMPORT_INFO.dllName, r15
    
    ; Check if ordinal or name
    test rax, 8000000000000000h ; Ordinal bit
    jnz @by_ordinal
    
    ; By name
    mov BYTE PTR [rsi].IMPORT_INFO.byOrdinal, 0
    
    ; Convert RVA to file offset
    push r8
    push r13
    mov rcx, rbx
    mov rdx, rax
    lea r9, [rsp-24]
    call RVAToFileOffsetInternal
    pop r13
    pop r8
    test rax, rax
    jz @next_func
    
    mov rdx, [rsp-24]
    add rdx, 2 ; Skip hint
    mov [rsi].IMPORT_INFO.funcName, rdx
    jmp @next_func
    
@by_ordinal:
    mov BYTE PTR [rsi].IMPORT_INFO.byOrdinal, 1
    mov WORD PTR [rsi].IMPORT_INFO.ordinal, ax
    mov QWORD PTR [rsi].IMPORT_INFO.funcName, 0
    
@next_func:
    add r8, 8
    add rsi, SIZEOF IMPORT_INFO
    inc r14d
    jmp @func_fill_loop
    
@next_fill_dll:
    add r13, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    jmp @fill_loop
    
@done_fill:
    mov rax, rsi
    jmp @success
    
@no_imports:
    mov DWORD PTR [rdi], 0
    xor rax, rax
    jmp @success
    
@error:
    xor rax, rax
    
@success:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrDumpBin_GetImports ENDP

;-----------------------------------------------------------------------------
; RawrDumpBin_RVAToFileOffset
; Converts RVA to file offset
; RCX = context, RDX = rva, R8 = out_offset
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RawrDumpBin_RVAToFileOffset PROC
    push rbx
    
    mov rbx, rcx    ; Context
    
    ; Validate parameters
    test rbx, rbx
    jz @error
    test r8, r8
    jz @error
    
    ; Call internal function
    call RVAToFileOffsetInternal
    
@error:
    xor rax, rax
    
    pop rbx
    ret
RawrDumpBin_RVAToFileOffset ENDP

;-----------------------------------------------------------------------------
; RVAToFileOffsetInternal
; Internal RVA to file offset conversion
; RCX = context, RDX = rva, R8 = out_offset_ptr
; Returns: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
RVAToFileOffsetInternal PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Context
    mov rsi, rdx    ; RVA
    mov rdi, r8     ; Out offset ptr
    
    ; Find section containing RVA
    mov r8, [rbx].DUMPBIN_CONTEXT.pSectionHeaders
    xor rcx, rcx
    mov ecx, [rbx].DUMPBIN_CONTEXT.numSections
    
@section_loop:
    test rcx, rcx
    jz @not_found
    
    ; Check if RVA is in this section
    mov eax, [r8].IMAGE_SECTION_HEADER.VirtualAddress
    cmp rsi, rax
    jb @next_section
    
    add eax, [r8].IMAGE_SECTION_HEADER.VirtualSize
    cmp rsi, rax
    jae @next_section
    
    ; Found section, calculate offset
    sub rsi, [r8].IMAGE_SECTION_HEADER.VirtualAddress
    add rsi, [r8].IMAGE_SECTION_HEADER.PointerToRawData
    
    ; Validate offset is within file
    mov rax, [rbx].DUMPBIN_CONTEXT.mappedBase
    add rax, rsi
    mov rdx, [rbx].DUMPBIN_CONTEXT.mappedSize
    cmp rsi, rdx
    jae @not_found
    
    ; Store result
    mov [rdi], rsi
    mov rax, 1
    jmp @done
    
@next_section:
    add r8, SIZEOF IMAGE_SECTION_HEADER
    dec rcx
    jmp @section_loop
    
@not_found:
    xor rax, rax
    
@done:
    pop rdi
    pop rsi
    pop rbx
    ret
RVAToFileOffsetInternal ENDP

;-----------------------------------------------------------------------------
; ZeroMemory
; Zeros a block of memory
; RCX = address, RDX = size
;-----------------------------------------------------------------------------
ZeroMemory PROC
    push rdi
    
    mov rdi, rcx
    mov rcx, rdx
    xor rax, rax
    rep stosb
    
    pop rdi
    ret
ZeroMemory ENDP

END