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

.data
    file_name       db "out.exe", 0
    msg_success     db "[+] TerraForm Manifested: out.exe", 10, 0
    msg_error       db "[!] Error", 10, 0
    
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
                    times 128 db 0   ; Data directories
    
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

; --- Parse TerraForm Syntax (Simplified) ---
; This is a basic parser for demonstration
parse_terraform proc
    ; Placeholder: Emit a simple "ret" instruction
    mov rcx, 0C3h  ; RET opcode
    xor rdx, rdx
    call emit_x64
    ret
parse_terraform endp

; --- Write PE to File ---
write_pe proc
    sub rsp, 40
    
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
main proc
    sub rsp, 40
    
    ; Parse TerraForm (placeholder)
    call parse_terraform
    
    ; Write PE
    call write_pe
    
    ; Exit
    xor ecx, ecx
    call ExitProcess
main endp

end