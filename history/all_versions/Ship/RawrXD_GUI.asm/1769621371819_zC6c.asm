; RawrXD_GUI.asm - Win32 Titan IDE (RichEdit + Ring Consumer)
; Links to: Titan_MetaInit (bootstrap) + Streaming API
OPTION CASEMAP:NONE
OPTION WIN64:3

 includelib kernel32.lib
 includelib user32.lib
 includelib gdi32.lib
 includelib comctl32.lib

 EXTERN Titan_MetaInit : PROC
 EXTERN Titan_Initialize : PROC
 EXTERN Titan_CreateContext : PROC
 EXTERN Titan_LoadModel_GGUF : PROC
 EXTERN Titan_BeginStreamingInference : PROC
 EXTERN Titan_ConsumeToken : PROC

 EXTERN Arena_Alloc : PROC        ; If needed for UI buffers

.CONST
 ID_EDIT_MAIN    EQU 1001
 ID_RICH_OUT     EQU 1002
 ID_BTN_RUN      EQU 1003
 WM_TITAN_TOKEN  EQU WM_USER + 100
 TIMER_POLL      EQU 1
 POLL_INTERVAL   EQU 16           ; 60Hz

.DATA?
 g_hInstance     QWORD ?
 g_hWnd          QWORD ?
 g_hEdit         QWORD ?
 g_hRich         QWORD ?
 g_hContext      QWORD ?
 g_hThread       QWORD ?
 tokenBuf        BYTE 4096 dup(?)

.CODE
WinMain PROC
    sub rsp, 88h
    
    call GetModuleHandleA, 0
    mov g_hInstance, rax
    
    ; Init Common Controls
    mov ics.dwSize, sizeof INITCOMMONCONTROLSEX
    mov ics.dwICC, 0x0000FFFF
    call InitCommonControlsEx, OFFSET ics
    
    ; Register class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.lpfnWndProc, OFFSET WndProc
    mov wc.hInstance, rax
    mov wc.lpszClassName, OFFSET clsName
    mov wc.hbrBackground, COLOR_WINDOW+1
    call RegisterClassExA, OFFSET wc
    
    ; Create window
    call CreateWindowExA, WS_EX_CLIENTEDGE, OFFSET clsName, \
         OFFSET wndTitle, WS_OVERLAPPEDWINDOW or WS_VISIBLE or WS_MAXIMIZE, \
         CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, 0, 0, g_hInstance, 0
    mov g_hWnd, rax
    
    ; Init Titan (background)
    call Titan_MetaInit
    
    ; Create polling timer (16ms)
    call SetTimer, rax, TIMER_POLL, POLL_INTERVAL, 0
    
    ; Msg loop
@@msg:
    call GetMessageA, OFFSET msg, 0, 0, 0
    test eax, eax
    jz @@done
    call TranslateMessage, OFFSET msg
    call DispatchMessageA, OFFSET msg
    jmp @@msg
@@done:
    xor eax, eax
    add rsp, 88h
    ret
WinMain ENDP

WndProc PROC hWnd:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    mov hwnd_, rcx
    
    cmp edx, WM_CREATE
    je @@create
    cmp edx, WM_SIZE
    je @@size
    cmp edx, WM_COMMAND
    je @@command
    cmp edx, WM_TIMER
    je @@timer
    cmp edx, WM_TITAN_TOKEN
    je @@token
    cmp edx, WM_DESTROY
    je @@destroy
    
    call DefWindowProcA
    ret

@@create:
    ; RichEdit for output
    call LoadLibraryA, OFFSET richDll
    call CreateWindowExA, WS_EX_CLIENTEDGE, OFFSET richCls, 0, \
         WS_CHILD or WS_VISIBLE or WS_VSCROLL or ES_MULTILINE or ES_READONLY or ES_AUTOVSCROLL, \
         0, 400, 800, 300, hwnd_, ID_RICH_OUT, g_hInstance, 0
    mov g_hRich, rax
    call SendMessageA, rax, EM_SETBKGNDCOLOR, 0, 0x1E1E1E   ; Dark bg
    
    ; Edit for input (top)
    call CreateWindowExA, WS_EX_CLIENTEDGE, OFFSET richCls, 0, \
         WS_CHILD or WS_VISIBLE or WS_VSCROLL or ES_MULTILINE, \
         0, 0, 800, 380, hwnd_, ID_EDIT_MAIN, g_hInstance, 0
    mov g_hEdit, rax
    
    ; Button
    call CreateWindowExA, 0, OFFSET "BUTTON", OFFSET "Run Inference", \
         WS_CHILD or WS_VISIBLE or BS_DEFPUSHBUTTON, \
         700, 360, 100, 30, hwnd_, ID_BTN_RUN, g_hInstance, 0
    
    xor eax, eax
    ret

@@timer:
    ; Poll for tokens (non-blocking)
    sub rsp, 40
    lea rcx, [tokenBuf]
    mov edx, 4096
    call Titan_ConsumeToken
    add rsp, 40
    
    test rax, rax
    jz @@no_token
    
    ; Post to self to handle in main thread (RichEdit isn't thread-safe)
    call PostMessageA, hwnd_, WM_TITAN_TOKEN, rax, OFFSET tokenBuf
    
@@no_token:
    xor eax, eax
    ret

@@token:
    ; lParam = ptr, wParam = len
    mov rcx, g_hRich
    call SendMessageA, EM_SETSEL, -1, -1      ; End of text
    call SendMessageA, EM_REPLACESEL, 0, r9   ; Append
    call SendMessageA, EM_SCROLLCARET, 0, 0   ; Auto-scroll
    xor eax, eax
    ret

@@command:
    cmp r8w, ID_BTN_RUN
    jne @@done_cmd
    
    ; Get text
    call SendMessageA, g_hEdit, WM_GETTEXTLENGTH, 0, 0
    inc rax
    push rax
    call HeapAlloc, GetProcessHeap(), 0, rax
    mov rbx, rax
    call SendMessageA, g_hEdit, WM_GETTEXT, [rsp], rax
    
    ; Launch inference thread
    call CreateThread, 0, 0, OFFSET InferenceThread, rbx, 0, 0
    
@@done_cmd:
    xor eax, eax
    ret

@@size:
    ; Resize child windows
    call GetClientRect, hwnd_, OFFSET rc
    mov eax, rc.bottom
    sub eax, 100
    call SetWindowPos, g_hEdit, 0, 0, 0, rc.right, eax, SWP_NOZORDER
    mov eax, rc.bottom
    sub eax, 100
    call SetWindowPos, g_hRich, 0, 0, eax, rc.right, 100, SWP_NOZORDER
    xor eax, eax
    ret

@@destroy:
    call PostQuitMessage, 0
    xor eax, eax
    ret
    
@@default:
    call DefWindowProcA
    ret
WndProc ENDP

InferenceThread PROC lpPrompt:QWORD
    sub rsp, 40
    call Titan_CreateContext
    mov g_hContext, rax
    
    mov rcx, lpPrompt               ; Model path or actual prompt logic
    mov rdx, rax                    ; Context
    call Titan_BeginStreamingInference
    
    call HeapFree, GetProcessHeap(), 0, lpPrompt
    add rsp, 40
    ret
InferenceThread ENDP

.DATA
 clsName     BYTE "RawrXD_Titan_GUI",0
 wndTitle    BYTE "RawrXD Titan IDE [MetaReverse]",0
 richDll     BYTE "Riched20.dll",0
 richCls     BYTE "RichEdit20W",0
 ics         INITCOMMONCONTROLSEX <>
 wc          WNDCLASSEX <>
 msg         MSG <>
 rc          RECT <>

.DATA?
 hwnd_       QWORD ?
 hFont       QWORD ?

; Protos
GetModuleHandleA PROTO :QWORD
InitCommonControlsEx PROTO :QWORD
RegisterClassExA PROTO :QWORD
CreateWindowExA PROTO :VARARG
SetTimer PROTO :QWORD, :QWORD, :QWORD, :QWORD
GetMessageA PROTO :QWORD, :QWORD, :QWORD, :QWORD
TranslateMessage PROTO :QWORD
DispatchMessageA PROTO :QWORD
PostQuitMessage PROTO :QWORD
DefWindowProcA PROTO :VARARG
PostMessageA PROTO :QWORD, :QWORD, :QWORD, :QWORD
SendMessageA PROTO :QWORD, :QWORD, :QWORD, :QWORD
LoadLibraryA PROTO :QWORD
GetClientRect PROTO :QWORD, :QWORD
SetWindowPos PROTO :QWORD, :QWORD, :QWORD, :QWORD, :QWORD, :QWORD, :QWORD
HeapAlloc PROTO :QWORD, :QWORD, :QWORD
HeapFree PROTO :QWORD, :QWORD, :QWORD
GetProcessHeap PROTO
CreateThread PROTO :QWORD, :QWORD, :QWORD, :QWORD, :QWORD, :QWORD

END
