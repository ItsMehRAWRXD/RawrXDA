; =============================================================================
; RawrXD_Agentic_GUI_ml64.asm
; GUI entry with live token streaming into editor surface
; =============================================================================

OPTION CASEMAP:NONE

EXTERN GetModuleHandleA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN SetTimer:PROC
EXTERN ExitProcess:PROC

EXTERN InitializeAmphibiousCore:PROC
EXTERN RunAutonomousCycle_ml64:PROC

WS_OVERLAPPEDWINDOW EQU 00CF0000h
WS_VISIBLE          EQU 10000000h
WS_VSCROLL          EQU 00200000h
ES_MULTILINE        EQU 00000004h
ES_AUTOVSCROLL      EQU 00000040h
ES_AUTOHSCROLL      EQU 00000080h

STYLE_EDIT_WINDOW   EQU 10EF00C4h
CW_USEDEFAULT       EQU 80000000h
SW_SHOWDEFAULT      EQU 10
WM_TIMER            EQU 0113h

.data
    szEditClass db "EDIT",0
    szTitle     db "RawrXD Amphibious Live Stream",0
    szGuiPrompt db "Live GUI request: stream local-model output directly into the IDE editor surface.",0

.code

GuiMain PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 184
    .allocstack 184
    .endprolog

    xor rcx, rcx
    call GetModuleHandleA
    mov rbx, rax

    xor ecx, ecx
    lea rdx, szEditClass
    lea r8, szTitle
    mov r9d, STYLE_EDIT_WINDOW

    mov eax, CW_USEDEFAULT
    mov qword ptr [rsp+20h], rax
    mov qword ptr [rsp+28h], rax

    mov qword ptr [rsp+30h], 980
    mov qword ptr [rsp+38h], 640
    mov qword ptr [rsp+40h], 0
    mov qword ptr [rsp+48h], 0
    mov qword ptr [rsp+50h], rbx
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA

    test rax, rax
    jz GuiFail

    mov rbx, rax

    mov rcx, rbx
    mov edx, SW_SHOWDEFAULT
    call ShowWindow

    mov rcx, rbx
    call UpdateWindow

    mov rcx, rbx
    xor rdx, rdx
    lea r8, szGuiPrompt
    mov r9d, 1
    call InitializeAmphibiousCore

    call RunAutonomousCycle_ml64

    mov rcx, rbx
    mov rdx, 1
    mov r8d, 700
    xor r9d, r9d
    call SetTimer

GuiLoop:
    lea rcx, [rsp+60h]
    xor rdx, rdx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA

    test eax, eax
    jle GuiExit

    cmp dword ptr [rsp+68h], WM_TIMER
    jne GuiDispatch
    call RunAutonomousCycle_ml64

GuiDispatch:
    lea rcx, [rsp+60h]
    call TranslateMessage
    lea rcx, [rsp+60h]
    call DispatchMessageA
    jmp GuiLoop

GuiExit:
    xor ecx, ecx
    call ExitProcess

GuiFail:
    mov ecx, 2
    call ExitProcess

GuiMain ENDP

END
