; =============================================================================
; RXUC-TerraForm v1.0 — Universal Language Compiler
; Pure MASM64, Direct PE Emission, Zero Dependencies
; Build: ml64 /c terraform.asm && link /entry:main terraform.obj kernel32.lib
; =============================================================================
option casemap:none

; Windows API Definitions
externdef ExitProcess : proc
externdef CreateFileA : proc
externdef WriteFile : proc
externdef CloseHandle : proc
externdef GetStdHandle : proc
externdef SetFilePointer : proc

.data
    file_name       db "out.exe", 0
    msg_success     db "[+] TerraForm Manifested: out.exe", 10, 0
    msg_error       db "[!] Error", 10, 0
    
    ; TerraForm tokens
    T_EOF           equ 0
    T_ID            equ 1
    T_NUM           equ 2
    T_FN            equ 3
    T_LET           equ 4
    T_LP            equ 5
    T_RP            equ 6
    T_LB            equ 7
    T_RB            equ 8
    T_ASSIGN        equ 9
    T_SEMI          equ 10
    
    ; Minimal PE Header Template (DOS + NT + Sections)
    align 8
    dos_header      db 4Dh, 5Ah, 90h, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0FFh, 0FFh, 0, 0
                    db 0B8h, 0, 0, 0, 0, 0, 0, 0, 40h, 0, 0, 0, 0, 0, 0, 0
                    times 32 db 0
                    db 80h, 0, 0, 0 ; Offset to PE Signature
    
    nt_header       db "PE", 0, 0, 0 ; Signature
                    dw 8664h         ; Machine (x64)
                    dw 2             ; Number of sections
                    dd 0             ; Timestamp
                    dd 0             ; Symbol table offset
                    dd 0             ; Number of symbols
                    dw 0F0h          ; Optional header size
                    dw 22h           ; Characteristics
    
    opt_header      dw 20Bh          ; Magic (PE32+)
                    db 0, 1          ; Linker version
                    dd 0             ; Code size
                    dd 0             ; Data size
                    dd 0             ; BSS size
                    dd 1000h         ; Entry point RVA
                    dd 1000h         ; Code base
                    dq 400000h       ; Image base
                    dd 1000h         ; Section alignment
                    dd 200h          ; File alignment
                    dw 6, 0          ; OS version
                    dw 0, 0          ; Image version
                    dw 6, 0          ; Subsystem version
                    dd 0             ; Win32 version
                    dd 3000h         ; Image size
                    dd 400h          ; Headers size
                    dd 0             ; Checksum
                    dw 3             ; Subsystem (console)
                    dw 0             ; DLL characteristics
                    dq 100000h       ; Stack reserve
                    dq 1000h         ; Stack commit
                    dq 100000h       ; Heap reserve
                    dq 1000h         ; Heap commit
                    dd 0             ; Loader flags
                    dd 16            ; Data directories
                    128 DUP(0)       ; Data directories
    
    text_section    db ".text", 0, 0, 0
                    dd 0             ; Virtual size
                    dd 2000h         ; Virtual address
                    dd 0             ; Raw size
                    dd 400h          ; Raw address
                    dd 0             ; Relocations
                    dd 0             ; Line numbers
                    dw 0, 0          ; Counts
                    dd 60000020h     ; Characteristics
    
    data_section    db ".data", 0, 0, 0
                    dd 0             ; Virtual size
                    dd 3000h         ; Virtual address
                    dd 0             ; Raw size
                    dd 600h          ; Raw address
                    dd 0             ; Relocations
                    dd 0             ; Line numbers
                    dw 0, 0          ; Counts
                    dd 0C0000040h    ; Characteristics

.bss
    hFile           dq ?
    bytesWritten    dd ?
    textBuffer      db 4096 dup(?)
    dataBuffer      db 4096 dup(?)
    textSize        dd ?
    dataSize        dd ?
    
    ; Lexer state
    sourcePtr       dq ?
    sourceLen       dd ?
    currentPos      dd ?
    currentToken    dd ?
    currentVal      dd ?

.code
; --- Utility: Write Byte to Buffer ---
; RCX: Buffer ptr, RDX: Offset, R8: Byte
write_byte proc
    mov rax, rcx
    add rax, rdx
    mov [rax], r8b
    ret
write_byte endp

; --- Emit x64 Opcode ---
; RCX: Opcode, RDX: ModRM (if any)
emit_x64 proc
    ; Simple emitter for basic ops
    mov rax, textSize
    lea rcx, textBuffer
    call write_byte
    inc textSize
    test rdx, rdx
    jz @@done
    mov r8, rdx
    mov rdx, rax
    inc rdx
    call write_byte
    inc textSize
@@done:
    ret
emit_x64 endp

; --- Emit DWORD ---
; RCX: DWORD value
emit_dword proc
    mov r8d, ecx
    shr r8d, 0
    and r8d, 0FFh
    mov rdx, textSize
    lea rcx, textBuffer
    call write_byte
    inc textSize
    
    mov r8d, ecx
    shr r8d, 8
    and r8d, 0FFh
    mov rdx, textSize
    call write_byte
    inc textSize
    
    mov r8d, ecx
    shr r8d, 16
    and r8d, 0FFh
    mov rdx, textSize
    call write_byte
    inc textSize
    
    mov r8d, ecx
    shr r8d, 24
    and r8d, 0FFh
    mov rdx, textSize
    call write_byte
    inc textSize
    ret
emit_dword endp

; --- Parse TerraForm Syntax (Basic) ---
; This is a basic parser for fn, let, numbers, identifiers
parse_terraform proc
    ; Initialize lexer
    mov sourcePtr, rcx  ; Assume RCX has source string
    mov sourceLen, edx  ; EDX has length
    mov currentPos, 0
    
    ; Parse basic constructs
    call next_token
    cmp currentToken, T_FN
    jne @@check_let
    
    ; Parse fn
    call next_token
    cmp currentToken, T_ID
    jne @@error
    ; Emit function prolog
    mov rcx, 55h  ; push rbp
    call emit_x64
    mov rcx, 48h  ; REX.W
    call emit_x64
    mov rcx, 89h  ; mov rbp, rsp
    call emit_x64
    mov rcx, 0E5h
    call emit_x64
    jmp @@done
    
@@check_let:
    cmp currentToken, T_LET
    jne @@done
    ; Parse let
    call next_token
    cmp currentToken, T_ID
    jne @@error
    call next_token
    cmp currentToken, T_ASSIGN
    jne @@error
    call next_token
    cmp currentToken, T_NUM
    jne @@error
    ; Emit mov rax, imm
    mov rcx, 48h  ; REX.W
    call emit_x64
    mov rcx, 0C7h ; mov rax, imm32
    call emit_x64
    mov rcx, 0C0h
    call emit_x64
    mov ecx, currentVal
    call emit_dword
    
@@done:
    ; Emit epilog
    mov rcx, 5Dh  ; pop rbp
    call emit_x64
    mov rcx, 0C3h ; ret
    call emit_x64
    ret
    
@@error:
    ret
parse_terraform endp

; --- Lexer: Get Next Token ---
next_token proc
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@eof
    
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    
    ; Skip whitespace
@@skip_ws:
    cmp al, ' '
    je @@next_char
    cmp al, 9  ; tab
    je @@next_char
    cmp al, 10 ; nl
    je @@next_char
    cmp al, 13 ; cr
    je @@next_char
    jmp @@check_token
    
@@next_char:
    inc currentPos
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@eof
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    jmp @@skip_ws
    
@@check_token:
    cmp al, 'f'
    je @@check_fn
    cmp al, 'l'
    je @@check_let
    cmp al, '('
    je @@lp
    cmp al, ')'
    je @@rp
    cmp al, '{'
    je @@lb
    cmp al, '}'
    je @@rb
    cmp al, '='
    je @@assign
    cmp al, ';'
    je @@semi
    cmp al, '0'
    jb @@id
    cmp al, '9'
    jbe @@num
    jmp @@id
    
@@check_fn:
    ; Check "fn"
    inc currentPos
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@id
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    cmp al, 'n'
    jne @@id
    inc currentPos
    mov currentToken, T_FN
    ret
    
@@check_let:
    ; Check "let"
    inc currentPos
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@id
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    cmp al, 'e'
    jne @@id
    inc currentPos
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@id
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    cmp al, 't'
    jne @@id
    inc currentPos
    mov currentToken, T_LET
    ret
    
@@lp:
    mov currentToken, T_LP
    inc currentPos
    ret
@@rp:
    mov currentToken, T_RP
    inc currentPos
    ret
@@lb:
    mov currentToken, T_LB
    inc currentPos
    ret
@@rb:
    mov currentToken, T_RB
    inc currentPos
    ret
@@assign:
    mov currentToken, T_ASSIGN
    inc currentPos
    ret
@@semi:
    mov currentToken, T_SEMI
    inc currentPos
    ret
@@num:
    xor edx, edx
@@num_loop:
    sub al, '0'
    imul edx, 10
    add edx, eax
    inc currentPos
    mov eax, currentPos
    cmp eax, sourceLen
    jae @@num_done
    mov rcx, sourcePtr
    add rcx, rax
    mov al, [rcx]
    cmp al, '0'
    jb @@num_done
    cmp al, '9'
    jbe @@num_loop
@@num_done:
    mov currentToken, T_NUM
    mov currentVal, edx
    ret
@@id:
    mov currentToken, T_ID
    inc currentPos
    ret
@@eof:
    mov currentToken, T_EOF
    ret
next_token endp

; --- Write PE to File ---
write_pe proc
    sub rsp, 40
    
    ; Update PE header sizes
    mov eax, textSize
    mov nt_header[28], eax  ; SizeOfCode
    mov nt_header[32], eax  ; SizeOfInitializedData (approx)
    mov nt_header[272], eax ; VirtualSize .text
    mov nt_header[280], eax ; SizeOfRawData .text
    mov eax, dataSize
    mov nt_header[292], eax ; VirtualSize .data
    mov nt_header[300], eax ; SizeOfRawData .data
    
    ; Create file
    lea rcx, file_name
    mov edx, 40000000h  ; GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 2  ; CREATE_ALWAYS
    call CreateFileA
    mov hFile, rax
    cmp rax, -1
    je @@error
    
    ; Write DOS header
    mov rcx, hFile
    lea rdx, dos_header
    mov r8d, 64
    lea r9, bytesWritten
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write NT header
    mov rcx, hFile
    lea rdx, nt_header
    mov r8d, 24 + 240 + 80  ; NT + Opt + Sections
    lea r9, bytesWritten
    call WriteFile
    
    ; Pad to 0x400
    mov rcx, hFile
    mov edx, 400h
    xor r8, r8
    xor r9, r9
    call SetFilePointer
    
    ; Write .text
    mov rcx, hFile
    lea rdx, textBuffer
    mov r8d, textSize
    lea r9, bytesWritten
    call WriteFile
    
    ; Pad to 0x600
    mov rcx, hFile
    mov edx, 600h
    xor r8, r8
    xor r9, r9
    call SetFilePointer
    
    ; Write .data
    mov rcx, hFile
    lea rdx, dataBuffer
    mov r8d, dataSize
    lea r9, bytesWritten
    call WriteFile
    
    ; Close
    mov rcx, hFile
    call CloseHandle
    
    ; Success message
    lea rcx, msg_success
    call print
    jmp @@exit
    
@@error:
    lea rcx, msg_error
    call print
    
@@exit:
    add rsp, 40
    ret
write_pe endp

; --- Print String ---
print proc
    mov r12, rcx
    call strlen
    mov rdx, r12
    mov r8, rax
    mov ecx, -11
    call GetStdHandle
    mov rcx, rax
    mov r9, 0
    push 0
    call WriteConsoleA
    add rsp, 8
    ret
print endp

strlen proc
    xor rax, rax
@@l: cmp byte ptr [rcx+rax], 0
    je @@d
    inc rax
    jmp @@l
@@d: ret
strlen endp

; --- Main Entry ---
_start proc
    sub rsp, 40
    
    ; Hardcoded TerraForm source for demo: fn main() { let x = 42; }
    lea rcx, tf_source
    mov rdx, tf_source_len
    call parse_terraform
    
    ; Write PE
    call write_pe
    
    ; Exit
    xor ecx, ecx
    call ExitProcess
    
tf_source db "fn main() { let x = 42; }"
tf_source_len equ $ - tf_source
_start endp

; --- Data Section ---
.data
; PE Headers
dos_header db 4Dh, 5Ah, 090h, 000h, 003h, 000h, 000h, 000h
          db 004h, 000h, 000h, 000h, 0FFh, 0FFh, 000h, 000h
          db 0B8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
          db 040h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
          db 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
          db 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
          db 000h, 000h, 000h, 000h, 080h, 000h, 000h, 000h
          db 00Eh, 01Fh, 0BAh, 00Eh, 000h, 0B4h, 009h, 0CDh
          db 021h, 0B8h, 001h, 04Ch, 0CDh, 021h, 054h, 068h
          db 069h, 073h, 020h, 070h, 072h, 06Fh, 067h, 072h
          db 061h, 06Dh, 020h, 063h, 061h, 06Eh, 06Eh, 06Fh
          db 074h, 020h, 062h, 065h, 020h, 072h, 075h, 06Eh
          db 020h, 069h, 06Eh, 020h, 044h, 04Fh, 053h, 020h
          db 06Dh, 06Fh, 064h, 065h, 02Eh, 00Dh, 00Dh, 00Ah
          db 024h, 000h, 000h, 000h, 000h, 000h, 000h, 000h

nt_header db 050h, 045h, 000h, 000h  ; PE signature
         dw 8664h  ; Machine (x64)
         dw 2      ; NumberOfSections
         dd 0      ; TimeDateStamp
         dd 0      ; PointerToSymbolTable
         dd 0      ; NumberOfSymbols
         dw 240    ; SizeOfOptionalHeader
         dw 22Fh   ; Characteristics (EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE)

; Optional Header (64-bit)
         dw 020Bh  ; Magic (PE32+)
         db 0      ; MajorLinkerVersion
         db 0      ; MinorLinkerVersion
         dd 0      ; SizeOfCode
         dd 0      ; SizeOfInitializedData
         dd 0      ; SizeOfUninitializedData
         dd 400h   ; AddressOfEntryPoint (RVA of _start)
         dd 400h   ; BaseOfCode
         dq 400000h ; ImageBase
         dd 4      ; SectionAlignment
         dd 4      ; FileAlignment
         dw 0      ; MajorOperatingSystemVersion
         dw 0      ; MinorOperatingSystemVersion
         dw 0      ; MajorImageVersion
         dw 0      ; MinorImageVersion
         dw 4      ; MajorSubsystemVersion
         dw 0      ; MinorSubsystemVersion
         dd 0      ; Win32VersionValue
         dd 800h   ; SizeOfImage
         dd 4      ; SizeOfHeaders
         dd 0      ; CheckSum
         dw 3      ; Subsystem (CONSOLE)
         dw 0      ; DllCharacteristics
         dq 100000h ; SizeOfStackReserve
         dq 1000h   ; SizeOfStackCommit
         dq 100000h ; SizeOfHeapReserve
         dq 1000h   ; SizeOfHeapCommit
         dd 0      ; LoaderFlags
         dd 16     ; NumberOfRvaAndSizes

; Data Directories (16 entries, all zero for minimal PE)
         dq 16 dup (0)

; Section Headers
; .text section
         db ".text", 0, 0, 0  ; Name
         dd 0      ; VirtualSize
         dd 400h   ; VirtualAddress
         dd 0      ; SizeOfRawData
         dd 400h   ; PointerToRawData
         dd 0      ; PointerToRelocations
         dd 0      ; PointerToLinenumbers
         dw 0      ; NumberOfRelocations
         dw 0      ; NumberOfLinenumbers
         dd 60000020h ; Characteristics (CODE | EXECUTE | READ)

; .data section
         db ".data", 0, 0, 0  ; Name
         dd 0      ; VirtualSize
         dd 600h   ; VirtualAddress
         dd 0      ; SizeOfRawData
         dd 600h   ; PointerToRawData
         dd 0      ; PointerToRelocations
         dd 0      ; PointerToLinenumbers
         dw 0      ; NumberOfRelocations
         dw 0      ; NumberOfLinenumbers
         dd 0C0000040h ; Characteristics (INITIALIZED_DATA | READ | WRITE)

; Strings and variables
file_name db "output.exe", 0
msg_success db "PE file written successfully!", 10, 0
msg_error db "Error writing PE file!", 10, 0
hFile dq ?
bytesWritten dd ?

end