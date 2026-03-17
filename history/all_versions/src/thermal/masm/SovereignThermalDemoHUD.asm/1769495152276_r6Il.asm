; ============================================================================
; SovereignThermalDemoHUD.asm
; Real-Time Thermal Visualization for Safe Demo
; Lightweight GDI32 overlay showing live thermal bars and state machine
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
extern GetTickCount64 : PROC

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

; Demo states
DEMO_STATE_NORMAL   equ 0
DEMO_STATE_HEATING  equ 1
DEMO_STATE_COOLING  equ 2
DEMO_STATE_THROTTLE equ 3

; MMF Offsets
OFF_SIGNATURE       equ 0
OFF_COUNT           equ 8
OFF_TEMPS           equ 16

.data
    ; Colors
    COL_BACKGROUND      dd 00101010h ; Dark Grey (Key)
    COL_BAR_SAFE        dd 0000FF00h ; Green (BGR)
    COL_BAR_WARN        dd 0000FFFFh ; Yellow
    COL_BAR_CRIT        dd 000000FFh ; Red
    COL_TEXT            dd 00FFFFFFh ; White
    COL_STATE_NORMAL    dd 0000FF00h ; Green
    COL_STATE_HEATING   dd 0000FFFFh ; Yellow
    COL_STATE_THROTTLE  dd 000000FFh ; Red
    COL_STATE_COOLING   dd 0000AAFFh ; Orange
    szClassName     db "SovereignDemoHUD", 0
    szAppName       db "Sovereign Thermal Demo HUD", 0
    szMMFName       db "Global\SOVEREIGN_NVME_TEMPS", 0
    
    ; HUD Layout
    HUD_WIDTH       equ 400
    HUD_HEIGHT      equ 300
    HUD_X           equ 1400
    HUD_Y           equ 100
    
    ; Format strings
    szFmtTemp       db "Drive %d: %d°C", 0
    szFmtState      db "State: %s", 0
    szFmtCycle      db "Cycle: %d", 0
    szFmtSwap       db "SWAP: %d -> %d", 0
    szStateNormal   db "NORMAL", 0
    szStateHeating  db "HEATING", 0
    szStateCooling  db "COOLING", 0
    szStateThrottle db "THROTTLE", 0
    
    ; MMF
    hMMF            dq 0
    pView           dq 0
    g_CycleCount    dq 0
    g_DemoState     dd DEMO_STATE_NORMAL
    g_LastSwap      db 32 dup(0)
    
    ; GDI objects
    hBrushBack      dq 0
    hBrushSafe      dq 0
    hBrushWarn      dq 0
    hBrushCrit      dq 0
    hBrushState     dq 0
    
    ; Buffers
    wc              db 80 dup(0)
    msg             db 48 dup(0)
    ps              db 72 dup(0)
    rect            db 16 dup(0)
    buffer          db 64 dup(0)
    stateBuffer     db 32 dup(0)

.code

; ---------------------------------------------------------------------------
; InitMMF
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
; GetStateString
; ---------------------------------------------------------------------------
GetStateString PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov eax, g_DemoState
    cmp eax, DEMO_STATE_NORMAL
    je _normal
    cmp eax, DEMO_STATE_HEATING
    je _heating
    cmp eax, DEMO_STATE_COOLING
    je _cooling
    cmp eax, DEMO_STATE_THROTTLE
    je _throttle
    
    lea rax, szStateNormal
    jmp _done
    
_normal:
    lea rax, szStateNormal
    jmp _done
    
_heating:
    lea rax, szStateHeating
    jmp _done
    
_cooling:
    lea rax, szStateCooling
    jmp _done
    
_throttle:
    lea rax, szStateThrottle
    
_done:
    add rsp, 28h
    ret
GetStateString ENDP

; ---------------------------------------------------------------------------
; WndProc
; ---------------------------------------------------------------------------
WndProc PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    cmp edx, WM_DESTROY
    je _destroy
    cmp edx, WM_TIMER
    je _timer
    cmp edx, WM_PAINT
    je _paint
    
    call DefWindowProcA
    jmp _ret
    
_timer:
    mov rcx, [rbp+10h]  ; hWnd from stack
    xor rdx, rdx
    mov r8d, 0
    call InvalidateRect
    xor rax, rax
    jmp _ret
    
_paint:
    mov rcx, [rbp+10h]  ; hWnd
    lea rdx, ps
    call BeginPaint
    mov rbx, rax  ; hDC
    
    ; Set transparent background
    mov rcx, rbx
    mov edx, TRANSPARENT_MODE
    call SetBkMode
    
    ; Set text color
    mov rcx, rbx
    mov edx, [COL_TEXT]
    call SetTextColor
    
    ; Draw title
    mov rcx, rbx
    mov edx, 10
    mov r8d, 10
    lea r9, szAppName
    mov dword ptr [rsp+20h], 28
    call TextOutA
    
    ; Check MMF
    mov rax, pView
    test rax, rax
    jz _endpaint
    
    ; Get drive count
    mov ecx, [rax + OFF_COUNT]
    cmp ecx, 8
    jle _draw_drives
    mov ecx, 8
    
_draw_drives:
    xor r12, r12  ; Index
    lea r13, [rax + OFF_TEMPS]  ; Temps array
    
_drive_loop:
    cmp r12, rcx
    jge _draw_state
    
    ; Get temperature
    mov eax, [r13 + r12*4]
    cmp eax, 0
    jle _next_drive  ; Skip invalid temps
    
    ; Format temp string
    lea rcx, buffer
    lea rdx, szFmtTemp
    mov r8, r12
    mov r9d, eax
    call wsprintfA
    
    ; Calculate Y position
    mov eax, r12d
    imul eax, 25
    add eax, 40
    mov r14d, eax
    
    ; Draw text
    mov rcx, rbx
    mov edx, 10
    mov r8d, r14d
    lea r9, buffer
    mov dword ptr [rsp+20h], 15
    call TextOutA
    
    ; Draw thermal bar
    lea r15, rect
    mov dword ptr [r15], 120      ; Left
    mov dword ptr [r15+4], r14d   ; Top
    
    ; Right = 120 + temp * 2
    mov edx, [r13 + r12*4]
    imul edx, 2
    add edx, 120
    mov dword ptr [r15+8], edx    ; Right
    
    mov edx, r14d
    add edx, 18
    mov dword ptr [r15+12], edx   ; Bottom
    
    ; Select brush color based on temp
    mov edx, [r13 + r12*4]
    cmp edx, 60
    jge _crit_brush
    cmp edx, 45
    jge _warn_brush
    mov r8, hBrushSafe
    jmp _fill_bar
    
_crit_brush:
    mov r8, hBrushCrit
    jmp _fill_bar
    
_warn_brush:
    mov r8, hBrushWarn
    
_fill_bar:
    mov rcx, rbx
    mov rdx, r15
    call FillRect
    
_next_drive:
    inc r12
    jmp _drive_loop
    
_draw_state:
    ; Draw state
    call GetStateString
    mov r12, rax
    
    lea rcx, buffer
    lea rdx, szFmtState
    mov r8, r12
    call wsprintfA
    
    mov rcx, rbx
    mov edx, 10
    mov r8d, 220
    lea r9, buffer
    mov dword ptr [rsp+20h], 15
    call TextOutA
    
    ; Draw cycle count
    lea rcx, buffer
    lea rdx, szFmtCycle
    mov r8, g_CycleCount
    call wsprintfA
    
    mov rcx, rbx
    mov edx, 200
    mov r8d, 220
    lea r9, buffer
    mov dword ptr [rsp+20h], 10
    call TextOutA
    
_endpaint:
    mov rcx, [rbp+10h]
    lea rdx, ps
    call EndPaint
    xor rax, rax
    jmp _ret
    
_destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    
_ret:
    add rsp, 40h
    pop rbp
    ret
WndProc ENDP

; ---------------------------------------------------------------------------
; WinMain
; ---------------------------------------------------------------------------
WinMain PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    call InitMMF
    
    ; Create brushes
    mov ecx, [COL_BAR_SAFE]
    call CreateSolidBrush
    mov hBrushSafe, rax
    
    mov ecx, [COL_BAR_WARN]
    call CreateSolidBrush
    mov hBrushWarn, rax
    
    mov ecx, [COL_BAR_CRIT]
    call CreateSolidBrush
    mov hBrushCrit, rax
    
    mov ecx, [COL_BACKGROUND]
    call CreateSolidBrush
    mov hBrushBack, rax
    
    ; Register window class
    xor rcx, rcx
    call GetModuleHandleA
    mov rbx, rax
    
    lea rcx, wc
    mov dword ptr [rcx], 80
    mov dword ptr [rcx+4], CS_VREDRAW or CS_HREDRAW
    lea rax, WndProc
    mov qword ptr [rcx+8], rax
    mov dword ptr [rcx+16], 0
    mov dword ptr [rcx+20], 0
    mov qword ptr [rcx+24], rbx
    mov qword ptr [rcx+32], 0
    mov qword ptr [rcx+40], 0
    mov rax, hBrushBack
    mov qword ptr [rcx+48], rax
    mov qword ptr [rcx+56], 0
    lea rax, szClassName
    mov qword ptr [rcx+64], rax
    mov qword ptr [rcx+72], 0
    
    call RegisterClassExA
    
    ; Create window
    mov ecx, WS_EX_LAYERED or WS_EX_TOPMOST or WS_EX_TOOLWINDOW
    lea rdx, szClassName
    lea r8, szAppName
    mov r9d, WS_POPUP or WS_VISIBLE
    mov dword ptr [rsp+20h], HUD_X
    mov dword ptr [rsp+28h], HUD_Y
    mov dword ptr [rsp+30h], HUD_WIDTH
    mov dword ptr [rsp+38h], HUD_HEIGHT
    mov qword ptr [rsp+40h], 0
    mov qword ptr [rsp+48h], 0
    mov qword ptr [rsp+50h], rbx
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA
    mov rsi, rax
    
    ; Set layered attributes (transparent background)
    mov rcx, rsi
    mov edx, [COL_BACKGROUND]
    mov r8d, 200
    mov r9d, LWA_COLORKEY
    call SetLayeredWindowAttributes
    
    ; Show window
    mov rcx, rsi
    mov edx, 5
    call ShowWindow
    
    mov rcx, rsi
    call UpdateWindow
    
    ; Set timer (250ms updates)
    mov rcx, rsi
    mov edx, 1
    mov r8d, 250
    xor r9, r9
    call SetTimer
    
    ; Message loop
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
    
    add rsp, 28h
    ret
WinMain ENDP

; ---------------------------------------------------------------------------
; Main Entry (Windows subsystem)
; ---------------------------------------------------------------------------
main PROC
    sub rsp, 28h
    call WinMain
    add rsp, 28h
    ret
main ENDP

END main
