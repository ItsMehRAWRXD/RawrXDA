; Absolute minimal Windows GUI test
bits 64
default rel

extern GetModuleHandleA
extern RegisterClassExA
extern CreateWindowExA
extern ShowWindow
extern UpdateWindow
extern GetMessageA
extern TranslateMessage
extern DispatchMessageA
extern DefWindowProcA
extern PostQuitMessage
extern LoadCursorA

global main

%define NULL 0
%define WS_OVERLAPPEDWINDOW 0x00CF0000
%define WS_VISIBLE 0x10000000
%define CW_USEDEFAULT 0x80000000
%define SW_SHOW 5
%define CS_HREDRAW 2
%define CS_VREDRAW 1
%define IDC_ARROW 32512
%define WM_DESTROY 2

section .data
    className db "MinimalWnd", 0
    winTitle db "Minimal Test", 0

section .bss
    hInstance resq 1
    hWnd resq 1
    msg resb 48

section .text

WndProc:
    cmp edx, WM_DESTROY
    je .destroy
    jmp DefWindowProcA
.destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret

main:
    push rbp
    mov rbp, rsp
    sub rsp, 160
    
    ; Get instance
    xor ecx, ecx
    call GetModuleHandleA
    mov [hInstance], rax
    
    ; Register class
    mov dword [rsp], 80          ; cbSize
    mov dword [rsp+4], CS_HREDRAW | CS_VREDRAW
    lea rax, [WndProc]
    mov [rsp+8], rax
    mov dword [rsp+16], 0
    mov dword [rsp+20], 0
    mov rax, [hInstance]
    mov [rsp+24], rax
    mov qword [rsp+32], 0
    xor ecx, ecx
    mov edx, IDC_ARROW
    call LoadCursorA
    mov [rsp+40], rax
    mov qword [rsp+48], 6        ; COLOR_WINDOW+1
    mov qword [rsp+56], 0
    lea rax, [className]
    mov [rsp+64], rax
    mov qword [rsp+72], 0
    
    mov rcx, rsp
    call RegisterClassExA
    
    ; Create window
    xor ecx, ecx
    lea rdx, [className]
    lea r8, [winTitle]
    mov r9d, WS_OVERLAPPEDWINDOW | WS_VISIBLE
    mov dword [rsp+32], CW_USEDEFAULT
    mov dword [rsp+40], CW_USEDEFAULT
    mov dword [rsp+48], 800
    mov dword [rsp+56], 600
    mov qword [rsp+64], 0
    mov qword [rsp+72], 0
    mov rax, [hInstance]
    mov [rsp+80], rax
    mov qword [rsp+88], 0
    call CreateWindowExA
    mov [hWnd], rax
    
    ; Show
    mov rcx, [hWnd]
    mov edx, SW_SHOW
    call ShowWindow
    
    mov rcx, [hWnd]
    call UpdateWindow
    
    ; Message loop
.loop:
    lea rcx, [msg]
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA
    test eax, eax
    jz .exit
    
    lea rcx, [msg]
    call TranslateMessage
    lea rcx, [msg]
    call DispatchMessageA
    jmp .loop
    
.exit:
    xor eax, eax
    add rsp, 160
    pop rbp
    ret
