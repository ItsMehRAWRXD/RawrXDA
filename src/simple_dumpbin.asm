; ============================================================================
; MONOLITHIC x64 STANDALONE DUMPBIN
; 100% Pure MASM x64 - Zero External Dependencies (No .lib / .inc headers)
; Deep PE32+ Structure Analysis & Minimal Syscall-style API usage
; Optimized for Backend Development & Reverse Engineering
; ============================================================================

.code

; ============================================================================
; External Windows API declarations
; ============================================================================
EXTERN GetStdHandle    : PROC
EXTERN WriteConsoleA   : PROC
EXTERN ReadConsoleA    : PROC
EXTERN CreateFileA     : PROC
EXTERN ReadFile        : PROC
EXTERN CloseHandle     : PROC
EXTERN GetFileSizeEx   : PROC
EXTERN GetProcessHeap  : PROC
EXTERN HeapAlloc       : PROC
EXTERN HeapFree        : PROC
EXTERN ExitProcess     : PROC

; ============================================================================
; Constants
; ============================================================================
STD_INPUT_HANDLE        equ -10
STD_OUTPUT_HANDLE       equ -11
GENERIC_READ            equ 80000000h
FILE_SHARE_READ         equ 1
OPEN_EXISTING           equ 3
FILE_ATTRIBUTE_NORMAL   equ 80h
INVALID_HANDLE_VALUE    equ -1
HEAP_ZERO_MEMORY        equ 8

PE_SIGNATURE            equ 00004550h  ; "PE\0\0"
MZ_SIGNATURE            equ 5A4Dh      ; "MZ"

; ============================================================================
; Structures (Manual definitions for zero-dep)
; ============================================================================

IMAGE_DOS_HEADER STRUCT
    e_magic    WORD ?
    e_cblp     WORD ?
    e_cp       WORD ?
    e_crlc     WORD ?
    e_cparhdr  WORD ?
    e_minalloc WORD ?
    e_maxalloc WORD ?
    e_ss       WORD ?
    e_sp       WORD ?
    e_csum     WORD ?
    e_ip       WORD ?
    e_cs       WORD ?
    e_lfarlc   WORD ?
    e_ovno     WORD ?
    e_res      WORD 4 dup(?)
    e_oemid    WORD ?
    e_oeminfo  WORD ?
    e_res2     WORD 10 dup(?)
    e_lfanew   DWORD ?
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

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress DWORD ?
    Size           DWORD ?
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
    DataDirectory               IMAGE_DATA_DIRECTORY 16 dup(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature      DWORD ?
    FileHeader     IMAGE_FILE_HEADER <>
    OptionalHeader IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1                   BYTE 8 dup(?)
    Union_PhysicalAddress   DWORD ? ; or VirtualSize
    VirtualAddress          DWORD ?
    SizeOfRawData           DWORD ?
    PointerToRawData        DWORD ?
    PointerToRelocations    DWORD ?
    PointerToLinenumbers    DWORD ?
    NumberOfRelocations     WORD ?
    NumberOfLinenumbers     WORD ?
    Characteristics         DWORD ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk    DWORD ?
    TimeDateStamp         DWORD ?
    ForwarderChain        DWORD ?
    Name1                 DWORD ?
    FirstThunk            DWORD ?
IMAGE_IMPORT_DESCRIPTOR ENDS

; ============================================================================
; Data Section
; ============================================================================
.data
    ; UI Strings
    msg_header      db "=== MONOLITHIC x64 PE DUMPBIN ===", 13, 10, 0
    msg_prompt      db "Enter PE file path: ", 0
    msg_error_open  db "[-] Error: Could not open file.", 13, 10, 0
    msg_error_size  db "[-] Error: Could not get file size.", 13, 10, 0
    msg_error_read  db "[-] Error: Could not read file.", 13, 10, 0
    msg_error_pe    db "[-] Error: Not a valid PE64 file.", 13, 10, 0
    
    ; Info labels
    lbl_magic       db "[+] DOS Magic:          0x", 0
    lbl_lfanew      db "[+] PE Offset:          0x", 0
    lbl_signature   db "[+] NT Signature:       0x", 0
    lbl_sections    db "[+] Number of Sections: ", 0
    lbl_entry       db "[+] Entry Point RVA:    0x", 0
    lbl_imagebase   db "[+] Image Base:         0x", 0
    lbl_section_hdr db 13, 10, "--- SECTION TABLE ---", 13, 10, 0
    lbl_import_hdr  db 13, 10, "--- IMPORT TABLE ---", 13, 10, 0
    
    msg_newline     db 13, 10, 0
    msg_space       db "  ", 0
    msg_slash       db " / ", 0
    
    hex_chars       db "0123456789ABCDEF"

.data?
    stdin_h         dq ?
    stdout_h        dq ?
    file_h          dq ?
    file_size       dq ?
    file_buffer     dq ?
    p_nt_header     dq ?
    p_sections      dq ?
    num_sections    dw ?
    bytes_rw        dd ?
    input_buffer    db 260 dup(?)
    hex_buffer      db 20 dup(?)

; ============================================================================
; Main Entry
; ============================================================================
main PROC
    sub     rsp, 40

    ; Get Handles
    mov     rcx, STD_INPUT_HANDLE
    call    GetStdHandle
    mov     [stdin_h], rax
    
    mov     rcx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [stdout_h], rax
    
    ; Print Header
    lea     rcx, msg_header
    call    print_string
    
    ; Prompt for File
    lea     rcx, msg_prompt
    call    print_string
    
    ; Read Input
    mov     rcx, [stdin_h]
    lea     rdx, input_buffer
    mov     r8, 255
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    ReadConsoleA
    
    ; Trim input
    xor     rax, rax
    mov     eax, [bytes_rw]
    cmp     eax, 2
    jb      err_open
    lea     rdx, input_buffer
    add     rdx, rax
@@trim:
    dec     rdx
    cmp     byte ptr [rdx], 13
    je      @@zero
    cmp     byte ptr [rdx], 10
    je      @@zero
    jmp     @@done_trim
@@zero:
    mov     byte ptr [rdx], 0
    jmp     @@trim
@@done_trim:

    ; Open File
    lea     rcx, input_buffer
    mov     rdx, GENERIC_READ
    mov     r8, FILE_SHARE_READ
    xor     r9, r9
    mov     qword ptr [rsp+32], OPEN_EXISTING
    mov     qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+48], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      err_open
    mov     [file_h], rax
    
    ; Get File Size
    mov     rcx, [file_h]
    lea     rdx, file_size
    call    GetFileSizeEx
    test    rax, rax
    jz      err_size
    
    ; Allocate Memory
    call    GetProcessHeap
    mov     rcx, rax
    mov     rdx, HEAP_ZERO_MEMORY
    mov     r8, [file_size]
    call    HeapAlloc
    test    rax, rax
    jz      err_read
    mov     [file_buffer], rax
    
    ; Read File
    mov     rcx, [file_h]
    mov     rdx, [file_buffer]
    mov     r8, [file_size]
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    ReadFile
    test    rax, rax
    jz      err_read
    
    call    CloseHandle
    
    ; --- Analysis ---
    mov     rbx, [file_buffer]
    
    ; Validate DOS Header
    cmp     word ptr [rbx], MZ_SIGNATURE
    jne     err_pe
    
    ; Print DOS Info
    lea     rcx, lbl_magic
    call    print_string
    movzx   rcx, word ptr [rbx]
    call    print_hex16
    call    print_newline
    
    mov     eax, (IMAGE_DOS_HEADER ptr [rbx]).e_lfanew
    lea     rcx, lbl_lfanew
    call    print_string
    mov     ecx, eax
    call    print_hex32
    call    print_newline
    
    ; Validate PE Header
    mov     rbx, [file_buffer]
    add     rbx, rax                ; rbx points to IMAGE_NT_HEADERS64
    mov     [p_nt_header], rbx
    
    cmp     (IMAGE_NT_HEADERS64 ptr [rbx]).Signature, PE_SIGNATURE
    jne     err_pe
    
    lea     rcx, lbl_signature
    call    print_string
    mov     ecx, (IMAGE_NT_HEADERS64 ptr [rbx]).Signature
    call    print_hex32
    call    print_newline
    
    ; Machine / Sections
    lea     rcx, lbl_sections
    call    print_string
    movzx   rcx, (IMAGE_NT_HEADERS64 ptr [rbx]).FileHeader.NumberOfSections
    mov     [num_sections], cx
    call    print_dec
    call    print_newline
    
    ; Optional Header
    lea     rcx, lbl_entry
    call    print_string
    mov     ecx, (IMAGE_NT_HEADERS64 ptr [rbx]).OptionalHeader.AddressOfEntryPoint
    call    print_hex32
    call    print_newline
    
    lea     rcx, lbl_imagebase
    call    print_string
    mov     rcx, (IMAGE_NT_HEADERS64 ptr [rbx]).OptionalHeader.ImageBase
    call    print_hex64
    call    print_newline
    
    ; --- Sections ---
    lea     rcx, lbl_section_hdr
    call    print_string
    
    movzx   r12d, [num_sections]
    movzx   eax, (IMAGE_NT_HEADERS64 ptr [rbx]).FileHeader.SizeOfOptionalHeader
    lea     rsi, (IMAGE_NT_HEADERS64 ptr [rbx]).OptionalHeader
    add     rsi, rax                ; rsi points to first section header
    mov     [p_sections], rsi
    
sect_loop:
    test    r12d, r12d
    jz      do_imports
    
    lea     rcx, [rsi]
    call    print_string_n
    lea     rcx, msg_space
    call    print_string
    mov     ecx, (IMAGE_SECTION_HEADER ptr [rsi]).VirtualAddress
    call    print_hex32
    lea     rcx, msg_slash
    call    print_string
    mov     ecx, (IMAGE_SECTION_HEADER ptr [rsi]).SizeOfRawData
    call    print_hex32
    call    print_newline
    
    add     rsi, TYPE IMAGE_SECTION_HEADER
    dec     r12d
    jmp     sect_loop

do_imports:
    mov     rbx, [p_nt_header]
    mov     eax, (IMAGE_NT_HEADERS64 ptr [rbx]).OptionalHeader.DataDirectory[1*8].VirtualAddress
    test    eax, eax
    jz      done
    
    lea     rcx, lbl_import_hdr
    call    print_string
    
    mov     ecx, eax
    call    RvaToOffset
    test    rax, rax
    jz      done
    
    add     rax, [file_buffer]
    mov     rsi, rax

imp_loop:
    mov     eax, (IMAGE_IMPORT_DESCRIPTOR ptr [rsi]).Name1
    test    eax, eax
    jz      done
    
    mov     ecx, eax
    call    RvaToOffset
    add     rax, [file_buffer]
    mov     rcx, rax
    call    print_string
    call    print_newline
    
    add     rsi, TYPE IMAGE_IMPORT_DESCRIPTOR
    jmp     imp_loop

done:
    xor     rcx, rcx
    call    ExitProcess

err_open:
    lea     rcx, msg_error_open
    call    print_string
    jmp     done

err_size:
    lea     rcx, msg_error_size
    call    print_string
    jmp     done

err_read:
    lea     rcx, msg_error_read
    call    print_string
    jmp     done

err_pe:
    lea     rcx, msg_error_pe
    call    print_string
    jmp     done

main ENDP

; ============================================================================
; Utility Functions
; ============================================================================

RvaToOffset PROC
    ; ecx = RVA
    ; returns offset in rax
    push    rbx
    push    rsi
    push    rdi
    
    movzx   r8d, [num_sections]
    mov     rsi, [p_sections]
    
@@loop:
    test    r8d, r8d
    jz      @@none
    
    mov     eax, (IMAGE_SECTION_HEADER ptr [rsi]).VirtualAddress
    mov     edx, eax
    add     edx, (IMAGE_SECTION_HEADER ptr [rsi]).Union_PhysicalAddress ; VirtualSize
    
    cmp     ecx, eax
    jb      @@next
    cmp     ecx, edx
    jae     @@next
    
    ; Found it
    sub     ecx, eax
    add     ecx, (IMAGE_SECTION_HEADER ptr [rsi]).PointerToRawData
    mov     eax, ecx
    jmp     @@ret
    
@@next:
    add     rsi, TYPE IMAGE_SECTION_HEADER
    dec     r8d
    jmp     @@loop
    
@@none:
    xor     rax, rax
@@ret:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RvaToOffset ENDP

print_string PROC
    test    rcx, rcx
    jz      @@ret
    push    rsi
    push    rdi
    sub     rsp, 32
    mov     rsi, rcx
    xor     rdi, rdi
@@len:
    cmp     byte ptr [rsi+rdi], 0
    je      @@found
    inc     rdi
    jmp     @@len
@@found:
    mov     rcx, [stdout_h]
    mov     rdx, rsi
    mov     r8, rdi
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 32
    pop     rdi
    pop     rsi
@@ret:
    ret
print_string ENDP

print_string_n PROC
    push    rsi
    push    rdi
    sub     rsp, 32
    mov     rsi, rcx
    xor     rdi, rdi
@@len:
    cmp     rdi, 8
    je      @@found
    cmp     byte ptr [rsi+rdi], 0
    je      @@found
    inc     rdi
    jmp     @@len
@@found:
    mov     rcx, [stdout_h]
    mov     rdx, rsi
    mov     r8, rdi
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
print_string_n ENDP

print_newline PROC
    lea     rcx, msg_newline
    jmp     print_string
print_newline ENDP

print_hex64 PROC
    sub     rsp, 40
    mov     r10, rcx
    mov     r11, 16
@@loop:
    rol     r10, 4
    mov     rax, r10
    and     rax, 0Fh
    lea     rdx, hex_chars
    movzx   eax, byte ptr [rdx+rax]
    mov     byte ptr [hex_buffer], al
    mov     rcx, [stdout_h]
    lea     rdx, hex_buffer
    mov     r8, 1
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    dec     r11
    jnz     @@loop
    add     rsp, 40
    ret
print_hex64 ENDP

print_hex32 PROC
    sub     rsp, 40
    mov     r10d, ecx
    mov     r11, 8
@@loop:
    rol     r10d, 4
    mov     eax, r10d
    and     eax, 0Fh
    lea     rdx, hex_chars
    movzx   eax, byte ptr [rdx+rax]
    mov     byte ptr [hex_buffer], al
    mov     rcx, [stdout_h]
    lea     rdx, hex_buffer
    mov     r8, 1
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    dec     r11
    jnz     @@loop
    add     rsp, 40
    ret
print_hex32 ENDP

print_hex16 PROC
    sub     rsp, 40
    mov     r10w, cx
    mov     r11, 4
@@loop:
    rol     r10w, 4
    movzx   eax, r10w
    and     eax, 0Fh
    lea     rdx, hex_chars
    movzx   eax, byte ptr [rdx+rax]
    mov     byte ptr [hex_buffer], al
    mov     rcx, [stdout_h]
    lea     rdx, hex_buffer
    mov     r8, 1
    lea     r9, bytes_rw
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    dec     r11
    jnz     @@loop
    add     rsp, 40
    ret
print_hex16 ENDP

print_dec PROC
    sub     rsp, 56
    mov     rax, rcx
    lea     rdi, [hex_buffer + 19]
    mov     byte ptr [rdi], 0
    mov     rbx, 10
@@loop:
    xor     rdx, rdx
    div     rbx
    add     dl, '0'
    dec     rdi
    mov     [rdi], dl
    test    rax, rax
    jnz     @@loop
    mov     rcx, rdi
    call    print_string
    add     rsp, 56
    ret
print_dec ENDP

END
