; ============================================================================
; UIGGUF_STUBS.ASM - UI GGUF System Stubs (CreateMenuBar, CreateToolbar, etc)
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

PUBLIC UIGguf_CreateMenuBar
PUBLIC UIGguf_CreateToolbar
PUBLIC UIGguf_CreateStatusPane

; Menu IDs expected by `main_complete.asm` (HandleMenuCommand)
IDM_FILE_NEW        EQU 1001
IDM_FILE_OPEN       EQU 1002
IDM_FILE_SAVE       EQU 1003
IDM_FILE_SAVEAS     EQU 1004
IDM_FILE_EXIT       EQU 1010

IDM_EDIT_UNDO       EQU 1101
IDM_EDIT_REDO       EQU 1102
IDM_EDIT_CUT        EQU 1105
IDM_EDIT_COPY       EQU 1106
IDM_EDIT_PASTE      EQU 1107
IDM_EDIT_FIND       EQU 1110
IDM_EDIT_REPLACE    EQU 1111

; Build/AI menu IDs
IDM_BUILD_COMPILE   EQU 1201
IDM_BUILD_OPTIMIZE  EQU 1202
IDM_BUILD_PREPROC   EQU 1203
IDM_AI_AGENT        EQU 1301
IDM_AI_BROWSER      EQU 1302
IDM_AI_INFER        EQU 1303

; Control IDs
ID_TOOLBAR          EQU 100
ID_STATUSBAR        EQU 101

; StatusBar messages (avoid needing full commctrl headers)
SB_SETPARTS         EQU (WM_USER+4)
SB_SETTEXTA         EQU (WM_USER+1)

.data
szClassStatic       db "STATIC",0
szClassStatusBar    db "msctls_statusbar32",0

szMenuFile          db "&File",0
szMenuEdit          db "&Edit",0
szMenuNew           db "&New",0
szMenuOpen          db "&Open...",0
szMenuSave          db "&Save",0
szMenuSaveAs        db "Save &As...",0
szMenuExit          db "E&xit",0

szMenuUndo          db "&Undo",0
szMenuRedo          db "&Redo",0
szMenuCut           db "Cu&t",0
szMenuCopy          db "&Copy",0
szMenuPaste         db "&Paste",0
szMenuFind          db "&Find...",0
szMenuReplace       db "&Replace...",0

szMenuBuild         db "&Build",0
szMenuCompile       db "&Compile",0
szMenuOptimize      db "&Optimize",0
szMenuPreprocess    db "&Preprocess",0

szMenuAI            db "&AI / Tools",0
szMenuRunAgent      db "Run &Agent Loop",0
szMenuOpenBrowser   db "Open &Browser",0
szMenuInfer         db "Run &Inference",0

szToolbarPlaceholder db " ",0
szStatusReady       db "Ready",0
szStatusSpacer      db " ",0

.code

; ============================================================================
; UIGguf_CreateMenuBar - Create menu bar for GGUF loader
; Input: ECX = main window handle
; Output: EAX = menu handle
; ============================================================================
UIGguf_CreateMenuBar PROC hMainWindow:DWORD
    LOCAL hMenu:DWORD
    LOCAL hFile:DWORD
    LOCAL hEdit:DWORD
    LOCAL hBuild:DWORD
    LOCAL hAI:DWORD

    invoke CreateMenu
    mov hMenu, eax
    test eax, eax
    jz @fail

    ; File menu
    invoke CreatePopupMenu
    mov hFile, eax
    test eax, eax
    jz @fail
    invoke AppendMenuA, hFile, MF_STRING, IDM_FILE_NEW, ADDR szMenuNew
    invoke AppendMenuA, hFile, MF_STRING, IDM_FILE_OPEN, ADDR szMenuOpen
    invoke AppendMenuA, hFile, MF_STRING, IDM_FILE_SAVE, ADDR szMenuSave
    invoke AppendMenuA, hFile, MF_STRING, IDM_FILE_SAVEAS, ADDR szMenuSaveAs
    invoke AppendMenuA, hFile, MF_SEPARATOR, 0, 0
    invoke AppendMenuA, hFile, MF_STRING, IDM_FILE_EXIT, ADDR szMenuExit
    invoke AppendMenuA, hMenu, MF_POPUP, hFile, ADDR szMenuFile

    ; Edit menu
    invoke CreatePopupMenu
    mov hEdit, eax
    test eax, eax
    jz @fail
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_UNDO, ADDR szMenuUndo
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_REDO, ADDR szMenuRedo
    invoke AppendMenuA, hEdit, MF_SEPARATOR, 0, 0
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_CUT, ADDR szMenuCut
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_COPY, ADDR szMenuCopy
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_PASTE, ADDR szMenuPaste
    invoke AppendMenuA, hEdit, MF_SEPARATOR, 0, 0
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_FIND, ADDR szMenuFind
    invoke AppendMenuA, hEdit, MF_STRING, IDM_EDIT_REPLACE, ADDR szMenuReplace
    invoke AppendMenuA, hMenu, MF_POPUP, hEdit, ADDR szMenuEdit

    ; Build menu
    invoke CreatePopupMenu
    mov hBuild, eax
    test eax, eax
    jz @fail
    invoke AppendMenuA, hBuild, MF_STRING, IDM_BUILD_COMPILE, ADDR szMenuCompile
    invoke AppendMenuA, hBuild, MF_STRING, IDM_BUILD_OPTIMIZE, ADDR szMenuOptimize
    invoke AppendMenuA, hBuild, MF_STRING, IDM_BUILD_PREPROC, ADDR szMenuPreprocess
    invoke AppendMenuA, hMenu, MF_POPUP, hBuild, ADDR szMenuBuild

    ; AI / Tools menu
    invoke CreatePopupMenu
    mov hAI, eax
    test eax, eax
    jz @fail
    invoke AppendMenuA, hAI, MF_STRING, IDM_AI_AGENT, ADDR szMenuRunAgent
    invoke AppendMenuA, hAI, MF_STRING, IDM_AI_BROWSER, ADDR szMenuOpenBrowser
    invoke AppendMenuA, hAI, MF_STRING, IDM_AI_INFER, ADDR szMenuInfer
    invoke AppendMenuA, hMenu, MF_POPUP, hAI, ADDR szMenuAI

    ; Attach
    invoke SetMenu, hMainWindow, hMenu
    mov eax, hMenu
    ret

@fail:
    xor eax, eax
    ret
UIGguf_CreateMenuBar ENDP

; ============================================================================
; UIGguf_CreateToolbar - Create toolbar with buttons
; Input: ECX = parent window handle
; Output: EAX = toolbar handle
; ============================================================================
UIGguf_CreateToolbar PROC hParent:DWORD
    ; Minimal visual toolbar: a STATIC child occupying the toolbar area.
    ; Main window handles layout/resizing.
    invoke GetModuleHandleA, NULL
    invoke CreateWindowExA, 0, ADDR szClassStatic, ADDR szToolbarPlaceholder, \
        WS_CHILD or WS_VISIBLE or SS_NOTIFY, \
        0, 0, 0, 0, \
        hParent, ID_TOOLBAR, eax, NULL
    ret
UIGguf_CreateToolbar ENDP

; ============================================================================
; UIGguf_CreateStatusPane - Create status bar with segments
; Input: ECX = parent window handle
; Output: EAX = status bar handle
; ============================================================================
UIGguf_CreateStatusPane PROC hParent:DWORD
    LOCAL hStatus:DWORD
    LOCAL parts[1]:DWORD

    ; Real status bar control
    invoke GetModuleHandleA, NULL
    invoke CreateWindowExA, 0, ADDR szClassStatusBar, NULL, \
        WS_CHILD or WS_VISIBLE, \
        0, 0, 0, 0, \
        hParent, ID_STATUSBAR, eax, NULL
    mov hStatus, eax
    test eax, eax
    jz @fail

    ; Single stretching part
    mov parts[0*4], -1
    invoke SendMessageA, hStatus, SB_SETPARTS, 1, ADDR parts
    invoke SendMessageA, hStatus, SB_SETTEXTA, 0, ADDR szStatusReady

    mov eax, hStatus
    ret

@fail:
    xor eax, eax
    ret
UIGguf_CreateStatusPane ENDP

END
