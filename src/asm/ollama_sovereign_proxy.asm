; ═══════════════════════════════════════════════════════════════════
; RawrXD Sovereign Ollama Proxy — Zero-dep MASM64 TCP proxy
; Intercepts VS Code → Ollama and injects agent capabilities on-the-fly
; Build: ml64 /c /Fo ollama_sovereign_proxy.obj ollama_sovereign_proxy.asm
;        link /SUBSYSTEM:CONSOLE /ENTRY:main ollama_sovereign_proxy.obj kernel32.lib ws2_32.lib
; ═══════════════════════════════════════════════════════════════════

option casemap:none

; ── Win32/WinSock imports ─────────────────────────────────────────
EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN WSAStartup:PROC
EXTERN WSACleanup:PROC
EXTERN socket:PROC
EXTERN bind:PROC
EXTERN listen:PROC
EXTERN accept:PROC
EXTERN connect:PROC
EXTERN closesocket:PROC
EXTERN recv:PROC
EXTERN send:PROC
EXTERN setsockopt:PROC
EXTERN htons:PROC
EXTERN inet_addr:PROC
EXTERN CreateThread:PROC
EXTERN CloseHandle:PROC
EXTERN WaitForSingleObject:PROC

; ── Constants ─────────────────────────────────────────────────────
AF_INET           equ 2
SOCK_STREAM       equ 1 
IPPROTO_TCP       equ 6
SOL_SOCKET        equ 0FFFFh
SO_REUSEADDR      equ 4
PROXY_PORT        equ 11435        ; VS Code points here
OLLAMA_PORT       equ 11434        ; Real Ollama backend
BUFFER_SIZE       equ 65536        ; 64KB per connection
INFINITE          equ 0FFFFFFFFh
STD_OUTPUT_HANDLE equ -11

; ── Structures ────────────────────────────────────────────────────
WSADATA STRUCT
    wVersion        dw ?
    wHighVersion    dw ?
    szDescription   db 257 dup(?)
    szSystemStatus  db 129 dup(?)
    iMaxSockets     dw ?
    iMaxUdpDg       dw ?
    lpVendorInfo    dd ?
WSADATA ENDS

SOCKADDR_IN STRUCT
    sin_family      dw ?
    sin_port        dw ?
    sin_addr        dd ?
    sin_zero        db 8 dup(?)
SOCKADDR_IN ENDS

CLIENT_CONTEXT STRUCT
    clientSocket    dq ?
    targetSocket    dq ?
    buffer          db BUFFER_SIZE dup(?)
    bytesRead       dq ?
CLIENT_CONTEXT ENDS

.data
wsaData         WSADATA <>
listenSocket    dq 0
localAddr       SOCKADDR_IN <>
remoteAddr      SOCKADDR_IN <>
threadHandle    dq 0

; ── Injection strings ─────────────────────────────────────────────
szStartMsg      db 'RawrXD Sovereign Proxy: localhost:11435 -> localhost:11434', 0Dh, 0Ah, 0
szApiTags       db '/api/tags', 0
szApiShow       db '/api/show', 0
szBigDaddyG     db 'bigdaddyg', 0
szBG40          db 'bg40', 0
szCapabilityInject db ',"capabilities":{"tools":true,"agent":true,"vision":false,"reasoning":true}', 0
szFamilyInject  db ',"family":"claude-opus","parameter_size":"40B"', 0
szLocalhostIP   db '127.0.0.1', 0

; ── HTTP response templates ───────────────────────────────────────
szHttpOK        db 'HTTP/1.1 200 OK', 0Dh, 0Ah
                db 'Content-Type: application/json', 0Dh, 0Ah
                db 'Access-Control-Allow-Origin: *', 0Dh, 0Ah
                db 'Connection: close', 0Dh, 0Ah, 0Dh, 0Ah, 0

szJsonModels    db '{"models":[', 0
szJsonModel     db '{"name":"bigdaddyg:latest"', 0
szJsonCaps      db ',"capabilities":{"tools":true,"agent":true,"vision":false,"reasoning":true}', 0
szJsonDetails   db ',"details":{"parent_model":"","format":"gguf","family":"claude-opus","families":["claude-opus"],"parameter_size":"40B","quantization_level":"Q4_K_M"}', 0
szJsonSize      db ',"size":42949672960', 0
szJsonEnd       db '}]}', 0

; ── Show Response strings ─────────────────────────────────────────
szJsonShowStart db '{', 0
szJsonShowModelfile db '"modelfile":"# RawrXD Sovereign\nFROM bigdaddyg:latest\n",', 0
szJsonShowDetails db '"details":{"parent_model":"","format":"gguf","family":"claude-opus","families":["claude-opus"],"parameter_size":"40B","quantization_level":"Q4_K_M"}', 0
szJsonShowEnd   db '}', 0

.code
; ────────────────────────────────────────────────────────────────
; PrintStr — Print null-terminated string to stdout
;   RCX = string pointer
; ────────────────────────────────────────────────────────────────
PrintStr PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 38h                       ; Align stack and make space for args
    
    mov     rbx, rcx
    call    StrLenA
    mov     r8, rax                        ; length
    
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    
    mov     rcx, rax                       ; handle
    mov     rdx, rbx                       ; buffer
    ; R8 = length (already set)
    lea     r9, [rsp+30h]                  ; lpNumberOfBytesWritten
    mov     qword ptr [rsp+20h], 0         ; lpOverlapped
    call    WriteFile
    
    add     rsp, 38h
    pop     rbp
    ret
PrintStr ENDP

; ────────────────────────────────────────────────────────────────
; main — TCP proxy entry point
; ────────────────────────────────────────────────────────────────
main PROC
    sub     rsp, 38h                       ; Shadow space + alignment
    
    ; WSAStartup(2.2, &wsaData)
    mov     cx, 0202h                      ; wVersionRequested
    lea     rdx, wsaData                   ; lpWSAData
    call    WSAStartup
    test    eax, eax
    jnz     @exit_error
    
    ; Create listening socket
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    mov     listenSocket, rax
    cmp     rax, -1
    je      @cleanup

    ; Set SO_REUSEADDR
    mov     rcx, listenSocket
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, [rsp+30h]                  ; use stack for the 'optval'
    mov     dword ptr [r9], 1              ; optval = 1
    mov     qword ptr [rsp+20h], 4         ; optlen = sizeof(int)
    call    setsockopt
    
    ; Bind to 0.0.0.0:11435
    mov     localAddr.sin_family, AF_INET
    mov     cx, PROXY_PORT
    call    htons
    mov     localAddr.sin_port, ax
    mov     localAddr.sin_addr, 0          ; INADDR_ANY
    
    mov     rcx, listenSocket
    lea     rdx, localAddr
    mov     r8d, sizeof SOCKADDR_IN
    call    bind
    test    eax, eax
    jnz     @cleanup
    
    ; Listen with backlog of 10
    mov     rcx, listenSocket
    mov     edx, 10
    call    listen
    test    eax, eax
    jnz     @cleanup
    
    ; Print startup message
    lea     rcx, szStartMsg
    call    PrintStr
    
@accept_loop:
    ; Accept incoming connection
    mov     rcx, listenSocket
    xor     edx, edx
    xor     r8, r8
    call    accept
    cmp     rax, -1
    je      @accept_loop
    
    ; Spawn thread for each client
    ; CreateThread(NULL, 0, HandleClientThread, clientSocket, 0, NULL)
    mov     r9, rax                        ; lpParameter = clientSocket
    lea     r8, HandleClientThread         ; lpStartAddress
    xor     edx, edx                       ; dwStackSize = 0
    xor     ecx, ecx                       ; lpThreadAttributes = NULL
    mov     qword ptr [rsp+20h], 0         ; dwCreationFlags = 0
    mov     qword ptr [rsp+28h], 0         ; lpThreadId = NULL
    call    CreateThread
    
    mov     rcx, rax                       ; thread handle
    call    CloseHandle                    ; Release handle (thread keeps running)
    
    jmp     @accept_loop

@cleanup:
    mov     rcx, listenSocket
    call    closesocket
    call    WSACleanup
    
@exit_error:
    mov     ecx, 1
    call    ExitProcess
main ENDP

; ────────────────────────────────────────────────────────────────
; HandleClientThread — Per-connection handler
;   RCX = thread parameter (client socket)
; ────────────────────────────────────────────────────────────────
HandleClientThread PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, sizeof CLIENT_CONTEXT + 20h
    
    mov     rdi, rcx                       ; client socket
    lea     rbx, [rsp+20h]                 ; context buffer
    mov     CLIENT_CONTEXT.clientSocket[rbx], rdi
    mov     CLIENT_CONTEXT.targetSocket[rbx], -1 ; Initialize as invalid

@relay_loop:
    ; Read from client
    mov     rcx, CLIENT_CONTEXT.clientSocket[rbx]
    lea     rdx, CLIENT_CONTEXT.buffer[rbx]
    mov     r8d, BUFFER_SIZE
    xor     r9d, r9d
    call    recv
    mov     CLIENT_CONTEXT.bytesRead[rbx], rax
    cmp     rax, 0
    jle     @close_target
    
    ; Check for /api/tags request
    lea     rcx, CLIENT_CONTEXT.buffer[rbx]
    lea     rdx, szApiTags
    call    StrStrA
    test    rax, rax
    jnz     @inject_tags
    
    ; Check for /api/show request  
    lea     rcx, CLIENT_CONTEXT.buffer[rbx]
    lea     rdx, szApiShow
    call    StrStrA
    test    rax, rax
    jnz     @inject_show
    
    ; If not intercepted, we need a backend connection
    cmp     qword ptr CLIENT_CONTEXT.targetSocket[rbx], -1
    jne     @forward_request

    ; Connect to real Ollama (127.0.0.1:11434)
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM  
    mov     r8d, IPPROTO_TCP
    call    socket
    mov     CLIENT_CONTEXT.targetSocket[rbx], rax
    cmp     rax, -1
    je      @close_target
    
    mov     remoteAddr.sin_family, AF_INET
    mov     cx, OLLAMA_PORT
    call    htons
    mov     remoteAddr.sin_port, ax
    
    lea     rcx, szLocalhostIP
    call    inet_addr
    mov     remoteAddr.sin_addr, eax
    
    mov     rcx, CLIENT_CONTEXT.targetSocket[rbx]
    lea     rdx, remoteAddr
    mov     r8d, sizeof SOCKADDR_IN
    call    connect
    test    eax, eax
    jnz     @close_target

@inject_tags:
    ; Send fake /api/tags with capabilities
    mov     rcx, CLIENT_CONTEXT.clientSocket[rbx]
    call    SendCapabilityResponse
    jmp     @close_target
    
@inject_show:
    ; Handle /api/show with model details
    mov     rcx, CLIENT_CONTEXT.clientSocket[rbx]
    call    SendModelShowResponse
    jmp     @close_target

@forward_request:
    ; Forward to real Ollama
    mov     rcx, CLIENT_CONTEXT.targetSocket[rbx]
    lea     rdx, CLIENT_CONTEXT.buffer[rbx]
    mov     r8, CLIENT_CONTEXT.bytesRead[rbx]
    xor     r9d, r9d
    call    send
    
@drain_target:
    ; Read response from Ollama
    mov     rcx, CLIENT_CONTEXT.targetSocket[rbx]
    lea     rdx, CLIENT_CONTEXT.buffer[rbx]
    mov     r8d, BUFFER_SIZE
    xor     r9d, r9d
    call    recv
    mov     CLIENT_CONTEXT.bytesRead[rbx], rax
    cmp     rax, 0
    jle     @close_target
    
    ; Send response to client
    mov     rcx, CLIENT_CONTEXT.clientSocket[rbx]
    lea     rdx, CLIENT_CONTEXT.buffer[rbx]
    mov     r8, CLIENT_CONTEXT.bytesRead[rbx]
    xor     r9d, r9d
    call    send
    
    ; Very basic chunking support: if the buffer was full, try to read more
    ; (Note: This is still not perfect but much better for streaming responses)
    cmp     rax, BUFFER_SIZE
    je      @drain_target
    
    jmp     @relay_loop

@close_target:
    mov     rcx, CLIENT_CONTEXT.targetSocket[rbx]
    call    closesocket
    
@close_client:
    mov     rcx, CLIENT_CONTEXT.clientSocket[rbx]
    call    closesocket
    
    lea     rsp, [rbp]
    pop     rbp
    ret
HandleClientThread ENDP

; ────────────────────────────────────────────────────────────────
; SendCapabilityResponse — Send fake /api/tags with agent capabilities
;   RCX = client socket
; ────────────────────────────────────────────────────────────────
SendCapabilityResponse PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h
    
    mov     rdi, rcx                       ; save client socket
    
    ; Send HTTP headers
    lea     rcx, szHttpOK
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szHttpOK
    xor     r9d, r9d
    call    send
    
    ; Send JSON start
    lea     rcx, szJsonModels
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonModels
    xor     r9d, r9d
    call    send
    
    ; Send model with capabilities
    lea     rcx, szJsonModel
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonModel
    xor     r9d, r9d
    call    send
    
    ; Send capabilities
    lea     rcx, szJsonCaps
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonCaps
    xor     r9d, r9d
    call    send
    
    ; Send details info
    lea     rcx, szJsonDetails
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonDetails
    xor     r9d, r9d
    call    send

    ; Send size info
    lea     rcx, szJsonSize
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonSize
    xor     r9d, r9d
    call    send
    
    ; Send JSON end
    lea     rcx, szJsonEnd
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonEnd
    xor     r9d, r9d
    call    send
    
    add     rsp, 30h
    pop     rbp
    ret
SendCapabilityResponse ENDP

; ────────────────────────────────────────────────────────────────
; SendModelShowResponse — Send /api/show with model metadata
;   RCX = client socket
; ────────────────────────────────────────────────────────────────
SendModelShowResponse PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 30h
    
    mov     rdi, rcx                       ; save client socket
    
    ; Send HTTP headers
    lea     rcx, szHttpOK
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szHttpOK
    xor     r9d, r9d
    call    send
    
    ; Send JSON parts
    lea     rcx, szJsonShowStart
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonShowStart
    xor     r9d, r9d
    call    send
    
    lea     rcx, szJsonShowModelfile
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonShowModelfile
    xor     r9d, r9d
    call    send
    
    lea     rcx, szJsonShowDetails
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonShowDetails
    xor     r9d, r9d
    call    send
    
    lea     rcx, szJsonShowEnd
    call    StrLenA
    mov     r8, rax
    mov     rcx, rdi
    lea     rdx, szJsonShowEnd
    xor     r9d, r9d
    call    send
    
    add     rsp, 30h
    pop     rbp
    ret
SendModelShowResponse ENDP

; ────────────────────────────────────────────────────────────────
; StrStrA — Simple substring search (case-sensitive)
;   RCX = haystack, RDX = needle
;   Returns: RAX = pointer to match or 0
; ────────────────────────────────────────────────────────────────
StrStrA PROC
    push    rbp
    mov     rbp, rsp
    push    rsi
    push    rdi
    
    mov     r10, rcx                       ; haystack
    mov     r11, rdx                       ; needle
    
    mov     rcx, r11
    call    StrLenA
    mov     r8, rax                        ; needle length
    test    r8, r8
    jz      @found_empty
    
    mov     rcx, r10
    call    StrLenA
    mov     r9, rax                        ; haystack length
    
    cmp     r9, r8
    jl      @not_found
    
    sub     r9, r8
    inc     r9                             ; iterations
    
@loop_start:
    mov     rsi, r11                       ; needle
    mov     rdi, r10                       ; haystack pos
    mov     rcx, r8                        ; length
    repe    cmpsb
    je      @found_match
    
    inc     r10
    dec     r9
    jnz     @loop_start
    
@not_found:
    xor     rax, rax
    jmp     @exit
    
@found_match:
    mov     rax, r10
    jmp     @exit
    
@found_empty:
    mov     rax, r10
    
@exit:
    pop     rdi
    pop     rsi
    pop     rbp
    ret
StrStrA ENDP

; ────────────────────────────────────────────────────────────────
; StrLenA — Get string length
;   RCX = string pointer
;   Returns: RAX = length
; ────────────────────────────────────────────────────────────────
StrLenA PROC
    mov     rax, rcx
    
@count_loop:
    cmp     byte ptr [rax], 0
    je      @done
    inc     rax
    jmp     @count_loop
    
@done:
    sub     rax, rcx
    ret
StrLenA ENDP

END