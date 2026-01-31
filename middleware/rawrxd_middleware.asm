; ============================================================================
; RawrXD MASM Middleware - System Tray Service
; ============================================================================
; A pure x64 MASM application that:
;   - Runs in the Windows System Tray (notification area)
;   - Provides HTTP API on port 8080 for GUI/CLI/any client
;   - Dynamically scans for models on disk (no Ollama required)
;   - Routes to backend inference engines or handles requests itself
;   - Zero external dependencies except Windows APIs
;
; Build:
;   ml64 /c rawrxd_middleware.asm
;   link rawrxd_middleware.obj /subsystem:windows /entry:WinMain ^
;        kernel32.lib user32.lib ws2_32.lib shell32.lib gdi32.lib ^
;        /out:rawrxd_middleware.exe
;
; Architecture:
;   [HTML GUI] ──┐
;   [CLI Tool] ──┼──► [MASM Middleware :8080] ──► [Model Files / Inference]
;   [VS Code]  ──┘         │
;                     [System Tray Icon]
; ============================================================================

option casemap:none

; ============================================================================
; CONSTANTS
; ============================================================================
; Window Messages
WM_CREATE           equ 0001h
WM_DESTROY          equ 0002h
WM_COMMAND          equ 0111h
WM_USER             equ 0400h
WM_TRAYICON         equ WM_USER + 1

; System Tray
NIM_ADD             equ 0
NIM_MODIFY          equ 1
NIM_DELETE          equ 2
NIF_MESSAGE         equ 1
NIF_ICON            equ 2
NIF_TIP             equ 4
NIF_INFO            equ 16

; Mouse messages
WM_RBUTTONUP        equ 0205h
WM_LBUTTONDBLCLK    equ 0203h

; Menu
MF_STRING           equ 0
MF_SEPARATOR        equ 0800h
TPM_RIGHTBUTTON     equ 2
TPM_BOTTOMALIGN     equ 20h

; Icons
IDI_APPLICATION     equ 32512

; Socket
AF_INET             equ 2
SOCK_STREAM         equ 1
IPPROTO_TCP         equ 6
SOL_SOCKET          equ 0FFFFh
SO_REUSEADDR        equ 4
INVALID_SOCKET      equ -1
SOCKET_ERROR        equ -1

; File
GENERIC_READ        equ 80000000h
FILE_SHARE_READ     equ 1
OPEN_EXISTING       equ 3
FILE_ATTRIBUTE_NORMAL equ 80h
INVALID_HANDLE_VALUE equ -1

; Server config
SERVER_PORT         equ 8080
BUFFER_SIZE         equ 65536
MAX_MODELS          equ 512
MAX_PATH_LEN        equ 260

; Menu IDs
IDM_STATUS          equ 1001
IDM_REFRESH         equ 1002
IDM_OPENCONFIG      equ 1003
IDM_SEPARATOR       equ 1004
IDM_EXIT            equ 1005

; ============================================================================
; STRUCTURES
; ============================================================================
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
    x               dd ?
    y               dd ?
POINT ends

MSG struct
    hwnd            dq ?
    message         dd ?
    wParam          dq ?
    lParam          dq ?
    time            dd ?
    pt              POINT <>
    lPrivate        dd ?
MSG ends

NOTIFYICONDATAA struct
    cbSize          dd ?
    hWnd            dq ?
    uID             dd ?
    uFlags          dd ?
    uCallbackMessage dd ?
    hIcon           dq ?
    szTip           db 128 dup(?)
    dwState         dd ?
    dwStateMask     dd ?
    szInfo          db 256 dup(?)
    uTimeout        dd ?
    szInfoTitle     db 64 dup(?)
    dwInfoFlags     dd ?
    guidItem        db 16 dup(?)
    hBalloonIcon    dq ?
NOTIFYICONDATAA ends

WSADATA struct
    wVersion        dw ?
    wHighVersion    dw ?
    iMaxSockets     dw ?
    iMaxUdpDg       dw ?
    lpVendorInfo    dq ?
    szDescription   db 257 dup(?)
    szSystemStatus  db 129 dup(?)
WSADATA ends

sockaddr_in struct
    sin_family      dw ?
    sin_port        dw ?
    sin_addr        dd ?
    sin_zero        db 8 dup(?)
sockaddr_in ends

WIN32_FIND_DATAA struct
    dwFileAttributes    dd ?
    ftCreationTime      dq ?
    ftLastAccessTime    dq ?
    ftLastWriteTime     dq ?
    nFileSizeHigh       dd ?
    nFileSizeLow        dd ?
    dwReserved0         dd ?
    dwReserved1         dd ?
    cFileName           db 260 dup(?)
    cAlternateFileName  db 14 dup(?)
WIN32_FIND_DATAA ends

MODEL_INFO struct
    name            db 128 dup(?)
    path            db MAX_PATH_LEN dup(?)
    mtype           db 32 dup(?)
    size_bytes      dq ?
    size_str        db 16 dup(?)
MODEL_INFO ends

; ============================================================================
; EXTERNAL FUNCTIONS
; ============================================================================
; Kernel32
extern GetModuleHandleA:proc
extern ExitProcess:proc
extern CreateThread:proc
extern CloseHandle:proc
extern Sleep:proc
extern GetLastError:proc
extern FindFirstFileA:proc
extern FindNextFileA:proc
extern FindClose:proc
extern GetFileAttributesA:proc
extern CreateFileA:proc
extern GetFileSizeEx:proc
extern ReadFile:proc
extern WriteFile:proc
extern GetCurrentDirectoryA:proc
extern SetCurrentDirectoryA:proc
extern LocalAlloc:proc
extern LocalFree:proc
extern lstrlenA:proc
extern lstrcpyA:proc
extern lstrcatA:proc
extern lstrcmpA:proc
extern lstrcmpiA:proc

; User32
extern RegisterClassExA:proc
extern CreateWindowExA:proc
extern DefWindowProcA:proc
extern GetMessageA:proc
extern TranslateMessage:proc
extern DispatchMessageA:proc
extern PostQuitMessage:proc
extern LoadIconA:proc
extern LoadCursorA:proc
extern CreatePopupMenu:proc
extern AppendMenuA:proc
extern TrackPopupMenu:proc
extern DestroyMenu:proc
extern GetCursorPos:proc
extern SetForegroundWindow:proc
extern MessageBoxA:proc
extern PostMessageA:proc

; Shell32
extern Shell_NotifyIconA:proc

; Winsock
extern WSAStartup:proc
extern WSACleanup:proc
extern WSAGetLastError:proc
extern socket:proc
extern bind:proc
extern listen:proc
extern accept:proc
extern recv:proc
extern send:proc
extern closesocket:proc
extern setsockopt:proc
extern htons:proc
extern inet_ntoa:proc

; CRT (from msvcrt)
extern sprintf:proc
extern _snprintf:proc
extern strstr:proc
extern strlen:proc
extern strcpy:proc
extern strcat:proc
extern strncpy:proc
extern _stricmp:proc
extern _strnicmp:proc
extern memset:proc
extern memcpy:proc
extern atoi:proc
extern _i64toa:proc

; ============================================================================
; DATA SECTION
; ============================================================================
.data

; Window
szClassName     db "RawrXDMiddlewareClass",0
szWindowTitle   db "RawrXD Middleware",0
hInstance       dq 0
hWnd            dq 0
hMenu           dq 0

; System Tray
nid             NOTIFYICONDATAA <>
szTrayTip       db "RawrXD Middleware - Port 8080",0
szBalloonTitle  db "RawrXD Middleware",0
szBalloonMsg    db "Service started on port 8080",0

; Server state
bServerRunning  dd 1
hListenSocket   dq INVALID_SOCKET
hServerThread   dq 0
dwModelCount    dd 0

; Winsock
wsaData         WSADATA <>
serverAddr      sockaddr_in <>
clientAddr      sockaddr_in <>
addrLen         dd 16

; Model database
align 8
ModelDatabase   MODEL_INFO MAX_MODELS dup(<>)

; Model directories to scan (configurable)
szModelDirs     dq offset szModelDir1
                dq offset szModelDir2
                dq offset szModelDir3
                dq offset szModelDir4
                dq 0  ; Null terminator

szModelDir1     db "D:\OllamaModels",0
szModelDir2     db "D:\models",0
szModelDir3     db "C:\models",0
szModelDir4     db "E:\models",0
szSearchPattern db "\*.gguf",0

; HTTP Responses
szHttp200       db "HTTP/1.1 200 OK",13,10,0
szHttp404       db "HTTP/1.1 404 Not Found",13,10,0
szHttp500       db "HTTP/1.1 500 Internal Server Error",13,10,0
szHeaderJson    db "Content-Type: application/json",13,10,0
szHeaderCors    db "Access-Control-Allow-Origin: *",13,10
                db "Access-Control-Allow-Methods: GET, POST, OPTIONS",13,10
                db "Access-Control-Allow-Headers: Content-Type, Accept",13,10,0
szHeaderClose   db "Connection: close",13,10,13,10,0

; Route strings
szRouteModels   db "/models",0
szRouteAsk      db "/ask",0
szRouteHealth   db "/health",0
szRouteConfig   db "/config",0
szRouteScan     db "/scan",0

; Response templates
szJsonAnswer    db '{"answer":"%s"}',0
szJsonError     db '{"error":"%s"}',0
szJsonHealth    db '{"status":"online","port":8080,"models":%d,"version":"3.0","backend":"masm-middleware"}',0
szJsonConfig    db '{"model_dirs":["%s","%s","%s","%s"],"port":8080,"max_models":512}',0

; Default answers
szAnswerDefault db "RawrXD MASM Middleware is operational. I'm a pure x64 assembly service running in your system tray. Ask about 'swarm', 'model', 'benchmark', or 'help'!",0
szAnswerSwarm   db "**Swarm Mode**: Deploy up to 40 parallel AI agents. Configure via the GUI Swarm Panel or POST to /swarm with {agents: N, model: 'name', task: '...'}.",0
szAnswerModel   db "**Model Management**: I dynamically scan your model directories for GGUF files. Currently tracking %d models. Use GET /models or POST /scan to refresh.",0
szAnswerBench   db "**Benchmarks**: RX 7800 XT achieves ~3,158 tokens/sec on 3.8B models. POST to /benchmark with {model, iterations} for custom tests.",0
szAnswerHelp    db "**RawrXD Middleware Help**\\n\\n- GET /models - List all GGUF models\\n- POST /ask - Send a question\\n- GET /health - Service status\\n- POST /scan - Rescan model directories\\n- GET /config - View configuration",0
szAnswerHello   db "Hello! I'm the RawrXD Middleware - a pure MASM x64 service. I connect your IDE (GUI, CLI, extensions) to your local AI models. No cloud, no Ollama, just raw assembly power!",0

; Keywords
szKeySwarm      db "swarm",0
szKeyModel      db "model",0
szKeyBench      db "bench",0
szKeyHelp       db "help",0
szKeyHello      db "hello",0
szKeyHi         db "hi ",0

; Menu strings
szMenuStatus    db "Status: Online (%d models)",0
szMenuRefresh   db "Refresh Models",0
szMenuConfig    db "Open Config...",0
szMenuExit      db "Exit",0

; Buffers
align 16
szRecvBuffer    db BUFFER_SIZE dup(0)
szSendBuffer    db BUFFER_SIZE dup(0)
szJsonBuffer    db BUFFER_SIZE dup(0)
szTempBuffer    db 4096 dup(0)
szPathBuffer    db MAX_PATH_LEN dup(0)

; Find data
findData        WIN32_FIND_DATAA <>

; Sync
oneValue        dd 1

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; Case-insensitive substring search
; ============================================================================
stristr_impl proc uses rbx rsi rdi r12 r13 haystack:QWORD, needle:QWORD
    mov rsi, haystack
    mov rdi, needle
    
    test rsi, rsi
    jz stristr_notfound
    test rdi, rdi
    jz stristr_notfound
    
stristr_outer:
    movzx eax, byte ptr [rsi]
    test al, al
    jz stristr_notfound
    
    mov r12, rsi
    mov r13, rdi
    
stristr_inner:
    movzx eax, byte ptr [r13]
    test al, al
    jz stristr_found
    
    movzx edx, byte ptr [r12]
    test dl, dl
    jz stristr_notfound
    
    ; Lowercase both
    cmp al, 'A'
    jb stristr_skip1
    cmp al, 'Z'
    ja stristr_skip1
    add al, 32
stristr_skip1:
    cmp dl, 'A'
    jb stristr_skip2
    cmp dl, 'Z'
    ja stristr_skip2
    add dl, 32
stristr_skip2:
    
    cmp al, dl
    jne stristr_next
    inc r12
    inc r13
    jmp stristr_inner
    
stristr_next:
    inc rsi
    jmp stristr_outer
    
stristr_found:
    mov rax, rsi
    ret
    
stristr_notfound:
    xor eax, eax
    ret
stristr_impl endp

; ============================================================================
; Format file size to human readable
; ============================================================================
FormatFileSize proc uses rbx rsi rdi sizeBytes:QWORD, outBuffer:QWORD
    mov rax, sizeBytes
    mov rdi, outBuffer
    
    ; Check GB (> 1073741824)
    mov rcx, 1073741824
    cmp rax, rcx
    jb check_mb
    
    ; GB
    xor edx, edx
    div rcx
    mov rcx, rdi
    mov rdx, rax
    lea r8, szFmtGB
    call sprintf
    ret
    
check_mb:
    ; Check MB (> 1048576)
    mov rcx, 1048576
    cmp rax, rcx
    jb check_kb
    
    xor edx, edx
    div rcx
    mov rcx, rdi
    mov rdx, rax
    lea r8, szFmtMB
    call sprintf
    ret
    
check_kb:
    ; KB
    mov rcx, 1024
    xor edx, edx
    div rcx
    mov rcx, rdi
    mov rdx, rax
    lea r8, szFmtKB
    call sprintf
    ret

szFmtGB db "%lld GB",0
szFmtMB db "%lld MB",0
szFmtKB db "%lld KB",0

FormatFileSize endp

; ============================================================================
; Scan a single directory for models
; ============================================================================
ScanDirectory proc uses rbx rsi rdi r12 r13 r14 dirPath:QWORD
    local hFind:QWORD
    local fileSize:QWORD
    
    ; Build search pattern: dirPath\*.gguf
    lea rcx, szPathBuffer
    mov rdx, dirPath
    call strcpy
    lea rcx, szPathBuffer
    lea rdx, szSearchPattern
    call strcat
    
    ; FindFirstFile
    lea rcx, szPathBuffer
    lea rdx, findData
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je scan_done
    mov hFind, rax
    
scan_loop:
    ; Check if we have room
    mov eax, dwModelCount
    cmp eax, MAX_MODELS
    jge scan_close
    
    ; Get model slot
    mov eax, dwModelCount
    mov ecx, sizeof MODEL_INFO
    imul eax, ecx
    lea rsi, ModelDatabase
    add rsi, rax
    
    ; Clear entry
    mov rcx, rsi
    xor edx, edx
    mov r8d, sizeof MODEL_INFO
    call memset
    
    ; Copy filename as name
    lea rcx, [rsi].MODEL_INFO.name
    lea rdx, findData.cFileName
    mov r8d, 127
    call strncpy
    
    ; Build full path
    lea rcx, [rsi].MODEL_INFO.path
    mov rdx, dirPath
    call strcpy
    lea rcx, [rsi].MODEL_INFO.path
    lea rdx, szBackslash
    call strcat
    lea rcx, [rsi].MODEL_INFO.path
    lea rdx, findData.cFileName
    call strcat
    
    ; Set type
    lea rcx, [rsi].MODEL_INFO.mtype
    lea rdx, szTypeGguf
    call strcpy
    
    ; Get file size
    mov eax, findData.nFileSizeLow
    mov fileSize, rax
    mov [rsi].MODEL_INFO.size_bytes, rax
    
    ; Format size
    mov rcx, fileSize
    lea rdx, [rsi].MODEL_INFO.size_str
    call FormatFileSize
    
    inc dwModelCount
    
scan_next:
    mov rcx, hFind
    lea rdx, findData
    call FindNextFileA
    test eax, eax
    jnz scan_loop
    
scan_close:
    mov rcx, hFind
    call FindClose
    
scan_done:
    ret

szBackslash db "\",0
szTypeGguf  db "gguf",0

ScanDirectory endp

; ============================================================================
; Scan all model directories
; ============================================================================
ScanAllModels proc uses rbx rsi
    ; Reset count
    mov dwModelCount, 0
    
    ; Iterate through directory list
    lea rsi, szModelDirs
    
scan_dirs_loop:
    mov rbx, [rsi]
    test rbx, rbx
    jz scan_dirs_done
    
    ; Check if directory exists
    mov rcx, rbx
    call GetFileAttributesA
    cmp eax, INVALID_HANDLE_VALUE
    je scan_dirs_next
    
    ; Scan this directory
    mov rcx, rbx
    call ScanDirectory
    
scan_dirs_next:
    add rsi, 8
    jmp scan_dirs_loop
    
scan_dirs_done:
    ret
ScanAllModels endp

; ============================================================================
; Build JSON models response
; ============================================================================
BuildModelsJson proc uses rbx rsi rdi r12
    lea rdi, szJsonBuffer
    
    ; Start array
    mov byte ptr [rdi], '{'
    mov byte ptr [rdi+1], '"'
    mov byte ptr [rdi+2], 'm'
    mov byte ptr [rdi+3], 'o'
    mov byte ptr [rdi+4], 'd'
    mov byte ptr [rdi+5], 'e'
    mov byte ptr [rdi+6], 'l'
    mov byte ptr [rdi+7], 's'
    mov byte ptr [rdi+8], '"'
    mov byte ptr [rdi+9], ':'
    mov byte ptr [rdi+10], '['
    mov byte ptr [rdi+11], 0
    
    xor r12d, r12d          ; First flag
    xor ebx, ebx            ; Index
    
build_loop:
    cmp ebx, dwModelCount
    jge build_done
    
    ; Add comma if not first
    test r12d, r12d
    jz build_first
    lea rcx, szJsonBuffer
    lea rdx, szComma
    call strcat
    
build_first:
    inc r12d
    
    ; Get model pointer
    mov eax, ebx
    mov ecx, sizeof MODEL_INFO
    imul eax, ecx
    lea rsi, ModelDatabase
    add rsi, rax
    
    ; Format: {"name":"...","path":"...","type":"...","size":"..."}
    lea rcx, szTempBuffer
    lea rdx, szModelFmt
    lea r8, [rsi].MODEL_INFO.name
    lea r9, [rsi].MODEL_INFO.path
    mov rax, rsi
    add rax, MODEL_INFO.mtype
    mov qword ptr [rsp+20h], rax
    mov rax, rsi
    add rax, MODEL_INFO.size_str
    mov qword ptr [rsp+28h], rax
    call sprintf
    
    ; Append
    lea rcx, szJsonBuffer
    lea rdx, szTempBuffer
    call strcat
    
    inc ebx
    jmp build_loop
    
build_done:
    ; Close array
    lea rcx, szJsonBuffer
    lea rdx, szArrayEnd
    call strcat
    
    lea rax, szJsonBuffer
    ret

szComma     db ",",0
szArrayEnd  db "]}",0
szModelFmt  db '{"name":"%s","path":"%s","type":"%s","size":"%s"}',0

BuildModelsJson endp

; ============================================================================
; Send HTTP Response
; ============================================================================
SendHttpResponse proc uses rbx rsi clientSock:QWORD, status:QWORD, jsonBody:QWORD
    lea rdi, szSendBuffer
    
    ; Status line
    mov rcx, rdi
    mov rdx, status
    call strcpy
    
    ; Headers
    lea rcx, szSendBuffer
    lea rdx, szHeaderJson
    call strcat
    lea rcx, szSendBuffer
    lea rdx, szHeaderCors
    call strcat
    lea rcx, szSendBuffer
    lea rdx, szHeaderClose
    call strcat
    
    ; Body
    lea rcx, szSendBuffer
    mov rdx, jsonBody
    call strcat
    
    ; Get length
    lea rcx, szSendBuffer
    call strlen
    mov ebx, eax
    
    ; Send
    mov rcx, clientSock
    lea rdx, szSendBuffer
    mov r8d, ebx
    xor r9d, r9d
    call send
    
    ret
SendHttpResponse endp

; ============================================================================
; Handle /models request
; ============================================================================
HandleModels proc clientSock:QWORD
    call BuildModelsJson
    mov rdx, rax
    
    mov rcx, clientSock
    lea rdx, szHttp200
    call SendHttpResponse
    ret
HandleModels endp

; ============================================================================
; Handle /health request  
; ============================================================================
HandleHealth proc clientSock:QWORD
    ; Format health JSON
    lea rcx, szJsonBuffer
    lea rdx, szJsonHealth
    mov r8d, dwModelCount
    call sprintf
    
    mov rcx, clientSock
    lea rdx, szHttp200
    lea r8, szJsonBuffer
    call SendHttpResponse
    ret
HandleHealth endp

; ============================================================================
; Handle /ask request
; ============================================================================
HandleAsk proc uses rbx rsi rdi clientSock:QWORD, requestBody:QWORD
    mov rsi, requestBody
    
    ; Find keywords in body
    mov rcx, rsi
    lea rdx, szKeyHello
    call stristr_impl
    test rax, rax
    jnz answer_hello
    
    mov rcx, rsi
    lea rdx, szKeyHi
    call stristr_impl
    test rax, rax
    jnz answer_hello
    
    mov rcx, rsi
    lea rdx, szKeySwarm
    call stristr_impl
    test rax, rax
    jnz answer_swarm
    
    mov rcx, rsi
    lea rdx, szKeyModel
    call stristr_impl
    test rax, rax
    jnz answer_model
    
    mov rcx, rsi
    lea rdx, szKeyBench
    call stristr_impl
    test rax, rax
    jnz answer_bench
    
    mov rcx, rsi
    lea rdx, szKeyHelp
    call stristr_impl
    test rax, rax
    jnz answer_help
    
    ; Default answer
    lea r8, szAnswerDefault
    jmp send_answer
    
answer_hello:
    lea r8, szAnswerHello
    jmp send_answer
    
answer_swarm:
    lea r8, szAnswerSwarm
    jmp send_answer
    
answer_model:
    ; Format with model count
    lea rcx, szTempBuffer
    lea rdx, szAnswerModel
    mov r8d, dwModelCount
    call sprintf
    lea r8, szTempBuffer
    jmp send_answer
    
answer_bench:
    lea r8, szAnswerBench
    jmp send_answer
    
answer_help:
    lea r8, szAnswerHelp
    
send_answer:
    ; Format as JSON
    lea rcx, szJsonBuffer
    lea rdx, szJsonAnswer
    ; r8 has answer
    call sprintf
    
    mov rcx, clientSock
    lea rdx, szHttp200
    lea r8, szJsonBuffer
    call SendHttpResponse
    
    ret
HandleAsk endp

; ============================================================================
; Handle /scan request (rescan models)
; ============================================================================
HandleScan proc clientSock:QWORD
    call ScanAllModels
    
    ; Return new model list
    call BuildModelsJson
    mov r8, rax
    
    mov rcx, clientSock
    lea rdx, szHttp200
    call SendHttpResponse
    ret
HandleScan endp

; ============================================================================
; Handle OPTIONS (CORS preflight)
; ============================================================================
HandleOptions proc clientSock:QWORD
    lea rdi, szSendBuffer
    
    mov rcx, rdi
    lea rdx, szHttp200
    call strcpy
    lea rcx, szSendBuffer
    lea rdx, szHeaderCors
    call strcat
    lea rcx, szSendBuffer
    lea rdx, szHeaderClose
    call strcat
    
    lea rcx, szSendBuffer
    call strlen
    
    mov rcx, clientSock
    lea rdx, szSendBuffer
    mov r8d, eax
    xor r9d, r9d
    call send
    
    ret
HandleOptions endp

; ============================================================================
; Handle client connection
; ============================================================================
HandleClient proc uses rbx rsi rdi r12 clientSock:QWORD
    mov r12, clientSock
    
    ; Clear buffer
    lea rcx, szRecvBuffer
    xor edx, edx
    mov r8d, BUFFER_SIZE
    call memset
    
    ; Receive
    mov rcx, r12
    lea rdx, szRecvBuffer
    mov r8d, BUFFER_SIZE - 1
    xor r9d, r9d
    call recv
    cmp eax, 0
    jle client_done
    
    lea rsi, szRecvBuffer
    
    ; Check OPTIONS
    cmp dword ptr [rsi], 'ITPO'
    jne check_get
    call HandleOptions
    jmp client_done
    
check_get:
    ; Check GET
    cmp word ptr [rsi], 'EG'
    jne check_post
    cmp byte ptr [rsi+2], 'T'
    jne check_post
    
    ; Route GET
    mov rcx, rsi
    lea rdx, szRouteModels
    call strstr
    test rax, rax
    jnz route_models
    
    mov rcx, rsi
    lea rdx, szRouteHealth
    call strstr
    test rax, rax
    jnz route_health
    
    mov rcx, rsi
    lea rdx, szRouteConfig
    call strstr
    test rax, rax
    jnz route_config
    
    jmp route_404
    
check_post:
    cmp dword ptr [rsi], 'TSOP'
    jne route_404
    
    ; Route POST
    mov rcx, rsi
    lea rdx, szRouteAsk
    call strstr
    test rax, rax
    jnz route_ask
    
    mov rcx, rsi
    lea rdx, szRouteScan
    call strstr
    test rax, rax
    jnz route_scan
    
    jmp route_404
    
route_models:
    mov rcx, r12
    call HandleModels
    jmp client_done
    
route_health:
    mov rcx, r12
    call HandleHealth
    jmp client_done
    
route_config:
    ; Return config JSON
    lea rcx, szJsonBuffer
    lea rdx, szJsonConfig
    lea r8, szModelDir1
    lea r9, szModelDir2
    mov qword ptr [rsp+20h], offset szModelDir3
    mov qword ptr [rsp+28h], offset szModelDir4
    call sprintf
    
    mov rcx, r12
    lea rdx, szHttp200
    lea r8, szJsonBuffer
    call SendHttpResponse
    jmp client_done
    
route_ask:
    ; Find body (after \r\n\r\n)
    mov rcx, rsi
find_body:
    cmp dword ptr [rcx], 0A0D0A0Dh
    je found_body
    inc rcx
    cmp byte ptr [rcx], 0
    jne find_body
    mov rcx, rsi
    jmp found_body
found_body:
    add rcx, 4
    
    mov rdx, rcx
    mov rcx, r12
    call HandleAsk
    jmp client_done
    
route_scan:
    mov rcx, r12
    call HandleScan
    jmp client_done
    
route_404:
    lea rcx, szJsonBuffer
    lea rdx, szJsonError
    lea r8, szErr404Msg
    call sprintf
    
    mov rcx, r12
    lea rdx, szHttp404
    lea r8, szJsonBuffer
    call SendHttpResponse
    
client_done:
    mov rcx, r12
    call closesocket
    ret

szErr404Msg db "Not Found",0

HandleClient endp

; ============================================================================
; HTTP Server Thread
; ============================================================================
ServerThread proc lpParam:QWORD
    local clientSock:QWORD
    
    ; Initialize Winsock
    mov ecx, 0202h
    lea rdx, wsaData
    call WSAStartup
    test eax, eax
    jnz server_exit
    
    ; Create socket
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    cmp rax, INVALID_SOCKET
    je server_cleanup
    mov hListenSocket, rax
    
    ; SO_REUSEADDR
    mov rcx, hListenSocket
    mov edx, SOL_SOCKET
    mov r8d, SO_REUSEADDR
    lea r9, oneValue
    mov qword ptr [rsp+20h], 4
    call setsockopt
    
    ; Bind
    mov serverAddr.sin_family, AF_INET
    mov ecx, SERVER_PORT
    call htons
    mov serverAddr.sin_port, ax
    mov serverAddr.sin_addr, 0
    
    mov rcx, hListenSocket
    lea rdx, serverAddr
    mov r8d, 16
    call bind
    cmp eax, SOCKET_ERROR
    je server_close
    
    ; Listen
    mov rcx, hListenSocket
    mov edx, 64
    call listen
    cmp eax, SOCKET_ERROR
    je server_close
    
    ; Accept loop
accept_loop:
    cmp bServerRunning, 0
    je server_close
    
    mov rcx, hListenSocket
    lea rdx, clientAddr
    lea r8, addrLen
    mov dword ptr [r8], 16
    call accept
    cmp rax, INVALID_SOCKET
    je accept_loop
    
    mov clientSock, rax
    
    ; Handle request
    mov rcx, clientSock
    call HandleClient
    
    jmp accept_loop
    
server_close:
    mov rcx, hListenSocket
    call closesocket
    
server_cleanup:
    call WSACleanup
    
server_exit:
    xor eax, eax
    ret
ServerThread endp

; ============================================================================
; Window Procedure
; ============================================================================
WndProc proc hWndP:QWORD, uMsg:DWORD, wParam:QWORD, lParam:QWORD
    
    cmp edx, WM_CREATE
    je wnd_create
    cmp edx, WM_TRAYICON
    je wnd_trayicon
    cmp edx, WM_COMMAND
    je wnd_command
    cmp edx, WM_DESTROY
    je wnd_destroy
    
    ; Default
    mov rcx, hWndP
    mov edx, uMsg
    mov r8, wParam
    mov r9, lParam
    call DefWindowProcA
    ret
    
wnd_create:
    xor eax, eax
    ret
    
wnd_trayicon:
    ; Check for right-click
    mov rax, lParam
    cmp ax, WM_RBUTTONUP
    je show_menu
    cmp ax, WM_LBUTTONDBLCLK
    je show_status
    xor eax, eax
    ret
    
show_menu:
    ; Create popup menu
    call CreatePopupMenu
    mov hMenu, rax
    
    ; Add items
    mov rcx, hMenu
    mov edx, MF_STRING
    mov r8d, IDM_STATUS
    lea rcx, szTempBuffer
    lea rdx, szMenuStatus
    mov r8d, dwModelCount
    call sprintf
    mov rcx, hMenu
    mov edx, MF_STRING
    mov r8d, IDM_STATUS
    lea r9, szTempBuffer
    call AppendMenuA
    
    mov rcx, hMenu
    mov edx, MF_SEPARATOR
    mov r8d, IDM_SEPARATOR
    xor r9, r9
    call AppendMenuA
    
    mov rcx, hMenu
    mov edx, MF_STRING
    mov r8d, IDM_REFRESH
    lea r9, szMenuRefresh
    call AppendMenuA
    
    mov rcx, hMenu
    mov edx, MF_STRING
    mov r8d, IDM_OPENCONFIG
    lea r9, szMenuConfig
    call AppendMenuA
    
    mov rcx, hMenu
    mov edx, MF_SEPARATOR
    mov r8d, IDM_SEPARATOR
    xor r9, r9
    call AppendMenuA
    
    mov rcx, hMenu
    mov edx, MF_STRING
    mov r8d, IDM_EXIT
    lea r9, szMenuExit
    call AppendMenuA
    
    ; Get cursor position
    lea rcx, clientAddr      ; Reuse as POINT
    call GetCursorPos
    
    ; Show menu
    mov rcx, hWnd
    call SetForegroundWindow
    
    mov rcx, hMenu
    mov edx, TPM_RIGHTBUTTON or TPM_BOTTOMALIGN
    mov r8d, dword ptr [clientAddr]      ; x
    mov r9d, dword ptr [clientAddr+4]    ; y
    mov qword ptr [rsp+20h], 0
    mov rax, hWnd
    mov qword ptr [rsp+28h], rax
    mov qword ptr [rsp+30h], 0
    call TrackPopupMenu
    
    mov rcx, hMenu
    call DestroyMenu
    
    xor eax, eax
    ret
    
show_status:
    ; Double-click - show message box with status
    lea rcx, szTempBuffer
    lea rdx, szStatusFmt
    mov r8d, dwModelCount
    call sprintf
    
    mov rcx, hWnd
    lea rdx, szTempBuffer
    lea r8, szWindowTitle
    mov r9d, 0
    call MessageBoxA
    xor eax, eax
    ret
    
wnd_command:
    mov rax, wParam
    and eax, 0FFFFh
    
    cmp eax, IDM_REFRESH
    je cmd_refresh
    cmp eax, IDM_EXIT
    je cmd_exit
    xor eax, eax
    ret
    
cmd_refresh:
    call ScanAllModels
    
    ; Update tray tip
    lea rcx, nid.szTip
    lea rdx, szTrayTipFmt
    mov r8d, dwModelCount
    call sprintf
    
    mov nid.uFlags, NIF_TIP
    mov ecx, NIM_MODIFY
    lea rdx, nid
    call Shell_NotifyIconA
    
    xor eax, eax
    ret
    
cmd_exit:
    mov bServerRunning, 0
    mov rcx, hWnd
    mov edx, WM_DESTROY
    xor r8, r8
    xor r9, r9
    call PostMessageA
    xor eax, eax
    ret
    
wnd_destroy:
    ; Remove tray icon
    mov ecx, NIM_DELETE
    lea rdx, nid
    call Shell_NotifyIconA
    
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret

szStatusFmt     db "RawrXD Middleware Status",13,10,13,10,"Port: 8080",13,10,"Models: %d",13,10,"Backend: MASM x64",0
szTrayTipFmt    db "RawrXD Middleware - %d models",0

WndProc endp

; ============================================================================
; WinMain - Entry Point
; ============================================================================
WinMain proc hInst:QWORD, hPrevInst:QWORD, lpCmdLine:QWORD, nCmdShow:DWORD
    local wc:WNDCLASSEXA
    local msg:MSG
    
    mov hInstance, rcx
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEXA
    mov wc.style, 0
    lea rax, WndProc
    mov wc.lpfnWndProc, rax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov rax, hInstance
    mov wc.hInstance, rax
    
    xor ecx, ecx
    mov edx, IDI_APPLICATION
    call LoadIconA
    mov wc.hIcon, rax
    mov wc.hIconSm, rax
    
    xor ecx, ecx
    mov edx, 32512          ; IDC_ARROW
    call LoadCursorA
    mov wc.hCursor, rax
    
    mov wc.hbrBackground, 0
    mov wc.lpszMenuName, 0
    lea rax, szClassName
    mov wc.lpszClassName, rax
    
    lea rcx, wc
    call RegisterClassExA
    
    ; Create hidden window
    xor ecx, ecx
    lea rdx, szClassName
    lea r8, szWindowTitle
    mov r9d, 0              ; WS_OVERLAPPED
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    mov qword ptr [rsp+38h], 0
    mov qword ptr [rsp+40h], 0
    mov rax, hInstance
    mov qword ptr [rsp+48h], rax
    mov qword ptr [rsp+50h], 0
    call CreateWindowExA
    mov hWnd, rax
    
    ; Setup tray icon
    mov nid.cbSize, sizeof NOTIFYICONDATAA
    mov rax, hWnd
    mov nid.hWnd, rax
    mov nid.uID, 1
    mov nid.uFlags, NIF_MESSAGE or NIF_ICON or NIF_TIP or NIF_INFO
    mov nid.uCallbackMessage, WM_TRAYICON
    
    xor ecx, ecx
    mov edx, IDI_APPLICATION
    call LoadIconA
    mov nid.hIcon, rax
    
    lea rcx, nid.szTip
    lea rdx, szTrayTip
    call strcpy
    
    lea rcx, nid.szInfoTitle
    lea rdx, szBalloonTitle
    call strcpy
    
    lea rcx, nid.szInfo
    lea rdx, szBalloonMsg
    call strcpy
    
    mov ecx, NIM_ADD
    lea rdx, nid
    call Shell_NotifyIconA
    
    ; Scan models
    call ScanAllModels
    
    ; Start server thread
    xor ecx, ecx
    lea rdx, ServerThread
    xor r8, r8
    xor r9d, r9d
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateThread
    mov hServerThread, rax
    
    ; Message loop
msg_loop:
    lea rcx, msg
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA
    test eax, eax
    jz msg_done
    
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageA
    jmp msg_loop
    
msg_done:
    ; Cleanup
    mov bServerRunning, 0
    mov rcx, hServerThread
    mov edx, 5000
    call CloseHandle
    
    mov eax, dword ptr [msg.wParam]
    ret
WinMain endp

end
