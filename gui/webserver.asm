; ============================================================================
; RawrXD Pure MASM Web Server - Serves HTML frontend
; ============================================================================
; Build: ml64 webserver.asm /link /subsystem:console /entry:start 
;        /defaultlib:kernel32.lib /defaultlib:ws2_32.lib 
;        /out:webserver.exe
; ============================================================================

option casemap:none

includelib kernel32.lib
includelib ws2_32.lib

; ============================================================================
; CONSTANTS
; ============================================================================
PORT            equ     3000
BUFFER_SIZE     equ     65536
MAX_FILE_SIZE   equ     10485760    ; 10MB max file size

; Socket constants
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
    wVersion        dw      ?
    wHighVersion    dw      ?
    szDescription   db      257 dup(?)
    szSystemStatus  db      129 dup(?)
    iMaxSockets     dw      ?
    iMaxUdpDg       dw      ?
    lpVendorInfo    dq      ?
WSADATA ends

sockaddr_in struct
    sin_family      dw      ?
    sin_port        dw      ?
    sin_addr        dd      ?
    sin_zero        db      8 dup(?)
sockaddr_in ends

; ============================================================================
; EXTERNAL FUNCTIONS
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

; ============================================================================
; DATA SECTION
; ============================================================================
.data
align 16

; Messages
szStartMsg      db      "RawrXD Web Server v1.0", 13, 10
                db      "Serving: http://localhost:3000/ide_chatbot.html", 13, 10
                db      "Press Ctrl+C to stop", 13, 10, 13, 10, 0
szListening     db      "[Server] Listening on port 3000...", 13, 10, 0
szRequest       db      "[Request] ", 0
szServing       db      "[Serving] ", 0
szError         db      "[Error] ", 0
szNewline       db      13, 10, 0

; HTTP Responses
szHttpOk        db      "HTTP/1.1 200 OK", 13, 10
                db      "Content-Type: text/html; charset=utf-8", 13, 10
                db      "Access-Control-Allow-Origin: *", 13, 10
                db      "Access-Control-Allow-Methods: GET, POST, OPTIONS", 13, 10
                db      "Access-Control-Allow-Headers: Content-Type, Accept", 13, 10
                db      "Connection: close", 13, 10
                db      "Content-Length: ", 0

szHttp404       db      "HTTP/1.1 404 Not Found", 13, 10
                db      "Content-Type: text/html", 13, 10
                db      "Connection: close", 13, 10, 13, 10
                db      "<html><body><h1>404 Not Found</h1></body></html>", 0
szHttp404Len    equ     $ - szHttp404

; File paths
szFilePath      db      "ide_chatbot.html", 0
szGetPrefix     db      "GET /", 0
szGetSuffix     db      " HTTP/", 0

; ============================================================================
; BSS SECTION
; ============================================================================
.data?
align 16

wsaData         WSADATA <>
serverAddr      sockaddr_in <>
listenSocket    dq      ?
clientSocket    dq      ?
hStdOut         dq      ?
buffer          db      BUFFER_SIZE dup(?)
fileBuffer      db      MAX_FILE_SIZE dup(?)
tempBuffer      db      256 dup(?)
fileSize        dq      ?
bytesRead       dd      ?
bytesSent       dd      ?

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; ENTRY POINT
; ============================================================================
start proc
    sub     rsp, 28h
    
    ; Get console handle
    mov     ecx, -11            ; STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     hStdOut, rax
    
    ; Print startup message
    lea     rcx, szStartMsg
    call    PrintConsole
    
    ; Initialize Winsock
    mov     ecx, 0202h          ; Version 2.2
    lea     rdx, wsaData
    call    WSAStartup
    test    eax, eax
    jnz     error_exit
    
    ; Create socket
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    cmp     rax, INVALID_SOCKET
    je      error_exit
    mov     listenSocket, rax
    
    ; Set SO_REUSEADDR
    mov     r9d, 4              ; sizeof(int)
    lea     r8, bytesRead
    mov     dword ptr [r8], 1
    mov     edx, SO_REUSEADDR
    mov     ecx, SOL_SOCKET
    mov     rcx, listenSocket
    call    setsockopt
    
    ; Setup server address
    mov     serverAddr.sin_family, AF_INET
    mov     ecx, PORT
    call    htons
    mov     serverAddr.sin_port, ax
    mov     serverAddr.sin_addr, 0  ; INADDR_ANY
    
    ; Bind socket
    mov     rcx, listenSocket
    lea     rdx, serverAddr
    mov     r8d, sizeof sockaddr_in
    call    bind
    test    eax, eax
    jnz     error_exit
    
    ; Listen
    mov     rcx, listenSocket
    mov     edx, 10             ; Backlog
    call    listen
    test    eax, eax
    jnz     error_exit
    
    lea     rcx, szListening
    call    PrintConsole
    
main_loop:
    ; Accept connection
    mov     rcx, listenSocket
    xor     rdx, rdx
    xor     r8, r8
    call    accept
    cmp     rax, INVALID_SOCKET
    je      main_loop
    mov     clientSocket, rax
    
    ; Receive request
    mov     rcx, clientSocket
    lea     rdx, buffer
    mov     r8d, BUFFER_SIZE
    xor     r9d, r9d
    call    recv
    test    eax, eax
    jle     close_client
    
    ; Log request
    lea     rcx, szRequest
    call    PrintConsole
    lea     rcx, buffer
    call    PrintConsole
    
    ; Parse GET request
    lea     rcx, buffer
    lea     rdx, szGetPrefix
    call    StrStartsWith
    test    al, al
    jz      send_404
    
    ; Extract filename
    lea     rcx, buffer
    add     rcx, 5              ; Skip "GET /"
    lea     rdx, tempBuffer
    call    ExtractPath
    
    ; Check if requesting HTML file
    lea     rcx, tempBuffer
    lea     rdx, szFilePath
    call    StrEqual
    test    al, al
    jz      send_404
    
    ; Read HTML file
    lea     rcx, szFilePath
    call    ReadHtmlFile
    test    rax, rax
    jz      send_404
    
    ; Send HTTP 200 + HTML
    call    SendHtmlResponse
    jmp     close_client
    
send_404:
    ; Send 404
    mov     rcx, clientSocket
    lea     rdx, szHttp404
    mov     r8d, szHttp404Len
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
; ReadHtmlFile - Read HTML file into fileBuffer
; Input: RCX = filename
; Output: RAX = file size (0 if error)
; ============================================================================
ReadHtmlFile proc
    push    rbx
    push    rsi
    sub     rsp, 28h
    
    mov     rbx, rcx
    
    ; Open file
    mov     rcx, rbx
    mov     edx, 80000000h      ; GENERIC_READ
    mov     r8d, 1              ; FILE_SHARE_READ
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], 3  ; OPEN_EXISTING
    call    CreateFileA
    cmp     rax, -1
    je      rf_error
    mov     rsi, rax
    
    ; Get file size
    mov     rcx, rsi
    lea     rdx, fileSize
    call    GetFileSizeEx
    test    eax, eax
    jz      rf_close
    
    ; Check size limit
    mov     rax, fileSize
    cmp     rax, MAX_FILE_SIZE
    ja      rf_close
    
    ; Read file
    mov     rcx, rsi
    lea     rdx, fileBuffer
    mov     r8d, dword ptr fileSize
    lea     r9, bytesRead
    mov     qword ptr [rsp+20h], 0
    call    ReadFile
    
    ; Close file
    mov     rcx, rsi
    call    CloseHandle
    
    ; Return size
    mov     rax, fileSize
    jmp     rf_done
    
rf_close:
    mov     rcx, rsi
    call    CloseHandle
    
rf_error:
    xor     rax, rax
    
rf_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
ReadHtmlFile endp

; ============================================================================
; SendHtmlResponse - Send HTTP 200 + HTML content
; ============================================================================
SendHtmlResponse proc
    sub     rsp, 38h
    
    ; Send header
    mov     rcx, clientSocket
    lea     rdx, szHttpOk
    lea     r8, szHttpOk
    call    lstrlenA
    mov     r8d, eax
    xor     r9d, r9d
    call    send
    
    ; Send content-length value
    mov     ecx, dword ptr fileSize
    lea     rdx, tempBuffer
    call    IntToStr
    
    mov     rcx, clientSocket
    lea     rdx, tempBuffer
    lea     r8, tempBuffer
    call    lstrlenA
    mov     r8d, eax
    xor     r9d, r9d
    call    send
    
    ; Send double CRLF
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
    
    ; Send HTML content
    mov     rcx, clientSocket
    lea     rdx, fileBuffer
    mov     r8d, dword ptr fileSize
    xor     r9d, r9d
    call    send
    
    add     rsp, 38h
    ret
SendHtmlResponse endp

; ============================================================================
; UTILITY FUNCTIONS
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
    jz      ssw_match
    
    movzx   ecx, byte ptr [rsi]
    cmp     cl, al
    jne     ssw_nomatch
    
    inc     rsi
    inc     rdi
    jmp     ssw_loop
    
ssw_match:
    mov     al, 1
    jmp     ssw_done
    
ssw_nomatch:
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
    jne     se_nomatch
    
    test    al, al
    jz      se_match
    
    inc     rsi
    inc     rdi
    jmp     se_loop
    
se_match:
    mov     al, 1
    jmp     se_done
    
se_nomatch:
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
    
    mov     eax, ecx
    mov     rsi, rdx
    mov     ebx, 10
    
    ; Handle zero
    test    eax, eax
    jnz     its_convert
    mov     byte ptr [rsi], '0'
    mov     byte ptr [rsi+1], 0
    jmp     its_done
    
its_convert:
    ; Find end
    mov     rdi, rsi
its_count:
    xor     edx, edx
    div     ebx
    push    rdx
    test    eax, eax
    jnz     its_count
    
    ; Write digits
its_write:
    pop     rax
    add     al, '0'
    mov     byte ptr [rsi], al
    inc     rsi
    cmp     rsp, rsp
    jne     its_write
    
    mov     byte ptr [rsi], 0
    
its_done:
    pop     rsi
    pop     rbx
    ret
IntToStr endp

end
