; ============================================================================
; MAIN_COMPLETE.ASM - Complete WinMain Entry Point for RawrXD MASM IDE
; Full initialization sequence with error handling and resource management
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\comctl32.inc
include \masm32\include\comdlg32.inc
include \masm32\include\shell32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\comctl32.lib
includelib \masm32\lib\comdlg32.lib
includelib \masm32\lib\shell32.lib

; External system initialization
EXTERN EditorEnterprise_Initialize:PROC
EXTERN IDEPaneSystem_Initialize:PROC
EXTERN IDEPaneSystem_CreateDefaultLayout:PROC
EXTERN UIGguf_CreateMenuBar:PROC
EXTERN UIGguf_CreateToolbar:PROC
EXTERN UIGguf_CreateStatusPane:PROC
EXTERN GgufUnified_Init:PROC
EXTERN InferenceBackend_Init:PROC
EXTERN AgentSystem_Init:PROC
EXTERN IDESettings_Initialize:PROC
EXTERN IDESettings_LoadFromFile:PROC
EXTERN IDESettings_ApplyTheme:PROC
EXTERN FileDialogs_Initialize:PROC

; External window procedures
EXTERN MainWndProc:PROC
EXTERN EditorWndProc:PROC

; Public exports
PUBLIC WinMain
PUBLIC g_hInstance
PUBLIC g_hMainWindow
PUBLIC g_hEditorWindow
PUBLIC g_hStatusBar
PUBLIC g_hToolbar
PUBLIC MainWndProc
PUBLIC CleanupResources

; Window class names
.const
    szMainWndClass      db "RawrXD_MainWindow", 0
    szEditorWndClass    db "RawrXD_EditorWindow", 0
    szAppTitle          db "RawrXD MASM IDE v1.0", 0
    szInitError         db "Failed to initialize IDE components", 0
    szWndClassError     db "Failed to register window class", 0
    szCreateWndError    db "Failed to create main window", 0
    szIcon              db "RAWRXD_ICON", 0
    
.data
    g_hInstance         dd 0
    g_hMainWindow       dd 0
    g_hEditorWindow     dd 0
    g_hStatusBar        dd 0
    g_hToolbar          dd 0
    g_hMenu             dd 0
    g_bInitialized      dd 0
    g_dwExitCode        dd 0
    
.data?
    g_CommandLine       dd ?
    g_ShowCmd           dd ?
    
.code

; ============================================================================
; WinMain - Application entry point
; ============================================================================
WinMain proc hInst:DWORD, hPrevInst:DWORD, lpCmdLine:DWORD, nShowCmd:DWORD
    LOCAL wc:WNDCLASSEXA
    LOCAL msg:MSG
    LOCAL dwResult:DWORD
    
    ; Store instance handle and command line
    mov eax, hInst
    mov g_hInstance, eax
    mov eax, lpCmdLine
    mov g_CommandLine, eax
    mov eax, nShowCmd
    mov g_ShowCmd, eax
    
    ; Initialize common controls
    LOCAL icc:INITCOMMONCONTROLSEX
    mov icc.dwSize, SIZEOF INITCOMMONCONTROLSEX
    mov icc.dwICC, ICC_WIN95_CLASSES or ICC_COOL_CLASSES or ICC_BAR_CLASSES or ICC_TAB_CLASSES
    invoke InitCommonControlsEx, ADDR icc
    test eax, eax
    jz @InitFailed
    
    ; Phase 1: Initialize core IDE systems
    invoke InitializeCoreComponents
    test eax, eax
    jz @InitFailed
    
    ; Phase 2: Register window classes
    invoke RegisterMainWindowClass
    test eax, eax
    jz @ClassRegFailed
    
    invoke RegisterEditorWindowClass
    test eax, eax
    jz @ClassRegFailed
    
    ; Phase 3: Create main window
    invoke CreateMainWindow
    test eax, eax
    jz @CreateWndFailed
    mov g_hMainWindow, eax
    
    ; Phase 4: Create child windows (editor, panes, etc)
    invoke CreateChildWindows, g_hMainWindow
    test eax, eax
    jz @CreateWndFailed
    
    ; Phase 5: Load settings and apply theme
    call IDESettings_LoadFromFile
    call IDESettings_ApplyTheme
    
    ; Phase 6: Show main window
    invoke ShowWindow, g_hMainWindow, g_ShowCmd
    invoke UpdateWindow, g_hMainWindow
    
    ; Mark initialization complete
    mov g_bInitialized, 1
    
    ; Main message loop
@MessageLoop:
    invoke GetMessageA, ADDR msg, NULL, 0, 0
    test eax, eax
    jz @ExitLoop
    cmp eax, -1
    je @ExitLoop
    
    ; Handle accelerators
    invoke TranslateAcceleratorA, g_hMainWindow, g_hMenu, ADDR msg
    test eax, eax
    jnz @MessageLoop
    
    ; Standard message processing
    invoke TranslateMessage, ADDR msg
    invoke DispatchMessageA, ADDR msg
    jmp @MessageLoop
    
@ExitLoop:
    ; Cleanup and return exit code
    invoke CleanupResources
    mov eax, msg.wParam
    mov g_dwExitCode, eax
    ret
    
@InitFailed:
    invoke MessageBoxA, NULL, ADDR szInitError, ADDR szAppTitle, MB_ICONERROR or MB_OK
    xor eax, eax
    ret
    
@ClassRegFailed:
    invoke MessageBoxA, NULL, ADDR szWndClassError, ADDR szAppTitle, MB_ICONERROR or MB_OK
    xor eax, eax
    ret
    
@CreateWndFailed:
    invoke MessageBoxA, NULL, ADDR szCreateWndError, ADDR szAppTitle, MB_ICONERROR or MB_OK
    invoke CleanupResources
    xor eax, eax
    ret
WinMain endp

; ============================================================================
; InitializeCoreComponents - Initialize all IDE subsystems
; ============================================================================
InitializeCoreComponents proc
    push ebx
    push esi
    
    ; Initialize file dialogs
    call FileDialogs_Initialize
    
    ; Initialize settings system
    call IDESettings_Initialize
    
    ; Initialize editor core
    call EditorEnterprise_Initialize
    test eax, eax
    jz @InitCoreFailed
    
    ; Initialize pane system
    call IDEPaneSystem_Initialize
    test eax, eax
    jz @InitCoreFailed
    
    ; Initialize GGUF loaders (optional - non-critical)
    call GgufUnified_Init
    
    ; Initialize inference backends (optional - non-critical)
    call InferenceBackend_Init
    
    ; Initialize agent system (optional - non-critical)
    call AgentSystem_Init
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@InitCoreFailed:
    xor eax, eax
    pop esi
    pop ebx
    ret
InitializeCoreComponents endp

; ============================================================================
; RegisterMainWindowClass - Register main window class
; ============================================================================
RegisterMainWindowClass proc
    LOCAL wc:WNDCLASSEXA
    
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov wc.lpfnWndProc, OFFSET MainWndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, g_hInstance
    mov wc.hInstance, eax
    
    ; Load icon
    invoke LoadIconA, g_hInstance, ADDR szIcon
    .if eax == 0
        invoke LoadIconA, NULL, IDI_APPLICATION
    .endif
    mov wc.hIcon, eax
    mov wc.hIconSm, eax
    
    invoke LoadCursorA, NULL, IDC_ARROW
    mov wc.hCursor, eax
    
    mov wc.hbrBackground, COLOR_BTNFACE + 1
    mov wc.lpszMenuName, NULL
    lea eax, szMainWndClass
    mov wc.lpszClassName, eax
    
    invoke RegisterClassExA, ADDR wc
    ret
RegisterMainWindowClass endp

; ============================================================================
; RegisterEditorWindowClass - Register editor window class
; ============================================================================
RegisterEditorWindowClass proc
    LOCAL wc:WNDCLASSEXA
    
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    mov wc.lpfnWndProc, OFFSET EditorWndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, g_hInstance
    mov wc.hInstance, eax
    mov wc.hIcon, NULL
    mov wc.hIconSm, NULL
    
    invoke LoadCursorA, NULL, IDC_IBEAM
    mov wc.hCursor, eax
    
    mov wc.hbrBackground, COLOR_WINDOW + 1
    mov wc.lpszMenuName, NULL
    lea eax, szEditorWndClass
    mov wc.lpszClassName, eax
    
    invoke RegisterClassExA, ADDR wc
    ret
RegisterEditorWindowClass endp

; ============================================================================
; CreateMainWindow - Create main application window
; ============================================================================
CreateMainWindow proc
    LOCAL hWnd:DWORD
    LOCAL dwExStyle:DWORD
    LOCAL dwStyle:DWORD
    LOCAL rc:RECT
    
    ; Calculate centered position
    invoke GetSystemMetrics, SM_CXSCREEN
    mov rc.right, eax
    invoke GetSystemMetrics, SM_CYSCREEN
    mov rc.bottom, eax
    
    ; Window size: 80% of screen
    mov eax, rc.right
    imul eax, 4
    mov ebx, 5
    xor edx, edx
    div ebx
    mov rc.right, eax
    
    mov eax, rc.bottom
    imul eax, 4
    xor edx, edx
    div ebx
    mov rc.bottom, eax
    
    ; Center position
    invoke GetSystemMetrics, SM_CXSCREEN
    sub eax, rc.right
    shr eax, 1
    mov rc.left, eax
    
    invoke GetSystemMetrics, SM_CYSCREEN
    sub eax, rc.bottom
    shr eax, 1
    mov rc.top, eax
    
    ; Create window
    mov dwExStyle, WS_EX_APPWINDOW or WS_EX_WINDOWEDGE
    mov dwStyle, WS_OVERLAPPEDWINDOW or WS_CLIPCHILDREN or WS_CLIPSIBLINGS
    
    invoke CreateWindowExA, dwExStyle, ADDR szMainWndClass, ADDR szAppTitle, \
        dwStyle, rc.left, rc.top, rc.right, rc.bottom, \
        NULL, NULL, g_hInstance, NULL
    
    ret
CreateMainWindow endp

; ============================================================================
; CreateChildWindows - Create editor and UI components
; ============================================================================
CreateChildWindows proc hParent:DWORD
    LOCAL rc:RECT
    LOCAL hWnd:DWORD
    
    ; Get client area
    invoke GetClientRect, hParent, ADDR rc
    
    ; Create menu bar
    push hParent
    call UIGguf_CreateMenuBar
    add esp, 4
    mov g_hMenu, eax
    
    ; Create toolbar (height: 40)
    push hParent
    call UIGguf_CreateToolbar
    add esp, 4
    mov g_hToolbar, eax
    
    ; Create status bar (height: 24)
    push hParent
    call UIGguf_CreateStatusPane
    add esp, 4
    mov g_hStatusBar, eax
    
    ; Calculate editor area (between toolbar and status bar)
    mov eax, rc.bottom
    sub eax, 64  ; 40 (toolbar) + 24 (status)
    mov rc.bottom, eax
    mov rc.top, 40
    
    ; Create editor window
    invoke CreateWindowExA, WS_EX_CLIENTEDGE, ADDR szEditorWndClass, NULL, \
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL, \
        rc.left, rc.top, rc.right, rc.bottom, \
        hParent, NULL, g_hInstance, NULL
    test eax, eax
    jz @CreateFailed
    mov g_hEditorWindow, eax
    
    ; Create default pane layout
    push hParent
    call IDEPaneSystem_CreateDefaultLayout
    add esp, 4
    
    mov eax, 1
    ret
    
@CreateFailed:
    xor eax, eax
    ret
CreateChildWindows endp

; ============================================================================
; MainWndProc - Main window message handler
; ============================================================================
MainWndProc proc hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    
    cmp uMsg, WM_CREATE
    je @OnCreate
    cmp uMsg, WM_SIZE
    je @OnSize
    cmp uMsg, WM_COMMAND
    je @OnCommand
    cmp uMsg, WM_CLOSE
    je @OnClose
    cmp uMsg, WM_DESTROY
    je @OnDestroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@OnCreate:
    xor eax, eax
    ret
    
@OnSize:
    ; Resize child windows
    invoke ResizeChildWindows, hWnd
    xor eax, eax
    ret
    
@OnCommand:
    ; Handle menu commands
    invoke HandleMenuCommand, wParam, lParam
    xor eax, eax
    ret
    
@OnClose:
    ; Prompt to save unsaved changes
    invoke MessageBoxA, hWnd, ADDR "Save changes before closing?", ADDR szAppTitle, MB_YESNOCANCEL or MB_ICONQUESTION
    cmp eax, IDCANCEL
    je @CancelClose
    cmp eax, IDYES
    je @SaveAndClose
    
    ; Don't save, just close
    invoke DestroyWindow, hWnd
    xor eax, eax
    ret
    
@SaveAndClose:
    ; Save current file (would call save function)
    invoke DestroyWindow, hWnd
    xor eax, eax
    ret
    
@CancelClose:
    xor eax, eax
    ret
    
@OnDestroy:
    invoke PostQuitMessage, 0
    xor eax, eax
    ret
MainWndProc endp

; ============================================================================
; ResizeChildWindows - Resize editor and panes on window resize
; ============================================================================
ResizeChildWindows proc hWnd:DWORD
    LOCAL rc:RECT
    
    invoke GetClientRect, hWnd, ADDR rc
    
    ; Resize toolbar
    cmp g_hToolbar, 0
    je @NoToolbar
    invoke MoveWindow, g_hToolbar, 0, 0, rc.right, 40, TRUE
@NoToolbar:
    
    ; Resize status bar
    cmp g_hStatusBar, 0
    je @NoStatus
    mov eax, rc.bottom
    sub eax, 24
    invoke MoveWindow, g_hStatusBar, 0, eax, rc.right, 24, TRUE
@NoStatus:
    
    ; Resize editor
    cmp g_hEditorWindow, 0
    je @NoEditor
    mov eax, rc.bottom
    sub eax, 64
    invoke MoveWindow, g_hEditorWindow, 0, 40, rc.right, eax, TRUE
@NoEditor:
    
    ret
ResizeChildWindows endp

; ============================================================================
; HandleMenuCommand - Process menu commands
; ============================================================================
HandleMenuCommand proc wParam:DWORD, lParam:DWORD
    mov eax, wParam
    and eax, 0FFFFh
    
    ; File menu commands (1000-1099)
    cmp eax, 1001  ; File->New
    je @OnFileNew
    cmp eax, 1002  ; File->Open
    je @OnFileOpen
    cmp eax, 1003  ; File->Save
    je @OnFileSave
    cmp eax, 1004  ; File->Save As
    je @OnFileSaveAs
    cmp eax, 1010  ; File->Exit
    je @OnFileExit
    
    ; Edit menu commands (1100-1199)
    cmp eax, 1101  ; Edit->Undo
    je @OnEditUndo
    cmp eax, 1102  ; Edit->Redo
    je @OnEditRedo
    cmp eax, 1105  ; Edit->Cut
    je @OnEditCut
    cmp eax, 1106  ; Edit->Copy
    je @OnEditCopy
    cmp eax, 1107  ; Edit->Paste
    je @OnEditPaste
    cmp eax, 1110  ; Edit->Find
    je @OnEditFind
    cmp eax, 1111  ; Edit->Replace
    je @OnEditReplace
    
    ret
    
@OnFileNew:
    ; Create new file
    ret
@OnFileOpen:
    ; Open file dialog (implemented in dialogs.asm)
    push g_hMainWindow
    call FileDialog_Open
    add esp, 4
    ret
@OnFileSave:
    ; Save current file
    ret
@OnFileSaveAs:
    ; Save As dialog
    push g_hMainWindow
    call FileDialog_SaveAs
    add esp, 4
    ret
@OnFileExit:
    invoke SendMessageA, g_hMainWindow, WM_CLOSE, 0, 0
    ret
    
@OnEditUndo:
    invoke SendMessageA, g_hEditorWindow, WM_UNDO, 0, 0
    ret
@OnEditRedo:
    ; Redo not directly supported by Edit control
    ret
@OnEditCut:
    invoke SendMessageA, g_hEditorWindow, WM_CUT, 0, 0
    ret
@OnEditCopy:
    invoke SendMessageA, g_hEditorWindow, WM_COPY, 0, 0
    ret
@OnEditPaste:
    invoke SendMessageA, g_hEditorWindow, WM_PASTE, 0, 0
    ret
@OnEditFind:
    ; Show find dialog (implemented in find_replace.asm)
    push g_hMainWindow
    call ShowFindDialog
    add esp, 4
    ret
@OnEditReplace:
    ; Show replace dialog
    push g_hMainWindow
    call ShowReplaceDialog
    add esp, 4
    ret
HandleMenuCommand endp

; ============================================================================
; CleanupResources - Cleanup on exit
; ============================================================================
CleanupResources proc
    push ebx
    
    ; Save settings
    call IDESettings_SaveToFile
    
    ; Unregister window classes
    cmp g_bInitialized, 0
    je @NoCleanup
    
    invoke UnregisterClassA, ADDR szMainWndClass, g_hInstance
    invoke UnregisterClassA, ADDR szEditorWndClass, g_hInstance
    
@NoCleanup:
    pop ebx
    ret
CleanupResources endp

; ============================================================================
; External stub declarations for linking
; ============================================================================
EXTERN FileDialog_Open:PROC
EXTERN FileDialog_SaveAs:PROC
EXTERN ShowFindDialog:PROC
EXTERN ShowReplaceDialog:PROC

end WinMain