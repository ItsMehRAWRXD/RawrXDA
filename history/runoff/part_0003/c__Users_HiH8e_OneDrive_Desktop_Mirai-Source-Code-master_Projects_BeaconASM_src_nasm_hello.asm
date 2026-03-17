; ============================================================================
; NASM Hello World - Windows x64
; ============================================================================
; Build: nasm -f win64 hello.asm -o hello.obj
;        link hello.obj /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:hello.exe
; ============================================================================

section .data
    msg db 'Hello from NASM Assembly!', 0xD, 0xA, 0
    msg_len equ $ - msg

section .text
    global main
    extern ExitProcess
    extern GetStdHandle
    extern WriteConsoleA

main:
    ; Allocate shadow space (Windows x64 calling convention)
    sub rsp, 40h
    
    ; Get stdout handle
    mov rcx, -11                ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r12, rax                ; Save handle
    
    ; Write to console
    mov rcx, r12                ; Console handle
    lea rdx, [rel msg]          ; Buffer
    mov r8, msg_len             ; Chars to write
    lea r9, [rsp+20h]           ; Chars written (stack temp)
    mov qword [rsp+20h], 0      ; Reserved parameter
    call WriteConsoleA
    
    ; Exit
    xor rcx, rcx                ; Exit code 0
    call ExitProcess
    
    ; Should never reach here
    add rsp, 40h
    ret
