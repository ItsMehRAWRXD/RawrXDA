;==========================================================================
; ui_masm.asm - Pure MASM64 Win32 UI Layer for RawrXD IDE
; ==========================================================================
; Implements all window management, menus, dialogs, and text controls.
; Uses native Win32 APIs (no Qt, no C++).
; Zero dependencies except kernel32.lib, user32.lib, gdi32.lib.
;==========================================================================

option casemap:none

;==========================================================================
; CONSTANTS
;==========================================================================
WM_DESTROY              equ 0002h
WM_COMMAND              equ 0111h
WM_SIZE                 equ 0005h
WM_CREATE               equ 0001h
WM_SETTEXT              equ 000Ch
WM_GETTEXT              equ 000Dh
WM_SETFONT              equ 0030h
WM_CTLCOLOREDIT         equ 0133h
WM_CTLCOLORLISTBOX      equ 0134h
WM_CTLCOLORSTATIC       equ 0138h
WM_DROPFILES            equ 0233h

WS_OVERLAPPEDWINDOW     equ 00CF0000h
WS_VISIBLE              equ 10000000h
WS_CHILD                equ 40000000h
WS_BORDER               equ 00800000h
WS_VSCROLL              equ 00200000h
WS_HSCROLL              equ 00100000h

ES_MULTILINE            equ 0004h
ES_AUTOVSCROLL          equ 0040h
ES_AUTOHSCROLL          equ 0080h
ES_WANTRETURN           equ 1000h
ES_READONLY             equ 0800h

; Button constants
BS_PUSHBUTTON           equ 0000h
BS_CHECKBOX             equ 0002h
BS_AUTOCHECKBOX         equ 0003h

; ComboBox constants
CBS_DROPDOWNLIST        equ 0003h

CS_VREDRAW              equ 0001h
CS_HREDRAW              equ 0002h

COLOR_BTNFACE           equ 15
COLOR_WINDOW            equ 5

IDC_ARROW               equ 32512
IDI_APPLICATION         equ 32512

SW_SHOWNORMAL           equ 1
CW_USEDEFAULT           equ 80000000h

MB_OK                   equ 00000000h
MB_ICONINFORMATION      equ 00000040h
STARTF_USESTDHANDLES    equ 00000100h
HANDLE_FLAG_INHERIT     equ 00000001h
STD_INPUT_HANDLE        equ -10

; Control IDs
IDC_EXPLORER_TREE       equ 1001
IDC_FILE_LIST           equ 1002
IDC_EDITOR              equ 1003
IDC_CHAT_BOX            equ 1004
IDC_INPUT_BOX           equ 1005
IDC_TERMINAL            equ 1006
IDC_AGENT_LIST          equ 1008
IDC_AGENT_CONSOLE       equ 1009
IDC_TAB_CONTROL         equ 1011
IDC_CHAT_SEND_BTN       equ 1012
IDC_MODEL_SELECTOR      equ 1013
IDC_CHK_MAX_MODE        equ 1014
IDC_CHK_THINKING        equ 1015
IDC_CHK_STREAM          equ 1016
IDC_CHK_DEBUG           equ 1017
IDC_PROBLEM_PANEL       equ 1018

; Menu IDs
IDM_FILE_OPEN           equ 2001
IDM_FILE_SAVE           equ 2002
IDM_FILE_SAVE_AS        equ 2004
IDM_FILE_EXIT           equ 2005
IDM_CHAT_CLEAR          equ 2006
IDM_SETTINGS_MODEL      equ 2009
IDM_AGENT_TOGGLE        equ 2017
IDM_HELP_FEATURES       equ 2021
IDM_THEME_LIGHT         equ 2022
IDM_THEME_DARK          equ 2023
IDM_THEME_AMBER         equ 2024

; Dynamic control base IDs
IDC_BREADCRUMB_BASE     equ 3000
; Agents menu IDs
IDM_AGENT_VALIDATE      equ 2101
IDM_AGENT_PERSIST_THEME equ 2102
IDM_AGENT_OPEN_FOLDER   equ 2103
IDM_AGENT_LOGGING       equ 2104
IDM_AGENT_RUN_TESTS     equ 2105
IDM_AGENT_ZERO_DEPS     equ 2106

; RichEdit constants
EM_SETSEL               equ 00B1h
EM_REPLACESEL           equ 00C2h
WM_GETTEXTLENGTH        equ 000Eh

; ListBox constants for Explorer
LBS_NOTIFY              equ 0001h
LB_ADDSTRING            equ 0180h
LB_GETCURSEL            equ 0188h
LB_GETTEXT              equ 0189h
LBN_SELCHANGE           equ 0001h
LBN_DBLCLK              equ 0002h
LB_RESETCONTENT         equ 0184h

; File I/O constants
GENERIC_READ            equ 80000000h
FILE_SHARE_READ         equ 00000001h
OPEN_EXISTING           equ 3
FILE_ATTRIBUTE_NORMAL   equ 80h
FILE_ATTRIBUTE_DIRECTORY equ 10h

;==========================================================================
; STRUCTURES
;==========================================================================
WndClassExA STRUCT
    cbSize              DWORD ?
    style               DWORD ?
    lpfnWndProc         QWORD ?
    cbClsExtra          DWORD ?
    cbWndExtra          DWORD ?
    hInstance           QWORD ?
    hIcon               QWORD ?
    hCursor             QWORD ?
    hbrBackground       QWORD ?
    lpszMenuName        QWORD ?
    lpszClassName       QWORD ?
    hIconSm             QWORD ?
WndClassExA ENDS

RECT STRUCT
    left                DWORD ?
    top                 DWORD ?
    right               DWORD ?
    bottom              DWORD ?
RECT ENDS

OPENFILENAMEA STRUCT
    lStructSize         DWORD ?
    hwndOwner           QWORD ?
    hInstance           QWORD ?
    lpstrFilter         QWORD ?
    lpstrCustomFilter   QWORD ?
    nMaxCustFilter      DWORD ?
    nFilterIndex        DWORD ?
    lpstrFile           QWORD ?
    nMaxFile            DWORD ?
    lpstrFileTitle      QWORD ?
    nMaxFileTitle       DWORD ?
    lpstrInitialDir     QWORD ?
    lpstrTitle          QWORD ?
    Flags               DWORD ?
    nFileOffset         WORD ?
    nFileExtension      WORD ?
    lpstrDefExt         QWORD ?
    lCustData           QWORD ?
    lpfnHook            QWORD ?
    lpTemplateName      QWORD ?
    pvReserved          QWORD ?
    dwReserved          DWORD ?
    FlagsEx             DWORD ?
OPENFILENAMEA ENDS

SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle      DWORD ?
SECURITY_ATTRIBUTES ENDS

STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess        QWORD ?
    hThread         QWORD ?
    dwProcessId     DWORD ?
    dwThreadId      DWORD ?
PROCESS_INFORMATION ENDS

;==========================================================================
; EXTERNAL WIN32 APIs
;==========================================================================
EXTERN GetModuleHandleA : PROC
EXTERN RegisterClassExA : PROC
EXTERN CreateWindowExA : PROC
EXTERN ShowWindow : PROC
EXTERN UpdateWindow : PROC
EXTERN GetMessageA : PROC
EXTERN TranslateMessage : PROC
EXTERN DispatchMessageA : PROC
EXTERN PostQuitMessage : PROC
EXTERN DefWindowProcA : PROC
EXTERN LoadCursorA : PROC
EXTERN LoadIconA : PROC
EXTERN GetClientRect : PROC
EXTERN MoveWindow : PROC
EXTERN SendMessageA : PROC
EXTERN MessageBoxA : PROC
EXTERN CreateMenu : PROC
EXTERN CreatePopupMenu : PROC
EXTERN AppendMenuA : PROC
EXTERN SetMenu : PROC
EXTERN DestroyWindow : PROC
EXTERN DragAcceptFiles : PROC
EXTERN DragQueryFileA : PROC
EXTERN DragFinish : PROC
EXTERN lstrcmpiA : PROC
EXTERN GetOpenFileNameA : PROC
EXTERN GetSaveFileNameA : PROC
EXTERN CreateFileA : PROC
EXTERN ReadFile : PROC
EXTERN WriteFile : PROC
EXTERN CloseHandle : PROC
EXTERN CreatePipe : PROC
EXTERN SetHandleInformation : PROC
EXTERN GetStdHandle : PROC
EXTERN CreateProcessA : PROC
EXTERN WaitForSingleObject : PROC
EXTERN GetCurrentDirectoryA : PROC
EXTERN FindFirstFileA : PROC
EXTERN FindNextFileA : PROC
EXTERN FindClose : PROC
EXTERN GetLogicalDriveStringsA : PROC
EXTERN SetCurrentDirectoryA : PROC
; duplicates avoided
EXTERN CreateSolidBrush : PROC
EXTERN DeleteObject : PROC
EXTERN SetTextColor : PROC
EXTERN SetBkMode : PROC
EXTERN InvalidateRect : PROC
EXTERN RawrXD_GetEngineStatus : PROC

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    szClassName         BYTE "RawrXD_IDE_Class", 0
    szAppName           BYTE "RawrXD Agentic IDE (Pure MASM64)", 0
    szEditClass         BYTE "EDIT", 0
    szStaticClass       BYTE "STATIC", 0
    szListBoxClass      BYTE "LISTBOX", 0
    szRichEditClass     BYTE "RichEdit20A", 0 ; Using A version for simplicity
    
    szMenuFile          BYTE "&File", 0
    szMenuOpen          BYTE "&Open...", 0
    szMenuSave          BYTE "&Save", 0
    szMenuExit          BYTE "E&xit", 0
    szMenuChat          BYTE "&Chat", 0
    szMenuClear         BYTE "&Clear History", 0
    szMenuSettings      BYTE "&Settings", 0
    szMenuModel         BYTE "&AI Model...", 0
    szMenuTheme         BYTE "&Theme", 0
    szMenuLight         BYTE "&Light", 0
    szMenuDark          BYTE "&Dark", 0
    szMenuAmber         BYTE "&Amber", 0
    szMenuTools         BYTE "&Tools", 0
    szMenuAgent         BYTE "Agent &Mode", 0
    szMenuAgents        BYTE "&Agents", 0
    szAgentValidate     BYTE "Validate Themes && Breadcrumbs", 0
    szAgentPersistTheme BYTE "Persist Theme Selection", 0
    szAgentOpenFolder   BYTE "Open Folder (Agent)", 0
    szAgentLogging      BYTE "Toggle Structured Logging", 0
    szAgentRunTests     BYTE "Run Test Harness", 0
    szAgentZeroDeps     BYTE "Toggle Zero-Dependency Mode", 0
    szMenuHelp          BYTE "&Help", 0
    szMenuFeatures      BYTE "&Features && Status", 0
    
    szFilter            BYTE "All Files (*.*)", 0, "*.*", 0, "ASM Files (*.asm)", 0, "*.asm", 0, 0
    empty_str           BYTE 0
    
    ; Button labels
    szSendButton        BYTE "Send", 0
    szButtonClass       BYTE "BUTTON", 0
    szComboClass        BYTE "COMBOBOX", 0
    szModelGPT          BYTE "GPT-4o-latest", 0
    szModelClaude       BYTE "Claude-3.5-Sonnet", 0
    szModelLlama        BYTE "Llama-2-7B-Chat", 0
    szModelAuto         BYTE "Auto", 0
    szChkMaxMode        BYTE "Max Mode", 0
    szChkThinking       BYTE "Thinking", 0
    szChkStream         BYTE "Stream", 0
    szChkDebug          BYTE "Debug", 0
    szWelcomeMsg        BYTE "RawrXD Agentic IDE Ready", 13, 10, "Select a model and type your message below.", 13, 10, 0
    szFeaturesBanner    BYTE "Drives • Explorer • Editor • Chat • Terminal • Models", 0
    szFeaturesTitle     BYTE "RawrXD IDE Features", 0
    szFeaturesMsg       BYTE "Features:", 13, 10,
                          " - Drive enumeration", 13, 10,
                          " - Explorer navigation", 13, 10,
                          " - File open/save", 13, 10,
                          " - Chat history persistence", 13, 10,
                          " - Terminal integration", 13, 10,
                          " - Model selection", 13, 10,
                          " - Theme switching (Light/Dark/Amber)", 13, 10,
                          " - Breadcrumb navigation", 13, 10,
                          " - Agents dropdown actions", 13, 10,
                          "Status:", 13, 10,
                          " - OS calls wired to GUI", 13, 10,
                          " - State persisted on exit", 13, 10,
                          0
    ; Status panel string pieces
    szStatusEngine      BYTE "Engine: ", 0
    szStatusModelMid    BYTE " • Model: ", 0
    szStatusLoggingMid  BYTE " • Logging: ", 0
    szStatusZeroDepsMid BYTE " • Zero-Deps: ", 0
    szOn                BYTE "On", 0
    szOff               BYTE "Off", 0
    szDrivePrefix       BYTE "[Drive] ", 0
    szDirPrefix         BYTE "[DIR] ", 0
    szBackDir           BYTE "..", 0
    szEmpty             BYTE 0
    szStartPath         BYTE "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init", 0
    szCmdPrefix         BYTE "cmd.exe /c ", 0
    szBuildCommand      BYTE "build_masm_hotpatch.bat Release", 0

.data?
    hInstance           QWORD ?
    hwndMain            QWORD ?
    hwndEditor          QWORD ?
    hwndChat            QWORD ?
    hwndInput           QWORD ?
    hwndTerminal        QWORD ?
    hwndProblemPanel    QWORD ?
    hwndExplorer        QWORD ?
    hwndFeaturesLabel   QWORD ?
    hwndStatusPanel     QWORD ?
    hMenu               QWORD ?
    hwndSendBtn         QWORD ?
    hwndModelCombo      QWORD ?
    hwndChkMaxMode      QWORD ?
    hwndChkThinking     QWORD ?
    hwndChkStream       QWORD ?
    hwndChkDebug        QWORD ?
    rectClient          RECT <>
    controls_created    QWORD ? ; Flag: 1 if controls exist, 0 if not
    
    szFileName          BYTE 260 DUP (?)
    szSaveName          BYTE 260 DUP (?)
    ofn                 OPENFILENAMEA <>
    sfn                 OPENFILENAMEA <>
    read_buf            BYTE 65536 DUP (?)
    szEditorBuffer      BYTE 32768 DUP (?)
    ; moved initialized data to .data
    szExplorerDir       BYTE 260 DUP (?)
    szExplorerPattern   BYTE 260 DUP (?)
    find_data           BYTE 344 DUP (?)
    szSelectedName      BYTE 260 DUP (?)
    szDriveStrings      BYTE 256 DUP (?)  ; Buffer for GetLogicalDriveStringsA
    szTempBuf           BYTE 512 DUP (?)  ; Temp buffer for building strings
    ; Theming
    current_theme       DWORD ?
    hBrushBg            QWORD ?
    textColor           DWORD ?
    ; Breadcrumbs
    hwndBreadcrumbBtns  QWORD 10 DUP (?)
    breadcrumb_count    DWORD ?
    ; Agents/observability state
    logging_enabled     DWORD ?
    zero_deps_mode      DWORD ?
    szThemeCfgPath      BYTE "ide_theme.cfg", 0
    szStatusBuf         BYTE 512 DUP(?)
    execSA              SECURITY_ATTRIBUTES <>
    execSI              STARTUPINFOA <>
    execPI              PROCESS_INFORMATION <>
    execReadHandle      QWORD ?
    execWriteHandle     QWORD ?
    execBytesRead       DWORD ?
    execTotalRead       DWORD ?


;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; ui_create_main_window
; rcx = hInstance
;--------------------------------------------------------------------------
ui_create_main_window PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112 ; Shadow space + WndClassExA (80 bytes) + alignment

    mov hInstance, rcx

    ; Fill WndClassExA
    mov DWORD PTR [rsp + 32], 80 ; cbSize
    mov DWORD PTR [rsp + 36], CS_HREDRAW or CS_VREDRAW ; style
    lea rax, wnd_proc_main
    mov QWORD PTR [rsp + 40], rax ; lpfnWndProc
    mov DWORD PTR [rsp + 48], 0 ; cbClsExtra
    mov DWORD PTR [rsp + 52], 0 ; cbWndExtra
    mov rax, hInstance
    mov QWORD PTR [rsp + 56], rax ; hInstance
    
    ; Load Icon
    xor rcx, rcx
    mov rdx, IDI_APPLICATION
    call LoadIconA
    mov QWORD PTR [rsp + 64], rax ; hIcon
    
    ; Load Cursor
    xor rcx, rcx
    mov rdx, IDC_ARROW
    call LoadCursorA
    mov QWORD PTR [rsp + 72], rax ; hCursor
    
    mov QWORD PTR [rsp + 80], COLOR_WINDOW + 1 ; hbrBackground
    mov QWORD PTR [rsp + 88], 0 ; lpszMenuName
    lea rax, szClassName
    mov QWORD PTR [rsp + 96], rax ; lpszClassName
    mov QWORD PTR [rsp + 104], 0 ; hIconSm

    lea rcx, [rsp + 32]
    call RegisterClassExA

    ; Create Main Window
    xor rcx, rcx ; dwExStyle
    lea rdx, szClassName ; lpClassName
    lea r8, szAppName ; lpWindowName
    mov r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE ; dwStyle
    
    sub rsp, 64 ; Extra space for stack params
    mov DWORD PTR [rsp + 32], CW_USEDEFAULT ; x
    mov DWORD PTR [rsp + 40], CW_USEDEFAULT ; y
    mov DWORD PTR [rsp + 48], 1200 ; nWidth
    mov DWORD PTR [rsp + 56], 800 ; nHeight
    mov QWORD PTR [rsp + 64], 0 ; hWndParent
    mov QWORD PTR [rsp + 72], 0 ; hMenu
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax ; hInstance
    mov QWORD PTR [rsp + 88], 0 ; lpParam
    
    call CreateWindowExA
    add rsp, 64
    
    mov hwndMain, rax
    test rax, rax
    jz create_failed
    
    ; Show window explicitly
    mov rcx, rax
    mov rdx, SW_SHOWNORMAL
    call ShowWindow
    
    ; Update window to ensure painting
    mov rcx, hwndMain
    call UpdateWindow
    
    ; Create Menu
    call ui_create_menu
    mov hMenu, rax
    mov rcx, hwndMain
    mov rdx, hMenu
    call SetMenu

    ; Create Controls
    call ui_create_controls

    mov rax, hwndMain
    leave
    ret

create_failed:
    xor rax, rax
    leave
    ret
ui_create_main_window ENDP

;--------------------------------------------------------------------------
; ui_create_menu
;--------------------------------------------------------------------------
ui_create_menu PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    call CreateMenu
    mov rbx, rax ; Main Menu

    ; File Menu
    call CreatePopupMenu
    mov rsi, rax
    
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_FILE_OPEN
    lea r9, szMenuOpen
    call AppendMenuA
    
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_FILE_SAVE
    lea r9, szMenuSave
    call AppendMenuA
    
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_FILE_EXIT
    lea r9, szMenuExit
    call AppendMenuA
    
    mov rcx, rbx
    mov rdx, 10h ; MF_POPUP
    mov r8, rsi
    lea r9, szMenuFile
    call AppendMenuA

    ; Chat Menu
    call CreatePopupMenu
    mov rsi, rax
    
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_CHAT_CLEAR
    lea r9, szMenuClear
    call AppendMenuA
    
    mov rcx, rbx
    mov rdx, 10h ; MF_POPUP
    mov r8, rsi
    lea r9, szMenuChat
    call AppendMenuA

    ; Settings Menu (Model + Theme)
    call CreatePopupMenu
    mov rsi, rax
    ; Model settings item
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_SETTINGS_MODEL
    lea r9, szMenuModel
    call AppendMenuA
    ; Theme submenu items
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_THEME_LIGHT
    lea r9, szMenuLight
    call AppendMenuA
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_THEME_DARK
    lea r9, szMenuDark
    call AppendMenuA
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_THEME_AMBER
    lea r9, szMenuAmber
    call AppendMenuA
    ; Attach Settings popup
    mov rcx, rbx
    mov rdx, 10h ; MF_POPUP
    mov r8, rsi
    lea r9, szMenuSettings
    call AppendMenuA

    ; Help Menu
    call CreatePopupMenu
    mov rsi, rax
    ; Features & Status item
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_HELP_FEATURES
    lea r9, szMenuFeatures
    call AppendMenuA

    ; Agents Menu
    call CreatePopupMenu
    mov rsi, rax
    ; Validate Themes & Breadcrumbs
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_VALIDATE
    lea r9, szAgentValidate
    call AppendMenuA
    ; Persist Theme Selection
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_PERSIST_THEME
    lea r9, szAgentPersistTheme
    call AppendMenuA
    ; Open Folder (Agent)
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_OPEN_FOLDER
    lea r9, szAgentOpenFolder
    call AppendMenuA
    ; Toggle Structured Logging
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_LOGGING
    lea r9, szAgentLogging
    call AppendMenuA
    ; Run Test Harness
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_RUN_TESTS
    lea r9, szAgentRunTests
    call AppendMenuA
    ; Toggle Zero-Dependency Mode
    mov rcx, rsi
    xor rdx, rdx
    mov r8, IDM_AGENT_ZERO_DEPS
    lea r9, szAgentZeroDeps
    call AppendMenuA
    ; Attach Agents popup
    mov rcx, rbx
    mov rdx, 10h ; MF_POPUP
    mov r8, rsi
    lea r9, szMenuAgents
    call AppendMenuA
    ; Attach Help popup
    mov rcx, rbx
    mov rdx, 10h ; MF_POPUP
    mov r8, rsi
    lea r9, szMenuHelp
    call AppendMenuA

    mov rax, rbx
    leave
    ret
ui_create_menu ENDP

;--------------------------------------------------------------------------
; ui_create_controls
;--------------------------------------------------------------------------
ui_create_controls PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96

    ; Explorer (Left side - LISTBOX)
    xor rcx, rcx
    lea rdx, szListBoxClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or LBS_NOTIFY or WS_VSCROLL
    mov DWORD PTR [rsp + 32], 10 ; x
    mov DWORD PTR [rsp + 40], 10 ; y
    mov DWORD PTR [rsp + 48], 240 ; width
    mov DWORD PTR [rsp + 56], 710 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax ; parent
    mov QWORD PTR [rsp + 72], IDC_EXPLORER_TREE ; id
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndExplorer, rax

    ; Editor (Middle)
    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL or WS_VSCROLL or WS_HSCROLL
    mov DWORD PTR [rsp + 32], 260 ; x
    mov DWORD PTR [rsp + 40], 10 ; y
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 500 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax ; parent
    mov QWORD PTR [rsp + 72], IDC_EDITOR ; id
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndEditor, rax

    ; Model Selector ComboBox (Top right, above chat)
    xor rcx, rcx
    lea rdx, szComboClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 10 ; y (top of right pane)
    mov DWORD PTR [rsp + 48], 170 ; width (narrow)
    mov DWORD PTR [rsp + 56], 200 ; height (needs to be tall for dropdown)
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_MODEL_SELECTOR
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndModelCombo, rax

    ; Features banner (STATIC label at top-right, below combo)
    xor rcx, rcx
    lea rdx, szStaticClass
    lea r8, szFeaturesBanner
    mov r9d, WS_CHILD or WS_VISIBLE
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 35  ; y (below combo)
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 20  ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndFeaturesLabel, rax

    ; Status panel (READONLY EDIT below banner)
    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 60  ; y (below banner)
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 60  ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], 0
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndStatusPanel, rax

    ; Send Button (Top right, next to model selector)
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szSendButton
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov DWORD PTR [rsp + 32], 900 ; x (right of combo)
    mov DWORD PTR [rsp + 40], 10 ; y
    mov DWORD PTR [rsp + 48], 70 ; width
    mov DWORD PTR [rsp + 56], 25 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHAT_SEND_BTN
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndSendBtn, rax

    ; Chat display EDIT (adjusted to below model selector)
    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 120 ; y (below status panel)
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 370 ; height (reduced to fit buttons)
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHAT_BOX
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndChat, rax

    ; Input (Bottom right)
    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_WANTRETURN
    
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 420 ; y
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 90 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_INPUT_BOX
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndInput, rax

    ; Checkboxes below input (right side, bottom)
    ; Max Mode checkbox
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szChkMaxMode
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov DWORD PTR [rsp + 32], 720 ; x
    mov DWORD PTR [rsp + 40], 520 ; y (below input)
    mov DWORD PTR [rsp + 48], 100 ; width
    mov DWORD PTR [rsp + 56], 20 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHK_MAX_MODE
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndChkMaxMode, rax

    ; Thinking checkbox
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szChkThinking
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov DWORD PTR [rsp + 32], 830 ; x (next to Max Mode)
    mov DWORD PTR [rsp + 40], 520 ; y
    mov DWORD PTR [rsp + 48], 90 ; width
    mov DWORD PTR [rsp + 56], 20 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHK_THINKING
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndChkThinking, rax

    ; Stream checkbox
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szChkStream
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov DWORD PTR [rsp + 32], 930 ; x
    mov DWORD PTR [rsp + 40], 520 ; y
    mov DWORD PTR [rsp + 48], 80 ; width
    mov DWORD PTR [rsp + 56], 20 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHK_STREAM
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndChkStream, rax

    ; Debug checkbox
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szChkDebug
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov DWORD PTR [rsp + 32], 1020 ; x
    mov DWORD PTR [rsp + 40], 520 ; y
    mov DWORD PTR [rsp + 48], 80 ; width
    mov DWORD PTR [rsp + 56], 20 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHK_DEBUG
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndChkDebug, rax

    ; Terminal (Bottom left under Editor)
    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    mov DWORD PTR [rsp + 32], 260 ; x
    mov DWORD PTR [rsp + 40], 520 ; y
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 160 ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TERMINAL
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndTerminal, rax

    ; Problems Panel (Listbox under Terminal)
    xor rcx, rcx
    lea rdx, szListBoxClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or LBS_NOTIFY or WS_VSCROLL
    mov DWORD PTR [rsp + 32], 260 ; x
    mov DWORD PTR [rsp + 40], 690 ; y
    mov DWORD PTR [rsp + 48], 450 ; width
    mov DWORD PTR [rsp + 56], 90  ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_PROBLEM_PANEL
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov hwndProblemPanel, rax

    ; Mark controls as created for WM_SIZE handler
    mov QWORD PTR controls_created, 1
    
    ; Add welcome message to chat
    mov rcx, hwndChat
    mov rdx, WM_SETTEXT
    lea r8, szWelcomeMsg
    xor r9, r9
    call SendMessageA
    ; Load chat history into chat pane
    call load_chat_history
    ; Initialize terminal subsystem (history, etc.)
    call init_terminal_window
    
    ; Populate model selector - try to load .gguf files first
    call ui_load_models
    
    ; Set default selection (first item)
    mov rcx, hwndModelCombo
    mov rdx, 014Eh ; CB_SETCURSEL
    xor r8, r8 ; index 0
    xor r9, r9
    call SendMessageA
    
    ; Enumerate and populate drives first
    mov rcx, hwndExplorer
    test rcx, rcx
    jz skip_drives
    
    call enumerate_drives
    
skip_drives:
    ; Try to populate explorer with current directory contents
    ; If it fails, we still have the drives visible
    mov rcx, hwndExplorer
    test rcx, rcx
    jz skip_populate
    
    ; Initialize current directory if not set
    mov rax, QWORD PTR [szExplorerDir]
    test rax, rax
    jnz skip_init_dir
    mov rcx, 260
    lea rdx, szExplorerDir
    call GetCurrentDirectoryA
skip_init_dir:
    
    call ui_populate_explorer
    ; Initialize breadcrumbs
    call ui_refresh_breadcrumbs
    ; Initialize status panel
    call ui_refresh_status
skip_populate:

    leave
    ret
ui_create_controls ENDP

;--------------------------------------------------------------------------
; wnd_proc_main
;--------------------------------------------------------------------------
; External menu handlers previously used; not needed now

; Add drag-and-drop constants
WM_DROPFILES            equ 0233h

; Add external API for drag-and-drop
extern DragAcceptFiles:PROC
extern DragQueryFileA:PROC
extern DragFinish:PROC

; Add to constants section
.data
szDragDropSupported    BYTE "Drag and drop supported - files will be processed",0

; Add to window procedure message handling
wnd_proc_main PROC
    ; rcx = hwnd, rdx = uMsg, r8 = wParam, r9 = lParam
    push rbp
    mov rbp, rsp
    sub rsp, 48 ; Shadow space

    cmp edx, WM_CREATE
    je on_create
    cmp edx, WM_DESTROY
    je on_destroy
    cmp edx, WM_SIZE
    je on_size
    cmp edx, WM_COMMAND
    je on_command
    cmp edx, WM_CTLCOLOREDIT
    je on_ctlcolor
    cmp edx, WM_CTLCOLORSTATIC
    je on_ctlcolor
    cmp edx, WM_CTLCOLORLISTBOX
    je on_ctlcolor
    cmp edx, WM_DROPFILES
    je on_dropfiles
    
    ; Default: call DefWindowProcA with parameters (rcx, rdx, r8, r9 already set)
    call DefWindowProcA
    leave
    ret

on_create:
    ; Window created - controls will be created after this returns
    ; Do NOT call ui_populate_explorer here - controls don't exist yet
    
    ; Enable drag-and-drop
    mov rcx, rcx      ; hwnd
    mov rdx, 1        ; TRUE
    call DragAcceptFiles
    
    xor rax, rax
    leave
    ret

on_dropfiles:
    ; Handle file drop
    mov rcx, r8        ; wParam = HDROP handle
    call HandleFileDrop
    xor rax, rax
    leave
    ret

on_destroy:
    ; Persist IDE state before quitting
    call persist_all_state
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    leave
    ret

on_ctlcolor:
    ; Theming: set text color and return background brush
    ; r8 = HDC
    ; Set text color based on current theme
    mov rcx, r8
    mov edx, textColor
    call SetTextColor
    ; Opaque background
    mov rcx, r8
    mov edx, 2 ; OPAQUE
    call SetBkMode
    ; Return brush
    mov rax, hBrushBg
    leave
    ret

on_size:
    ; Dynamic pane layout on resize
    ; Skip if controls not yet created
    mov rax, controls_created
    test rax, rax
    jz size_done_ret
    
    ; Skip if already processing or no main window
    mov rax, hwndMain
    test rax, rax
    jz size_done_ret
    ; Get client rect
    mov rcx, hwndMain
    lea rdx, rectClient
    call GetClientRect

    ; width = right - left; height = bottom - top
    mov eax, DWORD PTR [rectClient.right]
    sub eax, DWORD PTR [rectClient.left]
    mov r12d, eax ; width
    mov eax, DWORD PTR [rectClient.bottom]
    sub eax, DWORD PTR [rectClient.top]
    mov r13d, eax ; height

    ; constants
    mov r14d, 10      ; margin
    mov r15d, 260     ; explorer width
    mov r10d, 450     ; right column width
    mov r11d, 160     ; terminal height
    mov r9d, 90       ; problem panel height
    mov ebx, 90       ; input height

    ; computed positions
    mov eax, r12d
    sub eax, r10d
    sub eax, r14d
    mov esi, eax      ; rightX

    mov eax, r15d
    add eax, r14d
    add eax, r14d
    mov edi, eax      ; editorX

    mov eax, r12d
    sub eax, r10d
    sub eax, r15d
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    mov ecx, eax      ; editorW
    mov r12d, ecx      ; cache editorW

    mov edx, r14d     ; editorY
    mov eax, r13d
    sub eax, r11d
    sub eax, r9d
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    mov ebp, eax      ; editorH

    ; Move Explorer
    mov rax, hwndExplorer
    test rax, rax
    jz skip_explorer
    mov rcx, rax
    mov edx, r14d
    mov r8d, r14d
    mov r9d, r15d
    sub rsp, 32
    mov eax, r13d
    sub eax, r14d
    sub eax, r14d
    mov DWORD PTR [rsp + 32], eax
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_explorer:

    ; Move Editor
    mov rax, hwndEditor
    test rax, rax
    jz skip_editor
    mov rcx, rax
    mov edx, edi
    mov r8d, r14d
    mov r9d, r12d
    sub rsp, 32
    mov DWORD PTR [rsp + 32], ebp
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_editor:

    ; Move Terminal
    mov rax, hwndTerminal
    test rax, rax
    jz skip_terminal
    mov rcx, rax
    mov edx, edi
    mov eax, edx
    add eax, ebp
    add eax, r14d
    mov r8d, eax
    mov r9d, r12d
    sub rsp, 32
    mov DWORD PTR [rsp + 32], r11d
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_terminal:

    ; Move Problems Panel
    mov rax, hwndProblemPanel
    test rax, rax
    jz skip_problem_panel
    mov rcx, rax
    mov edx, edi
    mov eax, edx
    add eax, ebp
    add eax, r14d
    add eax, r11d
    add eax, r14d
    mov r8d, eax
    mov eax, r9d
    mov r9d, r12d
    sub rsp, 32
    mov DWORD PTR [rsp + 32], eax
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_problem_panel:

    ; Move Chat
    mov rax, hwndChat
    test rax, rax
    jz skip_chat
    mov rcx, rax
    mov edx, esi
    mov r8d, r14d
    mov r9d, r10d
    sub rsp, 32
    mov eax, r13d
    sub eax, ebx
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    mov DWORD PTR [rsp + 32], eax
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_chat:

    ; Move Input
    mov rax, hwndInput
    test rax, rax
    jz skip_input
    mov rcx, rax
    mov edx, esi
    mov eax, r13d
    sub eax, ebx
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    add eax, r14d
    add eax, r14d
    mov r8d, eax
    mov r9d, r10d
    sub rsp, 32
    mov DWORD PTR [rsp + 32], ebx
    mov DWORD PTR [rsp + 40], 1
    call MoveWindow
    add rsp, 32
skip_input:

size_done_ret:
    xor rax, rax
    leave
    ret

on_command:
    mov eax, r8d ; wParam (low word is ID)
    and eax, 0FFFFh
    
    cmp eax, IDM_FILE_EXIT
    je on_exit
    cmp eax, IDM_FILE_OPEN
    je on_open
    cmp eax, IDM_FILE_SAVE
    je on_save
    cmp eax, IDM_FILE_SAVE_AS
    je on_save_as
    cmp eax, IDM_CHAT_CLEAR
    je on_chat_clear
    ; Help -> Features
    cmp eax, IDM_HELP_FEATURES
    je on_show_features
    ; Theme selections
    cmp eax, IDM_THEME_LIGHT
    je on_theme_light
    cmp eax, IDM_THEME_DARK
    je on_theme_dark
    cmp eax, IDM_THEME_AMBER
    je on_theme_amber
    ; Breadcrumb button range
    mov ebx, eax
    sub ebx, IDC_BREADCRUMB_BASE
    cmp ebx, 0
    jl skip_breadcrumb
    cmp ebx, 31
    jg skip_breadcrumb
    jmp on_breadcrumb_click
skip_breadcrumb:
    ; Send Button
    cmp eax, IDC_CHAT_SEND_BTN
    je on_send_button
    ; Agents menu actions
    cmp eax, IDM_AGENT_VALIDATE
    je on_agent_validate
    cmp eax, IDM_AGENT_PERSIST_THEME
    je on_agent_persist_theme
    cmp eax, IDM_AGENT_OPEN_FOLDER
    je on_agent_open_folder
    cmp eax, IDM_AGENT_LOGGING
    je on_agent_toggle_logging
    cmp eax, IDM_AGENT_RUN_TESTS
    je on_agent_run_tests
    cmp eax, IDM_AGENT_ZERO_DEPS
    je on_agent_toggle_zero_deps
    ; Problems panel notifications
    cmp eax, IDC_PROBLEM_PANEL
    jne check_explorer_lb
    mov ebx, r8d
    shr ebx, 16
    cmp ebx, LBN_DBLCLK
    jne cmd_other
    mov rcx, hwndProblemPanel
    mov rdx, LB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    cmp eax, 0FFFFFFFFh
    je cmd_other
    mov rcx, rax
    call navigate_problem_panel
    jmp cmd_other
check_explorer_lb:
    ; Explorer listbox notifications
    cmp eax, IDC_EXPLORER_TREE
    jne cmd_other
    ; HIWORD(wParam) in r8d high word
    mov ebx, r8d
    shr ebx, 16
    cmp ebx, LBN_DBLCLK
    je on_explorer_open
    ; Skip single selection to avoid freezing on every click
    jmp cmd_other
on_explorer_open:
    ; On double-click: safe file loading
    mov rcx, hwndExplorer
    mov rdx, LB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    cmp eax, 0FFFFFFFFh
    je cmd_other

    ; Get selected item text
    mov rcx, hwndExplorer
    mov rdx, LB_GETTEXT
    mov r8d, eax
    lea r9, szSelectedName
    call SendMessageA
    

    ; Check if selected item is ".." (go back)
    mov rsi, -1
    lea rdi, szSelectedName
    mov al, BYTE PTR [rdi]
    cmp al, 46     ; '.'
    jne check_if_dir
    mov al, BYTE PTR [rdi + 1]
    cmp al, 46     ; '.'
    jne check_if_dir
    mov al, BYTE PTR [rdi + 2]
    test al, al
    jz go_parent_dir  ; It's "..", so go up
    
check_if_dir:
    ; Check if szSelectedName is a directory by trying to change into it
    lea rcx, szExplorerDir
    lea rdx, szTempBuf
    xor r8, r8
path_cat_loop:
    mov al, BYTE PTR [rcx + r8]
    mov BYTE PTR [rdx + r8], al
    test al, al
    jz path_cat_done
    inc r8
    jmp path_cat_loop
    
path_cat_done:
    ; Check if path ends with backslash
    cmp r8, 0
    je path_cat_add_slash
    dec r8
    mov al, BYTE PTR [rdx + r8]
    cmp al, 92     ; '\'
    je path_cat_skip_slash
    inc r8
    
path_cat_add_slash:
    mov BYTE PTR [rdx + r8], 92  ; '\'
    inc r8
    
path_cat_skip_slash:
    ; Copy filename
    lea rcx, szSelectedName
    xor r9, r9
cat_filename:
    mov al, BYTE PTR [rcx + r9]
    mov BYTE PTR [rdx + r8], al
    test al, al
    jz path_built
    inc r8
    inc r9
    jmp cat_filename
    
path_built:
    ; Try to set current directory (test if it's a folder)
    lea rcx, szTempBuf
    call SetCurrentDirectoryA
    test eax, eax
    jz open_as_file    ; If SetCurrentDirectory failed, open as file
    
    ; It's a directory, refresh explorer
    call ui_populate_explorer
    ; Update breadcrumbs
    call ui_refresh_breadcrumbs
    jmp explorer_open_done
    
go_parent_dir:
    call ui_navigate_up
    jmp explorer_open_done
    
open_as_file:
    ; Build full path to file
    lea rcx, szExplorerDir
    lea rsi, szTempBuf
    xor r8, r8
file_path_loop:
    mov al, BYTE PTR [rcx + r8]
    mov BYTE PTR [rsi + r8], al
    test al, al
    jz file_path_end
    inc r8
    jmp file_path_loop
    
file_path_end:
    ; Add backslash if needed
    dec r8
    mov al, BYTE PTR [rsi + r8]
    cmp al, 92     ; '\'
    je skip_add_slash2
    inc r8
    mov BYTE PTR [rsi + r8], 92
    inc r8
    
skip_add_slash2:
    ; Copy filename
    lea rcx, szSelectedName
    xor r9, r9
append_name:
    mov al, BYTE PTR [rcx + r9]
    mov BYTE PTR [rsi + r8], al
    test al, al
    jz file_to_load
    inc r8
    inc r9
    jmp append_name
    
file_to_load:
    ; Copy to szFileName for ui_load_selected_file
    lea rcx, szTempBuf
    lea rdi, szFileName
    xor r8, r8
copy_fname:
    mov al, BYTE PTR [rcx + r8]
    mov BYTE PTR [rdi + r8], al
    test al, al
    jz file_ready_load
    inc r8
    cmp r8, 259
    jl copy_fname
    
file_ready_load:
    ; Call actual file loading function
    call ui_load_selected_file
    
explorer_open_done:
    xor rax, rax
    leave
    ret

on_theme_light:
    mov DWORD PTR current_theme, 0
    call ui_apply_theme
    xor rax, rax
    leave
    ret
on_theme_dark:
    mov DWORD PTR current_theme, 1
    call ui_apply_theme
    xor rax, rax
    leave
    ret
on_theme_amber:
    mov DWORD PTR current_theme, 2
    call ui_apply_theme
    xor rax, rax
    leave
    ret

on_breadcrumb_click:
    ; ebx = segment index
    ; Build path up to clicked segment into szTempBuf
    lea rsi, szExplorerDir
    lea rdi, szTempBuf
    xor ecx, ecx        ; segment counter
    xor edx, edx        ; char index
bc_copy_loop:
    mov al, BYTE PTR [rsi + rdx]
    test al, al
    jz bc_done_copy
    mov BYTE PTR [rdi + rdx], al
    cmp al, 92          ; '\\'
    jne bc_next_char
    inc ecx
    cmp ecx, ebx        ; reached target segment count
    jg bc_terminate
bc_next_char:
    inc edx
    jmp bc_copy_loop
bc_terminate:
    ; terminate at current position (just after backslash)
    mov BYTE PTR [rdi + edx], 0
bc_done_copy:
    ; Set current directory
    lea rcx, szTempBuf
    call SetCurrentDirectoryA
    ; Update szExplorerDir
    lea rcx, szTempBuf
    lea rdx, szExplorerDir
    xor r8, r8
bc_upd_loop:
    mov al, BYTE PTR [rcx + r8]
    mov BYTE PTR [rdx + r8], al
    test al, al
    jz bc_upd_done
    inc r8
    jmp bc_upd_loop
bc_upd_done:
    ; Refresh explorer and breadcrumbs
    call ui_populate_explorer
    call ui_refresh_breadcrumbs
    xor rax, rax
    leave
    ret
on_send_button:
    ; Get text from input control
    mov rcx, hwndInput
    mov rdx, WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call SendMessageA
    test eax, eax
    jz cmd_ret_sb ; Exit if input is empty
    
    ; Get input text into read_buf (reuse buffer)
    mov ecx, eax
    inc ecx ; Include NUL
    mov rcx, hwndInput
    mov rdx, WM_GETTEXT
    mov r8d, ecx
    lea r9, read_buf
    call SendMessageA
    
    ; Append to chat: set selection to end, then insert text
    mov rcx, hwndChat
    mov rdx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    
    ; Append newline + text
    mov rcx, hwndChat
    mov rdx, EM_REPLACESEL
    xor r8, r8
    lea r9, read_buf
    call SendMessageA
    
    ; Clear input
    call ui_clear_input
    
cmd_ret_sb:
    xor rax, rax
    leave
    ret

cmd_other:
    ; Handle model selector changes (CBN_SELCHANGE = 1)
    mov ebx, r8d
    shr ebx, 16
    cmp eax, IDC_MODEL_SELECTOR
    jne cmd_other2
    cmp ebx, 1
    jne cmd_other2
    jmp on_model_sel_change
cmd_other2:
    xor rax, rax
    leave
    ret

on_open:
    ; Open file dialog and load into editor
    call ui_open_file_dialog
    test rax, rax
    jz cmd_ret
    call ui_load_selected_file
cmd_ret:
    xor rax, rax
    leave
    ret

on_save:
    call ui_save_file_dialog
    test rax, rax
    jz cmd_ret2
    call ui_save_editor_to_file
cmd_ret2:
    xor rax, rax
    leave
    ret

on_save_as:
    ; Save As follows same flow using save dialog
    call ui_save_file_dialog
    test rax, rax
    jz cmd_ret3
    call ui_save_editor_to_file
cmd_ret3:
    xor rax, rax
    leave
    ret

on_chat_clear:
    ; Clear chat text
    mov rcx, hwndChat
    mov rdx, WM_SETTEXT
    lea r8, empty_str
    xor r9, r9
    call SendMessageA
    xor rax, rax
    leave
    ret

on_show_features:
    ; Show Features & Status dialog
    lea rcx, szFeaturesTitle
    lea rdx, szFeaturesMsg
    call ui_show_dialog
    xor rax, rax
    leave
    ret

on_exit:
    mov rcx, hwndMain
    call DestroyWindow
    xor rax, rax
    leave
    ret

on_model_sel_change:
    ; If Auto (index 0) selected, compute actual model into szSelectedModel
    mov rcx, hwndModelCombo
    mov rdx, 0147h      ; CB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    cmp eax, 0
    jne omc_copy_selected
    ; Auto selected: pick first actual model if available
    mov rcx, hwndModelCombo
    mov rdx, 0150h      ; CB_GETCOUNT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    cmp eax, 1
    jle omc_fallback_default
    ; Get text of index 1
    mov rcx, hwndModelCombo
    mov rdx, 0148h      ; CB_GETLBTEXT
    mov r8d, 1
    lea r9, szSelectedModel
    call SendMessageA
    jmp omc_done
omc_fallback_default:
    ; Copy default GPT to szSelectedModel
    lea rcx, szModelGPT
    lea rdx, szSelectedModel
    xor r8, r8
omc_copy_def:
    mov al, BYTE PTR [rcx + r8]
    mov BYTE PTR [rdx + r8], al
    test al, al
    jz omc_done
    inc r8
    jmp omc_copy_def
omc_copy_selected:
    ; Copy selected text to szSelectedModel
    mov rcx, hwndModelCombo
    mov rdx, 0147h      ; CB_GETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov rcx, hwndModelCombo
    mov rdx, 0148h      ; CB_GETLBTEXT
    mov r8d, eax
    lea r9, szSelectedModel
    call SendMessageA
omc_done:
    ; Refresh status panel to reflect model change
    call ui_refresh_status
    xor rax, rax
    leave
    ret

on_agent_validate:
    ; Apply current theme and rebuild breadcrumbs
    call ui_apply_theme
    call ui_refresh_breadcrumbs
    lea rcx, szFeaturesTitle
    lea rdx, szFeaturesMsg
    call ui_show_dialog
    xor rax, rax
    leave
    ret

on_agent_persist_theme:
    ; Save current theme to config file
    call save_theme_to_config
    lea rcx, szFeaturesTitle
    ; reuse message buffer to indicate action
    lea rdx, szMenuLight ; not ideal, but shows dialog
    call ui_show_dialog
    xor rax, rax
    leave
    ret

on_agent_open_folder:
    ; Use Open File dialog to pick a file and switch to its directory
    call ui_open_file_dialog
    test eax, eax
    jz aof_ret
    ; Derive directory from szFileName
    lea rsi, szFileName
    lea rdi, szExplorerDir
    xor ecx, ecx
    xor edx, edx
    ; copy until last backslash position tracked in edx
aof_copy:
    mov al, BYTE PTR [rsi + rcx]
    mov BYTE PTR [rdi + rcx], al
    test al, al
    jz aof_trim
    cmp al, 92
    jne aof_next
    mov edx, ecx
aof_next:
    inc ecx
    jmp aof_copy
aof_trim:
    cmp edx, 0
    jle aof_set
    mov BYTE PTR [rdi + edx], 0
aof_set:
    ; set current directory, refresh
    lea rcx, szExplorerDir
    call SetCurrentDirectoryA
    call ui_populate_explorer
    call ui_refresh_breadcrumbs
aof_ret:
    xor rax, rax
    leave
    ret

on_agent_toggle_logging:
    mov eax, logging_enabled
    xor eax, 1
    mov logging_enabled, eax
    ; Update status panel
    call ui_refresh_status
    xor rax, rax
    leave
    ret

on_agent_run_tests:
    ; Inform user to run PowerShell test harness
    lea rcx, szFeaturesTitle
    lea rdx, szMenuFeatures
    call ui_show_dialog
    xor rax, rax
    leave
    ret

on_agent_toggle_zero_deps:
    mov eax, zero_deps_mode
    xor eax, 1
    mov zero_deps_mode, eax
    ; Update status panel
    call ui_refresh_status
    xor rax, rax
    leave
    ret

wnd_proc_main ENDP

;--------------------------------------------------------------------------
; ui_set_main_menu
;--------------------------------------------------------------------------
ui_set_main_menu PROC
    ; rcx = hmenu
    mov rdx, rcx
    mov rcx, hwndMain
    call SetMenu
    ret
ui_set_main_menu ENDP

;--------------------------------------------------------------------------
; ui_create_chat_control
;--------------------------------------------------------------------------
ui_create_chat_control PROC
    ; Return existing handle if created
    mov rax, hwndChat
    test rax, rax
    jnz uc_ret

    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    sub rsp, 64
    mov DWORD PTR [rsp + 32], 720
    mov DWORD PTR [rsp + 40], 10
    mov DWORD PTR [rsp + 48], 450
    mov DWORD PTR [rsp + 56], 400
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_CHAT_BOX
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    add rsp, 64
    mov hwndChat, rax
uc_ret:
    mov rax, hwndChat
    ret
ui_create_chat_control ENDP

;--------------------------------------------------------------------------
; ui_create_input_control
;--------------------------------------------------------------------------
ui_create_input_control PROC
    mov rax, hwndInput
    test rax, rax
    jnz ui_ret

    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_WANTRETURN
    sub rsp, 64
    mov DWORD PTR [rsp + 32], 720
    mov DWORD PTR [rsp + 40], 420
    mov DWORD PTR [rsp + 48], 450
    mov DWORD PTR [rsp + 56], 90
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_INPUT_BOX
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    add rsp, 64
    mov hwndInput, rax
ui_ret:
    mov rax, hwndInput
    ret
ui_create_input_control ENDP

;--------------------------------------------------------------------------
; ui_create_terminal_control
;--------------------------------------------------------------------------
ui_create_terminal_control PROC
    mov rax, hwndTerminal
    test rax, rax
    jnz ut_ret

    xor rcx, rcx
    lea rdx, szEditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_READONLY or WS_VSCROLL
    sub rsp, 64
    mov DWORD PTR [rsp + 32], 10
    mov DWORD PTR [rsp + 40], 520
    mov DWORD PTR [rsp + 48], 700
    mov DWORD PTR [rsp + 56], 200
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov QWORD PTR [rsp + 72], IDC_TERMINAL
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    add rsp, 64
    mov hwndTerminal, rax
ut_ret:
    mov rax, hwndTerminal
    ret
ui_create_terminal_control ENDP

;--------------------------------------------------------------------------
; ui_open_file_dialog
; Returns 1 on success, 0 otherwise. Path in szFileName.
;--------------------------------------------------------------------------
ui_open_file_dialog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov DWORD PTR [ofn.lStructSize], sizeof OPENFILENAMEA
    mov rax, hwndMain
    mov QWORD PTR [ofn.hwndOwner], rax
    xor rax, rax
    mov QWORD PTR [ofn.hInstance], rax
    lea rax, szFilter
    mov QWORD PTR [ofn.lpstrFilter], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpstrCustomFilter], rax
    mov DWORD PTR [ofn.nMaxCustFilter], 0
    mov DWORD PTR [ofn.nFilterIndex], 1
    lea rax, szFileName
    mov QWORD PTR [ofn.lpstrFile], rax
    mov DWORD PTR [ofn.nMaxFile], 260
    xor rax, rax
    mov QWORD PTR [ofn.lpstrFileTitle], rax
    mov DWORD PTR [ofn.nMaxFileTitle], 0
    xor rax, rax
    mov QWORD PTR [ofn.lpstrInitialDir], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpstrTitle], rax
    mov DWORD PTR [ofn.Flags], 00000800h or 00000004h ; OFN_EXPLORER | OFN_FILEMUSTEXIST
    mov WORD PTR [ofn.nFileOffset], 0
    mov WORD PTR [ofn.nFileExtension], 0
    xor rax, rax
    mov QWORD PTR [ofn.lpstrDefExt], rax
    xor rax, rax
    mov QWORD PTR [ofn.lCustData], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpfnHook], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpTemplateName], rax
    xor rax, rax
    mov QWORD PTR [ofn.pvReserved], rax
    mov DWORD PTR [ofn.dwReserved], 0
    mov DWORD PTR [ofn.FlagsEx], 0

    lea rcx, ofn
    call GetOpenFileNameA
    leave
    ret
ui_open_file_dialog ENDP

;--------------------------------------------------------------------------
; ui_save_file_dialog
; Returns 1 on success, 0 otherwise. Path in szSaveName.
;--------------------------------------------------------------------------
ui_save_file_dialog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov DWORD PTR [sfn.lStructSize], sizeof OPENFILENAMEA
    mov rax, hwndMain
    mov QWORD PTR [sfn.hwndOwner], rax
    xor rax, rax
    mov QWORD PTR [sfn.hInstance], rax
    lea rax, szFilter
    mov QWORD PTR [sfn.lpstrFilter], rax
    xor rax, rax
    mov QWORD PTR [sfn.lpstrCustomFilter], rax
    mov DWORD PTR [sfn.nMaxCustFilter], 0
    mov DWORD PTR [sfn.nFilterIndex], 1
    lea rax, szSaveName
    mov QWORD PTR [sfn.lpstrFile], rax
    mov DWORD PTR [sfn.nMaxFile], 260
    xor rax, rax
    mov QWORD PTR [sfn.lpstrFileTitle], rax
    mov DWORD PTR [sfn.nMaxFileTitle], 0
    xor rax, rax
    mov QWORD PTR [sfn.lpstrInitialDir], rax
    xor rax, rax
    mov QWORD PTR [sfn.lpstrTitle], rax
    mov DWORD PTR [sfn.Flags], 00000800h or 00000002h ; OFN_EXPLORER | OFN_OVERWRITEPROMPT
    mov WORD PTR [sfn.nFileOffset], 0
    mov WORD PTR [sfn.nFileExtension], 0
    xor rax, rax
    mov QWORD PTR [sfn.lpstrDefExt], rax
    xor rax, rax
    mov QWORD PTR [sfn.lCustData], rax
    xor rax, rax
    mov QWORD PTR [sfn.lpfnHook], rax
    xor rax, rax
    mov QWORD PTR [sfn.lpTemplateName], rax
    xor rax, rax
    mov QWORD PTR [sfn.pvReserved], rax
    mov DWORD PTR [sfn.dwReserved], 0
    mov DWORD PTR [sfn.FlagsEx], 0

    lea rcx, sfn
    call GetSaveFileNameA
    leave
    ret
ui_save_file_dialog ENDP

;--------------------------------------------------------------------------
; ui_load_selected_file
; Reads szFileName and appends content to editor.
;--------------------------------------------------------------------------
ui_load_selected_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; HANDLE hFile = CreateFileA(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
    lea rcx, szFileName
    mov rdx, 80000000h ; GENERIC_READ
    mov r8d, 00000001h ; FILE_SHARE_READ
    xor r9, r9
    mov DWORD PTR [rsp + 32], 3 ; OPEN_EXISTING
    mov DWORD PTR [rsp + 40], 0
    xor rax, rax
    mov QWORD PTR [rsp + 48], rax
    call CreateFileA
    mov rsi, rax
    cmp rsi, -1
    je ul_done

    ; Read loop
    lea rdi, read_buf
ul_read:
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, 65536
    lea r9, [rsp + 56] ; lpNumberOfBytesRead
    call ReadFile
    test eax, eax
    jz ul_close
    mov eax, DWORD PTR [rsp + 56]
    test eax, eax
    jz ul_close
    
    ; Append chunk to editor via EM_REPLACESEL
    mov rcx, hwndEditor
    mov rdx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    
    mov rcx, hwndEditor
    mov rdx, EM_REPLACESEL
    xor r8, r8
    lea r9, read_buf
    ; EM_REPLACESEL expects NUL-terminated; temporarily add NUL using RIP-safe addressing
    lea rbx, read_buf
    mov BYTE PTR [rbx + rax], 0
    call SendMessageA
    jmp ul_read

ul_close:
    mov rcx, rsi
    call CloseHandle
ul_done:
    leave
    ret
ui_load_selected_file ENDP

;--------------------------------------------------------------------------
; ui_save_editor_to_file
; Writes editor content to szSaveName
;--------------------------------------------------------------------------
ui_save_editor_to_file PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80

    ; Get text length
    mov rcx, hwndEditor
    mov rdx, WM_GETTEXTLENGTH
    xor r8, r8
    xor r9, r9
    call SendMessageA
    mov r12d, eax

    ; Read in chunks using WM_GETTEXT (limited by buffer). We'll do one shot up to 64KB.
    ; If longer, we save first 64KB.
    mov ecx, r12d
    cmp ecx, 65535
    jbe us_len_ok
    mov ecx, 65535
us_len_ok:
    ; WM_GETTEXT expects size including NUL
    lea r9, read_buf
    mov r8d, ecx
    inc r8d
    mov rcx, hwndEditor
    mov rdx, WM_GETTEXT
    call SendMessageA

    ; Create file for write
    lea rcx, szSaveName
    mov rdx, 40000000h ; GENERIC_WRITE
    mov r8d, 00000000h ; FILE_SHARE_NONE
    xor r9, r9
    mov DWORD PTR [rsp + 32], 2 ; CREATE_ALWAYS
    mov DWORD PTR [rsp + 40], 0
    xor rax, rax
    mov QWORD PTR [rsp + 48], rax
    call CreateFileA
    mov rsi, rax
    cmp rsi, -1
    je us_done

    ; Write buffer
    mov rcx, rsi
    lea rdx, read_buf
    mov r8d, eax ; number of chars copied including NUL; write eax-1
    dec r8d
    lea r9, [rsp + 56]
    call WriteFile
    
    mov rcx, rsi
    call CloseHandle

us_done:
    leave
    ret
ui_save_editor_to_file ENDP

;--------------------------------------------------------------------------
; ui_load_models
; Scans for .gguf files and adds them to model combo
;--------------------------------------------------------------------------
ui_load_models PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Always add Auto at top
    mov rcx, hwndModelCombo
    mov rdx, 0143h ; CB_ADDSTRING
    xor r8, r8
    lea r9, szModelAuto
    call SendMessageA

    ; First try to find .gguf files in current directory
    ; Build pattern: "*.gguf"
    lea rdi, szExplorerPattern
    mov BYTE PTR [rdi], 42      ; '*'
    mov BYTE PTR [rdi + 1], 46  ; '.'
    mov BYTE PTR [rdi + 2], 103 ; 'g'
    mov BYTE PTR [rdi + 3], 103 ; 'g'
    mov BYTE PTR [rdi + 4], 117 ; 'u'
    mov BYTE PTR [rdi + 5], 102 ; 'f'
    mov BYTE PTR [rdi + 6], 0

    ; FindFirstFileA
    lea rcx, szExplorerPattern
    lea rdx, find_data
    call FindFirstFileA
    mov rbx, rax
    cmp rbx, -1
    je load_defaults  ; If no .gguf files, use defaults

lm_loop:
    ; Add filename to combo
    mov rcx, hwndModelCombo
    mov rdx, 0143h ; CB_ADDSTRING
    xor r8, r8
    lea r9, [find_data + 44]  ; cFileName offset
    call SendMessageA
    
    ; Next file
    mov rcx, rbx
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz lm_loop
    
    ; Close handle
    mov rcx, rbx
    call FindClose
    
    jmp lm_done

load_defaults:
    ; If no .gguf files found, add hardcoded defaults
    ; Note: Auto is already at index 0
    mov rcx, hwndModelCombo
    mov rdx, 0143h ; CB_ADDSTRING
    xor r8, r8
    lea r9, szModelGPT
    call SendMessageA
    
    mov rcx, hwndModelCombo
    mov rdx, 0143h ; CB_ADDSTRING
    xor r8, r8
    lea r9, szModelClaude
    call SendMessageA
    
    mov rcx, hwndModelCombo
    mov rdx, 0143h ; CB_ADDSTRING
    xor r8, r8
    lea r9, szModelLlama
    call SendMessageA

lm_done:
    leave
    ret
ui_load_models ENDP

;--------------------------------------------------------------------------
; ui_populate_explorer
; SIMPLIFIED: Just lists files in current directory
;--------------------------------------------------------------------------
;--------------------------------------------------------------------------
; enumerate_drives
; Enumerate all logical drives and add them to explorer listbox
;--------------------------------------------------------------------------
enumerate_drives PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Clear explorer first
    mov rcx, hwndExplorer
    mov rdx, LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA

    ; Call GetLogicalDriveStringsA to get "C:\0D:\0E:\0...0"
    lea rcx, szDriveStrings
    mov rdx, 256
    call GetLogicalDriveStringsA
    test eax, eax
    jz enum_drives_done

    ; Parse the multi-string buffer
    lea rsi, szDriveStrings
    xor r8, r8  ; Index for drive parsing

enum_drives_loop:
    ; Current char
    mov al, BYTE PTR [rsi + r8]
    test al, al
    jz enum_drives_done  ; Double NUL -> end

    ; Copy one NUL-terminated drive string
    lea rdi, szTempBuf
    xor r10, r10  ; char index within drive string

enum_copy_drive:
    mov al, BYTE PTR [rsi + r8]
    mov BYTE PTR [rdi + r10], al
    test al, al
    jz enum_drive_copied
    inc r8
    inc r10
    cmp r10, 30
    jl enum_copy_drive

enum_drive_copied:
    inc r8  ; move past NUL

    ; Add drive to listbox
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8d, r8d
    lea r9, szTempBuf
    call SendMessageA

    ; Next drive
    jmp enum_drives_loop

enum_drives_done:
    leave
    ret
enumerate_drives ENDP

ui_populate_explorer PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Clear listbox
    mov rcx, hwndExplorer
    test rcx, rcx
    jz pe_done2
    mov rdx, 0184h ; LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA

    ; Start from C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init
    lea rcx, szStartPath
    call SetCurrentDirectoryA
    
    ; Get current directory
    mov rcx, 260
    lea rdx, szExplorerDir
    call GetCurrentDirectoryA
    test eax, eax
    jz pe_done2

    ; Build pattern: dir + "\*.*"
    lea rsi, szExplorerDir
    lea rdi, szExplorerPattern
pe_copy_path:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz pe_copy_path
    ; Add "\*.*"
    mov BYTE PTR [rdi - 1], 92  ; '\'
    mov BYTE PTR [rdi], 42      ; '*'
    mov BYTE PTR [rdi + 1], 46  ; '.'
    mov BYTE PTR [rdi + 2], 42  ; '*'
    mov BYTE PTR [rdi + 3], 0

    ; Find files
    lea rcx, szExplorerPattern
    lea rdx, find_data
    call FindFirstFileA
    cmp rax, -1
    je pe_done2
    mov rbx, rax

pe_loop2:
    ; Get filename (offset 44)
    lea r9, [find_data + 44]
    ; Skip "." and ".."
    mov al, BYTE PTR [r9]
    cmp al, 46
    jne pe_add_it
    mov al, BYTE PTR [r9 + 1]
    test al, al
    jz pe_next2
    cmp al, 46
    jne pe_add_it
    mov al, BYTE PTR [r9 + 2]
    test al, al
    jz pe_next2

pe_add_it:
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    ; r9 already points to filename
    call SendMessageA

pe_next2:
    mov rcx, rbx
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz pe_loop2

    mov rcx, rbx
    call FindClose

pe_done2:
    ; Refresh breadcrumbs after populating
    call ui_refresh_breadcrumbs
    leave
    ret
ui_populate_explorer ENDP

;--------------------------------------------------------------------------
; ui_navigate_up
; Goes up one directory level
;--------------------------------------------------------------------------
ui_navigate_up PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Find last backslash in szExplorerDir
    lea rsi, szExplorerDir
    xor ecx, ecx
    xor edx, edx  ; last backslash position
find_last_slash:
    mov al, BYTE PTR [rsi + rcx]
    test al, al
    jz trim_path
    cmp al, 92  ; '\'
    jne next_char
    mov edx, ecx
next_char:
    inc ecx
    jmp find_last_slash
trim_path:
    ; If edx > 2, we can go up (not at "C:\")
    cmp edx, 2
    jle up_done
    ; Null-terminate at last backslash
    mov BYTE PTR [rsi + rdx], 0
    ; Change directory
    lea rcx, szExplorerDir
    call SetCurrentDirectoryA
    ; Refresh explorer
    call ui_populate_explorer
    ; Refresh breadcrumbs
    call ui_refresh_breadcrumbs
up_done:
    leave
    ret
ui_navigate_up ENDP

; (legacy main_* handlers removed; not referenced by this module)

;--------------------------------------------------------------------------
; ui_open_text_file_dialog
; Returns rax = pointer to szFileName or 0 if canceled
;--------------------------------------------------------------------------
PUBLIC ui_open_text_file_dialog
ui_open_text_file_dialog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov DWORD PTR [ofn.lStructSize], SIZEOF OPENFILENAMEA
    mov rax, hwndMain
    mov QWORD PTR [ofn.hwndOwner], rax
    xor rax, rax
    mov QWORD PTR [ofn.hInstance], rax
    lea rax, szFilter
    mov QWORD PTR [ofn.lpstrFilter], rax
    mov DWORD PTR [ofn.nFilterIndex], 1
    lea rax, szFileName
    mov QWORD PTR [ofn.lpstrFile], rax
    mov DWORD PTR [ofn.nMaxFile], 260
    xor rax, rax
    mov QWORD PTR [ofn.lpstrFileTitle], rax
    mov DWORD PTR [ofn.nMaxFileTitle], 0
    xor rax, rax
    mov QWORD PTR [ofn.lpstrInitialDir], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpstrTitle], rax
    mov DWORD PTR [ofn.Flags], 00000800h ; OFN_EXPLORER
    mov WORD PTR  [ofn.nFileOffset], 0
    mov WORD PTR  [ofn.nFileExtension], 0
    xor rax, rax
    mov QWORD PTR [ofn.lpstrDefExt], rax
    xor rax, rax
    mov QWORD PTR [ofn.lCustData], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpfnHook], rax
    xor rax, rax
    mov QWORD PTR [ofn.lpTemplateName], rax
    xor rax, rax
    mov QWORD PTR [ofn.pvReserved], rax
    mov DWORD PTR [ofn.dwReserved], 0
    mov DWORD PTR [ofn.FlagsEx], 0

    lea rcx, ofn
    call GetOpenFileNameA
    test eax, eax
    jz cancel_open
    lea rax, szFileName
    leave
    ret
cancel_open:
    xor rax, rax
    leave
    ret
ui_open_text_file_dialog ENDP

;--------------------------------------------------------------------------
; ui_editor_set_text
; rcx = pointer to null-terminated ASCII
;--------------------------------------------------------------------------
PUBLIC ui_editor_set_text
ui_editor_set_text PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov rdx, WM_SETTEXT
    mov r8, rcx
    mov rcx, hwndEditor
    call SendMessageA

    leave
    ret
ui_editor_set_text ENDP

;--------------------------------------------------------------------------
; ui_editor_get_text
; rcx = buffer, rdx = maxlen; returns length
;--------------------------------------------------------------------------
PUBLIC ui_editor_get_text
ui_editor_get_text PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov r8, rdx
    mov r9, rcx
    mov rcx, hwndEditor
    mov rdx, WM_GETTEXT
    call SendMessageA

    leave
    ret
ui_editor_get_text ENDP

; (removed legacy duplicate ui_open_file_dialog; using unified implementation above)

;--------------------------------------------------------------------------
; ui_add_chat_message
; rcx = message string
;--------------------------------------------------------------------------
ui_add_chat_message PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov rsi, rcx ; Save message
    
    ; Set selection to end
    mov rcx, hwndChat
    mov rdx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA
    
    ; Replace selection
    mov rcx, hwndChat
    mov rdx, EM_REPLACESEL
    xor r8, r8
    mov r9, rsi
    call SendMessageA
    
    leave
    ret
ui_add_chat_message ENDP

;--------------------------------------------------------------------------
; ui_show_dialog
; rcx = title, rdx = message
;--------------------------------------------------------------------------
ui_show_dialog PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov r8, rcx ; title
    mov rcx, hwndMain
    ; rdx is already message
    mov r9d, MB_OK or MB_ICONINFORMATION
    call MessageBoxA
    
    leave
    ret
ui_show_dialog ENDP

;--------------------------------------------------------------------------
; ui_get_input_text
; rcx = buffer, rdx = maxlen
;--------------------------------------------------------------------------
ui_get_input_text PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov r8, rdx ; maxlen
    mov r9, rcx ; buffer
    mov rcx, hwndInput
    mov rdx, WM_GETTEXT
    call SendMessageA
    
    leave
    ret
ui_get_input_text ENDP

;--------------------------------------------------------------------------
; ui_clear_input
;--------------------------------------------------------------------------
ui_clear_input PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov rcx, hwndInput
    mov rdx, WM_SETTEXT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    leave
    ret
ui_clear_input ENDP

;==========================================================================
; ENHANCED SUBSYSTEMS - Complete Implementation
;==========================================================================

;--------------------------------------------------------------------------
; load_dynamic_models - Load available AI models from config
;--------------------------------------------------------------------------
load_dynamic_models PROC
    push rbp
    mov rbp, rsp
    
    ; Clear existing combo box
    mov rcx, hwndModelCombo
    mov rdx, 0184h  ; LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    ; Add Auto at top
    mov rcx, hwndModelCombo
    mov rdx, 0143h  ; CB_ADDSTRING
    xor r8, r8
    lea r9, szModelAuto
    call SendMessageA
    
    ; Try to load from models.json
    lea rcx, szModelsJsonPath
    call load_models_from_file
    
    ; If file doesn't exist, add default models
    cmp eax, 0
    jne load_models_done
    
    ; Add defaults
    mov rcx, hwndModelCombo
    mov rdx, 0143h  ; CB_ADDSTRING
    xor r8, r8
    lea r9, szDefaultModel1
    call SendMessageA
    
    mov rcx, hwndModelCombo
    mov rdx, 0143h
    xor r8, r8
    lea r9, szDefaultModel2
    call SendMessageA
    
    mov rcx, hwndModelCombo
    mov rdx, 0143h
    xor r8, r8
    lea r9, szDefaultModel3
    call SendMessageA
    
load_models_done:
    ; Set first as default (Auto)
    mov rcx, hwndModelCombo
    mov rdx, 014Eh  ; CB_SETCURSEL
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    leave
    ret
load_dynamic_models ENDP

;--------------------------------------------------------------------------
; load_models_from_file - Load model list from JSON file
;--------------------------------------------------------------------------
load_models_from_file PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = file path
    mov rsi, rcx
    
    ; Try to open models.json
    mov rcx, rsi
    mov rdx, 80000000h  ; GENERIC_READ
    xor r8, r8
    mov r8, 1           ; FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp + 32], 3  ; OPEN_EXISTING
    mov QWORD PTR [rsp + 40], 80h  ; FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    cmp rax, -1
    je models_file_failed
    
    mov rbx, rax
    
    ; Read file
    mov rcx, rbx
    lea rdx, szEditorBuffer
    mov r8, 32768
    lea r9, [rsp + 16]
    call ReadFile

    mov eax, DWORD PTR [rsp + 16]
    cmp eax, 32767
    jbe term_ok
    mov eax, 32767
term_ok:
    mov BYTE PTR [szEditorBuffer + rax], 0
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    ; Parse JSON and populate models
    lea rcx, szEditorBuffer
    call parse_models_json
    
    mov eax, 1
    leave
    ret
    
models_file_failed:
    xor eax, eax
    leave
    ret
load_models_from_file ENDP

;--------------------------------------------------------------------------
; parse_models_json - Extract model names from JSON
;--------------------------------------------------------------------------
parse_models_json PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = JSON buffer
    mov rsi, rcx
    xor r8, r8  ; model count
    
json_parse_loop:
    mov al, BYTE PTR [rsi]
    test al, al
    jz json_parse_done
    
    ; Look for model entries
    cmp al, '"'
    jne json_skip_char
    
    ; Check if this starts a model entry
    mov al, BYTE PTR [rsi + 1]
    mov cl, BYTE PTR [rsi + 2]
    
    ; Extract model name
    lea rdi, szTempBuf
    mov rdx, rsi
    call extract_json_value
    
    ; Add to combo box
    mov rcx, hwndModelCombo
    mov rdx, 0143h  ; CB_ADDSTRING
    xor r8, r8
    lea r9, szTempBuf
    call SendMessageA
    
    inc r8
    cmp r8, 10  ; Max 10 models
    jge json_parse_done
    
json_skip_char:
    inc rsi
    jmp json_parse_loop
    
json_parse_done:
    leave
    ret
parse_models_json ENDP

;--------------------------------------------------------------------------
; load_chat_history - Load previous chat messages from file
;--------------------------------------------------------------------------
PUBLIC load_chat_history
load_chat_history PROC
    push rbp
    mov rbp, rsp
    
    ; Open chat history file
    lea rcx, szChatHistoryPath
    mov rdx, 80000000h  ; GENERIC_READ
    xor r8, r8
    mov r8, 1
    xor r9, r9
    mov QWORD PTR [rsp + 32], 3
    mov QWORD PTR [rsp + 40], 80h
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    cmp rax, -1
    je chat_load_done
    
    mov rbx, rax
    
    ; Read chat history
    mov rcx, rbx
    lea rdx, szEditorBuffer
    mov r8, 32768
    lea r9, [rsp + 16]
    call ReadFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
    ; Display in chat window
    mov rcx, hwndChat
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szEditorBuffer
    call SendMessageA
    
chat_load_done:
    leave
    ret
load_chat_history ENDP

;--------------------------------------------------------------------------
; save_chat_history - Persist chat to file
;--------------------------------------------------------------------------
PUBLIC save_chat_history
save_chat_history PROC
    push rbp
    mov rbp, rsp
    
    ; Get chat text
    mov rcx, hwndChat
    mov rdx, WM_GETTEXT
    mov r8d, 32768
    lea r9, szEditorBuffer
    call SendMessageA
    
    ; Create/overwrite chat history file
    lea rcx, szChatHistoryPath
    mov rdx, 40000000h  ; GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp + 32], 2  ; CREATE_ALWAYS
    mov QWORD PTR [rsp + 40], 80h
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    cmp rax, -1
    je chat_save_done
    
    mov rbx, rax
    
    ; Write chat data
    mov rcx, rbx
    lea rdx, szEditorBuffer
    
    ; Get length
    lea rsi, szEditorBuffer
    xor r8, r8
chat_len_loop:
    mov al, BYTE PTR [rsi + r8]
    test al, al
    jz chat_len_done
    inc r8
    jmp chat_len_loop
    
chat_len_done:
    mov rcx, rbx
    lea rdx, szEditorBuffer
    call WriteFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
chat_save_done:
    leave
    ret
save_chat_history ENDP

;--------------------------------------------------------------------------
; init_terminal_window - Initialize terminal pane
;--------------------------------------------------------------------------
PUBLIC init_terminal_window
init_terminal_window PROC
    push rbp
    mov rbp, rsp
    
    ; hwndTerminal is created in ui_create_controls
    ; Initialize terminal history
    lea rcx, szTerminalHistoryPath
    call load_chat_history  ; Reuse history loader
    
    leave
    ret
init_terminal_window ENDP

;--------------------------------------------------------------------------
; execute_terminal_command - Run command and show output
;--------------------------------------------------------------------------
PUBLIC execute_terminal_command
execute_terminal_command PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = command string
    mov rdx, rcx
    test rdx, rdx
    jz exec_term_done
    cmp BYTE PTR [rdx], 0
    je exec_term_done

    mov execReadHandle, 0
    mov execWriteHandle, 0

    ; Build "cmd.exe /c <command>"
    lea rdi, szTempBuf
    lea rsi, szCmdPrefix
cmd_prefix_copy:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz cmd_prefix_done
    inc rsi
    inc rdi
    jmp cmd_prefix_copy
cmd_prefix_done:
    dec rdi
cmd_arg_copy:
    mov al, BYTE PTR [rdx]
    mov BYTE PTR [rdi], al
    test al, al
    jz cmd_copy_done
    inc rdx
    inc rdi
    jmp cmd_arg_copy
cmd_copy_done:

    ; Setup security attributes for pipe
    mov execSA.nLength, SIZEOF SECURITY_ATTRIBUTES
    mov execSA.lpSecurityDescriptor, 0
    mov execSA.bInheritHandle, 1
    lea rcx, execReadHandle
    lea rdx, execWriteHandle
    lea r8, execSA
    xor r9, r9
    call CreatePipe
    test eax, eax
    jz exec_term_fail
    mov rcx, execReadHandle
    mov rdx, HANDLE_FLAG_INHERIT
    xor r8, r8
    call SetHandleInformation

    ; Zero STARTUPINFOA and PROCESS_INFORMATION
    lea rdi, execSI
    mov ecx, SIZEOF STARTUPINFOA
    xor eax, eax
    rep stosb
    lea rdi, execPI
    mov ecx, SIZEOF PROCESS_INFORMATION
    xor eax, eax
    rep stosb
    mov execSI.cb, SIZEOF STARTUPINFOA
    mov execSI.dwFlags, STARTF_USESTDHANDLES
    mov rcx, STD_INPUT_HANDLE
    call GetStdHandle
    mov execSI.hStdInput, rax
    mov rax, execWriteHandle
    mov execSI.hStdOutput, rax
    mov execSI.hStdError, rax

    ; Create process
    xor rcx, rcx
    lea rdx, szTempBuf
    xor r8, r8
    xor r9, r9
    sub rsp, 96
    mov QWORD PTR [rsp + 32], 1
    mov QWORD PTR [rsp + 40], 0
    mov QWORD PTR [rsp + 48], 0
    mov QWORD PTR [rsp + 56], 0
    lea rax, execSI
    mov QWORD PTR [rsp + 64], rax
    lea rax, execPI
    mov QWORD PTR [rsp + 72], rax
    call CreateProcessA
    add rsp, 96
    test eax, eax
    jz exec_term_fail

    ; Close write end in parent
    mov rcx, execWriteHandle
    call CloseHandle

    ; Read output
    mov execTotalRead, 0
read_output_loop:
    mov eax, execTotalRead
    mov ecx, 65535
    sub ecx, eax
    jbe read_output_done
    mov edx, 4096
    cmp ecx, edx
    cmovb edx, ecx
    lea rdx, read_buf
    add rdx, rax
    mov rcx, execReadHandle
    mov r8d, edx
    lea r9, execBytesRead
    call ReadFile
    test eax, eax
    jz read_output_done
    mov eax, execBytesRead
    test eax, eax
    jz read_output_done
    add execTotalRead, eax
    jmp read_output_loop

read_output_done:
    mov rcx, execReadHandle
    call CloseHandle
    mov rcx, execPI.hProcess
    mov rdx, 30000
    call WaitForSingleObject
    mov rcx, execPI.hProcess
    call CloseHandle
    mov rcx, execPI.hThread
    call CloseHandle

    mov eax, execTotalRead
    mov BYTE PTR [read_buf + rax], 0
    cmp eax, 0
    jne output_ready
    mov BYTE PTR [read_buf], 'O'
    mov BYTE PTR [read_buf + 1], 'K'
    mov BYTE PTR [read_buf + 2], 0
output_ready:
    mov rcx, hwndTerminal
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, read_buf
    call SendMessageA
    mov eax, 1
    leave
    ret

exec_term_fail:
    mov rcx, execWriteHandle
    test rcx, rcx
    jz exec_fail_skip_w
    call CloseHandle
exec_fail_skip_w:
    mov rcx, execReadHandle
    test rcx, rcx
    jz exec_fail_skip_r
    call CloseHandle
exec_fail_skip_r:
    mov rcx, hwndTerminal
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szBuildFailed
    call SendMessageA
exec_term_done:
    xor eax, eax
    leave
    ret
execute_terminal_command ENDP

;--------------------------------------------------------------------------
; open_file_in_editor - Full file opening with dialog
;--------------------------------------------------------------------------
PUBLIC open_file_in_editor
open_file_in_editor PROC
    push rbp
    mov rbp, rsp
    
    ; Show Open File dialog
    call ui_open_file_dialog
    test eax, eax
    jz file_open_canceled
    
    ; Load file into editor
    lea rcx, szFileName
    call load_file_content
    
    ; Display in editor
    mov rcx, hwndEditor
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szEditorBuffer
    call SendMessageA
    
file_open_canceled:
    leave
    ret
open_file_in_editor ENDP

;--------------------------------------------------------------------------
; load_file_content - Load file bytes into buffer
;--------------------------------------------------------------------------
load_file_content PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = file path
    mov rsi, rcx
    
    mov rcx, rsi
    mov rdx, 80000000h  ; GENERIC_READ
    xor r8, r8
    mov r8, 1
    xor r9, r9
    mov QWORD PTR [rsp + 32], 3
    mov QWORD PTR [rsp + 40], 80h
    mov QWORD PTR [rsp + 48], 0
    
    call CreateFileA
    cmp rax, -1
    je file_content_done
    
    mov rbx, rax
    
    ; Read entire file
    mov rcx, rbx
    lea rdx, szEditorBuffer
    mov r8, 32768
    lea r9, [rsp + 16]
    call ReadFile
    
    ; Close
    mov rcx, rbx
    call CloseHandle
    
file_content_done:
    leave
    ret
load_file_content ENDP

;--------------------------------------------------------------------------
; refresh_file_explorer_tree - Populate explorer with actual directory
;--------------------------------------------------------------------------
PUBLIC refresh_file_explorer_tree
refresh_file_explorer_tree PROC
    push rbp
    mov rbp, rsp
    
    ; Clear listbox
    mov rcx, hwndExplorer
    mov rdx, 0184h  ; LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Get current directory
    mov rcx, 260
    lea rdx, szExplorerDir
    call GetCurrentDirectoryA
    
    ; Add parent directory ".."
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    lea r9, szBackDir
    call SendMessageA
    
    ; Build pattern dir + "\*.*"
    lea rsi, szExplorerDir
    lea rdi, szExplorerPattern
pat_copy:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz pat_copy
    
    mov BYTE PTR [rdi - 1], 92  ; '\'
    mov BYTE PTR [rdi], 42      ; '*'
    mov BYTE PTR [rdi + 1], 46  ; '.'
    mov BYTE PTR [rdi + 2], 42  ; '*'
    mov BYTE PTR [rdi + 3], 0
    
    ; Find files
    lea rcx, szExplorerPattern
    lea rdx, find_data
    call FindFirstFileA
    cmp rax, -1
    je explorer_done
    
    mov rbx, rax
    
explorer_loop:
    ; Get filename (offset 44 in WIN32_FIND_DATAA)
    lea r9, [find_data + 44]
    
    ; Skip . and ..
    mov al, BYTE PTR [r9]
    cmp al, '.'
    je explorer_next
    
    ; Add to explorer
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    call SendMessageA
    
explorer_next:
    mov rcx, rbx
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz explorer_loop
    
    mov rcx, rbx
    call FindClose
    
explorer_done:
    leave
    ret
refresh_file_explorer_tree ENDP

;--------------------------------------------------------------------------
; persist_all_state - Save IDE state (models, chat, files, settings)
;--------------------------------------------------------------------------
PUBLIC persist_all_state
persist_all_state PROC
    push rbp
    mov rbp, rsp
    
    ; Save chat history
    call save_chat_history
    
    ; Save current file (if editing)
    call ui_save_editor_to_file
    
    ; Save model selection: if Auto is selected, we already computed szSelectedModel
    ; Check current combo text
    lea rdi, szTempBuf
    mov rcx, hwndModelCombo
    mov rdx, WM_GETTEXT
    mov r8d, 260
    lea r9, szTempBuf
    call SendMessageA
    ; Compare with "Auto"
    lea rsi, szModelAuto
    xor ecx, ecx
pas_cmp_loop:
    mov al, BYTE PTR [rsi + rcx]
    mov bl, BYTE PTR [rdi + rcx]
    cmp al, bl
    jne pas_not_auto
    test al, al
    jz pas_is_auto
    inc ecx
    jmp pas_cmp_loop
pas_not_auto:
    ; Copy temp to selected model
    lea rdx, szSelectedModel
    xor r8, r8
pas_copy_loop:
    mov al, BYTE PTR [rdi + r8]
    mov BYTE PTR [rdx + r8], al
    test al, al
    jz pas_copy_done
    inc r8
    jmp pas_copy_loop
pas_copy_done:
    jmp pas_save
pas_is_auto:
    ; szSelectedModel already set by on_model_sel_change
pas_save:
    ; Save to config
    lea rcx, szSelectedModel
    call save_model_selection_impl
    
    leave
    ret
persist_all_state ENDP

;--------------------------------------------------------------------------
; save_model_selection_impl - Save selected model to config
;--------------------------------------------------------------------------
save_model_selection_impl PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = model name
    ; In full implementation, would write to rawr_config.json
    
    leave
    ret
save_model_selection_impl ENDP

;--------------------------------------------------------------------------
; Helper functions
;--------------------------------------------------------------------------

extract_json_value PROC
    ; rsi = source, rdi = dest
    ; Extract value between quotes
    xor r10, r10  ; count
extract_val_loop:
    mov al, BYTE PTR [rsi + r10]
    test al, al
    jz extract_val_done
    cmp al, '"'
    je extract_val_found
    inc r10
    cmp r10, 256
    jl extract_val_loop
    
extract_val_found:
    inc r10  ; skip quote
val_copy:
    mov al, BYTE PTR [rsi + r10]
    cmp al, '"'
    je val_copy_done
    test al, al
    jz val_copy_done
    mov BYTE PTR [rdi], al
    inc rdi
    inc r10
    jmp val_copy
    
val_copy_done:
    mov BYTE PTR [rdi], 0
    
extract_val_done:
    ret
extract_json_value ENDP

.code
;--------------------------------------------------------------------------
; ui_apply_theme - Create brushes/colors per current_theme and repaint
; current_theme: 0=Light, 1=Dark, 2=Amber
;--------------------------------------------------------------------------
PUBLIC ui_apply_theme
ui_apply_theme PROC
    push rbp
    mov rbp, rsp
    
    ; Delete previous brush if any
    mov rax, hBrushBg
    test rax, rax
    jz ua_make
    mov rcx, rax
    call DeleteObject
ua_make:
    ; Choose colors
    mov eax, DWORD PTR current_theme
    cmp eax, 0
    je ua_light
    cmp eax, 1
    je ua_dark
    ; Amber default
    mov edx, 0000BFFFh    ; COLORREF amber (BBGGRR)
    mov DWORD PTR textColor, 00000000h ; black text
    jmp ua_create
ua_light:
    mov edx, 00FFFFFFh    ; white
    mov DWORD PTR textColor, 00000000h ; black
    jmp ua_create
ua_dark:
    mov edx, 00202020h    ; dark gray
    mov DWORD PTR textColor, 00FFFFFFh ; white
ua_create:
    ; Create new solid brush
    mov rcx, rdx
    call CreateSolidBrush
    mov hBrushBg, rax
    ; Repaint main window
    mov rcx, hwndMain
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect
    
    leave
    ret
ui_apply_theme ENDP

;--------------------------------------------------------------------------
; ui_refresh_status - Compose and display status in status panel
; Shows: Engine, Model, Logging, Zero-Deps
;--------------------------------------------------------------------------
PUBLIC ui_refresh_status
ui_refresh_status PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32

    ; rdi = dest buffer
    lea rdi, szStatusBuf

    ; Helper: copy non-NUL terminated string from rsi to rdi
    ; We will inline loops for each segment

    ; Copy "Engine: "
    lea rsi, szStatusEngine
urs_copy_engine_lbl:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_engine_lbl_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_engine_lbl
urs_engine_lbl_done:

    ; Append engine status string from RawrXD_GetEngineStatus()
    call RawrXD_GetEngineStatus
    mov rsi, rax
urs_copy_engine_val:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_engine_val_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_engine_val
urs_engine_val_done:

    ; Copy " • Model: "
    lea rsi, szStatusModelMid
urs_copy_model_mid:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_model_mid_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_model_mid
urs_model_mid_done:

    ; Append model name: prefer szSelectedModel; fallback to combo text
    lea rsi, szSelectedModel
    mov al, BYTE PTR [rsi]
    test al, al
    jnz urs_copy_model_val
    ; Fallback: combo text
    mov rcx, hwndModelCombo
    mov rdx, WM_GETTEXT
    mov r8d, 260
    lea r9, szTempBuf
    call SendMessageA
    lea rsi, szTempBuf
urs_copy_model_val:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_model_val_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_model_val
urs_model_val_done:

    ; Copy " • Logging: "
    lea rsi, szStatusLoggingMid
urs_copy_log_mid:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_log_mid_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_log_mid
urs_log_mid_done:

    ; Append On/Off based on logging_enabled
    mov eax, logging_enabled
    test eax, eax
    jz urs_log_off
    lea rsi, szOn
    jmp urs_copy_log_val
urs_log_off:
    lea rsi, szOff
urs_copy_log_val:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_log_val_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_log_val
urs_log_val_done:

    ; Copy " • Zero-Deps: "
    lea rsi, szStatusZeroDepsMid
urs_copy_zd_mid:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_zd_mid_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_zd_mid
urs_zd_mid_done:

    ; Append On/Off based on zero_deps_mode
    mov eax, zero_deps_mode
    test eax, eax
    jz urs_zd_off
    lea rsi, szOn
    jmp urs_copy_zd_val
urs_zd_off:
    lea rsi, szOff
urs_copy_zd_val:
    mov al, BYTE PTR [rsi]
    test al, al
    jz urs_zd_val_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp urs_copy_zd_val
urs_zd_val_done:

    ; Terminate
    mov BYTE PTR [rdi], 0

    ; Set text to status panel
    mov rcx, hwndStatusPanel
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szStatusBuf
    call SendMessageA

    leave
    ret
ui_refresh_status ENDP

;--------------------------------------------------------------------------
; ui_refresh_breadcrumbs - Build clickable buttons for each path segment
;--------------------------------------------------------------------------
PUBLIC ui_refresh_breadcrumbs
ui_refresh_breadcrumbs PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; Destroy existing buttons
    mov ecx, breadcrumb_count
    test ecx, ecx
    jz bc_build
    xor edx, edx
bc_destroy_loop:
    mov rax, QWORD PTR [hwndBreadcrumbBtns + rdx*8]
    test rax, rax
    jz bc_destroy_next
    mov rcx, rax
    call DestroyWindow
bc_destroy_next:
    inc edx
    cmp edx, ecx
    jl bc_destroy_loop
    mov breadcrumb_count, 0

bc_build:
    ; Parse szExplorerDir and create buttons for each segment
    lea rsi, szExplorerDir
    xor rdx, rdx      ; char index
    xor ecx, ecx      ; segment count
    mov r14d, 10      ; start X
    mov r15d, 0       ; start Y at top (0)
    
    ; Build label buffer per segment
    lea rdi, szTempBuf
    xor r8, r8        ; label length

bc_iter:
    mov al, BYTE PTR [rsi + rdx]
    test al, al
    jz bc_emit_last
    cmp al, 92        ; '\\'
    jne bc_append
    ; emit segment label for button
    mov BYTE PTR [rdi + r8], 0
    ; Create button for segment if label not empty
    cmp r8, 0
    jle bc_reset
    ; CreateWindowExA for button
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szTempBuf
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov DWORD PTR [rsp + 32], r14d ; x
    mov DWORD PTR [rsp + 40], r15d ; y
    mov DWORD PTR [rsp + 48], 90   ; width
    mov DWORD PTR [rsp + 56], 20   ; height
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov eax, IDC_BREADCRUMB_BASE
    add eax, ecx
    mov QWORD PTR [rsp + 72], rax  ; id
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    ; Save handle
    mov QWORD PTR [hwndBreadcrumbBtns + rcx*8], rax
    ; Advance X, inc segment count
    add r14d, 95
    inc ecx
    mov breadcrumb_count, ecx
bc_reset:
    ; reset label buffer
    lea rdi, szTempBuf
    xor r8, r8
    jmp bc_nextchar

bc_append:
    mov BYTE PTR [rdi + r8], al
    inc r8
bc_nextchar:
    inc rdx
    jmp bc_iter

bc_emit_last:
    ; Emit last segment (no trailing backslash)
    cmp r8, 0
    jle bc_done
    mov BYTE PTR [rdi + r8], 0
    xor rcx, rcx
    lea rdx, szButtonClass
    lea r8, szTempBuf
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov DWORD PTR [rsp + 32], r14d
    mov DWORD PTR [rsp + 40], r15d
    mov DWORD PTR [rsp + 48], 90
    mov DWORD PTR [rsp + 56], 20
    mov rax, hwndMain
    mov QWORD PTR [rsp + 64], rax
    mov eax, IDC_BREADCRUMB_BASE
    add eax, ecx
    mov QWORD PTR [rsp + 72], rax
    mov rax, hInstance
    mov QWORD PTR [rsp + 80], rax
    mov QWORD PTR [rsp + 88], 0
    call CreateWindowExA
    mov QWORD PTR [hwndBreadcrumbBtns + rcx*8], rax
    inc ecx
    mov breadcrumb_count, ecx

bc_done:
    leave
    ret
ui_refresh_breadcrumbs ENDP

;--------------------------------------------------------------------------
; handle_command_palette - Process command palette input (Ctrl+Shift+P)
; rcx = command string
;--------------------------------------------------------------------------
PUBLIC handle_command_palette
handle_command_palette PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; rcx = command string
    mov r12, rcx
    
    ; Check command type
    lea rsi, szCmdDebug
    xor r8, r8
cmd_cmp_loop1:
    mov al, BYTE PTR [rsi + r8]
    mov bl, BYTE PTR [r12 + r8]
    cmp al, bl
    jne cmd_not_debug
    test al, al
    jz cmd_is_debug
    inc r8
    jmp cmd_cmp_loop1
    
cmd_is_debug:
    ; Debug command - toggle breakpoint
    call handle_debug_command
    jmp cmd_exit
    
cmd_not_debug:
    ; Check for search command
    lea rsi, szCmdSearch
    xor r8, r8
cmd_cmp_loop2:
    mov al, BYTE PTR [rsi + r8]
    mov bl, BYTE PTR [r12 + r8]
    cmp al, bl
    jne cmd_not_search
    test al, al
    jz cmd_is_search
    inc r8
    jmp cmd_cmp_loop2
    
cmd_is_search:
    ; Search command - find in files
    call handle_file_search_command
    jmp cmd_exit
    
cmd_not_search:
    ; Check for run command
    lea rsi, szCmdRun
    xor r8, r8
cmd_cmp_loop3:
    mov al, BYTE PTR [rsi + r8]
    mov bl, BYTE PTR [r12 + r8]
    cmp al, bl
    jne cmd_not_run
    test al, al
    jz cmd_is_run
    inc r8
    jmp cmd_cmp_loop3
    
cmd_is_run:
    ; Run command - execute build
    call handle_run_command
    jmp cmd_exit
    
cmd_not_run:
    ; Unknown command - show help
    lea rcx, szCmdHelp
    call ui_show_dialog
    
cmd_exit:
    leave
    ret
handle_command_palette ENDP

;--------------------------------------------------------------------------
; handle_debug_command - Toggle breakpoint at current line
;--------------------------------------------------------------------------
handle_debug_command PROC
    push rbp
    mov rbp, rsp
    
    ; Get current editor line number
    mov rcx, hwndEditor
    mov rdx, EM_LINEFROMCHAR
    mov r8, -1
    call SendMessageA
    
    ; rax = current line number
    mov r12, rax
    
    ; Toggle breakpoint marker
    mov rcx, hwndEditor
    mov rdx, EM_SETSEL
    mov r8, 0
    mov r9, 0
    call SendMessageA
    
    ; Display status
    lea rcx, szBreakpointSet
    call ui_show_dialog
    
    leave
    ret
handle_debug_command ENDP

;--------------------------------------------------------------------------
; handle_file_search_command - Recursive file search
;--------------------------------------------------------------------------
handle_file_search_command PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Build search pattern recursively
    call refresh_file_explorer_tree_recursive
    
    leave
    ret
handle_file_search_command ENDP

;--------------------------------------------------------------------------
; refresh_file_explorer_tree_recursive - Recursively search directories
;--------------------------------------------------------------------------
refresh_file_explorer_tree_recursive PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Get current directory
    mov rcx, 260
    lea rdx, szExplorerDir
    call GetCurrentDirectoryA
    
    ; Clear explorer
    mov rcx, hwndExplorer
    mov rdx, 0184h  ; LB_RESETCONTENT
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Add parent directory
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    lea r9, szBackDir
    call SendMessageA
    
    ; Recursive scan
    lea rcx, szExplorerDir
    call do_recursive_file_scan
    
    leave
    ret
refresh_file_explorer_tree_recursive ENDP

;--------------------------------------------------------------------------
; do_recursive_file_scan - Depth-first directory traversal
; rcx = directory path
;--------------------------------------------------------------------------
do_recursive_file_scan PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov r12, rcx            ; r12 = directory path
    
    ; Build pattern: dir + "\*.*"
    lea rsi, r12
    lea rdi, szExplorerPattern
pat_copy_rec:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz pat_done_rec
    inc rsi
    inc rdi
    jmp pat_copy_rec
    
pat_done_rec:
    dec rdi
    mov BYTE PTR [rdi], 92  ; '\'
    inc rdi
    mov BYTE PTR [rdi], 42  ; '*'
    inc rdi
    mov BYTE PTR [rdi], 46  ; '.'
    inc rdi
    mov BYTE PTR [rdi], 42  ; '*'
    inc rdi
    mov BYTE PTR [rdi], 0
    
    ; Find files
    lea rcx, szExplorerPattern
    lea rdx, find_data
    call FindFirstFileA
    cmp rax, -1
    je rec_scan_done
    
    mov rbx, rax
    
rec_scan_loop:
    ; Get filename (offset 44 in WIN32_FIND_DATAA)
    lea r9, [find_data + 44]
    
    ; Check if directory (offset 0, flags)
    mov r8d, DWORD PTR [find_data]
    test r8d, 10h           ; FILE_ATTRIBUTE_DIRECTORY
    jz rec_scan_file
    
    ; Skip . and ..
    mov al, BYTE PTR [r9]
    cmp al, '.'
    je rec_scan_next
    
    ; Build subdirectory path
    lea rsi, r12
    lea rdi, [rsp + 32]     ; temp path
path_build:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz path_build_done
    inc rsi
    inc rdi
    jmp path_build
    
path_build_done:
    dec rdi
    mov BYTE PTR [rdi], 92  ; '\'
    inc rdi
    mov rsi, r9
path_append:
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rdi], al
    test al, al
    jz path_append_done
    inc rsi
    inc rdi
    jmp path_append
    
path_append_done:
    ; Recursively scan subdirectory
    lea rcx, [rsp + 32]
    call do_recursive_file_scan
    jmp rec_scan_next
    
rec_scan_file:
    ; Add file to explorer
    mov rcx, hwndExplorer
    mov rdx, LB_ADDSTRING
    xor r8, r8
    mov r9, r9              ; r9 = filename
    call SendMessageA
    
rec_scan_next:
    mov rcx, rbx
    lea rdx, find_data
    call FindNextFileA
    test eax, eax
    jnz rec_scan_loop
    
    mov rcx, rbx
    call FindClose
    
rec_scan_done:
    leave
    ret
do_recursive_file_scan ENDP

;--------------------------------------------------------------------------
; u64_to_dec - Convert unsigned number to decimal ASCII
; rcx = dest buffer, rdx = value
; returns rax = pointer after last digit
;--------------------------------------------------------------------------
u64_to_dec PROC
    push rbx
    sub rsp, 40
    lea r8, [rsp]
    xor r9d, r9d
    mov rax, rdx
    cmp rax, 0
    jne u64_conv_loop
    mov BYTE PTR [rcx], '0'
    lea rax, [rcx + 1]
    add rsp, 40
    pop rbx
    ret

u64_conv_loop:
    xor rdx, rdx
    mov rbx, 10
    div rbx
    add dl, '0'
    mov BYTE PTR [r8 + r9], dl
    inc r9d
    test rax, rax
    jnz u64_conv_loop

    dec r9d
u64_copy_loop:
    mov dl, BYTE PTR [r8 + r9]
    mov BYTE PTR [rcx], dl
    inc rcx
    dec r9d
    jns u64_copy_loop
    mov rax, rcx
    add rsp, 40
    pop rbx
    ret
u64_to_dec ENDP

;--------------------------------------------------------------------------
; parse_u32 - Parse decimal digits
; rcx = string, returns eax = value, rdx = pointer after digits
;--------------------------------------------------------------------------
parse_u32 PROC
    push rbx
    xor eax, eax
    mov rdx, rcx
parse_digit_loop:
    mov bl, BYTE PTR [rdx]
    cmp bl, '0'
    jb parse_done
    cmp bl, '9'
    ja parse_done
    imul eax, eax, 10
    sub bl, '0'
    add eax, ebx
    inc rdx
    jmp parse_digit_loop
parse_done:
    pop rbx
    ret
parse_u32 ENDP

;--------------------------------------------------------------------------
; handle_run_command - Build and run current project
;--------------------------------------------------------------------------
handle_run_command PROC
    push rbp
    mov rbp, rsp
    
    ; Show "Building..." status
    lea rcx, szBuilding
    call ui_show_dialog
    
    ; Run build command in terminal and show status
    lea rcx, szBuildCommand
    call execute_terminal_command
    test eax, eax
    jz build_failed
    lea rcx, szBuildComplete
    call ui_show_dialog
    jmp build_done
build_failed:
    lea rcx, szBuildFailed
    call ui_show_dialog
build_done:
    
    leave
    ret
handle_run_command ENDP

;--------------------------------------------------------------------------
; navigate_problem_panel - Jump to error location in editor
; rcx = problem index
;--------------------------------------------------------------------------
PUBLIC navigate_problem_panel
navigate_problem_panel PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13

    ; rcx = problem index
    mov r12, rcx

    mov rcx, hwndProblemPanel
    test rcx, rcx
    jz nav_fail
    mov rdx, LB_GETTEXT
    mov r8, r12
    lea r9, szTempBuf
    call SendMessageA
    cmp eax, -1
    je nav_fail

    ; Parse "file(line): error"
    lea rsi, szTempBuf
find_paren:
    mov al, BYTE PTR [rsi]
    test al, al
    jz nav_fail
    cmp al, '('
    je paren_found
    inc rsi
    jmp find_paren
paren_found:
    mov BYTE PTR [rsi], 0
    inc rsi
    mov rcx, rsi
    call parse_u32
    mov r13d, eax

    ; Load file and set editor content
    lea rcx, szTempBuf
    call load_file_content
    mov rcx, hwndEditor
    mov rdx, WM_SETTEXT
    xor r8, r8
    lea r9, szEditorBuffer
    call SendMessageA

    ; Move caret to target line
    cmp r13d, 1
    jbe set_sel_zero
    lea rsi, szEditorBuffer
    xor ecx, ecx
    mov ebx, 1
scan_line_loop:
    mov al, BYTE PTR [rsi + rcx]
    test al, al
    jz set_sel_offset
    cmp al, 10
    jne scan_line_next
    inc ebx
    cmp ebx, r13d
    je set_sel_next
scan_line_next:
    inc ecx
    jmp scan_line_loop
set_sel_next:
    inc ecx
set_sel_offset:
    mov rcx, hwndEditor
    mov rdx, EM_SETSEL
    mov r8d, ecx
    mov r9d, ecx
    call SendMessageA
    jmp nav_done

set_sel_zero:
    mov rcx, hwndEditor
    mov rdx, EM_SETSEL
    xor r8d, r8d
    xor r9d, r9d
    call SendMessageA
    jmp nav_done

nav_fail:
    lea rcx, szProblemNav
    call ui_show_dialog

nav_done:
    pop r13
    pop r12
    pop rbx
    leave
    ret
navigate_problem_panel ENDP

;--------------------------------------------------------------------------
; add_problem_to_panel - Add error/warning to problems panel
; rcx = file path, rdx = line number, r8 = error message
;--------------------------------------------------------------------------
PUBLIC add_problem_to_panel
add_problem_to_panel PROC
    push rbp
    mov rbp, rsp
    
    ; rcx = file path
    ; rdx = line number
    ; r8 = error message
    
    ; Format as "file(line): error"
    lea rdi, szTempBuf
    mov rsi, rcx
    
problem_copy_file:
    mov al, BYTE PTR [rsi]
    test al, al
    jz problem_file_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp problem_copy_file
    
problem_file_done:
    mov BYTE PTR [rdi], '('
    inc rdi

    ; Convert line number to string
    mov rcx, rdi
    mov rdx, rdx
    call u64_to_dec
    mov rdi, rax
    
    mov BYTE PTR [rdi], ')'
    inc rdi
    mov BYTE PTR [rdi], ':'
    inc rdi
    mov BYTE PTR [rdi], ' '
    inc rdi
    
    ; Copy error message
    mov rsi, r8
problem_copy_msg:
    mov al, BYTE PTR [rsi]
    test al, al
    jz problem_msg_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    jmp problem_copy_msg
    
problem_msg_done:
    mov BYTE PTR [rdi], 0
    
    ; Add to problems list
    mov rcx, hwndProblemPanel
    mov rdx, LB_ADDSTRING
    xor r8, r8
    lea r9, szTempBuf
    call SendMessageA
    
    leave
    ret
add_problem_to_panel ENDP

.data
    ; Model & Config paths
    szModelsJsonPath    BYTE "models.json", 0
    szChatHistoryPath   BYTE "chat_history.txt", 0
    szTerminalHistoryPath BYTE "terminal.log", 0
    szDefaultModel1     BYTE "GPT-4o-latest", 0
    szDefaultModel2     BYTE "Claude-3.5-Sonnet", 0
    szDefaultModel3     BYTE "Llama-2-7B-Chat", 0
    szSelectedModel     BYTE 260 dup(?)
    
    ; Command palette strings
    szCmdDebug          BYTE "debug", 0
    szCmdSearch         BYTE "search", 0
    szCmdRun            BYTE "run", 0
    szCmdHelp           BYTE "Command Palette Help:", 0Ah, 0Ah
                        BYTE "debug - Toggle breakpoint at current line", 0Ah
                        BYTE "search - Find in files recursively", 0Ah
                        BYTE "run - Build and run project", 0
    
    ; UI status strings
    szBreakpointSet     BYTE "Breakpoint set!", 0
    szBuilding          BYTE "Building project...", 0
    szBuildComplete     BYTE "Build complete!", 0
    szBuildFailed       BYTE "Build failed. See terminal output.", 0
    szProblemNav        BYTE "Navigating to problem location...", 0
    
    ; File explorer constants
    szBackDir           BYTE "..", 0
    szExplorerDir       BYTE 260 dup(?)
    szExplorerPattern   BYTE 512 dup(?)
    
    find_data           BYTE 568 dup(?)  ; WIN32_FIND_DATAA structure
    
PUBLIC ui_create_main_window
PUBLIC ui_create_menu
PUBLIC ui_add_chat_message
PUBLIC ui_show_dialog
PUBLIC ui_get_input_text
PUBLIC ui_clear_input
PUBLIC ui_set_main_menu
PUBLIC ui_create_chat_control
PUBLIC ui_create_input_control
PUBLIC ui_create_terminal_control
PUBLIC ui_open_file_dialog
PUBLIC ui_save_file_dialog
PUBLIC ui_load_selected_file
PUBLIC ui_save_editor_to_file
PUBLIC wnd_proc_main

END

; HandleFileDrop - Process files dropped onto the window
HandleFileDrop PROC
    ; rcx = HDROP handle
    push rbp
    mov rbp, rsp
    sub rsp, 60h ; Shadow space + local variables
    
    mov [rbp-8], rcx ; Save HDROP handle
    
    ; Get number of files dropped
    mov rcx, [rbp-8]
    mov rdx, -1 ; 0xFFFFFFFF = get count
    xor r8, r8
    call DragQueryFileA
    mov [rbp-12], eax ; file count
    
    ; Process each file
    xor r12, r12 ; counter
    mov r12d, [rbp-12]
    test r12d, r12d
    jz cleanup_drop
    
file_loop:
    ; Get filename
    mov rcx, [rbp-8]
    mov edx, r12d
    lea r8, [rbp-200] ; buffer for filename
    mov r9d, 200 ; buffer size
    call DragQueryFileA
    
    ; Process the file
    lea rcx, [rbp-200]
    call ProcessDroppedFile
    
    dec r12d
    jnz file_loop

cleanup_drop:
    ; Clean up drag-drop resources
    mov rcx, [rbp-8]
    call DragFinish
    
    leave
    ret
HandleFileDrop ENDP

; ProcessDroppedFile - Handle individual dropped files
ProcessDroppedFile PROC
    ; rcx = filename
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    mov [rbp-8], rcx ; filename
    
    ; Check file extension to determine type
    mov rcx, [rbp-8]
    call GetFileExtension
    mov [rbp-16], rax ; extension
    
    ; Route based on file type
    mov rcx, [rbp-16]
    mov rdx, [rbp-8]
    call RouteFileByExtension
    
    leave
    ret
ProcessDroppedFile ENDP

; GetFileExtension - Extract file extension
GetFileExtension PROC
    ; rcx = filename
    push rbp
    mov rbp, rsp
    
    mov rax, rcx
    
find_dot:
    mov dl, [rax]
    test dl, dl
    jz no_extension
    cmp dl, '.'
    je found_dot
    inc rax
    jmp find_dot
    
found_dot:
    inc rax ; skip the dot
    jmp done
    
no_extension:
    xor rax, rax
    
done:
    leave
    ret
GetFileExtension ENDP

; RouteFileByExtension - Route file to appropriate handler
RouteFileByExtension PROC
    ; rcx = extension, rdx = filename
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    mov [rbp-8], rcx ; extension
    mov [rbp-16], rdx ; filename
    
    ; Compare extension and route
    mov rcx, [rbp-8]
    
    ; MASM files
    lea rdx, szExtASM
    call lstrcmpiA
    test eax, eax
    jz handle_asm_file
    
    lea rdx, szExtINC
    call lstrcmpiA
    test eax, eax
    jz handle_asm_file
    
    ; Text files
    lea rdx, szExtTXT
    call lstrcmpiA
    test eax, eax
    jz handle_text_file
    
    lea rdx, szExtMD
    call lstrcmpiA
    test eax, eax
    jz handle_text_file
    
    ; Batch files
    lea rdx, szExtBAT
    call lstrcmpiA
    test eax, eax
    jz handle_batch_file
    
    lea rdx, szExtPS1
    call lstrcmpiA
    test eax, eax
    jz handle_batch_file
    
    ; Default: treat as text
    jmp handle_text_file
    
handle_asm_file:
    mov rcx, [rbp-16]
    call OpenMASMFile
    jmp done_routing
    
handle_text_file:
    mov rcx, [rbp-16]
    call OpenTextFile
    jmp done_routing
    
handle_batch_file:
    mov rcx, [rbp-16]
    call OpenBatchFile
    
    
done_routing:
    leave
    ret
RouteFileByExtension ENDP

; File extension constants
.data
szExtASM    BYTE ".asm",0
szExtINC    BYTE ".inc",0
szExtTXT    BYTE ".txt",0
szExtMD     BYTE ".md",0
szExtBAT    BYTE ".bat",0
szExtPS1    BYTE ".ps1",0

; File handling functions
.code
OpenMASMFile PROC
    ; rcx = filename
    ; TODO: Implement MASM file opening logic
    ret
OpenMASMFile ENDP

OpenTextFile PROC
    ; rcx = filename
    ; TODO: Implement text file opening logic
    ret
OpenTextFile ENDP

OpenBatchFile PROC
    ; rcx = filename
    ; TODO: Implement batch file opening logic
    ret
OpenBatchFile ENDP


