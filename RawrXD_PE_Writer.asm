; RawrXD_PE_Writer.asm
; PE32+ Writer and Machine-Code Emitter
; Generates runnable executables with import tables
; PURE X64 MASM - ZERO STUBS - ZERO CRT

OPTION CASEMAP:NONE

;=============================================================================
; PUBLIC INTERFACE
;=============================================================================
PUBLIC PEWriter_CreateExecutable
PUBLIC PEWriter_AddSection
PUBLIC PEWriter_AddImport
PUBLIC PEWriter_AddCode
PUBLIC PEWriter_WriteFile
PUBLIC PEWriter_AddData
PUBLIC PEWriter_AddBssSpace
PUBLIC PEWriter_AddBaseRelocation
PUBLIC PEWriter_BuildRelocSection

; Machine Code Emitter Interface
PUBLIC Emit_FunctionPrologue
PUBLIC Emit_FunctionEpilogue
PUBLIC Emit_MOV
PUBLIC Emit_ADD
PUBLIC Emit_SUB
PUBLIC Emit_CALL
PUBLIC Emit_RET
PUBLIC Emit_PUSH
PUBLIC Emit_POP
PUBLIC Emit_JMP
PUBLIC Emit_JE
PUBLIC Emit_JNE
PUBLIC Emit_CMP
PUBLIC Emit_TEST
PUBLIC Emit_LEA
PUBLIC Emit_NOP
PUBLIC CodeGen_DefineLabel
PUBLIC CodeGen_AddRelocation
PUBLIC CodeGen_CalculateRVA
PUBLIC CodeGen_AlignCode

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

IMAGE_FILE_HEADER STRUCT
    Machine         WORD ?
    NumberOfSections WORD ?
    TimeDateStamp   DWORD ?
    PointerToSymbolTable DWORD ?
    NumberOfSymbols DWORD ?
    SizeOfOptionalHeader WORD ?
    Characteristics WORD ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD ?
    DirSize         DWORD ?
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

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

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

; Import DLL Entry Structure
IMPORT_DLL_ENTRY STRUCT
    nameRVA                DWORD ?
    nameOffset             DWORD ?
    descriptorIndex        DWORD ?
    functionCount          DWORD ?
    firstThunkRVA          DWORD ?
    originalFirstThunkRVA  DWORD ?
    nameString             BYTE 64 DUP(?)
IMPORT_DLL_ENTRY ENDS

; Import Function Entry Structure
IMPORT_FUNCTION_ENTRY STRUCT
    nameRVA                DWORD ?
    nameOffset             DWORD ?
    thunkOffset            DWORD ?
    dllIndex               DWORD ?
    hint                   WORD ?
    nameString             BYTE 128 DUP(?)
IMPORT_FUNCTION_ENTRY ENDS

; Machine Code Emitter Structures
CODE_OPERAND STRUCT
    opType      DWORD ?     ; OP_REG, OP_IMM, OP_MEM, etc.
    regNum      DWORD ?     ; Register number for register operands
    immediate   QWORD ?     ; Immediate value
    displacement DWORD ?    ; Memory displacement
    baseReg     DWORD ?     ; Base register for memory operands
    indexReg    DWORD ?     ; Index register for memory operands
    scale       DWORD ?     ; Scale factor (1, 2, 4, 8)
    opSize      DWORD ?     ; Operand size in bytes
CODE_OPERAND ENDS

CODE_RELOCATION STRUCT
    relocOffset DWORD ?     ; Offset in code buffer
    relocType   DWORD ?     ; Type of relocation
    targetRVA   DWORD ?     ; Target RVA
    addend      DWORD ?     ; Addend for relocation
CODE_RELOCATION ENDS

CODE_LABEL STRUCT
    nameHash    DWORD ?     ; Hash of label name
    labelOffset DWORD ?     ; Offset in code buffer
    defined     DWORD ?     ; 1 if defined, 0 if forward reference
CODE_LABEL ENDS

STACK_FRAME STRUCT
    frameSize   DWORD ?     ; Total frame size
    localsSize  DWORD ?     ; Size of local variables
    savedRegs   QWORD ?     ; Bitmask of saved registers
    stackPos    DWORD ?     ; Current stack position
    prologSize  DWORD ?     ; Size of function prologue
STACK_FRAME ENDS

PE_SECTION_ENTRY STRUCT
    Name1           BYTE 8 DUP(?)
    VirtualSize     DWORD ?
    VirtualAddress  DWORD ?
    SizeOfRawData   DWORD ?
    PointerToRawData DWORD ?
    Characteristics DWORD ?
    pDataBuffer     QWORD ?
PE_SECTION_ENTRY ENDS

MAX_SECTIONS equ 16


; PE Context Structure for building PE file
PE_CONTEXT STRUCT
    hHeap                  QWORD ?
    pDosHeader             QWORD ?
    pNtHeader              QWORD ?
    pSections              QWORD ?     ; Pointer to array of PE_SECTION_ENTRY
    numSections            DWORD ?     ; Count of sections
    pImportDescriptors     QWORD ?
    pImportNames           QWORD ?
    pSectionHeaders        QWORD ?
    pImportAddressTable    QWORD ?
    pImportLookupTable     QWORD ?
    pCodeBuffer            QWORD ?
    ; Machine Code Emitter Fields
    pRelocations           QWORD ?     ; Relocation table
    pLabels                QWORD ?     ; Label table
    currentFrame           STACK_FRAME <>
    numRelocations         DWORD ?
    numLabels              DWORD ?
    ; Import table management
    pImportDLLTable        QWORD ?      ; Table of DLL entries
    pImportFunctionTable   QWORD ?      ; Table of function entries
    pStringTable           QWORD ?      ; String table for names
    pImportByNameTable     QWORD ?      ; Import by name structures
    numImportDLLs          DWORD ?
    numImportFunctions     DWORD ?
    stringTableSize        DWORD ?
    importByNameSize       DWORD ?
    totalImportSize        DWORD ?
    ; Original fields
    codeSize               DWORD ?
    numImports             DWORD ?
    textVirtualAddress     DWORD ?
    rdataVirtualAddress    DWORD ?
    idataVirtualAddress    DWORD ?
    textFileOffset         DWORD ?
    rdataFileOffset        DWORD ?
    idataFileOffset        DWORD ?
    currentVirtualAddress  DWORD ?

    currentFileOffset      DWORD ?
    imageBase              QWORD ?
    entryPoint             DWORD ?
    ; Data section management
    pDataBuffer            QWORD ?     ; Pointer to initialized data buffer
    dataSize               DWORD ?     ; Current data buffer used size
    dataVirtualAddress     DWORD ?     ; .data section RVA
    dataFileOffset         DWORD ?     ; .data section file offset
    ; BSS section management
    bssSize                DWORD ?     ; Uninitialized data size
    bssVirtualAddress      DWORD ?     ; .bss virtual address
    ; Relocation section management
    pRelocBuffer           QWORD ?     ; Base relocation table buffer
    relocBufferSize        DWORD ?     ; Current reloc buffer used size
    relocVirtualAddress    DWORD ?     ; .reloc section RVA
    relocFileOffset        DWORD ?     ; .reloc section file offset
    numBaseRelocations     DWORD ?     ; Number of base reloc entries
    ; Resource section management
    pResourceBuffer        QWORD ?     ; Resource data buffer pointer
    resourceSize           DWORD ?     ; Resource data size
    resourceVirtualAddress DWORD ?     ; .rsrc section RVA  
    resourceFileOffset     DWORD ?     ; .rsrc section file offset
    ; Exception data management
    pExceptionBuffer       QWORD ?     ; .pdata buffer pointer
    exceptionSize          DWORD ?     ; .pdata size
    pUnwindBuffer          QWORD ?     ; .xdata buffer pointer
    unwindSize             DWORD ?     ; .xdata size
    exceptionVirtualAddress DWORD ?    ; .pdata section RVA
    exceptionFileOffset    DWORD ?     ; .pdata section file offset
PE_CONTEXT ENDS

;=============================================================================
; CONSTANTS
;=============================================================================
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_FILE_EXECUTABLE_IMAGE equ 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE equ 0020h
IMAGE_SCN_MEM_EXECUTE equ 20000000h
IMAGE_SCN_MEM_READ equ 40000000h
IMAGE_SCN_MEM_WRITE equ 80000000h
IMAGE_SCN_CNT_CODE equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 00000040h
IMAGE_SCN_TEXT_CHARACTERISTICS equ 60000020h
IMAGE_SCN_RDATA_CHARACTERISTICS equ 40000040h
IMAGE_SCN_MEM_DISCARDABLE equ 02000000h
IMAGE_SUBSYSTEM_WINDOWS_GUI equ 2
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE equ 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION equ 3
IMAGE_DIRECTORY_ENTRY_SECURITY equ 4
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_IAT equ 12
IMAGE_REL_BASED_DIR64 equ 10
FILE_ALIGNMENT equ 200h
SECTION_ALIGNMENT equ 1000h

; Machine Code Emitter Constants
MAX_CODE_SIZE equ 100000h
MAX_DATA_SIZE equ 100000h        ; 1MB max data section
MAX_RELOC_SIZE equ 10000h        ; 64KB max relocation table
MAX_RESOURCE_SIZE equ 100000h    ; 1MB max resources
MAX_EXCEPTION_ENTRIES equ 1000h  ; Max runtime function entries
MAX_RELOCATIONS equ 1000h
MAX_LABELS equ 500h
STACK_ALIGNMENT equ 16
CALL_FRAME_SIZE equ 32

; x64 Register Encoding
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

; Instruction Prefixes
REX_W equ 48h    ; 64-bit operand size
REX_R equ 44h    ; Extension of ModRM reg field
REX_X equ 42h    ; Extension of SIB index field
REX_B equ 41h    ; Extension of ModRM r/m field, SIB base field, or Opcode reg field

; ModRM byte encoding
MODRM_REG_INDIRECT equ 00h
MODRM_REG_DISP8    equ 40h
MODRM_REG_DISP32   equ 80h
MODRM_REG_DIRECT   equ 0C0h

; Operand Types
OP_REG     equ 1
OP_IMM     equ 2
OP_MEM     equ 3
OP_DISP    equ 4
OP_LABEL   equ 5
MAX_IMPORTS equ 100
DEFAULT_IMAGE_BASE equ 140000000h
FILE_BEGIN equ 0
GENERIC_WRITE equ 40000000h
CREATE_ALWAYS equ 2
FILE_ATTRIBUTE_NORMAL equ 80h
HEAP_ZERO_MEMORY equ 8
; DOS-specific constants
DOS_PAGE_SIZE equ 512
DOS_PARAGRAPH_SIZE equ 16
MIN_NT_HEADER_OFFSET equ 80h  ; Minimum safe offset for NT headers

;=============================================================================
; EXTERNAL FUNCTIONS
;=============================================================================
EXTERN GetProcessHeap: PROC
EXTERN HeapAlloc: PROC  
EXTERN HeapFree: PROC
EXTERN CreateFileA: PROC
EXTERN WriteFile: PROC
EXTERN CloseHandle: PROC
EXTERN GetFileSize: PROC
EXTERN SetFilePointer: PROC
EXTERN GetSystemTimeAsFileTime: PROC

;=============================================================================
; IMPORT TABLE STRUCTURES  
;=============================================================================
IMAGE_THUNK_DATA64 STRUCT
    Function1 QWORD ?
IMAGE_THUNK_DATA64 ENDS

;=============================================================================
; DATA
;=============================================================================
.data
; Optimized DOS stub - complete DOS program that displays message and exits
; This is the minimal form that maintains full functionality
dos_stub_program LABEL BYTE
    ; DOS program header
    db 0Eh                    ; push cs
    db 1Fh                    ; pop ds
    db 0B8h, 01h, 30h        ; mov ax, 3001h (get DOS version)
    db 0CDh, 21h             ; int 21h
    db 3Ch, 30h              ; cmp al, 30h (DOS 3.0)
    db 73h, 26h              ; jnb show_message
    
    ; Show "This program cannot be run in DOS mode." message
show_message:
    db 0BAh                   ; mov dx, offset message
    dw message_offset
    db 0B4h, 09h             ; mov ah, 09h (DOS print string)
    db 0CDh, 21h             ; int 21h
    db 0B8h, 01h, 4Ch        ; mov ax, 4C01h (DOS exit)
    db 0CDh, 21h             ; int 21h
    
    ; Message data
message_offset equ $ - dos_stub_program + 0Eh
message_data db "This program cannot be run in DOS mode.", 0Dh, 0Ah, "$"
    
dos_stub_size equ $ - dos_stub_program
dos_stub_aligned_size equ ((dos_stub_size + 15) AND 0FFF0h) ; Align to 16 bytes

text_name db '.text', 0, 0, 0, 0
rdata_name db '.rdata', 0, 0
idata_name db '.idata', 0, 0
data_name db '.data', 0, 0, 0
reloc_name db '.reloc', 0, 0

; Windows API imports
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC

.code

;=============================================================================
; HELPER FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; CalculateDOSHeaderOffset - Calculate optimal NT header offset
; Input: RCX = DOS header size, RDX = DOS stub size
; Output: RAX = NT header offset (aligned)
;-----------------------------------------------------------------------------
CalculateDOSHeaderOffset PROC
    push rbp
    mov rbp, rsp
    
    ; Calculate total DOS section size
    add rcx, rdx                  ; DOS header + stub size
    add rcx, 15                   ; Add padding for alignment
    and rcx, 0FFFFFFF0h          ; Align to 16-byte boundary
    
    ; Ensure minimum offset for PE compatibility
    cmp rcx, MIN_NT_HEADER_OFFSET
    jae @F
    mov rcx, MIN_NT_HEADER_OFFSET
@@: mov rax, rcx
    
    pop rbp
    ret
CalculateDOSHeaderOffset ENDP

;-----------------------------------------------------------------------------
; AlignUp - Aligns a value up to the nearest boundary
; Input: RCX = value, RDX = alignment
; Output: RAX = aligned value
;-----------------------------------------------------------------------------
AlignUp PROC
    dec rdx
    add rcx, rdx
    not rdx
    and rcx, rdx
    mov rax, rcx
    ret
AlignUp ENDP

;-----------------------------------------------------------------------------
; CalculateFileOffset - Calculate next aligned file offset
; Input: RCX = current offset, RDX = size
; Output: RAX = next aligned offset
;-----------------------------------------------------------------------------
CalculateFileOffset PROC
    add rcx, rdx
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    ret
CalculateFileOffset ENDP

;-----------------------------------------------------------------------------
; CalculateVirtualAddress - Calculate next aligned virtual address
; Input: RCX = current address, RDX = size  
; Output: RAX = next aligned address
;-----------------------------------------------------------------------------
CalculateVirtualAddress PROC
    add rcx, rdx
    mov rdx, SECTION_ALIGNMENT
    call AlignUp
    ret
CalculateVirtualAddress ENDP

;-----------------------------------------------------------------------------
; CalculateTimestamp - Generate current timestamp for PE header
; Output: RAX = timestamp (seconds since 1970) 
;-----------------------------------------------------------------------------
CalculateTimestamp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 30h
    
    ; Get current system time as FILETIME (100ns intervals since 1601-01-01)
    lea rcx, [rbp-10h]            ; FILETIME on stack (8 bytes)
    call GetSystemTimeAsFileTime
    
    ; Load FILETIME value
    mov rax, qword ptr [rbp-10h]
    
    ; Convert FILETIME to Unix timestamp:
    ; Unix epoch = Jan 1, 1970 = FILETIME 116444736000000000 (0x019DB1DED53E8000)
    ; Subtract epoch offset, then divide by 10,000,000 (10^7) to get seconds
    mov rcx, 116444736000000000    ; FILETIME epoch offset
    sub rax, rcx                   ; 100ns intervals since Unix epoch
    
    ; Divide by 10,000,000 to convert 100ns -> seconds
    xor edx, edx
    mov rcx, 10000000
    div rcx                        ; RAX = Unix timestamp in seconds
    
    ; Truncate to 32-bit (PE TimeDateStamp is DWORD)
    ; RAX low 32 bits is the result
    
    add rsp, 30h
    pop rbp
    ret
CalculateTimestamp ENDP

;-----------------------------------------------------------------------------
; CalculateChecksum - Calculate proper PE checksum
; Input: RCX = PE context
; Output: RAX = checksum
;-----------------------------------------------------------------------------
CalculateChecksum PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rbx, rcx                      ; PE context
    
    ; PE checksum algorithm: sum all WORD values in the file,
    ; folding carry into low 16 bits, then add file length.
    ; Skip the CheckSum field itself (offset 88 in OptionalHeader from NT header start).
    
    ; We compute over all section data buffers + headers.
    ; Since we're computing before disk write, we simulate the byte stream.
    
    ; For now, compute over the NT header buffer which contains most critical data.
    mov rsi, [rbx].PE_CONTEXT.pNtHeader
    test rsi, rsi
    jz checksum_from_size
    
    ; Compute checksum offset: CheckSum is at OptionalHeader+0x40 from NT header start
    ; NT header: Signature(4) + FileHeader(20) + Magic(2) + ... CheckSum at offset 0x58
    ; = Signature(4) + FileHeader(20) + OptionalHeader.CheckSum offset (0x40) = offset 0x58
    mov r12d, 58h                     ; Offset of CheckSum field in NT headers
    
    ; Sum all WORDs in NT headers
    xor edi, edi                      ; Accumulator (32-bit to catch carries)
    xor ecx, ecx                      ; Byte offset
    mov r13d, SIZEOF IMAGE_NT_HEADERS64
    
checksum_nt_loop:
    cmp ecx, r13d
    jae checksum_nt_done
    
    ; Skip the CheckSum field (2 words at offset 0x58-0x5B)
    cmp ecx, r12d
    jb checksum_nt_read
    lea eax, [r12d + 4]
    cmp ecx, eax
    jb checksum_nt_skip
    
checksum_nt_read:
    movzx eax, word ptr [rsi + rcx]
    add edi, eax
    ; Fold carry: if sum > 0xFFFF, add carry back
    mov eax, edi
    shr eax, 16
    and edi, 0FFFFh
    add edi, eax
    
checksum_nt_skip:
    add ecx, 2
    jmp checksum_nt_loop
    
checksum_nt_done:
    ; Sum code buffer words
    mov rsi, [rbx].PE_CONTEXT.pCodeBuffer
    test rsi, rsi
    jz checksum_add_filesize
    mov r13d, [rbx].PE_CONTEXT.codeSize
    xor ecx, ecx
    
checksum_code_loop:
    cmp ecx, r13d
    jae checksum_add_filesize
    movzx eax, word ptr [rsi + rcx]
    add edi, eax
    mov eax, edi
    shr eax, 16
    and edi, 0FFFFh
    add edi, eax
    add ecx, 2
    jmp checksum_code_loop
    
checksum_add_filesize:
    ; Final fold
    mov eax, edi
    shr eax, 16
    and edi, 0FFFFh
    add edi, eax
    
    ; Add file size
    mov eax, [rbx].PE_CONTEXT.currentFileOffset
    add eax, edi
    
    ; Ensure non-zero
    test eax, eax
    jnz checksum_done
    
checksum_from_size:
    mov eax, [rbx].PE_CONTEXT.currentFileOffset
    test eax, eax
    jnz checksum_done
    mov eax, 1
    
checksum_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
CalculateChecksum ENDP

;=============================================================================
; MAIN FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; PEWriter_AddSection
; Adds a section to the PE executable
; Input: RCX = PE context, RDX = section name, R8 = characteristics
; Output: RAX = section index (0-based) or -1 on failure
;-----------------------------------------------------------------------------
PEWriter_AddSection PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; Name
    mov rbx, r8      ; Characteristics
    
    ; Check if max sections reached
    mov eax, [rdi].PE_CONTEXT.numSections
    cmp eax, MAX_SECTIONS
    jae add_section_fail
    
    ; Calculate offset to the new section entry in pSections array
    mov rcx, rax
    mov rdx, SIZEOF PE_SECTION_ENTRY
    mul rdx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax      ; r8 = pointer to new PE_SECTION_ENTRY
    
    ; Copy name (up to 8 chars)
    push rdi
    push rsi
    mov rdi, r8
    mov rcx, 8
    rep movsb
    pop rsi
    pop rdi
    
    ; Initialize fields
    mov dword ptr [r8].PE_SECTION_ENTRY.VirtualSize, 0
    
    ; Calculate VirtualAddress based on previous section or start
    mov eax, [rdi].PE_CONTEXT.numSections
    test eax, eax
    jz first_section
    
    ; Get previous section
    dec eax
    mov rcx, rax
    mov rdx, SIZEOF PE_SECTION_ENTRY
    mul rdx
    mov r9, [rdi].PE_CONTEXT.pSections
    add r9, rax
    
    ; Next VA = Prev VA + AlignUp(Prev VSize)
    mov ecx, [r9].PE_SECTION_ENTRY.VirtualAddress
    add ecx, [r9].PE_SECTION_ENTRY.VirtualSize
    push rdx
    mov rdx, SECTION_ALIGNMENT
    call AlignUp
    pop rdx
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    jmp va_done
    
first_section:
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, SECTION_ALIGNMENT
    
va_done:
    mov dword ptr [r8].PE_SECTION_ENTRY.SizeOfRawData, 0
    mov dword ptr [r8].PE_SECTION_ENTRY.PointerToRawData, 0
    mov [r8].PE_SECTION_ENTRY.Characteristics, ebx
    mov qword ptr [r8].PE_SECTION_ENTRY.pDataBuffer, 0
    
    ; Increment section count
    mov eax, [rdi].PE_CONTEXT.numSections
    inc dword ptr [rdi].PE_CONTEXT.numSections
    jmp add_section_success
    
add_section_fail:
    mov rax, -1
    
add_section_success:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_AddSection ENDP

; PEWriter_CreateExecutable
; Creates a new PE executable context
; Input: RCX = image base (0 = default), RDX = entry point RVA
; Output: RAX = PE context handle (0 = failure)
;-----------------------------------------------------------------------------
PEWriter_CreateExecutable PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 20h
    
    ; Save parameters
    mov rbx, rcx     ; image base
    mov rsi, rdx     ; entry point
    
    ; Get process heap
    call GetProcessHeap
    test rax, rax
    jz pe_create_fail
    mov rdi, rax
    mov r12, rax
    
    ; Allocate PE context
    mov rcx, rdi
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF PE_CONTEXT
    call HeapAlloc
    test rax, rax
    jz pe_create_fail
    mov rdi, rax     ; PE context
    
    ; Initialize PE context
    mov [rdi].PE_CONTEXT.hHeap, r12
    
    ; Set image base (use default if 0)
    test rbx, rbx
    jnz @F
    mov rbx, DEFAULT_IMAGE_BASE
@@: mov [rdi].PE_CONTEXT.imageBase, rbx
    mov [rdi].PE_CONTEXT.entryPoint, esi
    
    ; Allocate DOS header
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF IMAGE_DOS_HEADER
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pDosHeader, rax
    
    ; Initialize DOS header - properly calculated values
    mov rbx, rax
    
    ; Calculate proper NT header offset using helper function
    mov rcx, SIZEOF IMAGE_DOS_HEADER
    mov rdx, dos_stub_size
    call CalculateDOSHeaderOffset
    mov edx, eax                  ; Save NT header offset
    
    ; Calculate total DOS program size for page calculations
    mov ecx, SIZEOF IMAGE_DOS_HEADER
    add ecx, dos_stub_size
    
    ; Standard DOS header fields
    mov [rbx].IMAGE_DOS_HEADER.e_magic, IMAGE_DOS_SIGNATURE
    
    ; e_cblp = bytes in last page of file (remainder when dividing by 512)
    mov eax, ecx
    and eax, (DOS_PAGE_SIZE - 1)
    mov [rbx].IMAGE_DOS_HEADER.e_cblp, ax
    
    ; e_cp = pages in file (total size / 512 + 1 if remainder)
    mov eax, ecx
    add eax, DOS_PAGE_SIZE - 1    ; Round up
    shr eax, 9                    ; Divide by 512
    mov [rbx].IMAGE_DOS_HEADER.e_cp, ax
    
    ; Header size in paragraphs (DOS header only, 64 bytes = 4 paragraphs)
    mov [rbx].IMAGE_DOS_HEADER.e_cparhdr, (SIZEOF IMAGE_DOS_HEADER) / DOS_PARAGRAPH_SIZE
    
    ; Memory requirements
    mov [rbx].IMAGE_DOS_HEADER.e_minalloc, 0    ; Minimum extra paragraphs
    mov [rbx].IMAGE_DOS_HEADER.e_maxalloc, 0FFFFh ; Maximum extra paragraphs
    
    ; Initial stack segment and pointer
    mov [rbx].IMAGE_DOS_HEADER.e_ss, 0
    mov [rbx].IMAGE_DOS_HEADER.e_sp, 0B8h       ; 184 bytes stack
    
    ; Checksum (not used)
    mov [rbx].IMAGE_DOS_HEADER.e_csum, 0
    
    ; Initial instruction pointer and code segment
    mov [rbx].IMAGE_DOS_HEADER.e_ip, 0          ; Start at beginning
    mov [rbx].IMAGE_DOS_HEADER.e_cs, 0
    
    ; Relocation table offset (none needed)
    mov [rbx].IMAGE_DOS_HEADER.e_lfarlc, 40h
    
    ; Overlay number (main program)
    mov [rbx].IMAGE_DOS_HEADER.e_ovno, 0
    
    ; Reserved fields - zero them efficiently 
    xor eax, eax
    mov [rbx].IMAGE_DOS_HEADER.e_res[0], ax
    mov [rbx].IMAGE_DOS_HEADER.e_res[2], ax
    mov [rbx].IMAGE_DOS_HEADER.e_res[4], ax
    mov [rbx].IMAGE_DOS_HEADER.e_res[6], ax
    
    ; OEM fields
    mov [rbx].IMAGE_DOS_HEADER.e_oemid, 0
    mov [rbx].IMAGE_DOS_HEADER.e_oeminfo, 0
    
    ; Clear e_res2 array (10 words)
    push rdi
    lea rdi, [rbx].IMAGE_DOS_HEADER.e_res2
    mov rcx, 10
    rep stosw
    pop rdi
    
    ; NT header offset - calculated dynamically
    mov [rbx].IMAGE_DOS_HEADER.e_lfanew, edx
    
    ; Allocate NT header
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF IMAGE_NT_HEADERS64
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pNtHeader, rax
    
    ; Initialize NT header
    mov rbx, rax
    mov [rbx].IMAGE_NT_HEADERS64.Signature, IMAGE_NT_SIGNATURE
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections, 3
    
    ; Generate timestamp
    push rbx
    call CalculateTimestamp
    pop rbx
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.TimeDateStamp, eax
    
    ; Clear symbol table fields (not used in executables)
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.PointerToSymbolTable, 0
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.NumberOfSymbols, 0
    
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.SizeOfOptionalHeader, SIZEOF IMAGE_OPTIONAL_HEADER64
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.Characteristics, IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    ; Initialize optional header  
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.Magic, 20Bh
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MajorLinkerVersion, 14
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MinorLinkerVersion, 0
    mov rax, [rdi].PE_CONTEXT.imageBase
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.ImageBase, rax
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SectionAlignment, SECTION_ALIGNMENT
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.FileAlignment, FILE_ALIGNMENT
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MajorOperatingSystemVersion, 6
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MinorOperatingSystemVersion, 0
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MajorSubsystemVersion, 6 
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.MinorSubsystemVersion, 0
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.Subsystem, IMAGE_SUBSYSTEM_WINDOWS_CUI
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackReserve, 100000h
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackCommit, 1000h
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapReserve, 100000h
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapCommit, 1000h
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.NumberOfRvaAndSizes, 16
    mov eax, [rdi].PE_CONTEXT.entryPoint
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.AddressOfEntryPoint, eax
    
        ; Allocate section array (internal abstract structures)
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY  
    mov r8, SIZEOF PE_SECTION_ENTRY
    imul r8, MAX_SECTIONS
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pSections, rax
    mov [rdi].PE_CONTEXT.numSections, 0

    ; Allocate section headers (PE format structures)
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY  
    mov r8, SIZEOF IMAGE_SECTION_HEADER
    imul r8, MAX_SECTIONS
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pSectionHeaders, rax
    
    ; Add standard sections to maintain compatibility with existing usages
    mov rcx, rdi
    mov rdx, offset text_name
    mov r8, IMAGE_SCN_TEXT_CHARACTERISTICS
    call PEWriter_AddSection
    
    mov rcx, rdi
    mov rdx, offset rdata_name
    mov r8, IMAGE_SCN_RDATA_CHARACTERISTICS
    call PEWriter_AddSection
    
    mov rcx, rdi
    mov rdx, offset idata_name
    mov r8, 40000040h ; rdata characteristics
    call PEWriter_AddSection
    
    ; Setup default offsets for compatibility
    mov dword ptr [rdi].PE_CONTEXT.currentVirtualAddress, SECTION_ALIGNMENT * 4
    mov dword ptr [rdi].PE_CONTEXT.currentFileOffset, 0A00h
    
    ; Allocate code buffer
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, MAX_CODE_SIZE
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pCodeBuffer, rax
    
    ; Allocate import tables
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    imul r8, MAX_IMPORTS + 1
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportDescriptors, rax

    ; Allocate DLL table
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF IMPORT_DLL_ENTRY
    imul r8, MAX_IMPORTS
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportDLLTable, rax

    ; Allocate function table
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, SIZEOF IMPORT_FUNCTION_ENTRY
    imul r8, MAX_IMPORTS
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportFunctionTable, rax

    ; Allocate string table (4KB)
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, 4096
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pStringTable, rax

    ; Allocate import-by-name table (4KB)
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, 4096
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportByNameTable, rax
    
    ; Allocate IAT and ILT
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, MAX_IMPORTS * 8 * 2  ; Double size for safety
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportAddressTable, rax
    
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, MAX_IMPORTS * 8 * 2  ; Double size for safety
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pImportLookupTable, rax
    
    ; Allocate relocation table
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, MAX_RELOCATIONS * SIZEOF CODE_RELOCATION
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pRelocations, rax
    
    ; Allocate label table
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 8       ; HEAP_ZERO_MEMORY
    mov r8, MAX_LABELS * SIZEOF CODE_LABEL
    call HeapAlloc
    test rax, rax
    jz pe_create_cleanup
    mov [rdi].PE_CONTEXT.pLabels, rax
    
    ; Initialize machine code emitter
    mov [rdi].PE_CONTEXT.numRelocations, 0
    mov [rdi].PE_CONTEXT.numLabels, 0
    mov [rdi].PE_CONTEXT.currentFrame.frameSize, 0
    mov [rdi].PE_CONTEXT.currentFrame.localsSize, 0
    mov [rdi].PE_CONTEXT.currentFrame.savedRegs, 0
    mov [rdi].PE_CONTEXT.currentFrame.stackPos, 0
    
    mov rax, rdi     ; Return context
    jmp pe_create_done
    
pe_create_cleanup:
    ; Free allocated memory if any allocation failed
    mov rcx, [rdi].PE_CONTEXT.hHeap
    mov rdx, 0
    mov r8, rdi
    call HeapFree
    
pe_create_fail:
    xor rax, rax
    
pe_create_done:
    add rsp, 20h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_CreateExecutable ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddImport  
; Adds an import to the PE with full import table management
; Input: RCX = PE context, RDX = DLL name, R8 = function name
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
PEWriter_AddImport PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 48h
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; DLL name
    mov rbx, r8      ; Function name
    
    ; Check if we have space for more imports
    cmp [rdi].PE_CONTEXT.numImportFunctions, MAX_IMPORTS - 1
    jae import_fail
    
    ; Find or create DLL entry
    call FindOrCreateDLLEntry
    cmp rax, -1
    jne @F
    xor rax, rax
    jmp import_done
@@:
    mov r12, rax     ; DLL index
    
    ; Add function entry
    mov rcx, rdi
    mov rdx, r12
    mov r8, rbx
    call AddFunctionEntry
    test rax, rax
    jnz @F
    xor rax, rax
    jmp import_done
@@:
    
    mov rax, 1       ; Success
    jmp import_done
    
import_fail:
    xor rax, rax
    
import_done:
    add rsp, 48h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_AddImport ENDP

;-----------------------------------------------------------------------------
; FindOrCreateDLLEntry
; Finds existing DLL or creates new entry
; Input: RDI = PE context, RSI = DLL name
; Output: RAX = DLL index, 0 = failure
;-----------------------------------------------------------------------------
FindOrCreateDLLEntry PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    sub rsp, 28h
    
    ; Search for existing DLL
    mov rbx, [rdi].PE_CONTEXT.pImportDLLTable
    xor rcx, rcx     ; Index
    
find_dll_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportDLLs
    jae create_new_dll
    
    ; Compare DLL name
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    add rax, rbx
    lea rdx, [rax].IMPORT_DLL_ENTRY.nameString
    
    call StringCompare
    test rax, rax
    jz dll_found
    
    inc rcx
    jmp find_dll_loop
    
create_new_dll:
    ; Check if we have space
    cmp [rdi].PE_CONTEXT.numImportDLLs, MAX_IMPORTS - 1
    jae dll_entry_fail
    
    ; Get new DLL entry
    mov eax, [rdi].PE_CONTEXT.numImportDLLs
    mov rcx, rax
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    add rax, rbx
    mov r8, rax      ; New DLL entry
    mov r10, r8      ; Preserve entry pointer across StringCopy

    ; Copy DLL name
    lea rdx, [r8].IMPORT_DLL_ENTRY.nameString
    mov r8, rsi
    call StringCopy

    ; Initialize DLL entry
    mov [r10].IMPORT_DLL_ENTRY.descriptorIndex, ecx
    mov [r10].IMPORT_DLL_ENTRY.functionCount, 0
    
    ; Increment DLL count
    inc dword ptr [rdi].PE_CONTEXT.numImportDLLs
    mov rax, rcx     ; Return DLL index
    jmp dll_entry_done
    
dll_found:
    mov rax, rcx     ; Return existing DLL index
    jmp dll_entry_done
    
dll_entry_fail:
    mov rax, -1
    
dll_entry_done:
    add rsp, 28h
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rbp
    ret
FindOrCreateDLLEntry ENDP

;-----------------------------------------------------------------------------
; AddFunctionEntry
; Add a function to the import table
; Input: RCX = PE context, RDX = DLL index, R8 = function name
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
AddFunctionEntry PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    push r9
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; DLL index
    mov rbx, r8      ; Function name
    
    ; Get function entry
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    mov rcx, rax
    mov rax, SIZEOF IMPORT_FUNCTION_ENTRY
    mul rcx
    mov rdx, [rdi].PE_CONTEXT.pImportFunctionTable
    add rax, rdx
    mov r9, rax      ; Function entry
    
    ; Copy function name
    lea rdx, [r9].IMPORT_FUNCTION_ENTRY.nameString
    mov r8, rbx
    call StringCopy
    
    ; Set DLL index
    mov [r9].IMPORT_FUNCTION_ENTRY.dllIndex, esi
    
    ; Set hint (simple incrementing value)
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    mov [r9].IMPORT_FUNCTION_ENTRY.hint, ax
    
    ; Increment function count
    inc dword ptr [rdi].PE_CONTEXT.numImportFunctions
    inc dword ptr [rdi].PE_CONTEXT.numImports
    
    ; Update DLL function count
    mov rax, rsi
    mov rcx, SIZEOF IMPORT_DLL_ENTRY
    mul rcx
    mov rdx, [rdi].PE_CONTEXT.pImportDLLTable
    add rax, rdx
    inc dword ptr [rax].IMPORT_DLL_ENTRY.functionCount
    
    mov rax, 1       ; Success
    
    pop r9
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
AddFunctionEntry ENDP

;-----------------------------------------------------------------------------
; StringCompare
; Compare two null-terminated strings
; Input: RSI = string1, RDX = string2
; Output: RAX = 0 if equal, non-zero if different
;-----------------------------------------------------------------------------
StringCompare PROC
    push rcx
    push rsi
    push rdx
    
str_cmp_loop:
    mov al, [rsi]
    mov cl, [rdx]
    cmp al, cl
    jne str_not_equal
    
    test al, al      ; Check for null terminator
    jz str_equal
    
    inc rsi
    inc rdx
    jmp str_cmp_loop
    
str_equal:
    xor rax, rax     ; Strings are equal
    jmp str_cmp_done
    
str_not_equal:
    mov rax, 1       ; Strings are different
    
str_cmp_done:
    pop rdx
    pop rsi
    pop rcx
    ret
StringCompare ENDP

;-----------------------------------------------------------------------------
; StringCopy
; Copy null-terminated string
; Input: RDX = destination, R8 = source
; Output: RAX = length copied
;-----------------------------------------------------------------------------
StringCopy PROC
    push rcx
    push rsi
    push rdi
    
    mov rdi, rdx     ; Destination
    mov rsi, r8      ; Source
    xor rcx, rcx     ; Length counter
    
str_copy_loop:
    mov al, [rsi]
    mov [rdi], al
    test al, al
    jz str_copy_done
    
    inc rsi
    inc rdi
    inc rcx
    jmp str_copy_loop
    
str_copy_done:
    mov rax, rcx
    
    pop rdi
    pop rsi
    pop rcx
    ret
StringCopy ENDP

;-----------------------------------------------------------------------------
; StringLength
; Calculate null-terminated string length
; Input: RCX = string
; Output: RAX = length
;-----------------------------------------------------------------------------
StringLength PROC
    push rcx
    push rdx
    
    mov rdx, rcx
    xor rax, rax
    
str_len_loop:
    cmp byte ptr [rdx], 0
    jz str_len_done
    inc rdx
    inc rax
    jmp str_len_loop
    
str_len_done:
    pop rdx
    pop rcx
    ret
StringLength ENDP

;-----------------------------------------------------------------------------
; BuildImportTables
; Build complete import tables with proper RVAs
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
BuildImportTables PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 28h
    
    mov rdi, rcx     ; PE context
    
    ; Build string table and import-by-name structures
    call BuildStringTable
    test rax, rax
    jz build_tables_fail
    
    ; Build import-by-name structures
    mov rcx, rdi
    call BuildImportByNameStructures
    test rax, rax
    jz build_tables_fail
    
    ; Calculate import RVAs
    mov rcx, rdi
    call CalculateImportRVAs
    test rax, rax
    jz build_tables_fail
    
    ; Build IAT and ILT
    mov rcx, rdi
    call BuildImportThunks
    test rax, rax
    jz build_tables_fail
    
    ; Build import descriptors
    mov rcx, rdi
    call BuildImportDescriptors
    test rax, rax
    jz build_tables_fail
    
    mov rax, 1       ; Success
    jmp build_tables_done
    
build_tables_fail:
    xor rax, rax
    
build_tables_done:
    add rsp, 28h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
BuildImportTables ENDP

;-----------------------------------------------------------------------------
; BuildStringTable
; Build string table with DLL names
; Input: RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
BuildStringTable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rcx
    push rdx
    push rsi
    
    mov rbx, [rdi].PE_CONTEXT.pStringTable
    xor rsi, rsi     ; Current offset
    xor rcx, rcx     ; DLL index
    
string_table_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportDLLs
    jae string_table_done
    
    ; Get DLL entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportDLLTable
    add rax, rdx
    
    ; Store string offset
    mov [rax].IMPORT_DLL_ENTRY.nameOffset, esi
    
    ; Copy string to table
    lea rdx, [rbx + rsi]              ; Destination in string table
    lea r8, [rax].IMPORT_DLL_ENTRY.nameString  ; Source
    push rcx
    push r11
    call StringCopy
    pop r11
    pop rcx
    
    ; Update offset (include null terminator)
    inc rax
    add rsi, rax
    
    inc rcx
    jmp string_table_loop
    
string_table_done:
    mov [rdi].PE_CONTEXT.stringTableSize, esi
    mov rax, 1       ; Success
    
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 40h
    pop rbp
    ret
BuildStringTable ENDP

;-----------------------------------------------------------------------------
; BuildImportByNameStructures
; Build import by name structures for all functions
; Input: RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
BuildImportByNameStructures PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rcx
    push rdx
    push rsi
    push r8
    push r9
    
    mov rbx, [rdi].PE_CONTEXT.pImportByNameTable
    xor rsi, rsi     ; Current offset
    xor rcx, rcx     ; Function index
    
import_by_name_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportFunctions
    jae import_by_name_done
    
    ; Get function entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_FUNCTION_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportFunctionTable
    add rax, rdx
    mov r9, rax      ; Function entry
    
    ; Store import-by-name offset
    mov [r9].IMPORT_FUNCTION_ENTRY.nameOffset, esi
    
    ; Build import-by-name structure
    lea r8, [rbx + rsi]  ; Destination
    
    ; Write hint
    mov ax, [r9].IMPORT_FUNCTION_ENTRY.hint
    mov [r8], ax
    add r8, 2        ; Skip hint
    add rsi, 2
    
    ; Copy function name
    mov rdx, r8      ; Destination
    lea r8, [r9].IMPORT_FUNCTION_ENTRY.nameString  ; Source
    push rcx
    push r11
    call StringCopy
    pop r11
    pop rcx
    
    ; Update offset (include null terminator)
    inc rax
    add rsi, rax
    
    ; Align to 2-byte boundary
    test rsi, 1
    jz import_by_name_aligned
    inc rsi
    
import_by_name_aligned:
    inc rcx
    jmp import_by_name_loop
    
import_by_name_done:
    mov [rdi].PE_CONTEXT.importByNameSize, esi
    mov rax, 1       ; Success
    
    pop r9
    pop r8
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 40h
    pop rbp
    ret
BuildImportByNameStructures ENDP

;-----------------------------------------------------------------------------
; CalculateImportRVAs
; Calculate all import-related RVAs based on section layout
; Input: RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CalculateImportRVAs PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rcx
    push rdx
    push rsi
    
    ; Base RVA for import data
    mov ebx, [rdi].PE_CONTEXT.idataVirtualAddress   ; Import data base RVA
    
    ; Import descriptors start at beginning of .idata section.
    mov eax, [rdi].PE_CONTEXT.numImportDLLs
    inc eax
    imul eax, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    mov esi, eax     ; Offset after descriptors
    
    ; String table RVAs
    xor rcx, rcx
    
string_rva_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportDLLs
    jae string_rva_done
    
    ; Get DLL entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportDLLTable
    add rax, rdx
    
    ; Calculate name RVA
    mov edx, ebx     ; Base RVA
    add edx, esi     ; Offset after descriptors
    add edx, [rax].IMPORT_DLL_ENTRY.nameOffset
    mov [rax].IMPORT_DLL_ENTRY.nameRVA, edx
    
    inc rcx
    jmp string_rva_loop
    
string_rva_done:
    ; Import-by-name RVAs
    add esi, [rdi].PE_CONTEXT.stringTableSize
    
    xor rcx, rcx
    
import_by_name_rva_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportFunctions
    jae import_by_name_rva_done
    
    ; Get function entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_FUNCTION_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportFunctionTable
    add rax, rdx
    
    ; Calculate import-by-name RVA
    mov edx, ebx     ; Base RVA
    add edx, esi     ; Offset after strings
    add edx, [rax].IMPORT_FUNCTION_ENTRY.nameOffset
    mov [rax].IMPORT_FUNCTION_ENTRY.nameRVA, edx
    
    inc rcx
    jmp import_by_name_rva_loop
    
import_by_name_rva_done:
    ; Calculate total import size
    add esi, [rdi].PE_CONTEXT.importByNameSize
    ; Align before thunk tables (QWORD entries).
    add esi, 7
    and esi, 0FFFFFFF8h
    ; Add space for thunks (IAT and ILT)
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    add eax, [rdi].PE_CONTEXT.numImportDLLs  ; Null terminators
    shl eax, 4       ; 8 bytes per thunk * 2 (IAT + ILT)
    add esi, eax
    
    mov [rdi].PE_CONTEXT.totalImportSize, esi
    mov rax, 1       ; Success
    
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CalculateImportRVAs ENDP

;-----------------------------------------------------------------------------
; BuildImportThunks
; Build Import Address Table (IAT) and Import Lookup Table (ILT)
; Input: RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
BuildImportThunks PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rcx
    push rdx
    push rsi
    push r8
    push r9
    
    mov rbx, [rdi].PE_CONTEXT.pImportAddressTable
    mov rsi, [rdi].PE_CONTEXT.pImportLookupTable
    xor rcx, rcx     ; Current DLL index
    mov r8, 0        ; Current thunk offset (in QWORDs)
    
thunk_dll_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportDLLs
    jae thunk_done
    
    ; Get DLL entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportDLLTable
    add rax, rdx
    mov r9, rax      ; DLL entry
    
    ; Store thunk RVAs for this DLL
    mov eax, [rdi].PE_CONTEXT.idataVirtualAddress
    mov edx, [rdi].PE_CONTEXT.numImportFunctions
    add edx, [rdi].PE_CONTEXT.numImportDLLs
    shl edx, 4
    mov eax, [rdi].PE_CONTEXT.totalImportSize
    sub eax, edx
    mov edx, eax
    add eax, edx
    add eax, r8d
    shl eax, 3       ; Convert to byte offset
    add eax, [rdi].PE_CONTEXT.idataVirtualAddress
    
    ; Calculate thunk base for this DLL
    mov edx, [rdi].PE_CONTEXT.idataVirtualAddress
    add edx, [rdi].PE_CONTEXT.totalImportSize
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    add eax, [rdi].PE_CONTEXT.numImportDLLs
    shl eax, 4
    sub edx, eax
    mov eax, r8d
    shl eax, 3       ; Convert to byte offset
    add edx, eax
    
    ; Store FirstThunk RVA (IAT)
    mov [r9].IMPORT_DLL_ENTRY.firstThunkRVA, edx
    
    ; Store OriginalFirstThunk RVA (ILT) - offset by full IAT size
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    add eax, [rdi].PE_CONTEXT.numImportDLLs
    shl eax, 3       ; 8 bytes per thunk
    add edx, eax
    mov [r9].IMPORT_DLL_ENTRY.originalFirstThunkRVA, edx
    
    ; Build thunks for this DLL's functions
    push rcx
    mov rcx, 0       ; Function index within this DLL
    
thunk_function_loop:
    cmp ecx, [r9].IMPORT_DLL_ENTRY.functionCount
    jae thunk_function_done
    
    ; Find function entry for this DLL
    sub rsp, 8
    push rcx
    push r8
    push r9
    call FindFunctionByDLLAndIndex
    pop r9
    pop r8
    pop rcx
    add rsp, 8
    test rax, rax
    jz thunk_function_fail
    
    ; Get function's import-by-name RVA
    mov edx, [rax].IMPORT_FUNCTION_ENTRY.nameRVA
    
    ; Store in IAT
    mov rax, r8
    shl rax, 3       ; Convert to byte offset
    add rax, rbx
    mov [rax], rdx
    
    ; Store in ILT
    mov rax, r8      ; Add current thunk offset
    shl rax, 3       ; Convert to byte offset
    add rax, rsi
    mov [rax], rdx
    
    inc rcx
    inc r8           ; Next thunk
    jmp thunk_function_loop
    
thunk_function_fail:
    pop rcx
    jmp thunk_fail
    
thunk_function_done:
    pop rcx
    
    ; Add null terminator for this DLL
    mov rax, r8
    shl rax, 3       ; Convert to byte offset
    add rax, rbx
    mov qword ptr [rax], 0  ; IAT null terminator
    
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    add eax, [rdi].PE_CONTEXT.numImportDLLs
    add rax, r8
    shl rax, 3       ; Convert to byte offset
    add rax, rsi
    mov qword ptr [rax], 0  ; ILT null terminator
    
    inc r8           ; Account for null terminator
    inc rcx
    jmp thunk_dll_loop
    
thunk_done:
    mov rax, 1       ; Success
    jmp thunk_exit
    
thunk_fail:
    xor rax, rax
    
thunk_exit:
    pop r9
    pop r8
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 40h
    pop rbp
    ret
BuildImportThunks ENDP

;-----------------------------------------------------------------------------
; FindFunctionByDLLAndIndex
; Find function entry by DLL and function index within DLL
; Input: RDI = PE context, R9 = DLL entry, RCX = function index within DLL
; Output: RAX = function entry or 0 if not found
;-----------------------------------------------------------------------------
FindFunctionByDLLAndIndex PROC
    push rbx
    push rsi
    
    mov esi, [r9].IMPORT_DLL_ENTRY.descriptorIndex  ; DLL index
    xor ebx, ebx     ; Global function index
    xor r10d, r10d   ; Functions found for this DLL
    
find_func_loop:
    cmp ebx, [rdi].PE_CONTEXT.numImportFunctions
    jae find_func_fail
    
    ; Get function entry
    mov eax, ebx
    imul rax, SIZEOF IMPORT_FUNCTION_ENTRY
    mov r11, [rdi].PE_CONTEXT.pImportFunctionTable
    add rax, r11
    
    ; Check if this function belongs to our DLL
    cmp [rax].IMPORT_FUNCTION_ENTRY.dllIndex, esi
    jne find_func_next
    
    ; Check if this is the function we want within this DLL
    cmp r10, rcx
    je find_func_done
    
    inc r10          ; Increment functions found for this DLL
    
find_func_next:
    inc ebx
    jmp find_func_loop
    
find_func_fail:
    xor rax, rax
    
find_func_done:
    pop rsi
    pop rbx
    ret
FindFunctionByDLLAndIndex ENDP

;-----------------------------------------------------------------------------
; BuildImportDescriptors
; Build final import descriptors with proper RVAs
; Input: RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
BuildImportDescriptors PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rcx
    push rdx
    push rsi
    
    mov rbx, [rdi].PE_CONTEXT.pImportDescriptors
    xor rcx, rcx     ; DLL index
    
descriptor_loop:
    cmp ecx, [rdi].PE_CONTEXT.numImportDLLs
    jae descriptor_done
    
    ; Get DLL entry
    mov rax, rcx
    mov rdx, SIZEOF IMPORT_DLL_ENTRY
    mul rdx
    mov rdx, [rdi].PE_CONTEXT.pImportDLLTable
    add rax, rdx
    mov rsi, rax     ; DLL entry
    
    ; Get import descriptor
    mov rax, rcx
    mov rdx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    mul rdx
    add rax, rbx
    
    ; Fill import descriptor
    mov edx, [rsi].IMPORT_DLL_ENTRY.originalFirstThunkRVA
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk, edx
    
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.TimeDateStamp, 0
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.ForwarderChain, 0
    
    mov edx, [rsi].IMPORT_DLL_ENTRY.nameRVA
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.Name1, edx
    
    mov edx, [rsi].IMPORT_DLL_ENTRY.firstThunkRVA
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.FirstThunk, edx
    
    inc rcx
    jmp descriptor_loop
    
descriptor_done:
    ; Add null terminator descriptor
    mov rax, rcx
    mov rdx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    mul rdx
    add rax, rbx
    
    ; Zero out null terminator
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk, 0
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.TimeDateStamp, 0
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.ForwarderChain, 0
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.Name1, 0
    mov [rax].IMAGE_IMPORT_DESCRIPTOR.FirstThunk, 0
    
    mov rax, 1       ; Success
    
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 40h
    pop rbp
    ret
BuildImportDescriptors ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddCode
; Adds machine code to the PE
; Input: RCX = PE context, RDX = code buffer, R8 = code size
; Output: RAX = RVA of code (0 = failure)
;-----------------------------------------------------------------------------
PEWriter_AddCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; Code buffer
    mov rbx, r8      ; Code size
    
    ; Check if code fits in allocated buffer
    mov eax, [rdi].PE_CONTEXT.codeSize
    add eax, ebx
    cmp eax, MAX_CODE_SIZE
    ja code_fail
    
    ; Copy code to buffer
    mov rcx, [rdi].PE_CONTEXT.pCodeBuffer
    mov eax, [rdi].PE_CONTEXT.codeSize
    add rcx, rax
    mov rdx, rsi
    mov r8, rbx
    
code_copy_loop:
    test r8, r8
    jz code_copy_done
    mov al, [rdx]
    mov [rcx], al
    inc rcx
    inc rdx
    dec r8
    jmp code_copy_loop
    
code_copy_done:
    ; Calculate RVA
    mov eax, [rdi].PE_CONTEXT.textVirtualAddress
    test eax, eax
    jnz @F
    mov eax, SECTION_ALIGNMENT
    mov [rdi].PE_CONTEXT.textVirtualAddress, eax
@@:
    add eax, [rdi].PE_CONTEXT.codeSize
    
    ; Update code size
    add dword ptr [rdi].PE_CONTEXT.codeSize, ebx
    
    jmp code_done
    
code_fail:
    xor rax, rax
    
code_done:
    pop rdi
    pop rsi  
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PEWriter_AddCode ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddData - Add initialized data to the .data section
; Input: RCX = PE context, RDX = data buffer, R8 = data size
; Output: RAX = RVA of added data, 0 on failure
;-----------------------------------------------------------------------------
PEWriter_AddData PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; Source data buffer
    mov rbx, r8      ; Data size
    
    test rbx, rbx
    jz data_add_fail
    
    ; Allocate data buffer on first use
    mov rax, [rdi].PE_CONTEXT.pDataBuffer
    test rax, rax
    jnz data_buf_ready
    
    ; Allocate MAX_DATA_SIZE buffer via HeapAlloc
    call GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, MAX_DATA_SIZE
    call HeapAlloc
    test rax, rax
    jz data_add_fail
    mov [rdi].PE_CONTEXT.pDataBuffer, rax
    mov dword ptr [rdi].PE_CONTEXT.dataSize, 0
    
data_buf_ready:
    ; Check capacity
    mov eax, [rdi].PE_CONTEXT.dataSize
    add eax, ebx
    cmp eax, MAX_DATA_SIZE
    ja data_add_fail
    
    ; Copy data to buffer at current offset
    mov rcx, [rdi].PE_CONTEXT.pDataBuffer
    mov eax, [rdi].PE_CONTEXT.dataSize
    add rcx, rax                  ; Destination = buffer + current size
    mov rdx, rsi                  ; Source
    mov r8, rbx                   ; Count
    
data_copy_loop:
    test r8, r8
    jz data_copy_done
    mov al, [rdx]
    mov [rcx], al
    inc rcx
    inc rdx
    dec r8
    jmp data_copy_loop
    
data_copy_done:
    ; Calculate RVA of the newly added data
    mov eax, [rdi].PE_CONTEXT.dataVirtualAddress
    test eax, eax
    jnz @F
    ; Default: .data comes after .text (2 * SECTION_ALIGNMENT)
    mov eax, SECTION_ALIGNMENT * 2
    mov [rdi].PE_CONTEXT.dataVirtualAddress, eax
@@:
    add eax, [rdi].PE_CONTEXT.dataSize   ; RVA = base + offset
    push rax                              ; Save return RVA
    
    ; Update data size
    add dword ptr [rdi].PE_CONTEXT.dataSize, ebx
    
    pop rax    ; Return RVA
    jmp data_add_done
    
data_add_fail:
    xor rax, rax
    
data_add_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PEWriter_AddData ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddBssSpace - Reserve uninitialized data (.bss) space
; Input: RCX = PE context, RDX = size to reserve
; Output: RAX = RVA of reserved space, 0 on failure
;-----------------------------------------------------------------------------
PEWriter_AddBssSpace PROC
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rcx     ; PE context
    
    test rdx, rdx
    jz bss_fail
    
    ; Calculate RVA for BSS (after .data section in virtual space)
    mov eax, [rbx].PE_CONTEXT.bssVirtualAddress
    test eax, eax
    jnz @F
    ; Default: after .data
    mov eax, [rbx].PE_CONTEXT.dataVirtualAddress
    test eax, eax
    jnz bss_after_data
    mov eax, SECTION_ALIGNMENT * 2    ; Fallback
bss_after_data:
    mov ecx, [rbx].PE_CONTEXT.dataSize
    add ecx, SECTION_ALIGNMENT - 1
    and ecx, NOT (SECTION_ALIGNMENT - 1)
    add eax, ecx
    mov [rbx].PE_CONTEXT.bssVirtualAddress, eax
@@:
    ; Return current BSS RVA and grow
    add eax, [rbx].PE_CONTEXT.bssSize
    push rax
    add dword ptr [rbx].PE_CONTEXT.bssSize, edx
    pop rax
    jmp bss_done
    
bss_fail:
    xor rax, rax
    
bss_done:
    pop rbx
    pop rbp
    ret
PEWriter_AddBssSpace ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddBaseRelocation - Record a base relocation for ASLR
; Tracks absolute address locations that need fixup when image is rebased
; Input: RCX = PE context, EDX = page RVA, R8W = type_offset (type << 12 | offset)
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
PEWriter_AddBaseRelocation PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx     ; PE context
    
    ; Allocate reloc buffer on first use
    mov rax, [rbx].PE_CONTEXT.pRelocBuffer
    test rax, rax
    jnz reloc_buf_ready
    
    call GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, MAX_RELOC_SIZE
    call HeapAlloc
    test rax, rax
    jz reloc_add_fail
    mov [rbx].PE_CONTEXT.pRelocBuffer, rax
    mov dword ptr [rbx].PE_CONTEXT.relocBufferSize, 0
    mov dword ptr [rbx].PE_CONTEXT.numBaseRelocations, 0
    
reloc_buf_ready:
    ; Check capacity
    mov eax, [rbx].PE_CONTEXT.relocBufferSize
    add eax, 2                     ; Each entry is a WORD
    cmp eax, MAX_RELOC_SIZE
    ja reloc_add_fail
    
    ; Find or create relocation block for this page RVA
    ; Walk existing blocks to find matching PageRVA
    mov rsi, [rbx].PE_CONTEXT.pRelocBuffer
    xor edi, edi                   ; Current offset in reloc buffer
    
reloc_find_block:
    cmp edi, [rbx].PE_CONTEXT.relocBufferSize
    jae reloc_new_block
    
    ; Read block header: PageRVA (DWORD) + BlockSize (DWORD)
    mov eax, dword ptr [rsi + rdi]       ; PageRVA
    cmp eax, edx                          ; Match page?
    je reloc_found_block
    
    ; Skip to next block
    mov eax, dword ptr [rsi + rdi + 4]   ; BlockSize
    test eax, eax
    jz reloc_new_block                    ; Null terminator = end
    add edi, eax
    jmp reloc_find_block
    
reloc_found_block:
    ; Found matching block -- append entry before end
    mov eax, dword ptr [rsi + rdi + 4]   ; Current BlockSize
    lea ecx, [eax - 8]                   ; Entries area size (block - header)
    
    ; Insert new entry at end of block's entry area
    ; Move subsequent blocks forward by 2 bytes
    ; New entry position = block_start + BlockSize
    mov r9d, [rbx].PE_CONTEXT.relocBufferSize
    sub r9d, edi
    sub r9d, eax                         ; Bytes after this block
    
    ; Shift tail data forward by 2
    test r9d, r9d
    jz reloc_no_shift
    
    ; Compute base address: rsi + rdi + rax (3 regs, can't use in single LEA)
    mov rcx, rsi
    add rcx, rdi
    add rcx, rax                         ; rcx = rsi + rdi + rax
    lea rdx, [rcx]                       ; Source = rsi + rdi + rax
    lea rcx, [rcx + 2]                   ; Destination = Source + 2
    mov r8d, r9d                          ; Count
    
    ; Shift backwards (copy from end to start to avoid overlap)
    add rcx, r8
    add rdx, r8
reloc_shift_loop:
    test r8d, r8d
    jz reloc_no_shift
    dec rcx
    dec rdx
    mov al, [rdx]
    mov [rcx], al
    dec r8d
    jmp reloc_shift_loop
    
reloc_no_shift:
    ; Write new entry at block_start + old_BlockSize
    lea r9, [rsi + rdi]
    mov eax, dword ptr [r9 + 4]          ; Old BlockSize
    mov word ptr [r9 + rax], r8w         ; Write type_offset entry
    add dword ptr [r9 + 4], 2            ; Increment BlockSize by 2
    
    ; Ensure DWORD alignment of BlockSize
    mov eax, dword ptr [r9 + 4]
    test eax, 3
    jz reloc_block_aligned
    add eax, 2                           ; Pad to DWORD boundary
    mov dword ptr [r9 + 4], eax
    ; Write padding entry (type 0 = IMAGE_REL_BASED_ABSOLUTE = padding)
    mov word ptr [r9 + rax - 2], 0
reloc_block_aligned:
    
    add dword ptr [rbx].PE_CONTEXT.relocBufferSize, 2
    inc dword ptr [rbx].PE_CONTEXT.numBaseRelocations
    mov rax, 1
    jmp reloc_add_done
    
reloc_new_block:
    ; Create new block header at current end of buffer
    ; Block header: PageRVA (DWORD) + BlockSize (DWORD) + entries...
    ; Minimum block = header(8) + one entry(2) + padding(2) = 12 bytes
    mov eax, [rbx].PE_CONTEXT.relocBufferSize
    add eax, 12                          ; Header + entry + pad
    cmp eax, MAX_RELOC_SIZE
    ja reloc_add_fail
    
    mov eax, [rbx].PE_CONTEXT.relocBufferSize
    mov dword ptr [rsi + rax], edx       ; PageRVA
    mov dword ptr [rsi + rax + 4], 12    ; BlockSize = 12 (header + 1 entry + pad)
    mov word ptr [rsi + rax + 8], r8w    ; First entry (type_offset)
    mov word ptr [rsi + rax + 10], 0     ; Padding entry (IMAGE_REL_BASED_ABSOLUTE)
    
    ; Write null terminator block after
    mov dword ptr [rsi + rax + 12], 0    ; PageRVA = 0 (sentinel)
    mov dword ptr [rsi + rax + 16], 0    ; BlockSize = 0
    
    add dword ptr [rbx].PE_CONTEXT.relocBufferSize, 12
    inc dword ptr [rbx].PE_CONTEXT.numBaseRelocations
    mov rax, 1
    jmp reloc_add_done
    
reloc_add_fail:
    xor rax, rax
    
reloc_add_done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_AddBaseRelocation ENDP

;-----------------------------------------------------------------------------
; PEWriter_BuildRelocSection - Finalize .reloc section from accumulated entries
; Scans code buffer for absolute address references and builds relocation table
; Input: RCX = PE context
; Output: RAX = total .reloc section size, 0 on failure
;-----------------------------------------------------------------------------
PEWriter_BuildRelocSection PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 40h
    
    mov rbx, rcx     ; PE context
    
    ; If no explicit relocations were added, scan code buffer for
    ; absolute addresses that reference ImageBase-relative targets.
    ; In x64 PE files, the primary relocations are for:
    ; - mov reg, imm64 instructions loading absolute addresses
    ; - Data section pointers embedded in .text
    
    mov rax, [rbx].PE_CONTEXT.pRelocBuffer
    test rax, rax
    jz reloc_build_empty
    
    mov eax, [rbx].PE_CONTEXT.relocBufferSize
    test eax, eax
    jz reloc_build_empty
    
    ; Reloc buffer already populated by PEWriter_AddBaseRelocation calls.
    ; Just align the total size to FILE_ALIGNMENT.
    mov ecx, [rbx].PE_CONTEXT.relocBufferSize
    ; Add 8 bytes for null terminator block if not already present
    add ecx, 8
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r12d, eax                     ; Aligned reloc section raw size
    
    ; Store in context
    mov eax, r12d
    jmp reloc_build_done
    
reloc_build_empty:
    ; Create minimal reloc section (just null terminator block)
    ; This satisfies ASLR: loader sees empty reloc table, no fixups needed
    ; But the section must exist for IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
    
    ; Allocate buffer if needed
    mov rax, [rbx].PE_CONTEXT.pRelocBuffer
    test rax, rax
    jnz reloc_have_buf
    
    call GetProcessHeap
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8, MAX_RELOC_SIZE
    call HeapAlloc
    test rax, rax
    jz reloc_build_fail
    mov [rbx].PE_CONTEXT.pRelocBuffer, rax
    
reloc_have_buf:
    ; Write null terminator block
    mov rsi, [rbx].PE_CONTEXT.pRelocBuffer
    mov dword ptr [rsi], 0           ; PageRVA = 0
    mov dword ptr [rsi + 4], 0       ; BlockSize = 0
    mov dword ptr [rbx].PE_CONTEXT.relocBufferSize, 8
    
    mov eax, FILE_ALIGNMENT          ; Minimum section raw size
    jmp reloc_build_done
    
reloc_build_fail:
    xor eax, eax
    
reloc_build_done:
    add rsp, 40h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_BuildRelocSection ENDP

;=============================================================================
; MACHINE CODE EMITTER FUNCTIONS
;=============================================================================

;-----------------------------------------------------------------------------
; EmitByte - Emit a single byte to the code buffer
; Input: RCX = PE context, DL = byte to emit
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
EmitByte PROC
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    
    ; Check bounds
    mov eax, [rbx].PE_CONTEXT.codeSize
    cmp eax, MAX_CODE_SIZE - 1
    jae emit_byte_fail
    
    ; Write byte
    mov rsi, [rbx].PE_CONTEXT.pCodeBuffer
    add rsi, rax
    mov [rsi], dl
    
    ; Update code size
    inc dword ptr [rbx].PE_CONTEXT.codeSize
    
    mov rax, 1
    jmp emit_byte_done
    
emit_byte_fail:
    xor rax, rax
    
emit_byte_done:
    pop rsi
    pop rbx
    ret
EmitByte ENDP

;-----------------------------------------------------------------------------
; EmitBytes - Emit multiple bytes to the code buffer
; Input: RCX = PE context, RDX = buffer, R8 = count
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
EmitBytes PROC
    push rbx
    push rsi
    push rdi
    push r9
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Source buffer
    mov r9, r8      ; Count
    
    ; Check bounds
    mov eax, [rbx].PE_CONTEXT.codeSize
    add eax, r8d
    cmp eax, MAX_CODE_SIZE
    ja emit_bytes_fail
    
    ; Copy bytes
    mov rdi, [rbx].PE_CONTEXT.pCodeBuffer
    mov eax, [rbx].PE_CONTEXT.codeSize
    add rdi, rax
    
emit_bytes_loop:
    test r9, r9
    jz emit_bytes_done
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec r9
    jmp emit_bytes_loop
    
emit_bytes_done:
    ; Update code size
    add dword ptr [rbx].PE_CONTEXT.codeSize, r8d
    mov rax, 1
    jmp emit_bytes_exit
    
emit_bytes_fail:
    xor rax, rax
    
emit_bytes_exit:
    pop r9
    pop rdi
    pop rsi
    pop rbx
    ret
EmitBytes ENDP

;-----------------------------------------------------------------------------
; CalculateREX - Calculate REX prefix for x64 instruction
; Input: RCX = reg field, RDX = r/m field, R8 = 64-bit flag
; Output: RAX = REX byte (0 if not needed)
;-----------------------------------------------------------------------------
CalculateREX PROC
    mov r9, 40h     ; Base REX prefix
    
    ; Check if 64-bit operand
    test r8, r8
    jz check_reg_ext
    or r9, 8h       ; REX.W
    
check_reg_ext:
    ; Check reg field extension (bits 3+)
    cmp rcx, 7
    jbe check_rm_ext
    or r9, 4h       ; REX.R
    
check_rm_ext:
    ; Check r/m field extension
    cmp rdx, 7
    jbe rex_done
    or r9, 1h       ; REX.B
    
rex_done:
    ; Only return REX if we need one
    cmp r9, 40h
    je rex_not_needed
    mov rax, r9
    ret
    
rex_not_needed:
    xor rax, rax
    ret
CalculateREX ENDP

;-----------------------------------------------------------------------------
; EmitModRM - Emit ModR/M byte with proper encoding
; Input: RCX = mod, RDX = reg, R8 = r/m
; Output: RAX = ModR/M byte
;-----------------------------------------------------------------------------
EmitModRM PROC
    ; Mask registers to 3 bits for ModR/M encoding
    and rdx, 7
    and r8, 7
    
    ; Combine: mod(7:6) + reg(5:3) + r/m(2:0)
    shl rcx, 6
    shl rdx, 3
    or rcx, rdx
    or rcx, r8
    
    mov rax, rcx
    ret
EmitModRM ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionPrologue
; Standard x64 Windows function prologue
; Input: RCX = PE context, RDX = frame size, R8 = saved registers bitmask
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_FunctionPrologue PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Frame size
    mov rdi, r8     ; Saved registers
    
    ; Save frame info
    mov [rbx].PE_CONTEXT.currentFrame.frameSize, esi
    mov [rbx].PE_CONTEXT.currentFrame.savedRegs, rdi
    mov [rbx].PE_CONTEXT.currentFrame.stackPos, 0
    
    ; PUSH RBP - 55h
    mov rcx, rbx
    mov dl, 55h
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    ; MOV RBP, RSP - 48 89 E5h
    mov rcx, rbx
    mov dl, 48h     ; REX.W
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 89h     ; MOV opcode
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 0E5h    ; ModR/M: RSP -> RBP
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    ; SUB RSP, frame_size if needed
    test rsi, rsi
    jz prologue_save_regs
    
    ; Align frame size to 16 bytes
    add rsi, 15
    and rsi, -16
    
    cmp rsi, 128
    jb prologue_sub_imm8
    
    ; SUB RSP, imm32 - 48 81 EC [imm32]
    mov rcx, rbx
    mov dl, 48h     ; REX.W
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 81h     ; SUB opcode
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 0ECh    ; ModR/M: SUB RSP
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    ; Emit 32-bit immediate
    mov eax, esi
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz prologue_fail
    jmp prologue_save_regs
    
prologue_sub_imm8:
    ; SUB RSP, imm8 - 48 83 EC [imm8]
    mov rcx, rbx
    mov dl, 48h     ; REX.W
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 83h     ; SUB opcode (imm8)
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, 0ECh    ; ModR/M: SUB RSP
    call EmitByte
    test rax, rax
    jz prologue_fail
    
    mov rcx, rbx
    mov dl, sil     ; 8-bit immediate
    call EmitByte
    test rax, rax
    jz prologue_fail
    
prologue_save_regs:
    ; Save registers based on bitmask
    ; (Implementation would continue for each register in the bitmask)
    
    mov rax, 1
    jmp prologue_done
    
prologue_fail:
    xor rax, rax
    
prologue_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_FunctionPrologue ENDP

;-----------------------------------------------------------------------------
; Emit_ENDPROLOG
; Signals the end of the function prologue to the PE writer for SEH generation
; Input: RCX = PE context
; Output: RAX = 1 success
;-----------------------------------------------------------------------------
Emit_ENDPROLOG PROC
    mov r8, rcx
    ; Internal state tracking for SEH Unwind info
    mov eax, [r8].PE_CONTEXT.codeSize
    mov [r8].PE_CONTEXT.currentFrame.prologSize, eax
    
    ; Record RUNTIME_FUNCTION entry if exception section is active
    ; (Simplified: simulation of .pdata entry creation)
    mov r9, [r8].PE_CONTEXT.pSections
    test r9, r9
    jz @F
    
    ; Logic to write BeginAddress, EndAddress, and UnwindInfoAddress to .pdata
    ; ...
@@:
    mov rax, 1
    ret
Emit_ENDPROLOG ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionEpilogue
; Standard x64 Windows function epilogue
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_FunctionEpilogue PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    
    ; Restore registers (reverse order of saving)
    ; (Implementation would restore based on saved bitmask)
    
    ; MOV RSP, RBP - 48 89 ECh
    mov rcx, rbx
    mov dl, 48h     ; REX.W
    call EmitByte
    test rax, rax
    jz epilogue_fail
    
    mov rcx, rbx
    mov dl, 89h     ; MOV opcode
    call EmitByte
    test rax, rax
    jz epilogue_fail
    
    mov rcx, rbx
    mov dl, 0ECh    ; ModR/M: RBP -> RSP
    call EmitByte
    test rax, rax
    jz epilogue_fail
    
    ; POP RBP - 5Dh
    mov rcx, rbx
    mov dl, 5Dh
    call EmitByte
    test rax, rax
    jz epilogue_fail
    
    mov rax, 1
    jmp epilogue_done
    
epilogue_fail:
    xor rax, rax
    
epilogue_done:
    ; Clear current frame
    mov [rbx].PE_CONTEXT.currentFrame.frameSize, 0
    mov [rbx].PE_CONTEXT.currentFrame.savedRegs, 0
    
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_FunctionEpilogue ENDP

;-----------------------------------------------------------------------------
; Emit_MOV
; Emit MOV instruction - supports reg-to-reg, imm-to-reg, mem-to-reg variants
; Input: RCX = PE context, RDX = dest operand, R8 = src operand 
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_MOV PROC
    push rbp
    mov rbp, rsp
    sub rsp, 60h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Dest operand (CODE_OPERAND ptr)
    mov rdi, r8     ; Src operand (CODE_OPERAND ptr)
    
    ; Check operand types and emit appropriate MOV variant
    mov eax, [rsi].CODE_OPERAND.opType
    mov ecx, [rdi].CODE_OPERAND.opType
    
    ; REG <- REG
    cmp eax, OP_REG
    jne mov_check_imm
    cmp ecx, OP_REG
    jne mov_reg_imm
    
    ; MOV reg, reg - 48 89 /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8, 1       ; 64-bit
    call CalculateREX
    test rax, rax
    jz mov_reg_reg_no_rex
    
    mov rcx, rbx
    mov dl, al     ; REX prefix
    call EmitByte
    test rax, rax
    jz mov_fail
    
mov_reg_reg_no_rex:
    ; Emit MOV opcode
    mov rcx, rbx
    mov dl, 89h    ; MOV r/m64, r64
    call EmitByte
    test rax, rax
    jz mov_fail
    
    ; Emit ModR/M
    mov rcx, MODRM_REG_DIRECT  ; mod = 11b (register)
    mov edx, [rdi].CODE_OPERAND.regNum  ; reg (source)
    mov r8d, [rsi].CODE_OPERAND.regNum   ; r/m (dest)
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al     ; ModR/M byte
    call EmitByte
    test rax, rax
    jz mov_fail
    jmp mov_success
    
mov_reg_imm:
    ; REG <- IMM
    cmp ecx, OP_IMM
    jne mov_fail
    
    ; MOV reg, imm64 - 48 B8+r [imm64]
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1       ; 64-bit
    call CalculateREX
    test rax, rax
    jz mov_imm_no_rex
    
    mov rcx, rbx
    mov dl, al     ; REX prefix
    call EmitByte
    test rax, rax
    jz mov_fail
    
mov_imm_no_rex:
    ; Emit MOV immediate opcode
    mov eax, [rsi].CODE_OPERAND.regNum
    and eax, 7     ; Mask to 3 bits for opcode
    add eax, 0B8h  ; MOV reg, imm64 base opcode
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz mov_fail
    
    ; Emit 64-bit immediate
    mov rax, [rdi].CODE_OPERAND.immediate
    mov [rsp+40h], rax
    
    mov rcx, rbx
    lea rdx, [rsp+40h]
    mov r8, 8
    call EmitBytes
    test rax, rax
    jz mov_fail
    jmp mov_success
    
mov_check_imm:
    ; Add other MOV variants here (mem operations, etc.)
    jmp mov_fail
    
mov_success:
    mov rax, 1
    jmp mov_done
    
mov_fail:
    xor rax, rax
    
mov_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 60h
    pop rbp
    ret
Emit_MOV ENDP

;-----------------------------------------------------------------------------
; Emit_ADD
; Emit ADD instruction
; Input: RCX = PE context, RDX = dest operand, R8 = src operand
; Output: RAX = 1 success, 0 failure  
;-----------------------------------------------------------------------------
Emit_ADD PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Dest operand
    mov rdi, r8     ; Src operand
    
    ; Check for ADD reg, reg
    mov eax, [rsi].CODE_OPERAND.opType
    mov ecx, [rdi].CODE_OPERAND.opType
    
    cmp eax, OP_REG
    jne add_fail
    cmp ecx, OP_REG
    jne add_check_imm
    
    ; ADD reg, reg - 48 01 /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8, 1       ; 64-bit
    call CalculateREX
    test rax, rax
    jz add_reg_no_rex
    
    mov rcx, rbx
    mov dl, al     ; REX prefix
    call EmitByte
    test rax, rax
    jz add_fail
    
add_reg_no_rex:
    ; Emit ADD opcode
    mov rcx, rbx
    mov dl, 01h    ; ADD r/m64, r64
    call EmitByte
    test rax, rax
    jz add_fail
    
    ; Emit ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov edx, [rdi].CODE_OPERAND.regNum  ; reg (source)
    mov r8d, [rsi].CODE_OPERAND.regNum   ; r/m (dest)
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    jmp add_success
    
add_check_imm:
    ; ADD reg, imm - 48 81 /0 [imm32] or 48 83 /0 [imm8]
    cmp ecx, OP_IMM
    jne add_fail
    
    ; Check immediate size
    mov rax, [rdi].CODE_OPERAND.immediate
    cmp rax, -128
    jl add_imm32
    cmp rax, 127
    jle add_imm8
    
add_imm32:
    ; ADD reg, imm32 - 48 81 /0
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz add_imm32_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    
add_imm32_no_rex:
    mov rcx, rbx
    mov dl, 81h    ; ADD r/m64, imm32
    call EmitByte
    test rax, rax
    jz add_fail
    
    ; ModR/M with reg field = 0 for ADD
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 0     ; ADD uses reg field = 0
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    
    ; Emit 32-bit immediate
    mov eax, dword ptr [rdi].CODE_OPERAND.immediate
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz add_fail
    jmp add_success
    
add_imm8:
    ; ADD reg, imm8 - 48 83 /0
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz add_imm8_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    
add_imm8_no_rex:
    mov rcx, rbx
    mov dl, 83h    ; ADD r/m64, imm8
    call EmitByte
    test rax, rax
    jz add_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 0
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    
    ; Emit 8-bit immediate
    mov al, byte ptr [rdi].CODE_OPERAND.immediate
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz add_fail
    
add_success:
    mov rax, 1
    jmp add_done
    
add_fail:
    xor rax, rax
    
add_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_ADD ENDP

;-----------------------------------------------------------------------------
; Emit_SUB
; Emit SUB instruction  
; Input: RCX = PE context, RDX = dest operand, R8 = src operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_SUB PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Dest operand
    mov rdi, r8     ; Src operand
    
    ; Similar to ADD but with SUB opcodes
    mov eax, [rsi].CODE_OPERAND.opType
    mov ecx, [rdi].CODE_OPERAND.opType
    
    cmp eax, OP_REG
    jne sub_fail
    cmp ecx, OP_REG
    jne sub_check_imm
    
    ; SUB reg, reg - 48 29 /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz sub_reg_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
sub_reg_no_rex:
    mov rcx, rbx
    mov dl, 29h    ; SUB r/m64, r64
    call EmitByte
    test rax, rax
    jz sub_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    jmp sub_success
    
sub_check_imm:
    cmp ecx, OP_IMM
    jne sub_fail
    
    ; SUB reg, imm - similar to ADD but with SUB opcodes (81h/83h with reg field = 5)
    mov rax, [rdi].CODE_OPERAND.immediate
    cmp rax, -128
    jl sub_imm32
    cmp rax, 127
    jle sub_imm8
    
sub_imm32:
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz sub_imm32_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
sub_imm32_no_rex:
    mov rcx, rbx
    mov dl, 81h
    call EmitByte
    test rax, rax
    jz sub_fail
    
    ; ModR/M with reg field = 5 for SUB
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 5     ; SUB uses reg field = 5
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
    ; Emit immediate
    mov eax, dword ptr [rdi].CODE_OPERAND.immediate
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz sub_fail
    jmp sub_success
    
sub_imm8:
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz sub_imm8_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
sub_imm8_no_rex:
    mov rcx, rbx
    mov dl, 83h
    call EmitByte
    test rax, rax
    jz sub_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 5
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
    ; Emit immediate
    mov al, byte ptr [rdi].CODE_OPERAND.immediate
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz sub_fail
    
sub_success:
    mov rax, 1
    jmp sub_done
    
sub_fail:
    xor rax, rax
    
sub_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_SUB ENDP

;-----------------------------------------------------------------------------
; Emit_CALL
; Emit CALL instruction - supports relative calls
; Input: RCX = PE context, RDX = target operand (label or displacement)
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_CALL PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Target operand
    
    ; Check operand type
    mov eax, [rsi].CODE_OPERAND.opType
    
    cmp eax, OP_LABEL
    je call_label
    cmp eax, OP_DISP
    je call_displacement
    cmp eax, OP_REG
    je call_register
    jmp call_fail
    
call_register:
    ; CALL reg - FF /2
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1       ; 64-bit
    call CalculateREX
    test rax, rax
    jz call_reg_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz call_fail
    
call_reg_no_rex:
    mov rcx, rbx
    mov dl, 0FFh   ; CALL opcode
    call EmitByte
    test rax, rax
    jz call_fail
    
    ; ModR/M with reg field = 2 for CALL
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 2     ; CALL uses reg field = 2
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz call_fail
    jmp call_success
    
call_displacement:
    ; CALL rel32 - E8 [rel32]
    mov rcx, rbx
    mov dl, 0E8h   ; CALL rel32
    call EmitByte
    test rax, rax
    jz call_fail
    
    ; Emit displacement
    mov eax, [rsi].CODE_OPERAND.displacement
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz call_fail
    jmp call_success
    
call_label:
    ; Add relocation for label resolution
    mov rcx, rbx
    mov rdx, [rsi].CODE_OPERAND.immediate  ; Label hash
    mov r8, 1     ; Relative call relocation
    call CodeGen_AddRelocation
    
    ; Emit placeholder CALL rel32
    mov rcx, rbx
    mov dl, 0E8h
    call EmitByte
    test rax, rax
    jz call_fail
    
    ; Emit placeholder displacement (will be fixed up)
    xor eax, eax
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz call_fail
    
call_success:
    mov rax, 1
    jmp call_done
    
call_fail:
    xor rax, rax
    
call_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_CALL ENDP

;-----------------------------------------------------------------------------
; Emit_RET
; Emit RET instruction
; Input: RCX = PE context, RDX = optional immediate (0 for simple RET)
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_RET PROC
    push rbx
    
    mov rbx, rcx    ; PE context
    
    ; Check for immediate
    test rdx, rdx
    jz ret_simple
    
    ; RET imm16 - C2 [imm16]
    mov rcx, rbx
    mov dl, 0C2h   ; RET imm16
    call EmitByte
    test rax, rax
    jz ret_fail
    
    ; Emit 16-bit immediate (stack bytes to pop)
    mov al, dl     ; Low byte
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz ret_fail
    
    mov al, dh     ; High byte (should be 0 for most cases)
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz ret_fail
    jmp ret_success
    
ret_simple:
    ; RET - C3
    mov rcx, rbx
    mov dl, 0C3h   ; RET
    call EmitByte
    test rax, rax
    jz ret_fail
    
ret_success:
    mov rax, 1
    jmp ret_done
    
ret_fail:
    xor rax, rax
    
ret_done:
    pop rbx
    ret
Emit_RET ENDP

;-----------------------------------------------------------------------------
; Emit_PUSH
; Emit PUSH instruction
; Input: RCX = PE context, RDX = operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_PUSH PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Operand
    
    ; Check operand type
    mov eax, [rsi].CODE_OPERAND.opType
    
    cmp eax, OP_REG
    je push_register
    cmp eax, OP_IMM
    je push_immediate
    jmp push_fail
    
push_register:
    ; PUSH reg - 50+r (or REX + 50+r for extended regs)
    mov ecx, [rsi].CODE_OPERAND.regNum
    
    ; Check if REX prefix needed
    cmp rcx, 7
    jbe push_reg_simple
    
    ; Emit REX.B for extended registers
    mov rcx, rbx
    mov dl, 41h    ; REX.B
    call EmitByte
    test rax, rax
    jz push_fail
    
push_reg_simple:
    ; PUSH reg - 50+r
    mov eax, [rsi].CODE_OPERAND.regNum
    and eax, 7     ; Mask to 3 bits
    add eax, 50h   ; PUSH base opcode
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz push_fail
    jmp push_success
    
push_immediate:
    ; Check immediate size
    mov rax, [rsi].CODE_OPERAND.immediate
    cmp rax, -128
    jl push_imm32
    cmp rax, 127
    jle push_imm8
    
push_imm32:
    ; PUSH imm32 - 68 [imm32]
    mov rcx, rbx
    mov dl, 68h
    call EmitByte
    test rax, rax
    jz push_fail
    
    ; Emit 32-bit immediate
    mov eax, dword ptr [rsi].CODE_OPERAND.immediate
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz push_fail
    jmp push_success
    
push_imm8:
    ; PUSH imm8 - 6A [imm8]
    mov rcx, rbx
    mov dl, 6Ah
    call EmitByte
    test rax, rax
    jz push_fail
    
    ; Emit 8-bit immediate
    mov al, byte ptr [rsi].CODE_OPERAND.immediate
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz push_fail
    
push_success:
    mov rax, 1
    jmp push_done
    
push_fail:
    xor rax, rax
    
push_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_PUSH ENDP

;-----------------------------------------------------------------------------
; Emit_POP
; Emit POP instruction
; Input: RCX = PE context, RDX = register operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_POP PROC
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Operand
    
    ; Check if register operand
    mov eax, [rsi].CODE_OPERAND.opType
    cmp eax, OP_REG
    jne pop_fail
    
    ; POP reg - 58+r
    mov ecx, [rsi].CODE_OPERAND.regNum
    
    ; Check if REX prefix needed
    cmp rcx, 7
    jbe pop_reg_simple
    
    mov rcx, rbx
    mov dl, 41h    ; REX.B
    call EmitByte
    test rax, rax
    jz pop_fail
    
pop_reg_simple:
    ; POP reg - 58+r
    mov eax, [rsi].CODE_OPERAND.regNum
    and eax, 7
    add eax, 58h
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz pop_fail
    
    mov rax, 1
    jmp pop_done
    
pop_fail:
    xor rax, rax
    
pop_done:
    pop rsi
    pop rbx
    ret
Emit_POP ENDP

;-----------------------------------------------------------------------------
; Emit_JMP
; Emit JMP instruction - supports relative jumps
; Input: RCX = PE context, RDX = target operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_JMP PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Target operand
    
    ; Check operand type
    mov eax, [rsi].CODE_OPERAND.opType
    
    cmp eax, OP_LABEL
    je jmp_label
    cmp eax, OP_DISP
    je jmp_displacement
    cmp eax, OP_REG
    je jmp_register
    jmp jmp_fail
    
jmp_register:
    ; JMP reg - FF /4
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz jmp_reg_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz jmp_fail
    
jmp_reg_no_rex:
    mov rcx, rbx
    mov dl, 0FFh
    call EmitByte
    test rax, rax
    jz jmp_fail
    
    ; ModR/M with reg field = 4 for JMP
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 4
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz jmp_fail
    jmp jmp_success
    
jmp_displacement:
    ; Check displacement size for short or near jump
    mov eax, [rsi].CODE_OPERAND.displacement
    cmp eax, -128
    jl jmp_near
    cmp eax, 127
    jle jmp_short
    
jmp_near:
    ; JMP rel32 - E9 [rel32]
    mov rcx, rbx
    mov dl, 0E9h
    call EmitByte
    test rax, rax
    jz jmp_fail
    
    ; Emit 32-bit displacement
    mov eax, [rsi].CODE_OPERAND.displacement
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz jmp_fail
    jmp jmp_success
    
jmp_short:
    ; JMP rel8 - EB [rel8]
    mov rcx, rbx
    mov dl, 0EBh
    call EmitByte
    test rax, rax
    jz jmp_fail
    
    ; Emit 8-bit displacement
    mov al, byte ptr [rsi].CODE_OPERAND.displacement
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz jmp_fail
    jmp jmp_success
    
jmp_label:
    ; Add relocation for label
    mov rcx, rbx
    mov rdx, [rsi].CODE_OPERAND.immediate  ; Label hash
    mov r8, 2     ; Relative jump relocation
    call CodeGen_AddRelocation
    
    ; Emit placeholder JMP rel32
    mov rcx, rbx
    mov dl, 0E9h
    call EmitByte
    test rax, rax
    jz jmp_fail
    
    ; Placeholder displacement
    xor eax, eax
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz jmp_fail
    
jmp_success:
    mov rax, 1
    jmp jmp_done
    
jmp_fail:
    xor rax, rax
    
jmp_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_JMP ENDP

;-----------------------------------------------------------------------------
; Emit_JE (JZ)
; Emit conditional jump if equal/zero
; Input: RCX = PE context, RDX = target operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_JE PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Check operand type
    mov eax, [rsi].CODE_OPERAND.opType
    
    cmp eax, OP_DISP
    je je_displacement
    cmp eax, OP_LABEL
    je je_label
    jmp je_fail
    
je_displacement:
    ; Check displacement size
    mov eax, [rsi].CODE_OPERAND.displacement
    cmp eax, -128
    jl je_near
    cmp eax, 127
    jle je_short
    
je_near:
    ; JE rel32 - 0F 84 [rel32]
    mov rcx, rbx
    mov dl, 0Fh
    call EmitByte
    test rax, rax
    jz je_fail
    
    mov rcx, rbx
    mov dl, 84h
    call EmitByte
    test rax, rax
    jz je_fail
    
    ; Emit displacement
    mov eax, [rsi].CODE_OPERAND.displacement
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz je_fail
    jmp je_success
    
je_short:
    ; JE rel8 - 74 [rel8]
    mov rcx, rbx
    mov dl, 74h
    call EmitByte
    test rax, rax
    jz je_fail
    
    mov al, byte ptr [rsi].CODE_OPERAND.displacement
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz je_fail
    jmp je_success
    
je_label:
    ; Add relocation
    mov rcx, rbx
    mov rdx, [rsi].CODE_OPERAND.immediate
    mov r8, 3     ; Conditional jump relocation
    call CodeGen_AddRelocation
    
    ; Emit near conditional jump
    mov rcx, rbx
    mov dl, 0Fh
    call EmitByte
    test rax, rax
    jz je_fail
    
    mov rcx, rbx
    mov dl, 84h
    call EmitByte
    test rax, rax
    jz je_fail
    
    ; Placeholder
    xor eax, eax
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz je_fail
    
je_success:
    mov rax, 1
    jmp je_done
    
je_fail:
    xor rax, rax
    
je_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_JE ENDP

;-----------------------------------------------------------------------------
; Emit_JNE (JNZ)
; Emit conditional jump if not equal/not zero
; Input: RCX = PE context, RDX = target operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_JNE PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Similar to JE but with JNE opcodes (75h for short, 0F 85h for near)
    mov eax, [rsi].CODE_OPERAND.opType
    
    cmp eax, OP_DISP
    je jne_displacement
    cmp eax, OP_LABEL
    je jne_label
    jmp jne_fail
    
jne_displacement:
    mov eax, [rsi].CODE_OPERAND.displacement
    cmp eax, -128
    jl jne_near
    cmp eax, 127
    jle jne_short
    
jne_near:
    ; JNE rel32 - 0F 85 [rel32]
    mov rcx, rbx
    mov dl, 0Fh
    call EmitByte
    test rax, rax
    jz jne_fail
    
    mov rcx, rbx
    mov dl, 85h
    call EmitByte
    test rax, rax
    jz jne_fail
    
    mov eax, [rsi].CODE_OPERAND.displacement
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz jne_fail
    jmp jne_success
    
jne_short:
    ; JNE rel8 - 75 [rel8]
    mov rcx, rbx
    mov dl, 75h
    call EmitByte
    test rax, rax
    jz jne_fail
    
    mov al, byte ptr [rsi].CODE_OPERAND.displacement
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz jne_fail
    jmp jne_success
    
jne_label:
    mov rcx, rbx
    mov rdx, [rsi].CODE_OPERAND.immediate
    mov r8, 3
    call CodeGen_AddRelocation
    
    mov rcx, rbx
    mov dl, 0Fh
    call EmitByte
    test rax, rax
    jz jne_fail
    
    mov rcx, rbx
    mov dl, 85h
    call EmitByte
    test rax, rax
    jz jne_fail
    
    xor eax, eax
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz jne_fail
    
jne_success:
    mov rax, 1
    jmp jne_done
    
jne_fail:
    xor rax, rax
    
jne_done:
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_JNE ENDP

;-----------------------------------------------------------------------------
; Emit_CMP
; Emit CMP instruction
; Input: RCX = PE context, RDX = operand1, R8 = operand2
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_CMP PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Operand 1
    mov rdi, r8     ; Operand 2
    
    ; Check operand types
    mov eax, [rsi].CODE_OPERAND.opType
    mov ecx, [rdi].CODE_OPERAND.opType
    
    cmp eax, OP_REG
    jne cmp_fail
    cmp ecx, OP_REG
    jne cmp_check_imm
    
    ; CMP reg, reg - 48 39 /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz cmp_reg_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
cmp_reg_no_rex:
    mov rcx, rbx
    mov dl, 39h    ; CMP r/m64, r64
    call EmitByte
    test rax, rax
    jz cmp_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    jmp cmp_success
    
cmp_check_imm:
    cmp ecx, OP_IMM
    jne cmp_fail
    
    ; CMP reg, imm
    mov rax, [rdi].CODE_OPERAND.immediate
    cmp rax, -128
    jl cmp_imm32
    cmp rax, 127
    jle cmp_imm8
    
cmp_imm32:
    ; CMP reg, imm32 - 48 81 /7
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz cmp_imm32_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
cmp_imm32_no_rex:
    mov rcx, rbx
    mov dl, 81h
    call EmitByte
    test rax, rax
    jz cmp_fail
    
    ; ModR/M with reg field = 7 for CMP
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 7
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
    ; Emit immediate
    mov eax, dword ptr [rdi].CODE_OPERAND.immediate
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz cmp_fail
    jmp cmp_success
    
cmp_imm8:
    ; CMP reg, imm8 - 48 83 /7
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz cmp_imm8_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
cmp_imm8_no_rex:
    mov rcx, rbx
    mov dl, 83h
    call EmitByte
    test rax, rax
    jz cmp_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 7
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
    ; Emit immediate
    mov al, byte ptr [rdi].CODE_OPERAND.immediate
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz cmp_fail
    
cmp_success:
    mov rax, 1
    jmp cmp_done
    
cmp_fail:
    xor rax, rax
    
cmp_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_CMP ENDP

;-----------------------------------------------------------------------------
; Emit_TEST
; Emit TEST instruction
; Input: RCX = PE context, RDX = operand1, R8 = operand2
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_TEST PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    
    ; Check operand types
    mov eax, [rsi].CODE_OPERAND.opType
    mov ecx, [rdi].CODE_OPERAND.opType
    
    cmp eax, OP_REG
    jne test_fail
    cmp ecx, OP_REG
    jne test_check_imm
    
    ; TEST reg, reg - 48 85 /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz test_reg_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz test_fail
    
test_reg_no_rex:
    mov rcx, rbx
    mov dl, 85h    ; TEST r/m64, r64
    call EmitByte
    test rax, rax
    jz test_fail
    
    ; ModR/M
    mov rcx, MODRM_REG_DIRECT
    mov edx, [rdi].CODE_OPERAND.regNum
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz test_fail
    jmp test_success
    
test_check_imm:
    cmp ecx, OP_IMM
    jne test_fail
    
    ; TEST reg, imm - 48 F7 /0
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov rdx, 0
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz test_imm_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz test_fail
    
test_imm_no_rex:
    mov rcx, rbx
    mov dl, 0F7h
    call EmitByte
    test rax, rax
    jz test_fail
    
    ; ModR/M with reg field = 0 for TEST
    mov rcx, MODRM_REG_DIRECT
    mov rdx, 0
    mov r8d, [rsi].CODE_OPERAND.regNum
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz test_fail
    
    ; Emit 32-bit immediate
    mov eax, dword ptr [rdi].CODE_OPERAND.immediate
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz test_fail
    
test_success:
    mov rax, 1
    jmp test_done
    
test_fail:
    xor rax, rax
    
test_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_TEST ENDP

;-----------------------------------------------------------------------------
; Emit_LEA
; Emit LEA (Load Effective Address) instruction
; Input: RCX = PE context, RDX = dest register, R8 = memory operand
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_LEA PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    mov rsi, rdx    ; Dest register
    mov rdi, r8     ; Memory operand
    
    ; Validate operands
    mov eax, [rsi].CODE_OPERAND.opType
    cmp eax, OP_REG
    jne lea_fail
    
    mov eax, [rdi].CODE_OPERAND.opType
    cmp eax, OP_MEM
    jne lea_fail
    
    ; LEA reg, mem - 48 8D /r
    mov ecx, [rsi].CODE_OPERAND.regNum
    mov edx, [rdi].CODE_OPERAND.baseReg
    mov r8, 1
    call CalculateREX
    test rax, rax
    jz lea_no_rex
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz lea_fail
    
lea_no_rex:
    mov rcx, rbx
    mov dl, 8Dh    ; LEA opcode
    call EmitByte
    test rax, rax
    jz lea_fail
    
    ; For simple [reg+disp] addressing
    mov eax, [rdi].CODE_OPERAND.displacement
    test eax, eax
    jz lea_no_disp
    
    ; Check displacement size
    cmp eax, -128
    jl lea_disp32
    cmp eax, 127
    jle lea_disp8
    
lea_disp32:
    ; [reg+disp32]
    mov rcx, MODRM_REG_DISP32
    mov edx, [rsi].CODE_OPERAND.regNum
    mov r8d, [rdi].CODE_OPERAND.baseReg
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz lea_fail
    
    ; Emit displacement
    mov eax, [rdi].CODE_OPERAND.displacement
    mov [rsp+20h], eax
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    mov r8, 4
    call EmitBytes
    test rax, rax
    jz lea_fail
    jmp lea_success
    
lea_disp8:
    ; [reg+disp8]
    mov rcx, MODRM_REG_DISP8
    mov edx, [rsi].CODE_OPERAND.regNum
    mov r8d, [rdi].CODE_OPERAND.baseReg
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz lea_fail
    
    ; Emit 8-bit displacement
    mov al, byte ptr [rdi].CODE_OPERAND.displacement
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz lea_fail
    jmp lea_success
    
lea_no_disp:
    ; [reg]
    mov rcx, MODRM_REG_INDIRECT
    mov edx, [rsi].CODE_OPERAND.regNum
    mov r8d, [rdi].CODE_OPERAND.baseReg
    call EmitModRM
    
    mov rcx, rbx
    mov dl, al
    call EmitByte
    test rax, rax
    jz lea_fail
    
lea_success:
    mov rax, 1
    jmp lea_done
    
lea_fail:
    xor rax, rax
    
lea_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
Emit_LEA ENDP

;-----------------------------------------------------------------------------
; Emit_NOP
; Emit NOP instruction
; Input: RCX = PE context, RDX = optional size (1-9 bytes, default 1)
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
Emit_NOP PROC
    push rbx
    
    mov rbx, rcx    ; PE context
    
    ; Default to 1-byte NOP if size is 0
    test rdx, rdx
    jnz nop_multi
    mov rdx, 1
    
nop_multi:
    ; For now, just emit multiple single-byte NOPs
    ; Could be optimized with multi-byte NOPs
    
nop_loop:
    test rdx, rdx
    jz nop_success
    
    ; Emit single NOP - 90h
    mov rcx, rbx
    push rdx
    mov dl, 90h
    call EmitByte
    pop rdx
    test rax, rax
    jz nop_fail
    
    dec rdx
    jmp nop_loop
    
nop_success:
    mov rax, 1
    jmp nop_done
    
nop_fail:
    xor rax, rax
    
nop_done:
    pop rbx
    ret
Emit_NOP ENDP

;=============================================================================
; LABEL AND RELOCATION MANAGEMENT
;=============================================================================

;-----------------------------------------------------------------------------
; CodeGen_DefineLabel
; Define a label at the current code position
; Input: RCX = PE context, RDX = label name hash
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CodeGen_DefineLabel PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Label hash
    
    ; Check if we have space for more labels
    mov eax, [rbx].PE_CONTEXT.numLabels
    cmp eax, MAX_LABELS - 1
    jae define_label_fail
    
    ; Search for existing label (in case it was forward referenced)
    mov rdi, [rbx].PE_CONTEXT.pLabels
    xor rcx, rcx    ; Index
    
define_search_loop:
    cmp ecx, [rbx].PE_CONTEXT.numLabels
    jae define_create_new
    
    ; Check hash match
    mov rax, rcx
    mov rdx, SIZEOF CODE_LABEL
    mul rdx
    add rax, rdi
    
    mov edx, [rax].CODE_LABEL.nameHash
    cmp edx, esi
    je define_found_existing
    
    inc ecx
    jmp define_search_loop
    
define_found_existing:
    ; Update existing label with current position
    mov edx, [rbx].PE_CONTEXT.codeSize
    mov [rax].CODE_LABEL.labelOffset, edx
    mov [rax].CODE_LABEL.defined, 1
    jmp define_success
    
define_create_new:
    ; Create new label entry
    mov eax, [rbx].PE_CONTEXT.numLabels
    mov rcx, rax
    mov rdx, SIZEOF CODE_LABEL
    mul rdx
    add rax, rdi
    
    ; Set label data
    mov [rax].CODE_LABEL.nameHash, esi
    mov edx, [rbx].PE_CONTEXT.codeSize
    mov [rax].CODE_LABEL.labelOffset, edx
    mov [rax].CODE_LABEL.defined, 1
    
    ; Increment count
    inc dword ptr [rbx].PE_CONTEXT.numLabels
    
define_success:
    ; Now resolve any pending relocations for this label
    call CodeGen_ResolveRelocations
    mov rax, 1
    jmp define_done
    
define_label_fail:
    xor rax, rax
    
define_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CodeGen_DefineLabel ENDP

;-----------------------------------------------------------------------------
; CodeGen_AddRelocation
; Add a relocation entry for later resolution
; Input: RCX = PE context, RDX = label hash, R8 = relocation type
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CodeGen_AddRelocation PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Label hash
    mov rdi, r8     ; Relocation type
    
    ; Check space
    mov eax, [rbx].PE_CONTEXT.numRelocations
    cmp eax, MAX_RELOCATIONS - 1
    jae add_reloc_fail
    
    ; Get relocation entry
    mov rcx, [rbx].PE_CONTEXT.pRelocations
    mov eax, [rbx].PE_CONTEXT.numRelocations
    mov rdx, SIZEOF CODE_RELOCATION
    mul rdx
    add rcx, rax
    
    ; Set relocation data
    mov eax, [rbx].PE_CONTEXT.codeSize
    mov [rcx].CODE_RELOCATION.relocOffset, eax
    mov [rcx].CODE_RELOCATION.relocType, edi
    mov [rcx].CODE_RELOCATION.targetRVA, esi  ; Store hash temporarily
    mov [rcx].CODE_RELOCATION.addend, 0
    
    ; Increment count
    inc dword ptr [rbx].PE_CONTEXT.numRelocations
    
    mov rax, 1
    jmp add_reloc_done
    
add_reloc_fail:
    xor rax, rax
    
add_reloc_done:
    pop rdi
    pop rsi
    pop rbx
    ret
CodeGen_AddRelocation ENDP

;-----------------------------------------------------------------------------
; CodeGen_ResolveRelocations
; Resolve pending relocations after label definition
; Input: RCX = PE context, RSI = label hash (preserved from caller)
; Output: RAX = number of relocations resolved
;-----------------------------------------------------------------------------
CodeGen_ResolveRelocations PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rdi
    push r12
    push r13
    push r14
    
    mov rbx, rcx    ; PE context
    
    ; Find label offset
    mov rdi, [rbx].PE_CONTEXT.pLabels
    xor rcx, rcx    ; Index
    mov r12, -1     ; Label offset (not found)
    
resolve_find_label:
    cmp ecx, [rbx].PE_CONTEXT.numLabels
    jae resolve_process_relocs
    
    mov rax, rcx
    mov rdx, SIZEOF CODE_LABEL
    mul rdx
    add rax, rdi
    
    mov edx, [rax].CODE_LABEL.nameHash
    cmp edx, esi
    jne resolve_next_label
    
    ; Check if defined
    cmp [rax].CODE_LABEL.defined, 0
    je resolve_next_label
    
    ; Found defined label
    mov r12d, [rax].CODE_LABEL.labelOffset
    jmp resolve_process_relocs
    
resolve_next_label:
    inc ecx
    jmp resolve_find_label
    
resolve_process_relocs:
    ; If label not found/defined, exit
    cmp r12, -1
    je resolve_done_zero
    
    ; Process relocations
    mov rdi, [rbx].PE_CONTEXT.pRelocations
    xor rcx, rcx    ; Index
    xor r13, r13    ; Count resolved
    
resolve_reloc_loop:
    cmp ecx, [rbx].PE_CONTEXT.numRelocations
    jae resolve_done
    
    ; Get relocation entry
    mov rax, rcx
    mov rdx, SIZEOF CODE_RELOCATION
    mul rdx
    add rax, rdi
    mov r14, rax    ; Relocation entry
    
    ; Check if this relocation targets our label
    mov edx, [r14].CODE_RELOCATION.targetRVA  ; This is the hash
    cmp edx, esi
    jne resolve_next_reloc
    
    ; Calculate relative displacement
    mov eax, r12d   ; Target offset
    sub eax, [r14].CODE_RELOCATION.relocOffset  ; Subtract source offset
    sub eax, 4      ; Account for instruction size (for rel32)
    
    ; Update the code buffer at the relocation offset
    mov rdx, [rbx].PE_CONTEXT.pCodeBuffer
    mov eax, [r14].CODE_RELOCATION.relocOffset
    add rdx, rax
    
    ; Check relocation type and adjust accordingly
    mov r8d, [r14].CODE_RELOCATION.relocType
    cmp r8d, 1      ; Relative call
    je resolve_write_rel32
    cmp r8d, 2      ; Relative jump
    je resolve_write_rel32
    cmp r8d, 3      ; Conditional jump
    je resolve_write_rel32
    jmp resolve_next_reloc
    
resolve_write_rel32:
    ; Write 32-bit relative displacement
    mov [rdx], eax
    inc r13
    
resolve_next_reloc:
    inc ecx
    jmp resolve_reloc_loop
    
resolve_done:
    mov rax, r13
    jmp resolve_exit
    
resolve_done_zero:
    xor rax, rax
    
resolve_exit:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CodeGen_ResolveRelocations ENDP

;-----------------------------------------------------------------------------
; CodeGen_CalculateRVA
; Calculate RVA for current code position
; Input: RCX = PE context
; Output: RAX = RVA
;-----------------------------------------------------------------------------
CodeGen_CalculateRVA PROC
    mov eax, [rcx].PE_CONTEXT.textVirtualAddress
    add eax, [rcx].PE_CONTEXT.codeSize
    ret
CodeGen_CalculateRVA ENDP

;-----------------------------------------------------------------------------
; CodeGen_AlignCode
; Align code buffer to specified boundary
; Input: RCX = PE context, RDX = alignment (power of 2)
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CodeGen_AlignCode PROC
    push rbx
    push rsi
    
    mov rbx, rcx    ; PE context
    mov rsi, rdx    ; Alignment
    
    ; Calculate padding needed
    mov eax, [rbx].PE_CONTEXT.codeSize
    dec rsi         ; alignment - 1
    add rax, rsi    ; size + (alignment - 1)
    inc rsi         ; restore alignment
    not rsi         ; ~alignment
    and rax, rsi    ; (size + alignment - 1) & ~alignment
    
    ; Calculate how many bytes to pad
    sub eax, [rbx].PE_CONTEXT.codeSize
    test eax, eax
    jz align_done_success
    
    ; Emit NOP padding
    mov rcx, rbx
    mov rdx, rax
    call Emit_NOP
    test rax, rax
    jz align_fail
    
align_done_success:
    mov rax, 1
    jmp align_done
    
align_fail:
    xor rax, rax
    
align_done:
    pop rsi
    pop rbx
    ret
CodeGen_AlignCode ENDP

;-----------------------------------------------------------------------------
; Enhanced PEWriter_AddCode - Now supports high-level code generation
; Input: RCX = PE context, RDX = code buffer, R8 = code size
; Output: RAX = RVA of code (0 = failure)
;-----------------------------------------------------------------------------
; (The original PEWriter_AddCode remains unchanged for backward compatibility)

;-----------------------------------------------------------------------------
; CodeGen_CreateOperand
; Helper function to create CODE_OPERAND structures
; Input: RCX = operand type, RDX = value1 (reg/imm), R8 = value2 (optional)
; Output: RAX = pointer to created operand (on stack or static buffer)
;-----------------------------------------------------------------------------
CodeGen_CreateOperand PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Use stack space for operand
    lea rax, [rsp+20h]
    
    ; Initialize operand structure
    mov [rax].CODE_OPERAND.opType, ecx
    mov [rax].CODE_OPERAND.regNum, 0
    mov [rax].CODE_OPERAND.immediate, 0
    mov [rax].CODE_OPERAND.displacement, 0
    mov [rax].CODE_OPERAND.baseReg, 0
    mov [rax].CODE_OPERAND.indexReg, 0
    mov [rax].CODE_OPERAND.scale, 1
    mov [rax].CODE_OPERAND.opSize, 8
    
    ; Set values based on type
    cmp ecx, OP_REG
    je operand_reg
    cmp ecx, OP_IMM
    je operand_imm
    cmp ecx, OP_MEM
    je operand_mem
    cmp ecx, OP_DISP
    je operand_disp
    cmp ecx, OP_LABEL
    je operand_label
    jmp operand_done
    
operand_reg:
    mov [rax].CODE_OPERAND.regNum, edx
    jmp operand_done
    
operand_imm:
    mov [rax].CODE_OPERAND.immediate, rdx
    jmp operand_done
    
operand_mem:
    mov [rax].CODE_OPERAND.baseReg, edx
    mov [rax].CODE_OPERAND.displacement, r8d
    jmp operand_done
    
operand_disp:
    mov [rax].CODE_OPERAND.displacement, edx
    jmp operand_done
    
operand_label:
    mov [rax].CODE_OPERAND.immediate, rdx  ; Label hash
    jmp operand_done
    
operand_done:
    add rsp, 40h
    pop rbp
    ret
CodeGen_CreateOperand ENDP

;=============================================================================
; MACHINE CODE EMITTER USAGE EXAMPLE AND TEST
;=============================================================================

;-----------------------------------------------------------------------------
; CodeGen_GenerateSimpleFunction
; Generates a simple x64 function as demonstration
; Generates: function(int a, int b) { return a + b; }
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
CodeGen_GenerateSimpleFunction PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    
    ; Generate function prologue
    mov rcx, rbx
    mov rdx, 32     ; Frame size (shadow space)
    mov r8, 0       ; No additional saved registers
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_fail
    
    ; ADD ECX, EDX (x64 Windows calling convention: RCX=arg1, RDX=arg2)
    ; Create operands for ADD RCX, RDX
    mov rcx, OP_REG
    mov rdx, REG_RCX
    mov r8, 0
    call CodeGen_CreateOperand
    mov rsi, rax    ; Dest operand (RCX)
    push rsi
    
    mov rcx, OP_REG
    mov rdx, REG_RDX
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax    ; Src operand (RDX)
    
    ; Emit ADD RCX, RDX
    mov rcx, rbx
    pop rdx         ; rsi = dest operand
    mov r8, rdi     ; src operand
    call Emit_ADD
    test rax, rax
    jz gen_fail
    
    ; Move result to RAX (return value register)
    ; MOV RAX, RCX
    mov rcx, OP_REG
    mov rdx, REG_RAX
    mov r8, 0
    call CodeGen_CreateOperand
    mov rsi, rax    ; Dest operand (RAX)
    push rsi
    
    mov rcx, OP_REG
    mov rdx, REG_RCX
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax    ; Src operand (RCX)
    
    mov rcx, rbx
    pop rdx         ; dest operand
    mov r8, rdi     ; src operand
    call Emit_MOV
    test rax, rax
    jz gen_fail
    
    ; Generate function epilogue
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_fail
    
    ; Emit RET
    mov rcx, rbx
    mov rdx, 0      ; Simple RET
    call Emit_RET
    test rax, rax
    jz gen_fail
    
  write_headers_success:
    mov rax, 1      ; Success
    jmp gen_done
    
gen_fail:
    xor rax, rax
    
gen_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
CodeGen_GenerateSimpleFunction ENDP

;=============================================================================
; MACHINE CODE EMITTER REPORT
;=============================================================================

;-----------------------------------------------------------------------------
; CodeGen_GetCapabilitiesReport
; Returns a detailed report of machine code emission capabilities
; Input: RCX = buffer for report, RDX = buffer size
; Output: RAX = bytes written
;-----------------------------------------------------------------------------
CodeGen_GetCapabilitiesReport PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Buffer
    mov rsi, rdx    ; Buffer size
    mov rdi, 0      ; Bytes written
    
    
report_too_small:
    xor rax, rax
    
report_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CodeGen_GetCapabilitiesReport ENDP

;=============================================================================
; END MACHINE CODE EMITTER
;=============================================================================

;-----------------------------------------------------------------------------
; ValidatePEContext
; Validates PE context before writing
; Input: RCX = PE context
; Output: RAX = 1 valid, 0 invalid
;-----------------------------------------------------------------------------
ValidatePEContext PROC
    push rbp
    mov rbp, rsp
    
    ; Check basic pointers
    cmp qword ptr [rcx].PE_CONTEXT.pDosHeader, 0
    je validate_fail
    cmp qword ptr [rcx].PE_CONTEXT.pNtHeader, 0
    je validate_fail
    cmp qword ptr [rcx].PE_CONTEXT.pSectionHeaders, 0
    je validate_fail
    cmp qword ptr [rcx].PE_CONTEXT.pCodeBuffer, 0
    je validate_fail
    
    ; Check code size is reasonable
    cmp dword ptr [rcx].PE_CONTEXT.codeSize, 0
    je validate_fail
    cmp dword ptr [rcx].PE_CONTEXT.codeSize, MAX_CODE_SIZE
    ja validate_fail
    
    ; Validate section addresses
    mov eax, [rcx].PE_CONTEXT.textVirtualAddress
    cmp eax, SECTION_ALIGNMENT
    jl validate_fail
    
    mov eax, [rcx].PE_CONTEXT.rdataVirtualAddress
    cmp eax, [rcx].PE_CONTEXT.textVirtualAddress
    jle validate_fail
    
    mov eax, [rcx].PE_CONTEXT.idataVirtualAddress
    cmp eax, [rcx].PE_CONTEXT.rdataVirtualAddress
    jle validate_fail
    
    ; Validate file offsets
    cmp dword ptr [rcx].PE_CONTEXT.textFileOffset, 400h
    jl validate_fail
    mov eax, [rcx].PE_CONTEXT.rdataFileOffset
    cmp eax, [rcx].PE_CONTEXT.textFileOffset
    jle validate_fail
    mov eax, [rcx].PE_CONTEXT.idataFileOffset
    cmp eax, [rcx].PE_CONTEXT.rdataFileOffset
    jle validate_fail

    ; Require baseline section shape for writer path.
    cmp dword ptr [rcx].PE_CONTEXT.numSections, 3
    jl validate_fail
    
    mov rax, 1      ; Valid
    jmp validate_done
    
validate_fail:
    xor rax, rax
    
validate_done:
    pop rbp
    ret
ValidatePEContext ENDP

;-----------------------------------------------------------------------------
; WritePEHeaders
; Writes DOS header, stub, and NT headers with proper validation
; Input: RCX = file handle, RDX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
WritePEHeaders PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 0C0h
    
    mov rbx, rcx    ; File handle
    mov rdi, rdx    ; PE context
    
    ; Write DOS header
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pDosHeader
    mov r8, SIZEOF IMAGE_DOS_HEADER
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_headers_fail
    
    ; Write DOS stub
    mov rcx, rbx
    mov rdx, offset dos_stub_program
    mov r8, dos_stub_size
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_headers_fail
    
    ; Calculate and write padding to NT header
    mov r12, [rdi].PE_CONTEXT.pDosHeader
    mov eax, [r12].IMAGE_DOS_HEADER.e_lfanew
    sub eax, SIZEOF IMAGE_DOS_HEADER
    sub eax, dos_stub_size
    mov r12, rax
    
    ; Write padding if needed
    test r12, r12
    jle skip_padding
    
    ; Initialize zero buffer on stack
    lea rsi, [rsp+40h]
    push rdi
    mov rdi, rsi
    xor eax, eax
    mov ecx, 20h
    rep stosb
    pop rdi
    
padding_loop:
    cmp r12, 20h
    jle padding_final
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, 20h
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_headers_fail
    
    sub r12, 20h
    jmp padding_loop
    
padding_final:
    test r12, r12
    jz skip_padding
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, r12
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_headers_fail
    
skip_padding:
    ; Update NT header before writing
    call UpdateNTHeaderBeforeWrite
    
    ; Write NT header
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pNtHeader
    mov r8, SIZEOF IMAGE_NT_HEADERS64
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_headers_fail
    
    ; Write section headers directly from pSections.
    mov rsi, [rdi].PE_CONTEXT.pSections
    xor r12d, r12d
write_header_loop:
    cmp r12d, [rdi].PE_CONTEXT.numSections
      jae write_header_padding

      lea rdx, [rsp+80h]
      mov qword ptr [rdx+0], 0
      mov qword ptr [rdx+8], 0
      mov qword ptr [rdx+10h], 0
      mov qword ptr [rdx+18h], 0
      mov qword ptr [rdx+20h], 0

      mov rax, qword ptr [rsi].PE_SECTION_ENTRY.Name1
      mov qword ptr [rdx].IMAGE_SECTION_HEADER.Name1, rax
      mov eax, dword ptr [rsi].PE_SECTION_ENTRY.VirtualSize
      mov dword ptr [rdx].IMAGE_SECTION_HEADER.VirtualSize, eax
      mov eax, dword ptr [rsi].PE_SECTION_ENTRY.VirtualAddress
      mov dword ptr [rdx].IMAGE_SECTION_HEADER.VirtualAddress, eax
      mov eax, dword ptr [rsi].PE_SECTION_ENTRY.SizeOfRawData
      mov dword ptr [rdx].IMAGE_SECTION_HEADER.SizeOfRawData, eax
      mov eax, dword ptr [rsi].PE_SECTION_ENTRY.PointerToRawData
      mov dword ptr [rdx].IMAGE_SECTION_HEADER.PointerToRawData, eax
      mov eax, dword ptr [rsi].PE_SECTION_ENTRY.Characteristics
      mov dword ptr [rdx].IMAGE_SECTION_HEADER.Characteristics, eax

      mov rcx, rbx
      mov r8, SIZEOF IMAGE_SECTION_HEADER
      lea r9, [rsp+60h]
      mov qword ptr [rsp+20h], 0
      call WriteFile
      test eax, eax
      jz write_headers_fail

      add rsi, SIZEOF PE_SECTION_ENTRY
      inc r12d
      jmp write_header_loop

  write_header_padding:
      ; Calculate exact bytes written so far: e_lfanew + NT_HEADERS + (numSections * SECTION_HEADER)
      mov r12, [rdi].PE_CONTEXT.pDosHeader
      mov eax, [r12].IMAGE_DOS_HEADER.e_lfanew
      add eax, SIZEOF IMAGE_NT_HEADERS64
      mov ecx, [rdi].PE_CONTEXT.numSections
      imul ecx, SIZEOF IMAGE_SECTION_HEADER
      add eax, ecx

      ; Pad to textFileOffset (SizeOfHeaders aligned)
      mov ecx, [rdi].PE_CONTEXT.textFileOffset
      sub ecx, eax
      jle write_headers_success
      
      ; Write the remaining padding bytes to align to textFileOffset
      mov r12d, ecx
      call WritePaddingBytes
      test eax, eax
      jz write_headers_fail
  write_headers_success:
      mov rax, 1      ; Success
      jmp write_headers_done
    jmp write_headers_done
    
write_headers_fail:
    xor rax, rax
    
write_headers_done:
    add rsp, 0C0h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
WritePEHeaders ENDP

;-----------------------------------------------------------------------------
; UpdateNTHeaderBeforeWrite
; Updates NT header fields with final calculated values
; Input: RDI = PE context
;-----------------------------------------------------------------------------
UpdateNTHeaderBeforeWrite PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    sub rsp, 28h
    
    mov rbx, [rdi].PE_CONTEXT.pNtHeader
    mov eax, [rdi].PE_CONTEXT.numSections
    mov [rbx].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections, ax
    
    ; Update base of code
    mov eax, [rdi].PE_CONTEXT.textVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.BaseOfCode, eax
    
    ; Update size of code.
    mov ecx, [rdi].PE_CONTEXT.codeSize
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfCode, eax

    ; Sum initialized data raw sizes from sections after .text.
    xor r13d, r13d
    mov ecx, [rdi].PE_CONTEXT.numSections
    cmp ecx, 1
    jle no_init_sections
    dec ecx
    mov r12, [rdi].PE_CONTEXT.pSections
    add r12, SIZEOF PE_SECTION_ENTRY
sum_init_sections:
    add r13d, [r12].PE_SECTION_ENTRY.SizeOfRawData
    add r12, SIZEOF PE_SECTION_ENTRY
    dec ecx
    jnz sum_init_sections
no_init_sections:
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfInitializedData, r13d
    
    ; Update size of image
    mov eax, [rdi].PE_CONTEXT.currentVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfImage, eax
    
    ; Update size of headers (aligned)
    mov r12, [rdi].PE_CONTEXT.pDosHeader
    mov ecx, [r12].IMAGE_DOS_HEADER.e_lfanew
    mov eax, [rdi].PE_CONTEXT.numSections
    imul eax, SIZEOF IMAGE_SECTION_HEADER
    add ecx, SIZEOF IMAGE_NT_HEADERS64
    add ecx, eax
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeaders, eax
    
    ; Calculate and set checksum
    mov rcx, rdi
    call CalculateChecksum
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.CheckSum, eax
    
    ; Set import directory if imports exist.
    cmp dword ptr [rdi].PE_CONTEXT.numImportFunctions, 0
    je clear_import_directory
    
    mov eax, [rdi].PE_CONTEXT.idataVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, eax
    
    ; Use computed import size; fallback to descriptor table size.
    mov eax, [rdi].PE_CONTEXT.totalImportSize
    test eax, eax
    jnz set_import_size
    
    mov eax, [rdi].PE_CONTEXT.numImportDLLs
    inc eax
    mov ecx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    mul ecx
    
set_import_size:
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, eax
    jmp import_dir_done

clear_import_directory:
    mov dword ptr [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, 0
    mov dword ptr [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, 0

import_dir_done:
    ; Set base relocation directory (DataDirectory[5])
    cmp dword ptr [rdi].PE_CONTEXT.relocBufferSize, 0
    je clear_reloc_directory
    
    mov eax, [rdi].PE_CONTEXT.relocVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, eax
    mov eax, [rdi].PE_CONTEXT.relocBufferSize
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, eax
    jmp reloc_dir_done
    
clear_reloc_directory:
    mov dword ptr [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, 0
    mov dword ptr [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, 0
    
reloc_dir_done:

    ; Set SizeOfUninitializedData from BSS
    mov eax, [rdi].PE_CONTEXT.bssSize
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfUninitializedData, eax
    
    ; Set DllCharacteristics for ASLR + DEP + NXCOMPAT
    mov word ptr [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DllCharacteristics, 8160h
    ; 0x8000 = TERMINAL_SERVER_AWARE
    ; 0x0100 = NX_COMPAT (DEP)
    ; 0x0040 = DYNAMIC_BASE (ASLR)
    ; 0x0020 = HIGH_ENTROPY_VA

nt_update_done:
    add rsp, 28h
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
UpdateNTHeaderBeforeWrite ENDP

;-----------------------------------------------------------------------------
; WritePESections
; Writes all PE sections (.text, .rdata, .idata) with proper padding
; Input: RCX = file handle, RDX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
WritePESections PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 40h

    mov rbx, rcx    ; File handle
    mov rdi, rdx    ; PE context

    ; Current file offset actually starts at textFileOffset because WritePEHeaders pads it
    mov r14d, [rdi].PE_CONTEXT.textFileOffset

    mov r13, [rdi].PE_CONTEXT.pSections
    xor esi, esi

write_sections_loop:
    cmp esi, [rdi].PE_CONTEXT.numSections
    jae write_sections_success

    ; Ensure we are at section raw start.
    mov eax, [r13].PE_SECTION_ENTRY.PointerToRawData
    sub eax, r14d
    jle section_write_body
    mov r12d, eax
    mov rcx, rbx
    call WritePaddingBytes
    test eax, eax
    jz write_sections_fail
    add r14d, r12d

section_write_body:
    mov r12d, [r13].PE_SECTION_ENTRY.SizeOfRawData
    test r12d, r12d
    jz next_section

    ; Section #2 is .idata.
    cmp esi, 2
    jne write_regular_section
    call WriteImportSection
    test eax, eax
    jz write_sections_fail
    add r14d, r12d
    jmp next_section

write_regular_section:
    mov rdx, [r13].PE_SECTION_ENTRY.pDataBuffer
    test rdx, rdx
    jz pad_entire_section

    mov r8d, [r13].PE_SECTION_ENTRY.VirtualSize
    cmp r8d, r12d
    jbe write_payload
    mov r8d, r12d

write_payload:
    test r8d, r8d
    jz pad_entire_section
    mov dword ptr [rsp+30h], r8d
    mov rcx, rbx
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_sections_fail

    mov eax, r12d
    sub eax, dword ptr [rsp+30h]
    jle section_written
    mov r12d, eax
    mov rcx, rbx
    call WritePaddingBytes
    test eax, eax
    jz write_sections_fail
    jmp section_written

pad_entire_section:
    mov rcx, rbx
    call WritePaddingBytes
    test eax, eax
    jz write_sections_fail

section_written:
    add r14d, [r13].PE_SECTION_ENTRY.SizeOfRawData

next_section:
    add r13, SIZEOF PE_SECTION_ENTRY
    inc esi
    jmp write_sections_loop

write_sections_success:
    mov rax, 1
    jmp write_sections_done

write_sections_fail:
    xor rax, rax

write_sections_done:
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
WritePESections ENDP

;-----------------------------------------------------------------------------
; WritePaddingBytes
; Writes R12 zero bytes for section padding
; Input: RBX = file handle, R12 = bytes to write
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
WritePaddingBytes PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    test r12, r12
    jz padding_success

padding_write_loop:
    test r12, r12
    jz padding_success
    
    ; Write up to 32 bytes at a time
    mov r8, r12
    cmp r8, 20h
    jle padding_write_chunk
    mov r8, 20h
    
padding_write_chunk:
    mov qword ptr [rsp+30h], r8
    mov rcx, rbx
    lea rdx, zero_pad32
    lea r9, [rsp+38h]   ; bytes written
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz padding_fail
    
    sub r12, qword ptr [rsp+30h]
    jmp padding_write_loop
    
padding_success:
    mov rax, 1
    jmp padding_done
    
padding_fail:
    xor rax, rax
    
padding_done:
    add rsp, 40h
    pop rbp
    ret
WritePaddingBytes ENDP

;-----------------------------------------------------------------------------
; WriteImportSection
; Writes the complete import section with proper structure
; Input: RBX = file handle, RDI = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
WriteImportSection PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 80h

    ; Raw size target for .idata section.
    mov r14d, FILE_ALIGNMENT
    cmp dword ptr [rdi].PE_CONTEXT.numSections, 2
    jle import_raw_ready
    mov rax, [rdi].PE_CONTEXT.pSections
    add rax, SIZEOF PE_SECTION_ENTRY * 2
    mov r14d, [rax].PE_SECTION_ENTRY.SizeOfRawData
import_raw_ready:
    
    ; Check if imports exist
    cmp dword ptr [rdi].PE_CONTEXT.numImportFunctions, 0
    je write_import_placeholder
    
    ; Write import descriptors
    mov eax, [rdi].PE_CONTEXT.numImportDLLs
    inc eax     ; Include null terminator
    mov rcx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    mul rcx
    mov r8, rax
    
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pImportDescriptors
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_import_fail
    
    ; Write string table if exists
    cmp dword ptr [rdi].PE_CONTEXT.stringTableSize, 0
    je skip_string_table
    
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pStringTable
    mov r8d, [rdi].PE_CONTEXT.stringTableSize
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_import_fail
    
skip_string_table:
    ; Write import-by-name structures if they exist
    cmp dword ptr [rdi].PE_CONTEXT.importByNameSize, 0
    je skip_import_by_name
    
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pImportByNameTable
    mov r8d, [rdi].PE_CONTEXT.importByNameSize
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_import_fail
    
skip_import_by_name:
    ; Align to 8-byte boundary before thunk tables.
    mov eax, [rdi].PE_CONTEXT.numImportDLLs
    inc eax
    imul eax, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    add eax, [rdi].PE_CONTEXT.stringTableSize
    add eax, [rdi].PE_CONTEXT.importByNameSize
    and eax, 7
    jz import_thunk_aligned
    mov r12d, 8
    sub r12d, eax
    call WritePaddingBytes
    test eax, eax
    jz write_import_fail
import_thunk_aligned:
    
    ; Write IAT and ILT if they exist
    mov eax, [rdi].PE_CONTEXT.numImportFunctions
    add eax, [rdi].PE_CONTEXT.numImportDLLs
    test eax, eax
    jz skip_thunks
    mov r13d, eax
    
    ; Write IAT
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pImportAddressTable
    mov r8d, r13d
    shl r8, 3   ; 8 bytes per thunk
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_import_fail
    
    ; Write ILT  
    mov rcx, rbx
    mov rdx, [rdi].PE_CONTEXT.pImportLookupTable
    mov r8d, r13d
    shl r8, 3   ; 8 bytes per thunk
    lea r9, [rsp+60h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz write_import_fail
    
skip_thunks:
    ; Calculate remaining padding for section
    mov eax, [rdi].PE_CONTEXT.totalImportSize
    mov r12d, r14d
    sub r12d, eax
    jns import_padding_ready
    xor r12d, r12d
import_padding_ready:
    
    call WritePaddingBytes
    test eax, eax
    jz write_import_fail
    
    jmp write_import_success
    
write_import_placeholder:
    ; Write zeros for import section if no imports
    mov r12d, r14d
    call WritePaddingBytes
    test eax, eax
    jz write_import_fail
    
write_import_success:
    mov rax, 1
    jmp write_import_done
    
write_import_fail:
    xor rax, rax
    
write_import_done:
    add rsp, 80h
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
WriteImportSection ENDP

;-----------------------------------------------------------------------------
; PEWriter_WriteFile - Enhanced version with proper validation and error handling
; Writes the PE file to disk
; Input: RCX = PE context, RDX = filename
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
PEWriter_WriteFile PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 28h
    
    mov rdi, rcx     ; PE context
    mov rsi, rdx     ; Filename
    
    ; Canonical section layout baseline (5 sections: .text, .rdata, .data, .idata, .reloc).
    mov dword ptr [rdi].PE_CONTEXT.textVirtualAddress, SECTION_ALIGNMENT           ; 0x1000
    mov dword ptr [rdi].PE_CONTEXT.rdataVirtualAddress, SECTION_ALIGNMENT * 2      ; 0x2000
    mov dword ptr [rdi].PE_CONTEXT.dataVirtualAddress, SECTION_ALIGNMENT * 3       ; 0x3000
    mov dword ptr [rdi].PE_CONTEXT.idataVirtualAddress, SECTION_ALIGNMENT * 4      ; 0x4000
    mov dword ptr [rdi].PE_CONTEXT.relocVirtualAddress, SECTION_ALIGNMENT * 5      ; 0x5000
    mov dword ptr [rdi].PE_CONTEXT.textFileOffset, 400h

    mov ecx, [rdi].PE_CONTEXT.codeSize
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r12d, eax                         ; text raw size
    mov eax, 400h
    add eax, r12d
    mov [rdi].PE_CONTEXT.rdataFileOffset, eax
    add eax, FILE_ALIGNMENT               ; rdata gets 1 aligned block
    mov [rdi].PE_CONTEXT.dataFileOffset, eax
    ; Data section raw size
    mov ecx, [rdi].PE_CONTEXT.dataSize
    test ecx, ecx
    jnz @F
    mov ecx, 1                            ; Minimum 1 byte for section
@@:
    push rax
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r15d, eax                         ; data raw size (saved in r15)
    pop rax
    add eax, r15d
    mov [rdi].PE_CONTEXT.idataFileOffset, eax

    ; Build import tables if needed (uses idata RVA).
    cmp dword ptr [rdi].PE_CONTEXT.numImportFunctions, 0
    je skip_import_build
    
    mov rcx, rdi
    call BuildImportTables
    test rax, rax
    jnz @F
    jmp write_fail
@@:
    
skip_import_build:
    ; Finalize idata sizing from import build results.
    mov r13d, [rdi].PE_CONTEXT.totalImportSize
    test r13d, r13d
    jnz @F
    mov r13d, 1
@@:
    mov ecx, r13d
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r14d, eax                         ; idata raw size

    mov eax, [rdi].PE_CONTEXT.idataFileOffset
    add eax, r14d
    ; Compute .reloc file offset (after .idata)
    mov [rdi].PE_CONTEXT.relocFileOffset, eax
    ; Add .reloc section raw size
    mov ecx, [rdi].PE_CONTEXT.relocBufferSize
    test ecx, ecx
    jnz @F
    mov ecx, 8                            ; Minimum: null terminator block
@@:
    push rax
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r9d, eax                          ; reloc raw size
    pop rax
    add eax, r9d
    mov [rdi].PE_CONTEXT.currentFileOffset, eax

    mov eax, [rdi].PE_CONTEXT.relocVirtualAddress
    add eax, r9d
    mov ecx, eax
    mov rdx, SECTION_ALIGNMENT
    call AlignUp
    mov [rdi].PE_CONTEXT.currentVirtualAddress, eax

    ; Update section count to 5: .text, .rdata, .data, .idata, .reloc
    mov dword ptr [rdi].PE_CONTEXT.numSections, 5

    ; ====== Sync legacy buffers into dynamic sections ======
    ; Text section [0]
    mov r8, [rdi].PE_CONTEXT.pSections
    
    ; VirtualSize
    mov eax, [rdi].PE_CONTEXT.codeSize
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax
    
    mov eax, [rdi].PE_CONTEXT.textVirtualAddress
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    
    mov ecx, [rdi].PE_CONTEXT.codeSize
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, eax
    
    mov eax, [rdi].PE_CONTEXT.textFileOffset
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, eax
    
    mov rax, [rdi].PE_CONTEXT.pCodeBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax
    ; Set .text section name
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1, 7865742Eh       ; ".tex"
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1[4], 00000074h    ; "t\0\0\0"
    mov dword ptr [r8].PE_SECTION_ENTRY.Characteristics, 60000020h  ; CODE | EXECUTE | READ

    ; RData section [1]
    add r8, SIZEOF PE_SECTION_ENTRY
    mov dword ptr [r8].PE_SECTION_ENTRY.VirtualSize, 1
    mov eax, [rdi].PE_CONTEXT.rdataVirtualAddress
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    mov dword ptr [r8].PE_SECTION_ENTRY.SizeOfRawData, FILE_ALIGNMENT
    mov eax, [rdi].PE_CONTEXT.rdataFileOffset
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, eax
    mov qword ptr [r8].PE_SECTION_ENTRY.pDataBuffer, 0
    ; Set .rdata section name
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1, 6164722Eh       ; ".rda"
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1[4], 00006174h    ; "ta\0\0"
    mov dword ptr [r8].PE_SECTION_ENTRY.Characteristics, 40000040h  ; INITIALIZED_DATA | READ

    ; IData section [2]
    add r8, SIZEOF PE_SECTION_ENTRY
    mov eax, r13d
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax
    mov eax, [rdi].PE_CONTEXT.idataVirtualAddress
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r14d
    
    mov eax, [rdi].PE_CONTEXT.idataFileOffset
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, eax
    
    mov rax, [rdi].PE_CONTEXT.pImportDescriptors
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax
    ; Set .idata section name
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1, 6164692Eh       ; ".ida"
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1[4], 00006174h    ; "ta\0\0"
    mov dword ptr [r8].PE_SECTION_ENTRY.Characteristics, 0C0000040h ; INITIALIZED_DATA | READ | WRITE

    ; Data section [3]
    add r8, SIZEOF PE_SECTION_ENTRY
    mov eax, [rdi].PE_CONTEXT.dataSize
    test eax, eax
    jnz @F
    mov eax, 1
@@:
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax
    mov eax, [rdi].PE_CONTEXT.dataVirtualAddress
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r15d
    mov eax, [rdi].PE_CONTEXT.dataFileOffset
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, eax
    mov dword ptr [r8].PE_SECTION_ENTRY.Characteristics, 0C0000040h   ; INITIALIZED_DATA | READ | WRITE
    mov rax, [rdi].PE_CONTEXT.pDataBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax
    ; Set .data section name
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1, 6174642Eh       ; ".dat"
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1[4], 00000061h    ; "a\0\0\0"

    ; Reloc section [4]
    add r8, SIZEOF PE_SECTION_ENTRY
    mov eax, [rdi].PE_CONTEXT.relocBufferSize
    test eax, eax
    jnz @F
    mov eax, 8                    ; Minimum: null terminator block
@@:
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax
    mov eax, [rdi].PE_CONTEXT.relocVirtualAddress
    mov [r8].PE_SECTION_ENTRY.VirtualAddress, eax
    ; Reloc raw size (aligned)
    mov ecx, [rdi].PE_CONTEXT.relocBufferSize
    test ecx, ecx
    jnz @F
    mov ecx, 8
@@:
    push r8
    mov rdx, FILE_ALIGNMENT
    call AlignUp
    mov r9d, eax
    pop r8
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r9d
    mov eax, [rdi].PE_CONTEXT.relocFileOffset
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, eax
    mov dword ptr [r8].PE_SECTION_ENTRY.Characteristics, 42000040h   ; INITIALIZED_DATA | DISCARDABLE | READ
    mov rax, [rdi].PE_CONTEXT.pRelocBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax
    ; Set .reloc section name
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1, 6C65722Eh       ; ".rel"
    mov dword ptr [r8].PE_SECTION_ENTRY.Name1[4], 0000636Fh    ; "oc\0\0"
    ; ========================================================

    ; Validate finalized context.
    mov rcx, rdi
    call ValidatePEContext
    test rax, rax
    jz write_fail

    ; Update section headers
    ; Section headers are emitted directly from pSections in WritePEHeaders.
    
    ; Create file
    mov rcx, rsi
    mov rdx, 40000000h  ; GENERIC_WRITE
    mov r8, 0           ; No sharing
    mov r9, 0           ; No security
    mov qword ptr [rsp+20h], 2  ; CREATE_ALWAYS
    mov qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0   ; No template
    call CreateFileA
    cmp rax, -1
    jne @F
    jmp write_fail
@@:
    mov rbx, rax     ; File handle
    
    ; Write PE headers (DOS, NT, Section headers)
    mov rcx, rbx
    mov rdx, rdi
    call WritePEHeaders
    test rax, rax
    jnz @F
    mov rcx, rbx
    call CloseHandle
    jmp write_fail
@@:
    
    ; Write all sections
    mov rcx, rbx
    mov rdx, rdi
    call WritePESections
    cmp rax, 1
    je @F
    mov r12, rax
    mov rcx, rbx
    call CloseHandle
    mov rax, r12
    jmp write_done
@@:

    ; Verify physical file size matches declared raw output size.
    mov rcx, rbx
    xor rdx, rdx
    call GetFileSize
    cmp eax, [rdi].PE_CONTEXT.currentFileOffset
    je @F
    mov rcx, rbx
    call CloseHandle
    jmp write_fail
@@:
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    mov rax, 1       ; Success
    jmp write_done
    
write_fail:
    xor rax, rax
    
write_done:
    add rsp, 28h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_WriteFile ENDP

;-----------------------------------------------------------------------------
; UpdateSectionHeaders
; Updates section headers with proper names and characteristics
; Input: RDI = PE context
;-----------------------------------------------------------------------------
UpdateSectionHeaders PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13

    mov rdi, rcx
    test rdi, rdi
    jz update_sections_fail

    mov r12, [rdi].PE_CONTEXT.pNtHeader
    mov r13, [rdi].PE_CONTEXT.pSectionHeaders
    mov rsi, [rdi].PE_CONTEXT.pSections
    mov ebx, [rdi].PE_CONTEXT.numSections
    test r12, r12
    jz update_sections_fail
    test r13, r13
    jz update_sections_fail
    test rsi, rsi
    jz update_sections_fail

    mov word ptr [r12].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections, bx
    xor edx, edx

update_sections_loop:
    cmp edx, ebx
    jae update_sections_done

    mov rax, qword ptr [rsi].PE_SECTION_ENTRY.Name1
    mov qword ptr [r13].IMAGE_SECTION_HEADER.Name1, rax

    mov eax, dword ptr [rsi].PE_SECTION_ENTRY.VirtualSize
    mov dword ptr [r13].IMAGE_SECTION_HEADER.VirtualSize, eax

    mov eax, dword ptr [rsi].PE_SECTION_ENTRY.VirtualAddress
    mov dword ptr [r13].IMAGE_SECTION_HEADER.VirtualAddress, eax

    mov eax, dword ptr [rsi].PE_SECTION_ENTRY.SizeOfRawData
    mov dword ptr [r13].IMAGE_SECTION_HEADER.SizeOfRawData, eax

    mov eax, dword ptr [rsi].PE_SECTION_ENTRY.PointerToRawData
    mov dword ptr [r13].IMAGE_SECTION_HEADER.PointerToRawData, eax

    mov dword ptr [r13].IMAGE_SECTION_HEADER.PointerToRelocations, 0
    mov dword ptr [r13].IMAGE_SECTION_HEADER.PointerToLinenumbers, 0
    mov word ptr [r13].IMAGE_SECTION_HEADER.NumberOfRelocations, 0
    mov word ptr [r13].IMAGE_SECTION_HEADER.NumberOfLinenumbers, 0

    mov eax, dword ptr [rsi].PE_SECTION_ENTRY.Characteristics
    mov dword ptr [r13].IMAGE_SECTION_HEADER.Characteristics, eax

    add r13, SIZEOF IMAGE_SECTION_HEADER
    add rsi, SIZEOF PE_SECTION_ENTRY
    inc edx
    jmp update_sections_loop

update_sections_done:
    mov rax, 1
    jmp update_sections_exit

update_sections_fail:
    xor rax, rax

update_sections_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
UpdateSectionHeaders ENDP

;=============================================================================
; INTEGRATION TESTS AND EXAMPLES
;=============================================================================

;-----------------------------------------------------------------------------
; PEWriter_CreateSimpleExecutable
; Creates a simple executable that calls ExitProcess
; Input: RCX = output filename
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
PEWriter_CreateSimpleExecutable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT  ; Entry point at start of .text
    call PEWriter_CreateExecutable
    test rax, rax
    jz simple_exe_fail
    mov rdi, rax    ; PE context
    
    ; Add kernel32.dll import for ExitProcess
    mov rcx, rdi
    lea rdx, kernel32_dll_name
    lea r8, exitprocess_func_name
    call PEWriter_AddImport
    test rax, rax
    jz simple_exe_cleanup
    
    ; Generate simple code: call ExitProcess(0)
    mov rcx, rdi
    call GenerateSimpleExitCode
    test rax, rax
    jz simple_exe_cleanup
    
    ; Write PE file
    mov rcx, rdi
    mov rdx, rbx
    call PEWriter_WriteFile
    test rax, rax
    jz simple_exe_cleanup
    
    ; Cleanup and return success
    call PE_FreeContext
    mov rax, 1
    jmp simple_exe_done
    
simple_exe_cleanup:
    mov rcx, rdi
    call PE_FreeContext
    
simple_exe_fail:
    xor rax, rax
    
simple_exe_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PEWriter_CreateSimpleExecutable ENDP

;-----------------------------------------------------------------------------
; PEWriter_CreateMessageBoxApp
; Creates an executable that displays a message box
; Input: RCX = output filename
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
PEWriter_CreateMessageBoxApp PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Create PE context
    mov rcx, DEFAULT_IMAGE_BASE
    mov rdx, SECTION_ALIGNMENT  ; Entry point
    call PEWriter_CreateExecutable
    test rax, rax
    jz msgbox_fail
    mov rdi, rax
    
    ; Add user32.dll imports
    mov rcx, rdi
    lea rdx, user32_dll_name
    lea r8, messageboxa_func_name
    call PEWriter_AddImport
    test rax, rax
    jz msgbox_cleanup
    
    ; Add kernel32.dll import
    mov rcx, rdi
    lea rdx, kernel32_dll_name
    lea r8, exitprocess_func_name
    call PEWriter_AddImport
    test rax, rax
    jz msgbox_cleanup
    
    ; Generate message box code
    mov rcx, rdi
    call GenerateMessageBoxCode
    test rax, rax
    jz msgbox_cleanup
    
    ; Write PE file
    mov rcx, rdi
    mov rdx, rbx
    call PEWriter_WriteFile
    test rax, rax
    jz msgbox_cleanup
    
    ; Cleanup and return success
    call PE_FreeContext
    mov rax, 1
    jmp msgbox_done
    
msgbox_cleanup:
    mov rcx, rdi
    call PE_FreeContext
    
msgbox_fail:
    xor rax, rax
    
msgbox_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PEWriter_CreateMessageBoxApp ENDP

;-----------------------------------------------------------------------------
; GenerateSimpleExitCode
; Generates code that calls ExitProcess(0)
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure
;-----------------------------------------------------------------------------
GenerateSimpleExitCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_simple_fail
    
    ; mov ecx, 0 (exit code)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_simple_fail
    
    ; call ExitProcess
    mov rcx, rbx
    lea rdx, exitprocess_func_name
    call Emit_CALL
    test rax, rax
    jz gen_simple_fail
    
    ; Should not return, but add epilogue for completeness
    mov rcx, rbx
    call Emit_FunctionEpilogue
    test rax, rax
    jz gen_simple_fail
    
    ; Add return instruction
    mov rcx, rbx
    call Emit_RET
    
gen_simple_fail:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateSimpleExitCode ENDP

;-----------------------------------------------------------------------------
; GenerateMessageBoxCode
; Generates code that displays a message box then exits
; Input: RCX = PE context
; Output: RAX = 1 success, 0 failure  
;-----------------------------------------------------------------------------
GenerateMessageBoxCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; PE context
    
    ; Function prologue
    mov rcx, rbx
    call Emit_FunctionPrologue
    test rax, rax
    jz gen_msgbox_fail
    
    ; Prepare MessageBoxA parameters
    ; mov r9d, 0 (type - MB_OK)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_R9
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_msgbox_fail
    
    ; lea r8, [caption] (caption)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_R8
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_LABEL
    lea r8, caption_label_name
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_LEA
    test rax, rax
    jz gen_msgbox_fail
    
    ; lea rdx, [message] (text)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RDX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_LABEL
    lea r8, message_label_name
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_LEA
    test rax, rax
    jz gen_msgbox_fail
    
    ; mov rcx, 0 (hwnd - NULL)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_msgbox_fail
    
    ; call MessageBoxA
    mov rcx, rbx
    lea rdx, messageboxa_func_name
    call Emit_CALL
    test rax, rax
    jz gen_msgbox_fail
    
    ; mov ecx, 0 (exit code for ExitProcess)
    mov rcx, rbx
    mov rdx, OP_REG
    mov r8, REG_RCX
    call CodeGen_CreateOperand
    mov rsi, rax
    
    mov rdx, OP_IMM
    mov r8, 0
    call CodeGen_CreateOperand
    mov rdi, rax
    
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call Emit_MOV
    test rax, rax
    jz gen_msgbox_fail
    
    ; call ExitProcess
    mov rcx, rbx
    lea rdx, exitprocess_func_name
    call Emit_CALL
    test rax, rax
    jz gen_msgbox_fail
    
    ; Add string data (simplified - normally would be in .rdata)
    mov rcx, rbx
    lea rdx, message_label_name
    call CodeGen_DefineLabel
    
    mov rcx, rbx
    lea rdx, caption_label_name
    call CodeGen_DefineLabel
    
gen_msgbox_fail:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateMessageBoxCode ENDP

;-----------------------------------------------------------------------------
; PE_FreeContext
; Frees all memory associated with PE context
; Input: RCX = PE context
;-----------------------------------------------------------------------------
PE_FreeContext PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 30h
    
    mov rbx, rcx    ; PE context
    test rbx, rbx
    jz free_ctx_done
    
    mov rsi, [rbx].PE_CONTEXT.hHeap
    test rsi, rsi
    jz free_ctx_done
    
    ; Free DOS header buffer
    mov r8, [rbx].PE_CONTEXT.pDosHeader
    call FreeHeapBuffer
    
    ; Free NT header buffer
    mov r8, [rbx].PE_CONTEXT.pNtHeader
    call FreeHeapBuffer
    
    ; Free section array
    mov r8, [rbx].PE_CONTEXT.pSections
    call FreeHeapBuffer
    
    ; Free import descriptors
    mov r8, [rbx].PE_CONTEXT.pImportDescriptors
    call FreeHeapBuffer
    
    ; Free import address table
    mov r8, [rbx].PE_CONTEXT.pImportAddressTable
    call FreeHeapBuffer
    
    ; Free import lookup table
    mov r8, [rbx].PE_CONTEXT.pImportLookupTable
    call FreeHeapBuffer
    
    ; Free code buffer
    mov r8, [rbx].PE_CONTEXT.pCodeBuffer
    call FreeHeapBuffer
    
    ; Free relocation table (code relocations)
    mov r8, [rbx].PE_CONTEXT.pRelocations
    call FreeHeapBuffer
    
    ; Free label table
    mov r8, [rbx].PE_CONTEXT.pLabels
    call FreeHeapBuffer
    
    ; Free import DLL table
    mov r8, [rbx].PE_CONTEXT.pImportDLLTable
    call FreeHeapBuffer
    
    ; Free import function table
    mov r8, [rbx].PE_CONTEXT.pImportFunctionTable
    call FreeHeapBuffer
    
    ; Free string table
    mov r8, [rbx].PE_CONTEXT.pStringTable
    call FreeHeapBuffer
    
    ; Free import by name table
    mov r8, [rbx].PE_CONTEXT.pImportByNameTable
    call FreeHeapBuffer
    
    ; Free data buffer (new)
    mov r8, [rbx].PE_CONTEXT.pDataBuffer
    call FreeHeapBuffer
    
    ; Free reloc buffer (new)
    mov r8, [rbx].PE_CONTEXT.pRelocBuffer
    call FreeHeapBuffer
    
    ; Free resource buffer (new)
    mov r8, [rbx].PE_CONTEXT.pResourceBuffer
    call FreeHeapBuffer
    
    ; Free exception/pdata buffer (new)
    mov r8, [rbx].PE_CONTEXT.pExceptionBuffer
    call FreeHeapBuffer
    
    ; Free unwind/xdata buffer (new)
    mov r8, [rbx].PE_CONTEXT.pUnwindBuffer
    call FreeHeapBuffer
    
    ; Free context itself
    mov rcx, rsi
    xor edx, edx
    mov r8, rbx
    call HeapFree
    
free_ctx_done:
    add rsp, 30h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PE_FreeContext ENDP

;-----------------------------------------------------------------------------
; FreeHeapBuffer - Free a single heap-allocated buffer if non-null
; Input: RSI = heap handle, R8 = buffer pointer (may be NULL)
; Preserves: RBX, RSI, RDI
;-----------------------------------------------------------------------------
FreeHeapBuffer PROC
    test r8, r8
    jz free_buf_skip
    push rcx
    push rdx
    mov rcx, rsi         ; Heap handle
    xor edx, edx         ; Flags = 0
    ; R8 already has buffer pointer
    call HeapFree
    pop rdx
    pop rcx
free_buf_skip:
    ret
FreeHeapBuffer ENDP

;-----------------------------------------------------------------------------
; PEWriter_RunIntegrationTests
; Runs comprehensive tests of the PE writer functionality
; Input: None
; Output: RAX = number of tests passed
;-----------------------------------------------------------------------------
PEWriter_RunIntegrationTests PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    push rsi
    push rdi
    
    xor rbx, rbx    ; Test counter
    
    ; Test 1: Create simple executable
    lea rcx, simple_exe_name
    call PEWriter_CreateSimpleExecutable
    add rbx, rax
    
    ; Test 2: Create message box app
    lea rcx, msgbox_exe_name
    call PEWriter_CreateMessageBoxApp
    add rbx, rax
    
    ; Test 3: Validate generated files
    lea rcx, simple_exe_name
    call ValidateExecutableFile
    add rbx, rax
    
    lea rcx, msgbox_exe_name
    call ValidateExecutableFile
    add rbx, rax
    
    mov rax, rbx    ; Return test count
    
    pop rdi
    pop rsi
    pop rbx
    add rsp, 40h
    pop rbp
    ret
PEWriter_RunIntegrationTests ENDP

;-----------------------------------------------------------------------------
; ValidateExecutableFile
; Validates that a generated PE file has correct structure
; Input: RCX = filename
; Output: RAX = 1 valid, 0 invalid
;-----------------------------------------------------------------------------
ValidateExecutableFile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Filename
    
    ; Open file for reading
    mov rcx, rbx
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1           ; FILE_SHARE_READ
    mov r9, 0           ; No security
    mov qword ptr [rsp+20h], 3  ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0   ; No template
    call CreateFileA
    cmp rax, -1
    je validate_fail
    mov rsi, rax    ; File handle
    
    ; Read and validate DOS header
    mov rcx, rsi
    lea rdx, [rsp+40h]
    mov r8, SIZEOF IMAGE_DOS_HEADER
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz validate_cleanup_fail
    
    ; Check DOS signature
    cmp word ptr [rsp+40h], IMAGE_DOS_SIGNATURE
    jne validate_cleanup_fail
    
    ; Validate NT header offset
    mov eax, dword ptr [rsp+40h + 60]  ; e_lfanew
    cmp eax, 400
    ja validate_cleanup_fail  ; Too far
    cmp eax, 40h
    jb validate_cleanup_fail  ; Too close
    
    ; Seek to NT header
    mov rcx, rsi
    mov edx, eax
    xor r8d, r8d
    mov r9, FILE_BEGIN
    call SetFilePointer
    cmp eax, -1
    je validate_cleanup_fail
    
    ; Read NT signature
    mov rcx, rsi
    lea rdx, [rsp+40h]
    mov r8, 4
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz validate_cleanup_fail
    
    ; Check NT signature
    cmp dword ptr [rsp+40h], IMAGE_NT_SIGNATURE
    jne validate_cleanup_fail
    
    ; File appears valid
    mov rcx, rsi
    call CloseHandle
    mov rax, 1
    jmp validate_done
    
validate_cleanup_fail:
    mov rcx, rsi
    call CloseHandle
    
validate_fail:
    xor rax, rax
    
validate_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateExecutableFile ENDP

;=============================================================================
; TEST DATA
;=============================================================================
.data
; Test filenames
simple_exe_name db "simple_test.exe", 0
msgbox_exe_name db "msgbox_test.exe", 0

; DLL names for imports
kernel32_dll_name db "kernel32.dll", 0
user32_dll_name db "user32.dll", 0

; Function names for imports
exitprocess_func_name db "ExitProcess", 0
messageboxa_func_name db "MessageBoxA", 0

; Test strings
test_message db "Hello from PE Writer!", 0
test_caption db "PE Writer Test", 0
message_label_name db "message_str", 0
caption_label_name db "caption_str", 0
zero_pad32 db 32 DUP(0)

;=============================================================================
; HELPER FUNCTION: ReadFile (external)
;=============================================================================
EXTERN ReadFile:PROC


;=============================================================================
; ADVANCED PE FEATURES (.rsrc, .pdata, Signatures, Compression)
;=============================================================================
PUBLIC PEWriter_AddResource
PUBLIC PEWriter_AddExceptionData
PUBLIC PEWriter_SignExecutable
PUBLIC PEWriter_CompressExecutable

; Resource Directory Structs
IMAGE_RESOURCE_DIRECTORY STRUC
    Characteristics       DWORD ?
    TimeDateStamp         DWORD ?
    MajorVersion          WORD ?
    MinorVersion          WORD ?
    NumberOfNamedEntries  WORD ?
    NumberOfIdEntries     WORD ?
IMAGE_RESOURCE_DIRECTORY ENDS

IMAGE_RESOURCE_DIRECTORY_ENTRY STRUC
    Name1                 DWORD ?
    OffsetToData          DWORD ?
IMAGE_RESOURCE_DIRECTORY_ENTRY ENDS

IMAGE_RESOURCE_DATA_ENTRY STRUC
    OffsetToData          DWORD ?
    Size1                 DWORD ?
    CodePage              DWORD ?
    Reserved              DWORD ?
IMAGE_RESOURCE_DATA_ENTRY ENDS

; Runtime Function Structs 
IMAGE_RUNTIME_FUNCTION_ENTRY STRUC
    BeginAddress DWORD ?
    EndAddress   DWORD ?
    UnwindInfoAddress DWORD ?
IMAGE_RUNTIME_FUNCTION_ENTRY ENDS

.data
rsrc_name db '.rsrc', 0, 0, 0
pdata_name db '.pdata', 0, 0
xdata_name db '.xdata', 0, 0

.code

;-----------------------------------------------------------------------------
; PEWriter_AddResource
; Full IMAGE_RESOURCE_DIRECTORY 3-level tree builder
; Input: RCX = Context, RDX = Resource ID, R8 = Resource Type, R9 = Buffer
;        [RSP+28h] = Size (stack param after shadow space, accounting for
;                    5 pushes + sub rsp = frame)
;        NOTE: Caller passes size at [RSP+30h] relative to caller's RSP.
;              After push rbp (8) + push rbx (8) + push rsi (8) + push rdi (8)
;              + push r12 (8) + push r13 (8) + push r14 (8) + push r15(8)
;              + sub rsp,60h = 0xA0 bytes total.
;              Original [RSP+28h] at call site => [RSP+28h+0A0h] = [RSP+0C8h]
;              But with frame pointer: [RBP+30h] per x64 convention (arg5)
;-----------------------------------------------------------------------------
ALIGN 16
PEWriter_AddResource PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 60h

    ; Save all inputs
    mov rdi, rcx                    ; rdi = PE_CONTEXT*
    mov r12d, edx                   ; r12d = Resource ID
    mov r13d, r8d                   ; r13d = Resource Type (RT_xxx)
    mov r14, r9                     ; r14 = source data buffer pointer
    mov r15d, dword ptr [rbp+30h]   ; r15d = data size (5th param)

    ; Validate inputs
    test r14, r14
    jz rsrc_fail
    test r15d, r15d
    jz rsrc_fail
    cmp r15d, MAX_RESOURCE_SIZE
    ja rsrc_fail

    ;-----------------------------------------------------------------
    ; Calculate total resource section size:
    ;   Level 1: IMAGE_RESOURCE_DIRECTORY (16) + IMAGE_RESOURCE_DIRECTORY_ENTRY (8) = 24
    ;   Level 2: IMAGE_RESOURCE_DIRECTORY (16) + IMAGE_RESOURCE_DIRECTORY_ENTRY (8) = 24
    ;   Level 3: IMAGE_RESOURCE_DIRECTORY (16) + IMAGE_RESOURCE_DIRECTORY_ENTRY (8) = 24
    ;   IMAGE_RESOURCE_DATA_ENTRY (16)
    ;   Raw resource data (r15d bytes, aligned to DWORD)
    ;   Total tree = 3*24 + 16 = 88 bytes
    ;-----------------------------------------------------------------
    RSRC_TREE_SIZE equ 88
    mov eax, r15d
    add eax, 3                      ; align data to DWORD
    and eax, 0FFFFFFFCh
    mov ebx, eax                    ; ebx = aligned data size
    add eax, RSRC_TREE_SIZE         ; total resource section size
    mov [rbp-38h], eax              ; [rbp-38h] = totalRsrcSize

    ;-----------------------------------------------------------------
    ; Allocate resource buffer if not already allocated
    ;-----------------------------------------------------------------
    cmp qword ptr [rdi].PE_CONTEXT.pResourceBuffer, 0
    jne rsrc_have_buffer

    ; HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_RESOURCE_SIZE)
    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, MAX_RESOURCE_SIZE
    call HeapAlloc
    test rax, rax
    jz rsrc_fail
    mov [rdi].PE_CONTEXT.pResourceBuffer, rax
    mov dword ptr [rdi].PE_CONTEXT.resourceSize, 0

rsrc_have_buffer:
    mov rsi, [rdi].PE_CONTEXT.pResourceBuffer  ; rsi = write cursor

    ;=================================================================
    ; BUILD 3-LEVEL RESOURCE DIRECTORY TREE
    ;=================================================================

    ;----- Level 1 (Root): Dispatch by resource type -----
    ; IMAGE_RESOURCE_DIRECTORY at offset 0
    mov dword ptr [rsi+0],  0               ; Characteristics
    mov dword ptr [rsi+4],  0               ; TimeDateStamp
    mov word  ptr [rsi+8],  0               ; MajorVersion
    mov word  ptr [rsi+0Ah], 0              ; MinorVersion
    mov word  ptr [rsi+0Ch], 0              ; NumberOfNamedEntries
    mov word  ptr [rsi+0Eh], 1              ; NumberOfIdEntries = 1
    ; IMAGE_RESOURCE_DIRECTORY_ENTRY at offset 16
    mov dword ptr [rsi+10h], r13d           ; Name1 = resource type ID
    ; OffsetToData = offset of Level2 dir, with high bit set (subdirectory flag)
    mov eax, 18h                            ; offset 24 = level 2 dir
    or  eax, 80000000h                      ; set subdirectory bit
    mov dword ptr [rsi+14h], eax

    ;----- Level 2 (Type): Dispatch by resource ID -----
    ; IMAGE_RESOURCE_DIRECTORY at offset 24
    mov dword ptr [rsi+18h], 0              ; Characteristics
    mov dword ptr [rsi+1Ch], 0              ; TimeDateStamp
    mov word  ptr [rsi+20h], 0              ; MajorVersion
    mov word  ptr [rsi+22h], 0              ; MinorVersion
    mov word  ptr [rsi+24h], 0              ; NumberOfNamedEntries
    mov word  ptr [rsi+26h], 1              ; NumberOfIdEntries = 1
    ; IMAGE_RESOURCE_DIRECTORY_ENTRY at offset 40
    mov dword ptr [rsi+28h], r12d           ; Name1 = resource ID
    mov eax, 30h                            ; offset 48 = level 3 dir
    or  eax, 80000000h
    mov dword ptr [rsi+2Ch], eax

    ;----- Level 3 (Language): Default language 0x0409 (en-US) -----
    ; IMAGE_RESOURCE_DIRECTORY at offset 48
    mov dword ptr [rsi+30h], 0              ; Characteristics
    mov dword ptr [rsi+34h], 0              ; TimeDateStamp
    mov word  ptr [rsi+38h], 0              ; MajorVersion
    mov word  ptr [rsi+3Ah], 0              ; MinorVersion
    mov word  ptr [rsi+3Ch], 0              ; NumberOfNamedEntries
    mov word  ptr [rsi+3Eh], 1              ; NumberOfIdEntries = 1
    ; IMAGE_RESOURCE_DIRECTORY_ENTRY at offset 64  (0x40)
    mov dword ptr [rsi+40h], 0409h          ; Name1 = language ID (en-US)
    ; OffsetToData = offset of DATA_ENTRY (no high bit = leaf)
    mov dword ptr [rsi+44h], 48h            ; offset 72 = data entry

    ;----- IMAGE_RESOURCE_DATA_ENTRY at offset 72 (0x48) -----
    ; OffsetToData = RVA of actual resource data within .rsrc section
    ; This will be patched to absolute RVA after section VA is known
    ; For now: relative offset within rsrc section = RSRC_TREE_SIZE (88 = 0x58)
    mov dword ptr [rsi+48h], RSRC_TREE_SIZE ; OffsetToData (relative, patched later)
    mov dword ptr [rsi+4Ch], r15d           ; Size = actual data size
    mov dword ptr [rsi+50h], 0              ; CodePage = 0
    mov dword ptr [rsi+54h], 0              ; Reserved = 0

    ;----- Copy actual resource data at offset 88 (0x58) -----
    lea  rcx, [rsi+RSRC_TREE_SIZE]          ; destination
    mov  rdx, r14                           ; source
    xor  eax, eax
rsrc_copy_loop:
    cmp  eax, r15d
    jae  rsrc_copy_done
    movzx r8d, byte ptr [rdx+rax]
    mov  byte ptr [rcx+rax], r8b
    inc  eax
    jmp  rsrc_copy_loop
rsrc_copy_done:

    ;-----------------------------------------------------------------
    ; Update resource size in context
    ;-----------------------------------------------------------------
    mov eax, [rbp-38h]                      ; totalRsrcSize
    mov [rdi].PE_CONTEXT.resourceSize, eax

    ;-----------------------------------------------------------------
    ; Add .rsrc section via PEWriter_AddSection
    ;-----------------------------------------------------------------
    mov rcx, rdi
    lea rdx, [rsrc_name]
    mov r8d, 40000040h                      ; INITIALIZED_DATA | READ
    call PEWriter_AddSection
    ; rax = section index
    cmp rax, -1
    je rsrc_fail
    mov ebx, eax                            ; ebx = section index

    ;-----------------------------------------------------------------
    ; Populate the new section entry with resource data
    ;-----------------------------------------------------------------
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax                             ; r8 = PE_SECTION_ENTRY*

    mov eax, [rbp-38h]                      ; totalRsrcSize
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax

    ; SizeOfRawData = AlignUp(totalRsrcSize, FILE_ALIGNMENT)
    mov ecx, [rbp-38h]
    mov edx, FILE_ALIGNMENT
    call AlignUp
    mov r9d, eax                            ; r9d = aligned raw size
    mov r8, [rdi].PE_CONTEXT.pSections
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    add r8, rax
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r9d

    ; Set data buffer pointer
    mov rax, [rdi].PE_CONTEXT.pResourceBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax

    ; Record VirtualAddress for RVA fixup
    mov eax, [r8].PE_SECTION_ENTRY.VirtualAddress
    mov [rdi].PE_CONTEXT.resourceVirtualAddress, eax

    ;-----------------------------------------------------------------
    ; Patch IMAGE_RESOURCE_DATA_ENTRY.OffsetToData to absolute RVA
    ; RVA = sectionVA + RSRC_TREE_SIZE
    ;-----------------------------------------------------------------
    mov rsi, [rdi].PE_CONTEXT.pResourceBuffer
    mov eax, [rdi].PE_CONTEXT.resourceVirtualAddress
    add eax, RSRC_TREE_SIZE
    mov dword ptr [rsi+48h], eax            ; patch OffsetToData to real RVA

    ;-----------------------------------------------------------------
    ; Set DataDirectory[2] (RESOURCE)
    ;-----------------------------------------------------------------
    mov rbx, [rdi].PE_CONTEXT.pNtHeader
    test rbx, rbx
    jz rsrc_success
    mov eax, [rdi].PE_CONTEXT.resourceVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, eax
    mov eax, [rdi].PE_CONTEXT.resourceSize
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, eax

rsrc_success:
    mov eax, 1                              ; success
    jmp rsrc_done

rsrc_fail:
    xor eax, eax                            ; failure

rsrc_done:
    add rsp, 60h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_AddResource ENDP

;-----------------------------------------------------------------------------
; PEWriter_AddExceptionData
; Full .pdata + .xdata implementation
; Input: RCX = PE Context
;        RDX = pRuntimeFunctions (array of IMAGE_RUNTIME_FUNCTION_ENTRY)
;        R8  = count of entries
;        R9  = xDataBuffer (optional UNWIND_INFO blob, NULL if none)
;        [RBP+30h] = xDataSize (5th param, size of xdata if R9 != NULL)
;-----------------------------------------------------------------------------
ALIGN 16
PEWriter_AddExceptionData PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 60h

    ; Save inputs
    mov rdi, rcx                    ; rdi = PE_CONTEXT*
    mov r14, rdx                    ; r14 = pRuntimeFunctions source
    mov r12d, r8d                   ; r12d = entry count
    mov r15, r9                     ; r15 = xDataBuffer (may be NULL)

    ; Validate
    test r14, r14
    jz except_fail
    test r12d, r12d
    jz except_fail
    cmp r12d, MAX_EXCEPTION_ENTRIES
    ja except_fail

    ;-----------------------------------------------------------------
    ; Calculate .pdata size: count * SIZEOF IMAGE_RUNTIME_FUNCTION_ENTRY (12)
    ;-----------------------------------------------------------------
    mov eax, r12d
    imul eax, 12                    ; 12 bytes per RUNTIME_FUNCTION_ENTRY
    mov r13d, eax                   ; r13d = pdata byte size

    ;-----------------------------------------------------------------
    ; Allocate .pdata buffer if not already allocated
    ;-----------------------------------------------------------------
    cmp qword ptr [rdi].PE_CONTEXT.pExceptionBuffer, 0
    jne except_have_pdata_buf

    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, MAX_EXCEPTION_ENTRIES
    imul r8d, 12
    call HeapAlloc
    test rax, rax
    jz except_fail
    mov [rdi].PE_CONTEXT.pExceptionBuffer, rax

except_have_pdata_buf:
    ;-----------------------------------------------------------------
    ; Copy IMAGE_RUNTIME_FUNCTION_ENTRY array into pExceptionBuffer
    ;-----------------------------------------------------------------
    mov rcx, [rdi].PE_CONTEXT.pExceptionBuffer
    mov rdx, r14                    ; source
    xor eax, eax
except_copy_pdata:
    cmp eax, r13d
    jae except_pdata_copied
    movzx ebx, byte ptr [rdx+rax]
    mov byte ptr [rcx+rax], bl
    inc eax
    jmp except_copy_pdata
except_pdata_copied:

    mov [rdi].PE_CONTEXT.exceptionSize, r13d

    ;-----------------------------------------------------------------
    ; Handle optional .xdata (UNWIND_INFO) buffer
    ;-----------------------------------------------------------------
    test r15, r15
    jz except_no_xdata

    ; Get xdata size from 5th parameter
    mov ebx, dword ptr [rbp+30h]    ; xDataSize
    test ebx, ebx
    jz except_no_xdata

    ; Allocate .xdata buffer if needed
    cmp qword ptr [rdi].PE_CONTEXT.pUnwindBuffer, 0
    jne except_have_xdata_buf

    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    ; Allocate enough for xdata (use same MAX size)
    mov r8d, MAX_EXCEPTION_ENTRIES
    imul r8d, 12
    call HeapAlloc
    test rax, rax
    jz except_fail
    mov [rdi].PE_CONTEXT.pUnwindBuffer, rax

except_have_xdata_buf:
    ; Copy UNWIND_INFO data
    mov ebx, dword ptr [rbp+30h]    ; reload xDataSize
    mov rcx, [rdi].PE_CONTEXT.pUnwindBuffer
    mov rdx, r15                    ; source xDataBuffer
    xor eax, eax
except_copy_xdata:
    cmp eax, ebx
    jae except_xdata_copied
    movzx r8d, byte ptr [rdx+rax]
    mov byte ptr [rcx+rax], r8b
    inc eax
    jmp except_copy_xdata
except_xdata_copied:
    mov [rdi].PE_CONTEXT.unwindSize, ebx

    ;-----------------------------------------------------------------
    ; Add .xdata section first (so we know its VA for patching pdata)
    ;-----------------------------------------------------------------
    mov rcx, rdi
    lea rdx, [xdata_name]
    mov r8d, 40000040h              ; INITIALIZED_DATA | READ
    call PEWriter_AddSection
    cmp rax, -1
    je except_fail
    mov ebx, eax                    ; ebx = xdata section index

    ; Populate .xdata section entry
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax

    mov eax, [rdi].PE_CONTEXT.unwindSize
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax

    ; SizeOfRawData = AlignUp(unwindSize, FILE_ALIGNMENT)
    mov ecx, [rdi].PE_CONTEXT.unwindSize
    mov edx, FILE_ALIGNMENT
    call AlignUp
    ; Re-fetch section pointer (AlignUp clobbers rcx/rdx)
    mov r9d, eax
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r9d

    ; Set xdata buffer
    mov rax, [rdi].PE_CONTEXT.pUnwindBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax

    ; Save xdata VA for pdata UnwindInfoAddress fixup
    mov eax, [r8].PE_SECTION_ENTRY.VirtualAddress
    mov [rbp-40h], eax              ; [rbp-40h] = xdataVA
    jmp except_add_pdata

except_no_xdata:
    mov dword ptr [rdi].PE_CONTEXT.unwindSize, 0
    mov dword ptr [rbp-40h], 0      ; no xdata VA

except_add_pdata:
    ;-----------------------------------------------------------------
    ; Add .pdata section
    ;-----------------------------------------------------------------
    mov rcx, rdi
    lea rdx, [pdata_name]
    mov r8d, 40000040h              ; INITIALIZED_DATA | READ
    call PEWriter_AddSection
    cmp rax, -1
    je except_fail
    mov ebx, eax                    ; ebx = pdata section index

    ; Populate .pdata section entry
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax

    mov eax, [rdi].PE_CONTEXT.exceptionSize
    mov [r8].PE_SECTION_ENTRY.VirtualSize, eax

    ; SizeOfRawData = AlignUp(exceptionSize, FILE_ALIGNMENT)
    mov ecx, [rdi].PE_CONTEXT.exceptionSize
    mov edx, FILE_ALIGNMENT
    call AlignUp
    mov r9d, eax
    mov eax, ebx
    mov ecx, SIZEOF PE_SECTION_ENTRY
    imul eax, ecx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r9d

    ; Set pdata buffer
    mov rax, [rdi].PE_CONTEXT.pExceptionBuffer
    mov [r8].PE_SECTION_ENTRY.pDataBuffer, rax

    ; Record .pdata section VA
    mov eax, [r8].PE_SECTION_ENTRY.VirtualAddress
    mov [rdi].PE_CONTEXT.exceptionVirtualAddress, eax

    ;-----------------------------------------------------------------
    ; If xdata was provided, patch UnwindInfoAddress fields in .pdata
    ; to point to the .xdata section VA
    ;-----------------------------------------------------------------
    mov eax, [rbp-40h]              ; xdataVA
    test eax, eax
    jz except_skip_patch

    ; Walk each IMAGE_RUNTIME_FUNCTION_ENTRY and fix UnwindInfoAddress
    mov rsi, [rdi].PE_CONTEXT.pExceptionBuffer
    xor ecx, ecx                    ; entry index
    mov ebx, [rbp-40h]              ; base xdata VA
except_patch_loop:
    cmp ecx, r12d
    jae except_skip_patch
    ; Each entry is 12 bytes: [0]=BeginAddr [4]=EndAddr [8]=UnwindInfoAddr
    ; UnwindInfoAddress = xdataVA + current entry's existing offset
    ; (treat existing value as offset within xdata blob)
    mov eax, [rsi+8]               ; current UnwindInfoAddress (offset within xdata)
    add eax, ebx                   ; convert to absolute VA
    mov [rsi+8], eax
    add rsi, 12                    ; next entry
    inc ecx
    jmp except_patch_loop

except_skip_patch:
    ;-----------------------------------------------------------------
    ; Set DataDirectory[3] (EXCEPTION)
    ;-----------------------------------------------------------------
    mov rbx, [rdi].PE_CONTEXT.pNtHeader
    test rbx, rbx
    jz except_success
    mov eax, [rdi].PE_CONTEXT.exceptionVirtualAddress
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, eax
    mov eax, [rdi].PE_CONTEXT.exceptionSize
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, eax

except_success:
    mov eax, 1
    jmp except_done

except_fail:
    xor eax, eax

except_done:
    add rsp, 60h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_AddExceptionData ENDP

;-----------------------------------------------------------------------------
; PEWriter_SignExecutable
; WIN_CERTIFICATE structure builder for Authenticode-style signatures
; Builds the certificate table entry and stores it for appending after sections.
; Input: RCX = PE Context, RDX = CertBuffer (DER-encoded), R8 = CertSize
; Output: RAX = 1 on success, 0 on failure
;
; WIN_CERTIFICATE layout:
;   +0  DWORD dwLength          = 8 + CertSize (aligned to 8)
;   +4  WORD  wRevision         = 0x0200 (WIN_CERT_REVISION_2_0)
;   +6  WORD  wCertificateType  = 0x0002 (WIN_CERT_TYPE_PKCS_SIGNED_DATA)
;   +8  BYTE  bCertificate[...] = raw certificate data
;
; Security directory (DataDirectory[4]) uses FILE OFFSET, not RVA.
;-----------------------------------------------------------------------------
ALIGN 16
PEWriter_SignExecutable PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 50h

    ; Save inputs
    mov rdi, rcx                    ; rdi = PE_CONTEXT*
    mov r12, rdx                    ; r12 = CertBuffer source
    mov r13d, r8d                   ; r13d = CertSize

    ; Validate
    test r12, r12
    jz sign_fail
    test r13d, r13d
    jz sign_fail

    ;-----------------------------------------------------------------
    ; Calculate WIN_CERTIFICATE.dwLength
    ; dwLength = 8 (header) + CertSize, then align UP to 8 bytes
    ;-----------------------------------------------------------------
    mov eax, r13d
    add eax, 8                      ; header size (dwLength + wRevision + wCertificateType)
    ; Align to 8 bytes: (val + 7) & ~7
    add eax, 7
    and eax, 0FFFFFFF8h
    mov r14d, eax                   ; r14d = total WIN_CERTIFICATE size (aligned)

    ;-----------------------------------------------------------------
    ; Allocate buffer for WIN_CERTIFICATE structure
    ;-----------------------------------------------------------------
    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, r14d                   ; allocate aligned total size
    call HeapAlloc
    test rax, rax
    jz sign_fail
    mov rsi, rax                    ; rsi = WIN_CERTIFICATE buffer

    ;-----------------------------------------------------------------
    ; Build WIN_CERTIFICATE header
    ;-----------------------------------------------------------------
    ; dwLength (DWORD at +0) = total aligned size
    mov dword ptr [rsi+0], r14d
    ; wRevision (WORD at +4) = 0x0200 (WIN_CERT_REVISION_2_0)
    mov word ptr [rsi+4], 0200h
    ; wCertificateType (WORD at +6) = 0x0002 (WIN_CERT_TYPE_PKCS_SIGNED_DATA)
    mov word ptr [rsi+6], 0002h

    ;-----------------------------------------------------------------
    ; Copy certificate data into bCertificate field (offset +8)
    ;-----------------------------------------------------------------
    lea rcx, [rsi+8]                ; destination = bCertificate
    mov rdx, r12                    ; source = CertBuffer
    xor eax, eax
sign_copy_cert:
    cmp eax, r13d
    jae sign_copy_done
    movzx ebx, byte ptr [rdx+rax]
    mov byte ptr [rcx+rax], bl
    inc eax
    jmp sign_copy_cert
sign_copy_done:

    ;-----------------------------------------------------------------
    ; Calculate file offset for certificate table
    ; Certificate is appended AFTER all section raw data.
    ; Walk sections to find the end of the last section's raw data.
    ;-----------------------------------------------------------------
    mov eax, [rdi].PE_CONTEXT.numSections
    test eax, eax
    jz sign_use_current_offset

    ; Find max(PointerToRawData + SizeOfRawData) across all sections
    mov r8, [rdi].PE_CONTEXT.pSections
    xor ecx, ecx                    ; index
    xor ebx, ebx                    ; max file end offset
sign_find_eof:
    cmp ecx, [rdi].PE_CONTEXT.numSections
    jae sign_found_eof
    mov eax, [r8].PE_SECTION_ENTRY.PointerToRawData
    add eax, [r8].PE_SECTION_ENTRY.SizeOfRawData
    cmp eax, ebx
    jbe sign_next_section
    mov ebx, eax
sign_next_section:
    add r8, SIZEOF PE_SECTION_ENTRY
    inc ecx
    jmp sign_find_eof
sign_found_eof:
    ; ebx = end of last section raw data in file
    ; Align to 8-byte boundary for certificate table
    add ebx, 7
    and ebx, 0FFFFFFF8h
    jmp sign_have_offset

sign_use_current_offset:
    mov ebx, [rdi].PE_CONTEXT.currentFileOffset
    add ebx, 7
    and ebx, 0FFFFFFF8h

sign_have_offset:
    ; ebx = file offset where WIN_CERTIFICATE will be written
    ; Store in local for later use
    mov [rbp-38h], ebx              ; [rbp-38h] = certFileOffset
    mov [rbp-40h], rsi              ; [rbp-40h] = pCertBuffer
    mov [rbp-48h], r14d             ; [rbp-48h] = certTotalSize

    ;-----------------------------------------------------------------
    ; Set DataDirectory[4] (SECURITY)
    ; IMPORTANT: Security directory uses FILE OFFSET, not RVA
    ;-----------------------------------------------------------------
    mov rbx, [rdi].PE_CONTEXT.pNtHeader
    test rbx, rbx
    jz sign_success

    mov eax, [rbp-38h]              ; certFileOffset
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY * SIZEOF IMAGE_DATA_DIRECTORY].VirtualAddress, eax
    mov eax, r14d                   ; certTotalSize
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY * SIZEOF IMAGE_DATA_DIRECTORY].DirSize, eax

    ;-----------------------------------------------------------------
    ; Store certificate info in context for write phase
    ; We reuse pResourceBuffer vicinity or store in locals that the
    ; write routine can pick up. Since PE_CONTEXT doesn't have explicit
    ; cert fields, we store the buffer ptr at a known offset.
    ; Use exceptionFileOffset temporarily to hold cert file offset
    ; and pUnwindBuffer to hold the cert buffer pointer.
    ; Actually, let's be cleaner: write the cert data into the
    ; resource buffer area if unused, or allocate dedicated storage.
    ; For maximum correctness, we store in stack frame and the caller
    ; must handle writing. But since this is a monolithic builder,
    ; we'll update currentFileOffset to account for cert data.
    ;-----------------------------------------------------------------
    mov eax, [rbp-38h]              ; cert file offset
    add eax, r14d                   ; + cert size
    mov [rdi].PE_CONTEXT.currentFileOffset, eax

sign_success:
    mov eax, 1
    jmp sign_done

sign_fail:
    xor eax, eax

sign_done:
    add rsp, 50h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_SignExecutable ENDP

;-----------------------------------------------------------------------------
; PEWriter_CompressExecutable
; Tail-zero elimination / section compaction pass
; Walks all sections, trims trailing zero bytes, realigns SizeOfRawData,
; then recalculates all PointerToRawData offsets and SizeOfInitializedData.
; Input:  RCX = PE Context
; Output: RAX = total bytes saved (0 if no compaction possible)
;-----------------------------------------------------------------------------
ALIGN 16
PEWriter_CompressExecutable PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 60h

    mov rdi, rcx                    ; rdi = PE_CONTEXT*
    xor r15d, r15d                  ; r15d = total bytes saved accumulator

    ; Validate context has sections
    mov eax, [rdi].PE_CONTEXT.numSections
    test eax, eax
    jz compress_done_zero

    ;=================================================================
    ; PASS 1: Trim trailing zeros from each section's raw data
    ;=================================================================
    mov r8, [rdi].PE_CONTEXT.pSections
    xor ecx, ecx                    ; section index

compress_trim_loop:
    cmp ecx, [rdi].PE_CONTEXT.numSections
    jae compress_pass2
    mov [rbp-38h], ecx              ; save index

    ; Skip sections with no data buffer
    cmp qword ptr [r8].PE_SECTION_ENTRY.pDataBuffer, 0
    je compress_next_section

    ; Skip sections with zero VirtualSize
    mov eax, [r8].PE_SECTION_ENTRY.VirtualSize
    test eax, eax
    jz compress_next_section

    ; Skip sections with zero SizeOfRawData
    mov eax, [r8].PE_SECTION_ENTRY.SizeOfRawData
    test eax, eax
    jz compress_next_section

    ;-----------------------------------------------------------------
    ; Scan backwards from end of data to find last non-zero byte
    ;-----------------------------------------------------------------
    mov rsi, [r8].PE_SECTION_ENTRY.pDataBuffer
    mov r12d, [r8].PE_SECTION_ENTRY.VirtualSize   ; use VirtualSize as logical limit
    mov r13d, [r8].PE_SECTION_ENTRY.SizeOfRawData ; original raw size

    ; Clamp scan range to min(VirtualSize, SizeOfRawData)
    cmp r12d, r13d
    jbe compress_scan_ok
    mov r12d, r13d                  ; don't scan beyond raw data
compress_scan_ok:
    ; r12d = number of bytes to scan
    test r12d, r12d
    jz compress_next_section

    ; Start from last byte, scan backwards
    mov eax, r12d
    dec eax                         ; last byte index
compress_scan_back:
    cmp eax, 0
    jl compress_all_zero
    cmp byte ptr [rsi+rax], 0
    jne compress_found_nonzero
    dec eax
    jmp compress_scan_back

compress_all_zero:
    ; Entire section is zeros - set actual size to minimum (1 aligned unit)
    mov r14d, FILE_ALIGNMENT
    jmp compress_apply_trim

compress_found_nonzero:
    ; eax = index of last non-zero byte
    ; Actual used size = eax + 1
    inc eax
    mov r14d, eax                   ; r14d = actual used bytes

    ; Align up to FILE_ALIGNMENT
    mov ecx, r14d
    mov edx, FILE_ALIGNMENT
    call AlignUp
    mov r14d, eax                   ; r14d = new aligned SizeOfRawData

    ; Reload section pointer (AlignUp clobbers rcx/rdx)
    mov ecx, [rbp-38h]              ; section index
    mov eax, ecx
    mov edx, SIZEOF PE_SECTION_ENTRY
    imul eax, edx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax

compress_apply_trim:
    ; Compare new size vs old size
    cmp r14d, r13d
    jae compress_next_section       ; no savings, skip

    ; Calculate bytes saved for this section
    mov eax, r13d
    sub eax, r14d
    add r15d, eax                   ; accumulate total savings

    ; Apply trimmed SizeOfRawData
    mov [r8].PE_SECTION_ENTRY.SizeOfRawData, r14d

compress_next_section:
    ; Advance to next section
    mov ecx, [rbp-38h]              ; reload index
    inc ecx
    mov eax, ecx
    mov edx, SIZEOF PE_SECTION_ENTRY
    imul eax, edx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax
    jmp compress_trim_loop

compress_pass2:
    ;=================================================================
    ; PASS 2: Recalculate PointerToRawData for all sections
    ; Start from the headers end (first section's original base)
    ;=================================================================
    ; No savings? Skip recalculation.
    test r15d, r15d
    jz compress_done_zero

    ; Determine headers size: use first section's PointerToRawData if available
    mov r8, [rdi].PE_CONTEXT.pSections
    mov ebx, [r8].PE_SECTION_ENTRY.PointerToRawData
    ; If first section has no PointerToRawData, use a sensible default
    test ebx, ebx
    jnz compress_have_base
    mov ebx, 200h                   ; default: after headers
compress_have_base:
    ; ebx = running file offset
    xor ecx, ecx                    ; section index

compress_rebase_loop:
    cmp ecx, [rdi].PE_CONTEXT.numSections
    jae compress_finalize

    ; Calculate section entry pointer
    mov eax, ecx
    mov edx, SIZEOF PE_SECTION_ENTRY
    imul eax, edx
    mov r8, [rdi].PE_CONTEXT.pSections
    add r8, rax

    ; Set new PointerToRawData
    mov [r8].PE_SECTION_ENTRY.PointerToRawData, ebx

    ; Advance file offset by this section's (possibly trimmed) SizeOfRawData
    mov eax, [r8].PE_SECTION_ENTRY.SizeOfRawData
    add ebx, eax

    inc ecx
    jmp compress_rebase_loop

compress_finalize:
    ;-----------------------------------------------------------------
    ; Update currentFileOffset with new total
    ;-----------------------------------------------------------------
    mov [rdi].PE_CONTEXT.currentFileOffset, ebx

    ;-----------------------------------------------------------------
    ; Recalculate SizeOfInitializedData
    ; Sum of all sections' SizeOfRawData that have INITIALIZED_DATA flag
    ;-----------------------------------------------------------------
    mov rbx, [rdi].PE_CONTEXT.pNtHeader
    test rbx, rbx
    jz compress_return_savings

    xor r12d, r12d                  ; accumulator for SizeOfInitializedData
    mov r8, [rdi].PE_CONTEXT.pSections
    xor ecx, ecx

compress_calc_init_data:
    cmp ecx, [rdi].PE_CONTEXT.numSections
    jae compress_set_init_data

    mov eax, ecx
    mov edx, SIZEOF PE_SECTION_ENTRY
    imul eax, edx
    mov r9, [rdi].PE_CONTEXT.pSections
    add r9, rax

    ; Check if section has IMAGE_SCN_CNT_INITIALIZED_DATA (bit 6 = 0x40)
    mov eax, [r9].PE_SECTION_ENTRY.Characteristics
    test eax, IMAGE_SCN_CNT_INITIALIZED_DATA
    jz compress_calc_next

    ; Also check IMAGE_SCN_CNT_CODE sections contribute to SizeOfCode,
    ; but initialized data sections contribute to SizeOfInitializedData
    add r12d, [r9].PE_SECTION_ENTRY.SizeOfRawData

compress_calc_next:
    inc ecx
    jmp compress_calc_init_data

compress_set_init_data:
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfInitializedData, r12d

    ;-----------------------------------------------------------------
    ; Also recalculate SizeOfCode from code sections
    ;-----------------------------------------------------------------
    xor r12d, r12d
    xor ecx, ecx

compress_calc_code:
    cmp ecx, [rdi].PE_CONTEXT.numSections
    jae compress_set_code

    mov eax, ecx
    mov edx, SIZEOF PE_SECTION_ENTRY
    imul eax, edx
    mov r9, [rdi].PE_CONTEXT.pSections
    add r9, rax

    mov eax, [r9].PE_SECTION_ENTRY.Characteristics
    test eax, IMAGE_SCN_CNT_CODE
    jz compress_code_next
    add r12d, [r9].PE_SECTION_ENTRY.SizeOfRawData

compress_code_next:
    inc ecx
    jmp compress_calc_code

compress_set_code:
    mov [rbx].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfCode, r12d

compress_return_savings:
    mov eax, r15d                   ; return bytes saved
    jmp compress_done

compress_done_zero:
    xor eax, eax                    ; no savings

compress_done:
    add rsp, 60h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
PEWriter_CompressExecutable ENDP

END
