; Win32IDE_Sidebar_Pure.asm
; Pure x64 MASM64 - Zero Qt, Zero CRT, Zero Dependencies
; Win64 ABI: RCX, RDX, R8, R9, then stack (8+8 aligned)
; Assemble: ml64 /c /Fo$(OutDir)sidebar_pure.obj Win32IDE_Sidebar_Pure.asm

option casemap:none

; =========================================================
; EXPORTS
; =========================================================
public Sidebar_Init
public Sidebar_Create
public Sidebar_WndProc
public DebugEngine_Create
public DebugEngine_Step
public DebugEngine_Detach
public Logger_Write
public TreeView_PopulateAsync
public Theme_SetDarkMode
public reverse_string
public reverse_words
public reverse_string_inplace
public apply_text_reversal

; =========================================================
; CONSTANTS
; =========================================================
NULL equ 0
TRUE equ 1
FALSE equ 0
INFINITE equ 0FFFFFFFFh
INVALID_HANDLE_VALUE equ -1

; Window styles
WS_CHILD equ 40000000h
WS_VISIBLE equ 10000000h

; Window Messages
WM_CREATE equ 1h
WM_DESTROY equ 2h
WM_SIZE equ 5h
WM_COMMAND equ 111h
WM_USER equ 400h
WM_NOTIFY equ 4Eh
WM_PAINT equ 0Fh
WM_ERASEBKGND equ 14h
WM_TIMER equ 113h
TV_FIRST equ 1100h
TVM_INSERTITEM equ TV_FIRST + 50
TVM_EXPAND equ TV_FIRST + 2
TVM_SETIMAGELIST equ TV_FIRST + 9
TVM_GETNEXTITEM equ TV_FIRST + 10
TVGN_ROOT equ 0
TVGN_CHILD equ 4
TVGN_NEXT equ 1
TVIF_TEXT equ 1h
TVIF_IMAGE equ 2h
TVIF_SELECTEDIMAGE equ 20h
TVIF_CHILDREN equ 40h
TVS_HASBUTTONS equ 1h
TVS_HASLINES equ 2h
TVS_LINESATROOT equ 4h
TVS_SHOWSELALWAYS equ 20h
TVS_EX_DOUBLEBUFFER equ 4
TVS_EX_FADEINOUTEXPAND equ 40h
GWL_STYLE equ 0FFFFFFF0h
GWL_EXSTYLE equ 0FFFFFFECh
GWLP_WNDPROC equ 0FFFFFFFCh

; Process/Thread Access
DEBUG_PROCESS equ 1h
DEBUG_ONLY_THIS_PROCESS equ 2h
CREATE_NEW_CONSOLE equ 10h
DEBUG_EVENT_EXCEPTION equ 1
DEBUG_EVENT_CREATE_THREAD equ 2
DEBUG_EVENT_CREATE_PROCESS equ 3
DEBUG_EVENT_EXIT_THREAD equ 4
DEBUG_EVENT_EXIT_PROCESS equ 5
DEBUG_EVENT_LOAD_DLL equ 6
DEBUG_EVENT_UNLOAD_DLL equ 7
DEBUG_EVENT_OUTPUT_DEBUG_STRING equ 8
EXCEPTION_DEBUG_EVENT equ 1
CREATE_THREAD_DEBUG_EVENT equ 2
CREATE_PROCESS_DEBUG_EVENT equ 3
EXIT_THREAD_DEBUG_EVENT equ 4
EXIT_PROCESS_DEBUG_EVENT equ 5
LOAD_DLL_DEBUG_EVENT equ 6
UNLOAD_DLL_DEBUG_EVENT equ 7
OUTPUT_DEBUG_STRING_EVENT equ 8
DBG_CONTINUE equ 10002h
DBG_EXCEPTION_NOT_HANDLED equ 80010001h
CONTEXT_CONTROL equ 1h
CONTEXT_INTEGER equ 2h
CONTEXT_SEGMENTS equ 4h
CONTEXT_FLOATING_POINT equ 8h
CONTEXT_DEBUG_REGISTERS equ 10h
CONTEXT_FULL equ CONTEXT_CONTROL or CONTEXT_INTEGER or CONTEXT_SEGMENTS
CONTEXT_SIZE equ 1232   ; sizeof(CONTEXT) on x64

; File Access
GENERIC_WRITE equ 40000000h
GENERIC_READ equ 80000000h
OPEN_ALWAYS equ 4
FILE_SHARE_READ equ 1h
FILE_SHARE_WRITE equ 2h
FILE_ATTRIBUTE_NORMAL equ 80h

; DWM
DWMWA_USE_IMMERSIVE_DARK_MODE equ 14h

; =========================================================
; EXTERNALS (Win32 API)
; =========================================================
externdef __imp_CreateWindowExA:qword
externdef __imp_RegisterClassExA:qword
externdef __imp_DefWindowProcA:qword
externdef __imp_SendMessageA:qword
externdef __imp_PostMessageA:qword
externdef __imp_GetMessageA:qword
externdef __imp_PeekMessageA:qword
externdef __imp_TranslateMessage:qword
externdef __imp_DispatchMessageA:qword
externdef __imp_CreateFileA:qword
externdef __imp_WriteFile:qword
externdef __imp_CloseHandle:qword
externdef __imp_OutputDebugStringA:qword
externdef __imp_GetModuleHandleA:qword
externdef __imp_GetProcessHeap:qword
externdef __imp_HeapAlloc:qword
externdef __imp_HeapFree:qword
externdef __imp_CreateProcessA:qword
externdef __imp_WaitForDebugEventEx:qword
externdef __imp_ContinueDebugEvent:qword
externdef __imp_DebugActiveProcessStop:qword
externdef __imp_GetThreadContext:qword
externdef __imp_SetThreadContext:qword
externdef __imp_OpenThread:qword
externdef __imp_SuspendThread:qword
externdef __imp_ResumeThread:qword
externdef __imp_GetWindowLongPtrA:qword
externdef __imp_SetWindowLongPtrA:qword
externdef __imp_GetClientRect:qword
externdef __imp_MoveWindow:qword
externdef __imp_DwmSetWindowAttribute:qword
externdef __imp_lstrlenA:qword
externdef __imp_lstrcpyA:qword
externdef __imp_lstrcmpiA:qword
externdef __imp_wsprintfA:qword
externdef __imp_RtlZeroMemory:qword
externdef __imp_RtlCopyMemory:qword
externdef __imp_InitializeCriticalSection:qword
externdef __imp_EnterCriticalSection:qword
externdef __imp_LeaveCriticalSection:qword
externdef __imp_CreateThread:qword
externdef __imp_WaitForSingleObject:qword
externdef __imp_Sleep:qword
externdef __imp_GetTickCount64:qword

; =========================================================
; STRUCTURES
; =========================================================
WNDCLASSEXA struct
    cbSize          dd ?
    style           dd ?
    lpfnWndProc     dq ?
    cbClsExtra      dd ?
    cbWndExtra      dd ?
    hInstance       dq ?
    hIcon           dq ?
    hCursor         dq ?
    hbrBackground   dq ?
    lpszMenuName    dq ?
    lpszClassName   dq ?
    hIconSm         dq ?
WNDCLASSEXA ends

POINT struct
    x   dd ?
    y   dd ?
POINT ends

RECT struct
    left    dd ?
    top     dd ?
    right   dd ?
    bottom  dd ?
RECT ends

TVINSERTSTRUCTA struct
    hParent     dq ?
    hInsertAfter    dq ?
    item        db 56 dup(?) ; TVITEMEXA
TVINSERTSTRUCTA ends

DEBUG_EVENT struct
    dwDebugEventCode    dd ?
    dwProcessId         dd ?
    dwThreadId          dd ?
    union
        Exception           db 160 dup(?) ; EXCEPTION_DEBUG_INFO
        CreateThread        db 24 dup(?)  ; CREATE_THREAD_DEBUG_INFO
        CreateProcessInfo   db 48 dup(?)  ; CREATE_PROCESS_DEBUG_INFO
        ExitThread          db 8 dup(?)   ; EXIT_THREAD_DEBUG_INFO
        ExitProcess         db 8 dup(?)   ; EXIT_PROCESS_DEBUG_INFO
        LoadDll             db 32 dup(?)  ; LOAD_DLL_DEBUG_INFO
        UnloadDll           db 8 dup(?)   ; UNLOAD_DLL_DEBUG_INFO
        DebugString         db 24 dup(?)  ; OUTPUT_DEBUG_STRING_INFO
        RipInfo             db 8 dup(?)   ; RIP_INFO
    ends
DEBUG_EVENT ends

CONTEXT struct
    P1Home      dq ?
    P2Home      dq ?
    P3Home      dq ?
    P4Home      dq ?
    P5Home      dq ?
    P6Home      dq ?
    ContextFlags    dd ?
    MxCsr       dd ?
    SegCs       dw ?
    SegDs       dw ?
    SegEs       dw ?
    SegFs       dw ?
    SegGs       dw ?
    SegSs       dw ?
    EFlags      dd ?
    ctx_Dr0     dq ?
    ctx_Dr1     dq ?
    ctx_Dr2     dq ?
    ctx_Dr3     dq ?
    ctx_Dr6     dq ?
    ctx_Dr7     dq ?
    ctx_Rax     dq ?
    ctx_Rcx     dq ?
    ctx_Rdx     dq ?
    ctx_Rbx     dq ?
    ctx_Rsp     dq ?
    ctx_Rbp     dq ?
    ctx_Rsi     dq ?
    ctx_Rdi     dq ?
    ctx_R8      dq ?
    ctx_R9      dq ?
    ctx_R10     dq ?
    ctx_R11     dq ?
    ctx_R12     dq ?
    ctx_R13     dq ?
    ctx_R14     dq ?
    ctx_R15     dq ?
    ctx_Rip     dq ?
    FltSave     db 512 dup(?) ; XSAVE_FORMAT
CONTEXT ends

CREATE_PROCESS_DEBUG_INFO struct
    hFile                   dq ?
    hProcess                dq ?
    hThread                 dq ?
    lpBaseOfImage           dq ?
    dwDebugInfoFileOffset   dd ?
    nDebugInfoSize          dd ?
    lpThreadLocalBase       dq ?
    lpStartAddress          dq ?
    lpImageName             dq ?
    fUnicode                dw ?
    pad                     dw ?
CREATE_PROCESS_DEBUG_INFO ends

EXCEPTION_DEBUG_INFO struct
    ExceptionRecord db 152 dup(?) ; EXCEPTION_RECORD
    dwFirstChance   dd ?
    pad             dd ?
EXCEPTION_DEBUG_INFO ends

CRITICAL_SECTION struct
    DebugInfo       dq ?
    LockCount       dd ?
    RecursionCount  dd ?
    OwningThread    dq ?
    LockSemaphore   dq ?
    SpinCount       dq ?
CRITICAL_SECTION ends

; =========================================================
; DATA SECTION
; =========================================================
.data
align 8
g_hSidebarWnd       dq 0
g_hTreeView         dq 0
g_hDebugProcess     dq 0
g_hDebugThread      dq 0
g_dwProcessId       dd 0
g_dwThreadId        dd 0
g_bDebugging        db 0
g_hLogFile          dq INVALID_HANDLE_VALUE
g_csLog             CRITICAL_SECTION <>
g_csDebug           CRITICAL_SECTION <>
g_szLogPath         db "D:\\rawrxd\\logs\\sidebar_debug.log",0
g_szClassName       db "RawrXD_Sidebar_Pure",0
g_szTreeViewClass   db "SysTreeView32",0
g_szDbgPrefix       db "[RAWRXD DBG] ",0
g_szNewline         db 13,10,0

; Debug register strings for logging
g_szDr0             db "Dr0=",0
g_szDr1             db " Dr1=",0
g_szDr2             db " Dr2=",0
g_szDr3             db " Dr3=",0
g_szRip             db " Rip=",0
g_szRax             db " Rax=",0

; Reversal engine data
align 8
g_reversals_applied dq 0

; Reversal strategy constants
REVERSAL_FULL       equ 1       ; Full character reversal
REVERSAL_WORDS      equ 2       ; Word-order reversal

; =========================================================
; BSS SECTION (Uninitialized)
; =========================================================
.data?
align 16
g_ctx               db 1232 dup(?)  ; CONTEXT structure (1232 bytes)
g_dbgEvent          DEBUG_EVENT <>
g_szTempBuffer      db 4096 dup(?)
g_szModuleBuffer    db 260 dup(?)
g_szReversalBuf     db 4096 dup(?)  ; Scratch buffer for reversal ops
g_szWordStarts      dq 256 dup(?)   ; Pointer table for word-start addresses
g_dwWordLens        dd 256 dup(?)   ; Parallel table of word lengths

; =========================================================
; CODE SECTION
; =========================================================
.code

; ---------------------------------------------------------
; Helper: strlen
; RCX = string
; Returns RAX = length
Sidebar_strlen proc
    mov rax, rcx
@@:
    cmp byte ptr [rax], 0
    je @F
    inc rax
    jmp @B
@@:
    sub rax, rcx
    ret
Sidebar_strlen endp

; ---------------------------------------------------------
; Logger_Write: Pure Win32 logging, no Qt
; RCX = Level (0=Debug,1=Info,2=Warn,3=Error)
; RDX = File (char*)
; R8  = Line (int)
; R9  = Message (char*)
Logger_Write proc
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 104    ; Increased from 88h for proper alignment
    
    mov r12, rcx        ; Level
    mov rsi, rdx        ; File
    mov rdi, r9         ; Message
    
    ; Enter critical section
    lea rcx, g_csLog
    call __imp_EnterCriticalSection
    
    ; Open log file if not open
    cmp g_hLogFile, INVALID_HANDLE_VALUE
    jne @F
    
    lea rcx, g_szLogPath
    mov edx, GENERIC_WRITE
    mov r8d, FILE_SHARE_READ
    mov r9d, NULL
    mov qword ptr [rsp+40h], OPEN_ALWAYS
    mov qword ptr [rsp+48h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+50h], NULL
    call __imp_CreateFileA
    mov g_hLogFile, rax
    
@@:
    ; Format: [LEVEL][File:Line] Message\r\n
    lea rbx, g_szTempBuffer
    
    ; Add timestamp prefix (simplified as [RAWRXD])
    lea rcx, g_szDbgPrefix
    mov rdx, rbx
@@copy_prefix:
    mov al, [rcx]
    mov [rdx], al
    inc rcx
    inc rdx
    test al, al
    jnz @@copy_prefix
    dec rdx
    
    ; Add level
    mov byte ptr [rdx], '['
    inc rdx
    cmp r12, 0
    je @@lvl_dbg
    cmp r12, 1
    je @@lvl_info
    cmp r12, 2
    je @@lvl_warn
@@lvl_err:
    mov dword ptr [rdx], "RRE]" ; ERR]
    add rdx, 4
    jmp @@lvl_done
@@lvl_warn:
    mov dword ptr [rdx], "NRW]" ; WRN]
    add rdx, 4
    jmp @@lvl_done
@@lvl_dbg:
    mov dword ptr [rdx], "GBD]" ; DBG]
    add rdx, 4
    jmp @@lvl_done
@@lvl_info:
    mov byte ptr [rdx], 'I'
    mov byte ptr [rdx+1], 'N'
    mov byte ptr [rdx+2], 'F'
    mov byte ptr [rdx+3], 'O'
    mov byte ptr [rdx+4], ']'
    add rdx, 5
@@lvl_done:
    mov byte ptr [rdx], '['
    inc rdx
    
    ; Copy filename (just last part)
    mov rcx, rsi
    call Sidebar_strlen
    mov r8, rax
    
    ; Find last backslash
    mov rcx, rsi
    add rcx, r8
@@find_slash:
    cmp rcx, rsi
    je @@copy_file_start
    mov al, [rcx]
    cmp al, '\'
    je @@copy_file_start
    cmp al, '/'
    je @@copy_file_start
    dec rcx
    jmp @@find_slash
@@copy_file_start:
    inc rcx
    
    ; Copy from slash to end
@@copy_file:
    mov al, [rcx]
    test al, al
    jz @@file_done
    mov [rdx], al
    inc rcx
    inc rdx
    jmp @@copy_file
@@file_done:
    
    ; Add line number
    mov byte ptr [rdx], ':'
    inc rdx
    ; Convert R8 (line) to string - simplified, just show 0 for now in this stub
    mov byte ptr [rdx], '0'
    inc rdx
    mov byte ptr [rdx], ']'
    mov byte ptr [rdx+1], ' '
    add rdx, 2
    
    ; Copy message
    mov rcx, rdi
@@copy_msg:
    mov al, [rcx]
    mov [rdx], al
    inc rcx
    inc rdx
    test al, al
    jnz @@copy_msg
    dec rdx
    
    ; Add newline
    mov byte ptr [rdx], 13
    mov byte ptr [rdx+1], 10
    mov byte ptr [rdx+2], 0
    add rdx, 2
    
    ; Calculate length
    lea rcx, g_szTempBuffer
    sub rdx, rcx
    mov r10, rdx        ; Save length in r10
    
    ; Write to file
    mov rcx, g_hLogFile
    lea rdx, g_szTempBuffer
    mov r8, r10         ; Length from r10
    lea r9, [rsp+36h]   ; Bytes written pointer (adjusted offset)
    mov qword ptr [rsp+40h], NULL
    call __imp_WriteFile
    
    ; Also OutputDebugString
    lea rcx, g_szTempBuffer
    call __imp_OutputDebugStringA
    
    ; Leave critical section
    lea rcx, g_csLog
    call __imp_LeaveCriticalSection
    
    add rsp, 104
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Logger_Write endp

; ---------------------------------------------------------
; DebugEngine_Create: Real Win32 debugging, zero stubs
; RCX = CommandLine (char*)
; RDX = WorkingDir (char*)
; Returns: RAX = 0 (success) or error code
DebugEngine_Create proc
    push rbx
    push rsi
    push rdi
    sub rsp, 176    ; STARTUPINFOA (104) + PROCESS_INFORMATION (24) + extra align padding
    
    mov rsi, rcx        ; CommandLine
    mov rdi, rdx        ; WorkingDir
    
    ; Zero structures
    lea rcx, [rsp+20h]
    mov edx, 128        ; sizeof(STARTUPINFOA) rounded up
    call __imp_RtlZeroMemory
    
    lea rcx, [rsp+0A8h]
    mov edx, 24         ; sizeof(PROCESS_INFORMATION)
    call __imp_RtlZeroMemory
    
    ; Set STARTUPINFOA size
    mov dword ptr [rsp+20h], 68h  ; sizeof(STARTUPINFOA)
    
    ; CreateProcess with DEBUG_PROCESS
    mov rcx, NULL       ; ApplicationName
    mov rdx, rsi        ; CommandLine
    mov r8, NULL        ; ProcessAttributes
    mov r9, NULL        ; ThreadAttributes
    
    mov qword ptr [rsp+40h], TRUE ; InheritHandles
    mov rax, DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS or CREATE_NEW_CONSOLE
    mov qword ptr [rsp+48h], rax  ; CreationFlags
    mov qword ptr [rsp+50h], NULL ; Environment
    mov qword ptr [rsp+58h], rdi  ; CurrentDirectory
    lea rax, [rsp+20h]
    mov qword ptr [rsp+60h], rax  ; StartupInfo
    lea rax, [rsp+0A8h]
    mov qword ptr [rsp+68h], rax  ; ProcessInformation
    
    call __imp_CreateProcessA
    test eax, eax
    jz @@error
    
    ; Store handles
    mov rax, [rsp+0A8h]  ; hProcess
    mov g_hDebugProcess, rax
    mov rax, [rsp+0B0h]  ; hThread
    mov g_hDebugThread, rax
    mov eax, [rsp+0B8h]  ; dwProcessId
    mov g_dwProcessId, eax
    mov eax, [rsp+0BCh]  ; dwThreadId
    mov g_dwThreadId, eax
    mov g_bDebugging, 1
    
    xor eax, eax
    jmp @@done
    
@@error:
    mov eax, 1
    
@@done:
    add rsp, 176
    pop rdi
    pop rsi
    pop rbx
    ret
DebugEngine_Create endp

; ---------------------------------------------------------
; DebugEngine_Step: Single step using TF (Trap Flag)
; RCX = bInto (0=Over, 1=Into)
DebugEngine_Step proc
    push rbx
    sub rsp, 104
    
    mov bl, cl      ; Save bInto
    
    ; Enter debug CS
    lea rcx, g_csDebug
    call __imp_EnterCriticalSection
    
    ; Get context
    mov dword ptr [rsp+20h], CONTEXT_FULL  ; ContextFlags at first field
    mov rcx, g_hDebugThread
    lea rdx, [rsp+20h]  ; temp context on stack
    call __imp_GetThreadContext
    test eax, eax
    jz @@done
    
    ; Set Trap Flag (EFLAGS bit 8) - offset 40 in CONTEXT
    lea rax, [rsp+20h]
    add rax, 40
    or dword ptr [rax], 100h   ; TF = 0x100
    
    ; If step into (call/int), don't skip
    ; If step over, we need to check if current instruction is CALL and set breakpoint after
    
    mov rcx, g_hDebugThread
    lea rdx, [rsp+20h]  ; temp context on stack
    call __imp_SetThreadContext
    
    ; Continue execution
    mov ecx, g_dwProcessId
    mov edx, g_dwThreadId
    mov r8d, DBG_CONTINUE
    call __imp_ContinueDebugEvent
    
@@done:
    lea rcx, g_csDebug
    call __imp_LeaveCriticalSection
    
    add rsp, 104
    pop rbx
    ret
DebugEngine_Step endp

; ---------------------------------------------------------
; DebugEngine_Detach
DebugEngine_Detach proc
    mov g_bDebugging, 0
    mov ecx, g_dwProcessId
    call __imp_DebugActiveProcessStop
    ret
DebugEngine_Detach endp

; ---------------------------------------------------------
; TreeView_PopulateAsync: Post message to trigger lazy load
; RCX = hTreeView
TreeView_PopulateAsync proc
    mov rdx, rcx        ; hWnd
    mov ecx, WM_USER + 100h  ; Custom populate message
    xor r8d, r8d        ; wParam
    xor r9d, r9d        ; lParam
    call __imp_PostMessageA
    ret
TreeView_PopulateAsync endp

; ---------------------------------------------------------
; Theme_SetDarkMode: Force DWM dark mode
; RCX = hWnd
; RDX = bDark (0/1)
Theme_SetDarkMode proc
    sub rsp, 28h
    
    mov r8d, edx        ; bDark
    mov edx, DWMWA_USE_IMMERSIVE_DARK_MODE
    mov r9d, 4          ; sizeof(BOOL)
    mov dword ptr [rsp+20h], r9d
    call __imp_DwmSetWindowAttribute
    
    add rsp, 28h
    ret
Theme_SetDarkMode endp

; ---------------------------------------------------------
; Sidebar_WndProc: Main window procedure
Sidebar_WndProc proc
    ; Save non-volatile
    push rbx
    push rbp
    push rsi
    push rdi
    sub rsp, 88h
    
    mov rbx, rcx    ; hWnd
    mov esi, edx    ; Msg
    mov rdi, r8     ; wParam
    mov rbp, r9     ; lParam
    
    cmp esi, WM_CREATE
    je @@wm_create
    cmp esi, WM_SIZE
    je @@wm_size
    cmp esi, WM_USER + 100h
    je @@wm_populate_tree
    cmp esi, WM_NOTIFY
    je @@wm_notify
    
    jmp @@default
    
@@wm_create:
    ; Create TreeView child window
    xor ecx, ecx        ; ExStyle
    lea rdx, g_szTreeViewClass
    xor r8d, r8d        ; WindowName
    mov r9d, WS_CHILD or WS_VISIBLE or TVS_HASBUTTONS or TVS_HASLINES or TVS_LINESATROOT or TVS_SHOWSELALWAYS
    
    mov qword ptr [rsp+28h], 0      ; X
    mov qword ptr [rsp+30h], 0      ; Y
    mov qword ptr [rsp+38h], 300    ; Width
    mov qword ptr [rsp+40h], 800    ; Height
    mov rax, rbx
    mov qword ptr [rsp+48h], rax    ; hWndParent
    mov qword ptr [rsp+50h], 1      ; hMenu (ID)
    xor eax, eax
    mov qword ptr [rsp+58h], rax    ; hInstance - get from GetModuleHandle
    mov qword ptr [rsp+60h], rax    ; lpParam
    
    call __imp_CreateWindowExA
    mov g_hTreeView, rax
    
    ; Set extended styles for double buffering
    mov rcx, rax
    mov edx, GWL_EXSTYLE
    call __imp_GetWindowLongPtrA
    or rax, TVS_EX_DOUBLEBUFFER or TVS_EX_FADEINOUTEXPAND
    mov rcx, g_hTreeView
    mov edx, GWL_EXSTYLE
    mov r8, rax
    call __imp_SetWindowLongPtrA
    
    xor eax, eax
    jmp @@done
    
@@wm_size:
    ; Resize treeview to fill sidebar
    mov rcx, rbx
    lea rdx, [rsp+20h]  ; RECT
    call __imp_GetClientRect
    
    mov rcx, g_hTreeView
    xor edx, edx
    xor r8d, r8d
    mov r9d, [rsp+28h]  ; right (width) -> r9 for 4th param
    mov eax, [rsp+2Ch]  ; bottom (height)
    mov dword ptr [rsp+28h], eax   ; 5th param: height
    mov qword ptr [rsp+30h], 1      ; 6th param: bRepaint
    call __imp_MoveWindow
    
    xor eax, eax
    jmp @@done
    
@@wm_populate_tree:
    ; Async tree population - insert root item
    mov rcx, g_hTreeView
    mov edx, TVM_INSERTITEM
    lea r8, [rsp+20h]   ; TVINSERTSTRUCT
    
    ; Fill insert struct (simplified - just root)
    mov qword ptr [r8], TVGN_ROOT    ; hParent
    mov qword ptr [r8+8], 0          ; hInsertAfter
    
    xor eax, eax
    jmp @@done
    
@@wm_notify:
    ; Handle tree notifications (selection change, expand, etc)
    xor eax, eax
    jmp @@done
    
@@default:
    mov rcx, rbx
    mov edx, esi
    mov r8, rdi
    mov r9, rbp
    call __imp_DefWindowProcA
    
@@done:
    add rsp, 88h
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
Sidebar_WndProc endp

; ---------------------------------------------------------
; Sidebar_Init: Initialize critical sections and logging
Sidebar_Init proc
    sub rsp, 28h
    
    lea rcx, g_csLog
    call __imp_InitializeCriticalSection
    
    lea rcx, g_csDebug
    call __imp_InitializeCriticalSection
    
    xor eax, eax
    add rsp, 28h
    ret
Sidebar_Init endp

; ---------------------------------------------------------
; Sidebar_Create: Create sidebar window
; RCX = hParent
; RDX = x, R8 = y, R9 = width
; [rsp+28h] = height
Sidebar_Create proc
    push rbx
    sub rsp, 88h
    
    mov rbx, rcx    ; hParent
    
    ; Register class if needed (simplified - assume pre-registered or use system class)
    
    ; Create window
    xor ecx, ecx        ; ExStyle
    lea rdx, g_szClassName
    lea r8, szSidebarTitle
    mov r9d, WS_CHILD or WS_VISIBLE
    
    mov qword ptr [rsp+28h], rdx  ; lpClassName
    mov qword ptr [rsp+30h], r8   ; lpWindowName
    mov qword ptr [rsp+38h], r9   ; dwStyle
    mov qword ptr [rsp+40h], 0    ; x
    mov qword ptr [rsp+48h], 0    ; y
    mov qword ptr [rsp+50h], 300  ; nWidth
    mov rax, [rsp+0E0h] ; height from original stack (after push rbx; sub rsp,88h)
    mov qword ptr [rsp+58h], rax  ; nHeight
    mov qword ptr [rsp+60h], rbx  ; hWndParent
    mov qword ptr [rsp+68h], NULL ; hMenu
    xor eax, eax
    mov qword ptr [rsp+70h], rax  ; hInstance
    mov qword ptr [rsp+78h], rax  ; lpParam
    
    call __imp_CreateWindowExA
    
    mov g_hSidebarWnd, rax
    
    add rsp, 88h
    pop rbx
    ret

szSidebarTitle  db "RawrXD Sidebar",0
Sidebar_Create endp

; =========================================================
; REVERSAL ENGINE — Text transformation subsystem
; =========================================================
; Architecture:
;   reverse_string        — Full character reversal (src → dst)
;   reverse_words          — Word-order reversal, words intact
;   reverse_string_inplace — In-place swap, zero-alloc
;   apply_text_reversal    — Strategy dispatch (1=full, 2=words)
;
; ABI: Win64 — RCX, RDX, R8, R9, stack
; All functions are null-safe. NULL src → immediate ret (RAX=0).
; Reversal count tracked in g_reversals_applied.
; =========================================================

; ---------------------------------------------------------
; reverse_string
; Full character reversal: "Hello" → "olleH"
; RCX = src (null-terminated)
; RDX = dst (caller-allocated, at least strlen(src)+1)
; Returns RAX = length of reversed string
; ---------------------------------------------------------
reverse_string proc
    push rbx
    push rsi
    push rdi
    sub rsp, 28h

    ; Null guard
    test rcx, rcx
    jz @@rs_null
    test rdx, rdx
    jz @@rs_null

    mov rsi, rcx        ; src
    mov rdi, rdx        ; dst

    ; Measure src length
    mov rcx, rsi
    xor rax, rax
@@rs_len:
    cmp byte ptr [rcx+rax], 0
    je @@rs_len_done
    inc rax
    jmp @@rs_len
@@rs_len_done:
    mov rbx, rax        ; rbx = length
    test rbx, rbx
    jz @@rs_empty

    ; Copy in reverse: dst[i] = src[len-1-i]
    xor rcx, rcx        ; i = 0
@@rs_copy:
    cmp rcx, rbx
    jge @@rs_term
    mov rax, rbx
    dec rax
    sub rax, rcx        ; rax = len-1-i
    movzx edx, byte ptr [rsi+rax]
    mov byte ptr [rdi+rcx], dl
    inc rcx
    jmp @@rs_copy
@@rs_term:
    mov byte ptr [rdi+rbx], 0   ; Null-terminate

    ; Increment stats
    lock inc qword ptr [g_reversals_applied]

    mov rax, rbx        ; Return length
    jmp @@rs_ret

@@rs_empty:
    mov byte ptr [rdi], 0
    xor eax, eax
    jmp @@rs_ret

@@rs_null:
    xor eax, eax

@@rs_ret:
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    ret
reverse_string endp

; ---------------------------------------------------------
; reverse_words
; Reverse word order: "one two three" → "three two one"
; Words themselves stay intact.
; RCX = src (null-terminated)
; RDX = dst (caller-allocated, at least strlen(src)+1)
; Returns RAX = length written to dst
; ---------------------------------------------------------
reverse_words proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 28h

    ; Null guard
    test rcx, rcx
    jz @@rw_null
    test rdx, rdx
    jz @@rw_null

    mov rsi, rcx        ; src
    mov rdi, rdx        ; dst
    xor r12d, r12d      ; word count = 0

    ; --- Pass 1: Tokenize words into g_szWordStarts / g_dwWordLens ---
    xor rcx, rcx        ; i = 0
@@rw_skip_lead:
    movzx eax, byte ptr [rsi+rcx]
    test al, al
    jz @@rw_tokenized
    cmp al, ' '
    je @@rw_adv_lead
    cmp al, 9           ; tab
    je @@rw_adv_lead
    jmp @@rw_word_start
@@rw_adv_lead:
    inc rcx
    jmp @@rw_skip_lead

@@rw_word_start:
    ; Record word start pointer
    lea rax, [rsi+rcx]
    mov qword ptr [g_szWordStarts + r12*8], rax
    mov r13, rcx        ; save start index

    ; Scan to end of word
@@rw_scan_word:
    movzx eax, byte ptr [rsi+rcx]
    test al, al
    jz @@rw_word_end
    cmp al, ' '
    je @@rw_word_end
    cmp al, 9
    je @@rw_word_end
    inc rcx
    jmp @@rw_scan_word

@@rw_word_end:
    ; Compute word length
    mov rax, rcx
    sub rax, r13
    mov dword ptr [g_dwWordLens + r12*4], eax
    inc r12             ; word count++
    cmp r12, 256
    jge @@rw_tokenized   ; Guard: max 256 words

    ; Skip trailing whitespace
@@rw_skip_trail:
    movzx eax, byte ptr [rsi+rcx]
    test al, al
    jz @@rw_tokenized
    cmp al, ' '
    je @@rw_adv_trail
    cmp al, 9
    je @@rw_adv_trail
    jmp @@rw_word_start  ; Next word
@@rw_adv_trail:
    inc rcx
    jmp @@rw_skip_trail

@@rw_tokenized:
    ; --- Pass 2: Write words in reverse order into dst ---
    test r12d, r12d
    jz @@rw_empty

    xor r14d, r14d      ; dst write offset = 0
    mov rcx, r12
    dec rcx             ; i = word_count - 1

@@rw_emit:
    ; Get word [i]
    mov rax, qword ptr [g_szWordStarts + rcx*8]
    mov r13d, dword ptr [g_dwWordLens + rcx*4]

    ; Copy word bytes
    xor edx, edx
@@rw_copy_word:
    cmp edx, r13d
    jge @@rw_word_copied
    movzx r8d, byte ptr [rax+rdx]
    mov byte ptr [rdi+r14], r8b
    inc edx
    inc r14
    jmp @@rw_copy_word

@@rw_word_copied:
    ; Add space separator unless this is the last word (i==0)
    test ecx, ecx
    jz @@rw_finish
    mov byte ptr [rdi+r14], ' '
    inc r14
    dec rcx
    jmp @@rw_emit

@@rw_finish:
    mov byte ptr [rdi+r14], 0   ; Null-terminate

    ; Stats
    lock inc qword ptr [g_reversals_applied]

    mov rax, r14        ; Return length
    jmp @@rw_ret

@@rw_empty:
    mov byte ptr [rdi], 0
    xor eax, eax
    jmp @@rw_ret

@@rw_null:
    xor eax, eax

@@rw_ret:
    add rsp, 28h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
reverse_words endp

; ---------------------------------------------------------
; reverse_string_inplace
; In-place character reversal — zero extra allocation
; RCX = string (null-terminated, writable)
; Returns RAX = length
; ---------------------------------------------------------
reverse_string_inplace proc
    push rbx
    push rsi
    sub rsp, 28h

    ; Null guard
    test rcx, rcx
    jz @@ri_null

    mov rsi, rcx        ; base pointer

    ; Find length
    xor rax, rax
@@ri_len:
    cmp byte ptr [rsi+rax], 0
    je @@ri_len_done
    inc rax
    jmp @@ri_len
@@ri_len_done:
    mov rbx, rax        ; rbx = length
    cmp rbx, 2
    jl @@ri_trivial      ; 0 or 1 char — nothing to swap

    ; Two-pointer swap: left=0, right=len-1
    xor rcx, rcx        ; left
    mov rdx, rbx
    dec rdx             ; right = len-1

@@ri_swap:
    cmp rcx, rdx
    jge @@ri_done
    movzx eax, byte ptr [rsi+rcx]
    movzx r8d, byte ptr [rsi+rdx]
    mov byte ptr [rsi+rcx], r8b
    mov byte ptr [rsi+rdx], al
    inc rcx
    dec rdx
    jmp @@ri_swap

@@ri_done:
    lock inc qword ptr [g_reversals_applied]

@@ri_trivial:
    mov rax, rbx
    jmp @@ri_ret

@@ri_null:
    xor eax, eax

@@ri_ret:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
reverse_string_inplace endp

; ---------------------------------------------------------
; apply_text_reversal
; Strategy dispatch — main interface for the reversal engine
; RCX = src  (null-terminated input)
; RDX = dst  (output buffer, caller-allocated)
; R8D = strategy (1 = REVERSAL_FULL, 2 = REVERSAL_WORDS)
; Returns RAX = length written, 0 on error/null
; ---------------------------------------------------------
apply_text_reversal proc
    push rbx
    push rsi
    push rdi
    sub rsp, 28h

    ; Null guard
    test rcx, rcx
    jz @@at_null
    test rdx, rdx
    jz @@at_null

    mov rsi, rcx        ; preserve src
    mov rdi, rdx        ; preserve dst
    mov ebx, r8d        ; preserve strategy

    cmp ebx, REVERSAL_FULL
    je @@at_full
    cmp ebx, REVERSAL_WORDS
    je @@at_words

    ; Unknown strategy — default to full reversal
    jmp @@at_full

@@at_full:
    mov rcx, rsi
    mov rdx, rdi
    call reverse_string
    jmp @@at_ret

@@at_words:
    mov rcx, rsi
    mov rdx, rdi
    call reverse_words
    jmp @@at_ret

@@at_null:
    xor eax, eax

@@at_ret:
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    ret
apply_text_reversal endp

; ---------------------------------------------------------
; EXPORTS for linker
; ---------------------------------------------------------
end