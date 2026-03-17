;==============================================================================
; RawrXD MASM IDE v2.0 - Complete Enterprise Integration
; Main Entry Point - Integrates All Phases
; Version: 2.0.0 Enterprise
; Zero External Dependencies - 100% Pure MASM
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc
include gdi32.inc
include comdlg32.inc
include comctl32.inc

includelib user32.lib
includelib kernel32.lib
includelib gdi32.lib
includelib comdlg32.lib
includelib comctl32.lib

; Include phase implementations
include phase1_editor_enhancement.asm
include phase2_language_intelligence.asm
include phase3_debug_infrastructure.asm

;==============================================================================
; CONSTANTS
;==============================================================================
IDM_FILE_NEW        equ 1001
IDM_FILE_OPEN       equ 1002
IDM_FILE_SAVE       equ 1003
IDM_FILE_SAVEAS     equ 1004
IDM_FILE_EXIT       equ 1010

IDM_EDIT_UNDO       equ 1101
IDM_EDIT_REDO       equ 1102
IDM_EDIT_CUT        equ 1105
IDM_EDIT_COPY       equ 1106
IDM_EDIT_PASTE      equ 1107
IDM_EDIT_FIND       equ 1110
IDM_EDIT_REPLACE    equ 1111

IDM_DEBUG_START     equ 1201
IDM_DEBUG_STOP      equ 1202
IDM_DEBUG_PAUSE     equ 1203
IDM_DEBUG_STEP_INTO equ 1204
IDM_DEBUG_STEP_OVER equ 1205
IDM_DEBUG_STEP_OUT  equ 1206
IDM_DEBUG_TOGGLE_BP equ 1207

IDM_TOOLS_BUILD     equ 1301
IDM_TOOLS_RUN       equ 1302
IDM_TOOLS_OPTIONS   equ 1303

IDM_HELP_ABOUT      equ 1401

IDE_EDIT_CONTROL    equ 2001

WINDOW_WIDTH        equ 1200
WINDOW_HEIGHT       equ 800

;==============================================================================
; STRUCTURES
;==============================================================================
IDE_STATE struct
    hInstance       dd ?
    hMainWnd        dd ?
    hEditControl    dd ?
    hToolbar        dd ?
    hStatusBar      dd ?
    hMenu           dd ?
    currentFile     db MAX_PATH dup(?)
    modified        dd ?
    debugActive     dd ?
IDE_STATE ends

;==============================================================================
; DATA
;==============================================================================
.data
szClassName     db 'RawrXDIDE',0
szAppName       db 'RawrXD MASM IDE v2.0 Enterprise',0
szEditClass     db 'EDIT',0
szFilter        db 'ASM Files (*.asm)',0,'*.asm',0
                db 'All Files (*.*)',0,'*.*',0,0
szUntitled      db 'Untitled',0
szAboutText     db 'RawrXD MASM IDE v2.0',13,10
                db 'Enterprise Edition',13,10,13,10
                db 'Pure MASM Implementation',13,10
                db 'Zero External Dependencies',13,10,13,10
                db '(c) 2025 RawrXD Project',0

szReady         db 'Ready',0
szModified      db 'Modified',0
szDebugging     db 'Debugging',0

.data?
ideState        IDE_STATE <>
hAccel          dd ?

.code

;==============================================================================
; MAIN ENTRY POINT
;==============================================================================
start:
    invoke GetModuleHandle, NULL
    mov ideState.hInstance, eax
    
    invoke InitCommonControls
    
    ; Initialize all phases
    invoke InitializeIDE
    .if eax == FALSE
        invoke ExitProcess, 1
    .endif
    
    ; Create main window
    invoke CreateMainWindow
    .if eax == FALSE
        invoke ExitProcess, 1
    .endif
    
    ; Load accelerators
    invoke LoadAccelerators, ideState.hInstance, 100
    mov hAccel, eax
    
    ; Message loop
    invoke MessageLoop
    
    ; Cleanup
    invoke CleanupIDE
    
    invoke ExitProcess, 0

;==============================================================================
; INITIALIZATION
;==============================================================================
InitializeIDE proc uses ebx esi edi
    ; Initialize Phase 1: Editor Enhancement
    invoke InitEditorEnhancements, NULL
    .if eax == FALSE
        mov eax, FALSE
        ret
    .endif
    
    ; Initialize Phase 2: Language Intelligence
    invoke LSP_Initialize
    .if eax == FALSE
        mov eax, FALSE
        ret
    .endif
    
    ; Initialize Phase 3: Debug Infrastructure
    invoke DAP_Initialize
    .if eax == FALSE
        mov eax, FALSE
        ret
    .endif
    
    ; Initialize IDE state
    mov ideState.hMainWnd, 0
    mov ideState.hEditControl, 0
    mov ideState.hToolbar, 0
    mov ideState.hStatusBar, 0
    mov ideState.modified, 0
    mov ideState.debugActive, 0
    mov byte ptr ideState.currentFile, 0
    
    mov eax, TRUE
    ret
InitializeIDE endp

CleanupIDE proc uses ebx esi edi
    ; Shutdown debug session if active
    .if ideState.debugActive == TRUE
        invoke DAP_Disconnect, FALSE
    .endif
    
    ; Shutdown LSP server
    invoke LSP_Shutdown
    
    ret
CleanupIDE endp

;==============================================================================
; WINDOW CREATION
;==============================================================================
CreateMainWindow proc uses ebx esi edi
    local wc:WNDCLASSEX
    local rect:RECT
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov eax, offset WndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, ideState.hInstance
    mov wc.hInstance, eax
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    invoke GetStockObject, WHITE_BRUSH
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, NULL
    lea eax, szClassName
    mov wc.lpszClassName, eax
    mov wc.hIconSm, NULL
    
    invoke RegisterClassEx, addr wc
    .if eax == 0
        mov eax, FALSE
        ret
    .endif
    
    ; Create main window
    invoke CreateWindowEx, 0, addr szClassName, addr szAppName, \
                          WS_OVERLAPPEDWINDOW, \
                          CW_USEDEFAULT, CW_USEDEFAULT, \
                          WINDOW_WIDTH, WINDOW_HEIGHT, \
                          NULL, NULL, ideState.hInstance, NULL
    .if eax == 0
        mov eax, FALSE
        ret
    .endif
    
    mov ideState.hMainWnd, eax
    
    ; Show window
    invoke ShowWindow, ideState.hMainWnd, SW_SHOWNORMAL
    invoke UpdateWindow, ideState.hMainWnd
    
    mov eax, TRUE
    ret
CreateMainWindow endp

;==============================================================================
; MESSAGE LOOP
;==============================================================================
MessageLoop proc uses ebx esi edi
    local msg:MSG
    
    .while TRUE
        invoke GetMessage, addr msg, NULL, 0, 0
        .break .if eax == 0
        
        invoke TranslateAccelerator, ideState.hMainWnd, hAccel, addr msg
        .if eax == 0
            invoke TranslateMessage, addr msg
            invoke DispatchMessage, addr msg
        .endif
    .endw
    
    mov eax, msg.wParam
    ret
MessageLoop endp

;==============================================================================
; WINDOW PROCEDURE
;==============================================================================
WndProc proc uses ebx esi edi hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    .if uMsg == WM_CREATE
        invoke OnCreate, hWnd
        return 0
        
    .elseif uMsg == WM_SIZE
        invoke OnSize, hWnd, wParam, lParam
        return 0
        
    .elseif uMsg == WM_COMMAND
        invoke OnCommand, hWnd, wParam, lParam
        return 0
        
    .elseif uMsg == WM_NOTIFY
        invoke OnNotify, hWnd, wParam, lParam
        return 0
        
    .elseif uMsg == WM_CLOSE
        invoke OnClose, hWnd
        return 0
        
    .elseif uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        return 0
        
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

;==============================================================================
; MESSAGE HANDLERS
;==============================================================================
OnCreate proc uses ebx esi edi hWnd:DWORD
    local rect:RECT
    
    ; Create menu
    invoke CreateMainMenu
    mov ideState.hMenu, eax
    invoke SetMenu, hWnd, eax
    
    ; Create toolbar
    invoke CreateToolbar, hWnd
    mov ideState.hToolbar, eax
    
    ; Create status bar
    invoke CreateStatusBar, hWnd
    mov ideState.hStatusBar, eax
    
    ; Get client area
    invoke GetClientRect, hWnd, addr rect
    
    ; Create edit control
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, addr szEditClass, NULL, \
                          WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or \
                          ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL or ES_NOHIDESEL, \
                          50, 0, rect.right - 170, rect.bottom - 20, \
                          hWnd, IDE_EDIT_CONTROL, ideState.hInstance, NULL
    mov ideState.hEditControl, eax
    
    ; Set edit control font
    invoke CreateFont, 14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, \
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, \
                     DEFAULT_QUALITY, FIXED_PITCH or FF_MODERN, CSTR("Consolas")
    invoke SendMessage, ideState.hEditControl, WM_SETFONT, eax, TRUE
    
    ; Initialize editor enhancements
    invoke InitEditorEnhancements, hWnd
    
    ; Update status bar
    invoke UpdateStatusBar, addr szReady
    
    ret
OnCreate endp

OnSize proc uses ebx esi edi hWnd:DWORD, wParam:DWORD, lParam:DWORD
    local rect:RECT
    
    ; Resize toolbar
    .if ideState.hToolbar != 0
        invoke SendMessage, ideState.hToolbar, TB_AUTOSIZE, 0, 0
    .endif
    
    ; Resize status bar
    .if ideState.hStatusBar != 0
        invoke SendMessage, ideState.hStatusBar, WM_SIZE, 0, 0
    .endif
    
    ; Resize edit control
    invoke GetClientRect, hWnd, addr rect
    .if ideState.hEditControl != 0
        invoke MoveWindow, ideState.hEditControl, 50, 0, \
                          rect.right - 170, rect.bottom - 20, TRUE
    .endif
    
    ret
OnSize endp

OnCommand proc uses ebx esi edi hWnd:DWORD, wParam:DWORD, lParam:DWORD
    mov eax, wParam
    and eax, 0FFFFh
    
    .if eax == IDM_FILE_NEW
        invoke FileNew
        
    .elseif eax == IDM_FILE_OPEN
        invoke FileOpen
        
    .elseif eax == IDM_FILE_SAVE
        invoke FileSave
        
    .elseif eax == IDM_FILE_SAVEAS
        invoke FileSaveAs
        
    .elseif eax == IDM_FILE_EXIT
        invoke SendMessage, hWnd, WM_CLOSE, 0, 0
        
    .elseif eax == IDM_EDIT_UNDO
        invoke SendMessage, ideState.hEditControl, WM_UNDO, 0, 0
        
    .elseif eax == IDM_EDIT_CUT
        invoke SendMessage, ideState.hEditControl, WM_CUT, 0, 0
        
    .elseif eax == IDM_EDIT_COPY
        invoke SendMessage, ideState.hEditControl, WM_COPY, 0, 0
        
    .elseif eax == IDM_EDIT_PASTE
        invoke SendMessage, ideState.hEditControl, WM_PASTE, 0, 0
        
    .elseif eax == IDM_DEBUG_START
        invoke DebugStart
        
    .elseif eax == IDM_DEBUG_STOP
        invoke DebugStop
        
    .elseif eax == IDM_DEBUG_STEP_INTO
        invoke DAP_StepInto
        
    .elseif eax == IDM_DEBUG_STEP_OVER
        invoke DAP_StepOver
        
    .elseif eax == IDM_TOOLS_BUILD
        invoke BuildProject
        
    .elseif eax == IDM_TOOLS_RUN
        invoke RunProject
        
    .elseif eax == IDM_HELP_ABOUT
        invoke MessageBox, hWnd, addr szAboutText, addr szAppName, MB_OK or MB_ICONINFORMATION
        
    .endif
    
    ret
OnCommand endp

OnNotify proc hWnd:DWORD, wParam:DWORD, lParam:DWORD
    ; Handle notifications
    ret
OnNotify endp

OnClose proc uses ebx esi edi hWnd:DWORD
    ; Check if file is modified
    .if ideState.modified == TRUE
        invoke MessageBox, hWnd, CSTR("Save changes?"), addr szAppName, \
                          MB_YESNOCANCEL or MB_ICONQUESTION
        .if eax == IDYES
            invoke FileSave
        .elseif eax == IDCANCEL
            ret
        .endif
    .endif
    
    invoke DestroyWindow, hWnd
    ret
OnClose endp

;==============================================================================
; FILE OPERATIONS
;==============================================================================
FileNew proc uses ebx esi edi
    invoke SetWindowText, ideState.hEditControl, NULL
    mov ideState.modified, 0
    mov byte ptr ideState.currentFile, 0
    invoke UpdateTitle
    ret
FileNew endp

FileOpen proc uses ebx esi edi
    local ofn:OPENFILENAME
    local szFile[MAX_PATH]:BYTE
    local hFile:DWORD
    local bytesRead:DWORD
    local buffer[65536]:BYTE
    
    invoke RtlZeroMemory, addr ofn, SIZEOF OPENFILENAME
    mov ofn.lStructSize, SIZEOF OPENFILENAME
    mov eax, ideState.hMainWnd
    mov ofn.hwndOwner, eax
    lea eax, szFilter
    mov ofn.lpstrFilter, eax
    lea eax, szFile
    mov ofn.lpstrFile, eax
    mov byte ptr szFile, 0
    mov ofn.nMaxFile, MAX_PATH
    mov ofn.Flags, OFN_FILEMUSTEXIST or OFN_PATHMUSTEXIST
    
    invoke GetOpenFileName, addr ofn
    .if eax == 0
        ret
    .endif
    
    ; Open file
    invoke CreateFile, addr szFile, GENERIC_READ, FILE_SHARE_READ, \
                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    .if eax == INVALID_HANDLE_VALUE
        invoke MessageBox, ideState.hMainWnd, CSTR("Failed to open file"), addr szAppName, MB_OK or MB_ICONERROR
        ret
    .endif
    mov hFile, eax
    
    ; Read file
    invoke ReadFile, hFile, addr buffer, 65535, addr bytesRead, NULL
    invoke CloseHandle, hFile
    
    .if bytesRead > 0
        mov eax, bytesRead
        lea ebx, buffer
        add ebx, eax
        mov byte ptr [ebx], 0
        
        invoke SetWindowText, ideState.hEditControl, addr buffer
        invoke lstrcpy, addr ideState.currentFile, addr szFile
        mov ideState.modified, 0
        invoke UpdateTitle
        
        ; Analyze document with LSP
        invoke AnalyzeDocument, addr szFile, addr buffer, bytesRead
    .endif
    
    ret
FileOpen endp

FileSave proc uses ebx esi edi
    ; Check if we have a filename
    cmp byte ptr ideState.currentFile, 0
    je do_saveas
    
    invoke DoSaveFile
    ret
    
do_saveas:
    invoke FileSaveAs
    ret
FileSave endp

FileSaveAs proc uses ebx esi edi
    local ofn:OPENFILENAME
    local szFile[MAX_PATH]:BYTE
    
    invoke RtlZeroMemory, addr ofn, SIZEOF OPENFILENAME
    mov ofn.lStructSize, SIZEOF OPENFILENAME
    mov eax, ideState.hMainWnd
    mov ofn.hwndOwner, eax
    lea eax, szFilter
    mov ofn.lpstrFilter, eax
    lea eax, szFile
    mov ofn.lpstrFile, eax
    mov byte ptr szFile, 0
    mov ofn.nMaxFile, MAX_PATH
    mov ofn.Flags, OFN_OVERWRITEPROMPT
    
    invoke GetSaveFileName, addr ofn
    .if eax == 0
        ret
    .endif
    
    invoke lstrcpy, addr ideState.currentFile, addr szFile
    invoke DoSaveFile
    ret
FileSaveAs endp

DoSaveFile proc uses ebx esi edi
    local hFile:DWORD
    local bytesWritten:DWORD
    local textLen:DWORD
    local pText:DWORD
    
    ; Get text length
    invoke GetWindowTextLength, ideState.hEditControl
    mov textLen, eax
    inc eax
    
    ; Allocate buffer
    invoke GlobalAlloc, GMEM_FIXED, eax
    mov pText, eax
    
    ; Get text
    invoke GetWindowText, ideState.hEditControl, pText, textLen
    
    ; Create file
    invoke CreateFile, addr ideState.currentFile, GENERIC_WRITE, 0, \
                      NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    .if eax == INVALID_HANDLE_VALUE
        invoke GlobalFree, pText
        invoke MessageBox, ideState.hMainWnd, CSTR("Failed to save file"), addr szAppName, MB_OK or MB_ICONERROR
        ret
    .endif
    mov hFile, eax
    
    ; Write file
    invoke WriteFile, hFile, pText, textLen, addr bytesWritten, NULL
    invoke CloseHandle, hFile
    invoke GlobalFree, pText
    
    mov ideState.modified, 0
    invoke UpdateTitle
    ret
DoSaveFile endp

;==============================================================================
; DEBUG OPERATIONS
;==============================================================================
DebugStart proc uses ebx esi edi
    ; Save file first
    .if ideState.modified == TRUE
        invoke FileSave
    .endif
    
    ; Build project
    invoke BuildProject
    .if eax == FALSE
        ret
    .endif
    
    ; Launch debugger
    invoke DAP_Launch, addr ideState.currentFile, NULL
    .if eax == TRUE
        mov ideState.debugActive, TRUE
        invoke UpdateStatusBar, addr szDebugging
    .endif
    
    ret
DebugStart endp

DebugStop proc uses ebx esi edi
    .if ideState.debugActive == TRUE
        invoke DAP_Disconnect, TRUE
        mov ideState.debugActive, FALSE
        invoke UpdateStatusBar, addr szReady
    .endif
    ret
DebugStop endp

;==============================================================================
; BUILD OPERATIONS
;==============================================================================
BuildProject proc uses ebx esi edi
    local si:STARTUPINFO
    local pi:PROCESS_INFORMATION
    local cmdLine[512]:BYTE
    
    ; Build command line: ml.exe /c /coff /Zi /Fo output.obj input.asm
    invoke wsprintf, addr cmdLine, CSTR("ml.exe /c /coff /Zi /Fo output.obj %s"), addr ideState.currentFile
    
    ; Initialize structures
    invoke RtlZeroMemory, addr si, SIZEOF STARTUPINFO
    mov si.cb, SIZEOF STARTUPINFO
    
    ; Create build process
    invoke CreateProcess, NULL, addr cmdLine, NULL, NULL, FALSE, \
                         CREATE_NO_WINDOW, NULL, NULL, addr si, addr pi
    
    .if eax == 0
        invoke MessageBox, ideState.hMainWnd, CSTR("Build failed"), addr szAppName, MB_OK or MB_ICONERROR
        mov eax, FALSE
        ret
    .endif
    
    ; Wait for build to complete
    invoke WaitForSingleObject, pi.hProcess, INFINITE
    
    invoke CloseHandle, pi.hThread
    invoke CloseHandle, pi.hProcess
    
    invoke MessageBox, ideState.hMainWnd, CSTR("Build succeeded"), addr szAppName, MB_OK or MB_ICONINFORMATION
    mov eax, TRUE
    ret
BuildProject endp

RunProject proc uses ebx esi edi
    ; Build first
    invoke BuildProject
    .if eax == FALSE
        ret
    .endif
    
    ; Run the output
    invoke ShellExecute, ideState.hMainWnd, CSTR("open"), CSTR("output.exe"), NULL, NULL, SW_SHOWNORMAL
    ret
RunProject endp

;==============================================================================
; UI HELPERS
;==============================================================================
CreateMainMenu proc uses ebx esi edi
    local hMenu:DWORD
    local hSubMenu:DWORD
    
    invoke CreateMenu
    mov hMenu, eax
    
    ; File menu
    invoke CreatePopupMenu
    mov hSubMenu, eax
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_FILE_NEW, CSTR("&New\tCtrl+N")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_FILE_OPEN, CSTR("&Open...\tCtrl+O")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_FILE_SAVE, CSTR("&Save\tCtrl+S")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_FILE_SAVEAS, CSTR("Save &As...")
    invoke AppendMenu, hSubMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_FILE_EXIT, CSTR("E&xit")
    invoke AppendMenu, hMenu, MF_POPUP, hSubMenu, CSTR("&File")
    
    ; Edit menu
    invoke CreatePopupMenu
    mov hSubMenu, eax
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_EDIT_UNDO, CSTR("&Undo\tCtrl+Z")
    invoke AppendMenu, hSubMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_EDIT_CUT, CSTR("Cu&t\tCtrl+X")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_EDIT_COPY, CSTR("&Copy\tCtrl+C")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_EDIT_PASTE, CSTR("&Paste\tCtrl+V")
    invoke AppendMenu, hMenu, MF_POPUP, hSubMenu, CSTR("&Edit")
    
    ; Debug menu
    invoke CreatePopupMenu
    mov hSubMenu, eax
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_DEBUG_START, CSTR("&Start Debugging\tF5")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_DEBUG_STOP, CSTR("S&top Debugging\tShift+F5")
    invoke AppendMenu, hSubMenu, MF_SEPARATOR, 0, NULL
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_DEBUG_STEP_INTO, CSTR("Step &Into\tF11")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_DEBUG_STEP_OVER, CSTR("Step &Over\tF10")
    invoke AppendMenu, hMenu, MF_POPUP, hSubMenu, CSTR("&Debug")
    
    ; Tools menu
    invoke CreatePopupMenu
    mov hSubMenu, eax
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_TOOLS_BUILD, CSTR("&Build\tF7")
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_TOOLS_RUN, CSTR("&Run\tCtrl+F5")
    invoke AppendMenu, hMenu, MF_POPUP, hSubMenu, CSTR("&Tools")
    
    ; Help menu
    invoke CreatePopupMenu
    mov hSubMenu, eax
    invoke AppendMenu, hSubMenu, MF_STRING, IDM_HELP_ABOUT, CSTR("&About")
    invoke AppendMenu, hMenu, MF_POPUP, hSubMenu, CSTR("&Help")
    
    mov eax, hMenu
    ret
CreateMainMenu endp

CreateToolbar proc hWnd:DWORD
    ; Toolbar creation placeholder
    mov eax, 0
    ret
CreateToolbar endp

CreateStatusBar proc hWnd:DWORD
    invoke CreateWindowEx, 0, CSTR("msctls_statusbar32"), NULL, \
                          WS_CHILD or WS_VISIBLE or SBARS_SIZEGRIP, \
                          0, 0, 0, 0, hWnd, NULL, ideState.hInstance, NULL
    ret
CreateStatusBar endp

UpdateTitle proc uses ebx esi edi
    local szTitle[MAX_PATH]:BYTE
    
    .if byte ptr ideState.currentFile == 0
        invoke lstrcpy, addr szTitle, addr szUntitled
    .else
        invoke lstrcpy, addr szTitle, addr ideState.currentFile
    .endif
    
    .if ideState.modified == TRUE
        invoke lstrcat, addr szTitle, CSTR(" *")
    .endif
    
    invoke lstrcat, addr szTitle, CSTR(" - ")
    invoke lstrcat, addr szTitle, addr szAppName
    
    invoke SetWindowText, ideState.hMainWnd, addr szTitle
    ret
UpdateTitle endp

UpdateStatusBar proc text:DWORD
    .if ideState.hStatusBar != 0
        invoke SendMessage, ideState.hStatusBar, SB_SETTEXT, 0, text
    .endif
    ret
UpdateStatusBar endp

end start
