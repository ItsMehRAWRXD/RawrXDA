; ============================================================================
; gui_framework.asm - Pure MASM Win32 GUI shell for autonomous factory control
; Creates main window with Build button, input box, and queue list view
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
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
EXTERN LoadLibraryA:PROC
EXTERN SendMessageA:PROC
EXTERN GetWindowTextA:PROC
EXTERN SetWindowTextA:PROC
EXTERN lstrcpyA:PROC

EXTERN Queue_AddTask:PROC
EXTERN RefreshBuildQueue:PROC
EXTERN AgentMonitor_Init:PROC
EXTERN AgentMonitor_Populate:PROC

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
NULL                equ 0
SW_SHOWDEFAULT      equ 10
CS_HREDRAW          equ 0002h
CS_VREDRAW          equ 0001h
WS_OVERLAPPEDWINDOW equ 00CF0000h
WS_VISIBLE          equ 10000000h
WS_CHILD            equ 40000000h
WS_BORDER           equ 00800000h
WS_EX_CLIENTEDGE    equ 00000200h
BS_PUSHBUTTON       equ 00000000h
ES_LEFT             equ 0000h
ES_MULTILINE        equ 0004h
ES_AUTOVSCROLL      equ 0040h
ES_AUTOHSCROLL      equ 0080h
LVS_REPORT          equ 0001h
LVS_SHOWSELALWAYS   equ 0008h
LVS_EX_FULLROWSELECT equ 20h
WM_DESTROY          equ 0002h
WM_COMMAND          equ 0111h
WM_CREATE           equ 0001h
WM_SIZE             equ 0005h
IDI_APPLICATION     equ 7F00h
IDC_ARROW           equ 7F00h
CW_USEDEFAULT       equ 80000000h
COLOR_WINDOW        equ 5
LVM_SETEXTENDEDLISTVIEWSTYLE equ 1036h
LVM_INSERTCOLUMN    equ 101Bh
LVM_INSERTITEM      equ 1001h
LVM_DELETEALLITEMS  equ 1009h
EM_REPLACESEL       equ 00C2h

; ----------------------------------------------------------------------------
; STRUCTURES
; ----------------------------------------------------------------------------
WNDCLASSEX STRUCT
    cbSize           dd ?
    style            dd ?
    lpfnWndProc      dq ?
    cbClsExtra       dd ?
    cbWndExtra       dd ?
    hInstance        dq ?
    hIcon            dq ?
    hCursor          dq ?
    hbrBackground    dq ?
    lpszMenuName     dq ?
    lpszClassName    dq ?
    hIconSm          dq ?
WNDCLASSEX ENDS

MSG STRUCT
    hwnd             dq ?
    message          dd ?
    wParam           dq ?
    lParam           dq ?
    time             dd ?
    pt_x             dd ?
    pt_y             dd ?
MSG ENDS

LV_COLUMN STRUCT
    mask             dd ?
    fmt              dd ?
    cx               dd ?
    pszText          dq ?
    cchTextMax       dd ?
    iSubItem         dd ?
    iImage           dd ?
    iOrder           dd ?
LV_COLUMN ENDS

LV_ITEM STRUCT
    mask             dd ?
    iItem            dd ?
    iSubItem         dd ?
    state            dd ?
    stateMask        dd ?
    pszText          dq ?
    cchTextMax       dd ?
    iImage           dd ?
    lParam           dq ?
    iIndent          dd ?
    iGroupId         dd ?
    cColumns         dd ?
    puColumns        dq ?
    piColFmt         dq ?
    iGroup           dd ?
LV_ITEM ENDS

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC WinMain
PUBLIC WndProc
PUBLIC hInstance
PUBLIC hMainWnd
PUBLIC hEdit
PUBLIC hListView

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
hInstance       dq 0
hMainWnd        dq 0
hEdit           dq 0
hListView       dq 0
hBuildButton    dq 0

szClassName     db 'AutonomousFactory',0
szWindowTitle   db 'RawrXD Autonomous Factory v2.0 - Pure MASM',0
szButtonClass   db 'BUTTON',0
szEditClass     db 'EDIT',0
szListViewClass db 'SysListView32',0
szBuildButton   db 'Build Now',0
szBuildSpecBuf  db 1024 dup(0)

col1Text        db 'Agent',0
col2Text        db 'Status',0
col3Text        db 'Progress',0

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

WinMain PROC
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG

    ; Load common controls to ensure list view available
    lea rcx, szListViewClass
    call LoadLibraryA

    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea rax, WndProc
    mov wc.lpfnWndProc, rax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov rax, hInstance
    mov wc.hInstance, rax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, OFFSET szClassName

    invoke LoadIconA, NULL, IDI_APPLICATION
    mov wc.hIcon, rax
    mov wc.hIconSm, rax
    invoke LoadCursorA, NULL, IDC_ARROW
    mov wc.hCursor, rax

    invoke RegisterClassExA, ADDR wc

    ; Create main window
    invoke CreateWindowExA, 0, ADDR szClassName, ADDR szWindowTitle,
        WS_OVERLAPPEDWINDOW or WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 800, NULL, NULL, hInstance, NULL
    mov hMainWnd, rax

    ; Create Build button
    invoke CreateWindowExA, 0, ADDR szButtonClass, ADDR szBuildButton,
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON,
        20, 20, 200, 30, hMainWnd, 101, hInstance, NULL
    mov hBuildButton, rax

    ; Create edit box
    invoke CreateWindowExA, WS_EX_CLIENTEDGE, ADDR szEditClass, NULL,
        WS_CHILD or WS_VISIBLE or ES_LEFT or WS_BORDER,
        20, 60, 600, 25, hMainWnd, 102, hInstance, NULL
    mov hEdit, rax

    ; Create list view
    invoke CreateWindowExA, 0, ADDR szListViewClass, NULL,
        WS_CHILD or WS_VISIBLE or LVS_REPORT or LVS_SHOWSELALWAYS,
        20, 100, 1160, 650, hMainWnd, 103, hInstance, NULL
    mov hListView, rax

    ; Configure list view columns
    LOCAL col:LV_COLUMN
    mov col.mask, 1 or 2 or 4           ; LVCF_FMT | LVCF_WIDTH | LVCF_TEXT
    mov col.fmt, 0
    mov col.cx, 200
    mov col.pszText, OFFSET col1Text
    mov col.cchTextMax, LENGTHOF col1Text
    mov col.iSubItem, 0
    invoke SendMessageA, hListView, LVM_INSERTCOLUMN, 0, ADDR col

    mov col.pszText, OFFSET col2Text
    mov col.iSubItem, 1
    invoke SendMessageA, hListView, LVM_INSERTCOLUMN, 1, ADDR col

    mov col.pszText, OFFSET col3Text
    mov col.iSubItem, 2
    invoke SendMessageA, hListView, LVM_INSERTCOLUMN, 2, ADDR col

    invoke ShowWindow, hMainWnd, SW_SHOWDEFAULT
    invoke UpdateWindow, hMainWnd

@msgLoop:
    invoke GetMessageA, ADDR msg, NULL, 0, 0
    cmp eax, 0
    je @exitLoop
    invoke TranslateMessage, ADDR msg
    invoke DispatchMessageA, ADDR msg
    jmp @msgLoop

@exitLoop:
    mov rax, msg.wParam
    ret
WinMain ENDP

WndProc PROC hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    LOCAL lvi:LV_ITEM

    cmp edx, WM_CREATE
    je @wmCreate
    cmp edx, WM_COMMAND
    je @wmCommand
    cmp edx, WM_DESTROY
    je @wmDestroy

    ; Default processing
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret

@wmCreate:
    ; Initialize agent monitor pane
    call AgentMonitor_Init
    ret

@wmCommand:
    mov eax, wParam
    and eax, 0FFFFh
    cmp eax, 101                     ; Build button ID
    jne @notBuildButton

    ; Read spec string
    invoke GetWindowTextA, hEdit, ADDR szBuildSpecBuf, LENGTHOF szBuildSpecBuf
    lea rcx, szBuildSpecBuf
    call Queue_AddTask
    invoke SetWindowTextA, hEdit, NULL
    call RefreshBuildQueue
    call AgentMonitor_Populate
    ret

@notBuildButton:
    ret

@wmDestroy:
    invoke PostQuitMessage, 0
    ret
WndProc ENDP

END
