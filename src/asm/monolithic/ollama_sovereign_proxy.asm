; ollama_sovereign_proxy.asm
; Transparent HTTP proxy for Ollama capability injection
; Assemble: ml64 /c ollama_sovereign_proxy.asm
; Link: link ollama_sovereign_proxy.obj /subsystem:console /entry:main kernel32.lib ws2_32.lib

option casemap:none

EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateThread:PROC
EXTERN closesocket:PROC
EXTERN recv:PROC
EXTERN send:PROC
EXTERN accept:PROC
EXTERN bind:PROC
EXTERN listen:PROC
EXTERN socket:PROC
EXTERN WSAStartup:PROC
EXTERN connect:PROC
EXTERN inet_addr:PROC
EXTERN htons:PROC
EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC

PROXY_PORT equ 11435
OLLAMA_PORT equ 11434
BUFFER_SIZE equ 65536

ClientContext STRUCT
    clientSocket dq ?
    targetSocket dq ?
    bytesRead    dq ?
    buffer       db BUFFER_SIZE dup(?)
    injectBuf    db BUFFER_SIZE dup(?)
ClientContext ENDS

.data
wsaData db 408 dup(0)
listenSocket dq 0
msg_start db 'RawrXD Sovereign Proxy: localhost:11435 -> localhost:11434',0Dh,0Ah,0
tag_api db '/api/tags',0
ip_local db '127.0.0.1',0
MODEL_NAME db '"name":"bg40"',0
CAPABILITY_JSON db ',"capabilities":{"tools":true,"agent":true,"vision":false},"family":"claude-opus","parameter_size":"40B"',0
CAPABILITY_LEN equ $ - CAPABILITY_JSON - 1

.code
main PROC FRAME
    sub rsp, 68h
    .allocstack 68h
    .endprolog

    ; WSAStartup(0202h, &wsaData)
    mov cx, 0202h
    lea rdx, wsaData
    call WSAStartup
    test eax, eax
    jnz @cleanup

    ; socket(AF_INET, SOCK_STREAM, IPPROTO_IP)
    mov ecx, 2
    mov edx, 1
    xor r8d, r8d
    call socket
    mov listenSocket, rax
    cmp rax, -1
    je @cleanup

    ; bind
    sub rsp, 20h
    mov word ptr [rsp], 2 ; AF_INET
    mov cx, PROXY_PORT
    call htons
    mov word ptr [rsp+2], ax ; sin_port
    mov dword ptr [rsp+4], 0 ; INADDR_ANY
    mov qword ptr [rsp+8], 0 ; sin_zero

    mov rcx, listenSocket
    mov rdx, rsp
    mov r8d, 16
    call bind
    add rsp, 20h
    test eax, eax
    jnz @cleanup

    ; listen
    mov rcx, listenSocket
    mov edx, 10
    call listen
    test eax, eax
    jnz @cleanup

    ; WriteFile to stdout
    mov ecx, -11 ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    lea rdx, msg_start
    mov r8d, 60 ; length of msg_start
    lea r9, [rsp+30h] ; bytes written
    mov qword ptr [rsp+20h], 0
    call WriteFile

@accept_loop:
    mov rcx, listenSocket
    xor edx, edx
    xor r8d, r8d
    call accept
    cmp rax, -1
    je @accept_loop
    mov rdi, rax ; client socket

    ; Allocate ClientContext
    mov rcx, 0
    mov rdx, sizeof ClientContext
    mov r8d, 3000h ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 4     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @close_client
    mov rbx, rax
    mov [rbx.ClientContext.clientSocket], rdi

    ; CreateThread
    xor rcx, rcx
    xor rdx, rdx
    lea r8, handle_client
    mov r9, rbx
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    call CreateThread
    jmp @accept_loop

@close_client:
    mov rcx, rdi
    call closesocket
    jmp @accept_loop

@cleanup:
    xor ecx, ecx
    call ExitProcess
main ENDP

handle_client PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    mov rbx, rcx ; rbx = ClientContext*

    ; socket()
    mov ecx, 2
    mov edx, 1
    xor r8d, r8d
    call socket
    mov [rbx.ClientContext.targetSocket], rax
    cmp rax, -1
    je @hc_cleanup

    ; connect()
    sub rsp, 20h
    mov word ptr [rsp], 2 ; AF_INET
    mov cx, OLLAMA_PORT
    call htons
    mov word ptr [rsp+2], ax
    lea rcx, ip_local
    call inet_addr
    mov dword ptr [rsp+4], eax
    mov qword ptr [rsp+8], 0

    mov rcx, [rbx.ClientContext.targetSocket]
    mov rdx, rsp
    mov r8d, 16
    call connect
    add rsp, 20h
    test eax, eax
    jnz @hc_cleanup

@relay_loop:
    ; recv from client
    mov rcx, [rbx.ClientContext.clientSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8d, BUFFER_SIZE
    xor r9d, r9d
    call recv
    mov [rbx.ClientContext.bytesRead], rax
    cmp rax, 0
    jle @hc_cleanup

    ; check for /api/tags
    lea rcx, [rbx.ClientContext.buffer]
    mov rdx, [rbx.ClientContext.bytesRead]
    lea r8, tag_api
    mov r9d, 9
    call memmem
    test rax, rax
    jz @forward_req

    ; inject capabilities
    mov rcx, rbx
    call inject_capabilities
    jmp @relay_loop

@forward_req:
    ; send to target
    mov rcx, [rbx.ClientContext.targetSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8, [rbx.ClientContext.bytesRead]
    xor r9d, r9d
    call send

    ; recv from target
    mov rcx, [rbx.ClientContext.targetSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8d, BUFFER_SIZE
    xor r9d, r9d
    call recv
    mov [rbx.ClientContext.bytesRead], rax
    cmp rax, 0
    jle @hc_cleanup

    ; send to client
    mov rcx, [rbx.ClientContext.clientSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8, [rbx.ClientContext.bytesRead]
    xor r9d, r9d
    call send
    jmp @relay_loop

@hc_cleanup:
    mov rcx, [rbx.ClientContext.clientSocket]
    call closesocket
    mov rcx, [rbx.ClientContext.targetSocket]
    call closesocket

    mov rcx, rbx
    mov rdx, 0
    mov r8d, 8000h ; MEM_RELEASE
    call VirtualFree

    lea rsp, [rbp]
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
handle_client ENDP

inject_capabilities PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    mov rbx, rcx ; rbx = ClientContext*

    ; forward request to target first
    mov rcx, [rbx.ClientContext.targetSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8, [rbx.ClientContext.bytesRead]
    xor r9d, r9d
    call send

    ; recv response from target
    mov rcx, [rbx.ClientContext.targetSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8d, BUFFER_SIZE
    xor r9d, r9d
    call recv
    mov r15, rax ; r15 = response size
    cmp rax, 0
    jle @ic_send_orig

    ; find "name":"bg40"
    lea rcx, [rbx.ClientContext.buffer]
    mov rdx, r15
    lea r8, MODEL_NAME
    mov r9d, 13
    call memmem
    test rax, rax
    jz @ic_send_orig

    ; rax points to "name":"bg40"
    mov r12, rax
    add r12, 13 ; skip past "name":"bg40"

    ; copy first part to injectBuf
    lea rdi, [rbx.ClientContext.injectBuf]
    lea rsi, [rbx.ClientContext.buffer]
    mov rcx, r12
    sub rcx, rsi ; bytes to copy
    rep movsb

    ; inject CAPABILITY_JSON
    lea rsi, CAPABILITY_JSON
    mov rcx, CAPABILITY_LEN
    rep movsb

    ; copy rest of original
    lea rsi, [rbx.ClientContext.buffer]
    add rsi, r15 ; end of original
    mov rcx, rsi
    sub rcx, r12 ; remaining bytes
    mov rsi, r12
    rep movsb

    ; calculate new size
    lea rax, [rbx.ClientContext.injectBuf]
    sub rdi, rax
    mov r14, rdi

    ; send modified response
    mov rcx, [rbx.ClientContext.clientSocket]
    lea rdx, [rbx.ClientContext.injectBuf]
    mov r8, r14
    xor r9d, r9d
    call send
    jmp @ic_done

@ic_send_orig:
    mov rcx, [rbx.ClientContext.clientSocket]
    lea rdx, [rbx.ClientContext.buffer]
    mov r8, r15
    xor r9d, r9d
    call send

@ic_done:
    lea rsp, [rbp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
inject_capabilities ENDP

memmem PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog

    mov r12, rdx ; haystack_len
    mov r10, r8  ; needle
    mov r11, r9  ; needle_len
    mov r9, rcx  ; haystack

    xor r13, r13 ; i = 0
@mm_loop:
    mov rax, r12
    sub rax, r11
    cmp r13, rax
    jg @mm_not_found

    mov rsi, r9
    add rsi, r13
    mov rdi, r10
    mov rcx, r11
    repe cmpsb
    je @mm_found

    inc r13
    jmp @mm_loop

@mm_found:
    mov rax, r9
    add rax, r13
    jmp @mm_done

@mm_not_found:
    xor eax, eax

@mm_done:
    lea rsp, [rbp]
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
memmem ENDP

END