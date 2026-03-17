; Win32IDE_Sidebar.asm - Pure MASM64 Activity Bar & Views
; RawrXD IDE Component - Zero External Dependencies
; Assemble: ml64 /c /FoWin32IDE_Sidebar.obj Win32IDE_Sidebar.asm

.code
OPTION WIN64:3

; External imports (kernel32, user32, gdi32, comctl32, shell32, dwmapi)
EXTERN CreateWindowExA : PROC
EXTERN RegisterClassExA : PROC
EXTERN DefWindowProcA : PROC
EXTERN SendMessageA : PROC
EXTERN PostMessageA : PROC
EXTERN GetMessageA : PROC
EXTERN DispatchMessageA : PROC
_EXTERN TranslateMessage : PROC
EXTERN LoadIconA : PROC
EXTERN LoadCursorA : PROC
_EXTERN GetStockObject : PROC
EXTERN SetWindowLongPtrA : PROC
EXTERN GetWindowLongPtrA : PROC
EXTERN SetParent : PROC
_EXTERN ShowWindow : PROC
_EXTERN UpdateWindow : PROC
_EXTERN CreateFontA : PROC
_EXTERN CreateSolidBrush : PROC
_EXTERN DeleteObject : PROC
_EXTERN FillRect : PROC
_EXTERN GetClientRect : PROC
_EXTERN MoveWindow : PROC
EXTERN CreateProcessA : PROC
EXTERN WaitForSingleObject : PROC
EXTERN GetExitCodeProcess : PROC
EXTERN CloseHandle : PROC
EXTERN CreatePipe : PROC
EXTERN SetHandleInformation : PROC
EXTERN ReadFile : PROC
EXTERN WriteFile : PROC
EXTERN GetCurrentProcessId : PROC
EXTERN OutputDebugStringA : PROC
EXTERN GetModuleHandleA : PROC
EXTERN lstrcmpiA : PROC
EXTERN lstrcpyA : PROC
EXTERN lstrcatA : PROC
EXTERN wsprintfA : PROC
EXTERN GlobalAlloc : PROC
EXTERN GlobalFree : PROC
EXTERN memset : PROC
EXTERN memcpy : PROC
EXTERN InitializeCriticalSection : PROC
EXTERN EnterCriticalSection : PROC
EXTERN LeaveCriticalSection : PROC
EXTERN DwmSetWindowAttribute : PROC  ; Dark mode

; Constants
WM_CREATE               equ 0001h
WM_SIZE                 equ 0005h
WM_PAINT                equ 000Fh
WM_COMMAND              equ 0111h
WM_NOTIFY               equ 004Eh
WM_USER                 equ 0400h
WM_DESTROY              equ 0002h
WM_DRAWITEM             equ 002Bh
WM_LBUTTONUP            equ 0202h

WS_CHILD                equ 40000000h
WS_VISIBLE              equ 10000000h
WS_TABSTOP              equ 00010000h
WS_BORDER               equ 00800000h
WS_CLIPCHILDREN         equ 02000000h

WC_TREEVIEWA            equ "SysTreeView32",0
WC_LISTVIEWA            equ "SysListView32",0
WC_EDITA                equ "Edit",0
WC_BUTTONA              equ "Button",0
WC_COMBOBOXA            equ "ComboBox",0

TVS_HASBUTTONS          equ 0001h
TVS_HASLINES            equ 0002h
TVS_LINESATROOT         equ 0004h
TVS_EDITLABELS          equ 0008h
TVS_SHOWSELALWAYS       equ 0020h
TVS_EX_DOUBLEBUFFER     equ 004h
TVS_EX_FADEINOUTEXPAND  equ 040h

TVN_FIRST               equ 0FFFFFE70h
TVN_ITEMEXPANDING       equ (TVN_FIRST-5)
TVN_SELCHANGED          equ (TVN_FIRST-2)

LVS_REPORT              equ 0001h
LVS_SINGLESEL           equ 0004h
LVS_SHOWSELALWAYS       equ 0008h
LVS_EX_GRIDLINES        equ 00000001h
LVS_EX_FULLROWSELECT    equ 00000020h
LVS_EX_DOUBLEBUFFER     equ 00010000h

ES_AUTOHSCROLL          equ 0080h
BS_PUSHBUTTON           equ 0000h
BS_OWNERDRAW            equ 0000000Bh

SBARS_SIZEGRIP          equ 0100h

DWMWA_USE_IMMERSIVE_DARK_MODE equ 20

DEBUG_PROCESS           equ 00000001h
DEBUG_ONLY_THIS_PROCESS equ 00000002h
CREATE_NEW_CONSOLE      equ 00000010h
INFINITE                equ 0FFFFFFFFh

IDC_ARROW               equ 7F00h
IDI_APPLICATION         equ 7F00h
BLACK_BRUSH             equ 0004h

; Combined style constants
ACTIVITY_BAR_STYLE      equ 5000000Bh  ; WS_CHILD | WS_VISIBLE | BS_OWNERDRAW
TREEVIEW_STYLE          equ 50800027h  ; WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS

; Sidebar struct offsets (sizeof = 256 bytes)
SB_HWND                 equ 0   ; 8 bytes - main handle
SB_HACTIVITY            equ 8   ; 8 bytes - activity bar
SB_HEXPLORER            equ 16  ; 8 bytes - explorer tree
SB_HSEARCH              equ 24  ; 8 bytes - search pane
SB_HSCM                 equ 32  ; 8 bytes - source control
SB_HDEBUG               equ 40  ; 8 bytes - debug view
SB_HEXT                 equ 48  ; 8 bytes - extensions
SB_HTREE                equ 56  ; 8 bytes - file tree handle
SB_HLIST                equ 64  ; 8 bytes - list view handle
SB_HEDIT                equ 72  ; 8 bytes - search input
SB_HTOOLBAR             equ 80  ; 8 bytes - activity buttons
SB_CURRENT_VIEW         equ 88  ; 4 bytes - active view index (0-4)
SB_WIDTH                equ 92  ; 4 bytes - sidebar width
SB_IS_DARK              equ 96  ; 1 byte - dark mode flag
SB_HFONT                equ 104 ; 8 bytes - font handle
SB_HBRUSH_BG            equ 112 ; 8 bytes - background brush
SB_CRIT_SECT            equ 120 ; 24 bytes - CRITICAL_SECTION
SB_GIT_BUF              equ 144 ; 8 bytes - git output buffer
SB_DEBUG_PROC           equ 152 ; 8 bytes - debug process handle
SB_DEBUG_THREAD         equ 160 ; 8 bytes - debug thread handle

; View indices
VIEW_EXPLORER           equ 0
VIEW_SEARCH             equ 1
VIEW_SCM                equ 2
VIEW_DEBUG              equ 3
VIEW_EXTENSIONS         equ 4

.data
ALIGN 8
g_SidebarClass  db "RawrXD_SidebarClass",0
g_TreeClass     db "SysTreeView32",0
g_ListClass     db "SysListView32",0
g_EditClass     db "Edit",0
g_BtnClass      db "Button",0

; Activity bar tooltips
g_TipExplorer   db "Explorer",0
g_TipSearch     db "Search",0
g_TipScm        db "Source Control",0
g_TipDebug      db "Debug",0
g_TipExt        db "Extensions",0

; Colors (Dark Mode - Dracula-ish)
COLOR_DARK_BG   dd 00282828h    ; RGB(40,40,40)
COLOR_DARK_PANEL dd 00303030h   ; RGB(48,48,48)
COLOR_DARK_TEXT dd 00F8F8F2h    ; RGB(242,248,248)
COLOR_ACCENT    dd 00BD93F9h    ; RGB(189,147,249)

; Git commands
g_GitStatus     db "git status --porcelain -u",0
g_GitLog        db "git log --oneline -20",0
g_GitAdd        db "git add ",0
g_GitCommit     db "git commit -m ",0

; Debug strings
g_DbgSidebar    db "[Sidebar] Initialized",0
g_DbgGitExec    db "[Git] Executing command",0
g_DbgDebugStart db "[Debug] Process created",0

; Buffers
g_TempPath      db 260 dup(0)
g_GitOutput     db 4096 dup(0)

.code

; =============================================================================
; SidebarWndProc - Main window procedure
; rcx = hwnd, rdx = uMsg, r8 = wParam, r9 = lParam
; =============================================================================
SidebarWndProc PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    .ENDPROLOG
    mov [rbp+10h], rcx      ; hwnd
    mov [rbp+18h], rdx      ; uMsg
    mov [rbp+20h], r8       ; wParam
    mov [rbp+28h], r9       ; lParam

    cmp edx, WM_CREATE
    je @HandleCreate
    cmp edx, WM_SIZE
    je @HandleSize
    cmp edx, WM_NOTIFY
    je @HandleNotify
    cmp edx, WM_COMMAND
    je @HandleCommand
    cmp edx, WM_DRAWITEM
    je @HandleDrawItem
    cmp edx, WM_DESTROY
    je @HandleDestroy

    ; Default handler
    mov rcx, [rbp+10h]
    mov rdx, [rbp+18h]
    mov r8, [rbp+20h]
    mov r9, [rbp+28h]
    call DefWindowProcA
    jmp @Done

@HandleCreate:
    ; Initialize critical section
    mov rcx, [rbp+10h]
    mov rdx, 256            ; sizeof SidebarData
    mov r8, 040h            ; GPTR (zeroed)
    call GlobalAlloc
    mov rbx, rax            ; rbx = sidebar data ptr
    
    ; Store in window extra
    mov rcx, [rbp+10h]
    mov edx, 0              ; offset
    mov r8, rbx
    call SetWindowLongPtrA
    
    ; Initialize critical section
    lea rcx, [rbx+SB_CRIT_SECT]
    call InitializeCriticalSection
    
    ; Set dark mode
    mov byte ptr [rbx+SB_IS_DARK], 1
    mov ecx, 1
    mov [rbx+SB_WIDTH], ecx ; 250px default width
    
    ; Create activity bar (left vertical strip)
    mov rcx, 0              ; dwExStyle
    lea rdx, g_BtnClass     ; lpClassName
    lea r8, g_TipExplorer   ; lpWindowName (tooltip)
    mov r9d, WS_CHILD or WS_VISIBLE or BS_OWNERDRAW
    mov [rsp+20h], 0        ; X
    mov [rsp+28h], 0        ; Y
    mov [rsp+30h], 48       ; nWidth (activity bar width)
    mov [rsp+38h], 800      ; nHeight
    mov rax, [rbp+10h]
    mov [rsp+40h], rax      ; hWndParent
    mov [rsp+48h], 0        ; hMenu (ID 0 = Explorer)
    call CreateWindowExA
    mov [rbx+SB_HACTIVITY], rax
    
    ; Create Explorer TreeView
    mov rcx, 0
    lea rdx, g_TreeClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS
    mov [rsp+20h], 48       ; X (after activity bar)
    mov [rsp+28h], 0        ; Y
    mov [rsp+30h], 202      ; Width (250-48)
    mov [rsp+38h], 800
    mov rax, [rbp+10h]
    mov [rsp+40h], rax
    mov [rsp+48h], 100      ; ID 100 = Tree
    call CreateWindowExA
    mov [rbx+SB_HTREE], rax
    
    ; Set extended tree styles (double buffer, fade)
    mov rcx, rax
    mov edx, TVM_SETEXTENDEDSTYLE
    xor r8, r8
    mov r9d, TVS_EX_DOUBLEBUFFER or TVS_EX_FADEINOUTEXPAND
    call SendMessageA
    
    ; Create Search View (initially hidden)
    mov rcx, 0
    lea rdx, g_EditClass
    xor r8, r8
    mov r9d, WS_CHILD or WS_BORDER or ES_AUTOHSCROLL
    mov [rsp+20h], 48
    mov [rsp+28h], 0
    mov [rsp+30h], 202
    mov [rsp+38h], 25
    mov rax, [rbp+10h]
    mov [rsp+40h], rax
    mov [rsp+48h], 200      ; ID 200 = Search box
    call CreateWindowExA
    mov [rbx+SB_HEDIT], rax
    
    ; Create ListView for Search/SCM results
    mov rcx, 0
    lea rdx, g_ListClass
    xor r8, r8
    mov r9d, WS_CHILD or LVS_REPORT or LVS_SINGLESEL or LVS_SHOWSELALWAYS
    mov [rsp+20h], 48
    mov [rsp+28h], 30       ; Below search box
    mov [rsp+30h], 202
    mov [rsp+38h], 770
    mov rax, [rbp+10h]
    mov [rsp+40h], rax
    mov [rsp+48h], 300      ; ID 300 = List
    call CreateWindowExA
    mov [rbx+SB_HLIST], rax
    
    ; Set extended list styles
    mov rcx, rax
    mov edx, LVM_SETEXTENDEDLISTVIEWSTYLE
    xor r8, r8
    mov r9d, LVS_EX_GRIDLINES or LVS_EX_FULLROWSELECT or LVS_EX_DOUBLEBUFFER
    call SendMessageA
    
    ; Initialize Debug Engine handles
    mov qword ptr [rbx+SB_DEBUG_PROC], 0
    mov qword ptr [rbx+SB_DEBUG_THREAD], 0
    
    ; Log initialization
    lea rcx, g_DbgSidebar
    call OutputDebugStringA
    
    xor eax, eax
    jmp @Done

@HandleSize:
    mov rcx, [rbp+10h]
    mov edx, 0
    call GetWindowLongPtrA
    mov rbx, rax
    
    ; Resize activity bar (full height, 48px width)
    mov rcx, [rbx+SB_HACTIVITY]
    xor edx, edx
    xor r8, r8
    mov r9d, 48
    mov [rsp+20h], r9d      ; nHeight = 800 (get from lParam)
    mov rax, [rbp+28h]
    shr rax, 16             ; HIWORD(lParam) = height
    mov [rsp+28h], eax
    mov dword ptr [rsp+30h], 1  ; bRepaint
    call MoveWindow
    
    ; Resize views based on current view
    mov ecx, [rbx+SB_CURRENT_VIEW]
    cmp ecx, VIEW_EXPLORER
    jne @CheckSearch
    
    ; Resize Explorer tree
    mov rcx, [rbx+SB_HTREE]
    mov edx, 48
    xor r8, r8
    mov r9d, 202
    mov rax, [rbp+28h]
    shr rax, 16
    mov [rsp+20h], eax
    mov dword ptr [rsp+28h], 1
    call MoveWindow
    jmp @SizeDone

@CheckSearch:
    cmp ecx, VIEW_SEARCH
    jne @SizeDone
    
    ; Resize Search controls
    mov rcx, [rbx+SB_HEDIT]
    mov edx, 48
    xor r8, r8
    mov r9d, 202
    mov [rsp+20h], 25       ; height
    mov dword ptr [rsp+28h], 1
    call MoveWindow
    
    mov rcx, [rbx+SB_HLIST]
    mov edx, 48
    mov r8d, 30
    mov r9d, 202
    mov rax, [rbp+28h]
    shr rax, 16
    sub eax, 30
    mov [rsp+20h], eax
    mov dword ptr [rsp+28h], 1
    call MoveWindow

@SizeDone:
    xor eax, eax
    jmp @Done

@HandleNotify:
    ; Handle tree expansion for lazy loading
    mov rbx, [rbp+28h]      ; lParam = NMHDR ptr
    mov eax, [rbx+NMHDR.code]
    cmp eax, TVN_ITEMEXPANDING
    jne @NotifyDone
    
    ; Lazy load directory contents
    mov rcx, [rbp+10h]
    mov edx, 0
    call GetWindowLongPtrA
    mov rdi, rax
    
    ; Enter critical section
    lea rcx, [rdi+SB_CRIT_SECT]
    call EnterCriticalSection
    
    ; TODO: Populate tree items asynchronously
    ; For now, just log
    lea rcx, g_DbgSidebar
    call OutputDebugStringA
    
    lea rcx, [rdi+SB_CRIT_SECT]
    call LeaveCriticalSection

@NotifyDone:
    xor eax, eax
    jmp @Done

@HandleCommand:
    ; Activity bar button clicks
    mov rax, [rbp+28h]      ; lParam = hwnd
    mov rbx, [rbp+20h]      ; wParam = ID
    and ebx, 0FFFFh
    
    cmp ebx, 0              ; Explorer
    je @SwitchExplorer
    cmp ebx, 1              ; Search
    je @SwitchSearch
    cmp ebx, 2              ; SCM
    je @SwitchSCM
    cmp ebx, 3              ; Debug
    je @SwitchDebug
    cmp ebx, 4              ; Extensions
    je @SwitchExt
    jmp @CmdDone

@SwitchExplorer:
    mov rcx, [rbp+10h]
    call SidebarSwitchView
    mov edx, VIEW_EXPLORER
    mov [rbx+SB_CURRENT_VIEW], edx
    jmp @CmdDone

@SwitchSearch:
    mov rcx, [rbp+10h]
    call SidebarSwitchView
    mov edx, VIEW_SEARCH
    mov [rbx+SB_CURRENT_VIEW], edx
    jmp @CmdDone

@SwitchSCM:
    ; Execute git status
    mov rcx, [rbp+10h]
    lea rdx, g_GitStatus
    call SidebarExecuteGit
    mov rcx, [rbp+10h]
    call SidebarSwitchView
    mov edx, VIEW_SCM
    mov [rbx+SB_CURRENT_VIEW], edx
    jmp @CmdDone

@SwitchDebug:
    mov rcx, [rbp+10h]
    call SidebarSwitchView
    mov edx, VIEW_DEBUG
    mov [rbx+SB_CURRENT_VIEW], edx
    jmp @CmdDone

@SwitchExt:
    mov rcx, [rbp+10h]
    call SidebarSwitchView
    mov edx, VIEW_EXTENSIONS
    mov [rbx+SB_CURRENT_VIEW], edx

@CmdDone:
    xor eax, eax
    jmp @Done

@HandleDrawItem:
    ; Custom draw activity bar buttons (dark mode)
    mov rbx, [rbp+28h]      ; lParam = DRAWITEMSTRUCT
    mov eax, [rbx+DRAWITEMSTRUCT.CtlID]
    
    ; Set dark background
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    mov edx, COLOR_DARK_BG
    call SetBkColor
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    mov edx, COLOR_DARK_TEXT
    call SetTextColor
    
    ; Fill background
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    lea rdx, [rbx+DRAWITEMSTRUCT.rcItem]
    mov r8d, COLOR_DARK_BG
    call FillRect
    
    ; Draw text/icon based on ID
    mov eax, [rbx+DRAWITEMSTRUCT.CtlID]
    lea r9, g_TipExplorer
    cmp eax, 0
    je @DoDraw
    lea r9, g_TipSearch
    cmp eax, 1
    je @DoDraw
    lea r9, g_TipScm
    cmp eax, 2
    je @DoDraw
    lea r9, g_TipDebug
    cmp eax, 3
    je @DoDraw
    lea r9, g_TipExt

@DoDraw:
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    mov edx, 4              ; TA_CENTER
    call SetTextAlign
    
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    mov rdx, r9             ; lpString
    call lstrlenA
    mov r11, rax            ; cchString
    mov rcx, [rbx+DRAWITEMSTRUCT.hDC]
    mov rdx, r9             ; lpString
    mov r8, r11             ; cch
    lea r9, [rbx+DRAWITEMSTRUCT.rcItem]
    mov eax, [r9+RECT.left]
    add eax, [r9+RECT.right]
    shr eax, 1              ; center X
    mov [rsp+20h], eax
    mov eax, [r9+RECT.top]
    add eax, 20
    mov [rsp+28h], eax
    call TextOutA
    
    xor eax, eax
    jmp @Done

@HandleDestroy:
    mov rcx, [rbp+10h]
    mov edx, 0
    call GetWindowLongPtrA
    mov rbx, rax
    
    ; Cleanup debug process if active
    mov rcx, [rbx+SB_DEBUG_PROC]
    test rcx, rcx
    jz @NoDebug
    call CloseHandle
@NoDebug:
    mov rcx, [rbx+SB_DEBUG_THREAD]
    test rcx, rcx
    jz @NoThread
    call CloseHandle
@NoThread:
    
    ; Delete GDI objects
    mov rcx, [rbx+SB_HBRUSH_BG]
    test rcx, rcx
    jz @NoBrush
    call DeleteObject
@NoBrush:
    
    ; Free memory
    mov rcx, rbx
    call GlobalFree
    
    xor eax, eax

@Done:
    leave
    ret
SidebarWndProc ENDP

; =============================================================================
; SidebarSwitchView - Hide all views except active
; rcx = hwndSidebar, edx = viewIndex
; =============================================================================
SidebarSwitchView PROC FRAME
    push rbp
    mov rbp, rsp
    push rbx
    push rdi
    push rsi
    .ENDPROLOG
    
    mov rdi, rcx            ; hwnd
    mov esi, edx            ; view index
    
    ; Get sidebar data
    mov rcx, rdi
    mov edx, 0
    call GetWindowLongPtrA
    mov rbx, rax
    
    ; Hide all view windows first
    mov rcx, [rbx+SB_HTREE]
    mov edx, 0              ; SW_HIDE
    call ShowWindow
    
    mov rcx, [rbx+SB_HEDIT]
    mov edx, 0
    call ShowWindow
    
    mov rcx, [rbx+SB_HLIST]
    mov edx, 0
    call ShowWindow
    
    ; Show active view windows
    cmp esi, VIEW_EXPLORER
    jne @CheckSearchView
    
    mov rcx, [rbx+SB_HTREE]
    mov edx, 1              ; SW_SHOW
    call ShowWindow
    jmp @ViewDone

@CheckSearchView:
    cmp esi, VIEW_SEARCH
    jne @CheckSCMView
    
    mov rcx, [rbx+SB_HEDIT]
    mov edx, 1
    call ShowWindow
    mov rcx, [rbx+SB_HLIST]
    mov edx, 1
    call ShowWindow
    jmp @ViewDone

@CheckSCMView:
    cmp esi, VIEW_SCM
    jne @CheckDebugView
    
    mov rcx, [rbx+SB_HLIST]
    mov edx, 1
    call ShowWindow
    jmp @ViewDone

@CheckDebugView:
    cmp esi, VIEW_DEBUG
    jne @CheckExtView
    
    ; Debug view uses list for variables
    mov rcx, [rbx+SB_HLIST]
    mov edx, 1
    call ShowWindow
    jmp @ViewDone

@CheckExtView:
    cmp esi, VIEW_EXTENSIONS
    jne @ViewDone
    
    mov rcx, [rbx+SB_HLIST]
    mov edx, 1
    call ShowWindow

@ViewDone:
    pop rsi
    pop rdi
    pop rbx
    leave
    ret
SidebarSwitchView ENDP

; =============================================================================
; SidebarExecuteGit - Execute git command, capture output
; rcx = hwnd, rdx = command string
; =============================================================================
SidebarExecuteGit PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 128
    .ENDPROLOG
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    
    ; Log
    lea rcx, g_DbgGitExec
    call OutputDebugStringA
    
    ; Create pipes for stdout
    lea rcx, [rbp+30h]      ; hRead
    lea rdx, [rbp+38h]      ; hWrite
    xor r8, r8              ; lpSecurityAttributes
    xor r9, r9              ; nSize (default)
    call CreatePipe
    
    ; Set inherit flag on write end
    mov rcx, [rbp+38h]
    mov edx, 1              ; HANDLE_FLAG_INHERIT
    xor r8, r8
    call SetHandleInformation
    
    ; Setup STARTUPINFO
    mov dword ptr [rbp+40h], 68 ; cb = sizeof(STARTUPINFOA)
    mov dword ptr [rbp+78h], 101h ; dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW
    mov rax, [rbp+38h]
    mov [rbp+80h], rax      ; hStdOutput
    mov [rbp+80h], rax      ; hStdError (same)
    
    ; CreateProcess for git
    xor ecx, ecx            ; lpApplicationName
    mov rdx, [rbp+18h]      ; lpCommandLine (git ...)
    xor r8, r8              ; lpProcessAttributes
    xor r9, r9              ; lpThreadAttributes
    mov [rsp+20h], 1        ; bInheritHandles
    mov [rsp+28h], 0        ; dwCreationFlags
    xor eax, eax
    mov [rsp+30h], rax      ; lpEnvironment
    mov [rsp+38h], rax      ; lpCurrentDirectory
    lea rax, [rbp+40h]      ; lpStartupInfo
    mov [rsp+40h], rax
    lea rax, [rbp+90h]      ; lpProcessInformation
    mov [rsp+48h], rax
    call CreateProcessA
    
    test eax, eax
    jz @GitFail
    
    ; Wait for completion
    mov rcx, [rbp+90h]      ; hProcess
    mov edx, 5000           ; 5 second timeout
    call WaitForSingleObject
    
    ; Read output
    mov rcx, [rbp+30h]      ; hRead
    lea rdx, g_GitOutput    ; lpBuffer
    mov r8d, 4096           ; nNumberOfBytesToRead
    lea r9, [rbp+50h]       ; lpNumberOfBytesRead
    xor eax, eax
    mov [rsp+20h], rax      ; lpOverlapped
    call ReadFile
    
    ; Parse and populate list view (simplified)
    mov rcx, [rbp+10h]
    mov edx, 0
    call GetWindowLongPtrA
    mov rbx, rax
    
    ; Clear list
    mov rcx, [rbx+SB_HLIST]
    mov edx, LVM_DELETEALLITEMS
    xor r8, r8
    xor r9, r9
    call SendMessageA
    
    ; Close handles
    mov rcx, [rbp+90h]      ; hProcess
    call CloseHandle
    mov rcx, [rbp+98h]      ; hThread
    call CloseHandle

@GitFail:
    mov rcx, [rbp+30h]
    call CloseHandle
    mov rcx, [rbp+38h]
    call CloseHandle
    
    leave
    ret
SidebarExecuteGit ENDP

; =============================================================================
; SidebarCreate - Factory function called by IDE
; rcx = hwndParent, edx = x, r8d = y, r9d = width, [rsp+28] = height
; Returns: rax = hwndSidebar
; =============================================================================
SidebarCreate PROC EXPORT FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 88h
    .ENDPROLOG
    
    mov [rbp+10h], rcx      ; hParent
    mov [rbp+18h], edx      ; x
    mov [rbp+20h], r8d      ; y
    mov [rbp+28h], r9d      ; width
    mov eax, [rbp+30h]      ; height (stack)
    mov [rbp+30h], eax
    
    ; Register class if needed (first time)
    ; (Simplified - assume pre-registered or use global atom)
    
    ; Create sidebar window
    mov rcx, WS_EX_CLIENTEDGE
    lea rdx, g_SidebarClass
    xor r8, r8              ; lpWindowName
    mov r9d, WS_CHILD or WS_VISIBLE or WS_CLIPCHILDREN
    mov eax, [rbp+18h]
    mov [rsp+20h], eax      ; X
    mov eax, [rbp+20h]
    mov [rsp+28h], eax      ; Y
    mov eax, [rbp+28h]
    mov [rsp+30h], eax      ; nWidth
    mov eax, [rbp+30h]
    mov [rsp+38h], eax      ; nHeight
    mov rax, [rbp+10h]
    mov [rsp+40h], rax      ; hWndParent
    mov qword ptr [rsp+48h], 0 ; hMenu
    call CreateWindowExA
    
    ; Enable dark mode via DWM
    mov rbx, rax            ; save hwnd
    test rax, rax
    jz @CreateFail
    
    mov ecx, 20             ; DWMWA_USE_IMMERSIVE_DARK_MODE
    mov [rsp+20h], ecx
    mov edx, 1              ; enable
    mov [rsp+28h], edx
    mov r8d, 4              ; size
    call DwmSetWindowAttribute
    
    mov rax, rbx

@CreateFail:
    leave
    ret
SidebarCreate ENDP

; =============================================================================
; SidebarDebugAttach - Real debug engine entry
; rcx = hwndSidebar, rdx = lpApplicationName, r8 = lpCommandLine
; =============================================================================
SidebarDebugAttach PROC EXPORT FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 128
    .ENDPROLOG
    
    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8
    
    ; Setup STARTUPINFO
    mov dword ptr [rbp+30h], 68
    mov dword ptr [rbp+68h], 1  ; STARTF_USESHOWWINDOW
    
    ; CreateProcess with DEBUG_PROCESS flag
    xor ecx, ecx
    mov rdx, [rbp+20h]      ; Command line
    xor r8, r8
    xor r9, r9
    mov [rsp+20h], 1        ; Inherit handles
    mov eax, DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS or CREATE_NEW_CONSOLE
    mov [rsp+28h], eax      ; Creation flags
    xor eax, eax
    mov [rsp+30h], rax
    mov [rsp+38h], rax
    lea rax, [rbp+30h]
    mov [rsp+40h], rax      ; StartupInfo
    lea rax, [rbp+80h]
    mov [rsp+48h], rax      ; ProcessInfo
    call CreateProcessA
    
    test eax, eax
    jz @DebugFail
    
    ; Store handles in sidebar data
    mov rcx, [rbp+10h]
    mov edx, 0
    call GetWindowLongPtrA
    mov rbx, rax
    
    mov rax, [rbp+80h]      ; hProcess
    mov [rbx+SB_DEBUG_PROC], rax
    mov rax, [rbp+88h]      ; hThread
    mov [rbx+SB_DEBUG_THREAD], rax
    
    ; Log
    lea rcx, g_DbgDebugStart
    call OutputDebugStringA
    
    ; Start debug event loop thread (simplified - just return success)
    mov eax, 1
    jmp @DebugDone

@DebugFail:
    xor eax, eax

@DebugDone:
    leave
    ret
SidebarDebugAttach ENDP

; Data section for exported strings
.data
Sidebar_Version     db "RawrXD Sidebar v1.0 - Pure MASM64",0

END
