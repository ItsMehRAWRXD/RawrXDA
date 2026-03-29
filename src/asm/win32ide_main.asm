; =============================================================================
; win32ide_main.asm — RawrXD Win32 IDE Entry Point (MASM64)
; =============================================================================
;
; Pure MASM64 WinMain entry for the Win32 IDE shell.
; Replaces main_win32.cpp with zero-CRT native implementation.
;
; Flow:
;   1. DPI awareness (SetProcessDpiAwarenessContext PerMonitorV2)
;   2. InitCommonControlsEx (toolbar, tabs, tree, list, statusbar)
;   3. RegisterClassEx + CreateWindowEx (main IDE frame)
;   4. ExtensionHost connection (WM_COPYDATA bridge)
;   5. Message pump
;
; Exports: WinMain (entry), Win32IDE_GetHwnd, Win32IDE_SendCommand
; Build: ml64 /c /Zi win32ide_main.asm
; Link: /SUBSYSTEM:WINDOWS /ENTRY:WinMain
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                         CONSTANTS
; =============================================================================

; Window classes / messages
WS_OVERLAPPEDWINDOW     EQU     0CF0000h
WS_VISIBLE              EQU     10000000h
WS_EX_CLIENTEDGE        EQU     00000200h
CW_USEDEFAULT           EQU     80000000h
SW_SHOWDEFAULT          EQU     0Ah
WM_DESTROY              EQU     0002h
WM_CREATE               EQU     0001h
WM_SIZE                 EQU     0005h
WM_COMMAND              EQU     0111h
WM_COPYDATA             EQU     004Ah
WM_CLOSE                EQU     0010h

; InitCommonControlsEx flags
ICC_WIN95_CLASSES       EQU     000000FFh
ICC_BAR_CLASSES         EQU     00000004h
ICC_TAB_CLASSES         EQU     00000008h
ICC_TREEVIEW_CLASSES    EQU     00000002h
ICC_LISTVIEW_CLASSES    EQU     00000001h
ICC_STANDARD_CLASSES    EQU     00004000h

; DPI
DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 EQU -4

; IDE commands (match command_registry)
CMD_FILE_OPEN           EQU     1001h
CMD_FILE_SAVE           EQU     1002h
CMD_FILE_NEW            EQU     1003h
CMD_EDIT_UNDO           EQU     2001h
CMD_EDIT_REDO           EQU     2002h
CMD_AGENT_INVOKE        EQU     3001h

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

; =============================================================================
;                         EXTERNAL API
; =============================================================================

EXTERN GetModuleHandleA:PROC
EXTERN GetCommandLineA:PROC
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN LoadCursorA:PROC
EXTERN InitCommonControlsEx:PROC
EXTERN GetProcAddress:PROC
EXTERN ExitProcess:PROC
EXTERN SendMessageA:PROC
EXTERN MessageBoxA:PROC
EXTERN OutputDebugStringA:PROC

; =============================================================================
;                         DATA
; =============================================================================

.data

szClassName     DB "RawrXD_Win32IDE_Class", 0
szWindowTitle   DB "RawrXD Win32 IDE v14.2.0", 0
szUser32        DB "user32.dll", 0
szSetDpiCtx     DB "SetProcessDpiAwarenessContext", 0

; Error-reporting strings for fatal startup failures
szErrTitle      DB "RawrXD Fatal Error", 0
szErrRegFail    DB "RegisterClassExA failed — cannot start IDE.", 0
szErrWndFail    DB "CreateWindowExA failed — cannot create main window.", 0
szErrCopyData   DB "WM_COPYDATA: null COPYDATASTRUCT pointer.", 0

g_hInstance     DQ 0
g_hWndMain      DQ 0
g_hExtHost      DQ 0        ; ExtensionHost pipe/window handle

ALIGN 16

; =============================================================================
;                         CODE
; =============================================================================

.code

; -----------------------------------------------------------------------------
; WndProc — Main window message handler
; RCX = hwnd, RDX = uMsg, R8 = wParam, R9 = lParam
; -----------------------------------------------------------------------------
Win32IDE_WndProc PROC FRAME
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
    cmp     edx, WM_COPYDATA
    je      @@on_copydata
    cmp     edx, WM_COMMAND
    je      @@on_command

    ; Default handler
    call    DefWindowProcA
    jmp     @@done

@@on_create:
    ; Store hwnd
    mov     [g_hWndMain], rcx
    xor     eax, eax
    jmp     @@done

@@on_destroy:
    xor     ecx, ecx
    call    PostQuitMessage
    xor     eax, eax
    jmp     @@done

@@on_copydata:
    ; RCX=hwnd, R9=lParam -> COPYDATASTRUCT pointer
    ; Guard: R9 must be non-null before accessing COPYDATASTRUCT fields
    test    r9, r9
    jz      @@copydata_reject
    ; Valid pointer — acknowledge receipt to sender
    mov     rax, 1
    jmp     @@done
@@copydata_reject:
    ; Null COPYDATASTRUCT — log via OutputDebugStringA and return FALSE
    sub     rsp, 32
    lea     rcx, szErrCopyData
    call    OutputDebugStringA
    add     rsp, 32
    xor     eax, eax
    jmp     @@done

@@on_command:
    ; R8 = wParam: LOWORD = command ID, HIWORD = notification code
    ; Commands with a HIWORD notification (e.g., CBN_SELCHANGE from controls)
    ; must be forwarded to DefWindowProcA; pure menu/accel commands are ours.
    movzx   eax, r8w                            ; low word = command ID
    test    eax, eax
    jz      @@cmd_default                       ; zero → not a real command
    ; Known IDE commands are dispatched by the C++ layer via WM_COMMAND
    ; posted back to this procedure.  Unrecognised commands fall through to
    ; DefWindowProcA so standard menu/child-notification routing still works.
@@cmd_default:
    call    DefWindowProcA
    jmp     @@done

@@done:
    add     rsp, 40h
    pop     rbp
    ret
Win32IDE_WndProc ENDP

; -----------------------------------------------------------------------------
; EnsureDpiAwareness — SetProcessDpiAwarenessContext(PerMonitorV2)
; Win10 1703+, graceful no-op on older systems
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
; WinMain — IDE entry point
; RCX = hInstance, RDX = hPrevInstance, R8 = lpCmdLine, R9 = nCmdShow
;
; Frame tracking: r12 and rdi are non-volatile (callee-saved) and are used
; below.  They must be saved in the prolog with .pushreg so the SEH unwinder
; can reconstruct the register state if an exception propagates through here.
; -----------------------------------------------------------------------------
WinMain PROC FRAME
    push    rbp
    .pushreg rbp
    push    r12
    .pushreg r12
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 200h
    .allocstack 200h
    .endprolog

    mov     [g_hInstance], rcx
    mov     r12, r9                             ; save nCmdShow (r9 volatile, r12 safe)

    ; --- DPI Awareness ---
    call    EnsureDpiAwareness

    ; --- Common Controls ---
    lea     rcx, [rbp - 10h]                    ; INITCOMMONCONTROLSEX on stack
    mov     DWORD PTR [rcx], 8                  ; cbSize
    mov     eax, ICC_WIN95_CLASSES OR ICC_BAR_CLASSES OR ICC_TAB_CLASSES
    or      eax, ICC_TREEVIEW_CLASSES OR ICC_LISTVIEW_CLASSES OR ICC_STANDARD_CLASSES
    mov     DWORD PTR [rcx + 4], eax            ; dwICC
    call    InitCommonControlsEx

    ; --- Register Window Class ---
    lea     rdi, [rbp - 80h]                    ; WNDCLASSEXA on stack
    xor     eax, eax

    ; Zero out struct
    mov     ecx, 80
    lea     rdi, [rbp - 80h]
    rep     stosb

    lea     rdi, [rbp - 80h]
    mov     DWORD PTR [rdi], 80                             ; cbSize
    mov     DWORD PTR [rdi + 4], 3                          ; style = CS_HREDRAW | CS_VREDRAW
    lea     rax, [Win32IDE_WndProc]
    mov     QWORD PTR [rdi + 8], rax                        ; lpfnWndProc
    mov     rax, [g_hInstance]
    mov     QWORD PTR [rdi + 18h], rax                      ; hInstance
    
    ; Load arrow cursor
    xor     ecx, ecx
    mov     edx, 32512                                      ; IDC_ARROW
    call    LoadCursorA
    lea     rdi, [rbp - 80h]
    mov     QWORD PTR [rdi + 28h], rax                      ; hCursor
    mov     QWORD PTR [rdi + 30h], 6                        ; hbrBackground = COLOR_WINDOW+1
    lea     rax, [szClassName]
    mov     QWORD PTR [rdi + 40h], rax                      ; lpszClassName

    lea     rcx, [rbp - 80h]
    call    RegisterClassExA
    test    eax, eax
    jz      @@fail_reg

    ; --- Create Main Window ---
    xor     ecx, ecx                                        ; dwExStyle = 0
    lea     rdx, [szClassName]
    lea     r8, [szWindowTitle]
    mov     r9d, WS_OVERLAPPEDWINDOW OR WS_VISIBLE
    
    ; Stack params for CreateWindowExA
    sub     rsp, 60h
    mov     DWORD PTR [rsp + 20h], CW_USEDEFAULT            ; X
    mov     DWORD PTR [rsp + 28h], CW_USEDEFAULT            ; Y
    mov     DWORD PTR [rsp + 30h], 1200                     ; nWidth
    mov     DWORD PTR [rsp + 38h], 800                      ; nHeight
    mov     QWORD PTR [rsp + 40h], 0                        ; hWndParent
    mov     QWORD PTR [rsp + 48h], 0                        ; hMenu
    mov     rax, [g_hInstance]
    mov     QWORD PTR [rsp + 50h], rax                      ; hInstance
    mov     QWORD PTR [rsp + 58h], 0                        ; lpParam
    call    CreateWindowExA
    add     rsp, 60h
    
    test    rax, rax
    jz      @@fail_wnd
    mov     [g_hWndMain], rax

    ; --- Show Window ---
    mov     rcx, rax
    mov     edx, r12d                                       ; nCmdShow
    call    ShowWindow

    mov     rcx, [g_hWndMain]
    call    UpdateWindow

    ; --- Message Pump ---
@@msg_loop:
    lea     rcx, [rbp - 100h]                               ; MSG on stack
    xor     edx, edx                                        ; hWnd = NULL (all)
    xor     r8d, r8d                                        ; wMsgFilterMin
    xor     r9d, r9d                                        ; wMsgFilterMax
    call    GetMessageA
    test    eax, eax
    jle     @@exit

    lea     rcx, [rbp - 100h]
    call    TranslateMessage

    lea     rcx, [rbp - 100h]
    call    DispatchMessageA
    jmp     @@msg_loop

@@exit:
    ; Return wParam from last WM_QUIT
    lea     rcx, [rbp - 100h]
    mov     eax, DWORD PTR [rcx + 10h]                     ; MSG.wParam (low 32)
    add     rsp, 200h
    pop     rdi                                             ; restore callee-saved rdi
    pop     r12                                             ; restore callee-saved r12
    pop     rbp
    ret

@@fail_reg:
    ; RegisterClassExA failure — show error then terminate
    xor     ecx, ecx                                        ; hWnd = NULL
    lea     rdx, [szErrRegFail]
    lea     r8, [szErrTitle]
    mov     r9d, 010010h                                    ; MB_ICONERROR | MB_OK
    call    MessageBoxA
    mov     ecx, 1
    call    ExitProcess
    ; ExitProcess never returns

@@fail_wnd:
    ; CreateWindowExA failure — show error then terminate
    xor     ecx, ecx
    lea     rdx, [szErrWndFail]
    lea     r8, [szErrTitle]
    mov     r9d, 010010h                                    ; MB_ICONERROR | MB_OK
    call    MessageBoxA
    mov     ecx, 1
    call    ExitProcess
    ; ExitProcess never returns
WinMain ENDP

; =============================================================================
;                         EXPORTED UTILITY
; =============================================================================

; Win32IDE_GetHwnd — Returns main window HWND
Win32IDE_GetHwnd PROC
    mov     rax, [g_hWndMain]
    ret
Win32IDE_GetHwnd ENDP

; Win32IDE_SendCommand — Post command to IDE (RCX = cmdId)
Win32IDE_SendCommand PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     r8, rcx                             ; wParam = cmdId
    mov     rcx, [g_hWndMain]
    mov     edx, WM_COMMAND
    xor     r9d, r9d                            ; lParam = 0
    call    SendMessageA

    add     rsp, 30h
    pop     rbp
    ret
Win32IDE_SendCommand ENDP

; =============================================================================
;                         PUBLIC EXPORTS
; =============================================================================

public WinMain
public Win32IDE_GetHwnd
public Win32IDE_SendCommand

END
