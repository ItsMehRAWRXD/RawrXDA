; ============================================================================
; phase2_integration.asm
; Phase 2 Component Integration for RawrXD Pure MASM IDE
;
; Integrates: Menu System + Theme System + File Browser
; Total Lines: Integration layer (~500 LOC) + 2,745 LOC from components
;
; Purpose:
; - Initialize all three Phase 2 systems during WM_CREATE
; - Wire menu commands to theme/browser actions
; - Apply themes to all UI elements
; - Position file browser in main window client area
; - Handle resize events for file browser layout
;
; Architecture:
;   Main Window (WM_CREATE)
;        ↓
;   ├─→ MenuBar_Create()
;   ├─→ ThemeManager_Init() + SetTheme(Dark)
;   └─→ FileBrowser_Create()
;        ↓
;   Main Window (WM_COMMAND)
;        ↓
;   MenuBar_HandleCommand()
;        ↓
;   ├─→ IDM_VIEW_THEME → ThemeManager_SetTheme() + ApplyTheme()
;   ├─→ IDM_FILE_NEW/OPEN → FileBrowser actions
;   └─→ IDM_VIEW_EXPLORER → Show/Hide FileBrowser
; ============================================================================

; x64 MASM - no includes, define externals directly
option casemap:none

; Type aliases
HWND            TEXTEQU <QWORD>
HMENU           TEXTEQU <QWORD>

; Win32 API externals
EXTERN SendMessageA:PROC
EXTERN PostMessageA:PROC
EXTERN GetClientRect:PROC
EXTERN MoveWindow:PROC
EXTERN ShowWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN UpdateWindow:PROC
EXTERN SetMenu:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN FillRect:PROC
EXTERN CreateSolidBrush:PROC
EXTERN DeleteObject:PROC

; Window messages
WM_SIZE         EQU 0005h
WM_PAINT        EQU 000Fh
WM_COMMAND      EQU 0111h

; ============================================================================
; EXTERNAL DECLARATIONS - Menu System
; ============================================================================
EXTERN MenuBar_Create: PROC
EXTERN MenuBar_Destroy: PROC
EXTERN MenuBar_HandleCommand: PROC
EXTERN MenuBar_EnableMenuItem: PROC

; ============================================================================
; EXTERNAL DECLARATIONS - Theme System
; ============================================================================
EXTERN ThemeManager_Init: PROC
EXTERN ThemeManager_Cleanup: PROC
EXTERN ThemeManager_SetTheme: PROC
EXTERN ThemeManager_GetColor: PROC
EXTERN ThemeManager_ApplyTheme: PROC
EXTERN ThemeManager_SaveTheme: PROC
EXTERN ThemeManager_LoadTheme: PROC

; ============================================================================
; EXTERNAL DECLARATIONS - File Browser
; ============================================================================
EXTERN FileBrowser_Create: PROC
EXTERN FileBrowser_Destroy: PROC
EXTERN FileBrowser_LoadDirectory: PROC
EXTERN FileBrowser_LoadDrives: PROC
EXTERN FileBrowser_GetSelectedPath: PROC
EXTERN FileBrowser_SetFilter: PROC
EXTERN FileBrowser_SortBy: PROC
EXTERN FileBrowser_Refresh: PROC

; ============================================================================
; CONSTANTS - Menu Command IDs (must match menu_system.asm)
; ============================================================================
IDM_FILE_NEW              EQU 1001
IDM_FILE_OPEN             EQU 1002
IDM_FILE_SAVE             EQU 1003
IDM_FILE_SAVE_AS          EQU 1004
IDM_FILE_CLOSE            EQU 1005
IDM_FILE_EXIT             EQU 1006

IDM_VIEW_EXPLORER         EQU 3001
IDM_VIEW_OUTPUT           EQU 3002
IDM_VIEW_TERMINAL         EQU 3003
IDM_VIEW_CHAT             EQU 3004
IDM_VIEW_THEME            EQU 3005
IDM_VIEW_FULLSCREEN       EQU 3006

; Theme IDs
THEME_DARK                EQU 0
THEME_LIGHT               EQU 1
THEME_HIGH_CONTRAST       EQU 2

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data

    ; Global Phase 2 state - using flat variables instead of struct
    hMainWindow         QWORD 0          ; Main window handle
    hMenuBar            QWORD 0          ; Menu bar handle
    hFileBrowser        QWORD 0          ; File browser window handle
    browserVisible      DWORD 1          ; 1=visible, 0=hidden
    currentTheme        DWORD 0          ; Current theme ID (0=Dark, 1=Light)
    initialized         DWORD 0          ; 1=all systems initialized
    
    ; String resources
    szThemeChanged  BYTE "Theme changed successfully", 0
    szError         BYTE "Error", 0
    szSuccess       BYTE "Success", 0
    
    ; Color constants (BGR format)
    colorDarkBg     DWORD 001E1E1EH    ; #1E1E1E
    colorLightBg    DWORD 00FFFFFFH    ; #FFFFFF

.code

; ============================================================================
; PUBLIC FUNCTION: Phase2_Initialize
;
; Purpose: Initialize all Phase 2 components
;
; Parameters:
;   rcx = hWnd (main window handle)
;
; Returns:
;   rax = 1 on success, 0 on failure
;
; Call from main window WM_CREATE handler
; ============================================================================

PUBLIC Phase2_Initialize
Phase2_Initialize PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rbx, rcx  ; Save hWnd
    
    ; Store main window handle
    mov gPhase2State.hMainWindow, rcx
    
    ; 1. Initialize Theme Manager
    call ThemeManager_Init
    test rax, rax
    jz Phase2_Init_Failed
    
    ; 2. Set default theme (Dark)
    mov ecx, THEME_DARK
    call ThemeManager_SetTheme
    test rax, rax
    jz Phase2_Init_Failed
    
    mov gPhase2State.currentTheme, THEME_DARK
    
    ; 3. Create menu bar
    mov rcx, rbx  ; hWnd
    call MenuBar_Create
    test rax, rax
    jz Phase2_Init_Failed
    
    mov gPhase2State.hMenuBar, rax
    
    ; Set menu bar to main window
    mov rcx, rbx  ; hWnd
    mov rdx, rax  ; hMenu
    call SetMenu
    
    ; 4. Create file browser
    mov rcx, rbx                    ; hWnd (parent)
    mov edx, 0                      ; x
    mov r8d, 0                      ; y
    mov r9d, 300                    ; width (will be resized on WM_SIZE)
    sub rsp, 32
    mov DWORD PTR [rsp+32], 600    ; height
    call FileBrowser_Create
    add rsp, 32
    
    test rax, rax
    jz Phase2_Init_Failed
    
    mov gPhase2State.hFileBrowser, rax
    
    ; 5. Load system drives in file browser
    mov rcx, rax  ; File browser handle
    call FileBrowser_LoadDrives
    
    ; 6. Apply theme to all windows
    mov rcx, rbx  ; Main window
    call ThemeManager_ApplyTheme
    
    mov rcx, gPhase2State.hFileBrowser
    test rcx, rcx
    jz Phase2_Init_SkipBrowser
    call ThemeManager_ApplyTheme
    
Phase2_Init_SkipBrowser:
    ; Mark as initialized
    mov gPhase2State.initialized, 1
    
    ; Success
    mov eax, 1
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
Phase2_Init_Failed:
    ; Cleanup partial initialization
    call Phase2_Cleanup
    
    xor eax, eax  ; Return 0 (failure)
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
Phase2_Initialize ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_Cleanup
;
; Purpose: Clean up all Phase 2 components
;
; Parameters: None
;
; Returns: None
;
; Call from main window WM_DESTROY handler
; ============================================================================

PUBLIC Phase2_Cleanup
Phase2_Cleanup PROC
    push rbx
    sub rsp, 32
    
    ; 1. Destroy file browser
    mov rcx, gPhase2State.hFileBrowser
    test rcx, rcx
    jz Phase2_Cleanup_SkipBrowser
    call FileBrowser_Destroy
    mov gPhase2State.hFileBrowser, 0
    
Phase2_Cleanup_SkipBrowser:
    ; 2. Destroy menu bar
    mov rcx, gPhase2State.hMenuBar
    test rcx, rcx
    jz Phase2_Cleanup_SkipMenu
    call MenuBar_Destroy
    mov gPhase2State.hMenuBar, 0
    
Phase2_Cleanup_SkipMenu:
    ; 3. Cleanup theme manager
    call ThemeManager_Cleanup
    
    ; Mark as not initialized
    mov gPhase2State.initialized, 0
    
    add rsp, 32
    pop rbx
    ret
Phase2_Cleanup ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_HandleCommand
;
; Purpose: Handle WM_COMMAND messages for Phase 2 systems
;
; Parameters:
;   rcx = command ID (LOWORD of wParam)
;
; Returns:
;   rax = 1 if command was handled, 0 if not
;
; Call from main window WM_COMMAND handler
; ============================================================================

PUBLIC Phase2_HandleCommand
Phase2_HandleCommand PROC
    push rbx
    push rdi
    sub rsp, 40
    
    mov ebx, ecx  ; Save command ID
    
    ; Check if initialized
    cmp gPhase2State.initialized, 0
    je Phase2_Cmd_NotHandled
    
    ; Switch on command ID
    cmp ebx, IDM_VIEW_THEME
    je Phase2_Cmd_ToggleTheme
    
    cmp ebx, IDM_VIEW_EXPLORER
    je Phase2_Cmd_ToggleExplorer
    
    cmp ebx, IDM_FILE_NEW
    je Phase2_Cmd_FileNew
    
    cmp ebx, IDM_FILE_OPEN
    je Phase2_Cmd_FileOpen
    
    ; Command not handled by Phase 2
    jmp Phase2_Cmd_NotHandled
    
Phase2_Cmd_ToggleTheme:
    ; Toggle between Dark and Light themes
    mov eax, gPhase2State.currentTheme
    test eax, eax  ; Is it Dark (0)?
    jz Phase2_Cmd_SetLight
    
    ; Currently Light, switch to Dark
    mov ecx, THEME_DARK
    call ThemeManager_SetTheme
    mov gPhase2State.currentTheme, THEME_DARK
    jmp Phase2_Cmd_ApplyTheme
    
Phase2_Cmd_SetLight:
    ; Currently Dark, switch to Light
    mov ecx, THEME_LIGHT
    call ThemeManager_SetTheme
    mov gPhase2State.currentTheme, THEME_LIGHT
    
Phase2_Cmd_ApplyTheme:
    ; Apply theme to all windows
    mov rcx, gPhase2State.hMainWindow
    call ThemeManager_ApplyTheme
    
    mov rcx, gPhase2State.hFileBrowser
    test rcx, rcx
    jz Phase2_Cmd_ThemeApplied
    call ThemeManager_ApplyTheme
    
Phase2_Cmd_ThemeApplied:
    ; Force redraw
    mov rcx, gPhase2State.hMainWindow
    mov edx, 1  ; bErase = TRUE
    call InvalidateRect
    
    mov eax, 1  ; Command handled
    add rsp, 40
    pop rdi
    pop rbx
    ret
    
Phase2_Cmd_ToggleExplorer:
    ; Toggle file browser visibility
    mov eax, gPhase2State.browserVisible
    test eax, eax
    jz Phase2_Cmd_ShowBrowser
    
    ; Currently visible, hide it
    mov rcx, gPhase2State.hFileBrowser
    mov edx, 0  ; SW_HIDE
    call ShowWindow
    mov gPhase2State.browserVisible, 0
    jmp Phase2_Cmd_ExplorerToggled
    
Phase2_Cmd_ShowBrowser:
    ; Currently hidden, show it
    mov rcx, gPhase2State.hFileBrowser
    mov edx, 5  ; SW_SHOW
    call ShowWindow
    mov gPhase2State.browserVisible, 1
    
Phase2_Cmd_ExplorerToggled:
    ; Trigger WM_SIZE to reposition browser
    mov rcx, gPhase2State.hMainWindow
    mov edx, 0  ; SIZE_RESTORED
    ; Get client rect
    lea r8, [rsp+32]  ; &RECT
    push rcx
    call GetClientRect
    pop rcx
    
    ; Extract width and height
    mov r9d, DWORD PTR [rsp+40]  ; right (width)
    mov eax, DWORD PTR [rsp+44]  ; bottom (height)
    shl rax, 32
    or r9, rax  ; lParam = MAKELONG(width, height)
    
    mov edx, 0  ; wParam = SIZE_RESTORED
    mov r8d, WM_SIZE
    xor r9d, r9d  ; Must recalculate
    call SendMessageA
    
    mov eax, 1  ; Command handled
    add rsp, 40
    pop rdi
    pop rbx
    ret
    
Phase2_Cmd_FileNew:
    ; Handle File->New (clear file browser selection)
    mov rcx, gPhase2State.hFileBrowser
    test rcx, rcx
    jz Phase2_Cmd_NotHandled
    
    ; Refresh file browser
    call FileBrowser_Refresh
    
    mov eax, 1  ; Command handled
    add rsp, 40
    pop rdi
    pop rbx
    ret
    
Phase2_Cmd_FileOpen:
    ; Handle File->Open (get selected file from browser)
    mov rcx, gPhase2State.hFileBrowser
    test rcx, rcx
    jz Phase2_Cmd_NotHandled
    
    ; Get selected path
    lea rdx, [rsp+32]  ; Buffer for path (260 bytes max)
    call FileBrowser_GetSelectedPath
    
    ; TODO: Open the file in editor (Phase 3 integration)
    ; For now, just return success
    
    mov eax, 1  ; Command handled
    add rsp, 40
    pop rdi
    pop rbx
    ret
    
Phase2_Cmd_NotHandled:
    xor eax, eax  ; Return 0 (not handled)
    add rsp, 40
    pop rdi
    pop rbx
    ret
Phase2_HandleCommand ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_HandleSize
;
; Purpose: Handle WM_SIZE to reposition file browser
;
; Parameters:
;   rcx = width (LOWORD of lParam)
;   rdx = height (HIWORD of lParam)
;
; Returns: None
;
; Call from main window WM_SIZE handler
; ============================================================================

PUBLIC Phase2_HandleSize
Phase2_HandleSize PROC
    push rbx
    push rdi
    sub rsp, 32
    
    mov ebx, ecx  ; width
    mov edi, edx  ; height
    
    ; Check if initialized and browser visible
    cmp gPhase2State.initialized, 0
    je Phase2_Size_Done
    
    cmp gPhase2State.browserVisible, 0
    je Phase2_Size_Done
    
    ; Position file browser on left side
    ; Left panel: 30% of width
    ; Layout: [FileBrowser 30%] [Editor Area 70%]
    
    ; Calculate browser width (30% of client width)
    mov eax, ebx
    imul eax, 30
    mov ecx, 100
    xor edx, edx
    div ecx  ; eax = width * 30 / 100
    
    ; Move file browser window
    mov rcx, gPhase2State.hFileBrowser
    mov edx, 0         ; x = 0
    mov r8d, 0         ; y = 0
    mov r9d, eax       ; width = 30%
    sub rsp, 32
    mov DWORD PTR [rsp+32], edi  ; height = client height
    mov DWORD PTR [rsp+40], 1    ; bRepaint = TRUE
    call MoveWindow
    add rsp, 32
    
Phase2_Size_Done:
    add rsp, 32
    pop rdi
    pop rbx
    ret
Phase2_HandleSize ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_HandlePaint
;
; Purpose: Handle WM_PAINT for custom drawing
;
; Parameters:
;   rcx = hWnd (main window handle)
;
; Returns: None
;
; Call from main window WM_PAINT handler (optional)
; ============================================================================

PUBLIC Phase2_HandlePaint
Phase2_HandlePaint PROC
    push rbx
    push rdi
    sub rsp, 56  ; PAINTSTRUCT (64 bytes)
    
    mov rbx, rcx  ; Save hWnd
    
    ; BeginPaint
    lea rdx, [rsp+32]  ; &PAINTSTRUCT
    call BeginPaint
    mov rdi, rax  ; hDC
    
    ; Get background color from theme
    xor ecx, ecx  ; Color index 0 (editorBackground)
    call ThemeManager_GetColor
    
    ; Create brush with theme color
    mov ecx, eax
    call CreateSolidBrush
    mov rcx, rax  ; hBrush
    
    ; Get client rect
    push rcx  ; Save brush
    mov rcx, rbx
    lea rdx, [rsp+40]  ; &RECT
    call GetClientRect
    pop rcx  ; Restore brush
    
    ; Fill rect with brush
    mov rdx, rdi  ; hDC
    lea r8, [rsp+32]  ; &RECT
    push rcx  ; hBrush
    call FillRect
    
    ; Delete brush
    pop rcx
    call DeleteObject
    
    ; EndPaint
    mov rcx, rbx
    lea rdx, [rsp+32]  ; &PAINTSTRUCT
    call EndPaint
    
    add rsp, 56
    pop rdi
    pop rbx
    ret
Phase2_HandlePaint ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_GetFileBrowserHandle
;
; Purpose: Get file browser window handle
;
; Parameters: None
;
; Returns:
;   rax = file browser HWND (or NULL if not created)
; ============================================================================

PUBLIC Phase2_GetFileBrowserHandle
Phase2_GetFileBrowserHandle PROC
    mov rax, gPhase2State.hFileBrowser
    ret
Phase2_GetFileBrowserHandle ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_GetMenuBarHandle
;
; Purpose: Get menu bar handle
;
; Parameters: None
;
; Returns:
;   rax = menu bar HMENU (or NULL if not created)
; ============================================================================

PUBLIC Phase2_GetMenuBarHandle
Phase2_GetMenuBarHandle PROC
    mov rax, gPhase2State.hMenuBar
    ret
Phase2_GetMenuBarHandle ENDP

; ============================================================================
; PUBLIC FUNCTION: Phase2_IsInitialized
;
; Purpose: Check if Phase 2 systems are initialized
;
; Parameters: None
;
; Returns:
;   rax = 1 if initialized, 0 otherwise
; ============================================================================

PUBLIC Phase2_IsInitialized
Phase2_IsInitialized PROC
    xor eax, eax
    mov al, BYTE PTR gPhase2State.initialized
    ret
Phase2_IsInitialized ENDP

END

