; RawrXD_GUI_Titan.asm - Win32 IDE with Titan Ring Consumer
; Assemble: ml64 /c /Zi RawrXD_GUI_Titan.asm
; Link: link /SUBSYSTEM:WINDOWS /OUT:RawrXD-IDE.exe RawrXD_GUI_Titan.obj Titan_Streaming_Orchestrator_Fixed.obj kernel32.lib user32.lib gdi32.lib comctl32.lib

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ============================================================================
; EXTERNAL IMPORTS - Win32 API
; ============================================================================
EXTERN GetModuleHandleA:PROC
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN LoadIconA:PROC
EXTERN LoadCursorA:PROC
EXTERN GetClientRect:PROC
EXTERN SetWindowPos:PROC
EXTERN SendMessageA:PROC
EXTERN SetTimer:PROC
EXTERN KillTimer:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN LoadLibraryA:PROC
EXTERN CreateFontA:PROC
EXTERN DeleteObject:PROC
EXTERN SetFocus:PROC
EXTERN GetDlgItem:PROC
EXTERN SetWindowTextA:PROC
EXTERN lstrlenA:PROC

; ============================================================================
; EXTERNAL IMPORTS - Titan High-Level API
; ============================================================================
EXTERN Titan_InitOrchestrator:PROC
EXTERN Titan_CreateContext:PROC
EXTERN Titan_LoadModel_GGUF:PROC
EXTERN Titan_BeginStreamingInference:PROC
EXTERN Titan_ConsumeToken:PROC
EXTERN Titan_Shutdown:PROC

; ============================================================================
; CONSTANTS
; ============================================================================
; Window styles
WS_OVERLAPPEDWINDOW EQU 00CF0000h
WS_VISIBLE          EQU 10000000h
WS_CHILD            EQU 40000000h
WS_VSCROLL          EQU 00200000h
WS_HSCROLL          EQU 00100000h
WS_EX_CLIENTEDGE    EQU 00000200h

; Edit styles
ES_MULTILINE        EQU 0004h
ES_AUTOVSCROLL      EQU 0040h
ES_AUTOHSCROLL      EQU 0080h
ES_READONLY         EQU 0800h
ES_WANTRETURN       EQU 1000h

; Messages
WM_CREATE           EQU 0001h
WM_DESTROY          EQU 0002h
WM_SIZE             EQU 0005h
WM_SETFONT          EQU 0030h
WM_COMMAND          EQU 0111h
WM_TIMER            EQU 0113h
WM_USER             EQU 0400h

; Edit messages
EM_SETSEL           EQU 00B1h
EM_REPLACESEL       EQU 00C2h
EM_SCROLLCARET      EQU 00B7h
EM_SETBKGNDCOLOR    EQU 0443h
WM_GETTEXTLENGTH    EQU 000Eh
WM_GETTEXT          EQU 000Dh

; Button notification
BN_CLICKED          EQU 0

; Colors
COLOR_WINDOW        EQU 5

; Misc
SW_SHOW             EQU 5
IDI_APPLICATION     EQU 32512
IDC_ARROW           EQU 32512
CLEARTYPE_QUALITY   EQU 5
FIXED_PITCH         EQU 1
HEAP_ZERO_MEMORY    EQU 8
SWP_NOZORDER        EQU 4

; Control IDs
ID_EDIT_INPUT       EQU 1001
ID_EDIT_OUTPUT      EQU 1002
ID_BTN_RUN          EQU 1003
ID_BTN_STOP         EQU 1004
ID_STATUS           EQU 1005
TIMER_POLL_ID       EQU 1

; Custom messages
WM_TITAN_TOKEN      EQU WM_USER + 100

; Buffer sizes
TOKEN_BUF_SIZE      EQU 4096
PROMPT_BUF_SIZE     EQU 16384

; ============================================================================
; STRUCTURES
; ============================================================================
WNDCLASSEXA STRUCT
    cbSize          DWORD ?
    style           DWORD ?
    lpfnWndProc     QWORD ?
    cbClsExtra      DWORD ?
    cbWndExtra      DWORD ?
    hInstance       QWORD ?
    hIcon           QWORD ?
    hCursor         QWORD ?
    hbrBackground   QWORD ?
    lpszMenuName    QWORD ?
    lpszClassName   QWORD ?
    hIconSm         QWORD ?
WNDCLASSEXA ENDS

MSG STRUCT
    hwnd            QWORD ?
    message         DWORD ?
    wParam          QWORD ?
    lParam          QWORD ?
    time            DWORD ?
    pt              QWORD ?
MSG ENDS

RECT STRUCT
    left            DWORD ?
    top             DWORD ?
    right           DWORD ?
    bottom          DWORD ?
RECT ENDS

; ============================================================================
; DATA SECTION
; ============================================================================
.DATA
    szClassName     BYTE "RawrXD_Titan_IDE",0
    szWindowTitle   BYTE "RawrXD Titan IDE [64MB DMA Ring | AVX-512]",0
    szRichDll       BYTE "Riched20.dll",0
    szRichClass     BYTE "RichEdit20W",0
    szEditClass     BYTE "EDIT",0
    szButtonClass   BYTE "BUTTON",0
    szStaticClass   BYTE "STATIC",0
    szFont          BYTE "Consolas",0
    
    szBtnRun        BYTE "Run Inference",0
    szBtnStop       BYTE "Stop",0
    szStatusReady   BYTE "Ready | Titan Orchestrator Active | 64MB Ring",0
    szStatusRunning BYTE "Streaming... | Tokens: ",0
    szStatusDone    BYTE "Complete | Total Tokens: ",0
    
    szDefaultModel  BYTE "D:\rawrxd\models\phi-3-mini.gguf",0
    szInitFail      BYTE "Titan Init Failed",0

.DATA?
    ALIGN 8
    hInstance       QWORD ?
    hWndMain        QWORD ?
    hEditInput      QWORD ?
    hEditOutput     QWORD ?
    hBtnRun         QWORD ?
    hBtnStop        QWORD ?
    hStatus         QWORD ?
    hFont           QWORD ?
    hContext        QWORD ?
    
    wc              WNDCLASSEXA <>
    msg             MSG <>
    rcClient        RECT <>
    
    tokenBuf        BYTE TOKEN_BUF_SIZE DUP(?)
    promptBuf       BYTE PROMPT_BUF_SIZE DUP(?)
    statusBuf       BYTE 256 DUP(?)
    
    dwTokenCount    DWORD ?
    bStreaming      DWORD ?

; ============================================================================
; CODE SECTION
; ============================================================================
.CODE

; ----------------------------------------------------------------------------
; WinMain - Entry point
; ----------------------------------------------------------------------------
WinMain PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    ; Get module handle
    xor ecx, ecx
    call GetModuleHandleA
    mov hInstance, rax
    
    ; Load RichEdit
    lea rcx, szRichDll
    call LoadLibraryA
    
    ; Initialize Titan Orchestrator
    call Titan_InitOrchestrator
    test rax, rax
    jnz @@init_failed
    
    ; Create Titan context
    call Titan_CreateContext
    test rax, rax
    jz @@init_failed
    mov hContext, rax
    
    ; Load default model
    mov rcx, hContext
    lea rdx, szDefaultModel
    call Titan_LoadModel_GGUF
    ; Ignore failure for now - can load later
    
    jmp @@register_class
    
@@init_failed:
    ; Could show message box here
    mov eax, 1
    jmp @@exit
    
@@register_class:
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, 3                 ; CS_HREDRAW | CS_VREDRAW
    lea rax, WndProc
    mov wc.lpfnWndProc, rax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov rax, hInstance
    mov wc.hInstance, rax
    
    mov ecx, IDI_APPLICATION
    xor edx, edx
    call LoadIconA
    mov wc.hIcon, rax
    mov wc.hIconSm, rax
    
    xor ecx, ecx
    mov edx, IDC_ARROW
    call LoadCursorA
    mov wc.hCursor, rax
    
    mov wc.hbrBackground, COLOR_WINDOW + 1
    mov wc.lpszMenuName, 0
    lea rax, szClassName
    mov wc.lpszClassName, rax
    
    lea rcx, wc
    call RegisterClassExA
    
    ; Create main window
    xor ecx, ecx                    ; dwExStyle
    lea rdx, szClassName
    lea r8, szWindowTitle
    mov r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE
    mov DWORD PTR [rsp+32], 100     ; x
    mov DWORD PTR [rsp+40], 100     ; y
    mov DWORD PTR [rsp+48], 1200    ; width
    mov DWORD PTR [rsp+56], 800     ; height
    mov QWORD PTR [rsp+64], 0       ; hWndParent
    mov QWORD PTR [rsp+72], 0       ; hMenu
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0       ; lpParam
    call CreateWindowExA
    mov hWndMain, rax
    
    test rax, rax
    jz @@exit
    
    ; Create monospace font
    mov ecx, -16                    ; height
    xor edx, edx                    ; width
    xor r8d, r8d                    ; escapement
    xor r9d, r9d                    ; orientation
    mov DWORD PTR [rsp+32], 400     ; weight
    mov DWORD PTR [rsp+40], 0       ; italic
    mov DWORD PTR [rsp+48], 0       ; underline
    mov DWORD PTR [rsp+56], 0       ; strikeout
    mov DWORD PTR [rsp+64], 0       ; charset
    mov DWORD PTR [rsp+72], 0       ; outprecision
    mov DWORD PTR [rsp+80], 0       ; clipprecision
    mov DWORD PTR [rsp+88], CLEARTYPE_QUALITY
    mov DWORD PTR [rsp+96], FIXED_PITCH
    lea rax, szFont
    mov [rsp+104], rax
    call CreateFontA
    mov hFont, rax
    
    ; Message loop
@@msg_loop:
    lea rcx, msg
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA
    
    test eax, eax
    jz @@exit
    
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageA
    jmp @@msg_loop
    
@@exit:
    call Titan_Shutdown
    
    add rsp, 128
    pop r12
    pop rbx
    pop rbp
    ret
WinMain ENDP

; ----------------------------------------------------------------------------
; WndProc - Window procedure
; ----------------------------------------------------------------------------
WndProc PROC FRAME hWnd:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    mov [rbp+16], rcx       ; hWnd
    mov [rbp+24], edx       ; uMsg
    mov [rbp+32], r8        ; wParam
    mov [rbp+40], r9        ; lParam
    
    cmp edx, WM_CREATE
    je @@OnCreate
    cmp edx, WM_SIZE
    je @@OnSize
    cmp edx, WM_COMMAND
    je @@OnCommand
    cmp edx, WM_TIMER
    je @@OnTimer
    cmp edx, WM_DESTROY
    je @@OnDestroy
    jmp @@Default
    
; ---- WM_CREATE ----
@@OnCreate:
    mov rbx, [rbp+16]       ; hWnd
    
    ; Create input edit (top 60%)
    mov ecx, WS_EX_CLIENTEDGE
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_WANTRETURN
    mov DWORD PTR [rsp+32], 10      ; x
    mov DWORD PTR [rsp+40], 10      ; y
    mov DWORD PTR [rsp+48], 1160    ; width
    mov DWORD PTR [rsp+56], 400     ; height
    mov [rsp+64], rbx               ; parent
    mov QWORD PTR [rsp+72], ID_EDIT_INPUT
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0
    call CreateWindowExA
    mov hEditInput, rax
    
    ; Set font
    mov rcx, rax
    mov edx, WM_SETFONT
    mov r8, hFont
    mov r9d, 1
    call SendMessageA
    
    ; Create output edit (bottom 30%)
    mov ecx, WS_EX_CLIENTEDGE
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_VSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY
    mov DWORD PTR [rsp+32], 10      ; x
    mov DWORD PTR [rsp+40], 460     ; y
    mov DWORD PTR [rsp+48], 1160    ; width
    mov DWORD PTR [rsp+56], 250     ; height
    mov rbx, [rbp+16]
    mov [rsp+64], rbx
    mov QWORD PTR [rsp+72], ID_EDIT_OUTPUT
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0
    call CreateWindowExA
    mov hEditOutput, rax
    
    ; Set font for output
    mov rcx, rax
    mov edx, WM_SETFONT
    mov r8, hFont
    mov r9d, 1
    call SendMessageA
    
    ; Create Run button
    xor ecx, ecx
    lea rdx, szButtonClass
    lea r8, szBtnRun
    mov r9d, WS_CHILD or WS_VISIBLE
    mov DWORD PTR [rsp+32], 10
    mov DWORD PTR [rsp+40], 420
    mov DWORD PTR [rsp+48], 120
    mov DWORD PTR [rsp+56], 30
    mov rbx, [rbp+16]
    mov [rsp+64], rbx
    mov QWORD PTR [rsp+72], ID_BTN_RUN
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0
    call CreateWindowExA
    mov hBtnRun, rax
    
    ; Create Stop button
    xor ecx, ecx
    lea rdx, szButtonClass
    lea r8, szBtnStop
    mov r9d, WS_CHILD or WS_VISIBLE
    mov DWORD PTR [rsp+32], 140
    mov DWORD PTR [rsp+40], 420
    mov DWORD PTR [rsp+48], 80
    mov DWORD PTR [rsp+56], 30
    mov rbx, [rbp+16]
    mov [rsp+64], rbx
    mov QWORD PTR [rsp+72], ID_BTN_STOP
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0
    call CreateWindowExA
    mov hBtnStop, rax
    
    ; Create status label
    xor ecx, ecx
    lea rdx, szStaticClass
    lea r8, szStatusReady
    mov r9d, WS_CHILD or WS_VISIBLE
    mov DWORD PTR [rsp+32], 10
    mov DWORD PTR [rsp+40], 720
    mov DWORD PTR [rsp+48], 1160
    mov DWORD PTR [rsp+56], 24
    mov rbx, [rbp+16]
    mov [rsp+64], rbx
    mov QWORD PTR [rsp+72], ID_STATUS
    mov rax, hInstance
    mov [rsp+80], rax
    mov QWORD PTR [rsp+88], 0
    call CreateWindowExA
    mov hStatus, rax
    
    xor eax, eax
    jmp @@done

; ---- WM_SIZE ----
@@OnSize:
    ; Get new client rect
    mov rcx, [rbp+16]
    lea rdx, rcClient
    call GetClientRect
    
    ; Calculate dimensions
    mov eax, rcClient.bottom
    sub eax, 100                    ; Leave space for buttons/status
    mov r12d, eax
    imul eax, 6
    mov ecx, 10
    xor edx, edx
    div ecx                         ; eax = 60% height for input
    mov r13d, eax
    
    ; Resize input edit
    mov rcx, hEditInput
    xor edx, edx                    ; HWND_TOP
    mov r8d, 10                     ; x
    mov r9d, 10                     ; y
    mov eax, rcClient.right
    sub eax, 20
    mov DWORD PTR [rsp+32], eax     ; width
    mov [rsp+40], r13d              ; height
    mov DWORD PTR [rsp+48], SWP_NOZORDER
    call SetWindowPos
    
    ; Position buttons below input
    mov eax, r13d
    add eax, 20
    mov r12d, eax                   ; Button Y
    
    mov rcx, hBtnRun
    xor edx, edx
    mov r8d, 10
    mov r9d, r12d
    mov DWORD PTR [rsp+32], 120
    mov DWORD PTR [rsp+40], 30
    mov DWORD PTR [rsp+48], SWP_NOZORDER
    call SetWindowPos
    
    mov rcx, hBtnStop
    xor edx, edx
    mov r8d, 140
    mov r9d, r12d
    mov DWORD PTR [rsp+32], 80
    mov DWORD PTR [rsp+40], 30
    mov DWORD PTR [rsp+48], SWP_NOZORDER
    call SetWindowPos
    
    ; Resize output edit
    mov eax, r12d
    add eax, 40
    mov r12d, eax                   ; Output Y
    
    mov eax, rcClient.bottom
    sub eax, r12d
    sub eax, 40                     ; Leave space for status
    mov r13d, eax                   ; Output height
    
    mov rcx, hEditOutput
    xor edx, edx
    mov r8d, 10
    mov r9d, r12d
    mov eax, rcClient.right
    sub eax, 20
    mov DWORD PTR [rsp+32], eax
    mov [rsp+40], r13d
    mov DWORD PTR [rsp+48], SWP_NOZORDER
    call SetWindowPos
    
    ; Position status
    mov eax, rcClient.bottom
    sub eax, 30
    
    mov rcx, hStatus
    xor edx, edx
    mov r8d, 10
    mov r9d, eax
    mov eax, rcClient.right
    sub eax, 20
    mov DWORD PTR [rsp+32], eax
    mov DWORD PTR [rsp+40], 24
    mov DWORD PTR [rsp+48], SWP_NOZORDER
    call SetWindowPos
    
    xor eax, eax
    jmp @@done

; ---- WM_COMMAND ----
@@OnCommand:
    mov rax, [rbp+32]               ; wParam
    movzx ecx, ax                   ; LOWORD = control ID
    shr eax, 16                     ; HIWORD = notification
    
    cmp cx, ID_BTN_RUN
    je @@OnRunClick
    cmp cx, ID_BTN_STOP
    je @@OnStopClick
    jmp @@Default
    
@@OnRunClick:
    cmp bStreaming, 1
    je @@done                       ; Already streaming
    
    ; Clear output
    mov rcx, hEditOutput
    lea rdx, szEmpty
    call SetWindowTextA
    
    ; Get prompt from input edit
    mov rcx, hEditInput
    mov edx, WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    inc eax
    mov r12d, eax                   ; Length + 1
    
    mov rcx, hEditInput
    mov edx, WM_GETTEXT
    mov r8d, PROMPT_BUF_SIZE
    lea r9, promptBuf
    call SendMessageA
    
    mov r13d, eax                   ; Actual length
    
    ; Start inference
    mov rcx, hContext
    lea rdx, promptBuf
    mov r8d, r13d
    call Titan_BeginStreamingInference
    test eax, eax
    jz @@done
    
    ; Start polling timer (16ms = ~60fps)
    mov rcx, [rbp+16]
    mov edx, TIMER_POLL_ID
    mov r8d, 16
    xor r9, r9
    call SetTimer
    
    mov bStreaming, 1
    mov dwTokenCount, 0
    
    ; Update status
    mov rcx, hStatus
    lea rdx, szStatusRunning
    call SetWindowTextA
    
    jmp @@done
    
@@OnStopClick:
    ; Stop polling
    mov rcx, [rbp+16]
    mov edx, TIMER_POLL_ID
    call KillTimer
    
    mov bStreaming, 0
    
    ; Update status
    mov rcx, hStatus
    lea rdx, szStatusReady
    call SetWindowTextA
    
    jmp @@done

; ---- WM_TIMER ----
@@OnTimer:
    cmp bStreaming, 0
    je @@done
    
    ; Consume tokens from ring
    mov rcx, hContext
    lea rdx, tokenBuf
    mov r8d, TOKEN_BUF_SIZE - 1
    call Titan_ConsumeToken
    
    test rax, rax
    jz @@CheckComplete
    
    mov r12, rax                    ; Bytes read
    
    ; Null-terminate
    lea rcx, tokenBuf
    add rcx, rax
    mov BYTE PTR [rcx], 0
    
    ; Append to output: EM_SETSEL -1, -1 then EM_REPLACESEL
    mov rcx, hEditOutput
    mov edx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    
    mov rcx, hEditOutput
    mov edx, EM_REPLACESEL
    xor r8d, r8d                    ; Don't undo
    lea r9, tokenBuf
    call SendMessageA
    
    ; Scroll to end
    mov rcx, hEditOutput
    mov edx, EM_SCROLLCARET
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Update token count
    inc dwTokenCount
    
    jmp @@done
    
@@CheckComplete:
    ; Check if context state is COMPLETE
    mov rax, hContext
    cmp DWORD PTR [rax+4], 3        ; CTX_STATE_COMPLETE
    jne @@done
    
    ; Stop timer
    mov rcx, [rbp+16]
    mov edx, TIMER_POLL_ID
    call KillTimer
    
    mov bStreaming, 0
    
    ; Update status to Done
    mov rcx, hStatus
    lea rdx, szStatusDone
    call SetWindowTextA
    
    jmp @@done

; ---- WM_DESTROY ----
@@OnDestroy:
    ; Kill timer if running
    mov rcx, [rbp+16]
    mov edx, TIMER_POLL_ID
    call KillTimer
    
    ; Delete font
    mov rcx, hFont
    call DeleteObject
    
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    jmp @@done
    
@@Default:
    mov rcx, [rbp+16]
    mov edx, [rbp+24]
    mov r8, [rbp+32]
    mov r9, [rbp+40]
    call DefWindowProcA
    jmp @@done
    
@@done:
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
WndProc ENDP

; Empty string for clearing
.DATA
szEmpty BYTE 0

END
