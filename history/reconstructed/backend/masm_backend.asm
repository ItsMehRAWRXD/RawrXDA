; =============================================================================
; MASM x64 HTTP Backend Server for RawrXD IDE
; =============================================================================
; Listens on port 8080, implements:
;   GET /models  -> Returns JSON model list
;   POST /ask    -> Returns JSON response
;   OPTIONS /*   -> CORS preflight support
;
; Build: ml64 /c masm_backend.asm
;        link masm_backend.obj ws2_32.lib kernel32.lib /subsystem:console /entry:start
;
; Run:   masm_backend.exe
; =============================================================================

option casemap:none

; Windows x64 calling convention
; RCX, RDX, R8, R9 for first 4 args, rest on stack
; RAX for return value
; Shadow space: 32 bytes (4 * 8) must be reserved

; ----- Constants -----
AF_INET         equ 2
SOCK_STREAM     equ 1
IPPROTO_TCP     equ 6
SOMAXCONN       equ 0x7fffffff
INVALID_SOCKET  equ -1
SOCKET_ERROR    equ -1

PORT            equ 8080

; ----- Structures -----
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

; ----- External functions -----
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
extern htons:proc
extern ExitProcess:proc
extern GetStdHandle:proc
extern WriteConsoleA:proc

.data
; ----- WSA Data -----
wsaData     WSADATA <>

; ----- Socket handles -----
listenSock  dq 0
clientSock  dq 0

; ----- Address structures -----
serverAddr  sockaddr_in <>
clientAddr  sockaddr_in <>
addrLen     dd 16

; ----- Buffers -----
recvBuffer  db 4096 dup(0)
sendBuffer  db 4096 dup(0)

; ----- Startup messages -----
msgStartup  db "=== RawrXD MASM Backend Server ===",13,10,0
msgStartLen equ $ - msgStartup - 1

msgListen   db "Listening on http://localhost:8080",13,10,0
msgListLen  equ $ - msgListen - 1

msgAccept   db "Client connected",13,10,0
msgAcceptLen equ $ - msgAccept - 1

msgError    db "Error occurred",13,10,0
msgErrLen   equ $ - msgError - 1

; ----- Route patterns -----
szGET       db "GET",0
szPOST      db "POST",0
szOPTIONS   db "OPTIONS",0
szModels    db "/models",0
szAsk       db "/ask",0

; ----- HTTP Responses -----
; CORS preflight response
httpCORS    db "HTTP/1.1 200 OK",13,10
            db "Access-Control-Allow-Origin: *",13,10
            db "Access-Control-Allow-Methods: GET, POST, OPTIONS",13,10
            db "Access-Control-Allow-Headers: Content-Type, Accept",13,10
            db "Access-Control-Max-Age: 86400",13,10
            db "Content-Length: 0",13,10
            db 13,10,0
httpCORSLen equ $ - httpCORS - 1

; GET /models response
httpModels  db "HTTP/1.1 200 OK",13,10
            db "Content-Type: application/json",13,10
            db "Access-Control-Allow-Origin: *",13,10
            db "Connection: close",13,10
            db 13,10
            db '{"models":[{"name":"rawrxd-7b","path":"D:\\models\\rawrxd-7b.gguf"},{"name":"rawrxd-13b","path":"D:\\models\\rawrxd-13b.gguf"},{"name":"codestral-22b","path":"D:\\models\\codestral-22b.gguf"}]}',0
httpModelsLen equ $ - httpModels - 1

; POST /ask response
httpAsk     db "HTTP/1.1 200 OK",13,10
            db "Content-Type: application/json",13,10
            db "Access-Control-Allow-Origin: *",13,10
            db "Connection: close",13,10
            db 13,10
            db '{"answer":"MASM backend is alive! This is a pure x64 assembly HTTP server responding to your request. The RawrXD IDE backend is operational."}',0
httpAskLen  equ $ - httpAsk - 1

; 404 response
http404     db "HTTP/1.1 404 Not Found",13,10
            db "Content-Type: application/json",13,10
            db "Access-Control-Allow-Origin: *",13,10
            db "Connection: close",13,10
            db 13,10
            db '{"error":"Not Found"}',0
http404Len  equ $ - http404 - 1

; Console handle
hStdOut     dq 0
bytesWritten dd 0

.code

; =============================================================================
; PrintConsole - Write string to console
; RCX = pointer to string, RDX = length
; =============================================================================
PrintConsole proc
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    
    mov rsi, rcx            ; string pointer
    mov rdi, rdx            ; length
    
    ; Get stdout handle
    mov ecx, -11            ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    ; WriteConsoleA
    mov rcx, hStdOut
    mov rdx, rsi
    mov r8, rdi
    lea r9, bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
PrintConsole endp

; =============================================================================
; StrStr - Find substring in string
; RCX = haystack, RDX = needle
; Returns: RAX = pointer to match or 0
; =============================================================================
StrStr proc
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx            ; haystack
    mov rdi, rdx            ; needle
    
.searchLoop:
    movzx eax, byte ptr [rsi]
    test al, al
    jz .notFound            ; end of haystack
    
    ; Try to match needle at current position
    mov rbx, rsi
    mov rcx, rdi
    
.matchLoop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz .found               ; end of needle = match!
    
    movzx edx, byte ptr [rbx]
    test dl, dl
    jz .notFound            ; end of haystack before needle matched
    
    cmp al, dl
    jne .nextPos            ; mismatch
    
    inc rbx
    inc rcx
    jmp .matchLoop
    
.nextPos:
    inc rsi
    jmp .searchLoop
    
.found:
    mov rax, rsi
    jmp .done
    
.notFound:
    xor eax, eax
    
.done:
    pop rdi
    pop rsi
    pop rbx
    ret
StrStr endp

; =============================================================================
; StrLen - Get string length
; RCX = string pointer
; Returns: RAX = length
; =============================================================================
StrLen proc
    xor rax, rax
.loop:
    cmp byte ptr [rcx + rax], 0
    je .done
    inc rax
    jmp .loop
.done:
    ret
StrLen endp

; =============================================================================
; SendResponse - Send HTTP response to client
; RCX = clientSock, RDX = response string, R8 = length
; =============================================================================
SendResponse proc
    push rbx
    sub rsp, 30h
    
    mov rbx, rcx            ; save socket
    
    ; send(socket, buffer, len, 0)
    mov rcx, rbx
    ; rdx already has buffer
    ; r8 already has length
    xor r9d, r9d            ; flags = 0
    call send
    
    add rsp, 30h
    pop rbx
    ret
SendResponse endp

; =============================================================================
; HandleRequest - Route and handle HTTP request
; RCX = clientSock, RDX = request buffer
; =============================================================================
HandleRequest proc
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    mov rbx, rcx            ; client socket
    mov r12, rdx            ; request buffer
    
    ; Check for OPTIONS (CORS preflight)
    mov rcx, r12
    lea rdx, szOPTIONS
    call StrStr
    test rax, rax
    jnz .sendCORS
    
    ; Check for GET /models
    mov rcx, r12
    lea rdx, szGET
    call StrStr
    test rax, rax
    jz .checkPost
    
    mov rcx, r12
    lea rdx, szModels
    call StrStr
    test rax, rax
    jnz .sendModels
    jmp .send404
    
.checkPost:
    ; Check for POST /ask
    mov rcx, r12
    lea rdx, szPOST
    call StrStr
    test rax, rax
    jz .send404
    
    mov rcx, r12
    lea rdx, szAsk
    call StrStr
    test rax, rax
    jnz .sendAsk
    jmp .send404
    
.sendCORS:
    mov rcx, rbx
    lea rdx, httpCORS
    mov r8, httpCORSLen
    call SendResponse
    jmp .done
    
.sendModels:
    mov rcx, rbx
    lea rdx, httpModels
    mov r8, httpModelsLen
    call SendResponse
    jmp .done
    
.sendAsk:
    mov rcx, rbx
    lea rdx, httpAsk
    mov r8, httpAskLen
    call SendResponse
    jmp .done
    
.send404:
    mov rcx, rbx
    lea rdx, http404
    mov r8, http404Len
    call SendResponse
    
.done:
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
HandleRequest endp

; =============================================================================
; Main entry point
; =============================================================================
start proc
    sub rsp, 58h            ; Shadow space + alignment
    
    ; ----- Print startup message -----
    lea rcx, msgStartup
    mov rdx, msgStartLen
    call PrintConsole
    
    ; ----- WSAStartup(2.2, &wsaData) -----
    mov ecx, 0202h          ; Version 2.2
    lea rdx, wsaData
    call WSAStartup
    test eax, eax
    jnz .error
    
    ; ----- Create socket -----
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    cmp rax, INVALID_SOCKET
    je .error
    mov listenSock, rax
    
    ; ----- Setup server address -----
    mov serverAddr.sin_family, AF_INET
    
    ; htons(PORT)
    mov ecx, PORT
    call htons
    mov serverAddr.sin_port, ax
    
    mov serverAddr.sin_addr, 0  ; INADDR_ANY
    
    ; ----- Bind socket -----
    mov rcx, listenSock
    lea rdx, serverAddr
    mov r8d, 16             ; sizeof(sockaddr_in)
    call bind
    cmp eax, SOCKET_ERROR
    je .error
    
    ; ----- Listen -----
    mov rcx, listenSock
    mov edx, SOMAXCONN
    call listen
    cmp eax, SOCKET_ERROR
    je .error
    
    ; ----- Print listening message -----
    lea rcx, msgListen
    mov rdx, msgListLen
    call PrintConsole
    
    ; ===== ACCEPT LOOP =====
.acceptLoop:
    ; accept(listenSock, &clientAddr, &addrLen)
    mov rcx, listenSock
    lea rdx, clientAddr
    lea r8, addrLen
    call accept
    cmp rax, INVALID_SOCKET
    je .acceptLoop          ; retry on error
    mov clientSock, rax
    
    ; Print connection message
    lea rcx, msgAccept
    mov rdx, msgAcceptLen
    call PrintConsole
    
    ; ----- Receive request -----
    ; Clear buffer first
    lea rdi, recvBuffer
    mov ecx, 4096
    xor eax, eax
.clearBuf:
    mov byte ptr [rdi], 0
    inc rdi
    dec ecx
    jnz .clearBuf
    
    ; recv(clientSock, recvBuffer, 4096, 0)
    mov rcx, clientSock
    lea rdx, recvBuffer
    mov r8d, 4096
    xor r9d, r9d
    call recv
    cmp eax, SOCKET_ERROR
    je .closeClient
    test eax, eax
    jz .closeClient         ; connection closed
    
    ; ----- Handle the request -----
    mov rcx, clientSock
    lea rdx, recvBuffer
    call HandleRequest
    
.closeClient:
    ; closesocket(clientSock)
    mov rcx, clientSock
    call closesocket
    
    jmp .acceptLoop
    
.error:
    lea rcx, msgError
    mov rdx, msgErrLen
    call PrintConsole
    
    call WSACleanup
    mov ecx, 1
    call ExitProcess
    
start endp

end
