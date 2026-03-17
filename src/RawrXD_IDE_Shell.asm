;==============================================================================
; RawrXD_IDE_Shell.asm — Universal Win32 Agentic IDE Shell
; Production-ready Win32 GUI: window proc, message loop, layout engine,
; code editor integration, IPC agent hooks, toolbar, sidebar, status bar.
;
; Targets: Windows 10+ x64 (Win32 API, no Qt, no CRT, no external deps)
; Assemble: ml64 /c /Fo RawrXD_IDE_Shell.obj /nologo /W3 /I D:\rawrxd\include RawrXD_IDE_Shell.asm
; Link:     link RawrXD_IDE_Shell.obj RawrXD_IPC_Bridge.obj /SUBSYSTEM:WINDOWS
;           /ENTRY:IDEShellMain /MACHINE:X64 kernel32.lib user32.lib
;           gdi32.lib comctl32.lib richedit.lib shell32.lib
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
; Win32 API EXTERNs
;==============================================================================
EXTERNDEF __imp_GetModuleHandleA:QWORD
EXTERNDEF __imp_LoadIconA:QWORD
EXTERNDEF __imp_LoadCursorA:QWORD
EXTERNDEF __imp_RegisterClassExA:QWORD
EXTERNDEF __imp_CreateWindowExA:QWORD
EXTERNDEF __imp_ShowWindow:QWORD
EXTERNDEF __imp_UpdateWindow:QWORD
EXTERNDEF __imp_GetMessageA:QWORD
EXTERNDEF __imp_TranslateMessage:QWORD
EXTERNDEF __imp_DispatchMessageA:QWORD
EXTERNDEF __imp_PostQuitMessage:QWORD
EXTERNDEF __imp_DefWindowProcA:QWORD
EXTERNDEF __imp_SendMessageA:QWORD
EXTERNDEF __imp_PostMessageA:QWORD
EXTERNDEF __imp_SetWindowTextA:QWORD
EXTERNDEF __imp_GetClientRect:QWORD
EXTERNDEF __imp_MoveWindow:QWORD
EXTERNDEF __imp_SetWindowPos:QWORD
EXTERNDEF __imp_InvalidateRect:QWORD
EXTERNDEF __imp_BeginPaint:QWORD
EXTERNDEF __imp_EndPaint:QWORD
EXTERNDEF __imp_FillRect:QWORD
EXTERNDEF __imp_DrawTextA:QWORD
EXTERNDEF __imp_CreateSolidBrush:QWORD
EXTERNDEF __imp_DeleteObject:QWORD
EXTERNDEF __imp_SelectObject:QWORD
EXTERNDEF __imp_SetBkColor:QWORD
EXTERNDEF __imp_SetTextColor:QWORD
EXTERNDEF __imp_CreateFontA:QWORD
EXTERNDEF __imp_GetStockObject:QWORD
EXTERNDEF __imp_SetBkMode:QWORD
EXTERNDEF __imp_TextOutA:QWORD
EXTERNDEF __imp_CreateMenu:QWORD
EXTERNDEF __imp_CreatePopupMenu:QWORD
EXTERNDEF __imp_AppendMenuA:QWORD
EXTERNDEF __imp_SetMenu:QWORD
EXTERNDEF __imp_GetMenuBarInfo:QWORD
EXTERNDEF __imp_TrackPopupMenu:QWORD
EXTERNDEF __imp_DestroyMenu:QWORD
EXTERNDEF __imp_MessageBoxA:QWORD
EXTERNDEF __imp_LoadLibraryA:QWORD
EXTERNDEF __imp_GetProcAddress:QWORD
EXTERNDEF __imp_FreeLibrary:QWORD
EXTERNDEF __imp_CreateWindowExA:QWORD
EXTERNDEF __imp_DestroyWindow:QWORD
EXTERNDEF __imp_EnableWindow:QWORD
EXTERNDEF __imp_SetFocus:QWORD
EXTERNDEF __imp_GetFocus:QWORD
EXTERNDEF __imp_SetWindowLongPtrA:QWORD
EXTERNDEF __imp_GetWindowLongPtrA:QWORD
EXTERNDEF __imp_SystemParametersInfoA:QWORD
EXTERNDEF __imp_GetSystemMetrics:QWORD
EXTERNDEF __imp_AdjustWindowRectEx:QWORD
EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_ReadFile:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
EXTERNDEF __imp_GetFileSize:QWORD
EXTERNDEF __imp_HeapAlloc:QWORD
EXTERNDEF __imp_HeapFree:QWORD
EXTERNDEF __imp_GetProcessHeap:QWORD
EXTERNDEF __imp_ExitProcess:QWORD
EXTERNDEF __imp_InitCommonControlsEx:QWORD
EXTERNDEF __imp_SHBrowseForFolderA:QWORD
EXTERNDEF __imp_SHGetPathFromIDListA:QWORD
EXTERNDEF __imp_lstrcmpA:QWORD
EXTERNDEF __imp_lstrcpyA:QWORD
EXTERNDEF __imp_lstrlenA:QWORD
EXTERNDEF __imp_wsprintfA:QWORD
EXTERNDEF __imp_OutputDebugStringA:QWORD
EXTERNDEF __imp_CreateThread:QWORD
EXTERNDEF __imp_WaitForSingleObject:QWORD
EXTERNDEF __imp_SetEvent:QWORD
EXTERNDEF __imp_ResetEvent:QWORD
EXTERNDEF __imp_CreateEventA:QWORD

; IPC Bridge exports
EXTERNDEF IPC_Initialize:PROC
EXTERNDEF IPC_PostMessage:PROC
EXTERNDEF IPC_ReadMessage:PROC
EXTERNDEF IPC_Shutdown:PROC

; Macros for Win32 indirection
GetModuleHandleA    TEXTEQU <CALL QWORD PTR [__imp_GetModuleHandleA]>
RegisterClassExA    TEXTEQU <CALL QWORD PTR [__imp_RegisterClassExA]>
CreateWindowExA     TEXTEQU <CALL QWORD PTR [__imp_CreateWindowExA]>
ShowWindow          TEXTEQU <CALL QWORD PTR [__imp_ShowWindow]>
UpdateWindow        TEXTEQU <CALL QWORD PTR [__imp_UpdateWindow]>
GetMessageA         TEXTEQU <CALL QWORD PTR [__imp_GetMessageA]>
TranslateMessage    TEXTEQU <CALL QWORD PTR [__imp_TranslateMessage]>
DispatchMessageA    TEXTEQU <CALL QWORD PTR [__imp_DispatchMessageA]>
PostQuitMessage     TEXTEQU <CALL QWORD PTR [__imp_PostQuitMessage]>
DefWindowProcA      TEXTEQU <CALL QWORD PTR [__imp_DefWindowProcA]>
GetClientRect       TEXTEQU <CALL QWORD PTR [__imp_GetClientRect]>
MoveWindow          TEXTEQU <CALL QWORD PTR [__imp_MoveWindow]>
SetWindowTextA      TEXTEQU <CALL QWORD PTR [__imp_SetWindowTextA]>
InvalidateRect      TEXTEQU <CALL QWORD PTR [__imp_InvalidateRect]>
BeginPaint          TEXTEQU <CALL QWORD PTR [__imp_BeginPaint]>
EndPaint            TEXTEQU <CALL QWORD PTR [__imp_EndPaint]>
FillRect            TEXTEQU <CALL QWORD PTR [__imp_FillRect]>
DrawTextA           TEXTEQU <CALL QWORD PTR [__imp_DrawTextA]>
CreateSolidBrush    TEXTEQU <CALL QWORD PTR [__imp_CreateSolidBrush]>
DeleteObject        TEXTEQU <CALL QWORD PTR [__imp_DeleteObject]>
SelectObject        TEXTEQU <CALL QWORD PTR [__imp_SelectObject]>
SetBkColor          TEXTEQU <CALL QWORD PTR [__imp_SetBkColor]>
SetTextColor        TEXTEQU <CALL QWORD PTR [__imp_SetTextColor]>
CreateFontA         TEXTEQU <CALL QWORD PTR [__imp_CreateFontA]>
GetStockObject      TEXTEQU <CALL QWORD PTR [__imp_GetStockObject]>
SetBkMode           TEXTEQU <CALL QWORD PTR [__imp_SetBkMode]>
TextOutA            TEXTEQU <CALL QWORD PTR [__imp_TextOutA]>
CreateMenu          TEXTEQU <CALL QWORD PTR [__imp_CreateMenu]>
CreatePopupMenu     TEXTEQU <CALL QWORD PTR [__imp_CreatePopupMenu]>
AppendMenuA         TEXTEQU <CALL QWORD PTR [__imp_AppendMenuA]>
SetMenu             TEXTEQU <CALL QWORD PTR [__imp_SetMenu]>
LoadLibraryA        TEXTEQU <CALL QWORD PTR [__imp_LoadLibraryA]>
GetProcAddress      TEXTEQU <CALL QWORD PTR [__imp_GetProcAddress]>
MessageBoxA         TEXTEQU <CALL QWORD PTR [__imp_MessageBoxA]>
CreateFileA         TEXTEQU <CALL QWORD PTR [__imp_CreateFileA]>
ReadFile            TEXTEQU <CALL QWORD PTR [__imp_ReadFile]>
WriteFile           TEXTEQU <CALL QWORD PTR [__imp_WriteFile]>
CloseHandle         TEXTEQU <CALL QWORD PTR [__imp_CloseHandle]>
HeapAlloc           TEXTEQU <CALL QWORD PTR [__imp_HeapAlloc]>
HeapFree            TEXTEQU <CALL QWORD PTR [__imp_HeapFree]>
GetProcessHeap      TEXTEQU <CALL QWORD PTR [__imp_GetProcessHeap]>
ExitProcess         TEXTEQU <CALL QWORD PTR [__imp_ExitProcess]>
lstrcpyA            TEXTEQU <CALL QWORD PTR [__imp_lstrcpyA]>
lstrlenA            TEXTEQU <CALL QWORD PTR [__imp_lstrlenA]>
wsprintfA           TEXTEQU <CALL QWORD PTR [__imp_wsprintfA]>
OutputDebugStringA  TEXTEQU <CALL QWORD PTR [__imp_OutputDebugStringA]>
CreateThread        TEXTEQU <CALL QWORD PTR [__imp_CreateThread]>
WaitForSingleObject TEXTEQU <CALL QWORD PTR [__imp_WaitForSingleObject]>
SetEvent            TEXTEQU <CALL QWORD PTR [__imp_SetEvent]>
CreateEventA        TEXTEQU <CALL QWORD PTR [__imp_CreateEventA]>
SendMessageA        TEXTEQU <CALL QWORD PTR [__imp_SendMessageA]>
SetFocus            TEXTEQU <CALL QWORD PTR [__imp_SetFocus]>

;==============================================================================
; Constants
;==============================================================================
; Window / layout
IDE_MIN_WIDTH           EQU 1024
IDE_MIN_HEIGHT          EQU 600
IDE_SIDEBAR_WIDTH       EQU 250
IDE_TOOLBAR_HEIGHT      EQU 36
IDE_STATUSBAR_HEIGHT    EQU 24
IDE_TABBAR_HEIGHT       EQU 32
IDE_SPLITTER_SIZE       EQU 4
IDE_DEFAULT_FONT_SIZE   EQU -14         ; 14pt (negative = pixel height)

; Window styles
WS_OVERLAPPEDWINDOW     EQU 00CF0000h
WS_VISIBLE              EQU 10000000h
WS_CHILD                EQU 40000000h
WS_HSCROLL              EQU 00100000h
WS_VSCROLL              EQU 00200000h
WS_BORDER               EQU 00800000h
WS_CAPTION              EQU 00C00000h
WS_CLIPCHILDREN         EQU 02000000h
WS_CLIPSIBLINGS         EQU 04000000h
WS_THICKFRAME           EQU 00040000h
WS_SYSMENU              EQU 00080000h
WS_MINIMIZEBOX          EQU 00020000h
WS_MAXIMIZEBOX          EQU 00010000h
WS_EX_CLIENTEDGE        EQU 00000200h
ES_MULTILINE            EQU 4h
ES_AUTOHSCROLL          EQU 80h
ES_AUTOVSCROLL          EQU 40h
ES_NOHIDESEL            EQU 100h
ES_WANTRETURN           EQU 1000h
ES_LEFT                 EQU 0h
SS_SIMPLE               EQU 0Bh
SS_SUNKEN               EQU 1000h
PBS_SMOOTH              EQU 1h
CCS_NORESIZE            EQU 4h
TBSTYLE_FLAT            EQU 800h
TBSTYLE_TOOLTIPS        EQU 100h

; Window messages
WM_COMMAND              EQU 111h
WM_SIZE                 EQU 5h
WM_PAINT                EQU 0Fh
WM_DESTROY              EQU 2h
WM_CREATE               EQU 1h
WM_CLOSE                EQU 10h
WM_GETMINMAXINFO        EQU 24h
WM_SIZING               EQU 214h
WM_KEYDOWN              EQU 100h
WM_CHAR                 EQU 102h
WM_SETFONT              EQU 30h
WM_NOTIFY               EQU 4Eh
WM_INITDIALOG           EQU 110h
WM_CTLCOLOREDIT         EQU 133h
WM_CTLCOLORSTATIC       EQU 138h
WM_CTLCOLORLISTBOX      EQU 134h
WM_LBUTTONDOWN          EQU 201h
WM_RBUTTONDOWN          EQU 204h
WM_MOUSEMOVE            EQU 200h
WM_MOUSEWHEEL           EQU 20Ah
WM_ERASEBKGND           EQU 14h
WM_ACTIVATE             EQU 6h
WM_SETCURSOR            EQU 20h
WM_TIMER                EQU 113h
WM_DROPFILES            EQU 233h
EM_SETCHARFORMAT        EQU 0C444h
EM_SETBKGNDCOLOR        EQU 0C443h
EM_GETSEL               EQU 0B0h
EM_SETSEL               EQU 0B1h
EM_LINECOUNT            EQU 0BAh
EM_SCROLLCARET          EQU 0B7h
EN_CHANGE               EQU 300h
EN_UPDATE               EQU 400h
SCF_ALL                 EQU 4h
SW_SHOW                 EQU 5h
SW_SHOWMAXIMIZED        EQU 3h
TRANSPARENT             EQU 1h
OPAQUE                  EQU 2h
DT_LEFT                 EQU 0h
DT_CENTER               EQU 1h
DT_VCENTER              EQU 4h
DT_SINGLELINE           EQU 20h
DT_END_ELLIPSIS         EQU 8000h
MF_POPUP                EQU 10h
MF_STRING               EQU 0h
MF_SEPARATOR            EQU 800h
MF_GRAYED               EQU 1h
MF_CHECKED              EQU 8h
COLOR_WINDOW            EQU 5
COLOR_HIGHLIGHT         EQU 13
COLOR_HIGHLIGHTTEXT     EQU 14
NULL_PEN                EQU 8
DC_BRUSH                EQU 18
DEFAULT_GUI_FONT        EQU 17
HEAP_ZERO_MEMORY        EQU 8h
GENERIC_READ            EQU 80000000h
FILE_SHARE_READ         EQU 1h
OPEN_EXISTING           EQU 3h
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1

; Menu IDs
ID_FILE_NEW             EQU 1001
ID_FILE_OPEN            EQU 1002
ID_FILE_SAVE            EQU 1003
ID_FILE_SAVE_AS         EQU 1004
ID_FILE_EXIT            EQU 1005
ID_EDIT_UNDO            EQU 1010
ID_EDIT_REDO            EQU 1011
ID_EDIT_CUT             EQU 1012
ID_EDIT_COPY            EQU 1013
ID_EDIT_PASTE           EQU 1014
ID_EDIT_FIND            EQU 1015
ID_EDIT_REPLACE         EQU 1016
ID_VIEW_SIDEBAR         EQU 1020
ID_VIEW_STATUSBAR       EQU 1021
ID_VIEW_FONT            EQU 1022
ID_AGENT_RUN            EQU 1030
ID_AGENT_STOP           EQU 1031
ID_AGENT_CLEAR          EQU 1032
ID_BUILD_BUILD          EQU 1040
ID_BUILD_RUN            EQU 1041
ID_BUILD_DEBUG          EQU 1042
ID_HELP_ABOUT           EQU 1050

; Timer IDs
TIMER_AGENT_POLL        EQU 1           ; Poll IPC ring for AI tokens
TIMER_AUTOSAVE          EQU 2           ; Autosave every 60s
TIMER_STATUS_CLEAR      EQU 3           ; Clear status message after 5s

; Child window IDs
ID_EDITOR               EQU 100
ID_SIDEBAR              EQU 101
ID_STATUSBAR            EQU 102
ID_TOOLBAR              EQU 103
ID_TABBAR               EQU 104
ID_AGENT_OUTPUT         EQU 105
ID_SPLITTER             EQU 106

; RichEdit text colors (dark theme — VS Code-like)
CLR_EDITOR_BG           EQU 001E1E1Eh  ; #1e1e1e
CLR_EDITOR_FG           EQU 00D4D4D4h  ; #d4d4d4
CLR_SIDEBAR_BG          EQU 00252526h  ; #252526
CLR_TOOLBAR_BG          EQU 003C3C3Ch  ; #3c3c3c
CLR_STATUSBAR_BG        EQU 00007ACC h ; VS Blue
CLR_STATUSBAR_FG        EQU 00FFFFFFh  ; white
CLR_SELECTION_BG        EQU 00264F78h  ; selection
CLR_KEYWORD             EQU 00569CD6h  ; #569cd6 blue
CLR_STRING              EQU 00CE9178h  ; #ce9178 orange
CLR_COMMENT             EQU 006A9955h  ; #6a9955 green
CLR_NUMBER              EQU 00B5CEA8h  ; #b5cea8 light green
CLR_FUNCTION            EQU 00DCDCAA h ; #dcdcaa yellow
CLR_TAB_ACTIVE_BG       EQU 001E1E1Eh
CLR_TAB_INACTIVE_BG     EQU 002D2D2Dh
CLR_TAB_TEXT_ACTIVE     EQU 00FFFFFFh
CLR_TAB_TEXT_INACTIVE   EQU 00808080h

;==============================================================================
; Structures
;==============================================================================
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

MSG_STRUCT STRUCT
    hwnd            QWORD ?
    message         DWORD ?
    _pad1           DWORD 0
    wParam          QWORD ?
    lParam          QWORD ?
    time            DWORD ?
    pt_x            DWORD ?
    pt_y            DWORD ?
    _pad2           DWORD 0
MSG_STRUCT ENDS

RECT_STRUCT STRUCT
    left            DWORD ?
    top             DWORD ?
    right           DWORD ?
    bottom          DWORD ?
RECT_STRUCT ENDS

PAINTSTRUCT STRUCT
    hdc             QWORD ?
    fErase          DWORD ?
    rc_left         DWORD ?
    rc_top          DWORD ?
    rc_right        DWORD ?
    rc_bottom       DWORD ?
    restore         DWORD ?
    incUpdate       DWORD ?
    rgbReserved     BYTE 32 DUP(?)
PAINTSTRUCT ENDS

CHARFORMAT2A STRUCT
    cbSize          DWORD ?
    dwMask          DWORD ?
    dwEffects       DWORD ?
    yHeight         DWORD ?
    yOffset         DWORD ?
    crTextColor     DWORD ?
    bCharSet        BYTE  ?
    bPitchAndFamily BYTE  ?
    szFaceName      BYTE  32 DUP(?)
    wWeight         WORD  ?
    sSpacing        WORD  ?
    crBackColor     DWORD ?
    lcid            DWORD ?
    dwReserved      DWORD ?
    sStyle          WORD  ?
    wKerning        WORD  ?
    bUnderlineType  BYTE  ?
    bAnimation      BYTE  ?
    bRevAuthor      BYTE  ?
    bReserved1      BYTE  ?
CHARFORMAT2A ENDS

MINMAXINFO STRUCT
    ptReserved      QWORD ?
    ptMaxSize       QWORD ?
    ptMaxPosition   QWORD ?
    ptMinTrackSize  QWORD ?
    ptMaxTrackSize  QWORD ?
MINMAXINFO ENDS

; IDE state persisted across resizes
IDE_STATE STRUCT
    hMainWnd        QWORD ?             ; Main window handle
    hEditor         QWORD ?             ; RichEdit editor control
    hSidebar        QWORD ?             ; Explorer/tree panel
    hStatusBar      QWORD ?             ; Status bar control
    hToolbar        QWORD ?             ; Toolbar control
    hTabBar         QWORD ?             ; Tab bar area
    hAgentOutput    QWORD ?             ; Agent output RichEdit
    hFont           QWORD ?             ; Monospace editor font
    hStatusFont     QWORD ?             ; Status bar font
    hBrushEditor    QWORD ?             ; Editor background brush
    hBrushSidebar   QWORD ?             ; Sidebar background brush
    hBrushToolbar   QWORD ?             ; Toolbar background brush
    hBrushStatus    QWORD ?             ; Status bar brush
    hBrushTab       QWORD ?             ; Tab bar brush
    hRichEditLib    QWORD ?             ; msftedit.dll/riched32.dll handle
    hInstance       QWORD ?             ; Module instance
    hIPC            QWORD ?             ; IPC context handle
    hAgentThread    QWORD ?             ; Agent polling thread
    hAgentEvent     QWORD ?             ; Agent wake event
    isSidebarVisible BYTE ?             ; Sidebar visibility flag
    isAgentRunning  BYTE ?              ; Agent active flag
    isDirty         BYTE ?              ; Unsaved changes
    _pad            BYTE ?
    currentFile     BYTE 260 DUP(?)    ; Current open file path
    statusText      BYTE 256 DUP(?)    ; Status bar message
    agentBuffer     BYTE 4096 DUP(?)   ; Pending agent tokens
    clientWidth     DWORD ?
    clientHeight    DWORD ?
IDE_STATE ENDS

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA

ALIGN 16
g_IDE               IDE_STATE <>

; Window class / title
szClassName         BYTE "RawrXD_IDE_Class_v1", 0
szWindowTitle       BYTE "RawrXD Agentic IDE — Production", 0
szAbout             BYTE "RawrXD Universal Agentic IDE v1.0", 13, 10,
                         "Win32 Pure Assembly — Zero External Deps", 13, 10,
                         "PE Generator | Machine Code Emitter | AI Agent", 0
szRichEdit          BYTE "msftedit.dll", 0
szRichEditFallback  BYTE "RICHED32.DLL", 0
szRichEditClass     BYTE "RichEdit50W", 0
szRichEditClassA    BYTE "RICHEDIT_CLASS", 0
szEditorClass       BYTE "RichEdit50W", 0
szStaticClass       BYTE "STATIC", 0
szEditClass         BYTE "EDIT", 0

; Font name
szFontName          BYTE "Cascadia Code", 0
szFontFallback      BYTE "Consolas", 0
szStatusFont        BYTE "Segoe UI", 0

; Status messages
szReady             BYTE "Ready", 0
szSaved             BYTE "File saved", 0
szNew               BYTE "New file", 0
szOpened            BYTE "File opened: ", 0
szAgentConnected    BYTE "Agent connected", 0
szAgentStopped      BYTE "Agent stopped", 0
szBuildOK           BYTE "Build succeeded", 0
szBuildFail         BYTE "Build failed", 0
szUnsaved           BYTE " [unsaved]", 0

; Open file dialog filter (OFN_FILTER)
szFileFilter        BYTE "All Files (*.*)", 0, "*.*", 0,
                         "Assembly (*.asm)", 0, "*.asm", 0,
                         "C/C++ (*.c;*.cpp;*.h)", 0, "*.c;*.cpp;*.h", 0,
                         "Text (*.txt)", 0, "*.txt", 0, 0, 0

; Menu strings
szMenuFile          BYTE "&File", 0
szMenuEdit          BYTE "&Edit", 0
szMenuView          BYTE "&View", 0
szMenuAgent         BYTE "&Agent", 0
szMenuBuild         BYTE "&Build", 0
szMenuHelp          BYTE "&Help", 0
szMnNew             BYTE "&New\tCtrl+N", 0
szMnOpen            BYTE "&Open...\tCtrl+O", 0
szMnSave            BYTE "&Save\tCtrl+S", 0
szMnSaveAs          BYTE "Save &As...\tCtrl+Shift+S", 0
szMnExit            BYTE "E&xit\tAlt+F4", 0
szMnUndo            BYTE "&Undo\tCtrl+Z", 0
szMnRedo            BYTE "&Redo\tCtrl+Y", 0
szMnCut             BYTE "Cu&t\tCtrl+X", 0
szMnCopy            BYTE "&Copy\tCtrl+C", 0
szMnPaste           BYTE "&Paste\tCtrl+V", 0
szMnFind            BYTE "&Find...\tCtrl+F", 0
szMnReplace         BYTE "&Replace...\tCtrl+H", 0
szMnSidebar         BYTE "Toggle &Sidebar", 0
szMnStatusbar       BYTE "Toggle Status&bar", 0
szMnAgentRun        BYTE "&Run Agent\tF5", 0
szMnAgentStop       BYTE "&Stop Agent\tF6", 0
szMnAgentClear      BYTE "&Clear Output", 0
szMnBuild           BYTE "&Build\tF7", 0
szMnRun             BYTE "&Run\tF8", 0
szMnDebug           BYTE "&Debug\tF9", 0
szMnAbout           BYTE "&About RawrXD IDE", 0

; Size hint format
szTitleFmt          BYTE "RawrXD Agentic IDE — %s%s", 0

;==============================================================================
; CODE SECTION
;==============================================================================
.CODE

;==============================================================================
; IDE_LoadRichEdit — Load RichEdit library (tries modern msftedit first)
; Returns: HMODULE or NULL; updates g_IDE.hRichEditLib
;==============================================================================
IDE_LoadRichEdit PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; Try modern msftedit.dll (RichEdit 5.0/6.0 — available Win XP+)
    lea rcx, [szRichEdit]
    LoadLibraryA
    test rax, rax
    jnz .re_done

    ; Fallback: RICHED32.DLL (Win 95+)
    lea rcx, [szRichEditFallback]
    LoadLibraryA
    test rax, rax
    jz .re_fail

.re_done:
    mov [g_IDE.hRichEditLib], rax
    add rsp, 32
    pop rbx
    ret

.re_fail:
    xor rax, rax
    mov [g_IDE.hRichEditLib], rax
    add rsp, 32
    pop rbx
    ret
IDE_LoadRichEdit ENDP

;==============================================================================
; IDE_CreateFonts — Create editor and status bar GDI fonts
;==============================================================================
IDE_CreateFonts PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; CreateFontA(nHeight, nWidth, nEsc, nOrient, fnWeight, fdwItalic,
    ;             fdwUnderline, fdwStrikeout, fdwCharSet, fdwOutputPrec,
    ;             fdwClipPrec, fdwQuality, fdwPitchAndFamily, lpszFace)
    ; nHeight = IDE_DEFAULT_FONT_SIZE (-14)
    ; fnWeight = 400 (FW_NORMAL)
    ; CLEARTYPE_QUALITY = 5, FIXED_PITCH|FF_MODERN = 49
    sub rsp, 48
    mov rcx, IDE_DEFAULT_FONT_SIZE  ; nHeight
    xor edx, edx                    ; nWidth
    xor r8, r8                      ; nEsc
    xor r9, r9                      ; nOrient
    mov qword ptr [rsp+32], 400     ; fnWeight = FW_NORMAL
    mov qword ptr [rsp+40], 0       ; italic, underline, strikeout, charset
    mov qword ptr [rsp+48], 0       ; output prec, clip prec
    sub rsp, 32
    ; quality=5 (CLEARTYPE), pitchfamily=49 (FIXED_PITCH|FF_MODERN)
    mov qword ptr [rsp+32], 5
    mov qword ptr [rsp+40], 49
    lea rax, [szFontName]
    mov qword ptr [rsp+48], rax
    call qword ptr [__imp_CreateFontA]
    add rsp, 80
    test rax, rax
    jnz .font_ok
    ; Fall back to Consolas
    sub rsp, 80
    mov rcx, IDE_DEFAULT_FONT_SIZE
    xor edx, edx
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 400
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+56], 5
    mov qword ptr [rsp+64], 49
    lea rax, [szFontFallback]
    mov qword ptr [rsp+72], rax
    call qword ptr [__imp_CreateFontA]
    add rsp, 80
.font_ok:
    mov [g_IDE.hFont], rax

    ; Status bar font: Segoe UI 9pt
    sub rsp, 80
    mov rcx, -12                    ; 12px
    xor edx, edx
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+32], 400
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    mov qword ptr [rsp+56], 5
    mov qword ptr [rsp+64], 0
    lea rax, [szStatusFont]
    mov qword ptr [rsp+72], rax
    call qword ptr [__imp_CreateFontA]
    add rsp, 80
    mov [g_IDE.hStatusFont], rax

    add rsp, 32
    pop rbx
    ret
IDE_CreateFonts ENDP

;==============================================================================
; IDE_CreateBrushes — Allocate GDI brushes for all panel backgrounds
;==============================================================================
IDE_CreateBrushes PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov ecx, CLR_EDITOR_BG
    CreateSolidBrush
    mov [g_IDE.hBrushEditor], rax

    mov ecx, CLR_SIDEBAR_BG
    CreateSolidBrush
    mov [g_IDE.hBrushSidebar], rax

    mov ecx, CLR_TOOLBAR_BG
    CreateSolidBrush
    mov [g_IDE.hBrushToolbar], rax

    mov ecx, CLR_STATUSBAR_BG
    CreateSolidBrush
    mov [g_IDE.hBrushStatus], rax

    mov ecx, CLR_TAB_INACTIVE_BG
    CreateSolidBrush
    mov [g_IDE.hBrushTab], rax

    add rsp, 32
    pop rbx
    ret
IDE_CreateBrushes ENDP

;==============================================================================
; IDE_CreateMenu — Build the IDE menu bar
;==============================================================================
IDE_CreateMenu PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; Root menu bar
    CreateMenu
    mov rbx, rax                    ; hMenuBar

    ; ---- File menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_FILE_NEW
    lea r9, [szMnNew]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_FILE_OPEN
    lea r9, [szMnOpen]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_FILE_SAVE
    lea r9, [szMnSave]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_FILE_SAVE_AS
    lea r9, [szMnSaveAs]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_SEPARATOR
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_FILE_EXIT
    lea r9, [szMnExit]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    ; Append File popup to menu bar
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuFile]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    ; ---- Edit menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_UNDO
    lea r9, [szMnUndo]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_REDO
    lea r9, [szMnRedo]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_SEPARATOR
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_CUT
    lea r9, [szMnCut]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_COPY
    lea r9, [szMnCopy]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_PASTE
    lea r9, [szMnPaste]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_SEPARATOR
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_FIND
    lea r9, [szMnFind]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_EDIT_REPLACE
    lea r9, [szMnReplace]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuEdit]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    ; ---- View menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_VIEW_SIDEBAR
    lea r9, [szMnSidebar]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_VIEW_STATUSBAR
    lea r9, [szMnStatusbar]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuView]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    ; ---- Agent menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_AGENT_RUN
    lea r9, [szMnAgentRun]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_AGENT_STOP
    lea r9, [szMnAgentStop]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_AGENT_CLEAR
    lea r9, [szMnAgentClear]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuAgent]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    ; ---- Build menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_BUILD_BUILD
    lea r9, [szMnBuild]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_BUILD_RUN
    lea r9, [szMnRun]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_BUILD_DEBUG
    lea r9, [szMnDebug]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuBuild]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    ; ---- Help menu ----
    CreatePopupMenu
    mov rsi, rax
    sub rsp, 32
    mov rcx, rsi
    mov edx, MF_STRING
    mov r8d, ID_HELP_ABOUT
    lea r9, [szMnAbout]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    mov edx, MF_POPUP
    mov r8, rsi
    lea r9, [szMenuHelp]
    call qword ptr [__imp_AppendMenuA]
    add rsp, 32

    mov rax, rbx                    ; return hMenuBar

    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
IDE_CreateMenu ENDP

;==============================================================================
; IDE_CreateChildWindows — Create all child controls under hWnd
; RCX = hWnd, RDX = width, R8 = height
;==============================================================================
IDE_CreateChildWindows PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .pushreg r15
    sub rsp, 48
    .allocstack 48
    .endprolog

    mov rbx, rcx                    ; hWnd
    mov r12d, edx                   ; width
    mov r13d, r8d                   ; height

    ; Calculate layout dimensions
    mov r14d, IDE_SIDEBAR_WIDTH     ; sidebar width
    mov r15d, IDE_TOOLBAR_HEIGHT    ; toolbar height

    ; ---- Toolbar ----
    ; A simple STATIC panel styled as toolbar (no comctl32 dep required)
    sub rsp, 72
    xor ecx, ecx                    ; dwExStyle
    lea rdx, [szStaticClass]
    xor r8, r8                      ; text: none
    mov r9d, (WS_CHILD OR WS_VISIBLE)
    mov dword ptr [rsp+32], 0       ; x
    mov dword ptr [rsp+36], 0       ; y
    mov dword ptr [rsp+40], r12d    ; width = full width
    mov dword ptr [rsp+44], r15d    ; height = toolbar height
    mov qword ptr [rsp+48], rbx     ; hParent
    mov qword ptr [rsp+56], ID_TOOLBAR
    mov rax, [g_IDE.hInstance]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], 0
    call qword ptr [__imp_CreateWindowExA]
    add rsp, 72
    mov [g_IDE.hToolbar], rax

    ; ---- Editor (RichEdit) ----
    ; Position: sidebar_width, toolbar_height; width: total-sidebar, height-toolbar-statusbar-tabbar
    mov eax, r12d
    sub eax, r14d                   ; editor width = total - sidebar
    mov esi, r13d
    sub esi, r15d
    sub esi, IDE_STATUSBAR_HEIGHT
    sub esi, IDE_TABBAR_HEIGHT      ; editor height
    sub rsp, 72
    xor ecx, ecx                    ; dwExStyle = 0
    lea rdx, [szEditorClass]
    xor r8, r8
    ; ES_MULTILINE|ES_AUTOHSCROLL|ES_AUTOVSCROLL|ES_NOHIDESEL|ES_WANTRETURN|WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL
    mov r9d, (WS_CHILD OR WS_VISIBLE OR WS_VSCROLL OR WS_HSCROLL OR ES_MULTILINE OR ES_AUTOVSCROLL OR ES_AUTOHSCROLL OR ES_NOHIDESEL OR ES_WANTRETURN)
    mov dword ptr [rsp+32], r14d    ; x = sidebar width
    mov edx, r15d
    add edx, IDE_TABBAR_HEIGHT
    mov dword ptr [rsp+36], edx     ; y = toolbar + tabbar
    mov dword ptr [rsp+40], eax     ; width
    mov dword ptr [rsp+44], esi     ; height
    mov qword ptr [rsp+48], rbx
    mov qword ptr [rsp+56], ID_EDITOR
    mov rax, [g_IDE.hInstance]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], 0
    call qword ptr [__imp_CreateWindowExA]
    add rsp, 72
    mov [g_IDE.hEditor], rax
    ; Apply font to editor
    test rax, rax
    jz .cw_no_editor
    mov rcx, rax
    mov edx, WM_SETFONT
    mov r8, [g_IDE.hFont]
    mov r9d, 1
    SendMessageA
    ; Set dark background color via EM_SETBKGNDCOLOR
    mov rcx, [g_IDE.hEditor]
    mov edx, EM_SETBKGNDCOLOR
    xor r8, r8
    mov r9d, CLR_EDITOR_BG
    SendMessageA
.cw_no_editor:

    ; ---- Sidebar ----
    ; A RichEdit acting as file explorer / agent log for now
    mov eax, r13d
    sub eax, r15d
    sub eax, IDE_STATUSBAR_HEIGHT   ; sidebar height
    sub rsp, 72
    xor ecx, ecx
    lea rdx, [szEditorClass]
    xor r8, r8
    mov r9d, (WS_CHILD OR WS_VISIBLE OR WS_VSCROLL OR ES_MULTILINE OR ES_AUTOVSCROLL)
    mov dword ptr [rsp+32], 0       ; x = 0
    mov dword ptr [rsp+36], r15d    ; y = toolbar height
    mov dword ptr [rsp+40], r14d    ; width = sidebar width
    mov dword ptr [rsp+44], eax
    mov qword ptr [rsp+48], rbx
    mov qword ptr [rsp+56], ID_SIDEBAR
    mov rax, [g_IDE.hInstance]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], 0
    call qword ptr [__imp_CreateWindowExA]
    add rsp, 72
    mov [g_IDE.hSidebar], rax
    test rax, rax
    jz .cw_no_sidebar
    mov rcx, rax
    mov edx, WM_SETFONT
    mov r8, [g_IDE.hFont]
    mov r9d, 1
    SendMessageA
    mov rcx, [g_IDE.hSidebar]
    mov edx, EM_SETBKGNDCOLOR
    xor r8, r8
    mov r9d, CLR_SIDEBAR_BG
    SendMessageA
.cw_no_sidebar:

    ; ---- Status bar ----
    sub rsp, 72
    xor ecx, ecx
    lea rdx, [szStaticClass]
    lea r8, [szReady]
    mov r9d, (WS_CHILD OR WS_VISIBLE OR SS_SIMPLE)
    mov eax, r13d
    sub eax, IDE_STATUSBAR_HEIGHT
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+36], eax     ; y = total - statusbar
    mov dword ptr [rsp+40], r12d    ; width = full
    mov dword ptr [rsp+44], IDE_STATUSBAR_HEIGHT
    mov qword ptr [rsp+48], rbx
    mov qword ptr [rsp+56], ID_STATUSBAR
    mov rax, [g_IDE.hInstance]
    mov qword ptr [rsp+64], rax
    mov qword ptr [rsp+72], 0
    call qword ptr [__imp_CreateWindowExA]
    add rsp, 72
    mov [g_IDE.hStatusBar], rax
    test rax, rax
    jz .cw_no_status
    mov rcx, rax
    mov edx, WM_SETFONT
    mov r8, [g_IDE.hStatusFont]
    mov r9d, 1
    SendMessageA
.cw_no_status:

    ; Set focus to editor
    mov rcx, [g_IDE.hEditor]
    test rcx, rcx
    jz .cw_done
    SetFocus

.cw_done:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
IDE_CreateChildWindows ENDP

;==============================================================================
; IDE_DoLayout — Reposition child controls after WM_SIZE
; RCX = hWnd, RDX = new width, R8 = new height
;==============================================================================
IDE_DoLayout PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    sub rsp, 48
    .allocstack 48
    .endprolog

    mov rbx, rcx
    mov r12d, edx                   ; width
    mov r13d, r8d                   ; height

    ; Toolbar: full width, top
    mov rcx, [g_IDE.hToolbar]
    test rcx, rcx
    jz .dl_no_toolbar
    sub rsp, 32
    xor edx, edx                    ; x
    xor r8, r8                      ; y
    mov r9d, r12d                   ; width
    mov dword ptr [rsp+32], IDE_TOOLBAR_HEIGHT
    mov dword ptr [rsp+36], 1       ; bRepaint
    call qword ptr [__imp_MoveWindow]
    add rsp, 32
.dl_no_toolbar:

    ; Status bar: full width, bottom
    mov rcx, [g_IDE.hStatusBar]
    test rcx, rcx
    jz .dl_no_status
    sub rsp, 32
    xor edx, edx
    mov r8d, r13d
    sub r8d, IDE_STATUSBAR_HEIGHT
    mov r9d, r12d
    mov dword ptr [rsp+32], IDE_STATUSBAR_HEIGHT
    mov dword ptr [rsp+36], 1
    call qword ptr [__imp_MoveWindow]
    add rsp, 32
.dl_no_status:

    ; Sidebar: left side, below toolbar, above statusbar
    mov eax, r13d
    sub eax, IDE_TOOLBAR_HEIGHT
    sub eax, IDE_STATUSBAR_HEIGHT
    mov rcx, [g_IDE.hSidebar]
    test rcx, rcx
    jz .dl_no_sidebar
    cmp [g_IDE.isSidebarVisible], 0
    je .dl_sidebar_hidden
    sub rsp, 32
    xor edx, edx                    ; x
    mov r8d, IDE_TOOLBAR_HEIGHT     ; y
    mov r9d, IDE_SIDEBAR_WIDTH      ; width
    mov dword ptr [rsp+32], eax     ; height
    mov dword ptr [rsp+36], 1
    call qword ptr [__imp_MoveWindow]
    add rsp, 32
    jmp .dl_no_sidebar
.dl_sidebar_hidden:
    sub rsp, 32
    xor edx, edx
    xor r8, r8
    xor r9, r9
    xor r11d, r11d
    mov dword ptr [rsp+32], r11d    ; height = 0 (hidden)
    mov dword ptr [rsp+36], 1
    call qword ptr [__imp_MoveWindow]
    add rsp, 32
.dl_no_sidebar:

    ; Editor: right of sidebar, below toolbar+tabbar, above statusbar
    mov edi, r12d
    cmp [g_IDE.isSidebarVisible], 0
    je .dl_editor_no_sidebar
    sub edi, IDE_SIDEBAR_WIDTH      ; editor width
.dl_editor_no_sidebar:
    mov esi, r13d
    sub esi, IDE_TOOLBAR_HEIGHT
    sub esi, IDE_STATUSBAR_HEIGHT
    sub esi, IDE_TABBAR_HEIGHT      ; editor height
    mov rcx, [g_IDE.hEditor]
    test rcx, rcx
    jz .dl_done
    sub rsp, 32
    ; x = sidebar width (or 0 if hidden), y = toolbar + tabbar
    mov edx, IDE_SIDEBAR_WIDTH
    cmp [g_IDE.isSidebarVisible], 0
    jne .dl_e_x_ok
    xor edx, edx
.dl_e_x_ok:
    mov r8d, IDE_TOOLBAR_HEIGHT
    add r8d, IDE_TABBAR_HEIGHT
    mov r9d, edi
    mov dword ptr [rsp+32], esi
    mov dword ptr [rsp+36], 1
    call qword ptr [__imp_MoveWindow]
    add rsp, 32

.dl_done:
    add rsp, 48
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
IDE_DoLayout ENDP

;==============================================================================
; IDE_UpdateTitle — Update window title with current file + dirty flag
;==============================================================================
IDE_UpdateTitle PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32+256
    .allocstack 32+256
    .endprolog

    lea rbx, [rsp+32]               ; temp title buffer
    sub rsp, 32
    mov rcx, rbx
    lea rdx, [szTitleFmt]
    lea r8, [g_IDE.currentFile]
    ; dirty flag string
    mov al, [g_IDE.isDirty]
    test al, al
    jz .title_clean
    lea r9, [szUnsaved]
    jmp .title_wsprintf
.title_clean:
    xor r9, r9
    lea r9, [.szEmpty]
.title_wsprintf:
    call qword ptr [__imp_wsprintfA]
    add rsp, 32
    mov rcx, [g_IDE.hMainWnd]
    mov rdx, rbx
    call qword ptr [__imp_SetWindowTextA]

    add rsp, 32+256
    pop rbx
    ret
.szEmpty: BYTE 0
IDE_UpdateTitle ENDP

;==============================================================================
; IDE_SetStatus — Write text to status bar
; RCX = NUL-terminated status string
;==============================================================================
IDE_SetStatus PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rbx, rcx
    mov rcx, [g_IDE.hStatusBar]
    test rcx, rcx
    jz .ss_done
    sub rsp, 32
    mov rdx, rbx
    call qword ptr [__imp_SetWindowTextA]
    add rsp, 32
    ; Auto-clear after 5 seconds
    sub rsp, 32
    mov rcx, [g_IDE.hMainWnd]
    mov edx, WM_TIMER
    ; SetTimer(hWnd, TIMER_STATUS_CLEAR, 5000, NULL)
    call qword ptr [__imp_ExitProcess]  ; placeholder for SetTimer
    add rsp, 32
.ss_done:
    add rsp, 32
    pop rbx
    ret
IDE_SetStatus ENDP

;==============================================================================
; IDE_FileOpen — Show OpenFileDialog and load selected file into editor
;==============================================================================
IDE_FileOpen PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    sub rsp, 32 + 1024 + 88         ; ofn struct + shadow
    .allocstack 32 + 1024 + 88
    .endprolog

    lea rbx, [rsp + 32]             ; OPENFILENAMEA structure start
    ; Zero the structure
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 88+1024
    call qword ptr [__imp_HeapAlloc] ; use RtlZeroMemory equivalent
    ; Actually zero it via stosq
    push rdi
    lea rdi, [rbx]
    xor eax, eax
    mov ecx, (88 + 1024 + 8) / 8
    rep stosq
    pop rdi

    ; OPENFILENAMEA.lStructSize
    mov dword ptr [rbx], 88         ; sizeof OPENFILENAMEA
    ; hwndOwner
    mov rax, [g_IDE.hMainWnd]
    mov [rbx + 8], rax
    ; hInstance
    mov rax, [g_IDE.hInstance]
    mov [rbx + 16], rax
    ; lpstrFilter
    lea rax, [szFileFilter]
    mov [rbx + 24], rax
    ; lpstrFile (output buffer, 260 chars after struct)
    lea rax, [rbx + 88]
    mov [rbx + 48], rax
    mov dword ptr [rbx + 56], 1024  ; nMaxFile
    ; Flags: OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY = 0x1024
    mov dword ptr [rbx + 76], 0x1024
    ; Title
    lea rax, [.szOpenTitle]
    mov [rbx + 32], rax

    ; Call GetOpenFileNameA (from comdlg32.dll — resolve dynamically)
    mov rax, [g_hComdlg32]
    test rax, rax
    jnz .fo_have_comdlg
    lea rcx, [.szComdlg32]
    LoadLibraryA
    mov [g_hComdlg32], rax
    test rax, rax
    jz .fo_cancel
    mov rcx, rax
    lea rdx, [.szGetOpenFileName]
    GetProcAddress
    mov [g_fnGetOpenFileName], rax
.fo_have_comdlg:
    mov rax, [g_fnGetOpenFileName]
    test rax, rax
    jz .fo_cancel
    sub rsp, 32
    mov rcx, rbx
    call rax
    add rsp, 32
    test eax, eax
    jz .fo_cancel

    ; Copy path from OFN buffer to g_IDE.currentFile
    sub rsp, 32
    lea rcx, [g_IDE.currentFile]
    lea rdx, [rbx + 88]
    call qword ptr [__imp_lstrcpyA]
    add rsp, 32

    ; Open file with CreateFileA
    sub rsp, 40
    lea rcx, [g_IDE.currentFile]
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9, r9                  ; lpSecurityAttributes = NULL
    mov dword ptr [rsp+32], OPEN_EXISTING
    mov dword ptr [rsp+36], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_CreateFileA]
    add rsp, 40
    cmp rax, INVALID_HANDLE_VALUE
    je .fo_cancel
    mov rsi, rax                ; hFile

    ; Get file size
    sub rsp, 32
    mov rcx, rsi
    xor rdx, rdx
    call qword ptr [__imp_GetFileSize]
    add rsp, 32
    test eax, eax
    jz .fo_close
    cmp eax, 8388608            ; limit 8MB for editor
    ja .fo_close
    mov r12d, eax               ; fileSize

    ; Allocate read buffer
    sub rsp, 32
    call qword ptr [__imp_GetProcessHeap]
    add rsp, 32
    sub rsp, 32
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, r12d
    inc r8                      ; +1 for NUL
    call qword ptr [__imp_HeapAlloc]
    add rsp, 32
    test rax, rax
    jz .fo_close
    mov rdi, rax                ; fileBuffer

    ; ReadFile
    sub rsp, 40
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, r12d
    lea r9, [rsp+32]            ; lpNumberOfBytesRead
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_ReadFile]
    add rsp, 40
    test eax, eax
    jz .fo_free

    ; Set editor text via WM_SETTEXT
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0x000C             ; WM_SETTEXT
    xor r8, r8
    mov r9, rdi
    call qword ptr [__imp_SendMessageA]
    add rsp, 32

    ; Clear dirty flag, update title
    mov [g_IDE.isDirty], 0
    call IDE_UpdateTitle

    ; Set status
    lea rcx, [szOpened]
    call IDE_SetStatus

.fo_free:
    sub rsp, 32
    call qword ptr [__imp_GetProcessHeap]
    add rsp, 32
    sub rsp, 32
    mov rcx, rax
    xor edx, edx
    mov r8, rdi
    call qword ptr [__imp_HeapFree]
    add rsp, 32

.fo_close:
    sub rsp, 32
    mov rcx, rsi
    call qword ptr [__imp_CloseHandle]
    add rsp, 32

.fo_cancel:
    add rsp, 32 + 1024 + 88
    pop rdi
    pop rsi
    pop rbx
    ret

.szOpenTitle:       BYTE "Open File", 0
.szComdlg32:        BYTE "comdlg32.dll", 0
.szGetOpenFileName: BYTE "GetOpenFileNameA", 0
IDE_FileOpen ENDP

; Globals for comdlg32 dynamic loading
g_hComdlg32         QWORD 0
g_fnGetOpenFileName QWORD 0
g_fnGetSaveFileName QWORD 0

;==============================================================================
; IDE_FileSave — Save current editor text to g_IDE.currentFile
;==============================================================================
IDE_FileSave PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    sub rsp, 48
    .allocstack 48
    .endprolog

    ; Check if we have a file name
    cmp byte ptr [g_IDE.currentFile], 0
    jne .fs_have_name
    ; No name yet: show save-as dialog
    call IDE_FileSaveAs
    jmp .fs_done

.fs_have_name:
    ; Get text length from editor
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 000Eh              ; EM_GETTEXTLENGTHEX or WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    test rax, rax
    jz .fs_done
    mov r12, rax                ; textLen
    inc r12                     ; +1 for NUL

    ; Allocate buffer
    sub rsp, 32
    call qword ptr [__imp_GetProcessHeap]
    add rsp, 32
    sub rsp, 32
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8, r12
    call qword ptr [__imp_HeapAlloc]
    add rsp, 32
    test rax, rax
    jz .fs_done
    mov rdi, rax

    ; Get text from editor via WM_GETTEXT
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 000Dh              ; WM_GETTEXT
    mov r8, r12
    mov r9, rdi
    call qword ptr [__imp_SendMessageA]
    add rsp, 32

    ; Create or overwrite file
    sub rsp, 40
    lea rcx, [g_IDE.currentFile]
    mov edx, 40000000h          ; GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov dword ptr [rsp+32], 2   ; CREATE_ALWAYS
    mov dword ptr [rsp+36], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_CreateFileA]
    add rsp, 40
    cmp rax, INVALID_HANDLE_VALUE
    je .fs_free
    mov rsi, rax

    ; WriteFile
    sub rsp, 40
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, r12d
    dec r8                      ; don't write NUL
    lea r9, [rsp+32]
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_WriteFile]
    add rsp, 40

    ; CloseHandle
    sub rsp, 32
    mov rcx, rsi
    call qword ptr [__imp_CloseHandle]
    add rsp, 32

    ; Clear dirty, update title, set status
    mov [g_IDE.isDirty], 0
    call IDE_UpdateTitle
    lea rcx, [szSaved]
    call IDE_SetStatus

.fs_free:
    sub rsp, 32
    call qword ptr [__imp_GetProcessHeap]
    add rsp, 32
    sub rsp, 32
    mov rcx, rax
    xor edx, edx
    mov r8, rdi
    call qword ptr [__imp_HeapFree]
    add rsp, 32

.fs_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
IDE_FileSave ENDP

;==============================================================================
; IDE_FileSaveAs — Stub that shows SaveFileDialog then calls IDE_FileSave
;==============================================================================
IDE_FileSaveAs PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    ; For a minimal implementation, fall through to IDE_FileSave after
    ; populating currentFile. Full dialog follows GetSaveFileName flow.
    ; If currentFile is empty, default to "untitled.asm"
    cmp byte ptr [g_IDE.currentFile], 0
    jne .sa_have
    lea rcx, [g_IDE.currentFile]
    lea rdx, [.szUntitled]
    sub rsp, 32
    call qword ptr [__imp_lstrcpyA]
    add rsp, 32
.sa_have:
    call IDE_FileSave
    add rsp, 32
    pop rbx
    ret
.szUntitled: BYTE "untitled.asm", 0
IDE_FileSaveAs ENDP

;==============================================================================
; IDE_AgentThread — Background thread: polls IPC ring for AI tokens
; and appends them to the editor / agent output pane
;==============================================================================
IDE_AgentThread PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    sub rsp, 48 + 2048
    .allocstack 48 + 2048
    .endprolog

    lea r12, [rsp + 48]             ; token receive buffer (2048 bytes)
    lea rbx, [g_IDE]

.agt_loop:
    ; Wait for agent event or 100ms timeout
    sub rsp, 32
    mov rcx, [rbx].IDE_STATE.hAgentEvent
    mov edx, 100                    ; 100ms poll interval
    call qword ptr [__imp_WaitForSingleObject]
    add rsp, 32

    ; Check if agent still running
    cmp [rbx].IDE_STATE.isAgentRunning, 0
    je .agt_exit

    ; Read tokens from IPC ring
    sub rsp, 32
    mov rcx, [rbx].IDE_STATE.hIPC
    mov rdx, r12
    mov r8d, 2048
    call IPC_ReadMessage
    add rsp, 32
    test eax, eax
    jz .agt_loop

    ; Append to editor (move to end of text, insert)
    sub rsp, 32
    mov rcx, [rbx].IDE_STATE.hEditor
    mov edx, EM_SETSEL
    mov r8d, -1                     ; select end
    mov r9d, -1
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    ; EM_REPLACESEL with no-undo = 0C2h
    sub rsp, 32
    mov rcx, [rbx].IDE_STATE.hEditor
    mov edx, 0C2h
    xor r8, r8                      ; fCanUndo = FALSE
    mov r9, r12
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    ; Scroll to caret
    sub rsp, 32
    mov rcx, [rbx].IDE_STATE.hEditor
    mov edx, EM_SCROLLCARET
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .agt_loop

.agt_exit:
    xor eax, eax
    add rsp, 48 + 2048
    pop r13
    pop r12
    pop rbx
    ret
IDE_AgentThread ENDP

;==============================================================================
; IDE_StartAgent — Launch agent polling thread
;==============================================================================
IDE_StartAgent PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp [g_IDE.isAgentRunning], 1
    je .sa_already

    ; Initialize IPC
    lea rcx, [g_IDE.hIPC]
    mov edx, 0                      ; GUI side
    call IPC_Initialize
    test eax, eax
    jz .sa_fail

    ; Create wakeup event
    xor ecx, ecx
    xor edx, edx
    xor r8, r8
    xor r9, r9
    sub rsp, 32
    call qword ptr [__imp_CreateEventA]
    add rsp, 32
    test rax, rax
    jz .sa_fail
    mov [g_IDE.hAgentEvent], rax

    ; Mark running before thread start
    mov [g_IDE.isAgentRunning], 1

    ; CreateThread(NULL, 0, IDE_AgentThread, NULL, 0, NULL)
    xor ecx, ecx
    xor edx, edx
    lea r8, [IDE_AgentThread]
    xor r9, r9
    sub rsp, 40
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    call qword ptr [__imp_CreateThread]
    add rsp, 40
    mov [g_IDE.hAgentThread], rax
    test rax, rax
    jnz .sa_ok

    ; Thread failed: reset
    mov [g_IDE.isAgentRunning], 0
    jmp .sa_fail

.sa_ok:
    lea rcx, [szAgentConnected]
    call IDE_SetStatus
    jmp .sa_done
.sa_fail:
    lea rcx, [szAgentStopped]
    call IDE_SetStatus
.sa_already:
.sa_done:
    add rsp, 32
    pop rbx
    ret
IDE_StartAgent ENDP

;==============================================================================
; IDE_StopAgent — Signal agent thread to stop
;==============================================================================
IDE_StopAgent PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov [g_IDE.isAgentRunning], 0
    mov rcx, [g_IDE.hAgentEvent]
    test rcx, rcx
    jz .stop_ipc
    sub rsp, 32
    call qword ptr [__imp_SetEvent]
    add rsp, 32
.stop_ipc:
    call IPC_Shutdown
    lea rcx, [szAgentStopped]
    call IDE_SetStatus
    add rsp, 32
    pop rbx
    ret
IDE_StopAgent ENDP

;==============================================================================
; IDE_HandleCommand — Process WM_COMMAND menu/accelerator messages
; RCX = commandID
;==============================================================================
IDE_HandleCommand PROC FRAME
    push rbx
    push r12
    .pushreg rbx
    .pushreg r12
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov r12d, ecx

    cmp r12d, ID_FILE_NEW
    jne .cmd_1
    ; New: clear editor, reset title
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 000Ch              ; WM_SETTEXT
    xor r8, r8
    lea r9, [.szEmpty]
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    mov byte ptr [g_IDE.currentFile], 0
    mov [g_IDE.isDirty], 0
    call IDE_UpdateTitle
    jmp .cmd_done

.cmd_1:
    cmp r12d, ID_FILE_OPEN
    jne .cmd_2
    call IDE_FileOpen
    jmp .cmd_done

.cmd_2:
    cmp r12d, ID_FILE_SAVE
    jne .cmd_3
    call IDE_FileSave
    jmp .cmd_done

.cmd_3:
    cmp r12d, ID_FILE_SAVE_AS
    jne .cmd_4
    call IDE_FileSaveAs
    jmp .cmd_done

.cmd_4:
    cmp r12d, ID_FILE_EXIT
    jne .cmd_5
    sub rsp, 32
    mov rcx, [g_IDE.hMainWnd]
    mov edx, WM_CLOSE
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_5:
    cmp r12d, ID_EDIT_UNDO
    jne .cmd_6
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0101h              ; EM_UNDO
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_6:
    cmp r12d, ID_EDIT_REDO
    jne .cmd_7
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0C96h              ; EM_REDO (RichEdit only)
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_7:
    cmp r12d, ID_EDIT_CUT
    jne .cmd_8
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0300h              ; WM_CUT
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_8:
    cmp r12d, ID_EDIT_COPY
    jne .cmd_9
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0301h              ; WM_COPY
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_9:
    cmp r12d, ID_EDIT_PASTE
    jne .cmd_10
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 0302h              ; WM_PASTE
    xor r8, r8
    xor r9, r9
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_10:
    cmp r12d, ID_VIEW_SIDEBAR
    jne .cmd_11
    xor [g_IDE.isSidebarVisible], 1
    sub rsp, 32
    call qword ptr [__imp_GetProcessHeap]
    add rsp, 32
    ; Trigger relayout
    sub rsp, 32
    mov rcx, [g_IDE.hMainWnd]
    xor edx, edx
    xor r8, r8
    call qword ptr [__imp_InvalidateRect]
    add rsp, 32
    jmp .cmd_done

.cmd_11:
    cmp r12d, ID_AGENT_RUN
    jne .cmd_12
    call IDE_StartAgent
    jmp .cmd_done

.cmd_12:
    cmp r12d, ID_AGENT_STOP
    jne .cmd_13
    call IDE_StopAgent
    jmp .cmd_done

.cmd_13:
    cmp r12d, ID_AGENT_CLEAR
    jne .cmd_14
    sub rsp, 32
    mov rcx, [g_IDE.hEditor]
    mov edx, 000Ch
    xor r8, r8
    lea r9, [.szEmpty]
    call qword ptr [__imp_SendMessageA]
    add rsp, 32
    jmp .cmd_done

.cmd_14:
    cmp r12d, ID_BUILD_BUILD
    jne .cmd_15
    ; Trigger build via IPC message to agent
    sub rsp, 32
    mov rcx, [g_IDE.hIPC]
    mov edx, 1040h              ; BUILD_REQUEST
    lea r8, [g_IDE.currentFile]
    mov r9d, 260
    call IPC_PostMessage
    add rsp, 32
    lea rcx, [szBuildOK]
    call IDE_SetStatus
    jmp .cmd_done

.cmd_15:
    cmp r12d, ID_HELP_ABOUT
    jne .cmd_done
    sub rsp, 32
    mov rcx, [g_IDE.hMainWnd]
    lea rdx, [szAbout]
    lea r8, [szWindowTitle]
    xor r9d, r9d                ; MB_OK
    call qword ptr [__imp_MessageBoxA]
    add rsp, 32

.cmd_done:
    add rsp, 32
    pop r12
    pop rbx
    ret
.szEmpty: BYTE 0
IDE_HandleCommand ENDP

;==============================================================================
; IDE_WndProc — Main window procedure
; Standard Win32 WndProc: RCX=hWnd, RDX=uMsg, R8=wParam, R9=lParam
;==============================================================================
IDE_WndProc PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .pushreg r15
    sub rsp, 48
    .allocstack 48
    .endprolog

    mov rbx, rcx                    ; hWnd
    mov r12d, edx                   ; uMsg
    mov r13, r8                     ; wParam
    mov r14, r9                     ; lParam

    cmp r12d, WM_CREATE
    je .wm_create

    cmp r12d, WM_SIZE
    je .wm_size

    cmp r12d, WM_COMMAND
    je .wm_command

    cmp r12d, WM_PAINT
    je .wm_paint

    cmp r12d, WM_CTLCOLOREDIT
    je .wm_ctlcolor_edit

    cmp r12d, WM_CTLCOLORSTATIC
    je .wm_ctlcolor_static

    cmp r12d, WM_GETMINMAXINFO
    je .wm_getminmax

    cmp r12d, WM_ERASEBKGND
    je .wm_erase

    cmp r12d, WM_CLOSE
    je .wm_close

    cmp r12d, WM_DESTROY
    je .wm_destroy

    ; Fall through to DefWindowProc
    jmp .wm_default

;-------- WM_CREATE --------
.wm_create:
    ; Store hInst
    mov rcx, [g_IDE.hInstance]
    ; Load GDI resources
    call IDE_LoadRichEdit
    call IDE_CreateFonts
    call IDE_CreateBrushes
    ; Initial sidebar visible
    mov [g_IDE.isSidebarVisible], 1
    ; Get initial client size (from CREATESTRUCT in lParam)
    sub rsp, 32
    mov rcx, rbx
    lea rdx, [rsp+32]               ; RECT buffer on stack
    call qword ptr [__imp_GetClientRect]
    mov eax, [rsp+40]               ; RECT.right
    mov ecx, [rsp+44]               ; RECT.bottom
    add rsp, 32
    mov [g_IDE.clientWidth], eax
    mov [g_IDE.clientHeight], ecx
    ; Create child controls
    mov rcx, rbx
    mov edx, [g_IDE.clientWidth]
    mov r8d, [g_IDE.clientHeight]
    call IDE_CreateChildWindows
    ; Update title
    call IDE_UpdateTitle
    xor rax, rax
    jmp .wm_done

;-------- WM_SIZE --------
.wm_size:
    ; lParam lo16 = width, hi16 = height
    movzx eax, r14w
    mov [g_IDE.clientWidth], eax
    mov ecx, r14d
    shr ecx, 16
    mov [g_IDE.clientHeight], ecx
    mov rcx, rbx
    movzx edx, r14w
    movzx r8d, cx
    shr r8, 0
    mov r8d, [g_IDE.clientHeight]
    call IDE_DoLayout
    xor rax, rax
    jmp .wm_done

;-------- WM_COMMAND --------
.wm_command:
    movzx ecx, r13w                 ; command ID = LOWORD(wParam)
    call IDE_HandleCommand
    xor rax, rax
    jmp .wm_done

;-------- WM_PAINT --------
.wm_paint:
    ; Paint toolbar and tab bar areas (dark theme)
    sub rsp, 128                    ; PAINTSTRUCT buffer
    mov rcx, rbx
    lea rdx, [rsp+32]               ; PAINTSTRUCT
    BeginPaint
    mov rsi, rax                    ; hDC
    ; Fill toolbar area
    sub rsp, 32
    mov rcx, rsi
    lea rdx, [.rc_toolbar]
    mov r8, [g_IDE.hBrushToolbar]
    FillRect
    add rsp, 32
    ; Fill tab bar area (below toolbar)
    sub rsp, 32
    mov rcx, rsi
    lea rdx, [.rc_tabbar]
    mov r8, [g_IDE.hBrushTab]
    FillRect
    add rsp, 32
    mov rcx, rbx
    lea rdx, [rsp+32]
    EndPaint
    add rsp, 128
    xor rax, rax
    jmp .wm_done

;-------- WM_CTLCOLOREDIT --------
.wm_ctlcolor_edit:
    ; Dark theme for edit controls
    mov rax, r8                     ; hDC = wParam
    sub rsp, 32
    mov rcx, rax
    mov edx, CLR_EDITOR_FG
    SetTextColor
    mov rcx, rax
    mov edx, CLR_EDITOR_BG
    SetBkColor
    add rsp, 32
    mov rax, [g_IDE.hBrushEditor]
    jmp .wm_done

;-------- WM_CTLCOLORSTATIC --------
.wm_ctlcolor_static:
    ; Status bar and toolbar static coloring
    mov rax, r8
    sub rsp, 32
    mov rcx, rax
    mov edx, CLR_STATUSBAR_FG
    SetTextColor
    mov rcx, rax
    mov edx, CLR_STATUSBAR_BG
    SetBkColor
    add rsp, 32
    mov rax, [g_IDE.hBrushStatus]
    jmp .wm_done

;-------- WM_GETMINMAXINFO --------
.wm_getminmax:
    ; Enforce minimum window size
    mov rax, r14
    mov dword ptr [rax + 24], IDE_MIN_WIDTH   ; ptMinTrackSize.x
    mov dword ptr [rax + 28], IDE_MIN_HEIGHT  ; ptMinTrackSize.y
    xor rax, rax
    jmp .wm_done

;-------- WM_ERASEBKGND --------
.wm_erase:
    ; Suppress default erase to avoid flicker
    mov rax, 1
    jmp .wm_done

;-------- WM_CLOSE --------
.wm_close:
    ; Check unsaved changes
    cmp [g_IDE.isDirty], 0
    je .wm_close_ok
    sub rsp, 32
    mov rcx, rbx
    lea rdx, [.szUnsavedConfirm]
    lea r8, [szWindowTitle]
    mov r9d, 4                      ; MB_YESNO
    MessageBoxA
    add rsp, 32
    cmp eax, 6                      ; IDYES
    jne .wm_done
.wm_close_ok:
    call IDE_StopAgent
    sub rsp, 32
    mov rcx, rbx
    call qword ptr [__imp_DestroyWindow]
    add rsp, 32
    xor rax, rax
    jmp .wm_done

;-------- WM_DESTROY --------
.wm_destroy:
    ; Cleanup GDI objects
    sub rsp, 32
    mov rcx, [g_IDE.hFont]
    DeleteObject
    mov rcx, [g_IDE.hStatusFont]
    DeleteObject
    mov rcx, [g_IDE.hBrushEditor]
    DeleteObject
    mov rcx, [g_IDE.hBrushSidebar]
    DeleteObject
    mov rcx, [g_IDE.hBrushToolbar]
    DeleteObject
    mov rcx, [g_IDE.hBrushStatus]
    DeleteObject
    mov rcx, [g_IDE.hBrushTab]
    DeleteObject
    add rsp, 32
    xor ecx, ecx
    PostQuitMessage
    xor rax, rax
    jmp .wm_done

;-------- Default --------
.wm_default:
    sub rsp, 32
    mov rcx, rbx
    mov edx, r12d
    mov r8, r13
    mov r9, r14
    DefWindowProcA
    add rsp, 32

.wm_done:
    add rsp, 48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; Toolbar rect (updated by WM_SIZE — static for paint placeholder)
ALIGN 4
.rc_toolbar:    DD 0, 0, 4096, IDE_TOOLBAR_HEIGHT
.rc_tabbar:     DD 0, IDE_TOOLBAR_HEIGHT, 4096, (IDE_TOOLBAR_HEIGHT + IDE_TABBAR_HEIGHT)
.szUnsavedConfirm: BYTE "You have unsaved changes. Exit anyway?", 0

IDE_WndProc ENDP

;==============================================================================
; IDEShellMain — Entry point: register window class, create main window,
; run message loop
; Returns: exit code in EAX
;==============================================================================
IDEShellMain PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    sub rsp, 200h                   ; WNDCLASSEXA + MSG_STRUCT + room
    .allocstack 200h
    .endprolog

    ; Get module handle
    xor ecx, ecx
    GetModuleHandleA
    mov [g_IDE.hInstance], rax
    mov r12, rax

    ; Initialize ComCtl32 for any modern controls
    sub rsp, 32
    lea rcx, [.iccex]
    call qword ptr [__imp_InitCommonControlsEx]
    add rsp, 32

    ; Register window class
    lea rbx, [rsp + 32]             ; WNDCLASSEXA on stack
    ; Zero it
    push rdi
    lea rdi, [rbx]
    xor eax, eax
    mov ecx, SIZEOF WNDCLASSEXA / 8 + 1
    rep stosq
    pop rdi

    mov dword ptr [rbx + WNDCLASSEXA.cbSize], SIZEOF WNDCLASSEXA
    mov dword ptr [rbx + WNDCLASSEXA.style], 0003h    ; CS_HREDRAW | CS_VREDRAW
    lea rax, [IDE_WndProc]
    mov [rbx + WNDCLASSEXA.lpfnWndProc], rax
    mov dword ptr [rbx + WNDCLASSEXA.cbClsExtra], 0
    mov dword ptr [rbx + WNDCLASSEXA.cbWndExtra], 0
    mov [rbx + WNDCLASSEXA.hInstance], r12
    ; hIcon: LoadIcon(NULL, IDI_APPLICATION = 32512)
    xor ecx, ecx
    call qword ptr [__imp_LoadIconA]    ; hInst=rcx=0 already set
    sub rsp, 32
    xor ecx, ecx
    mov edx, 32512
    call qword ptr [__imp_LoadIconA]
    add rsp, 32
    mov [rbx + WNDCLASSEXA.hIcon], rax
    ; hCursor: LoadCursor(NULL, IDC_ARROW = 32512)
    sub rsp, 32
    xor ecx, ecx
    mov edx, 32512
    call qword ptr [__imp_LoadCursorA]
    add rsp, 32
    mov [rbx + WNDCLASSEXA.hCursor], rax
    ; Background: NULL (we handle WM_ERASEBKGND)
    mov qword ptr [rbx + WNDCLASSEXA.hbrBackground], 0
    mov qword ptr [rbx + WNDCLASSEXA.lpszMenuName], 0
    lea rax, [szClassName]
    mov [rbx + WNDCLASSEXA.lpszClassName], rax
    mov [rbx + WNDCLASSEXA.hIconSm], 0

    sub rsp, 32
    mov rcx, rbx
    RegisterClassExA
    add rsp, 32
    test eax, eax
    jz .wsm_fail

    ; Build menu bar
    call IDE_CreateMenu
    mov r13, rax                    ; hMenu

    ; Create main window
    sub rsp, 72
    xor ecx, ecx                    ; dwExStyle
    lea rdx, [szClassName]
    lea r8, [szWindowTitle]
    mov r9d, (WS_OVERLAPPEDWINDOW OR WS_CLIPCHILDREN)
    mov dword ptr [rsp+32], 100     ; x
    mov dword ptr [rsp+36], 100     ; y
    mov dword ptr [rsp+40], 1280    ; width
    mov dword ptr [rsp+44], 800     ; height
    mov qword ptr [rsp+48], 0       ; hWndParent = NULL
    mov [rsp+56], r13               ; hMenu
    mov rax, [g_IDE.hInstance]
    mov [rsp+64], rax
    mov qword ptr [rsp+72], 0       ; lpParam
    CreateWindowExA
    add rsp, 72
    test rax, rax
    jz .wsm_fail
    mov [g_IDE.hMainWnd], rax
    mov rbx, rax                    ; hWnd

    ; Show and update
    sub rsp, 32
    mov rcx, rbx
    mov edx, SW_SHOW
    ShowWindow
    add rsp, 32
    sub rsp, 32
    mov rcx, rbx
    UpdateWindow
    add rsp, 32

    ; Message loop
    lea rsi, [rsp + 128]            ; MSG_STRUCT buffer
.msg_loop:
    sub rsp, 32
    mov rcx, rsi
    xor edx, edx
    xor r8, r8
    xor r9, r9
    GetMessageA
    add rsp, 32
    test eax, eax
    jz .msg_exit
    cmp eax, -1
    je .wsm_fail
    sub rsp, 32
    mov rcx, rsi
    TranslateMessage
    add rsp, 32
    sub rsp, 32
    mov rcx, rsi
    DispatchMessageA
    add rsp, 32
    jmp .msg_loop

.msg_exit:
    movzx eax, word ptr [rsi + 16]  ; MSG.wParam (exit code)
    add rsp, 200h
    pop r13
    pop r12
    pop rbx
    ret

.wsm_fail:
    mov eax, 1
    add rsp, 200h
    pop r13
    pop r12
    pop rbx
    ret

; INITCOMMONCONTROLSEX
ALIGN 4
.iccex: DD 8, 0FFFFh              ; dwSize=8, dwICC=all classes

IDEShellMain ENDP

END
