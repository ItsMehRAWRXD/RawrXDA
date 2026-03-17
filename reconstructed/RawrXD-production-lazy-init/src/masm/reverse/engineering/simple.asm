; =====================================================================
; REVERSE ENGINEERING MODULE - GHIDRA-LIKE FEATURES
; Pure MASM64 implementation for binary analysis
; =====================================================================

PUBLIC DisassembleX64, AnalyzePEHeader, HexDumpMemory, FindStrings

EXTERN GlobalAlloc:PROC
EXTERN GlobalFree:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrlenA:PROC
EXTERN wsprintfA:PROC

; =====================================================================
; CONSTANTS
; =====================================================================

GENERIC_READ        EQU 80000000h
OPEN_EXISTING       EQU 3
FILE_SHARE_READ     EQU 1
INVALID_HANDLE_VALUE EQU -1
PAGE_READONLY       EQU 2
FILE_MAP_READ       EQU 4
PE_SIGNATURE        EQU 00004550h
GPTR                EQU 40h

; =====================================================================
; STRUCTURES
; =====================================================================

PE_HEADER STRUCT
    signature       DWORD ?
    machine         WORD ?
    numberOfSections WORD ?
PE_HEADER ENDS

ANALYSIS_RESULT STRUCT
    fileType        DWORD ?
    sectionCount    DWORD ?
ANALYSIS_RESULT ENDS

; =====================================================================
; DATA
; =====================================================================

.data
szRet              db "ret",0
szNop              db "nop",0
szRex              db "rex.w",0
szHexFormat        db "%08X: ",0
szByteFormat       db "%02X ",0
szSpace            db " ",0
szNewLine          db 13,10,0

; =====================================================================
; CODE
; =====================================================================

.code

; =====================================================================
; DisassembleX64 - Simple x64 disassembler
; RCX = buffer address, RDX = buffer size, R8 = output buffer
; =====================================================================
DisassembleX64 PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; input buffer
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output buffer
    
    xor     r9, r9          ; instruction count
    
disasm_loop:
    cmp     r9, rsi
    jae     disasm_done
    
    mov     al, byte ptr [rbx + r9]
    
    ; Simple disassembly
    cmp     al, 0xC3        ; RET
    je      disasm_ret
    cmp     al, 0x90        ; NOP
    je      disasm_nop
    cmp     al, 0x48        ; REX.W prefix
    je      disasm_rex
    
    ; Default: just copy byte as hex
    mov     byte ptr [rdi], al
    inc     rdi
    jmp     disasm_next
    
disasm_ret:
    mov     rcx, rdi
    lea     rdx, szRet
    call    lstrcpyA
    add     rdi, 4
    jmp     disasm_next
    
disasm_nop:
    mov     rcx, rdi
    lea     rdx, szNop
    call    lstrcpyA
    add     rdi, 4
    jmp     disasm_next
    
disasm_rex:
    mov     rcx, rdi
    lea     rdx, szRex
    call    lstrcpyA
    add     rdi, 6
    
disasm_next:
    inc     r9
    jmp     disasm_loop
    
disasm_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
DisassembleX64 ENDP

; =====================================================================
; AnalyzePEHeader - PE file analysis
; RCX = file path
; Returns: RAX = analysis result structure
; =====================================================================
AnalyzePEHeader PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 60h
    
    mov     rbx, rcx        ; file path
    
    ; Open file
    mov     rcx, rbx
    mov     rdx, GENERIC_READ
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+20h], OPEN_EXISTING
    mov     qword ptr [rsp+28h], 0
    mov     qword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      analyze_error
    mov     rsi, rax        ; file handle
    
    ; Get file size
    mov     rcx, rsi
    xor     rdx, rdx
    call    GetFileSize
    mov     rdi, rax
    
    ; Create mapping
    mov     rcx, rsi
    xor     rdx, rdx
    mov     r8, PAGE_READONLY
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateFileMappingA
    test    rax, rax
    jz      analyze_close
    mov     r8, rax         ; mapping handle
    
    ; Map view
    mov     rcx, r8
    mov     rdx, FILE_MAP_READ
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    MapViewOfFile
    test    rax, rax
    jz      analyze_unmap
    mov     r9, rax         ; mapped view
    
    ; Check DOS header
    mov     eax, dword ptr [r9]
    cmp     ax, 5A4Dh       ; MZ signature
    jne     analyze_unview
    
    ; Get PE header offset
    mov     eax, dword ptr [r9 + 3Ch]  ; e_lfanew
    add     rax, r9
    
    ; Check PE signature
    mov     edx, dword ptr [rax]
    cmp     edx, PE_SIGNATURE
    jne     analyze_unview
    
    ; Allocate result structure
    mov     rcx, GPTR
    mov     rdx, SIZEOF ANALYSIS_RESULT
    call    GlobalAlloc
    mov     rbx, rax
    
    ; Set file type
    mov     dword ptr [rbx + ANALYSIS_RESULT.fileType], 1  ; PE
    
    ; Analyze sections
    mov     cx, word ptr [rax + PE_HEADER.numberOfSections]
    mov     word ptr [rbx + ANALYSIS_RESULT.sectionCount], cx
    
analyze_unview:
    mov     rcx, r9
    call    UnmapViewOfFile
    
analyze_unmap:
    mov     rcx, r8
    call    CloseHandle
    
analyze_close:
    mov     rcx, rsi
    call    CloseHandle
    mov     rax, rbx
    jmp     analyze_done
    
analyze_error:
    xor     rax, rax
    
analyze_done:
    add     rsp, 60h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
AnalyzePEHeader ENDP

; =====================================================================
; HexDumpMemory - Hexadecimal memory dump
; RCX = memory address, RDX = size, R8 = output buffer
; =====================================================================
HexDumpMemory PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; memory address
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output buffer
    
    xor     r9, r9          ; offset
    
hexdump_loop:
    cmp     r9, rsi
    jae     hexdump_done
    
    ; Format: "00000000: 00 00 00 00 00 00 00 00  ................"
    mov     rcx, rdi
    lea     rdx, szHexFormat
    mov     r8, r9
    call    wsprintfA
    add     rdi, 10
    
    ; Hex bytes
    mov     r10, 0
hex_bytes:
    cmp     r10, 8
    jae     hex_ascii
    mov     al, byte ptr [rbx + r9 + r10]
    mov     rcx, rdi
    lea     rdx, szByteFormat
    movzx   r8, al
    call    wsprintfA
    add     rdi, 3
    inc     r10
    jmp     hex_bytes
    
hex_ascii:
    mov     rcx, rdi
    lea     rdx, szSpace
    call    lstrcpyA
    add     rdi, 1
    
    ; ASCII representation
    mov     r10, 0
ascii_chars:
    cmp     r10, 8
    jae     hex_next
    mov     al, byte ptr [rbx + r9 + r10]
    cmp     al, 20h
    jb      ascii_dot
    cmp     al, 7Eh
    ja      ascii_dot
    mov     byte ptr [rdi], al
    jmp     ascii_next
ascii_dot:
    mov     byte ptr [rdi], '.'
ascii_next:
    inc     rdi
    inc     r10
    jmp     ascii_chars
    
hex_next:
    mov     rcx, rdi
    lea     rdx, szNewLine
    call    lstrcpyA
    add     rdi, 2
    add     r9, 8
    jmp     hexdump_loop
    
hexdump_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
HexDumpMemory ENDP

; =====================================================================
; FindStrings - Extract ASCII strings from binary
; RCX = buffer, RDX = size, R8 = output buffer
; =====================================================================
FindStrings PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; buffer
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output
    
    xor     r9, r9          ; position
    xor     r10, r10        ; string start
    xor     r11, r11        ; string length
    
strings_loop:
    cmp     r9, rsi
    jae     strings_done
    
    mov     al, byte ptr [rbx + r9]
    cmp     al, 20h         ; space
    jb      check_string
    cmp     al, 7Eh         ; tilde
    ja      check_string
    
    ; Valid ASCII character
    cmp     r11, 0
    jne     string_continue
    mov     r10, r9         ; start of string
    
string_continue:
    inc     r11
    jmp     string_next
    
check_string:
    cmp     r11, 4          ; minimum string length
    jb      string_reset
    
    ; Copy string to output
    mov     r12, r10
string_copy:
    mov     al, byte ptr [rbx + r12]
    mov     byte ptr [rdi], al
    inc     rdi
    inc     r12
    dec     r11
    jnz     string_copy
    
    mov     rcx, rdi
    lea     rdx, szNewLine
    call    lstrcpyA
    add     rdi, 2
    
string_reset:
    xor     r11, r11
    
string_next:
    inc     r9
    jmp     strings_loop
    
strings_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
FindStrings ENDP

END