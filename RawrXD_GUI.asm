;=============================================================================
; RawrXD_GUI.asm  — v2
; Amphibious Win32 GUI entry point
; ml64.exe + link /SUBSYSTEM:WINDOWS /ENTRY:WinMain
;
; Stack alignment rule (Win64):
;   RSP must be 16-byte aligned BEFORE every CALL instruction.
;   On function entry RSP%16=8.  1 push -> (N-8)%16=0.
;   sub rsp, K with K%16=0 keeps alignment.
;
;  WinMain: 1 push (rbp)  -> sub rsp, 96  (96%16=0) OK
;  WndProc: 1 push (rbp)  -> sub rsp, 128 (128%16=0) OK
;    layout: [rsp+0..7]=hWnd  [+8..15]=uMsg  [+16..23]=wParam
;            [+24..31]=lParam  [+32..103]=PAINTSTRUCT (72 b)
;=============================================================================
OPTION CASEMAP:NONE

;-----------------------------------------------------------------------------
; Sovereign core
;-----------------------------------------------------------------------------
EXTERN  Sovereign_Pipeline_Cycle    :PROC
EXTERN  g_CycleCounter              :QWORD
EXTERN  g_SovereignStatus           :QWORD
EXTERN  g_ActiveAgentCount          :DWORD

;-----------------------------------------------------------------------------
; Win32
;-----------------------------------------------------------------------------
EXTERN  LoadCursorA         :PROC
EXTERN  RegisterClassExA    :PROC
EXTERN  CreateWindowExA     :PROC
EXTERN  ShowWindow          :PROC
EXTERN  UpdateWindow        :PROC
EXTERN  SetTimer            :PROC
EXTERN  GetMessageA         :PROC
EXTERN  TranslateMessage    :PROC
EXTERN  DispatchMessageA    :PROC
EXTERN  PostQuitMessage     :PROC
EXTERN  BeginPaint          :PROC
EXTERN  EndPaint            :PROC
EXTERN  TextOutA            :PROC
EXTERN  DefWindowProcA      :PROC
EXTERN  ExitProcess         :PROC
EXTERN  GetStockObject      :PROC
EXTERN  wsprintfA           :PROC
EXTERN  SetBkMode           :PROC
EXTERN  SetTextColor        :PROC
EXTERN  lstrlenA            :PROC

;-----------------------------------------------------------------------------
; Constants
;-----------------------------------------------------------------------------
IDC_ARROW           EQU 32512
WHITE_BRUSH         EQU 0
TRANSPARENT_BK      EQU 1
WM_DESTROY          EQU 002h
WM_TIMER            EQU 0113h
WM_PAINT            EQU 00Fh
WM_CREATE           EQU 001h
SW_SHOWNORMAL       EQU 1
TIMER_AGENTIC       EQU 1
TIMER_MS            EQU 1000
CS_HREDRAW          EQU 0002h
CS_VREDRAW          EQU 0001h
WS_OVERLAPPEDWINDOW EQU 0CF0000h
CW_USEDEFAULT32     EQU 080000000h
COLOR_LIME          EQU 000FF00h

;=============================================================================
.data
ALIGN 16
szClassName DB  "RawrXD_SovereignWnd",0
szTitle     DB  "RawrXD | Sovereign Agentic Host",0
szLine1Fmt  DB  "Agentic Cycle: %I64u   Status: 0x%I64X",0
szLine2     DB  "IDE > Chat > Prompt > LLM > Stream > Renderer > [SOVEREIGN]",0
szLine3     DB  "WM_TIMER @ 1s driving Sovereign_Pipeline_Cycle",0

g_hInstance QWORD   0
g_hWnd      QWORD   0
g_StatusBuf DB      256 DUP(0)

; WNDCLASSEX static struct (80 bytes, filled at runtime)
ALIGN 8
g_WC_cbSize         DWORD   80
g_WC_style          DWORD   (CS_HREDRAW OR CS_VREDRAW)
g_WC_lpfnWndProc    QWORD   0
g_WC_cbClsExtra     DWORD   0
g_WC_cbWndExtra     DWORD   0
g_WC_hInstance      QWORD   0
g_WC_hIcon          QWORD   0
g_WC_hCursor        QWORD   0
g_WC_hbrBackground  QWORD   0
g_WC_lpszMenuName   QWORD   0
g_WC_lpszClassName  QWORD   0
g_WC_hIconSm        QWORD   0

;=============================================================================
.code

;=============================================================================
; WndProc
;   RCX=hWnd  RDX=uMsg  R8=wParam  R9=lParam
;
;   1 push (rbp) -> sub rsp, 144
;   [rsp+0..7]   = hWnd
;   [rsp+8..15]  = uMsg (QWORD)
;   [rsp+16..23] = wParam
;   [rsp+24..31] = lParam
;   [rsp+32..63] = shadow / 5th+ args for outgoing calls
;   [rsp+64..135]= PAINTSTRUCT (72 bytes)  PAINTSTRUCT.hdc = [rsp+64]
;=============================================================================
WndProc PROC FRAME
    push    rbp
    .PUSHREG rbp
    sub     rsp, 144
    .ALLOCSTACK 144
    lea     rbp, [rsp+144]
    .SETFRAME rbp, 144
    .ENDPROLOG

    mov     [rsp+0],  rcx
    mov     [rsp+8],  rdx
    mov     [rsp+16], r8
    mov     [rsp+24], r9

    cmp     edx, WM_CREATE
    je      Msg_Create
    cmp     edx, WM_TIMER
    je      Msg_Timer
    cmp     edx, WM_PAINT
    je      Msg_Paint
    cmp     edx, WM_DESTROY
    je      Msg_Destroy
    jmp     Msg_Def

Msg_Create:
    mov     dword ptr [g_ActiveAgentCount], 1
    mov     rcx, [rsp+0]
    mov     edx, TIMER_AGENTIC
    mov     r8d, TIMER_MS
    xor     r9d, r9d
    call    SetTimer
    xor     eax, eax
    jmp     WndProcRet

Msg_Timer:
    cmp     qword ptr [rsp+16], TIMER_AGENTIC
    jne     Msg_Def
    call    Sovereign_Pipeline_Cycle
    xor     eax, eax
    jmp     WndProcRet

Msg_Paint:
    mov     rcx, [rsp+0]
    lea     rdx, [rsp+64]       ; &PAINTSTRUCT
    call    BeginPaint
    ; hDC is now in PAINTSTRUCT.hdc at [rsp+64] and in rax

    mov     rcx, [rsp+64]       ; hDC
    mov     edx, TRANSPARENT_BK
    call    SetBkMode

    mov     rcx, [rsp+64]       ; hDC
    mov     edx, COLOR_LIME
    call    SetTextColor

    ; format cycle status into g_StatusBuf
    lea     rcx, g_StatusBuf
    lea     rdx, szLine1Fmt
    mov     r8,  qword ptr [g_CycleCounter]
    mov     r9,  qword ptr [g_SovereignStatus]
    call    wsprintfA

    ; TextOutA: line 1 (g_StatusBuf)
    lea     rcx, g_StatusBuf
    call    lstrlenA
    mov     [rsp+32], rax       ; 5th arg = char count
    mov     rcx, [rsp+64]       ; hDC (safe: rsp+64 = PAINTSTRUCT.hdc, not touched by above)
    mov     edx, 8
    mov     r8d, 12
    lea     r9,  g_StatusBuf
    call    TextOutA

    ; TextOutA: line 2
    lea     rcx, szLine2
    call    lstrlenA
    mov     [rsp+32], rax
    mov     rcx, [rsp+64]
    mov     edx, 8
    mov     r8d, 32
    lea     r9,  szLine2
    call    TextOutA

    ; TextOutA: line 3
    lea     rcx, szLine3
    call    lstrlenA
    mov     [rsp+32], rax
    mov     rcx, [rsp+64]
    mov     edx, 8
    mov     r8d, 52
    lea     r9,  szLine3
    call    TextOutA

    mov     rcx, [rsp+0]
    lea     rdx, [rsp+64]       ; &PAINTSTRUCT
    call    EndPaint
    xor     eax, eax
    jmp     WndProcRet

Msg_Destroy:
    xor     ecx, ecx
    call    PostQuitMessage
    xor     eax, eax
    jmp     WndProcRet

Msg_Def:
    mov     rcx, [rsp+0]
    mov     rdx, [rsp+8]
    mov     r8,  [rsp+16]
    mov     r9,  [rsp+24]
    call    DefWindowProcA

WndProcRet:
    add     rsp, 144
    pop     rbp
    ret
WndProc ENDP

;=============================================================================
; WinMain
;   RCX=hInstance  RDX=hPrevInstance  R8=lpCmdLine  R9=nCmdShow
;
;   1 push (rbp) -> sub rsp, 96 (96%16=0)
;   [rsp+0..47]  = MSG struct
;   [rsp+32..88] = CreateWindowExA args 4-11 (shadow at 32-63, args 64-95)
;=============================================================================
WinMain PROC FRAME
    push    rbp
    .PUSHREG rbp
    sub     rsp, 96
    .ALLOCSTACK 96
    lea     rbp, [rsp+96]
    .SETFRAME rbp, 96
    .ENDPROLOG

    mov     qword ptr [g_hInstance], rcx

    ; fill WNDCLASSEX
    lea     rax, WndProc
    mov     qword ptr [g_WC_lpfnWndProc], rax
    mov     qword ptr [g_WC_hInstance], rcx
    lea     rax, szClassName
    mov     qword ptr [g_WC_lpszClassName], rax

    xor     ecx, ecx
    mov     edx, IDC_ARROW
    call    LoadCursorA
    mov     qword ptr [g_WC_hCursor], rax

    mov     ecx, WHITE_BRUSH
    call    GetStockObject
    mov     qword ptr [g_WC_hbrBackground], rax

    lea     rcx, g_WC_cbSize    ; address of WNDCLASSEX (first field)
    call    RegisterClassExA
    test    ax, ax
    jz      WinFail

    ; CreateWindowExA (12 args: 4 regs + 8 stack)
    xor     ecx, ecx
    lea     rdx, szClassName
    lea     r8,  szTitle
    mov     r9d, WS_OVERLAPPEDWINDOW
    mov     qword ptr [rsp+32], CW_USEDEFAULT32    ; x
    mov     qword ptr [rsp+40], CW_USEDEFAULT32    ; y
    mov     qword ptr [rsp+48], 860                ; cx
    mov     qword ptr [rsp+56], 360                ; cy
    mov     qword ptr [rsp+64], 0                  ; hWndParent
    mov     qword ptr [rsp+72], 0                  ; hMenu
    mov     rax, qword ptr [g_hInstance]
    mov     qword ptr [rsp+80], rax                ; hInstance
    mov     qword ptr [rsp+88], 0                  ; lpParam
    call    CreateWindowExA
    test    rax, rax
    jz      WinFail
    mov     qword ptr [g_hWnd], rax

    mov     rcx, rax
    mov     edx, SW_SHOWNORMAL
    call    ShowWindow
    mov     rcx, qword ptr [g_hWnd]
    call    UpdateWindow

    ; message loop
MsgLoop:
    lea     rcx, [rsp+0]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    GetMessageA
    test    eax, eax
    jz      MsgDone
    lea     rcx, [rsp+0]
    call    TranslateMessage
    lea     rcx, [rsp+0]
    call    DispatchMessageA
    jmp     MsgLoop

MsgDone:
    mov     ecx, dword ptr [rsp+16]  ; MSG.wParam (exit code)
    call    ExitProcess

WinFail:
    mov     ecx, 1
    call    ExitProcess

    add     rsp, 96
    pop     rbp
    ret
WinMain ENDP

END
