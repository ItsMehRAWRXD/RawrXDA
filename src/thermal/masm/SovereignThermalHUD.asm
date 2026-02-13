; ============================================================================
; SovereignThermalHUD.asm
; GDI32 Transparent Thermal Overlay (MASM x64)
; Reads Global\SOVEREIGN_NVME_TEMPS and paints a desktop HUD
; ============================================================================
option casemap:none

; --- Win32 Externals ---
extern GetModuleHandleA : PROC
extern ExitProcess : PROC
extern RegisterClassExA : PROC
extern CreateWindowExA : PROC
extern ShowWindow : PROC
extern UpdateWindow : PROC
extern DefWindowProcA : PROC
extern GetMessageA : PROC
extern TranslateMessage : PROC
extern DispatchMessageA : PROC
extern PostQuitMessage : PROC
extern SetLayeredWindowAttributes : PROC
extern SetTimer : PROC
extern InvalidateRect : PROC
extern BeginPaint : PROC
extern EndPaint : PROC
extern CreateSolidBrush : PROC
extern CreatePen : PROC
extern SelectObject : PROC
extern DeleteObject : PROC
extern FillRect : PROC
extern Rectangle : PROC
extern TextOutA : PROC
extern SetBkMode : PROC
extern SetTextColor : PROC
extern OpenFileMappingA : PROC
extern MapViewOfFile : PROC
extern UnmapViewOfFile : PROC
extern CloseHandle : PROC
extern wsprintfA : PROC

; --- Constants ---
CS_VREDRAW          equ 1
CS_HREDRAW          equ 2
WS_POPUP            equ 80000000h
WS_VISIBLE          equ 10000000h
WS_EX_TOPMOST       equ 00000008h
WS_EX_TRANSPARENT   equ 00000020h
WS_EX_LAYERED       equ 00080000h
WS_EX_TOOLWINDOW    equ 00000080h
LWA_COLORKEY        equ 1
WM_DESTROY          equ 2
WM_PAINT            equ 15
WM_TIMER            equ 275
FILE_MAP_READ       equ 4
TRANSPARENT_MODE    equ 1

; --- Data ---
.data
    szClassName     db "SovereignHUD", 0
    szAppName       db "Sovereign Thermal HUD", 0
    szMMFName       db "Global\\SOVEREIGN_NVME_TEMPS", 0
    
    ; Colors
    colBackground   dd 00101010h ; Dark Grey (Key)
    colBarSafe      dd 0000FF00h ; Green (BGR)
    colBarWarn      dd 0000FFFFh ; Yellow
    colBarCrit      dd 000000FFh ; Red
    colText         dd 00FFFFFFh ; White

    ; MMF
    hMMF            dq 0
    pView           dq 0
    
    ; Formatting
    szFmtTemp       db "Drive %d: %d C", 0
    szTitle         db "SOVEREIGN THERMAL", 0
    
    ; WNDCLASSEXA structure (80 bytes on x64)
    wc              db 80 dup(0)
    msg             db 48 dup(0)
    ps              db 72 dup(0) ; PAINTSTRUCT
    rect            db 16 dup(0) ; RECT
    
    hBrushBack      dq 0
    hBrushSafe      dq 0
    hBrushWarn      dq 0
    hBrushCrit      dq 0
    buffer          db 64 dup(0)

.code

; ---------------------------------------------------------------------------
; Initialize MMF
; ---------------------------------------------------------------------------
InitMMF PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rcx, FILE_MAP_READ
    xor rdx, rdx
    lea r8, szMMFName
    call OpenFileMappingA
    test rax, rax
    jz _fail
    mov hMMF, rax
    
    mov rcx, rax
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], 1024
    call MapViewOfFile
    mov pView, rax
    
_fail:
    add rsp, 28h
    ret
InitMMF ENDP

; ---------------------------------------------------------------------------
; Window Procedure
; ---------------------------------------------------------------------------
WndProc PROC FRAME
    ; rcx=hWnd, rdx=uMsg, r8=wParam, r9=lParam
    push rbp
    mov rbp, rsp
    sub rsp, 40h ; Shadow + alignment
    .allocstack 40h
    .endprolog

    cmp edx, WM_DESTROY
    je _destroy
    cmp edx, WM_TIMER
    je _timer
    cmp edx, WM_PAINT
    je _paint
    
    ; Default
    call DefWindowProcA
    jmp _ret

_timer:
    ; Force repaint
    mov rcx, [rbp+16] ; hWnd (rbp points to old rbp, +16 is rcx home?) No.
    ; Arguments are in HOME SPACE of caller, but registers are volatile.
    ; Wait, x64 calling convention: registers RCX, RDX, R8, R9.
    ; We need to save them if we want to use them later? No, we are in the function.
    ; RCX is hWnd.
    
    ; Actually, rcx is passed in registers.
    ; But we are jumping from a compare ladder. Values in regs are preserved? No CMP doesn't change regs.
    ; But we need hWnd for InvalidateRect.
    ; Since we haven't clobbered RCX yet...
    
    ; Let's re-load RCX if needed? No, RCX is volatile across calls.
    ; Wait, we are inside WndProc. RCX holds hWnd at entry.
    ; If we haven't called anything yet, RCX is still hWnd.
    xor rdx, rdx
    mov r8d, 0 ; FALSE
    call InvalidateRect
    xor rax, rax
    jmp _ret

_paint:
    ; BeginPaint(hWnd, &ps)
    ; RCX still hWnd (if not clobbered)
    ; We need to be careful. In the _timer branch we assumed RCX was intact.
    ; But in _paint, we are at the top level.
    lea rdx, ps
    call BeginPaint
    mov rbx, rax ; hDC
    
    ; Set Bk Mode Transparent
    mov rcx, rbx
    mov edx, TRANSPARENT_MODE
    call SetBkMode
    
    ; Set Text Color
    mov rcx, rbx
    mov edx, colText ; White
    call SetTextColor
    
    ; Draw Title
    mov rcx, rbx
    mov edx, 10
    mov r8d, 10
    lea r9, szTitle
    mov dword ptr [rsp+20h], 17 ; Length
    call TextOutA
    
    ; Check MMF
    mov rax, pView
    test rax, rax
    jz _endpaint
    
    ; Loop drives (Max 5 for HUD)
    ; Offsets: 16 (Temps)
    xor r12, r12 ; Index
    lea r13, [rax + 16] ; Temps array
    
_drawloop:
    cmp r12, 5
    jge _endpaint
    
    mov eax, [r13 + r12*4] ; Temp
    
    ; Format String
    lea rcx, buffer
    lea rdx, szFmtTemp
    mov r8, r12
    mov r9d, eax
    call wsprintfA
    
    ; Y pos = 30 + Index * 20
    mov eax, r12d
    imul eax, 20
    add eax, 30
    mov r14d, eax ; Y
    
    ; TextOut
    mov rcx, rbx
    mov edx, 10
    mov r8d, r14d
    lea r9, buffer
    mov dword ptr [rsp+20h], 15 ; Length approx
    call TextOutA
    
    ; Draw Bar
    ; Rect: Left=120, Top=Y+2, Right=120+Temp*2, Bottom=Y+14
    lea rcx, rect
    mov dword ptr [rcx], 120         ; Left
    mov dword ptr [rcx+4], r14d      ; Top
    
    mov rax, pView
    mov edx, [r13 + r12*4] ; Re-read temp
    imul edx, 2
    add edx, 120
    mov dword ptr [rcx+8], edx       ; Right
    
    mov edx, r14d
    add edx, 14
    mov dword ptr [rcx+12], edx      ; Bottom
    
    ; Select Brush based on temp
    mov rax, pView
    mov edx, [r13 + r12*4]
    
    cmp edx, 60
    jge _crit
    cmp edx, 45
    jge _warn
    mov r8, hBrushSafe
    jmp _fill
_crit:
    mov r8, hBrushCrit
    jmp _fill
_warn:
    mov r8, hBrushWarn
    
_fill:
    mov rcx, rbx ; hDC
    lea rdx, rect
    ; r8 is brush
    call FillRect
    
    inc r12
    jmp _drawloop
    
_endpaint:
    ; EndPaint(hWnd, &ps)
    ; We need hWnd. It was in RCX at start... but we clobbered it heavily.
    ; We need to verify how we get hWnd.
    ; Normally we save it to stack or register at entry.
    ; But here, we can't easily recover it unless we saved it.
    ; THIS IS A BUG in the naive implementation.
    ; We must save RCX (hWnd) at entry.
    
    ; RE-DESIGN: Retrieve hWnd from stack/register?
    ; It's not standard.
    ; Let's assume we saved it.
    ; I'll fix this in the main code block by saving RCX to a non-volatile register (RDI/RSI/RBX)
    
_destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    jmp _ret

_ret:
    add rsp, 40h
    pop rbp
    ret
WndProc ENDP

; Wrapper to handle register saving
RealWndProc PROC FRAME
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    mov rsi, rcx ; Save hWnd
    
    ; Call logic (inlined for simplicity or jumped)
    ; Re-implementing logic here safely
    
    cmp edx, WM_DESTROY
    je __destroy
    cmp edx, WM_TIMER
    je __timer
    cmp edx, WM_PAINT
    je __paint
    
    call DefWindowProcA
    jmp __done

__timer:
    mov rcx, rsi
    xor rdx, rdx
    mov r8d, 0
    call InvalidateRect
    xor rax, rax
    jmp __done

__paint:
    mov rcx, rsi
    lea rdx, ps
    call BeginPaint
    mov rbx, rax ; hDC
    
    ; Trans
    mov rcx, rbx
    mov edx, TRANSPARENT_MODE
    call SetBkMode
    
    mov rcx, rbx
    mov edx, colText
    call SetTextColor
    
    ; Title
    mov rcx, rbx
    mov edx, 10
    mov r8d, 10
    lea r9, szTitle
    mov dword ptr [rsp+20h], 17
    call TextOutA
    
    ; MMF Check
    mov rax, pView
    test rax, rax
    jz __endpaint
    
    xor r12, r12
    lea r13, [rax + 16]
    
__drawloop:
    cmp r12, 5
    jge __endpaint
    
    ; Prepare string
    mov eax, [r13 + r12*4]
    lea rcx, buffer
    lea rdx, szFmtTemp
    mov r8, r12
    mov r9d, eax
    call wsprintfA
    
    ; Y
    mov eax, r12d
    imul eax, 20
    add eax, 30
    mov r14d, eax
    
    ; Text
    mov rcx, rbx
    mov edx, 10
    mov r8d, r14d
    lea r9, buffer
    mov dword ptr [rsp+20h], 15
    call TextOutA
    
    ; Bar
    lea rcx, rect
    mov dword ptr [rcx], 120
    mov dword ptr [rcx+4], r14d
    
    mov edx, [r13 + r12*4]
    imul edx, 2
    add edx, 120
    mov dword ptr [rcx+8], edx
    
    mov edx, r14d
    add edx, 14
    mov dword ptr [rcx+12], edx
    
    ; Brush
    mov edx, [r13 + r12*4]
    cmp edx, 60
    jge __bcrit
    cmp edx, 45
    jge __bwarn
    mov r8, hBrushSafe
    jmp __bfill
__bcrit:
    mov r8, hBrushCrit
    jmp __bfill
__bwarn:
    mov r8, hBrushWarn
__bfill:
    mov rcx, rbx
    lea rdx, rect
    call FillRect
    
    inc r12
    jmp __drawloop
    
__endpaint:
    mov rcx, rsi
    lea rdx, ps
    call EndPaint
    xor rax, rax
    jmp __done

__destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax

__done:
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RealWndProc ENDP

; ---------------------------------------------------------------------------
; Main Entry
; ---------------------------------------------------------------------------
WinMain PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    call InitMMF

    ; Brushes
    mov ecx, colBarSafe
    call CreateSolidBrush
    mov hBrushSafe, rax
    
    mov ecx, colBarWarn
    call CreateSolidBrush
    mov hBrushWarn, rax
    
    mov ecx, colBarCrit
    call CreateSolidBrush
    mov hBrushCrit, rax
    
    mov ecx, colBackground
    call CreateSolidBrush
    mov hBrushBack, rax

    ; Register Class
    xor rcx, rcx
    call GetModuleHandleA
    mov rbx, rax ; hInstance

    lea rcx, wc
    mov dword ptr [rcx], 80 ; cbSize
    mov dword ptr [rcx+4], CS_VREDRAW or CS_HREDRAW
    lea rax, RealWndProc
    mov qword ptr [rcx+8], rax
    mov dword ptr [rcx+16], 0
    mov dword ptr [rcx+20], 0
    mov qword ptr [rcx+24], rbx
    mov qword ptr [rcx+32], 0 ; Icon
    mov qword ptr [rcx+40], 0 ; Cursor
    mov rax, hBrushBack
    mov qword ptr [rcx+48], rax ; Background
    mov qword ptr [rcx+56], 0 ; Menu
    lea rax, szClassName
    mov qword ptr [rcx+64], rax
    mov qword ptr [rcx+72], 0 ; Small Icon
    
    call RegisterClassExA
    
    ; Create Window
    ; Top-Right: 1500, 100
    mov ecx, WS_EX_LAYERED or WS_EX_TOPMOST or WS_EX_TOOLWINDOW
    lea rdx, szClassName
    lea r8, szAppName
    mov r9d, WS_POPUP or WS_VISIBLE
    mov dword ptr [rsp+20h], 1400 ; X
    mov dword ptr [rsp+28h], 100  ; Y
    mov dword ptr [rsp+30h], 300  ; Width
    mov dword ptr [rsp+38h], 200  ; Height
    mov qword ptr [rsp+40h], 0    ; Parent
    mov qword ptr [rsp+48h], 0    ; Menu
    mov qword ptr [rsp+50h], rbx  ; hInst
    mov qword ptr [rsp+58h], 0    ; Param
    call CreateWindowExA
    mov rsi, rax ; hWnd
    
    ; Set Layered Attributes (Color Key = 0x101010)
    mov rcx, rsi
    mov edx, 00101010h ; CRKEY
    mov r8d, 200 ; Alpha (0-255)
    mov r9d, LWA_COLORKEY ; Key Only? Or Alph? LWA_COLORKEY | LWA_ALPHA
    ; If we want transparency for background but solid text:
    ; Use LWA_COLORKEY. Background is 0x101010.
    mov r9d, LWA_COLORKEY
    call SetLayeredWindowAttributes

    ; Show
    mov rcx, rsi
    mov edx, 5 ; SW_SHOW
    call ShowWindow
    
    mov rcx, rsi
    call UpdateWindow
    
    ; Timer (500ms)
    mov rcx, rsi
    mov edx, 1
    mov r8d, 500
    xor r9, r9
    call SetTimer
    
    ; Msg Loop
_msg_loop:
    lea rcx, msg
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    call GetMessageA
    test eax, eax
    jz _exit
    
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageA
    jmp _msg_loop
    
_exit:
    mov ecx, 0
    call ExitProcess

WinMain ENDP

END
