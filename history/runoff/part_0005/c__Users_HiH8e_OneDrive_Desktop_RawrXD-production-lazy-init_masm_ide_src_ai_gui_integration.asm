; ai_gui_integration.asm - GUI Integration for Advanced AI Features
; Wires AI engines to menus, dialogs, and widgets
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc

includelib kernel32.lib
includelib user32.lib
includelib comctl32.lib

PUBLIC AIGui_Init
PUBLIC AIGui_CreateMenus
PUBLIC AIGui_ShowRefactorDialog
PUBLIC AIGui_ShowNLDialog
PUBLIC AIGui_ShowContextPanel
PUBLIC AIGui_UpdateStatus

EXTERN AIContext_Init:PROC
EXTERN AIContext_BuildGraph:PROC
EXTERN AIContext_GetSuggestions:PROC
EXTERN AIRefactor_Rename:PROC
EXTERN AIRefactor_ExtractFunction:PROC
EXTERN AIRefactor_PreviewChanges:PROC
EXTERN AIRefactor_ApplyChanges:PROC
EXTERN AINL_ParseIntent:PROC
EXTERN AINL_GenerateCode:PROC
EXTERN AINL_ExplainCode:PROC

; Menu IDs
IDM_AI_ANALYZE_CODE         EQU 5001
IDM_AI_BUILD_GRAPH          EQU 5002
IDM_AI_FIND_REFS            EQU 5003
IDM_AI_RENAME               EQU 5010
IDM_AI_EXTRACT_FUNC         EQU 5011
IDM_AI_INLINE_FUNC          EQU 5012
IDM_AI_NL_TO_CODE           EQU 5020
IDM_AI_EXPLAIN_CODE         EQU 5021
IDM_AI_TRANSLATE            EQU 5022
IDM_AI_SHOW_CONTEXT         EQU 5030

; Window handles
.data
g_hMainWnd          dd 0
g_hContextPanel     dd 0
g_hRefactorDialog   dd 0
g_hNLDialog         dd 0
g_hStatusBar        dd 0

; Status messages
szAnalyzing         db "Analyzing codebase...",0
szBuildingGraph     db "Building context graph...",0
szRefactoring       db "Refactoring code...",0
szGenerating        db "Generating code...",0
szComplete          db "Complete!",0

.code

; ============================================================
; AIGui_Init - Initialize AI GUI components
; Input:  RCX = main window handle
; ============================================================
AIGui_Init PROC
    push rbx
    sub rsp, 32
    
    mov [g_hMainWnd], ecx
    
    ; Initialize AI engines
    call AIContext_Init
    call AIRefactor_Init
    call AINL_Init
    
    ; Create UI components
    call AIGui_CreateMenus
    call CreateContextPanel
    call CreateRefactorDialog
    call CreateNLDialog
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
AIGui_Init ENDP

; ============================================================
; AIGui_CreateMenus - Add AI menus to main menu bar
; ============================================================
AIGui_CreateMenus PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov ecx, [g_hMainWnd]
    invoke GetMenu, ecx
    mov rbx, rax
    
    ; Create AI submenu
    invoke CreatePopupMenu
    mov rsi, rax
    
    ; Context Analysis submenu
    invoke CreatePopupMenu
    mov rdi, rax
    
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_ANALYZE_CODE, "Analyze Current File"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_BUILD_GRAPH, "Build Codebase Graph"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_FIND_REFS, "Find All References"
    invoke AppendMenuA, rdi, MF_SEPARATOR, 0, 0
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_SHOW_CONTEXT, "Show Context Panel"
    
    invoke AppendMenuA, rsi, MF_POPUP, rdi, "Context Analysis"
    
    ; Refactoring submenu
    invoke CreatePopupMenu
    mov rdi, rax
    
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_RENAME, "Rename Symbol...\tCtrl+R"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_EXTRACT_FUNC, "Extract Function...\tCtrl+E"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_INLINE_FUNC, "Inline Function"
    
    invoke AppendMenuA, rsi, MF_POPUP, rdi, "Refactoring"
    
    ; Natural Language submenu
    invoke CreatePopupMenu
    mov rdi, rax
    
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_NL_TO_CODE, "Generate from Description...\tCtrl+G"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_EXPLAIN_CODE, "Explain Selected Code\tCtrl+?"
    invoke AppendMenuA, rdi, MF_STRING, IDM_AI_TRANSLATE, "Translate to Language..."
    
    invoke AppendMenuA, rsi, MF_POPUP, rdi, "Natural Language"
    
    ; Add to main menu
    invoke AppendMenuA, rbx, MF_POPUP, rsi, "AI Tools"
    
    ; Redraw menu bar
    mov ecx, [g_hMainWnd]
    invoke DrawMenuBar, ecx
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
AIGui_CreateMenus ENDP

; ============================================================
; AIGui_ShowRefactorDialog - Show refactoring dialog
; Input:  RCX = refactor type (REFACTOR_RENAME, etc)
; ============================================================
AIGui_ShowRefactorDialog PROC
    push rbx
    push rsi
    sub rsp, 48
    
    mov ebx, ecx              ; Refactor type
    
    ; Show appropriate dialog
    cmp ebx, 1                ; REFACTOR_RENAME
    je @show_rename
    cmp ebx, 2                ; REFACTOR_EXTRACT_FUNC
    je @show_extract
    jmp @done
    
@show_rename:
    ; Create rename dialog
    mov ecx, [g_hMainWnd]
    invoke DialogBoxParamA, 0, 100, ecx, RenameDialogProc, 0
    jmp @done
    
@show_extract:
    ; Create extract function dialog
    mov ecx, [g_hMainWnd]
    invoke DialogBoxParamA, 0, 101, ecx, ExtractDialogProc, 0
    
@done:
    add rsp, 48
    pop rsi
    pop rbx
    ret
AIGui_ShowRefactorDialog ENDP

; ============================================================
; AIGui_ShowNLDialog - Show natural language input dialog
; ============================================================
AIGui_ShowNLDialog PROC
    push rbx
    sub rsp, 32
    
    mov ecx, [g_hMainWnd]
    invoke DialogBoxParamA, 0, 102, ecx, NLDialogProc, 0
    
    add rsp, 32
    pop rbx
    ret
AIGui_ShowNLDialog ENDP

; ============================================================
; AIGui_ShowContextPanel - Toggle context analysis panel
; ============================================================
AIGui_ShowContextPanel PROC
    push rbx
    sub rsp, 32
    
    mov eax, [g_hContextPanel]
    test eax, eax
    jnz @toggle
    
    ; Create panel
    call CreateContextPanel
    mov [g_hContextPanel], eax
    
@toggle:
    mov eax, [g_hContextPanel]
    invoke IsWindowVisible, eax
    test eax, eax
    jnz @hide
    
@show:
    mov ecx, [g_hContextPanel]
    invoke ShowWindow, ecx, SW_SHOW
    jmp @done
    
@hide:
    mov ecx, [g_hContextPanel]
    invoke ShowWindow, ecx, SW_HIDE
    
@done:
    add rsp, 32
    pop rbx
    ret
AIGui_ShowContextPanel ENDP

; ============================================================
; AIGui_UpdateStatus - Update status bar with AI operation
; Input:  RCX = status message
; ============================================================
AIGui_UpdateStatus PROC
    push rbx
    sub rsp, 32
    
    mov ebx, ecx
    
    mov eax, [g_hStatusBar]
    test eax, eax
    jz @done
    
    invoke SendMessageA, eax, SB_SETTEXT, 0, ebx
    
@done:
    add rsp, 32
    pop rbx
    ret
AIGui_UpdateStatus ENDP

; ============================================================
; Dialog procedures
; ============================================================
RenameDialogProc PROC hDlg:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    cmp uMsg, WM_INITDIALOG
    je @init
    cmp uMsg, WM_COMMAND
    je @command
    xor eax, eax
    ret
    
@init:
    ; Initialize rename dialog
    mov eax, 1
    ret
    
@command:
    mov eax, wParam
    cmp ax, IDOK
    je @ok
    cmp ax, IDCANCEL
    je @cancel
    xor eax, eax
    ret
    
@ok:
    ; Get old and new names
    push hDlg
    push 1001
    lea rax, [esp+16]
    push rax
    push 256
    call GetDlgItemTextA
    
    push hDlg
    push 1002
    lea rax, [esp+16]
    push rax
    push 256
    call GetDlgItemTextA
    
    ; Call refactor engine
    lea rcx, [esp+16]
    lea rdx, [esp+16]
    xor r8, r8
    call AIRefactor_Rename
    
    ; Apply changes
    mov rcx, rax
    call AIRefactor_ApplyChanges
    
    push hDlg
    push 0
    call EndDialog
    mov eax, 1
    ret
    
@cancel:
    push hDlg
    push 0
    call EndDialog
    mov eax, 1
    ret
RenameDialogProc ENDP

ExtractDialogProc PROC hDlg:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    cmp uMsg, WM_INITDIALOG
    je @init
    cmp uMsg, WM_COMMAND
    je @command
    xor eax, eax
    ret
    
@init:
    mov eax, 1
    ret
    
@command:
    mov eax, wParam
    cmp ax, IDOK
    je @ok
    cmp ax, IDCANCEL
    je @cancel
    xor eax, eax
    ret
    
@ok:
    ; Get function name and line range
    ; Call AIRefactor_ExtractFunction
    ; Preview and apply changes
    
    push hDlg
    push 1
    call EndDialog
    mov eax, 1
    ret
    
@cancel:
    push hDlg
    push 0
    call EndDialog
    mov eax, 1
    ret
ExtractDialogProc ENDP

NLDialogProc PROC hDlg:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL intent:BYTE[512]
    LOCAL code_buffer:BYTE[4096]
    
    cmp uMsg, WM_INITDIALOG
    je @init
    cmp uMsg, WM_COMMAND
    je @command
    xor eax, eax
    ret
    
@init:
    ; Set dialog title and hints
    mov eax, 1
    ret
    
@command:
    mov eax, wParam
    cmp ax, IDOK
    je @ok
    cmp ax, IDCANCEL
    je @cancel
    xor eax, eax
    ret
    
@ok:
    ; Get natural language description
    push hDlg
    push 1001
    lea rax, [esp+16]
    push rax
    push 1024
    call GetDlgItemTextA
    
    ; Parse intent
    lea rcx, [esp+16]
    lea rdx, intent
    call AINL_ParseIntent
    
    ; Generate code
    lea rcx, intent
    lea rdx, code_buffer
    call AINL_GenerateCode
    
    ; Insert generated code into editor
    lea rcx, code_buffer
    call InsertCodeAtCursor
    
    push hDlg
    push 1
    call EndDialog
    mov eax, 1
    ret
    
@cancel:
    push hDlg
    push 0
    call EndDialog
    mov eax, 1
    ret
NLDialogProc ENDP

; ============================================================
; Helper procedures
; ============================================================
CreateContextPanel PROC
    LOCAL wc:WNDCLASSEXA
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea rax, ContextPanelWndProc
    mov wc.lpfnWndProc, rax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    invoke LoadIcon, 0, IDI_APPLICATION
    mov wc.hIcon, eax
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, 0
    lea rax, "AIContextPanel"
    mov wc.lpszClassName, rax
    mov wc.hIconSm, 0
    
    lea rax, wc
    invoke RegisterClassExA, rax
    
    ; Create window
    mov eax, [g_hMainWnd]
    invoke CreateWindowExA, WS_EX_TOOLWINDOW, "AIContextPanel", "Context Analysis", \
        WS_OVERLAPPED or WS_CAPTION or WS_SYSMENU or WS_THICKFRAME, \
        100, 100, 400, 600, eax, 0, 0, 0
    
    ret
CreateContextPanel ENDP

ContextPanelWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    cmp uMsg, WM_PAINT
    je @paint
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@paint:
    LOCAL ps:PAINTSTRUCT
    invoke BeginPaint, hWnd, ADDR ps
    
    ; Draw context tree/graph
    ; (Implementation would render the context graph)
    
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret
    
@destroy:
    xor eax, eax
    ret
ContextPanelWndProc ENDP

CreateRefactorDialog PROC
    ; Create refactoring preview dialog
    ; (Full implementation with diff view)
    xor eax, eax
    ret
CreateRefactorDialog ENDP

CreateNLDialog PROC
    ; Create NL input dialog  
    ; (Full implementation with suggestion box)
    xor eax, eax
    ret
CreateNLDialog ENDP

InsertCodeAtCursor PROC
    ; Insert generated code at current cursor position
    ret
InsertCodeAtCursor ENDP

END
