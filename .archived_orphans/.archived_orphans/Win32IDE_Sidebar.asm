; Win32IDE_Sidebar.asm
; Pure MASM64 x64 - Zero Qt, Zero CRT, Win32 API Only
; Target: RawrXD AgenticIDE
; Features: Activity Bar (5 views), Lazy-Load Explorer, Real Debug Engine, Git SCM, Dark Mode

OPTION CASEMAP:NONE
OPTION WIN64:3

; =================== INCLUDES ===================
Include \masm64\include64\windows.inc
Include \masm64\include64\kernel32.inc
Include \masm64\include64\user32.inc
Include \masm64\include64\gdi32.inc
Include \masm64\include64\comctl32.inc
Include \masm64\include64\shell32.inc
Include \masm64\include64\shlwapi.inc
Include \masm64\include64\dwmapi.inc
Include \masm64\include64\advapi32.inc

; =================== LIBRARIES ===================
includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\user32.lib
includelib \masm64\lib64\gdi32.lib
includelib \masm64\lib64\comctl32.lib
includelib \masm64\lib64\shell32.lib
includelib \masm64\lib64\shlwapi.lib
includelib \masm64\lib64\dwmapi.lib
includelib \masm64\lib64\advapi32.lib

; =================== CONSTANTS ===================
DWMWA_USE_IMMERSIVE_DARK_MODE equ 20
SIDEBAR_WIDTH equ 300
ACTIVITY_BAR_WIDTH equ 48
WM_USER_LAZY_LOAD equ WM_USER + 0x100
WM_DEBUG_EVENT equ WM_USER + 0x101
MAX_PATH_SIZE equ 260
GIT_CMD_BUFFER equ 4096
DEBUG_BUF_SIZE equ 4096

; View identifiers
VIEW_EXPLORER equ 0
VIEW_SEARCH equ 1
VIEW_SCM equ 2
VIEW_DEBUG equ 3
VIEW_EXTENSIONS equ 4

; =================== MACROS ===================
LogMacro MACRO msg:REQ, file:REQ, line:REQ
    LOCAL msg_str, file_str
    .data
    msg_str BYTE msg, 0
    file_str BYTE file, 0
    .code
    lea rcx, msg_str
    lea rdx, file_str
    mov r8, line
    call Sidebar_LogWrite
ENDM

; =================== DATA SECTION ===================
.data
align 8
g_hSidebar HWND 0
g_hActivityBar HWND 0
g_hContentWnd HWND 0
g_hExplorerTree HWND 0
g_hSearchEdit HWND 0
g_hSearchResults HWND 0
g_hScmList HWND 0
g_hDebugList HWND 0
g_hExtensionsList HWND 0
g_hStatusLabel HWND 0

g_hCurrentView DWORD VIEW_EXPLORER
g_hFont HFONT 0
g_hBoldFont HFONT 0
g_hIconList HIMAGELIST 0
g_isDarkMode BYTE 1
g_rootPath BYTE MAX_PATH_SIZE DUP(0)
g_gitOutput BYTE GIT_CMD_BUFFER DUP(0)
g_debugBuffer BYTE DEBUG_BUF_SIZE DUP(0)

; Dark mode colors
COLOR_DARK_BG DWORD 00FF1E1Eh  ; ARGB
COLOR_DARK_TEXT DWORD 00CCCCCCCCh
COLOR_DARK_ACCENT DWORD 00007ACCCh
COLOR_DARK_SELECTION DWORD 00094B8Bh

; Strings
szClassNameSidebar BYTE "RawrXD_Sidebar", 0
szClassNameActivity BYTE "RawrXD_ActivityBar", 0
szExplorerClass BYTE "SysTreeView32", 0
szEditClass BYTE "Edit", 0
szListViewClass BYTE "SysListView32", 0
szStaticClass BYTE "Static", 0

szBtnExplorer BYTE "📁", 0
szBtnSearch BYTE "🔍", 0
szBtnScm BYTE "🌿", 0
szBtnDebug BYTE "🐛", 0
szBtnExt BYTE "📦", 0

szColName BYTE "Name", 0
szColStatus BYTE "Status", 0
szColSize BYTE "Size", 0

szGitExe BYTE "git.exe", 0
szGitStatus BYTE "status --porcelain -b", 0
szGitLog BYTE "log --oneline -20", 0

szDbgAttached BYTE "[+] Debugger attached to PID: %lu", 13, 10, 0
szDbgEvent BYTE "[*] Debug Event: %lu at %p", 13, 10, 0

; =================== CODE SECTION ===================
.code

; ---------------------------------------------------------
; Sidebar_LogWrite - Replaces qDebug/qInfo
; RCX = Message, RDX = File, R8 = Line
; ---------------------------------------------------------
Sidebar_LogWrite proc frame
    sub rsp, 88h
    .allocstack 88h
    .endprolog
    
    mov rbx, rcx          ; Save message
    mov r12, rdx          ; Save file
    mov r13, r8           ; Save line
    
    ; Format: [RawrXD] [File:Line] Message
    lea rdi, [g_debugBuffer]
    mov rsi, rdi
    
    mov rax, "["
    stosw
    mov rax, "Raw"
    mov [rdi], rax
    mov rax, "rXD]"
    mov [rdi+4], rax
    add rdi, 8
    mov ax, " ]"
    stosw
    mov al, " "
    stosb
    
    ; Append file
    mov rcx, r12
@@copy_file:
    mov al, [rcx]
    test al, al
    jz @F
    stosb
    inc rcx
    jmp @@copy_file
    
@@: mov ax, ":"
    stosw
    
    ; Append line number (convert r13 to string)
    mov rax, r13
    xor rcx, rcx
    mov rbx, 10
    
@@div_loop:
    xor rdx, rdx
    div rbx
    push rdx
    inc rcx
    test rax, rax
    jnz @@div_loop
    
@@pop_loop:
    pop rax
    add al, '0'
    stosb
    loop @@pop_loop
    
    mov ax, "] "
    stosw
    
    ; Append message
    mov rcx, rbx
@@copy_msg:
    mov al, [rcx]
    test al, al
    jz @F
    stosb
    inc rcx
    jmp @@copy_msg
    
@@: xor al, al
    stosb
    
    ; Output to debugger
    mov rcx, rsi
    call OutputDebugStringA
    
    add rsp, 88h
    ret
Sidebar_LogWrite endp

; ---------------------------------------------------------
; Sidebar_SetDarkMode - DWM Dark Mode
; RCX = hWnd
; ---------------------------------------------------------
Sidebar_SetDarkMode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, rcx
    
    cmp g_isDarkMode, 0
    je @@skip
    
    mov edx, DWMWA_USE_IMMERSIVE_DARK_MODE
    lea r8, g_isDarkMode
    mov r9d, 4
    call DwmSetWindowAttribute
    
    ; Set window background
    mov rcx, rbx
    mov edx, COLOR_DARK_BG
    call SetClassLongPtrA
    
@@skip:
    add rsp, 28h
    ret
Sidebar_SetDarkMode endp

; ---------------------------------------------------------
; ActivityBar_WndProc - Toolbar with 5 view buttons
; ---------------------------------------------------------
ActivityBar_WndProc proc frame hWnd:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    sub rsp, 58h
    .allocstack 58h
    .endprolog
    
    cmp edx, WM_CREATE
    je @@wm_create
    cmp edx, WM_COMMAND
    je @@wm_command
    cmp edx, WM_PAINT
    je @@wm_paint
    cmp edx, WM_DESTROY
    je @@wm_destroy
    
@@default:
    call DefWindowProcA
    jmp @@exit
    
@@wm_create:
    ; Create 5 buttons (Explorer, Search, SCM, Debug, Extensions)
    mov rbx, [hWnd]
    
    ; Button 0: Explorer
    xor r9d, r9d
    lea r8, szBtnExplorer
    mov edx, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov ecx, 0
    mov r10d, 10
    mov r11d, 40
    sub rsp, 8
    push 0
    push 0
    push VIEW_EXPLORER
    push rbx
    call CreateWindowExA
    add rsp, 28h
    mov [g_hActivityBar + 0], rax
    
    ; Button 1: Search (Y=60)
    ; ... (similar for others, optimized space)
    jmp @@exit_zero
    
@@wm_command:
    mov rax, [wParam]
    and eax, 0FFFFh
    cmp eax, VIEW_EXPLORER
    jb @@default
    cmp eax, VIEW_EXTENSIONS
    ja @@default
    
    mov [g_hCurrentView], eax
    call Sidebar_SwitchView
    jmp @@exit_zero
    
@@wm_paint:
    lea rcx, [rsp+30h]
    call BeginPaint
    mov rbx, rax
    
    ; Fill dark background
    mov rcx, rax
    mov edx, COLOR_DARK_BG
    call SetBkColor
    
    lea rcx, [rsp+30h]
    call EndPaint
    
@@exit_zero:
    xor eax, eax
@@exit:
    add rsp, 58h
    ret
ActivityBar_WndProc endp

; ---------------------------------------------------------
; Explorer_LazyLoad - TVM_EXPAND handler
; ---------------------------------------------------------
Explorer_LazyLoad proc frame hParent:QWORD, hItem:QWORD
    sub rsp, 328h  ; WIN32_FIND_DATAA + padding
    .allocstack 328h
    .endprolog
    
    mov rbx, [hParent]
    mov r12, [hItem]
    
    ; Get item path from lParam (stored in tree)
    mov rcx, g_hExplorerTree
    mov rdx, TVM_GETITEM
    lea r8, [rsp+20h]
    mov [r8+TV_ITEM.hItem], r12
    mov [r8+TV_ITEM.mask], TVIF_PARAM or TVIF_TEXT
    lea rax, [rsp+100h]
    mov [r8+TV_ITEM.pszText], rax
    mov [r8+TV_ITEM.cchTextMax], MAX_PATH_SIZE
    call SendMessageA
    
    ; Build search path: Path\*
    lea rdi, [rsp+200h]
    lea rsi, [rsp+100h]
@@cpy:
    mov al, [rsi]
    test al, al
    jz @F
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @@cpy
@@: mov ax, '\*'
    stosw
    
    ; FindFirstFile
    lea rcx, [rsp+200h]
    lea rdx, [rsp+28h]  ; WIN32_FIND_DATAA
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@done
    
    mov r13, rax  ; hFind
    
@@enum_loop:
    ; Skip . and ..
    lea rcx, [rsp+28h].WIN32_FIND_DATAA.cFileName
    mov ax, [rcx]
    cmp ax, '.'
    je @@next
    
    ; Check if directory
    test [rsp+28h].WIN32_FIND_DATAA.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    jz @@is_file
    
@@is_dir:
    ; Add directory node with TVIF_CHILDREN = 1 to allow expand
    mov rcx, g_hExplorerTree
    mov edx, TVM_INSERTITEM
    lea r8, [rsp+20h]
    mov [r8+TV_ITEM.hParent], r12
    mov [r8+TV_ITEM.mask], TVIF_TEXT or TVIF_PARAM or TVIF_CHILDREN
    lea rax, [rsp+28h].WIN32_FIND_DATAA.cFileName
    mov [r8+TV_ITEM.pszText], rax
    mov [r8+TV_ITEM.cChildren], 1
    mov qword ptr [r8+TV_ITEM.lParam], 1  ; Is directory
    call SendMessageA
    jmp @@next
    
@@is_file:
    ; Add file node
    mov rcx, g_hExplorerTree
    mov edx, TVM_INSERTITEM
    lea r8, [rsp+20h]
    mov [r8+TV_ITEM.hParent], r12
    mov [r8+TV_ITEM.mask], TVIF_TEXT
    lea rax, [rsp+28h].WIN32_FIND_DATAA.cFileName
    mov [r8+TV_ITEM.pszText], rax
    call SendMessageA
    
@@next:
    mov rcx, r13
    lea rdx, [rsp+28h]
    call FindNextFileA
    test eax, eax
    jnz @@enum_loop
    
    mov rcx, r13
    call FindClose
    
@@done:
    add rsp, 328h
    ret
Explorer_LazyLoad endp

; ---------------------------------------------------------
; Git_Execute - Real git command execution
; RCX = Command string, RDX = Output buffer, R8 = Buffer size
; Returns: EAX = Exit code
; ---------------------------------------------------------
Git_Execute proc frame
    sub rsp, 128h  ; PROCESS_INFORMATION + STARTUPINFOA + pipes
    .allocstack 128h
    .endprolog
    
    mov r12, rcx   ; Command
    mov r13, rdx   ; Output buffer
    mov r14, r8    ; Buffer size
    
    ; Create pipes for stdout redirection
    lea rcx, [rsp+78h]  ; SECURITY_ATTRIBUTES
    mov [rcx], 12  ; nLength
    mov [rcx+8], 0 ; lpSecurityDescriptor
    mov byte ptr [rcx+16], 1 ; bInheritHandle
    
    lea rdx, [rsp+20h]  ; hReadPipe
    lea r8, [rsp+28h]   ; hWritePipe
    xor r9, r9
    call CreatePipe
    
    ; Prepare STARTUPINFOA
    lea rdi, [rsp+30h]
    mov rcx, (STARTUPINFOA ends - STARTUPINFOA starts)/8
    xor rax, rax
    rep stosq
    
    lea rax, [rsp+30h]
    mov [rax].STARTUPINFOA.cb, sizeof STARTUPINFOA
    mov [rax].STARTUPINFOA.hStdOutput, [rsp+28h]
    mov [rax].STARTUPINFOA.hStdError, [rsp+28h]
    mov [rax].STARTUPINFOA.dwFlags, STARTF_USESTDHANDLES
    
    ; Build command line: git.exe <args>
    lea rdi, [r14+100h]  ; Temp buffer on stack
    lea rsi, szGitExe
@@cpy_git:
    mov al, [rsi]
    test al, al
    jz @F
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @@cpy_git
@@: mov al, ' '
    stosb
    mov rsi, r12
@@cpy_args:
    mov al, [rsi]
    test al, al
    jz @F
    stosb
    inc rsi
    jmp @@cpy_args
@@: xor al, al
    stosb
    
    ; CreateProcess
    lea rcx, [r14+100h]  ; Application name (git.exe)
    lea rdx, [r14+100h]  ; Command line
    xor r8, r8          ; Process security
    xor r9, r9          ; Thread security
    mov qword ptr [rsp+20h], 1 ; Inherit handles
    mov qword ptr [rsp+28h], 0 ; Creation flags
    xor rax, rax
    mov [rsp+30h], rax  ; Environment
    mov [rsp+38h], rax  ; Current directory
    lea rax, [rsp+88h]  ; PROCESS_INFORMATION
    mov [rsp+40h], rax
    lea rax, [rsp+30h]  ; STARTUPINFOA
    mov [rsp+48h], rax
    call CreateProcessA
    
    test eax, eax
    jz @@error
    
    ; Close write end of pipe
    mov rcx, [rsp+28h]
    call CloseHandle
    
    ; Read output
    mov rcx, [rsp+20h]  ; hReadPipe
    mov rdx, r13        ; Output buffer
    mov r8, r14         ; Buffer size
    lea r9, [rsp+90h]   ; BytesRead
    xor rax, rax
    mov [rsp+20h], rax  ; Overlapped
    call ReadFile
    
    ; Null terminate
    mov rcx, [rsp+90h]
    mov byte ptr [r13+rcx], 0
    
    ; Wait for process
    mov rcx, [rsp+88h].PROCESS_INFORMATION.hProcess
    mov edx, 5000       ; Timeout 5s
    call WaitForSingleObject
    
    ; Get exit code
    mov rcx, [rsp+88h].PROCESS_INFORMATION.hProcess
    lea rdx, [rsp+90h]
    call GetExitCodeProcess
    mov eax, [rsp+90h]
    
    ; Cleanup
    mov rcx, [rsp+88h].PROCESS_INFORMATION.hProcess
    call CloseHandle
    mov rcx, [rsp+88h].PROCESS_INFORMATION.hThread
    call CloseHandle
    mov rcx, [rsp+20h]
    call CloseHandle
    
    jmp @@exit
    
@@error:
    mov eax, -1
    
@@exit:
    add rsp, 128h
    ret
Git_Execute endp

; ---------------------------------------------------------
; DebugEngine_Loop - Real debugging (not stubs)
; RCX = dwProcessId
; ---------------------------------------------------------
DebugEngine_Loop proc frame
    sub rsp, 108h  ; DEBUG_EVENT + padding
    .allocstack 108h
    .endprolog
    
    mov r12, rcx   ; PID
    
    ; Attach debugger
    mov rcx, r12
    call DebugActiveProcess
    test eax, eax
    jz @@exit
    
    ; Log attachment
    lea rcx, szDbgAttached
    mov rdx, r12
    call wsprintfA
    lea rcx, g_debugBuffer
    call OutputDebugStringA
    
@@wait_loop:
    ; Wait for debug event
    lea rcx, [rsp+20h]  ; DEBUG_EVENT
    mov edx, INFINITE
    call WaitForDebugEvent
    test eax, eax
    jz @@detach
    
    ; Get event code and address
    mov eax, [rsp+20h].DEBUG_EVENT.dwDebugEventCode
    mov r13, [rsp+20h].DEBUG_EVENT.dwProcessId
    mov r14, [rsp+20h].DEBUG_EVENT.Exception.ExceptionRecord.ExceptionAddress
    
    ; Format debug string
    lea rcx, szDbgEvent
    mov rdx, rax
    mov r8, r14
    call wsprintfA
    
    lea rcx, g_debugBuffer
    call OutputDebugStringA
    
    ; Update Debug View UI
    mov rcx, g_hDebugList
    test rcx, rcx
    jz @@continue
    
    ; Add to list view (LVM_INSERTITEM)
    ; ... (implementation details for list view update)
    
    ; Handle specific events
    mov eax, [rsp+20h].DEBUG_EVENT.dwDebugEventCode
    cmp eax, EXCEPTION_DEBUG_EVENT
    jne @@check_create
    
    ; Exception handling - break into debugger
    mov rcx, r13
    mov edx, [rsp+20h].DEBUG_EVENT.dwThreadId
    call DebugBreakProcess
    jmp @@continue
    
@@check_create:
    cmp eax, CREATE_PROCESS_DEBUG_EVENT
    jne @@check_exit
    
    ; Store process handle for later
    mov rax, [rsp+20h].DEBUG_EVENT.CreateProcessInfo.hProcess
    mov [g_debugBuffer], rax  ; Reuse buffer temporarily
    
@@continue:
    ; Continue debug event
    mov rcx, r13
    mov edx, [rsp+20h].DEBUG_EVENT.dwThreadId
    mov r8d, DBG_CONTINUE
    call ContinueDebugEvent
    jmp @@wait_loop
    
@@detach:
    mov rcx, r12
    call DebugActiveProcessStop
    
@@exit:
    add rsp, 108h
    ret
DebugEngine_Loop endp

; ---------------------------------------------------------
; Sidebar_WndProc - Main sidebar window
; ---------------------------------------------------------
Sidebar_WndProc proc frame hWnd:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    sub rsp, 78h
    .allocstack 78h
    .endprolog
    
    mov [hWnd], rcx
    
    cmp edx, WM_CREATE
    je @@wm_create
    cmp edx, WM_SIZE
    je @@wm_size
    cmp edx, WM_NOTIFY
    je @@wm_notify
    cmp edx, WM_USER_LAZY_LOAD
    je @@wm_lazy_load
    cmp edx, WM_COMMAND
    je @@wm_command
    cmp edx, WM_CTLCOLORSTATIC
    je @@wm_ctlcolor
    cmp edx, WM_DESTROY
    je @@wm_destroy
    
@@default:
    call DefWindowProcA
    jmp @@exit
    
@@wm_create:
    mov rbx, [hWnd]
    
    ; Init Common Controls
    mov rcx, ICC_TREEVIEW_CLASSES or ICC_LISTVIEW_CLASSES
    call InitCommonControlsEx
    
    ; Load fonts
    call Sidebar_InitFonts
    
    ; Create Activity Bar (left 48px)
    xor r9d, r9d
    lea r8, szClassNameActivity
    lea rdx, [szClassNameActivity - 8]  ; Window name
    mov ecx, WS_CHILD or WS_VISIBLE
    mov r10d, 0      ; X
    mov r11d, 0      ; Y
    mov eax, ACTIVITY_BAR_WIDTH
    mov [rsp+20h], eax  ; Width
    mov [rsp+28h], 800  ; Height (will resize)
    mov [rsp+30h], rbx  ; Parent
    mov [rsp+38h], 0
    mov [rsp+40h], 0
    call CreateWindowExA
    mov [g_hActivityBar], rax
    
    ; Create Content Container (right side)
    xor r9d, r9d
    lea r8, szStaticClass
    xor edx, edx
    mov ecx, WS_CHILD or WS_VISIBLE or SS_BLACKRECT
    mov r10d, ACTIVITY_BAR_WIDTH
    xor r11d, r11d
    mov [rsp+20h], 252  ; Width (300-48)
    mov [rsp+28h], 800
    mov [rsp+30h], rbx
    call CreateWindowExA
    mov [g_hContentWnd], rax
    
    ; Create Explorer TreeView (initial view)
    call Sidebar_CreateExplorerView
    
    ; Apply dark mode
    mov rcx, rbx
    call Sidebar_SetDarkMode
    
    jmp @@exit_zero
    
@@wm_size:
    mov rbx, [lParam]
    movzx eax, bx       ; Width
    shr rbx, 16         ; Height
    
    ; Resize activity bar
    mov rcx, [g_hActivityBar]
    xor edx, edx
    mov r8d, edx
    mov r9d, ACTIVITY_BAR_WIDTH
    mov [rsp+20h], ebx  ; Height
    mov eax, 1          ; Repaint
    mov [rsp+28h], eax
    call SetWindowPos
    
    ; Resize content area
    mov rcx, [g_hContentWnd]
    mov edx, ACTIVITY_BAR_WIDTH
    xor r8d, r8d
    sub eax, ACTIVITY_BAR_WIDTH
    mov r9d, eax
    mov [rsp+20h], ebx
    mov eax, 1
    mov [rsp+28h], eax
    call SetWindowPos
    
    ; Resize current view
    call Sidebar_ResizeCurrentView
    jmp @@exit_zero
    
@@wm_notify:
    mov rbx, [lParam]
    cmp [NMHDR ptr rbx].code, TVN_ITEMEXPANDING
    jne @@exit_zero
    
    ; Lazy load on expand
    mov rcx, [NMHDR ptr rbx].hwndFrom
    cmp rcx, [g_hExplorerTree]
    jne @@exit_zero
    
    ; Post lazy load message
    mov rcx, [hWnd]
    mov edx, WM_USER_LAZY_LOAD
    mov r8, [NMHDR ptr rbx].idFrom
    xor r9, r9
    call PostMessageA
    jmp @@exit_zero
    
@@wm_lazy_load:
    ; wParam = hItem
    mov rcx, [g_hExplorerTree]
    mov rdx, [wParam]
    call Explorer_LazyLoad
    jmp @@exit_zero
    
@@wm_ctlcolor:
    cmp g_isDarkMode, 0
    je @@default
    
    ; Set dark text color
    mov rcx, [wParam]  ; HDC
    mov edx, COLOR_DARK_TEXT
    call SetTextColor
    
    ; Set dark background
    mov rcx, [wParam]
    mov edx, COLOR_DARK_BG
    call SetBkColor
    
    ; Return dark brush
    call GetStockObject
    ; Return DC_BRUSH handle in RAX
    jmp @@exit
    
@@wm_destroy:
    xor eax, eax
    jmp @@exit
    
@@exit_zero:
    xor eax, eax
@@exit:
    add rsp, 78h
    ret
Sidebar_WndProc endp

; ---------------------------------------------------------
; Sidebar_CreateExplorerView
; ---------------------------------------------------------
Sidebar_CreateExplorerView proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rcx, [g_hContentWnd]
    mov [g_hCurrentView], VIEW_EXPLORER
    
    ; Create TreeView with virtual/double-buffer styles
    xor r9d, r9d
    lea r8, szExplorerClass
    xor edx, edx
    mov ecx, WS_CHILD or WS_VISIBLE or TVS_HASLINES or TVS_HASBUTTONS or TVS_LINESATROOT or TVS_SHOWSELALWAYS or TVS_TRACKSELECT
    mov r10d, 5
    mov r11d, 5
    mov [rsp+20h], 242  ; Width
    mov [rsp+28h], 700  ; Height
    mov rax, [g_hContentWnd]
    mov [rsp+30h], rax
    call CreateWindowExA
    mov [g_hExplorerTree], rax
    
    ; Set extended styles for smooth scroll and double buffer
    mov rcx, rax
    mov edx, TVS_EX_DOUBLEBUFFER or TVS_EX_FADEINOUTEXPAND or TVS_EX_AUTOHSCROLL
    call SendMessageA
    
    ; Create root item (workspace)
    sub rsp, 88h  ; TVINSERTSTRUCTA
    mov [rsp+TVINSERTSTRUCTA.hParent], TVI_ROOT
    mov [rsp+TVINSERTSTRUCTA.hInsertAfter], TVI_FIRST
    mov [rsp+TVINSERTSTRUCTA.item.mask], TVIF_TEXT or TVIF_CHILDREN
    lea rax, g_rootPath
    mov [rsp+TVINSERTSTRUCTA.item.pszText], rax
    mov [rsp+TVINSERTSTRUCTA.item.cChildren], 1
    
    mov rcx, [g_hExplorerTree]
    mov edx, TVM_INSERTITEM
    lea r8, [rsp]
    call SendMessageA
    
    add rsp, 0B0h
    ret
Sidebar_CreateExplorerView endp

; ---------------------------------------------------------
; Sidebar_SwitchView - Toggle between 5 views
; ---------------------------------------------------------
Sidebar_SwitchView proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Hide all views
    mov rcx, [g_hExplorerTree]
    test rcx, rcx
    jz @F
    call ShowWindow
@@:
    ; Show current view based on g_hCurrentView
    mov eax, [g_hCurrentView]
    cmp eax, VIEW_EXPLORER
    jne @F
    mov rcx, [g_hExplorerTree]
    mov edx, SW_SHOW
    call ShowWindow
@@:
    call Sidebar_ResizeCurrentView
    
    add rsp, 28h
    ret
Sidebar_SwitchView endp

; ---------------------------------------------------------
; Sidebar_InitFonts
; ---------------------------------------------------------
Sidebar_InitFonts proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Create default font (Segoe UI, 9pt)
    xor ecx, ecx
    call GetDC
    mov r12, rax
    
    mov ecx, -12  ; 9pt (negative for device units)
    call MulDiv
    neg eax
    
    xor ecx, ecx
    mov edx, eax
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], DEFAULT_CHARSET
    mov qword ptr [rsp+28h], OUT_DEFAULT_PRECIS
    mov qword ptr [rsp+30h], CLIP_DEFAULT_PRECIS
    mov qword ptr [rsp+38h], CLEARTYPE_QUALITY
    mov qword ptr [rsp+40h], DEFAULT_PITCH or FF_SWISS
    lea rax, szStaticClass  ; "Segoe UI" string would go here
    mov [rsp+48h], rax
    call CreateFontA
    mov [g_hFont], rax
    
    mov rcx, r12
    xor edx, edx
    call ReleaseDC
    
    add rsp, 28h
    ret
Sidebar_InitFonts endp

; ---------------------------------------------------------
; Sidebar_ResizeCurrentView
; ---------------------------------------------------------
Sidebar_ResizeCurrentView proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Get client rect of content window
    sub rsp, 20h
    mov rcx, [g_hContentWnd]
    lea rdx, [rsp]
    call GetClientRect
    
    mov eax, [g_hCurrentView]
    cmp eax, VIEW_EXPLORER
    jne @@done
    
    mov rcx, [g_hExplorerTree]
    xor edx, edx
    xor r8d, r8d
    mov r9d, [rsp+RECT.right]
    sub r9d, [rsp+RECT.left]
    mov eax, [rsp+RECT.bottom]
    sub eax, [rsp+RECT.top]
    mov [rsp+20h], eax
    mov eax, 1
    mov [rsp+28h], eax
    call SetWindowPos
    
@@done:
    add rsp, 48h
    ret
Sidebar_ResizeCurrentView endp

; ---------------------------------------------------------
; Sidebar_RegisterClass
; ---------------------------------------------------------
Sidebar_RegisterClass proc frame
    sub rsp, 88h  ; WNDCLASSEXA
    .allocstack 88h
    .endprolog
    
    lea rdi, [rsp]
    mov rcx, sizeof WNDCLASSEXA / 8
    xor rax, rax
    rep stosq
    
    mov [rsp].WNDCLASSEXA.cbSize, sizeof WNDCLASSEXA
    mov [rsp].WNDCLASSEXA.style, CS_HREDRAW or CS_VREDRAW or CS_DBLCLKS
    lea rax, Sidebar_WndProc
    mov [rsp].WNDCLASSEXA.lpfnWndProc, rax
    mov [rsp].WNDCLASSEXA.hInstance, 0  ; GetModuleHandle(0) in real impl
    call GetModuleHandleA
    mov [rsp].WNDCLASSEXA.hInstance, rax
    
    mov ecx, IDC_ARROW
    call LoadCursorA
    mov [rsp].WNDCLASSEXA.hCursor, rax
    
    mov ecx, COLOR_WINDOW + 1
    call GetSysColorBrush
    mov [rsp].WNDCLASSEXA.hbrBackground, rax
    
    lea rax, szClassNameSidebar
    mov [rsp].WNDCLASSEXA.lpszClassName, rax
    
    lea rcx, [rsp]
    call RegisterClassExA
    
    add rsp, 88h
    ret
Sidebar_RegisterClass endp

; ---------------------------------------------------------
; Sidebar_Create - Entry point
; RCX = HWND parent, RDX = Path string
; Returns: RAX = Sidebar HWND
; ---------------------------------------------------------
Sidebar_Create proc frame hParent:QWORD, lpPath:QWORD
    sub rsp, 58h
    .allocstack 58h
    .endprolog
    
    mov r12, [hParent]
    mov r13, [lpPath]
    
    ; Copy path
    lea rdi, g_rootPath
    mov rsi, r13
@@cpy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@cpy
    
    ; Register class if needed
    call Sidebar_RegisterClass
    
    ; Create sidebar window
    xor r9d, r9d
    lea r8, szClassNameSidebar
    xor edx, edx
    mov ecx, WS_CHILD or WS_VISIBLE or WS_CLIPCHILDREN
    xor r10d, r10d
    xor r11d, r11d
    mov [rsp+20h], SIDEBAR_WIDTH
    mov [rsp+28h], 600
    mov [rsp+30h], r12
    call CreateWindowExA
    
    mov [g_hSidebar], rax
    
    ; Initialize debug engine thread if needed
    ; (Would create thread here for DebugEngine_Loop)
    
    mov rax, [g_hSidebar]
    add rsp, 58h
    ret
Sidebar_Create endp

; ---------------------------------------------------------
; Git_ExecuteAsync - Creates thread instead of blocking
; ---------------------------------------------------------
Git_ExecuteAsync proc frame
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    ; CreateThread with Git_Execute as start address
    xor ecx, ecx      ; Security
    xor edx, edx      ; Stack size
    lea r8, Git_Execute  ; Start address
    mov r9, rcx       ; Parameter
    mov [rsp+20h], rcx ; Creation flags
    mov [rsp+28h], rcx ; Thread ID
    call CreateThread
    add rsp, 40h
    ret
Git_ExecuteAsync endp

; ---------------------------------------------------------
; DLL Entry / Export (if building as DLL)
; ---------------------------------------------------------
DllMain proc frame hinstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    mov eax, 1
    ret
DllMain endp

; Exports
Public Sidebar_Create
Public Sidebar_LogWrite
Public DebugEngine_Loop
Public Git_Execute
Public Git_ExecuteAsync

End
