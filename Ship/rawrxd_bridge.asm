; ============================================================================
; RawrXD MASM Backend v4.0 - AUTO-WIRING MIDDLE-END
; ============================================================================
; Build: ml64 rawrxd_bridge.asm /link /subsystem:windows /entry:WinMain
;        /defaultlib:kernel32.lib user32.lib shell32.lib ws2_32.lib 
;        wininet.lib shlwapi.lib /out:rawrxd_bridge.exe
; ============================================================================
; Features:
; - System tray integration (like Ollama)
; - Auto-detects Ollama (localhost:11434) and llama.cpp servers
; - Dynamic model discovery from multiple sources
; - 40-agent swarm management with process pooling
; - Auto-bridges frontend (8080) to backend (Ollama/llama.cpp)
; - Zero configuration - self-wiring
; ============================================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

option win64:3

; ============================================================================
; INCLUDES
; ============================================================================
include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc
include \masm64\include64\shell32.inc
include \masm64\include64\ws2_32.inc
include \masm64\include64\wininet.inc
include \masm64\include64\shlwapi.inc

includelib kernel32.lib
includelib user32.lib
includelib shell32.lib
includelib ws2_32.lib
includelib wininet.lib
includelib shlwapi.lib

; ============================================================================
; EQUATES
; ============================================================================
FRONTEND_PORT       equ     8080
OLLAMA_PORT         equ     11434
LLAMACPP_PORT       equ     8081
MAX_MODELS          equ     512
MAX_SWARM_AGENTS    equ     40
MAX_BACKENDS        equ     8
WM_TRAYICON         equ     WM_USER + 100
ID_TRAY_ICON        equ     1001
ID_MENU_EXIT        equ     2001
ID_MENU_STATUS      equ     2002
ID_MENU_MODELS      equ     2003
ID_MENU_SWARM       equ     2004
ID_TIMER_DISCOVER   equ     1
ID_TIMER_HEALTH     equ     2
DISCOVERY_INTERVAL  equ     30000       ; 30 seconds
HEALTH_INTERVAL     equ     5000        ; 5 seconds

; Backend types
BACKEND_NONE        equ     0
BACKEND_OLLAMA      equ     1
BACKEND_LLAMACPP    equ     2
BACKEND_RAWRXD      equ     3
BACKEND_CLOUD       equ     4

; Agent states
AGENT_IDLE          equ     0
AGENT_LOADING       equ     1
AGENT_INFERENCING   equ     2
AGENT_ERROR         equ     3

; ============================================================================
; STRUCTURES
; ============================================================================
BACKEND_INFO struct
    type            dd      ?           ; BACKEND_*
    active          dd      ?           ; Currently active
    host            db      64 dup(?)   ; Hostname/IP
    port            dd      ?           ; Port number
    models          dd      ?           ; Number of models
    latency         dd      ?           ; Last ping ms
    last_seen       dq      ?           ; Timestamp
BACKEND_INFO ends

MODEL_INFO struct
    name            db      128 dup(?)  ; Model name
    id              db     128 dup(?)  ; Backend-specific ID
    backend_idx     dd      ?           ; Index in backend table
    type            db      16 dup(?)   ; gguf/ollama/cloud
    size            db      16 dup(?)   ; Size string
    params          db      16 dup(?)   ; Parameter count
    quantization    db      16 dup(?)   ; Q4_K_M, etc
    family          db      32 dup(?)   ; llama, qwen, etc
    loaded          dd      ?           ; Currently in VRAM
    favorite        dd      ?           ; User favorite
MODEL_INFO ends

SWARM_AGENT struct
    id              dd      ?           ; Agent ID 1-40
    state           dd      ?           ; AGENT_*
    model_idx       dd      ?           ; Current model
    backend_idx     dd      ?           ; Which backend
    request_count   dd      ?           ; Total requests
    error_count     dd      ?           ; Failed requests
    thread_handle   dq      ?           ; Worker thread
    socket          dq      ?           ; Active connection
    last_request    dq      ?           ; Timestamp
SWARM_AGENT ends

BRIDGE_REQUEST struct
    client_socket   dq      ?           ; Frontend connection
    backend_type    dd      ?           ; Where to route
    model_idx       dd      ?           ; Selected model
    request_data    db      8192 dup(?) ; JSON payload
    response_data   db      65536 dup(?); Response buffer
    agent_id        dd      ?           ; Assigned agent
BRIDGE_REQUEST ends

; ============================================================================
; DATA SECTION
; ============================================================================
.data

; Window class
szClassName         db      "RawrXDBridgeClass",0
szWindowTitle       db      "RawrXD Backend Bridge",0
szTooltip           db      "RawrXD: Auto-wiring AI backend",0

; Menu strings
szMenuStatus        db      "Status: Active",0
szMenuModels        db      "Models (%d)",0
szMenuSwarm         db      "Swarm (%d/40)",0
szMenuExit          db      "Exit",0

; Backend detection URLs
szOllamaTags        db      "http://localhost:11434/api/tags",0
szLlamaCppModels    db      "http://localhost:8081/v1/models",0
szRawrxdHealth      db      "http://localhost:8082/health",0

; HTTP templates
szHttpGet           db      "GET %s HTTP/1.1",13,10
                    db      "Host: %s:%d",13,10
                    db      "Accept: application/json",13,10
                    db      "Connection: close",13,10,13,10,0

szHttpPost          db      "POST %s HTTP/1.1",13,10
                    db      "Host: %s:%d",13,10
                    db      "Content-Type: application/json",13,10
                    db      "Content-Length: %d",13,10
                    db      "Connection: close",13,10,13,10
                    db      "%s",0

; Frontend responses
szFrontendModels    db      '{"models":[',0
szFrontendModelFmt  db      '{"name":"%s","id":"%s","backend":"%s","type":"%s","size":"%s","loaded":%s}',0

; Ollama API paths
szOllamaGenerate    db      "/api/generate",0
szOllamaChat        db      "/api/chat",0
szOllamaEmbeddings  db      "/api/embeddings",0
szOllamaPs          db      "/api/ps",0

; Process names for auto-detection
szProcOllama        db      "ollama.exe",0
szProcLlamaCpp      db      "llama-server.exe",0
szProcKobold        db      "koboldcpp.exe",0

; Registry paths
szRegOllama         db      "SOFTWARE\Ollama",0
szRegInstallPath    db      "InstallPath",0

; Directories to scan
szDirOllamaModels   db      "C:\Users\%s\.ollama\models\manifests",0
szDirLlamaCpp       db      "D:\llama.cpp\models",0
szDirRawrxd         db      "D:\OllamaModels",0

; Window handles
hWndMain            dq      ?
hWndPopup           dq      ?
hMenuTray           dq      ?
hIcon               dq      ?
hMutex              dq      ?

; Winsock
wsaData             WSADATA <>
hListenSocket       dq      INVALID_SOCKET

; Data tables
align 8
BackendTable        BACKEND_INFO MAX_BACKENDS dup(<>)
dwBackendCount      dd      0
dwActiveBackend     dd      -1

align 8
ModelDatabase       MODEL_INFO MAX_MODELS dup(<>)
dwModelCount        dd      0

align 8
SwarmPool           SWARM_AGENT MAX_SWARM_AGENTS dup(<>)
dwActiveAgents      dd      0

; Buffers
szJsonBuffer        db      262144 dup(?)  ; 256KB JSON buffer
szHttpBuffer        db      65536 dup(?)   ; 64KB HTTP buffer
szTempPath          db      MAX_PATH dup(?)

; Synchronization
hDiscoveryEvent     dq      ?
hSwarmLock          dq      ?

; Running flag
bRunning            dd      1

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; ENTRY POINT
; ============================================================================
WinMain proc hInstance:HINSTANCE, hPrevInstance:HINSTANCE, 
             lpCmdLine:LPSTR, nCmdShow:INT
    local   wc:WNDCLASSEX
    local   msg:MSG
    
    sub     rsp, 68h
    
    ; Check single instance
    lea     rcx, szClassName
    call    CreateMutexA
    cmp     rax, 0
    je      @wm_exit
    mov     hMutex, rax
    
    call    WaitForSingleObject
    cmp     eax, WAIT_TIMEOUT
    je      @wm_already_running
    
    ; Initialize
    call    InitWinsock
    call    CreateMainWindow
    call    CreateTrayIcon
    call    InitDiscovery
    call    StartFrontendServer
    
    ; Main message loop
@msg_loop:
    xor     ecx, ecx
    xor     edx, edx
    xor     r8d, r8d
    lea     r9, msg
    call    GetMessageA
    test    eax, eax
    jz      @wm_done
    
    lea     rcx, msg
    call    TranslateMessage
    lea     rcx, msg
    call    DispatchMessageA
    
    jmp     @msg_loop
    
@wm_done:
    mov     bRunning, 0
    call    Cleanup
    
@wm_exit:
    xor     eax, eax
    add     rsp, 68h
    ret
    
@wm_already_running:
    ; Show existing instance
    lea     rcx, szClassName
    call    FindWindowA
    test    rax, rax
    jz      @wm_exit
    
    mov     rcx, rax
    call    ShowWindow
    mov     rcx, rax
    call    SetForegroundWindow
    jmp     @wm_exit

WinMain endp

; ============================================================================
; WINDOW PROCEDURE
; ============================================================================
WndProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    local   pt:POINT
    
    cmp     edx, WM_CREATE
    je      @wp_create
    cmp     edx, WM_DESTROY
    je      @wp_destroy
    cmp     edx, WM_COMMAND
    je      @wp_command
    cmp     edx, WM_TRAYICON
    je      @wp_tray
    cmp     edx, WM_TIMER
    je      @wp_timer
    
    jmp     @wp_default
    
@wp_create:
    mov     hWndMain, rcx
    mov     eax, 1
    ret
    
@wp_destroy:
    call    PostQuitMessage
    xor     eax, eax
    ret
    
@wp_command:
    mov     eax, r8d
    cmp     ax, ID_MENU_EXIT
    je      @wp_exit
    cmp     ax, ID_MENU_STATUS
    je      @wp_show_status
    cmp     ax, ID_MENU_MODELS
    je      @wp_show_models
    cmp     ax, ID_MENU_SWARM
    je      @wp_show_swarm
    jmp     @wp_default
    
@wp_exit:
    mov     bRunning, 0
    call    PostQuitMessage
    xor     eax, eax
    ret
    
@wp_show_status:
    call    ShowStatusWindow
    jmp     @wp_handled
    
@wp_show_models:
    call    ShowModelsWindow
    jmp     @wp_handled
    
@wp_show_swarm:
    call    ShowSwarmWindow
    jmp     @wp_handled
    
@wp_tray:
    cmp     r9d, WM_RBUTTONUP
    je      @wp_show_menu
    cmp     r9d, WM_LBUTTONDBLCLK
    je      @wp_show_status
    jmp     @wp_handled
    
@wp_show_menu:
    ; Get cursor position
    lea     rcx, pt
    call    GetCursorPos
    
    ; Create popup menu
    xor     ecx, ecx
    call    CreatePopupMenu
    mov     hMenuTray, rax
    
    ; Add items
    mov     rcx, hMenuTray
    mov     edx, MF_STRING
    mov     r8d, ID_MENU_STATUS
    lea     r9, szMenuStatus
    call    AppendMenuA
    
    mov     rcx, hMenuTray
    mov     edx, MF_STRING
    movzx   r8d, dwModelCount
    lea     r9, szTempPath
    lea     rcx, szMenuModels
    mov     rdx, r9
    call    sprintf
    mov     rcx, hMenuTray
    mov     edx, MF_STRING
    mov     r8d, ID_MENU_MODELS
    mov     r9, rax
    call    AppendMenuA
    
    mov     rcx, hMenuTray
    mov     edx, MF_SEPARATOR
    xor     r8d, r8d
    xor     r9d, r9d
    call    AppendMenuA
    
    mov     rcx, hMenuTray
    mov     edx, MF_STRING
    mov     r8d, ID_MENU_EXIT
    lea     r9, szMenuExit
    call    AppendMenuA
    
    ; Show menu
    mov     rcx, hWndMain
    mov     edx, TPM_RIGHTALIGN
    mov     r8d, pt.x
    mov     r9d, pt.y
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    TrackPopupMenu
    
    mov     rcx, hMenuTray
    call    DestroyMenu
    jmp     @wp_handled
    
@wp_timer:
    cmp     r8d, ID_TIMER_DISCOVER
    je      @wp_do_discovery
    cmp     r8d, ID_TIMER_HEALTH
    je      @wp_do_health
    jmp     @wp_handled
    
@wp_do_discovery:
    call    DiscoverBackends
    call    UpdateTrayTooltip
    jmp     @wp_handled
    
@wp_do_health:
    call    CheckBackendHealth
    jmp     @wp_handled
    
@wp_handled:
    xor     eax, eax
    ret
    
@wp_default:
    call    DefWindowProcA
    ret

WndProc endp

; ============================================================================
; INITIALIZATION
; ============================================================================
InitWinsock proc
    sub     rsp, 28h
    mov     ecx, 0202h
    lea     rdx, wsaData
    call    WSAStartup
    add     rsp, 28h
    ret
InitWinsock endp

CreateMainWindow proc
    local   wc:WNDCLASSEX
    
    sub     rsp, 88h
    
    ; Register window class
    mov     wc.cbSize, sizeof WNDCLASSEX
    mov     wc.style, CS_HREDRAW or CS_VREDRAW
    lea     rax, WndProc
    mov     wc.lpfnWndProc, rax
    mov     wc.cbClsExtra, 0
    mov     wc.cbWndExtra, 0
    xor     ecx, ecx
    call    GetModuleHandleA
    mov     wc.hInstance, rax
    mov     wc.hIcon, 0
    mov     wc.hCursor, 0
    mov     wc.hbrBackground, COLOR_WINDOW + 1
    mov     wc.lpszMenuName, 0
    lea     rax, szClassName
    mov     wc.lpszClassName, rax
    mov     wc.hIconSm, 0
    
    lea     rcx, wc
    call    RegisterClassExA
    
    ; Create hidden window
    xor     ecx, ecx
    lea     rdx, szClassName
    lea     r8, szWindowTitle
    mov     r9d, WS_OVERLAPPEDWINDOW
    mov     qword ptr [rsp+20h], CW_USEDEFAULT
    mov     qword ptr [rsp+28h], CW_USEDEFAULT
    mov     qword ptr [rsp+30h], 640
    mov     qword ptr [rsp+38h], 480
    mov     qword ptr [rsp+40h], 0
    mov     qword ptr [rsp+48h], 0
    call    GetModuleHandleA
    mov     qword ptr [rsp+50h], rax
    mov     qword ptr [rsp+58h], 0
    call    CreateWindowExA
    
    ; Hide window (tray only)
    mov     rcx, rax
    mov     edx, SW_HIDE
    call    ShowWindow
    
    add     rsp, 88h
    ret
CreateMainWindow endp

CreateTrayIcon proc
    local   nid:NOTIFYICONDATA
    
    sub     rsp, 28h
    
    ; Load icon
    xor     ecx, ecx
    call    GetModuleHandleA
    mov     rdx, IDI_APPLICATION
    call    LoadIconA
    mov     hIcon, rax
    
    ; Setup NOTIFYICONDATA
    mov     nid.cbSize, sizeof NOTIFYICONDATA
    mov     nid.hWnd, hWndMain
    mov     nid.uID, ID_TRAY_ICON
    mov     nid.uFlags, NIF_ICON or NIF_MESSAGE or NIF_TIP
    mov     nid.uCallbackMessage, WM_TRAYICON
    mov     nid.hIcon, hIcon
    lea     rsi, szTooltip
    lea     rdi, nid.szTip
    mov     ecx, 128
    rep movsb
    
    ; Add icon
    lea     rcx, nid
    call    Shell_NotifyIconA
    
    add     rsp, 28h
    ret
CreateTrayIcon endp

InitDiscovery proc
    sub     rsp, 28h
    
    ; Set timers
    mov     rcx, hWndMain
    mov     edx, ID_TIMER_DISCOVER
    mov     r8d, DISCOVERY_INTERVAL
    xor     r9d, r9d
    call    SetTimer
    
    mov     rcx, hWndMain
    mov     edx, ID_TIMER_HEALTH
    mov     r8d, HEALTH_INTERVAL
    xor     r9d, r9d
    call    SetTimer
    
    ; Create synchronization objects
    xor     ecx, ecx
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    CreateEventA
    mov     hDiscoveryEvent, rax
    
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, szClassName
    mov     r9d, MUTEX_ALL_ACCESS
    call    CreateMutexA
    mov     hSwarmLock, rax
    
    ; Initial discovery
    call    DiscoverBackends
    call    ScanLocalModels
    
    add     rsp, 28h
    ret
InitDiscovery endp

; ============================================================================
; BACKEND DISCOVERY
; ============================================================================
DiscoverBackends proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 38h
    
    xor     ebx, ebx            ; Backend index
    
    ; Check 1: Ollama on localhost:11434
    mov     ecx, ebx
    call    ProbeOllama
    test    eax, eax
    jz      @db_no_ollama
    
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.type, BACKEND_OLLAMA
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.active, 1
    lea     rdi, [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.host
    mov     dword ptr [rdi], "loca"
    mov     dword ptr [rdi+4], "lhos"
    mov     word ptr [rdi+8], "t"
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.port, 11434
    inc     ebx
    
    call    FetchOllamaModels
    
@db_no_ollama:
    
    ; Check 2: llama.cpp on localhost:8081
    mov     ecx, ebx
    call    ProbeLlamaCpp
    test    eax, eax
    jz      @db_no_llamacpp
    
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.type, BACKEND_LLAMACPP
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.active, 1
    lea     rdi, [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.host
    mov     dword ptr [rdi], "loca"
    mov     dword ptr [rdi+4], "lhos"
    mov     word ptr [rdi+8], "t"
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.port, 8081
    inc     ebx
    
    call    FetchLlamaCppModels
    
@db_no_llamacpp:
    
    ; Check 3: RawrXD native on localhost:8082
    mov     ecx, ebx
    call    ProbeRawrxd
    test    eax, eax
    jz      @db_no_rawrxd
    
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.type, BACKEND_RAWRXD
    mov     [BackendTable + ebx*sizeof BACKEND_INFO].BACKEND_INFO.active, 1
    inc     ebx
    
@db_no_rawrxd:
    
    mov     dwBackendCount, ebx
    
    ; Select best backend
    call    SelectBestBackend
    
    add     rsp, 38h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
DiscoverBackends endp

ProbeOllama proc idx:DWORD
    sub     rsp, 28h
    
    ; Try to connect to Ollama /api/tags
    lea     rcx, szOllamaTags
    call    HttpQuickGet
    test    rax, rax
    setnz   al
    
    add     rsp, 28h
    ret
ProbeOllama endp

ProbeLlamaCpp proc idx:DWORD
    sub     rsp, 28h
    
    lea     rcx, szLlamaCppModels
    call    HttpQuickGet
    test    rax, rax
    setnz   al
    
    add     rsp, 28h
    ret
ProbeLlamaCpp endp

ProbeRawrxd proc idx:DWORD
    sub     rsp, 28h
    
    lea     rcx, szRawrxdHealth
    call    HttpQuickGet
    test    rax, rax
    setnz   al
    
    add     rsp, 28h
    ret
ProbeRawrxd endp

FetchOllamaModels proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48h
    
    ; Get /api/tags from Ollama
    lea     rcx, szOllamaTags
    lea     rdx, szJsonBuffer
    mov     r8d, 262144
    call    HttpGet
    
    test    eax, eax
    jz      @fom_done
    
    ; Parse JSON response
    lea     rcx, szJsonBuffer
    call    ParseOllamaModelList
    
@fom_done:
    add     rsp, 48h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
FetchOllamaModels endp

FetchLlamaCppModels proc
    ; Similar to Ollama but different JSON format
    ret
FetchLlamaCppModels endp

ScanLocalModels proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 58h
    
    ; Scan D:\OllamaModels for .gguf files
    lea     rcx, szDirRawrxd
    lea     rdx, szTempPath
    call    ExpandEnvironmentStringsA
    
    lea     rcx, szTempPath
    lea     rdx, szSearchPattern
    call    strcat
    
    lea     rcx, szTempPath
    lea     rdx, szWin32FindData
    call    FindFirstFileA
    
    cmp     rax, INVALID_HANDLE_VALUE
    je      @slm_done
    
    mov     rbx, rax
    
@slm_loop:
    ; Check if .gguf
    lea     rcx, szWin32FindData.cFileName
    call    IsGgufFile
    test    eax, eax
    jz      @slm_next
    
    ; Add to database
    mov     eax, dwModelCount
    cmp     eax, MAX_MODELS
    jge     @slm_close
    
    imul    rdi, rax, sizeof MODEL_INFO
    lea     rdi, ModelDatabase[rdi]
    
    ; Copy name
    lea     rcx, [rdi].MODEL_INFO.name
    lea     rdx, szWin32FindData.cFileName
    call    strcpy
    
    ; Build path
    lea     rcx, [rdi].MODEL_INFO.path
    lea     rdx, szDirRawrxd
    call    strcpy
    lea     rcx, [rdi].MODEL_INFO.path
    mov     al, '\'
    call    strcat_char
    lea     rdx, szWin32FindData.cFileName
    call    strcat
    
    ; Set type
    lea     rcx, [rdi].MODEL_INFO.type
    lea     rdx, szTypeGguf
    call    strcpy
    
    ; Calculate size
    mov     rax, szWin32FindData.nFileSizeLow
    mov     rcx, szWin32FindData.nFileSizeHigh
    shl     rcx, 32
    or      rax, rcx
    lea     rdx, [rdi].MODEL_INFO.size
    call    FormatBytes
    
    inc     dwModelCount
    
@slm_next:
    mov     rcx, rbx
    lea     rdx, szWin32FindData
    call    FindNextFileA
    test    eax, eax
    jnz     @slm_loop
    
@slm_close:
    mov     rcx, rbx
    call    FindClose
    
@slm_done:
    add     rsp, 58h
    pop     rdi
    pop     rsi
    pop     rbx
    ret

szSearchPattern     db      "\*.gguf",0
szTypeGguf          db      "gguf",0

ScanLocalModels endp

; ============================================================================
; FRONTEND SERVER (Port 8080)
; ============================================================================
StartFrontendServer proc
    push    rbx
    sub     rsp, 38h
    
    ; Create listening socket
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    
    cmp     rax, INVALID_SOCKET
    je      @sfs_fail
    mov     rbx, rax
    mov     hListenSocket, rax
    
    ; Enable reuse
    mov     ecx, eax
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, szTempBuffer
    mov     dword ptr [r9], 1
    mov     qword ptr [rsp+20h], 4
    call    setsockopt
    
    ; Bind
    mov     word ptr [szTempBuffer], AF_INET
    mov     ax, FRONTEND_PORT
    xchg    al, ah
    mov     word ptr [szTempBuffer+2], ax
    mov     dword ptr [szTempBuffer+4], 0
    
    mov     rcx, rbx
    lea     rdx, szTempBuffer
    mov     r8d, 16
    call    bind
    
    cmp     rax, SOCKET_ERROR
    je      @sfs_fail_close
    
    ; Listen
    mov     rcx, rbx
    mov     edx, SOMAXCONN
    call    listen
    
    cmp     rax, SOCKET_ERROR
    je      @sfs_fail_close
    
    ; Start acceptor thread
    xor     ecx, ecx
    lea     rdx, FrontendAcceptThread
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0
    call    CreateThread
    
    mov     eax, 1
    jmp     @sfs_done
    
@sfs_fail_close:
    mov     rcx, rbx
    call    closesocket
    mov     hListenSocket, INVALID_SOCKET
    
@sfs_fail:
    xor     eax, eax
    
@sfs_done:
    add     rsp, 38h
    pop     rbx
    ret
StartFrontendServer endp

FrontendAcceptThread proc lpParam:LPVOID
    push    rbx
    push    rsi
    sub     rsp, 28h
    
@fat_loop:
    cmp     bRunning, 0
    je      @fat_done
    
    mov     rcx, hListenSocket
    lea     rdx, szTempBuffer
    lea     r8, szTempBuffer+16
    mov     dword ptr [r8], 16
    call    accept
    
    cmp     rax, INVALID_SOCKET
    je      @fat_loop
    
    mov     rbx, rax
    
    ; Create thread to handle client
    mov     ecx, sizeof BRIDGE_REQUEST
    call    malloc
    mov     rsi, rax
    
    mov     [BRIDGE_REQUEST ptr rsi].BRIDGE_REQUEST.client_socket, rbx
    
    xor     ecx, ecx
    lea     rdx, FrontendClientThread
    mov     r8, rsi
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0
    call    CreateThread
    
    jmp     @fat_loop
    
@fat_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
FrontendAcceptThread endp

FrontendClientThread proc lpParam:LPVOID
    push    rbx
    push    rsi
    sub     rsp, 48h
    
    mov     rsi, lpParam
    mov     rbx, [BRIDGE_REQUEST ptr rsi].BRIDGE_REQUEST.client_socket
    
    ; Receive HTTP request
    mov     rcx, rbx
    lea     rdx, szHttpBuffer
    mov     r8d, 65536
    xor     r9d, r9d
    call    recv
    
    cmp     rax, 0
    jle     @fct_cleanup
    
    mov     byte ptr [szHttpBuffer+rax], 0
    
    ; Parse request
    lea     rcx, szHttpBuffer
    lea     rdx, szTempBuffer
    call    ParseHttpRequest
    
    ; Route to appropriate handler
    mov     eax, [szTempBuffer]     ; method
    
    cmp     eax, METHOD_OPTIONS
    je      @fct_options
    
    cmp     eax, METHOD_GET
    jne     @fct_check_post
    
    ; Handle GET
    lea     rcx, [szTempBuffer+4]   ; path
    lea     rdx, szPathModels
    call    strcmp
    test    rax, rax
    jz      @fct_get_models
    
    lea     rcx, [szTempBuffer+4]
    lea     rdx, szPathHealth
    call    strcmp
    test    rax, rax
    jz      @fct_get_health
    
    jmp     @fct_404
    
@fct_check_post:
    cmp     eax, METHOD_POST
    jne     @fct_405
    
    lea     rcx, [szTempBuffer+4]
    lea     rdx, szPathAsk
    call    strcmp
    test    rax, rax
    jz      @fct_post_ask
    
    jmp     @fct_404
    
@fct_options:
    call    SendCorsOptions
    jmp     @fct_cleanup
    
@fct_get_models:
    call    SendFrontendModels
    jmp     @fct_cleanup
    
@fct_get_health:
    call    SendHealthStatus
    jmp     @fct_cleanup
    
@fct_post_ask:
    call    BridgeToBackend
    jmp     @fct_cleanup
    
@fct_404:
    call    Send404
    jmp     @fct_cleanup
    
@fct_405:
    call    Send405
    
@fct_cleanup:
    mov     rcx, rbx
    call    closesocket
    
    mov     rcx, rsi
    call    free
    
    add     rsp, 48h
    pop     rsi
    pop     rbx
    ret
FrontendClientThread endp

; ============================================================================
; BRIDGE LOGIC
; ============================================================================
BridgeToBackend proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 58h
    
    ; Extract question from JSON body
    lea     rcx, szHttpBuffer
    call    FindBodyStart
    mov     rsi, rax
    
    ; Parse JSON to get question and model
    lea     rcx, rsi
    lea     rdx, szTempBuffer
    call    ParseAskJson
    
    ; Select backend based on model
    lea     rcx, szTempBuffer+1024    ; model field
    call    SelectBackendForModel
    mov     ebx, eax
    
    ; Check if we need to spawn swarm agent
    cmp     dwActiveAgents, MAX_SWARM_AGENTS
    jge     @btb_no_swarm
    
    ; Assign to swarm agent
    call    AssignSwarmAgent
    mov     edi, eax
    
@btb_no_swarm:
    
    ; Route to selected backend
    cmp     ebx, BACKEND_OLLAMA
    je      @btb_ollama
    cmp     ebx, BACKEND_LLAMACPP
    je      @btb_llamacpp
    cmp     ebx, BACKEND_RAWRXD
    je      @btb_rawrxd
    
    ; Fallback: generate local response
    jmp     @btb_local
    
@btb_ollama:
    call    ForwardToOllama
    jmp     @btb_done
    
@btb_llamacpp:
    call    ForwardToLlamaCpp
    jmp     @btb_done
    
@btb_rawrxd:
    call    ForwardToRawrxd
    jmp     @btb_done
    
@btb_local:
    call    GenerateLocalResponse
    
@btb_done:
    add     rsp, 58h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
BridgeToBackend endp

ForwardToOllama proc
    sub     rsp, 38h
    
    ; Build Ollama API request
    lea     rcx, szJsonBuffer
    lea     rdx, szOllamaRequestFmt
    lea     r8, szTempBuffer          ; model
    lea     r9, szTempBuffer+2048     ; question
    call    sprintf
    
    ; POST to Ollama
    lea     rcx, szOllamaGenerate
    lea     rdx, szJsonBuffer
    call    strlen
    mov     r8d, eax
    call    HttpPostToOllama
    
    ; Parse response and forward to frontend
    call    ParseOllamaResponse
    call    SendFrontendAnswer
    
    add     rsp, 38h
    ret

szOllamaRequestFmt  db      '{"model":"%s","prompt":"%s","stream":false}',0
ForwardToOllama endp

; ============================================================================
; SWARM MANAGEMENT
; ============================================================================
AssignSwarmAgent proc
    push    rbx
    sub     rsp, 28h
    
    ; Find idle agent
    xor     ebx, ebx
    
@asa_loop:
    cmp     ebx, MAX_SWARM_AGENTS
    jge     @asa_none
    
    mov     eax, ebx
    imul    rax, sizeof SWARM_AGENT
    lea     rcx, SwarmPool[rax]
    
    cmp     [rcx].SWARM_AGENT.state, AGENT_IDLE
    je      @asa_found
    
    inc     ebx
    jmp     @asa_loop
    
@asa_found:
    ; Mark as loading
    mov     [rcx].SWARM_AGENT.state, AGENT_LOADING
    mov     eax, ebx
    inc     dwActiveAgents
    jmp     @asa_done
    
@asa_none:
    mov     eax, -1
    
@asa_done:
    add     rsp, 28h
    pop     rbx
    ret
AssignSwarmAgent endp

; ============================================================================
; RESPONSE SENDERS
; ============================================================================
SendFrontendModels proc
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48h
    
    lea     rdi, szJsonBuffer
    
    ; Start JSON
    lea     rsi, szFrontendModels
    call    strcpy
    mov     rdi, rax
    
    xor     ebx, ebx
    
@sfm_loop:
    cmp     ebx, dwModelCount
    jge     @sfm_close
    
    test    ebx, ebx
    jz      @sfm_first
    mov     byte ptr [rdi], ','
    inc     rdi
    
@sfm_first:
    ; Format model entry
    push    rdi
    mov     eax, ebx
    imul    rax, sizeof MODEL_INFO
    lea     rsi, ModelDatabase[rax]
    
    lea     rdi, szTempBuffer
    lea     rcx, szFrontendModelFmt
    lea     rdx, [rsi].MODEL_INFO.name
    lea     r8, [rsi].MODEL_INFO.id
    lea     r9, [rsi].MODEL_INFO.backend_idx
    
    ; Get backend name
    mov     eax, [rsi].MODEL_INFO.backend_idx
    imul    rax, sizeof BACKEND_INFO
    lea     rax, BackendTable[rax]
    
    push    qword ptr [rsi].MODEL_INFO.loaded
    push    qword ptr [rsi].MODEL_INFO.size
    push    qword ptr [rsi].MODEL_INFO.type
    push    rax
    call    sprintf
    add     rsp, 32
    
    pop     rdi
    
    ; Append
    lea     rcx, szTempBuffer
    call    strlen
    mov     r8d, eax
    lea     rsi, szTempBuffer
    rep movsb
    
    inc     ebx
    jmp     @sfm_loop
    
@sfm_close:
    ; Close JSON array
    lea     rsi, szJsonModelsEnd
    call    strcpy
    
    ; Send HTTP response
    call    SendJsonResponse
    
    add     rsp, 48h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SendFrontendModels endp

SendCorsOptions proc
    sub     rsp, 28h
    
    lea     rcx, szHttpBuffer
    
    lea     rdx, szHttp200
    call    strcpy
    
    lea     rdx, szHeaderCors
    call    strcat
    
    lea     rdx, szHeaderClose
    call    strcat
    
    mov     rdx, rax
    call    SendRawResponse
    
    add     rsp, 28h
    ret
SendCorsOptions endp

; ============================================================================
; HTTP UTILITIES
; ============================================================================
HttpQuickGet proc lpUrl:QWORD
    sub     rsp, 38h
    
    ; Use WinInet for simple GET
    xor     ecx, ecx
    call    InternetOpenA
    test    rax, rax
    jz      @hqg_fail
    
    mov     rbx, rax
    
    mov     rcx, rax
    mov     rdx, lpUrl
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    InternetOpenUrlA
    
    test    rax, rax
    jz      @hqg_close
    
    ; Success - close handle and return true
    mov     rcx, rax
    call    InternetCloseHandle
    
    mov     rcx, rbx
    call    InternetCloseHandle
    
    mov     eax, 1
    jmp     @hqg_done
    
@hqg_close:
    mov     rcx, rbx
    call    InternetCloseHandle
    
@hqg_fail:
    xor     eax, eax
    
@hqg_done:
    add     rsp, 38h
    ret
HttpQuickGet endp

; ============================================================================
; CLEANUP
; ============================================================================
Cleanup proc
    sub     rsp, 28h
    
    ; Remove tray icon
    local   nid:NOTIFYICONDATA
    mov     nid.cbSize, sizeof NOTIFYICONDATA
    mov     nid.hWnd, hWndMain
    mov     nid.uID, ID_TRAY_ICON
    lea     rcx, nid
    mov     edx, NIM_DELETE
    call    Shell_NotifyIconA
    
    ; Close sockets
    cmp     hListenSocket, INVALID_SOCKET
    je      @cu_no_socket
    
    mov     rcx, hListenSocket
    call    closesocket
    mov     hListenSocket, INVALID_SOCKET
    
@cu_no_socket:
    
    ; Cleanup Winsock
    call    WSACleanup
    
    add     rsp, 28h
    ret
Cleanup endp

; ============================================================================
; UTILITY FUNCTIONS (stubs - implement as needed)
; ============================================================================
strcmp proc
    mov     al, [rcx]
    mov     bl, [rdx]
    cmp     al, bl
    jne     @ne
    test    al, al
    jz      @eq
    inc     rcx
    inc     rdx
    jmp     strcmp
@eq:
    xor     eax, eax
    ret
@ne:
    mov     eax, 1
    ret
strcmp endp

strcpy proc
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rsi, rdx
@cp:
    lodsb
    stosb
    test    al, al
    jnz     @cp
    mov     rax, rdi
    dec     rax
    pop     rsi
    pop     rdi
    ret
strcpy endp

strcat proc
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rsi, rdx
@find:
    cmp     byte ptr [rdi], 0
    je      @cat
    inc     rdi
    jmp     @find
@cat:
    lodsb
    stosb
    test    al, al
    jnz     @cat
    mov     rax, rdi
    dec     rax
    pop     rsi
    pop     rdi
    ret
strcat endp

strcat_char proc
    push    rdi
    mov     rdi, rcx
@find:
    cmp     byte ptr [rdi], 0
    je      @cat
    inc     rdi
    jmp     @find
@cat:
    stosb
    mov     byte ptr [rdi], 0
    pop     rdi
    ret
strcat_char endp

strlen proc
    push    rdi
    mov     rdi, rcx
    xor     eax, eax
    xor     ecx, ecx
    not     rcx
    repne   scasb
    mov     rax, rcx
    not     rax
    dec     rax
    pop     rdi
    ret
strlen endp

IsGgufFile proc
    ; Check if filename ends with .gguf
    push    rbx
    mov     rbx, rcx
    call    strlen
    add     rbx, rax
    sub     rbx, 5
    
    cmp     dword ptr [rbx], ".ggu"
    jne     @not_gguf
    cmp     byte ptr [rbx+4], 'f'
    jne     @not_gguf
    
    mov     eax, 1
    pop     rbx
    ret
    
@not_gguf:
    xor     eax, eax
    pop     rbx
    ret
IsGgufFile endp

FormatBytes proc
    ; Convert bytes to human readable string
    ret
FormatBytes endp

FindBodyStart proc
    ; Find \r\n\r\n in HTTP request
    ret
FindBodyStart endp

ParseHttpRequest proc
    ret
ParseHttpRequest endp

ParseAskJson proc
    ret
ParseAskJson endp

ParseOllamaModelList proc
    ret
ParseOllamaModelList endp

ParseOllamaResponse proc
    ret
ParseOllamaResponse endp

SelectBackendForModel proc
    mov     eax, BACKEND_OLLAMA
    ret
SelectBackendForModel endp

SelectBestBackend proc
    ret
SelectBestBackend endp

CheckBackendHealth proc
    ret
CheckBackendHealth endp

UpdateTrayTooltip proc
    ret
UpdateTrayTooltip endp

ShowStatusWindow proc
    ret
ShowStatusWindow endp

ShowModelsWindow proc
    ret
ShowModelsWindow endp

ShowSwarmWindow proc
    ret
ShowSwarmWindow endp

SendHealthStatus proc
    ret
SendHealthStatus endp

Send404 proc
    ret
Send404 endp

Send405 proc
    ret
Send405 endp

SendJsonResponse proc
    ret
SendJsonResponse endp

SendRawResponse proc
    ret
SendRawResponse endp

SendFrontendAnswer proc
    ret
SendFrontendAnswer endp

ForwardToLlamaCpp proc
    ret
ForwardToLlamaCpp endp

ForwardToRawrxd proc
    ret
ForwardToRawrxd endp

GenerateLocalResponse proc
    ret
GenerateLocalResponse endp

HttpGet proc
    ret
HttpGet endp

HttpPostToOllama proc
    ret
HttpPostToOllama endp

; ============================================================================
; DATA (continued)
; ============================================================================
.data?

szWin32FindData     WIN32_FIND_DATA <>

; ============================================================================
; END
; ============================================================================
end