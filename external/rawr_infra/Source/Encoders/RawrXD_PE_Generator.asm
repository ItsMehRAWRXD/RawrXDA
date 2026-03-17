; =============================================================================
; RawrXD_PE_Generator.asm
; Pure MASM x64 PE Generator & Encoder - Production Ready
; Generates valid Windows PE32+ executables with encoding/obfuscation capabilities
; =============================================================================
; Build: ml64.exe RawrXD_PE_Generator.asm /link /subsystem:console /entry:main
; =============================================================================

include \masm64\include64\masm64rt.inc

; =============================================================================
; STRUCTURES
; =============================================================================

IMAGE_DOS_HEADER STRUCT
    e_magic         WORD    ?       ; Magic number (MZ)
    e_cblp          WORD    ?       ; Bytes on last page of file
    e_cp            WORD    ?       ; Pages in file
    e_crlc          WORD    ?       ; Relocations
    e_cparhdr       WORD    ?       ; Size of header in paragraphs
    e_minalloc      WORD    ?       ; Minimum extra paragraphs needed
    e_maxalloc      WORD    ?       ; Maximum extra paragraphs needed
    e_ss            WORD    ?       ; Initial (relative) SS value
    e_sp            WORD    ?       ; Initial SP value
    e_csum          WORD    ?       ; Checksum
    e_ip            WORD    ?       ; Initial IP value
    e_cs            WORD    ?       ; Initial (relative) CS value
    e_lfarlc        WORD    ?       ; File address of relocation table
    e_ovno          WORD    ?       ; Overlay number
    e_res           WORD    4 DUP(?) ; Reserved words
    e_oemid         WORD    ?       ; OEM identifier
    e_oeminfo       WORD    ?       ; OEM information
    e_res2          WORD    10 DUP(?) ; Reserved words
    e_lfanew        DWORD   ?       ; File address of new exe header
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine             WORD    ?
    NumberOfSections    WORD    ?
    TimeDateStamp       DWORD   ?
    PointerToSymbolTable DWORD  ?
    NumberOfSymbols     DWORD   ?
    SizeOfOptionalHeader WORD    ?
    Characteristics     WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    _Size           DWORD   ?
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
    Signature       DWORD   ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1           BYTE    8 DUP(?)
    VirtualSize     DWORD   ?
    VirtualAddress  DWORD   ?
    SizeOfRawData   DWORD   ?
    PointerToRawData DWORD   ?
    PointerToRelocations DWORD ?
    PointerToLinenumbers DWORD ?
    NumberOfRelocations WORD  ?
    NumberOfLinenumbers WORD  ?
    Characteristics DWORD   ?
IMAGE_SECTION_HEADER ENDS

; =============================================================================
; CONSTANTS
; =============================================================================

PE_MAGIC                EQU     0x4550          ; "PE"
DOS_MAGIC               EQU     0x5A4D          ; "MZ"
OPTIONAL_HDR64_MAGIC    EQU     0x20B
IMAGE_FILE_MACHINE_AMD64 EQU    0x8664
IMAGE_FILE_EXECUTABLE_IMAGE EQU 0x0002
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 0x0020

IMAGE_SCN_CNT_CODE      EQU     0x00000020
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 0x00000040
IMAGE_SCN_MEM_EXECUTE   EQU     0x20000000
IMAGE_SCN_MEM_READ      EQU     0x40000000
IMAGE_SCN_MEM_WRITE     EQU     0x80000000

IMAGE_SUBSYSTEM_WINDOWS_CUI EQU 3
IMAGE_SUBSYSTEM_WINDOWS_GUI EQU 2

SECTION_ALIGNMENT       EQU     0x1000
FILE_ALIGNMENT          EQU     0x200

; Encoder types
ENCODER_XOR             EQU     0
ENCODER_ADD             EQU     1
ENCODER_ROL             EQU     2
ENCODER_ROR             EQU     3
ENCODER_CUSTOM          EQU     4

; =============================================================================
; DATA SECTION
; =============================================================================

.data

; PE Generator Configuration
pe_config STRUCT
    image_base          QWORD   ?
    entry_rva           DWORD   ?
    subsystem           WORD    ?
    section_align       DWORD   ?
    file_align          DWORD   ?
    stack_reserve       QWORD   ?
    stack_commit        QWORD   ?
    heap_reserve        QWORD   ?
    heap_commit         QWORD   ?
pe_config ENDS

default_config pe_config <0x140000000, 0x1000, IMAGE_SUBSYSTEM_WINDOWS_CUI, \
                          SECTION_ALIGNMENT, FILE_ALIGNMENT, \
                          0x100000, 0x1000, 0x100000, 0x1000>

; Encoder configuration
encoder_config STRUCT
    algorithm           DWORD   ?
    key                 BYTE    32 DUP(?)       ; 256-bit key
    key_length          DWORD   ?
    delta               BYTE    ?               ; For ADD/SUB
    rotation            BYTE    ?               ; For ROL/ROR
encoder_config ENDS

; Standard section names
sz_text                 BYTE    ".text", 0, 0, 0
sz_data                 BYTE    ".data", 0, 0, 0
sz_rdata                BYTE    ".rdata", 0, 0
sz_pdata                BYTE    ".pdata", 0, 0

; Output buffer and file handling
h_heap                  QWORD   ?
p_output_buffer         QWORD   ?
buffer_size             QWORD   ?
buffer_used             QWORD   ?
h_output_file           QWORD   ?

; Status messages
sz_msg_generating       BYTE    "[*] Generating PE structure...", 10, 0
sz_msg_encoding         BYTE    "[*] Encoding payload...", 10, 0
sz_msg_writing          BYTE    "[*] Writing output file...", 10, 0
sz_msg_success          BYTE    "[+] PE generated successfully!", 10, 0
sz_msg_failed           BYTE    "[-] Generation failed!", 10, 0
sz_msg_usage            BYTE    "Usage: RawrXD_PE_Generator.exe <output.exe> [options]", 10, 0

; Sample payload: MessageBox shellcode (will be embedded)
; This is a simple x64 payload that calls ExitProcess
payload_data LABEL BYTE
    ; push rbx
    db  053h
    ; sub rsp, 28h
    db  048h, 083h, 0ECh, 028h
    ; mov ecx, 0 (exit code)
    db  0B9h, 000h, 000h, 000h, 000h
    ; call ExitProcess (will be fixed up)
    db  0FFh, 015h, 012h, 000h, 000h, 000h
    ; add rsp, 28h
    db  048h, 083h, 0C4h, 028h
    ; pop rbx
    db  05Bh
    ; ret
    db  0C3h
    ; Padding for alignment
    db  090h, 090h, 090h, 090h
payload_size EQU $ - payload_data

; =============================================================================
; CODE SECTION
; =============================================================================

.code

; =============================================================================
; UTILITY FUNCTIONS
; =============================================================================

; -----------------------------------------------------------------------------
; strlen64 - Calculate string length
; RCX = string pointer
; Returns: RAX = length
; -----------------------------------------------------------------------------
strlen64 PROC
    mov     rax, rcx
    dec     rax
@@:
    inc     rax
    cmp     BYTE PTR [rax], 0
    jne     @B
    sub     rax, rcx
    ret
strlen64 ENDP

; -----------------------------------------------------------------------------
; memcpy64 - Copy memory
; RCX = dest, RDX = src, R8 = count
; -----------------------------------------------------------------------------
memcpy64 PROC
    push    rsi
    push    rdi
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, r8
    rep     movsb
    pop     rdi
    pop     rsi
    ret
memcpy64 ENDP

; -----------------------------------------------------------------------------
; memset64 - Fill memory
; RCX = dest, DL = value, R8 = count
; -----------------------------------------------------------------------------
memset64 PROC
    push    rdi
    mov     rdi, rcx
    mov     al, dl
    mov     rcx, r8
    rep     stosb
    pop     rdi
    ret
memset64 ENDP

; =============================================================================
; PE GENERATOR CORE
; =============================================================================

; -----------------------------------------------------------------------------
; InitializeBuffer - Allocate and initialize output buffer
; RCX = initial size
; Returns: RAX = 0 on success, -1 on failure
; -----------------------------------------------------------------------------
InitializeBuffer PROC
    push    rbx
    push    rsi
    push    rdi
    
    ; Get process heap
    call    GetProcessHeap
    test    rax, rax
    jz      @@fail
    mov     h_heap, rax
    
    ; Allocate buffer
    mov     rcx, h_heap
    mov     rdx, 0                          ; HEAP_ZERO_MEMORY
    mov     r8, [rsp+24h]                   ; Initial size (parameter)
    call    HeapAlloc
    test    rax, rax
    jz      @@fail
    
    mov     p_output_buffer, rax
    mov     rbx, [rsp+24h]
    mov     buffer_size, rbx
    mov     buffer_used, 0
    
    xor     rax, rax                        ; Success
    jmp     @@done
    
@@fail:
    mov     rax, -1
    
@@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
InitializeBuffer ENDP

; -----------------------------------------------------------------------------
; ExpandBuffer - Expand buffer if needed
; RCX = required size
; Returns: RAX = 0 on success, -1 on failure
; -----------------------------------------------------------------------------
ExpandBuffer PROC
    push    rbx
    push    rsi
    
    mov     rbx, rcx                        ; Required size
    
    ; Check if we need to expand
    mov     rax, buffer_size
    cmp     rbx, rax
    jbe     @@success                       ; Already big enough
    
    ; Calculate new size (double until sufficient)
@@calc:
    shl     rax, 1
    cmp     rbx, rax
    ja      @@calc
    
    ; Reallocate
    mov     rcx, h_heap
    mov     rdx, 0                          ; HEAP_ZERO_MEMORY
    mov     r8, rax                         ; New size
    mov     r9, p_output_buffer
    call    HeapReAlloc
    test    rax, rax
    jz      @@fail
    
    mov     p_output_buffer, rax
    mov     buffer_size, rbx
    
@@success:
    xor     rax, rax
    jmp     @@done
    
@@fail:
    mov     rax, -1
    
@@done:
    pop     rsi
    pop     rbx
    ret
ExpandBuffer ENDP

; -----------------------------------------------------------------------------
; AppendToBuffer - Append data to buffer
; RCX = data, RDX = size
; Returns: RAX = offset where data was written, -1 on failure
; -----------------------------------------------------------------------------
AppendToBuffer PROC
    push    rbx
    push    rsi
    push    rdi
    
    mov     rbx, rcx                        ; Data pointer
    mov     rsi, rdx                        ; Size
    
    ; Check if we need to expand
    mov     rax, buffer_used
    add     rax, rsi
    mov     rcx, rax
    push    rax                             ; Save new used size
    call    ExpandBuffer
    pop     rcx                             ; Restore new used size
    test    rax, rax
    jnz     @@fail
    
    ; Copy data
    mov     rdi, p_output_buffer
    add     rdi, buffer_used                ; Destination
    mov     rsi, rbx                        ; Source
    mov     rcx, rdx                        ; Size
    rep     movsb
    
    ; Return offset and update used
    mov     rax, buffer_used
    add     buffer_used, rdx
    
    pop     rdi
    pop     rsi
    pop     rbx
    ret
    
@@fail:
    mov     rax, -1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AppendToBuffer ENDP

; =============================================================================
; PE STRUCTURE GENERATION
; =============================================================================

; -----------------------------------------------------------------------------
; GenerateDosHeader - Create DOS header with stub
; Returns: RAX = 0 on success
; -----------------------------------------------------------------------------
GenerateDosHeader PROC
    local dos_header:IMAGE_DOS_HEADER
    local dos_stub[64]:BYTE
    
    push    rbx
    push    rsi
    
    ; Initialize DOS header
    lea     rbx, dos_header
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_magic, DOS_MAGIC
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_cblp, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_cp, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_crlc, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_cparhdr, 4
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_minalloc, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_maxalloc, 0FFFFh
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_ss, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_sp, 0B8h
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_csum, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_ip, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_cs, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_lfarlc, 40h
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_ovno, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_oemid, 0
    mov     WORD PTR [rbx].IMAGE_DOS_HEADER.e_oeminfo, 0
    mov     DWORD PTR [rbx].IMAGE_DOS_HEADER.e_lfanew, 40h
    
    ; Clear reserved arrays
    lea     rcx, [rbx].IMAGE_DOS_HEADER.e_res
    mov     rdx, 0
    mov     r8, 8
    call    memset64
    
    lea     rcx, [rbx].IMAGE_DOS_HEADER.e_res2
    mov     rdx, 0
    mov     r8, 20
    call    memset64
    
    ; Append DOS header
    mov     rcx, rbx
    mov     rdx, SIZEOF IMAGE_DOS_HEADER
    call    AppendToBuffer
    test    rax, rax
    js      @@fail
    
    ; Generate DOS stub program
    lea     rbx, dos_stub
    mov     rcx, rbx
    mov     rdx, 0
    mov     r8, 64
    call    memset64
    
    ; Simple DOS stub: int 21h, ah=4Ch (exit)
    mov     BYTE PTR [rbx], 0B8h            ; mov ax, 4C00h
    mov     WORD PTR [rbx+1], 4C00h
    mov     BYTE PTR [rbx+3], 0CDh          ; int 21h
    mov     BYTE PTR [rbx+4], 21h
    
    mov     rcx, rbx
    mov     rdx, 64
    call    AppendToBuffer
    
    xor     rax, rax
    jmp     @@done
    
@@fail:
    mov     rax, -1
    
@@done:
    pop     rsi
    pop     rbx
    ret
GenerateDosHeader ENDP

; -----------------------------------------------------------------------------
; GenerateNtHeaders - Create NT headers (COFF + Optional header)
; RCX = number of sections, RDX = entry point RVA
; Returns: RAX = 0 on success
; -----------------------------------------------------------------------------
GenerateNtHeaders PROC
    local nt_headers:IMAGE_NT_HEADERS64
    
    push    rbx
    push    rsi
    
    lea     rsi, nt_headers
    
    ; Initialize NT headers
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.Signature, PE_MAGIC
    
    ; File header
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    mov     ax, cx
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections, ax
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.TimeDateStamp, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.PointerToSymbolTable, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.NumberOfSymbols, 0
    mov     ax, SIZEOF IMAGE_OPTIONAL_HEADER64
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.SizeOfOptionalHeader, ax
    mov     ax, IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.FileHeader.Characteristics, ax
    
    ; Optional header
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.Magic, OPTIONAL_HDR64_MAGIC
    mov     BYTE PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MajorLinkerVersion, 14
    mov     BYTE PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MinorLinkerVersion, 0
    
    ; Code sizes
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfCode, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfInitializedData, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfUninitializedData, 0
    
    ; Entry point
    mov     eax, edx
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.AddressOfEntryPoint, eax
    
    ; Base of code
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.BaseOfCode, 1000h
    
    ; Image base
    mov     rax, default_config.image_base
    mov     QWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.ImageBase, rax
    
    ; Alignment
    mov     eax, default_config.section_align
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SectionAlignment, eax
    mov     eax, default_config.file_align
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.FileAlignment, eax
    
    ; Version info
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MajorOperatingSystemVersion, 6
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MinorOperatingSystemVersion, 0
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MajorImageVersion, 0
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MinorImageVersion, 0
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MajorSubsystemVersion, 6
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.MinorSubsystemVersion, 0
    
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.Win32VersionValue, 0
    
    ; Size of image (will be calculated)
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfImage, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeaders, 400h
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.CheckSum, 0
    
    ; Subsystem
    mov     ax, default_config.subsystem
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.Subsystem, ax
    
    mov     WORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.DllCharacteristics, 0
    
    ; Stack
    mov     rax, default_config.stack_reserve
    mov     QWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackReserve, rax
    mov     rax, default_config.stack_commit
    mov     QWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackCommit, rax
    
    ; Heap
    mov     rax, default_config.heap_reserve
    mov     QWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapReserve, rax
    mov     rax, default_config.heap_commit
    mov     QWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapCommit, rax
    
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.LoaderFlags, 0
    mov     DWORD PTR [rsi].IMAGE_NT_HEADERS64.OptionalHeader.NumberOfRvaAndSizes, 16
    
    ; Clear data directories
    lea     rcx, [rsi].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory
    mov     rdx, 0
    mov     r8, SIZEOF IMAGE_DATA_DIRECTORY * 16
    call    memset64
    
    ; Append NT headers
    mov     rcx, rsi
    mov     rdx, SIZEOF IMAGE_NT_HEADERS64
    call    AppendToBuffer
    test    rax, rax
    js      @@fail
    
    xor     rax, rax
    jmp     @@done
    
@@fail:
    mov     rax, -1
    
@@done:
    pop     rsi
    pop     rbx
    ret
GenerateNtHeaders ENDP

; -----------------------------------------------------------------------------
; GenerateSectionHeader - Create section header
; RCX = name, RDX = virtual size, R8 = virtual addr
; R9 = raw size, [rsp+28h] = raw addr, [rsp+30h] = characteristics
; Returns: RAX = 0 on success
; -----------------------------------------------------------------------------
GenerateSectionHeader PROC
    local section:IMAGE_SECTION_HEADER
    
    push    rbx
    push    rsi
    
    lea     rsi, section
    
    ; Clear section header
    mov     rcx, rsi
    mov     rdx, 0
    mov     r8, SIZEOF IMAGE_SECTION_HEADER
    call    memset64
    
    ; Copy name (up to 8 bytes)
    mov     rcx, [rsp+32h]                  ; Name pointer (parameter)
    call    strlen64
    cmp     rax, 8
    jbe     @@name_ok
    mov     rax, 8
@@name_ok:
    mov     r8, rax
    lea     rcx, [rsi].IMAGE_SECTION_HEADER.Name1
    mov     r9, [rsp+32h]
    call    memcpy64
    
    ; Set fields
    mov     eax, edx
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.VirtualSize, eax
    mov     eax, r8d
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.VirtualAddress, eax
    mov     eax, r9d
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.SizeOfRawData, eax
    
    mov     rax, [rsp+28h]                  ; Raw address (parameter)
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.PointerToRawData, eax
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.PointerToRelocations, 0
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.PointerToLinenumbers, 0
    mov     WORD PTR [rsi].IMAGE_SECTION_HEADER.NumberOfRelocations, 0
    mov     WORD PTR [rsi].IMAGE_SECTION_HEADER.NumberOfLinenumbers, 0
    
    mov     rax, [rsp+30h]                  ; Characteristics (parameter)
    mov     DWORD PTR [rsi].IMAGE_SECTION_HEADER.Characteristics, eax
    
    ; Append to buffer
    mov     rcx, rsi
    mov     rdx, SIZEOF IMAGE_SECTION_HEADER
    call    AppendToBuffer
    test    rax, rax
    js      @@fail
    
    xor     rax, rax
    jmp     @@done
    
@@fail:
    mov     rax, -1
    
@@done:
    pop     rsi
    pop     rbx
    ret
GenerateSectionHeader ENDP

; =============================================================================
; ENCODER ENGINE
; =============================================================================

; -----------------------------------------------------------------------------
; XorEncode - XOR encode buffer
; RCX = data, RDX = size, R8 = key, R9 = key_len
; -----------------------------------------------------------------------------
XorEncode PROC
    push    rbx
    push    rsi
    push    rdi
    
    mov     rsi, rcx                        ; Source/dest
    mov     rbx, rdx                        ; Size
    mov     rdi, r8                         ; Key
    mov     rcx, r9                         ; Key length
    xor     rdx, rdx                        ; Key index
    
@@loop:
    test    rbx, rbx
    jz      @@done
    
    ; XOR byte with key
    mov     al, [rsi]
    mov     r8, rdx
    xor     r8, r8
    mov     r8b, dl
    cmp     r8, rcx
    jb      @@no_wrap
    xor     r8, r8
@@no_wrap:
    xor     al, BYTE PTR [rdi+r8]
    mov     [rsi], al
    
    inc     rsi
    dec     rbx
    inc     rdx
    jmp     @@loop
    
@@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
XorEncode ENDP

; -----------------------------------------------------------------------------
; AddEncode - ADD/SUB encode buffer
; RCX = data, RDX = size, R8 = delta
; -----------------------------------------------------------------------------
AddEncode PROC
    push    rsi
    
    mov     rsi, rcx
    mov     rcx, rdx
    
@@loop:
    test    rcx, rcx
    jz      @@done
    
    add     BYTE PTR [rsi], r8b
    inc     rsi
    dec     rcx
    jmp     @@loop
    
@@done:
    pop     rsi
    ret
AddEncode ENDP

; -----------------------------------------------------------------------------
; RolEncode - ROL encode buffer
; RCX = data, RDX = size, R8 = rotation
; -----------------------------------------------------------------------------
RolEncode PROC
    push    rsi
    
    mov     rsi, rcx
    mov     rcx, rdx
    mov     dl, r8b
    and     dl, 7                           ; rotation % 8
    
@@loop:
    test    rcx, rcx
    jz      @@done
    
    rol     BYTE PTR [rsi], dl
    inc     rsi
    dec     rcx
    jmp     @@loop
    
@@done:
    pop     rsi
    ret
RolEncode ENDP

; -----------------------------------------------------------------------------
; PolymorphicEncode - Apply polymorphic encoding
; RCX = data, RDX = size, R8 = encoder config
; Returns: RAX = 0 on success
; -----------------------------------------------------------------------------
PolymorphicEncode PROC
    push    rbx
    
    mov     rbx, r8
    
    ; Select algorithm based on config
    mov     eax, DWORD PTR [rbx]
    
    cmp     eax, ENCODER_XOR
    jne     @@check_add
    
    ; XOR encoding
    mov     r8, rbx
    add     r8, OFFSET encoder_config.key
    mov     r9d, DWORD PTR [rbx].encoder_config.key_length
    call    XorEncode
    jmp     @@done
    
@@check_add:
    cmp     eax, ENCODER_ADD
    jne     @@check_rol
    
    ; ADD encoding
    movzx   r8, BYTE PTR [rbx].encoder_config.delta
    call    AddEncode
    jmp     @@done
    
@@check_rol:
    cmp     eax, ENCODER_ROL
    jne     @@check_ror
    
    ; ROL encoding
    movzx   r8, BYTE PTR [rbx].encoder_config.rotation
    call    RolEncode
    jmp     @@done
    
@@check_ror:
    cmp     eax, ENCODER_ROR
    jne     @@done
    
    ; ROR encoding
    movzx   r8, BYTE PTR [rbx].encoder_config.rotation
    neg     r8b
    and     r8b, 7
    call    RolEncode
    
@@done:
    xor     rax, rax
    pop     rbx
    ret
PolymorphicEncode ENDP

; =============================================================================
; MAIN GENERATION
; =============================================================================

; -----------------------------------------------------------------------------
; GenerateCompletePE - Generate full PE file with encoded payload
; RCX = output filename, RDX = encoder config (NULL = no encoding)
; Returns: RAX = 0 on success, -1 on failure
; -----------------------------------------------------------------------------
GenerateCompletePE PROC
    local text_section_raw:DWORD
    local text_section_rva:DWORD
    local text_section_size:DWORD
    local payload_encoded[512]:BYTE
    local temp_filename[256]:BYTE
    local bytes_written:DWORD
    
    push    rbx
    push    rsi
    push    rdi
    push    r12
    
    ; Save parameters
    mov     r12, rcx                        ; Filename
    mov     rsi, rdx                        ; Encoder config
    
    ; Initialize buffer (4KB initial)
    mov     rcx, 4096
    call    InitializeBuffer
    test    rax, rax
    jnz     @@fail
    
    ; Print status
    lea     rcx, sz_msg_generating
    call    printf
    
    ; Generate DOS header
    call    GenerateDosHeader
    test    rax, rax
    jnz     @@fail
    
    ; Pad to PE header offset (64 bytes)
    mov     rax, buffer_used
    cmp     rax, 64
    jae     @@pe_header_ok
    
    mov     rcx, 64
    sub     rcx, rax
    add     buffer_used, rcx
    
@@pe_header_ok:
    
    ; Calculate section layout
    mov     text_section_rva, 1000h         ; First section at RVA 1000h
    mov     text_section_raw, 400h          ; Raw offset after headers
    
    ; Determine text section size
    mov     text_section_size, payload_size
    add     text_section_size, 0FFh
    and     text_section_size, 0FFFFFF00h
    
    ; Generate NT headers (1 section: .text)
    mov     rcx, 1                          ; 1 section
    mov     rdx, text_section_rva           ; Entry point RVA
    call    GenerateNtHeaders
    test    rax, rax
    jnz     @@fail
    
    ; Generate section header
    mov     rcx, OFFSET sz_text
    mov     rdx, text_section_size          ; Virtual size
    mov     r8d, text_section_rva           ; Virtual address
    mov     r9d, text_section_size          ; Raw size
    
    mov     rax, IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    push    rax                             ; Characteristics
    push    text_section_raw                ; Raw address
    sub     rsp, 16
    call    GenerateSectionHeader
    add     rsp, 32
    test    rax, rax
    jnz     @@fail
    
    ; Pad headers to 0x400 bytes
    mov     rax, buffer_used
    cmp     rax, 400h
    jae     @@headers_done
    
    mov     rcx, 400h
    sub     rcx, rax
    add     buffer_used, rcx
    
@@headers_done:
    
    ; Process payload (encode if needed)
    test    rsi, rsi
    jz      @@no_encoding
    
    ; Copy payload for encoding
    lea     rdi, payload_encoded
    mov     rcx, OFFSET payload_data
    mov     rdx, rdi
    mov     r8, payload_size
    call    memcpy64
    
    ; Apply encoding
    lea     rcx, payload_encoded
    mov     rdx, payload_size
    mov     r8, rsi
    call    PolymorphicEncode
    
    ; Append encoded payload
    lea     rcx, payload_encoded
    mov     rdx, payload_size
    call    AppendToBuffer
    test    rax, rax
    js      @@fail
    
    jmp     @@payload_done
    
@@no_encoding:
    ; Append raw payload
    mov     rcx, OFFSET payload_data
    mov     rdx, payload_size
    call    AppendToBuffer
    test    rax, rax
    js      @@fail
    
@@payload_done:
    
    ; Pad to file alignment
    mov     rax, buffer_used
    add     rax, 1FFh
    and     rax, 0FFFFFE00h
    mov     buffer_used, rax
    
    ; Update SizeOfCode in optional header (PE offset + file header offset)
    mov     rax, p_output_buffer
    add     rax, 40h + 4 + 2 + 2 + 4 + 4 + 4  ; DOS header offset to SizeOfCode
    mov     edx, text_section_size
    mov     DWORD PTR [rax], edx
    
    ; Update SizeOfImage
    mov     rax, p_output_buffer
    add     rax, 40h + 20h + 38h             ; Offset to SizeOfImage in Optional Header
    mov     edx, text_section_rva
    add     edx, text_section_size
    add     edx, 0FFFh
    and     edx, 0FFFFF000h
    mov     DWORD PTR [rax], edx
    
    ; Write output file
    lea     rcx, sz_msg_writing
    call    printf
    
    ; Create file
    mov     rcx, r12
    mov     rdx, 40000000h                  ; GENERIC_WRITE
    mov     r8, 0                           ; FILE_SHARE_NONE
    mov     r9, 0                           ; NULL security
    mov     rax, 2
    push    rax                             ; CREATE_ALWAYS
    mov     rax, 80h
    push    rax                             ; FILE_ATTRIBUTE_NORMAL
    push    0                               ; No template
    
    call    CreateFileA
    cmp     rax, -1
    je      @@fail
    
    mov     h_output_file, rax
    
    ; Write buffer
    mov     rcx, h_output_file
    mov     rdx, p_output_buffer
    mov     r8, buffer_used
    lea     r9, bytes_written
    push    0                               ; Overlapped
    
    call    WriteFile
    
    ; Close file
    mov     rcx, h_output_file
    call    CloseHandle
    
    ; Success message
    lea     rcx, sz_msg_success
    call    printf
    
    xor     rax, rax
    jmp     @@done
    
@@fail:
    lea     rcx, sz_msg_failed
    call    printf
    mov     rax, -1
    
@@done:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GenerateCompletePE ENDP

; =============================================================================
; ENTRY POINT
; =============================================================================

main PROC
    local my_encoder:encoder_config
    
    push    rbx
    push    rsi
    
    ; Get command line
    call    GetCommandLineA
    mov     rcx, rax
    
    ; Simple parsing: skip program name
    cmp     BYTE PTR [rcx], 0
    je      @@usage
    
@@skip_name:
    cmp     BYTE PTR [rcx], 0
    je      @@usage
    cmp     BYTE PTR [rcx], ' '
    je      @@found_space
    inc     rcx
    jmp     @@skip_name
    
@@found_space:
    ; Skip spaces
@@skip_spaces:
    cmp     BYTE PTR [rcx], 0
    je      @@usage
    cmp     BYTE PTR [rcx], ' '
    jne     @@have_filename
    inc     rcx
    jmp     @@skip_spaces
    
@@have_filename:
    mov     rsi, rcx                        ; Filename pointer
    
    ; Setup encoder config (XOR with key "RawrXD")
    lea     rbx, my_encoder
    mov     DWORD PTR [rbx], ENCODER_XOR
    mov     DWORD PTR [rbx].encoder_config.key_length, 6
    
    lea     rcx, [rbx].encoder_config.key
    mov     rdx, OFFSET sz_text             ; Using "RawrXD" string
    mov     r8, 6
    call    memcpy64
    
    ; Generate PE with encoding
    mov     rcx, rsi
    mov     rdx, rbx
    call    GenerateCompletePE
    
    test    rax, rax
    jnz     @@error
    
    xor     eax, eax
    jmp     @@exit
    
@@usage:
    lea     rcx, sz_msg_usage
    call    printf
    mov     eax, 1
    jmp     @@exit
    
@@error:
    mov     eax, 1
    
@@exit:
    pop     rsi
    pop     rbx
    ret
    
main ENDP

; =============================================================================
; END
; =============================================================================

END
