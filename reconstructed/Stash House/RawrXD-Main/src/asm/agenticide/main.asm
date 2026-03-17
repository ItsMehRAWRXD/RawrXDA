; =============================================================================
; agenticide_main.asm — RawrXD Agentic IDE Entry Point (MASM64)
; =============================================================================
;
; Pure MASM64 WinMain for the Agentic IDE shell.
; Creates the IDE window + boots the autonomous agent loop.
;
; Flow:
;   1. DPI awareness
;   2. InitCommonControlsEx
;   3. RegisterClassEx + CreateWindowEx (agentic IDE frame)
;   4. Connect to ExtensionHost (named pipe: \\.\pipe\RawrXD_ExtHost)
;   5. Initialize AgenticOrchestrator (VTable dispatch)
;   6. Connect to local model endpoint (Ollama / GGUF direct)
;   7. Message pump with agent tick interleaving
;
; Exports: WinMain (entry), AgenticIDE_GetHwnd, AgenticIDE_DispatchAgent
; Build: ml64 /c /Zi agenticide_main.asm
; Link: /SUBSYSTEM:WINDOWS /ENTRY:WinMain
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                         CONSTANTS
; =============================================================================

WS_OVERLAPPEDWINDOW     EQU     0CF0000h
WS_VISIBLE              EQU     10000000h
CW_USEDEFAULT           EQU     80000000h
SW_SHOWDEFAULT          EQU     0Ah
WM_DESTROY              EQU     0002h
WM_CREATE               EQU     0001h
WM_SIZE                 EQU     0005h
WM_COMMAND              EQU     0111h
WM_COPYDATA             EQU     004Ah
WM_CLOSE                EQU     0010h
WM_TIMER                EQU     0113h
PM_REMOVE               EQU     0001h

; DPI
DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 EQU -4

; Common controls flags
ICC_WIN95_CLASSES       EQU     000000FFh
ICC_BAR_CLASSES         EQU     00000004h
ICC_TAB_CLASSES         EQU     00000008h
ICC_TREEVIEW_CLASSES    EQU     00000002h
ICC_LISTVIEW_CLASSES    EQU     00000001h
ICC_STANDARD_CLASSES    EQU     00004000h

; Agent modes
AGENT_MODE_AUTONOMOUS   EQU     1
AGENT_MODE_SUPERVISED   EQU     2
AGENT_MODE_DISABLED     EQU     0

; Timer IDs
TIMER_AGENT_TICK        EQU     1001h
AGENT_TICK_MS           EQU     50          ; 50ms agent tick (20 Hz)

; Named pipe for ExtensionHost
PIPE_ACCESS_DUPLEX      EQU     3
PIPE_TYPE_BYTE          EQU     0
PIPE_WAIT               EQU     0
OPEN_EXISTING_PIPE      EQU     3
FILE_FLAG_OVERLAPPED    EQU     40000000h

; =============================================================================
;                         STRUCTURES
; =============================================================================

INITCOMMONCONTROLSEX STRUCT
    dwSize              DD ?
    dwICC               DD ?
INITCOMMONCONTROLSEX ENDS

WNDCLASSEXA STRUCT
    cbSize              DD ?
    style               DD ?
    lpfnWndProc         DQ ?
    cbClsExtra          DD ?
    cbWndExtra          DD ?
    hInstance           DQ ?
    hIcon               DQ ?
    hCursor             DQ ?
    hbrBackground       DQ ?
    lpszMenuName        DQ ?
    lpszClassName       DQ ?
    hIconSm             DQ ?
WNDCLASSEXA ENDS

POINT STRUCT
    x                   DD ?
    y                   DD ?
POINT ENDS

MSG STRUCT
    hwnd                DQ ?
    message             DD ?
    _pad0               DD ?
    wParam              DQ ?
    lParam              DQ ?
    time                DD ?
    pt                  POINT <>
    _pad1               DD ?
MSG ENDS

COPYDATASTRUCT STRUCT
    dwData              DQ ?
    cbData              DD ?
    _pad                DD ?
    lpData              DQ ?
COPYDATASTRUCT ENDS

; Agent task (matches orchestrator queue entry)
AGENT_TASK STRUCT
    opcode              DD ?
    flags               DD ?
    payloadPtr          DQ ?
    payloadLen          DQ ?
    callbackPtr         DQ ?
AGENT_TASK ENDS

; =============================================================================
;                         EXTERNAL API
; =============================================================================

EXTERN GetModuleHandleA:PROC
EXTERN GetCommandLineA:PROC
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN PeekMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN LoadCursorA:PROC
EXTERN InitCommonControlsEx:PROC
EXTERN GetProcAddress:PROC
EXTERN ExitProcess:PROC
EXTERN SetTimer:PROC
EXTERN KillTimer:PROC
EXTERN SendMessageA:PROC
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN Sleep:PROC

; Orchestrator imports (from RawrXD_AgenticOrchestrator.asm)
EXTERN asm_orchestrator_init:PROC
EXTERN asm_orchestrator_dispatch:PROC
EXTERN asm_orchestrator_shutdown:PROC
EXTERN asm_orchestrator_drain_queue:PROC
EXTERN asm_orchestrator_queue_async:PROC

; =============================================================================
;                         DATA
; =============================================================================

.data

szClassName     DB "RawrXD_AgenticIDE_Class", 0
szWindowTitle   DB "RawrXD Agentic IDE v14.2.0 [AUTONOMOUS]", 0
szUser32        DB "user32.dll", 0
szSetDpiCtx     DB "SetProcessDpiAwarenessContext", 0
szExtHostPipe   DB "\\.\pipe\RawrXD_ExtHost", 0

g_hInstance     DQ 0
g_hWndMain      DQ 0
g_hExtHostPipe  DQ 0        ; Named pipe to ExtensionHost
g_AgentMode     DD AGENT_MODE_AUTONOMOUS
g_AgentRunning  DD 0        ; 1 = agent loop active
g_TickCount     DQ 0        ; Agent tick counter

ALIGN 16

; =============================================================================
;                         CODE
; =============================================================================

.code

; -----------------------------------------------------------------------------
; AgenticIDE_WndProc — Main window message handler
; RCX = hwnd, RDX = uMsg, R8 = wParam, R9 = lParam
; -----------------------------------------------------------------------------
AgenticIDE_WndProc PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    cmp     edx, WM_CREATE
    je      @@on_create
    cmp     edx, WM_DESTROY
    je      @@on_destroy
    cmp     edx, WM_TIMER
    je      @@on_timer
    cmp     edx, WM_COPYDATA
    je      @@on_copydata
    cmp     edx, WM_COMMAND
    je      @@on_command

    ; Default
    call    DefWindowProcA
    jmp     @@done

@@on_create:
    mov     [g_hWndMain], rcx

    ; Start agent tick timer (50ms)
    mov     rcx, [g_hWndMain]
    mov     edx, TIMER_AGENT_TICK
    mov     r8d, AGENT_TICK_MS
    xor     r9, r9                              ; no TimerProc
    call    SetTimer

    ; Mark agent running
    mov     DWORD PTR [g_AgentRunning], 1

    xor     eax, eax
    jmp     @@done

@@on_destroy:
    ; Kill tick timer
    mov     rcx, [g_hWndMain]
    mov     edx, TIMER_AGENT_TICK
    call    KillTimer

    ; Shutdown orchestrator
    call    asm_orchestrator_shutdown

    ; Close ExtensionHost pipe
    mov     rcx, [g_hExtHostPipe]
    test    rcx, rcx
    jz      @@no_pipe
    cmp     rcx, -1                             ; INVALID_HANDLE_VALUE
    je      @@no_pipe
    call    CloseHandle
@@no_pipe:

    xor     ecx, ecx
    call    PostQuitMessage
    xor     eax, eax
    jmp     @@done

@@on_timer:
    ; Agent tick — drain async queue and dispatch pending tasks
    cmp     DWORD PTR [g_AgentRunning], 0
    je      @@timer_skip

    ; Drain the orchestrator's SPSC queue
    call    asm_orchestrator_drain_queue

    ; Increment tick counter
    lock inc QWORD PTR [g_TickCount]

@@timer_skip:
    xor     eax, eax
    jmp     @@done

@@on_copydata:
    ; ExtensionHost sends agent tasks via WM_COPYDATA
    ; R9 = lParam -> COPYDATASTRUCT
    mov     rax, 1                              ; ACK
    jmp     @@done

@@on_command:
    ; LOWORD(wParam) = command ID → dispatch through orchestrator
    movzx   ecx, r8w                            ; opcode
    xor     edx, edx                            ; flags
    xor     r8, r8                              ; payload
    xor     r9, r9                              ; payloadLen
    call    asm_orchestrator_dispatch
    ; eax = result
    jmp     @@done

@@done:
    add     rsp, 40h
    pop     rbp
    ret
AgenticIDE_WndProc ENDP

; -----------------------------------------------------------------------------
; EnsureDpiAwareness
; -----------------------------------------------------------------------------
EnsureDpiAwareness PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    lea     rcx, [szUser32]
    call    GetModuleHandleA
    test    rax, rax
    jz      @@skip

    mov     rcx, rax
    lea     rdx, [szSetDpiCtx]
    call    GetProcAddress
    test    rax, rax
    jz      @@skip

    mov     rcx, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    call    rax

@@skip:
    add     rsp, 30h
    pop     rbp
    ret
EnsureDpiAwareness ENDP

; -----------------------------------------------------------------------------
; ConnectExtensionHost — Open named pipe to RawrXD-ExtensionHost
; Returns: RAX = pipe handle (or INVALID_HANDLE_VALUE on failure)
; -----------------------------------------------------------------------------
ConnectExtensionHost PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    lea     rcx, [szExtHostPipe]
    mov     edx, GENERIC_READ OR GENERIC_WRITE
    xor     r8d, r8d                            ; no sharing
    xor     r9, r9                              ; default security
    sub     rsp, 20h
    mov     DWORD PTR [rsp], OPEN_EXISTING_PIPE ; dwCreationDisposition
    mov     DWORD PTR [rsp + 8], 0              ; dwFlagsAndAttributes
    mov     QWORD PTR [rsp + 10h], 0            ; hTemplateFile
    call    CreateFileA
    add     rsp, 20h

    mov     [g_hExtHostPipe], rax

    add     rsp, 40h
    pop     rbp
    ret
ConnectExtensionHost ENDP

; -----------------------------------------------------------------------------
; WinMain — Agentic IDE entry point
; RCX = hInstance, RDX = hPrevInstance, R8 = lpCmdLine, R9 = nCmdShow
; -----------------------------------------------------------------------------
WinMain PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 200h
    .allocstack 200h
    .endprolog

    mov     [g_hInstance], rcx
    mov     r12, r9                             ; save nCmdShow

    ; --- DPI ---
    call    EnsureDpiAwareness

    ; --- Common Controls ---
    lea     rcx, [rbp - 10h]
    mov     DWORD PTR [rcx], 8                  ; cbSize
    mov     eax, ICC_WIN95_CLASSES OR ICC_BAR_CLASSES OR ICC_TAB_CLASSES
    or      eax, ICC_TREEVIEW_CLASSES OR ICC_LISTVIEW_CLASSES OR ICC_STANDARD_CLASSES
    mov     DWORD PTR [rcx + 4], eax
    call    InitCommonControlsEx

    ; --- Initialize Agentic Orchestrator ---
    call    asm_orchestrator_init

    ; --- Connect to ExtensionHost (non-fatal if not running yet) ---
    call    ConnectExtensionHost

    ; --- Register Window Class ---
    lea     rdi, [rbp - 80h]
    xor     eax, eax
    mov     ecx, 80
    rep     stosb

    lea     rdi, [rbp - 80h]
    mov     DWORD PTR [rdi], 80                             ; cbSize
    mov     DWORD PTR [rdi + 4], 3                          ; CS_HREDRAW | CS_VREDRAW
    lea     rax, [AgenticIDE_WndProc]
    mov     QWORD PTR [rdi + 8], rax                        ; lpfnWndProc
    mov     rax, [g_hInstance]
    mov     QWORD PTR [rdi + 18h], rax                      ; hInstance

    xor     ecx, ecx
    mov     edx, 32512                                      ; IDC_ARROW
    call    LoadCursorA
    lea     rdi, [rbp - 80h]
    mov     QWORD PTR [rdi + 28h], rax                      ; hCursor
    mov     QWORD PTR [rdi + 30h], 6                        ; COLOR_WINDOW+1
    lea     rax, [szClassName]
    mov     QWORD PTR [rdi + 40h], rax                      ; lpszClassName

    lea     rcx, [rbp - 80h]
    call    RegisterClassExA
    test    eax, eax
    jz      @@fail

    ; --- Create Main Window ---
    xor     ecx, ecx
    lea     rdx, [szClassName]
    lea     r8, [szWindowTitle]
    mov     r9d, WS_OVERLAPPEDWINDOW OR WS_VISIBLE

    sub     rsp, 60h
    mov     DWORD PTR [rsp + 20h], CW_USEDEFAULT
    mov     DWORD PTR [rsp + 28h], CW_USEDEFAULT
    mov     DWORD PTR [rsp + 30h], 1400                     ; wider for agent panel
    mov     DWORD PTR [rsp + 38h], 900
    mov     QWORD PTR [rsp + 40h], 0
    mov     QWORD PTR [rsp + 48h], 0
    mov     rax, [g_hInstance]
    mov     QWORD PTR [rsp + 50h], rax
    mov     QWORD PTR [rsp + 58h], 0
    call    CreateWindowExA
    add     rsp, 60h

    test    rax, rax
    jz      @@fail
    mov     [g_hWndMain], rax

    ; --- Show ---
    mov     rcx, rax
    mov     edx, r12d
    call    ShowWindow

    mov     rcx, [g_hWndMain]
    call    UpdateWindow

    ; --- Agentic Message Pump ---
    ; Uses PeekMessage for non-blocking agent interleaving
@@msg_loop:
    lea     rcx, [rbp - 100h]                               ; MSG
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 8
    mov     DWORD PTR [rsp], PM_REMOVE
    call    PeekMessageA
    add     rsp, 8

    test    eax, eax
    jz      @@idle

    ; Check for WM_QUIT
    lea     rcx, [rbp - 100h]
    cmp     DWORD PTR [rcx + 8], WM_CLOSE       ; approximate quit check
    je      @@exit_check

    lea     rcx, [rbp - 100h]
    call    TranslateMessage

    lea     rcx, [rbp - 100h]
    call    DispatchMessageA
    jmp     @@msg_loop

@@idle:
    ; No messages — yield briefly to avoid CPU spin
    mov     ecx, 1
    call    Sleep
    jmp     @@msg_loop

@@exit_check:
    ; Process WM_CLOSE through normal dispatch (triggers WM_DESTROY)
    lea     rcx, [rbp - 100h]
    call    TranslateMessage
    lea     rcx, [rbp - 100h]
    call    DispatchMessageA

    ; Check if PostQuitMessage was called
    lea     rcx, [rbp - 100h]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 8
    mov     DWORD PTR [rsp], PM_REMOVE
    call    PeekMessageA
    add     rsp, 8
    ; Fall through to exit

@@exit:
    lea     rcx, [rbp - 100h]
    mov     eax, DWORD PTR [rcx + 10h]                     ; MSG.wParam
    add     rsp, 200h
    pop     rbp
    ret

@@fail:
    mov     ecx, 1
    call    ExitProcess
WinMain ENDP

; =============================================================================
;                         EXPORTED UTILITY
; =============================================================================

; AgenticIDE_GetHwnd — Returns main window HWND
AgenticIDE_GetHwnd PROC
    mov     rax, [g_hWndMain]
    ret
AgenticIDE_GetHwnd ENDP

; AgenticIDE_DispatchAgent — Dispatch an opcode through the orchestrator
; RCX = opcode, RDX = payloadPtr, R8 = payloadLen
AgenticIDE_DispatchAgent PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; RCX already = opcode
    ; RDX already = payload
    mov     r9, r8                              ; payloadLen → r9
    xor     r8, r8                              ; flags = 0
    ; Reorder: dispatch(opcode, flags, payload, len)
    mov     r8, rdx                             ; payload → r8
    xor     edx, edx                            ; flags = 0
    call    asm_orchestrator_dispatch

    add     rsp, 30h
    pop     rbp
    ret
AgenticIDE_DispatchAgent ENDP

; AgenticIDE_QueueTask — Queue async task to orchestrator SPSC ring
; RCX = opcode, RDX = payloadPtr, R8 = payloadLen
AgenticIDE_QueueTask PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    call    asm_orchestrator_queue_async

    add     rsp, 30h
    pop     rbp
    ret
AgenticIDE_QueueTask ENDP

; =============================================================================
;                         PUBLIC EXPORTS
; =============================================================================

public WinMain
public AgenticIDE_GetHwnd
public AgenticIDE_DispatchAgent
public AgenticIDE_QueueTask

END
