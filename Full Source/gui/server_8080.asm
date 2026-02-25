; ============================================================================
; RawrXD Backend Server — Production MASM x64 (no Node, no Python)
; ============================================================================
; Listens on 8080. Serves static GUI from gui/; proxies all other requests to
; Win32 tool_server / IDE on 11435. HTML uses the same real features as the
; CLI and Win32 GUI: /api/model, /api/cli, agents, hotpatch, etc.
;
; Routes:
;   GET  /, /launcher, /chatbot, /agents, ... -> static HTML from current dir
;   GET  /gui/* -> static assets (JS, CSS, etc.)
;   GET  /status, /health, /api/*, /models, ... -> proxy to 127.0.0.1:11435
;   POST /api/*, /v1/* -> proxy to backend
;   OPTIONS * -> 200 CORS
;
; Run tool_server (or Win32 IDE) on port 11435 first.
; Build: ml64 /c server_8080.asm /Zi
; Link:  link /subsystem:console /entry:start server_8080.obj kernel32.lib ws2_32.lib /out:server_8080.exe
; ============================================================================

option casemap:none

includelib kernel32.lib
includelib ws2_32.lib

; ============================================================================
; EXTERNS
; ============================================================================
extern ExitProcess:proc
extern GetStdHandle:proc
extern WriteConsoleA:proc
extern CreateFileA:proc
extern ReadFile:proc
extern GetFileSizeEx:proc
extern CloseHandle:proc
extern lstrcpyA:proc
extern lstrcatA:proc
extern lstrlenA:proc

extern WSAStartup:proc
extern WSACleanup:proc
extern socket:proc
extern bind:proc
extern listen:proc
extern accept:proc
extern send:proc
extern recv:proc
extern closesocket:proc
extern htons:proc
extern setsockopt:proc
extern connect:proc
extern inet_addr:proc

; ============================================================================
; CONSTANTS
; ============================================================================
PORT            equ     8080
BACKEND_PORT    equ     11435       ; Win32 tool_server / IDE (real features: /api/model, /api/cli, agents, etc.)
BUFFER_SIZE     equ     65536
MAX_FILE_SIZE   equ     10485760
AF_INET         equ     2
SOCK_STREAM     equ     1
IPPROTO_TCP     equ     6
INVALID_SOCKET  equ     -1
SOCKET_ERROR    equ     -1
SOL_SOCKET      equ     0FFFFh
SO_REUSEADDR    equ     4

; ============================================================================
; STRUCTURES
; ============================================================================
WSADATA struct
    wVersion        dw ?
    wHighVersion    dw ?
    szDescription   db 257 dup(?)
    szSystemStatus  db 129 dup(?)
    iMaxSockets     dw ?
    iMaxUdpDg       dw ?
    lpVendorInfo    dq ?
WSADATA ends

sockaddr_in struct
    sin_family      dw ?
    sin_port        dw ?
    sin_addr        dd ?
    sin_zero        db 8 dup(?)
sockaddr_in ends

; ============================================================================
; DATA
; ============================================================================
.data
align 16

szStartMsg      db "RawrXD Backend (MASM x64) — port 8080, proxy -> Win32 :11435", 13, 10
                db "  GUI static from here; /status /health /api/* -> tool_server/Win32 IDE", 13, 10
                db "  No Python. Run tool_server (or Win32 IDE) on 11435 first.", 13, 10, 0
szListening     db "[Server] Listening on port 8080...", 13, 10, 0
szRequest       db "[Request] ", 0
szNewline       db 13, 10, 0
szCrlfCrlf      db 13, 10, 13, 10, 0

; CORS + JSON 200 header (no Content-Length — we send it per response)
szJsonHeader    db "HTTP/1.1 200 OK", 13, 10
                db "Content-Type: application/json; charset=utf-8", 13, 10
                db "Access-Control-Allow-Origin: *", 13, 10
                db "Access-Control-Allow-Methods: GET, POST, OPTIONS", 13, 10
                db "Access-Control-Allow-Headers: Content-Type", 13, 10
                db "Connection: close", 13, 10
                db "Content-Length: ", 0
szJsonHeaderLen equ $ - szJsonHeader

szCors200       db "HTTP/1.1 200 OK", 13, 10
                db "Access-Control-Allow-Origin: *", 13, 10
                db "Access-Control-Allow-Methods: GET, POST, OPTIONS", 13, 10
                db "Access-Control-Allow-Headers: Content-Type", 13, 10
                db "Content-Length: 0", 13, 10, 13, 10, 0
szCors200Len    equ $ - szCors200

sz404           db "HTTP/1.1 404 Not Found", 13, 10
                db "Content-Type: application/json", 13, 10
                db "Connection: close", 13, 10, 13, 10
                db "{\"error\":\"Not found\"}", 0
sz404Len        equ $ - sz404

; Paths (no leading slash — we strip "GET /" or "POST /")
szLauncher      db "launcher.html", 0
szLauncherShort db "launcher", 0
szChatbot       db "ide_chatbot.html", 0
szChatbotShort  db "chatbot", 0
szAgents        db "agents.html", 0
szMultiwindow   db "multiwindow_ide.html", 0
szFeatureTester db "feature_tester.html", 0
szBruteforce    db "model_bruteforce.html", 0
szTestJumper    db "test_jumper.html", 0
szTestSwarm     db "test_swarm.html", 0
szStandalone    db "ide_chatbot_standalone.html", 0
szWin32         db "ide_chatbot_win32.html", 0
szGuiPrefix     db "gui/", 0

; Method prefixes
szGet           db "GET ", 0
szPost          db "POST ", 0
szOptions       db "OPTIONS ", 0

; Backend (tool_server / Win32 IDE)
szBackendHost   db "127.0.0.1", 0
sz502           db "HTTP/1.1 502 Bad Gateway", 13, 10
                db "Content-Type: application/json", 13, 10
                db "Connection: close", 13, 10, 13, 10
                db "{\"error\":\"Backend unreachable. Start tool_server or Win32 IDE on port 11435.\"}", 0
sz502Len        equ $ - sz502

; HTTP for static files
szHttpOk        db "HTTP/1.1 200 OK", 13, 10
                db "Content-Type: text/html; charset=utf-8", 13, 10
                db "Access-Control-Allow-Origin: *", 13, 10
                db "Connection: close", 13, 10
                db "Content-Length: ", 0

; ============================================================================
; BSS
; ============================================================================
.data?
align 16

wsaData         WSADATA <>
serverAddr      sockaddr_in <>
backendAddr     sockaddr_in <>
listenSocket    dq ?
clientSocket    dq ?
backendSocket   dq ?
hStdOut         dq ?
requestLen      dd ?
buffer          db BUFFER_SIZE dup(?)
fileBuffer      db MAX_FILE_SIZE dup(?)
tempBuffer      db 512 dup(?)
fileSize        dq ?
bytesRead       dd ?

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; ENTRY
; ============================================================================
start proc
    sub     rsp, 28h
    mov     ecx, -11
    call    GetStdHandle
    mov     hStdOut, rax

    lea     rcx, szStartMsg
    call    PrintConsole

    mov     ecx, 0202h
    lea     rdx, wsaData
    call    WSAStartup
    test    eax, eax
    jnz     error_exit

    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    cmp     rax, INVALID_SOCKET
    je      error_exit
    mov     listenSocket, rax

    mov     rcx, listenSocket
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, bytesRead
    mov     dword ptr [r9], 1
    mov     qword ptr [rsp+20h], 4
    call    setsockopt

    mov     serverAddr.sin_family, AF_INET
    mov     ecx, PORT
    call    htons
    mov     serverAddr.sin_port, ax
    mov     serverAddr.sin_addr, 0

    mov     rcx, listenSocket
    lea     rdx, serverAddr
    mov     r8d, sizeof sockaddr_in
    call    bind
    test    eax, eax
    jnz     error_exit

    mov     rcx, listenSocket
    mov     edx, 10
    call    listen
    test    eax, eax
    jnz     error_exit

    lea     rcx, szListening
    call    PrintConsole

main_loop:
    mov     rcx, listenSocket
    xor     rdx, rdx
    xor     r8, r8
    call    accept
    cmp     rax, INVALID_SOCKET
    je      main_loop
    mov     clientSocket, rax

    mov     rcx, clientSocket
    lea     rdx, buffer
    mov     r8d, BUFFER_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     close_client
    mov     requestLen, eax

    lea     rcx, szRequest
    call    PrintConsole
    lea     rcx, buffer
    call    PrintConsole

    ; --- OPTIONS -> CORS 200 ---
    lea     rcx, buffer
    lea     rdx, szOptions
    call    StrStartsWith
    test    al, al
    jz      try_get
    mov     rcx, clientSocket
    lea     rdx, szCors200
    mov     r8d, szCors200Len
    xor     r9d, r9d
    call    send
    jmp     close_client

try_get:
    lea     rcx, buffer
    lea     rdx, szGet
    call    StrStartsWith
    test    al, al
    jz      try_post

    ; GET: extract path (after "GET /")
    lea     rcx, buffer
    add     rcx, 5
    lea     rdx, tempBuffer
    call    ExtractPath

    ; GUI routes -> serve file (everything else proxied to Win32 backend)
    movzx   eax, byte ptr [tempBuffer]
    test    al, al
    jz      serve_launcher

    lea     rcx, tempBuffer
    lea     rdx, szLauncher
    call    StrEqual
    test    al, al
    jnz     serve_launcher
    lea     rcx, tempBuffer
    lea     rdx, szLauncherShort
    call    StrEqual
    test    al, al
    jnz     serve_launcher

    lea     rcx, tempBuffer
    lea     rdx, szChatbot
    call    StrEqual
    test    al, al
    jnz     serve_chatbot
    lea     rcx, tempBuffer
    lea     rdx, szChatbotShort
    call    StrEqual
    test    al, al
    jnz     serve_chatbot

    lea     rcx, tempBuffer
    lea     rdx, szAgents
    call    StrEqual
    test    al, al
    jnz     serve_agents

    lea     rcx, tempBuffer
    lea     rdx, szMultiwindow
    call    StrEqual
    test    al, al
    jnz     serve_multiwindow

    lea     rcx, tempBuffer
    lea     rdx, szFeatureTester
    call    StrEqual
    test    al, al
    jnz     serve_feature_tester

    lea     rcx, tempBuffer
    lea     rdx, szBruteforce
    call    StrEqual
    test    al, al
    jnz     serve_bruteforce

    lea     rcx, tempBuffer
    lea     rdx, szTestJumper
    call    StrEqual
    test    al, al
    jnz     serve_test_jumper

    lea     rcx, tempBuffer
    lea     rdx, szTestSwarm
    call    StrEqual
    test    al, al
    jnz     serve_test_swarm

    lea     rcx, tempBuffer
    lea     rdx, szStandalone
    call    StrEqual
    test    al, al
    jnz     serve_standalone

    lea     rcx, tempBuffer
    lea     rdx, szWin32
    call    StrEqual
    test    al, al
    jnz     serve_win32

    ; /gui/xxx -> serve file (tempBuffer = "gui/ide_chatbot_engine.js" etc.)
    lea     rcx, tempBuffer
    lea     rdx, szGuiPrefix
    call    StrStartsWith
    test    al, al
    jnz     serve_gui_file

    ; Any other GET -> proxy to Win32 backend (tool_server :11435)
    jmp     do_proxy_backend

serve_launcher:
    lea     rcx, szLauncher
    jmp     serve_file
serve_chatbot:
    lea     rcx, szChatbot
    jmp     serve_file
serve_agents:
    lea     rcx, szAgents
    jmp     serve_file
serve_multiwindow:
    lea     rcx, szMultiwindow
    jmp     serve_file
serve_feature_tester:
    lea     rcx, szFeatureTester
    jmp     serve_file
serve_bruteforce:
    lea     rcx, szBruteforce
    jmp     serve_file
serve_test_jumper:
    lea     rcx, szTestJumper
    jmp     serve_file
serve_test_swarm:
    lea     rcx, szTestSwarm
    jmp     serve_file
serve_standalone:
    lea     rcx, szStandalone
    jmp     serve_file
serve_win32:
    lea     rcx, szWin32
    jmp     serve_file

serve_gui_file:
    ; tempBuffer = "gui/ide_chatbot_engine.js" -> open "ide_chatbot_engine.js" (strip "gui/")
    lea     rcx, tempBuffer
    add     rcx, 4
    jmp     serve_file_by_ptr

serve_file:
    ; RCX = pointer to filename (zero-terminated)
serve_file_by_ptr:
    call    ReadHtmlFile
    test    rax, rax
    jz      send_404
    call    SendHtmlResponse
    jmp     close_client

do_proxy_backend:
    call    ProxyToBackend
    jmp     close_client

try_post:
    lea     rcx, buffer
    lea     rdx, szPost
    call    StrStartsWith
    test    al, al
    jz      send_404
    ; All POST -> proxy to Win32 backend
    jmp     do_proxy_backend

send_404:
    mov     rcx, clientSocket
    lea     rdx, sz404
    mov     r8d, sz404Len
    xor     r9d, r9d
    call    send

close_client:
    mov     rcx, clientSocket
    call    closesocket
    jmp     main_loop

error_exit:
    call    WSACleanup
    xor     ecx, ecx
    call    ExitProcess
    add     rsp, 28h
    ret
start endp

; ============================================================================
; ReadHtmlFile — RCX = filename -> RAX = size, fileBuffer filled
; ============================================================================
ReadHtmlFile proc
    push    rbx
    push    rsi
    sub     rsp, 38h
    mov     rbx, rcx
    mov     rcx, rbx
    mov     edx, 80000000h
    mov     r8d, 1
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 3
    call    CreateFileA
    cmp     rax, -1
    je      rf_fail
    mov     rsi, rax
    mov     rcx, rsi
    lea     rdx, fileSize
    call    GetFileSizeEx
    test    eax, eax
    jz      rf_close
    mov     rax, fileSize
    cmp     rax, MAX_FILE_SIZE
    ja      rf_close
    mov     rcx, rsi
    lea     rdx, fileBuffer
    mov     r8d, dword ptr fileSize
    lea     r9, bytesRead
    mov     qword ptr [rsp+20h], 0
    call    ReadFile
    mov     rcx, rsi
    call    CloseHandle
    mov     rax, fileSize
    jmp     rf_done
rf_close:
    mov     rcx, rsi
    call    CloseHandle
rf_fail:
    xor     rax, rax
rf_done:
    add     rsp, 38h
    pop     rsi
    pop     rbx
    ret
ReadHtmlFile endp

; ============================================================================
; SendHtmlResponse — send 200 + Content-Length + fileBuffer
; ============================================================================
SendHtmlResponse proc
    sub     rsp, 38h
    mov     rcx, clientSocket
    lea     rdx, szHttpOk
    lea     r8, szHttpOk
    call    lstrlenA
    mov     r8d, eax
    xor     r9d, r9d
    call    send
    mov     ecx, dword ptr fileSize
    lea     rdx, tempBuffer
    call    IntToStr
    lea     rcx, tempBuffer
    call    lstrlenA
    mov     rcx, clientSocket
    lea     rdx, tempBuffer
    mov     r8d, eax
    xor     r9d, r9d
    call    send
    mov     rcx, clientSocket
    lea     rdx, szNewline
    mov     r8d, 2
    xor     r9d, r9d
    call    send
    mov     rcx, clientSocket
    lea     rdx, szNewline
    mov     r8d, 2
    xor     r9d, r9d
    call    send
    mov     rcx, clientSocket
    lea     rdx, fileBuffer
    mov     r8d, dword ptr fileSize
    xor     r9d, r9d
    call    send
    add     rsp, 38h
    ret
SendHtmlResponse endp

; ============================================================================
; ProxyToBackend — Forward request to Win32 tool_server / IDE (port 11435)
; HTML uses same features as CLI/Win32 GUI: /api/model, /api/cli, agents, etc.
; ============================================================================
ProxyToBackend proc
    push    rbx
    push    rsi
    sub     rsp, 38h
    mov     backendAddr.sin_family, AF_INET
    mov     ecx, BACKEND_PORT
    call    htons
    mov     backendAddr.sin_port, ax
    lea     rcx, szBackendHost
    call    inet_addr
    mov     backendAddr.sin_addr, eax
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    cmp     rax, INVALID_SOCKET
    je      proxy_502
    mov     backendSocket, rax
    mov     rcx, backendSocket
    lea     rdx, backendAddr
    mov     r8d, sizeof sockaddr_in
    call    connect
    test    eax, eax
    jnz     proxy_close_502
    mov     rcx, backendSocket
    lea     rdx, buffer
    mov     r8d, requestLen
    xor     r9d, r9d
    call    send
    cmp     eax, SOCKET_ERROR
    je      proxy_close_502
proxy_loop:
    mov     rcx, backendSocket
    lea     rdx, buffer
    mov     r8d, BUFFER_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     proxy_done
    mov     rbx, rax
    mov     rcx, clientSocket
    lea     rdx, buffer
    mov     r8d, ebx
    xor     r9d, r9d
    call    send
    cmp     eax, SOCKET_ERROR
    je      proxy_done
    jmp     proxy_loop
proxy_done:
    mov     rcx, backendSocket
    call    closesocket
    mov     backendSocket, 0
    jmp     proxy_exit
proxy_close_502:
    mov     rcx, backendSocket
    call    closesocket
    mov     backendSocket, 0
proxy_502:
    mov     rcx, clientSocket
    lea     rdx, sz502
    mov     r8d, sz502Len
    xor     r9d, r9d
    call    send
proxy_exit:
    add     rsp, 38h
    pop     rsi
    pop     rbx
    ret
ProxyToBackend endp

; ============================================================================
; Helpers
; ============================================================================
PrintConsole proc
    sub     rsp, 38h
    mov     rbx, rcx
    mov     rcx, rbx
    call    lstrlenA
    mov     rcx, hStdOut
    mov     rdx, rbx
    mov     r8d, eax
    lea     r9, bytesRead
    mov     qword ptr [rsp+20h], 0
    call    WriteConsoleA
    add     rsp, 38h
    ret
PrintConsole endp

StrStartsWith proc
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
ssw_loop:
    movzx   eax, byte ptr [rdi]
    test    al, al
    jz      ssw_yes
    movzx   ecx, byte ptr [rsi]
    cmp     cl, al
    jne     ssw_no
    inc     rsi
    inc     rdi
    jmp     ssw_loop
ssw_yes:
    mov     al, 1
    jmp     ssw_done
ssw_no:
    xor     al, al
ssw_done:
    pop     rdi
    pop     rsi
    ret
StrStartsWith endp

StrEqual proc
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
se_loop:
    movzx   eax, byte ptr [rsi]
    movzx   ecx, byte ptr [rdi]
    cmp     al, cl
    jne     se_no
    test    al, al
    jz      se_yes
    inc     rsi
    inc     rdi
    jmp     se_loop
se_yes:
    mov     al, 1
    jmp     se_done
se_no:
    xor     al, al
se_done:
    pop     rdi
    pop     rsi
    ret
StrEqual endp

ExtractPath proc
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
ep_loop:
    movzx   eax, byte ptr [rsi]
    cmp     al, ' '
    je      ep_done
    cmp     al, '?'
    je      ep_done
    test    al, al
    jz      ep_done
    mov     byte ptr [rdi], al
    inc     rsi
    inc     rdi
    jmp     ep_loop
ep_done:
    mov     byte ptr [rdi], 0
    pop     rdi
    pop     rsi
    ret
ExtractPath endp

IntToStr proc
    push    rbx
    push    rsi
    push    rdi
    mov     eax, ecx
    mov     rsi, rdx
    mov     ebx, 10
    test    eax, eax
    jnz     its_go
    mov     byte ptr [rsi], '0'
    mov     byte ptr [rsi+1], 0
    jmp     its_out
its_go:
    xor     ecx, ecx
its_div:
    xor     edx, edx
    div     ebx
    push    rdx
    inc     ecx
    test    eax, eax
    jnz     its_div
its_wr:
    pop     rax
    add     al, '0'
    mov     byte ptr [rsi], al
    inc     rsi
    dec     ecx
    jnz     its_wr
    mov     byte ptr [rsi], 0
its_out:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
IntToStr endp

end
