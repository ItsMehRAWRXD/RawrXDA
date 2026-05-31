option casemap:none

AF_INET        equ 2
SOCK_STREAM    equ 1
IPPROTO_TCP    equ 6
BUFFER_SIZE    equ 65536

.data
host_ip        db "127.0.0.1",0
port           dw 11434
wsadata_buffer db 512 dup(0)

http_header db "POST /api/generate HTTP/1.1",13,10
            db "Host: 127.0.0.1:11434",13,10
            db "Content-Type: application/json",13,10
            db "Connection: keep-alive",13,10
            db "Content-Length: ",0

newline db 10

.data?
socket_handle  dq ?
recv_buffer    db BUFFER_SIZE dup(?)
carry_buffer   db BUFFER_SIZE dup(?)
carry_len      dq ?

.code

extern WSAStartup:proc
extern socket:proc
extern connect:proc
extern inet_addr:proc
extern htons:proc
extern send:proc
extern recv:proc
extern ndjson_parse_chunk:proc

Http_Init proc
    sub rsp, 40h

    mov ecx, 0202h
    lea rdx, wsadata_buffer
    call WSAStartup

    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    mov socket_handle, rax

    ; sockaddr_in setup
    sub rsp, 20h
    mov word ptr [rsp], AF_INET
    movzx ecx, word ptr [port]
    call htons
    mov word ptr [rsp+2], ax

    lea rcx, host_ip
    call inet_addr
    mov dword ptr [rsp+4], eax

    mov rcx, qword ptr [socket_handle]
    lea rdx, [rsp]
    mov r8d, 16
    call connect

    add rsp, 20h
    add rsp, 40h
    ret
Http_Init endp

Http_Send proc
    ; rcx = ptr json body
    ; rdx = length

    sub rsp, 40h

    ; Minimal send lane for integrated bring-up: send caller-supplied bytes.
    mov r10, rcx
    mov r11, rdx

    mov rcx, qword ptr [socket_handle]
    mov rdx, r10
    mov r8, r11
    xor r9d, r9d
    call send

    add rsp, 40h
    ret
Http_Send endp

Http_RecvLoop proc
    sub rsp, 40h

recv_loop:
    mov rcx, qword ptr [socket_handle]
    lea rdx, recv_buffer
    mov r8d, BUFFER_SIZE
    xor r9d, r9d
    call recv

    test rax, rax
    jle done

    mov rcx, offset recv_buffer
    mov rdx, rax
    call ndjson_parse_chunk

    jmp recv_loop

done:
    add rsp, 40h
    ret
Http_RecvLoop endp

end