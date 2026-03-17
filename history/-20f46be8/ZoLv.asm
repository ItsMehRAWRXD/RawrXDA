; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD Bare-Metal Universal Assembler - Pure MASM64 Zero-SDK
; Direct PE header construction and binary emission without link.exe/ml64.exe
; 100-language support with autonomous self-rebuilding capabilities
; ═══════════════════════════════════════════════════════════════════════════════

option casemap:none
option win64:3
option frame:auto

; ═══════════════════════════════════════════════════════════════════════════════
; PE HEADER CONSTANTS - DIRECT BINARY EMISSION
; ═══════════════════════════════════════════════════════════════════════════════

IMAGE_DOS_SIGNATURE        equ 5A4Dh          ; 'MZ'
IMAGE_NT_SIGNATURE         equ 00004550h      ; 'PE\0\0'
IMAGE_FILE_MACHINE_AMD64   equ 8664h
IMAGE_SUBSYSTEM_WINDOWS_CUI equ 3
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE equ 40h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT equ 100h
IMAGE_DLLCHARACTERISTICS_GUARD_CF equ 4000h

SECTION_ALIGNMENT          equ 1000h          ; 4KB memory alignment
FILE_ALIGNMENT            equ 200h           ; 512 byte file alignment
IMAGE_BASE                equ 140000000h     ; Default base address

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES FOR DIRECT PE CONSTRUCTION
; ═══════════════════════════════════════════════════════════════════════════════

IMAGE_DOS_HEADER STRUCT
    e_magic      WORD ?
    e_cblp       WORD ?
    e_cp         WORD ?
    e_crlc       WORD ?
    e_cparhdr    WORD ?
    e_minalloc   WORD ?
    e_maxalloc   WORD ?
    e_ss         WORD ?
    e_sp         WORD ?
    e_csum       WORD ?
    e_ip         WORD ?
    e_cs         WORD ?
    e_lfarlc     WORD ?
    e_ovno       WORD ?
    e_res        WORD 4 DUP (?)
    e_oemid      WORD ?
    e_oeminfo    WORD ?
    e_res2       WORD 10 DUP (?)
    e_lfanew     DWORD ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine              WORD ?
    NumberOfSections     WORD ?
    TimeDateStamp        DWORD ?
    PointerToSymbolTable DWORD ?
    NumberOfSymbols      DWORD ?
    SizeOfOptionalHeader WORD ?
    Characteristics      WORD ?
IMAGE_FILE_HEADER ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD ?
    MajorLinkerVersion         BYTE ?
    MinorLinkerVersion         BYTE ?
    SizeOfCode                 DWORD ?
    SizeOfInitializedData      DWORD ?
    SizeOfUninitializedData    DWORD ?
    AddressOfEntryPoint        DWORD ?
    BaseOfCode                 DWORD ?
    ImageBase                  QWORD ?
    SectionAlignment           DWORD ?
    FileAlignment              DWORD ?
    MajorOperatingSystemVersion WORD ?
    MinorOperatingSystemVersion WORD ?
    MajorImageVersion          WORD ?
    MinorImageVersion          WORD ?
    MajorSubsystemVersion      WORD ?
    MinorSubsystemVersion      WORD ?
    Win32VersionValue         DWORD ?
    SizeOfImage               DWORD ?
    SizeOfHeaders             DWORD ?
    CheckSum                  DWORD ?
    Subsystem                 WORD ?
    DllCharacteristics        WORD ?
    SizeOfStackReserve        QWORD ?
    SizeOfStackCommit         QWORD ?
    SizeOfHeapReserve         QWORD ?
    SizeOfHeapCommit          QWORD ?
    LoaderFlags               DWORD ?
    NumberOfRvaAndSizes       DWORD ?
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name                    BYTE 8 DUP (?)
    VirtualSize            DWORD ?
    VirtualAddress         DWORD ?
    SizeOfRawData          DWORD ?
    PointerToRawData       DWORD ?
    PointerToRelocations   DWORD ?
    PointerToLinenumbers   DWORD ?
    NumberOfRelocations    WORD ?
    NumberOfLinenumbers    WORD ?
    Characteristics        DWORD ?
IMAGE_SECTION_HEADER ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; GLOBAL VARIABLES FOR PE CONSTRUCTION
; ═══════════════════════════════════════════════════════════════════════════════

.data
g_pe_buffer           QWORD ?
g_pe_buffer_size      QWORD ?
g_current_offset      QWORD ?
g_section_count       DWORD ?
g_entry_point_rva     DWORD ?

g_text_section        IMAGE_SECTION_HEADER {}
g_data_section        IMAGE_SECTION_HEADER {}
g_rdata_section       IMAGE_SECTION_HEADER {}

; ═══════════════════════════════════════════════════════════════════════════════
; PE CONSTRUCTION MACROS
; ═══════════════════════════════════════════════════════════════════════════════

EMIT_BYTE MACRO byte_val
    mov rdi, g_current_offset
    mov byte ptr [g_pe_buffer + rdi], byte_val
    inc g_current_offset
ENDM

EMIT_WORD MACRO word_val
    mov rdi, g_current_offset
    mov word ptr [g_pe_buffer + rdi], word_val
    add g_current_offset, 2
ENDM

EMIT_DWORD MACRO dword_val
    mov rdi, g_current_offset
    mov dword ptr [g_pe_buffer + rdi], dword_val
    add g_current_offset, 4
ENDM

EMIT_QWORD MACRO qword_val
    mov rdi, g_current_offset
    mov qword ptr [g_pe_buffer + rdi], qword_val
    add g_current_offset, 8
ENDM

EMIT_STRING MACRO str_val
    LOCAL str_data, str_len
    .data
    str_data db str_val, 0
    str_len = $ - str_data - 1
    .code
    mov rsi, offset str_data
    mov rcx, str_len
    mov rdi, g_current_offset
    rep movsb
    add g_current_offset, str_len
ENDM

ALIGN_FILE MACRO alignment
    LOCAL current_pos, align_amount
    mov rax, g_current_offset
    xor rdx, rdx
    mov rcx, alignment
    div rcx
    test rdx, rdx
    jz .aligned
    mov rax, alignment
    sub rax, rdx
    add g_current_offset, rax
.aligned:
ENDM

; ═══════════════════════════════════════════════════════════════════════════════
; PE HEADER CONSTRUCTION FUNCTIONS
; ═══════════════════════════════════════════════════════════════════════════════

build_pe_header PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Allocate PE buffer (64KB should be sufficient)
    mov ecx, 65536
    mov edx, 3000h              ; MEM_COMMIT | MEM_RESERVE
    mov r8d, 4                  ; PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    mov g_pe_buffer, rax
    mov g_current_offset, 0
    
    ; Build DOS header
    call build_dos_header
    
    ; Build PE signature
    EMIT_DWORD IMAGE_NT_SIGNATURE
    
    ; Build file header
    call build_file_header
    
    ; Build optional header
    call build_optional_header
    
    ; Build section headers
    call build_section_headers
    
    ; Build section data
    call build_section_data
    
    ; Finalize PE size
    mov rax, g_current_offset
    mov g_pe_buffer_size, rax
    
    add rsp, 32
    pop rbp
    ret
build_pe_header ENDP

build_dos_header PROC
    ; Build DOS stub
    EMIT_WORD IMAGE_DOS_SIGNATURE
    EMIT_WORD 0090h              ; e_cblp
    EMIT_WORD 0003h              ; e_cp
    EMIT_WORD 0000h              ; e_crlc
    EMIT_WORD 0004h              ; e_cparhdr
    EMIT_WORD 0000h              ; e_minalloc
    EMIT_WORD 0FFFFh             ; e_maxalloc
    EMIT_WORD 0000h              ; e_ss
    EMIT_WORD 00B8h              ; e_sp
    EMIT_WORD 0000h              ; e_csum
    EMIT_WORD 0000h              ; e_ip
    EMIT_WORD 0000h              ; e_cs
    EMIT_WORD 0040h              ; e_lfarlc
    EMIT_WORD 0000h              ; e_ovno
    
    ; e_res (4 words)
    EMIT_WORD 0
    EMIT_WORD 0
    EMIT_WORD 0
    EMIT_WORD 0
    
    EMIT_WORD 0000h              ; e_oemid
    EMIT_WORD 0000h              ; e_oeminfo
    
    ; e_res2 (10 words)
    mov ecx, 10
.res2_loop:
    EMIT_WORD 0
    loop .res2_loop
    
    ; e_lfanew (offset to PE header)
    EMIT_DWORD 00000080h
    
    ; DOS stub program
    EMIT_BYTE 0Eh
    EMIT_BYTE 1Fh
    EMIT_BYTE 0BAh
    EMIT_BYTE 0Eh
    EMIT_BYTE 00h
    EMIT_BYTE 0B4h
    EMIT_BYTE 09h
    EMIT_BYTE 0CDh
    EMIT_BYTE 21h
    EMIT_BYTE 0B8h
    EMIT_BYTE 01h
    EMIT_BYTE 4Ch
    EMIT_BYTE 0CDh
    EMIT_BYTE 21h
    
    ; DOS message
    EMIT_STRING "This program cannot be run in DOS mode."
    
    ; Pad to 64 bytes
    ALIGN_FILE 64
    
    ret
build_dos_header ENDP

build_file_header PROC
    EMIT_WORD IMAGE_FILE_MACHINE_AMD64
    EMIT_WORD 3                  ; NumberOfSections (.text, .data, .rdata)
    
    ; TimeDateStamp (current time)
    call get_current_timestamp
    EMIT_DWORD eax
    
    EMIT_DWORD 0                 ; PointerToSymbolTable
    EMIT_DWORD 0                 ; NumberOfSymbols
    EMIT_WORD 00F0h              ; SizeOfOptionalHeader
    EMIT_WORD 0022h              ; Characteristics
    
    ret
build_file_header ENDP

build_optional_header PROC
    EMIT_WORD 020Bh              ; Magic (PE32+)
    EMIT_BYTE 0Ch                ; MajorLinkerVersion
    EMIT_BYTE 00h                ; MinorLinkerVersion
    
    EMIT_DWORD 00001000h         ; SizeOfCode
    EMIT_DWORD 00001000h         ; SizeOfInitializedData
    EMIT_DWORD 00000000h         ; SizeOfUninitializedData
    
    EMIT_DWORD 00001000h         ; AddressOfEntryPoint
    EMIT_DWORD 00001000h         ; BaseOfCode
    EMIT_QWORD IMAGE_BASE        ; ImageBase
    
    EMIT_DWORD SECTION_ALIGNMENT
    EMIT_DWORD FILE_ALIGNMENT
    
    EMIT_WORD 0006h              ; MajorOperatingSystemVersion
    EMIT_WORD 0000h              ; MinorOperatingSystemVersion
    EMIT_WORD 0000h              ; MajorImageVersion
    EMIT_WORD 0000h              ; MinorImageVersion
    EMIT_WORD 0006h              ; MajorSubsystemVersion
    EMIT_WORD 0000h              ; MinorSubsystemVersion
    
    EMIT_DWORD 00000000h         ; Win32VersionValue
    EMIT_DWORD 00003000h         ; SizeOfImage
    EMIT_DWORD 00000400h         ; SizeOfHeaders
    EMIT_DWORD 00000000h         ; CheckSum
    
    EMIT_WORD IMAGE_SUBSYSTEM_WINDOWS_CUI
    
    ; DllCharacteristics
    mov ax, IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
    or ax, IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    or ax, IMAGE_DLLCHARACTERISTICS_GUARD_CF
    EMIT_WORD ax
    
    EMIT_QWORD 0000000000100000h ; SizeOfStackReserve
    EMIT_QWORD 0000000000001000h ; SizeOfStackCommit
    EMIT_QWORD 0000000000100000h ; SizeOfHeapReserve
    EMIT_QWORD 0000000000001000h ; SizeOfHeapCommit
    
    EMIT_DWORD 00000000h         ; LoaderFlags
    EMIT_DWORD 00000010h         ; NumberOfRvaAndSizes
    
    ; Data directories (16 entries, all zero for now)
    mov ecx, 16
.data_dir_loop:
    EMIT_QWORD 0
    EMIT_QWORD 0
    loop .data_dir_loop
    
    ret
build_optional_header ENDP

build_section_headers PROC
    ; .text section
    EMIT_STRING ".text"
    EMIT_BYTE 0, 0, 0            ; Pad to 8 bytes
    
    EMIT_DWORD 00001000h         ; VirtualSize
    EMIT_DWORD 00001000h         ; VirtualAddress
    EMIT_DWORD 00001000h         ; SizeOfRawData
    EMIT_DWORD 00000400h         ; PointerToRawData
    EMIT_DWORD 0                 ; PointerToRelocations
    EMIT_DWORD 0                 ; PointerToLinenumbers
    EMIT_WORD 0                  ; NumberOfRelocations
    EMIT_WORD 0                  ; NumberOfLinenumbers
    EMIT_DWORD 60000020h         ; Characteristics
    
    ; .data section
    EMIT_STRING ".data"
    EMIT_BYTE 0, 0, 0            ; Pad to 8 bytes
    
    EMIT_DWORD 00001000h         ; VirtualSize
    EMIT_DWORD 00002000h         ; VirtualAddress
    EMIT_DWORD 00001000h         ; SizeOfRawData
    EMIT_DWORD 00001400h         ; PointerToRawData
    EMIT_DWORD 0                 ; PointerToRelocations
    EMIT_DWORD 0                 ; PointerToLinenumbers
    EMIT_WORD 0                  ; NumberOfRelocations
    EMIT_WORD 0                  ; NumberOfLinenumbers
    EMIT_DWORD 0C0000040h        ; Characteristics
    
    ; .rdata section
    EMIT_STRING ".rdata"
    EMIT_BYTE 0, 0               ; Pad to 8 bytes
    
    EMIT_DWORD 00001000h         ; VirtualSize
    EMIT_DWORD 00003000h         ; VirtualAddress
    EMIT_DWORD 00001000h         ; SizeOfRawData
    EMIT_DWORD 00001800h         ; PointerToRawData
    EMIT_DWORD 0                 ; PointerToRelocations
    EMIT_DWORD 0                 ; PointerToLinenumbers
    EMIT_WORD 0                  ; NumberOfRelocations
    EMIT_WORD 0                  ; NumberOfLinenumbers
    EMIT_DWORD 40000040h         ; Characteristics
    
    ret
build_section_headers ENDP

build_section_data PROC
    ; Align to file alignment
    ALIGN_FILE FILE_ALIGNMENT
    
    ; .text section data
    mov g_current_offset, 400h
    call build_text_section
    
    ; Align to file alignment
    ALIGN_FILE FILE_ALIGNMENT
    
    ; .data section data
    mov g_current_offset, 1400h
    call build_data_section
    
    ; Align to file alignment
    ALIGN_FILE FILE_ALIGNMENT
    
    ; .rdata section data
    mov g_current_offset, 1800h
    call build_rdata_section
    
    ret
build_section_data ENDP

build_text_section PROC
    ; Entry point code
    EMIT_BYTE 48h                ; push rbp
    EMIT_BYTE 89h                ; mov rbp, rsp
    EMIT_BYTE 0E5h
    
    ; Call main function
    EMIT_BYTE 0E8h               ; call relative
    EMIT_DWORD 0                 ; placeholder offset
    
    ; Exit process
    EMIT_BYTE 48h                ; mov rcx, 0
    EMIT_BYTE 0C7h
    EMIT_BYTE 0C1h
    EMIT_DWORD 0
    
    EMIT_BYTE 0FFh               ; call ExitProcess
    EMIT_BYTE 15h
    EMIT_DWORD 0                 ; placeholder IAT offset
    
    ; Main function
    EMIT_BYTE 55h                ; push rbp
    EMIT_BYTE 48h                ; mov rbp, rsp
    EMIT_BYTE 89h
    EMIT_BYTE 0E5h
    
    ; Compiler logic goes here
    ; ...
    
    EMIT_BYTE 5Dh                ; pop rbp
    EMIT_BYTE 0C3h               ; ret
    
    ret
build_text_section ENDP

build_data_section PROC
    ; Global variables
    EMIT_DWORD 0                 ; g_compiler_state
    EMIT_DWORD 0                 ; g_error_count
    EMIT_DWORD 0                 ; g_warning_count
    
    ; String constants
    EMIT_STRING "Universal Compiler Runtime"
    EMIT_STRING "1.0.0"
    
    ret
build_data_section ENDP

build_rdata_section PROC
    ; Import table
    EMIT_DWORD 0                 ; OriginalFirstThunk
    EMIT_DWORD 0                 ; TimeDateStamp
    EMIT_DWORD 0                 ; ForwarderChain
    EMIT_DWORD 0                 ; Name RVA
    EMIT_DWORD 0                 ; FirstThunk
    
    ; Null terminator
    times 5 EMIT_DWORD 0
    
    ret
build_rdata_section ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; UTILITY FUNCTIONS
; ═══════════════════════════════════════════════════════════════════════════════

get_current_timestamp PROC
    ; Simple timestamp generation
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret
get_current_timestamp ENDP

write_pe_file PROC
    ; rcx = filename
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Create file
    mov rdx, 40000000h           ; GENERIC_WRITE
    xor r8, r8                   ; No sharing
    xor r9, r9                   ; No security
    mov qword ptr [rsp+32], 2    ; CREATE_ALWAYS
    mov qword ptr [rsp+40], 0    ; No attributes
    call CreateFileA
    cmp rax, -1
    je .file_error
    mov rbx, rax                 ; Save file handle
    
    ; Write PE data
    mov rcx, rbx                 ; hFile
    mov rdx, g_pe_buffer         ; Buffer
    mov r8, g_pe_buffer_size     ; Bytes to write
    lea r9, [rsp+32]             ; Bytes written
    mov qword ptr [rsp+40], 0    ; Overlapped
    call WriteFile
    test rax, rax
    jz .write_error
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    add rsp, 48
    pop rbp
    ret
    
.file_error:
    lea rcx, [file_error_msg]
    call agent_log_error
    jmp .exit
    
.write_error:
    lea rcx, [write_error_msg]
    call agent_log_error
    
    ; Close file on error
    mov rcx, rbx
    call CloseHandle
    
.exit:
    add rsp, 48
    pop rbp
    ret
write_pe_file ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; MAIN ENTRY POINT
; ═══════════════════════════════════════════════════════════════════════════════

main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Initialize agentic system
    call agent_init_core
    
    ; Build PE header
    call build_pe_header
    
    ; Write to file
    lea rcx, [output_filename]
    call write_pe_file
    
    ; Start agentic monitoring
    call agentic_main
    
    add rsp, 32
    pop rbp
    ret
main ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════

.data
output_filename db "universal_compiler.exe", 0
file_error_msg db "Failed to create output file", 0
write_error_msg db "Failed to write PE data", 0

; Import table stubs
CreateFileA dq ?
WriteFile dq ?
CloseHandle dq ?
VirtualAlloc dq ?
VirtualFree dq ?

.code
; Entry point
public main

END