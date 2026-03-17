; Clean, minimal NASM IDE - Working foundationbits 64

; Addresses all systemic startup issuesdefault rel

bits 64

default relextern MessageBoxA

extern CreateWindowExA  

extern MessageBoxAextern RegisterClassExA

extern CreateWindowExA  extern DefWindowProcA

extern RegisterClassExAextern PostQuitMessage

extern DefWindowProcAextern GetMessageA

extern PostQuitMessageextern TranslateMessage

extern GetMessageAextern DispatchMessageA

extern TranslateMessageextern ExitProcess

extern DispatchMessageAextern GetModuleHandleA

extern ExitProcessextern UpdateWindow

extern GetModuleHandleAextern ShowWindow

extern UpdateWindowextern LoadCursorA

extern ShowWindowextern BeginPaint

extern LoadCursorAextern EndPaint

extern BeginPaintextern CreateSolidBrush

extern EndPaint

extern CreateSolidBrushsection .data

    class_name db "CleanNASMIDE", 0

section .data    window_title db "Professional NASM IDE - Clean Build", 0

    class_name db "CleanNASMIDE", 0    startup_msg db "Clean NASM IDE Starting...", 0

    window_title db "Professional NASM IDE - Clean Build", 0    ready_msg db "Professional NASM IDE Ready!", 10, 10, "Status: All core systems operational", 10, "Architecture: Minimal, stable design", 10, "Features: Basic window, message loop", 10, "Next: Add DirectX when stable", 0

    startup_msg db "Clean NASM IDE Starting...", 0    title db "NASM IDE", 0

    ready_msg db "Professional NASM IDE Ready!", 10, 10, "Status: All core systems operational", 10, "Architecture: Minimal, stable design", 10, "Features: Basic window, message loop", 10, "Next: Add DirectX when stable", 0

    title db "NASM IDE", 0section .bss

    window_handle resq 1

section .bss    msg resb 48

    window_handle resq 1    wc resb 80

    msg resb 48    ps resb 64

    wc resb 80

    ps resb 64section .text

global main

section .text

global mainmain:

    push rbp

main:    mov rbp, rsp

    push rbp    and rsp, -16

    mov rbp, rsp    sub rsp, 32

    and rsp, -16ECHO is off.

    sub rsp, 32    ; Show startup

        xor ecx, ecx

    ; Show startup    lea rdx, [startup_msg]

    xor ecx, ecx    lea r8, [title]

    lea rdx, [startup_msg]    mov r9d, 64

    lea r8, [title]    call MessageBoxA

    mov r9d, 64ECHO is off.

    call MessageBoxA    ; Register window class

        mov dword [wc], 80        ; cbSize

    ; Register window class    mov dword [wc+4], 3       ; style

    mov dword [wc], 80        ; cbSize    lea rax, [window_proc]

    mov dword [wc+4], 3       ; style    mov [wc+8], rax           ; lpfnWndProc

    lea rax, [window_proc]    mov dword [wc+16], 0      ; cbClsExtra

    mov [wc+8], rax           ; lpfnWndProc    mov dword [wc+20], 0      ; cbWndExtra

    mov dword [wc+16], 0      ; cbClsExtraECHO is off.

    mov dword [wc+20], 0      ; cbWndExtra    xor ecx, ecx

        call GetModuleHandleA

    xor ecx, ecx    mov [wc+24], rax          ; hInstance

    call GetModuleHandleAECHO is off.

    mov [wc+24], rax          ; hInstance    mov qword [wc+32], 0      ; hIcon

    ECHO is off.

    mov qword [wc+32], 0      ; hIcon    xor ecx, ecx

        mov edx, 32512            ; IDC_ARROW

    xor ecx, ecx    call LoadCursorA

    mov edx, 32512            ; IDC_ARROW    mov [wc+40], rax          ; hCursor

    call LoadCursorAECHO is off.

    mov [wc+40], rax          ; hCursor    mov rax, 6

        push rax

    mov rax, 6    call CreateSolidBrush

    push rax    add rsp, 8

    call CreateSolidBrush    mov [wc+48], rax          ; hbrBackground

    add rsp, 8ECHO is off.

    mov [wc+48], rax          ; hbrBackground    mov qword [wc+56], 0      ; lpszMenuName

        lea rax, [class_name]

    mov qword [wc+56], 0      ; lpszMenuName    mov [wc+64], rax          ; lpszClassName

    lea rax, [class_name]    mov qword [wc+72], 0      ; hIconSm

    mov [wc+64], rax          ; lpszClassNameECHO is off.

    mov qword [wc+72], 0      ; hIconSm    lea rcx, [wc]

        call RegisterClassExA

    lea rcx, [wc]    test eax, eax

    call RegisterClassExA    jz .exit

    test eax, eaxECHO is off.

    jz .exit    ; Create window

        sub rsp, 88

    ; Create window    mov qword [rsp+80], 0     ; lpParam

    sub rsp, 88    mov rax, [wc+24]

    mov qword [rsp+80], 0     ; lpParam    mov [rsp+72], rax         ; hInstance

    mov rax, [wc+24]    mov qword [rsp+64], 0     ; hMenu

    mov [rsp+72], rax         ; hInstance    mov qword [rsp+56], 0     ; hWndParent

    mov qword [rsp+64], 0     ; hMenu    mov qword [rsp+48], 600   ; nHeight

    mov qword [rsp+56], 0     ; hWndParent    mov qword [rsp+40], 800   ; nWidth

    mov qword [rsp+48], 600   ; nHeight    mov qword [rsp+32], 100   ; y

    mov qword [rsp+40], 800   ; nWidth    mov qword [rsp+24], 100   ; x

    mov qword [rsp+32], 100   ; y    mov qword [rsp+16], 0x10CF0000  ; dwStyle

    mov qword [rsp+24], 100   ; x    lea rax, [window_title]

    mov qword [rsp+16], 0x10CF0000  ; dwStyle    mov [rsp+8], rax          ; lpWindowName

    lea rax, [window_title]    lea rax, [class_name]

    mov [rsp+8], rax          ; lpWindowName    mov [rsp], rax            ; lpClassName

    lea rax, [class_name]    mov rcx, 0                ; dwExStyle

    mov [rsp], rax            ; lpClassName    mov rdx, 0

    mov rcx, 0                ; dwExStyle    mov r8, 0

    mov rdx, 0    mov r9, 0

    mov r8, 0    call CreateWindowExA

    mov r9, 0    add rsp, 88

    call CreateWindowExAECHO is off.

    add rsp, 88    test rax, rax

        jz .exit

    test rax, rax    mov [window_handle], rax

    jz .exitECHO is off.

    mov [window_handle], rax    ; Show window

        mov rcx, rax

    ; Show window    mov rdx, 1

    mov rcx, rax    call ShowWindow

    mov rdx, 1ECHO is off.

    call ShowWindow    mov rcx, [window_handle]

        call UpdateWindow

    mov rcx, [window_handle]ECHO is off.

    call UpdateWindow    ; Show ready message

        xor ecx, ecx

    ; Show ready message    lea rdx, [ready_msg]

    xor ecx, ecx    lea r8, [title]

    lea rdx, [ready_msg]    mov r9d, 64

    lea r8, [title]    call MessageBoxA

    mov r9d, 64ECHO is off.

    call MessageBoxA    ; Message loop

    .loop:

    ; Message loop    lea rcx, [msg]

.loop:    xor edx, edx

    lea rcx, [msg]    xor r8d, r8d

    xor edx, edx    xor r9d, r9d

    xor r8d, r8d    call GetMessageA

    xor r9d, r9d    test eax, eax

    call GetMessageA    jz .exit

    test eax, eax    js .exit

    jz .exitECHO is off.

    js .exit    lea rcx, [msg]

        call TranslateMessage

    lea rcx, [msg]ECHO is off.

    call TranslateMessage    lea rcx, [msg]

        call DispatchMessageA

    lea rcx, [msg]    jmp .loop

    call DispatchMessageAECHO is off.

    jmp .loop.exit:

        mov rsp, rbp

.exit:    pop rbp

    mov rsp, rbp    xor ecx, ecx

    pop rbp    call ExitProcess

    xor ecx, ecx

    call ExitProcesswindow_proc:

    cmp edx, 2

window_proc:    je .destroy

    cmp edx, 2    cmp edx, 15

    je .destroy    je .paint

    cmp edx, 15ECHO is off.

    je .paint    call DefWindowProcA

        ret

    call DefWindowProcAECHO is off.

    ret.destroy:

        xor ecx, ecx

.destroy:    call PostQuitMessage

    xor ecx, ecx    xor eax, eax

    call PostQuitMessage    ret

    xor eax, eaxECHO is off.

    ret.paint:

        push rbp

.paint:    mov rbp, rsp

    push rbp    sub rsp, 32

    mov rbp, rspECHO is off.

    sub rsp, 32    mov rdx, rcx

        lea rcx, [ps]

    mov rdx, rcx    call BeginPaint

    lea rcx, [ps]ECHO is off.

    call BeginPaint    lea rcx, [ps]

        call EndPaint

    lea rcx, [ps]ECHO is off.

    call EndPaint    mov rsp, rbp

        pop rbp

    mov rsp, rbp    xor eax, eax

    pop rbp    ret

    xor eax, eax
    ret