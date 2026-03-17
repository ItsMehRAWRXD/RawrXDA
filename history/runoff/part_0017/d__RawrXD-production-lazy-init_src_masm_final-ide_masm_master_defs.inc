; ============================================================================
; MASM_MASTER_DEFS.INC
; Centralized Windows API declarations and constants for all MASM files
; Include at the top of every MASM source file that uses Windows APIs
; ============================================================================

; ============================================================================
; WINDOWS API EXTERNAL DECLARATIONS
; ============================================================================

; Memory and Heap APIs
extern HeapAlloc:proc
extern HeapFree:proc
extern GetProcessHeap:proc

; Thread and Synchronization APIs
extern CreateMutexA:proc
extern ReleaseMutex:proc
extern WaitForSingleObject:proc
extern CreateThreadEx:proc
extern CreateEventA:proc
extern SetEvent:proc
extern ResetEvent:proc
extern CloseHandle:proc
extern CreatePipeEx:proc
extern CreatePipe:proc
extern SetHandleInformation:proc

; Process APIs
extern CreateProcessA:proc
extern TerminateProcess:proc
extern WriteFile:proc
extern ReadFile:proc
extern PeekNamedPipe:proc
extern GetStdHandle:proc
extern SetStdHandle:proc
extern ExitProcess:proc
extern Sleep:proc
extern GetCurrentDirectoryA:proc

; Window APIs
extern CreateWindowExA:proc
extern DefWindowProcA:proc
extern GetModuleHandleA:proc
extern RegisterClassExA:proc
extern LoadCursorA:proc
extern ShowWindow:proc
extern UpdateWindow:proc
extern BeginPaint:proc
extern EndPaint:proc
extern SetTextColor:proc
extern SetBkColor:proc
extern TextOutA:proc
extern GetDC:proc
extern ReleaseDC:proc
extern InvalidateRect:proc
extern CreateMenu:proc
extern CreatePopupMenu:proc
extern AppendMenuA:proc
extern SetMenu:proc
extern SendMessageA:proc

; File Dialog APIs
extern GetOpenFileNameA:proc
extern GetSaveFileNameA:proc

; String APIs
extern wsprintfA:proc
extern GetTickCount:proc

; ============================================================================
; WINDOWS API CONSTANTS
; ============================================================================

; Wait constants
INFINITE equ 0FFFFFFFFh
WAIT_OBJECT_0 equ 0
WAIT_TIMEOUT equ 258

; Handle constants
INVALID_HANDLE_VALUE equ -1
STD_INPUT_HANDLE equ -11
STD_OUTPUT_HANDLE equ -12
STD_ERROR_HANDLE equ -13

; Window constants (some pre-defined in windows.inc)
WM_PAINT equ 15
WM_CREATE equ 1
WM_DESTROY equ 2
WS_OVERLAPPEDWINDOW equ 00CF0000h
; CW_USEDEFAULT already defined in windows.inc - do not redefine

; Heap constants
HEAP_ZERO_MEMORY equ 8

; Pipe constants
PIPE_ACCESS_INBOUND equ 1
PIPE_ACCESS_OUTBOUND equ 2
FILE_FLAG_OVERLAPPED equ 40000000h
HANDLE_FLAG_INHERIT equ 1

; Process constants
CREATE_NEW_CONSOLE equ 10h
STARTF_USESTDHANDLES equ 100h

; Cursor constants
IDC_ARROW equ 32512

; Text constants
WM_SETTEXT equ 12
WM_GETTEXT equ 13

; ============================================================================
; STRUCTURE DEFINITIONS
; ============================================================================

; Generic structure size constants
SECURITY_ATTRIBUTES_SIZE equ 12
STARTUPINFOA_SIZE equ 68
PROCESS_INFORMATION_SIZE equ 16
WNDCLASSEXA_SIZE equ 80
RECT_SIZE equ 16
PAINTSTRUCT_SIZE equ 56
OPENFILENAME_SIZE equ 152

; SECURITY_ATTRIBUTES structure
SECURITY_ATTRIBUTES STRUCT
    nLength DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle DWORD ?
SECURITY_ATTRIBUTES ENDS

; RECT structure
RECT STRUCT
    left DWORD ?
    top DWORD ?
    right DWORD ?
    bottom DWORD ?
RECT ENDS

; PAINTSTRUCT structure
PAINTSTRUCT STRUCT
    hdc QWORD ?
    fErase DWORD ?
    rcPaint RECT <>
    fRestore DWORD ?
    fIncUpdate DWORD ?
    rgbReserved BYTE 32 DUP(?)
PAINTSTRUCT ENDS

; STARTUPINFOA structure (simplified, 68 bytes)
STARTUPINFOA STRUCT
    cb DWORD ?                  ; offset 0
    lpReserved QWORD ?         ; offset 4
    lpDesktop QWORD ?          ; offset 12
    lpTitle QWORD ?            ; offset 20
    dwX DWORD ?                ; offset 28
    dwY DWORD ?                ; offset 32
    dwXSize DWORD ?            ; offset 36
    dwYSize DWORD ?            ; offset 40
    dwXCountChars DWORD ?       ; offset 44
    dwYCountChars DWORD ?       ; offset 48
    dwFillAttribute DWORD ?     ; offset 52
    dwFlags DWORD ?             ; offset 56
    wShowWindow WORD ?          ; offset 60
    cbReserved2 WORD ?          ; offset 62
    lpReserved2 QWORD ?        ; offset 64
STARTUPINFOA ENDS

; PROCESS_INFORMATION structure
PROCESS_INFORMATION STRUCT
    hProcess QWORD ?
    hThread QWORD ?
    dwProcessId DWORD ?
    dwThreadId DWORD ?
PROCESS_INFORMATION ENDS

; WNDCLASSEXA structure (simplified)
WNDCLASSEXA STRUCT
    cbSize DWORD ?
    style DWORD ?
    lpfnWndProc QWORD ?
    cbClsExtra DWORD ?
    cbWndExtra DWORD ?
    hInstance QWORD ?
    hIcon QWORD ?
    hCursor QWORD ?
    hbrBackground QWORD ?
    lpszMenuName QWORD ?
    lpszClassName QWORD ?
    hIconSm QWORD ?
WNDCLASSEXA ENDS

; ============================================================================
; COMMON GLOBAL DATA STRUCTURES (for use in user code)
; ============================================================================

; These should be defined in .data? section of user code
; Global handle variables
;   hStdInput QWORD ?
;   hStdOutput QWORD ?
;   hStdError QWORD ?
;   hStdInRead QWORD ?
;   hStdInWrite QWORD ?
;   hStdOutRead QWORD ?
;   hStdOutWrite QWORD ?
;   hStdErrRead QWORD ?
;   hStdErrWrite QWORD ?
;
; Global working variables
;   bytesRead QWORD ?
;   bytesWritten QWORD ?
;   bytesAvailable QWORD ?
;   pi PROCESS_INFORMATION <>
;   si STARTUPINFOA <>

; ============================================================================
; COMPONENT INCLUDES
; ============================================================================

; Phase A: Foundation Layer
; Settings Manager component
include settings_manager.inc

; Terminal Pool component
include terminal_pool.inc

; Hotpatch System component
include hotpatch_system.inc

; Phase B: Core IDE Layer
; MainWindow Architecture component
include mainwindow_masm.inc

; File Browser component
include file_browser.inc

; Phase C: Backend Infrastructure
; LSP Client component
include lsp_client.inc

; Inference Manager component
include inference_manager.inc

; Agent Coordinator component
include agent_coordinator.inc

; Server Layer component
include server_layer.inc

; Phase D: Advanced Infrastructure
; Caching Layer component
include caching_layer.inc

; Stream Processor component
include stream_processor.inc

; Distributed Executor component
include distributed_executor.inc

; Advanced Auth component
include advanced_auth.inc





