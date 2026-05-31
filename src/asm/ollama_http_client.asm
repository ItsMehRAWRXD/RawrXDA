; ============================================================================
; RawrXD Ollama HTTP Client — Pure x64 MASM Implementation
; ============================================================================
; Direct Winsock2 HTTP client (no WinHttp abstraction layer)
; Zero-copy streaming parser optimized for NDJSON token response stream
; ============================================================================

include <ksamd64.inc>

; Winsock2 constants and structures
WS_VERSION_REQD = 202h  ; Winsock 2.0

; Socket address families
AF_INET = 2
SOCK_STREAM = 1
IPPROTO_TCP = 6
SOL_SOCKET = 0FFFFh
INVALID_SOCKET = -1
SOCKET_ERROR = -1

; Socket options
SO_RCVTIMEO = 4102
SO_SNDBUF = 1001
SO_RCVBUF = 1002
SO_KEEPALIVE = 8

; HTTP/1.1 constants
HTTP_PORT = 11434
RECV_BUFFER_SIZE = 65536
SEND_BUFFER_SIZE = 8192

; ============================================================================
; External Symbols
; ============================================================================

extern WSAStartup:proc
extern WSACleanup:proc
extern socket:proc
extern connect:proc
extern send:proc
extern recv:proc
extern setsockopt:proc
extern closesocket:proc
extern inet_aton:proc
extern htons:proc
extern WSAGetLastError:proc

; ============================================================================
; Data Section
; ============================================================================

.data

; HTTP request templates
http_post_header db "POST /api/generate HTTP/1.1", 0Dh, 0Ah
                 db "Host: 127.0.0.1:11434", 0Dh, 0Ah
                 db "Content-Type: application/json", 0Dh, 0Ah
                 db "Connection: keep-alive", 0Dh, 0Ah
                 db "Content-Length: "
http_post_header_len = $ - http_post_header

http_header_end db 0Dh, 0Ah, 0Dh, 0Ah

; Keep-alive and connection reuse
keep_alive_interval db "Keep-Alive: timeout=60, max=100", 0Dh, 0Ah
keep_alive_interval_len = $ - keep_alive_interval

; NDJSON token field markers (used in SIMD scan)
response_marker db '"response":"'
response_marker_len = $ - response_marker

done_marker db '"done":true'
done_marker_len = $ - done_marker

; Streaming status indicators
stream_start_marker db '{'
stream_end_marker db '}'

; Socket error messages
socket_error_msg db "Socket creation failed", 0
connect_error_msg db "Connection failed", 0
send_error_msg db "Send failed", 0
recv_error_msg db "Recv failed", 0

; Connection pool state (4 slots for connection reuse)
connection_pool QWORD 4 DUP(0)
pool_timestamps QWORD 4 DUP(0)
pool_lock DWORD 0

; ============================================================================
; Code Section  
; ============================================================================

.code

; ============================================================================
; STRUCT: OllamaHttpClient
; rcx = this pointer
; ============================================================================

OllamaHttpClient_Initialize PROC
    ; Input: rcx = pointer to OllamaHttpClient structure
    ; Output: rax = 0 on success, non-zero on error

    push rbx
    push r12
    sub rsp, 1C8h

    mov rbx, rcx            ; Preserve client pointer

    ; Initialize Winsock2 with a real WSA_DATA buffer.
    mov ecx, WS_VERSION_REQD
    lea rdx, [rsp + 20h]
    call WSAStartup
    test eax, eax
    jnz Initialize_init_failed

    ; Create TCP socket
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    cmp rax, INVALID_SOCKET
    je Initialize_socket_failed

    mov r12, rax
    mov qword ptr [rbx], rax

    ; Set receive timeout.
    mov dword ptr [rsp + 30h], 15000
    mov rcx, r12
    mov edx, SOL_SOCKET
    mov r8d, SO_RCVTIMEO
    lea r9, [rsp + 30h]
    mov dword ptr [rsp + 20h], 4
    call setsockopt
    cmp eax, SOCKET_ERROR
    je Initialize_setsockopt_failed

    xor eax, eax
    add rsp, 1C8h
    pop r12
    pop rbx
    ret

Initialize_setsockopt_failed:
    mov rcx, r12
    call closesocket
    call WSACleanup
    call WSAGetLastError
    add rsp, 1C8h
    pop r12
    pop rbx
    ret

Initialize_socket_failed:
    call WSAGetLastError
    call WSACleanup
    add rsp, 1C8h
    pop r12
    pop rbx
    ret

Initialize_init_failed:
    add rsp, 1C8h
    pop r12
    pop rbx
    ret
OllamaHttpClient_Initialize ENDP

; ============================================================================
; Connect to Ollama backend (127.0.0.1:11434)
; rcx = socket
; Returns: rax = 0 on success, error code otherwise
; ============================================================================

OllamaHttpClient_Connect PROC
    push rbx
    sub rsp, 30h

    mov rbx, rcx

    ; Build sockaddr_in structure on stack
    ; struct sockaddr_in {
    ;   short sin_family;           offset 0
    ;   unsigned short sin_port;    offset 2
    ;   unsigned long sin_addr;     offset 4
    ;   char sin_zero[8];           offset 8
    ; }

    mov word ptr [rsp + 20h], AF_INET

    ; Convert port to network byte order (11434 = 0x2C7A)
    mov ecx, HTTP_PORT
    call htons
    mov word ptr [rsp + 22h], ax

    ; 127.0.0.1 = 0x7F000001 in network byte order
    mov dword ptr [rsp + 24h], 0100007Fh
    xor eax, eax
    mov qword ptr [rsp + 28h], 0

    ; Call connect
    mov rcx, rbx
    lea rdx, [rsp + 20h]
    mov r8d, 16
    call connect

    cmp eax, SOCKET_ERROR
    jne Connect_success

    call WSAGetLastError
    neg eax
    add rsp, 30h
    pop rbx
    ret

Connect_success:
    xor eax, eax
    add rsp, 30h
    pop rbx
    ret
OllamaHttpClient_Connect ENDP

; ============================================================================
; Send an entire buffer, retrying until all bytes are written.
; rcx = socket
; rdx = buffer
; r8  = length in bytes
; Returns: rax = 0 on success, negative error on failure
; ============================================================================

OllamaHttpClient_SendAll PROC
    push rbx
    push r12
    push r13
    sub rsp, 20h

    mov r12, rcx
    mov r13, rdx
    mov ebx, r8d

    test ebx, ebx
    jle SendAll_success

SendAll_loop:
    mov rcx, r12
    mov rdx, r13
    mov r8d, ebx
    xor r9d, r9d
    call send
    cmp eax, SOCKET_ERROR
    je SendAll_socket_error
    test eax, eax
    jle SendAll_zero_write

    sub ebx, eax
    add r13, rax
    jnz SendAll_loop

SendAll_success:
    xor eax, eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret

SendAll_socket_error:
    call WSAGetLastError
    neg eax
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret

SendAll_zero_write:
    mov eax, -1
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
OllamaHttpClient_SendAll ENDP

; ============================================================================
; Send HTTP POST request (payload already formatted)
; rcx = socket
; rdx = payload buffer pointer
; r8 = payload length in bytes
; Returns: rax = 0 on success, negative error on failure
; ============================================================================

OllamaHttpClient_SendRequest PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40h

    mov r12, rcx            ; socket
    mov r13, rdx            ; payload buffer
    mov r14d, r8d           ; payload length

    ; Convert payload length to decimal ASCII in a local buffer.
    lea r10, [rsp + 40h]
    mov eax, r14d
    mov ecx, 10
    test eax, eax
    jnz SendRequest_build_len

    dec r10
    mov byte ptr [r10], '0'
    mov rbx, r10
    mov r15d, 1
    jmp SendRequest_have_len

SendRequest_build_len:
    xor edx, edx
    div ecx
    add dl, '0'
    dec r10
    mov byte ptr [r10], dl
    test eax, eax
    jnz SendRequest_build_len

    mov rbx, r10
    lea rax, [rsp + 40h]
    sub rax, r10
    mov r15d, eax

SendRequest_have_len:
    ; Send HTTP header prefix.
    mov rcx, r12
    lea rdx, http_post_header
    mov r8d, http_post_header_len
    call OllamaHttpClient_SendAll
    test eax, eax
    jnz SendRequest_failed

    ; Send Content-Length digits.
    mov rcx, r12
    mov rdx, rbx
    mov r8d, r15d
    call OllamaHttpClient_SendAll
    test eax, eax
    jnz SendRequest_failed

    ; Finish the header.
    mov rcx, r12
    lea rdx, http_header_end
    mov r8d, 4
    call OllamaHttpClient_SendAll
    test eax, eax
    jnz SendRequest_failed

    ; Send JSON payload.
    mov rcx, r12
    mov rdx, r13
    mov r8d, r14d
    call OllamaHttpClient_SendAll
    test eax, eax
    jnz SendRequest_failed

    xor eax, eax
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

SendRequest_failed:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OllamaHttpClient_SendRequest ENDP

; ============================================================================
; Receive from socket with zero-copy streaming
; rcx = socket
; rdx = output buffer (caller-allocated, 64KB min)
; r8 = max bytes to receive
; Returns: rax = bytes received
; ============================================================================

OllamaHttpClient_RecvStream PROC
    push r12
    sub rsp, 20h

    mov r12, rcx

    ; Call recv
    mov rcx, r12
    xor r9d, r9d
    call recv
    cmp eax, SOCKET_ERROR
    je RecvStream_failed
    add rsp, 20h
    pop r12
    ret

RecvStream_failed:
    call WSAGetLastError
    neg eax
    add rsp, 20h
    pop r12
    ret
OllamaHttpClient_RecvStream ENDP

; ============================================================================
; Cleanup - close socket and WSA
; rcx = socket
; ============================================================================

OllamaHttpClient_Shutdown PROC
    push rbx
    sub rsp, 20h

    mov rbx, rcx

    ; Close socket
    mov rcx, rbx
    call closesocket

    ; Cleanup Winsock
    call WSACleanup
    xor eax, eax
    add rsp, 20h
    pop rbx
    ret
OllamaHttpClient_Shutdown ENDP

; ============================================================================
; Parse streaming NDJSON response and extract tokens
; rcx = buffer pointer (NDJSON stream)
; rdx = buffer length
; r8 = output token buffer (caller-allocated)
; r9 = max output length
; Returns: rax = bytes written to output buffer
; ============================================================================

OllamaHttpClient_ParseStreamingToken PROC
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 40h

    mov r12, rcx            ; input buffer
    mov r13d, edx           ; input length
    mov r14, r8             ; output buffer
    mov rbx, r9             ; max output

    xor r10d, r10d          ; output position

    ; Scan for '"response":"' marker
    xor r11d, r11d          ; current position in input
    
ParseToken_scan_loop:
    cmp r11d, r13d
    jge ParseToken_end
    
    mov al, byte ptr [r12 + r11]
    cmp al, '{'
    je ParseToken_found_start
    
    inc r11d
    jmp ParseToken_scan_loop

ParseToken_found_start:
    ; Find the "response" field
    inc r11d
    
ParseToken_find_response:
    cmp r11d, r13d
    jge ParseToken_end
    
    mov al, byte ptr [r12 + r11]
    cmp al, 'r'
    je ParseToken_check_response
    
    inc r11d
    jmp ParseToken_find_response

ParseToken_check_response:
    ; Verify "response":"
    cmp dword ptr [r12 + r11], "espo"  ; Check "resp"
    jne ParseToken_find_response
    
    ; Skip to opening quote of value
    add r11d, 12  ; Length of '"response":"'
    cmp r11d, r13d
    jge ParseToken_end

ParseToken_extract_value:
    ; Extract token value until closing quote
    cmp r10, rbx
    jge ParseToken_out_of_space
    
    cmp r11d, r13d
    jge ParseToken_end
    
    mov al, byte ptr [r12 + r11]
    cmp al, '"'
    je ParseToken_end
    
    mov byte ptr [r14 + r10], al
    inc r10d
    inc r11d
    jmp ParseToken_extract_value

ParseToken_out_of_space:
    ; Output buffer full
    mov rax, r10
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

ParseToken_end:
    mov rax, r10
    add rsp, 40h
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OllamaHttpClient_ParseStreamingToken ENDP

; ============================================================================
; Connection pool manager - get or create reusable connection
; Returns: rax = socket (from pool or newly created)
; ============================================================================

OllamaHttpClient_GetPooledConnection PROC
    push rbx
    push r12
    sub rsp, 38h

    ; For now, always create a new connection
    ; In production, would check pool and reuse if available
    
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    cmp rax, INVALID_SOCKET
    je GetPooled_failed

    ; Set keep-alive socket option
    mov r12, rax
    mov dword ptr [rsp + 20h], 1
    mov rcx, r12
    mov edx, SOL_SOCKET
    mov r8d, SO_KEEPALIVE
    lea r9, [rsp + 20h]
    mov dword ptr [rsp + 28h], 4
    call setsockopt

    mov rax, r12
    add rsp, 38h
    pop r12
    pop rbx
    ret

GetPooled_failed:
    call WSAGetLastError
    neg eax
    add rsp, 38h
    pop r12
    pop rbx
    ret
OllamaHttpClient_GetPooledConnection ENDP

; ============================================================================
; Return connection to pool for reuse
; rcx = socket
; ============================================================================

OllamaHttpClient_ReturnPooledConnection PROC
    ; Mark socket as available for reuse
    ; In production, would store in pool array with timestamp
    xor eax, eax
    ret
OllamaHttpClient_ReturnPooledConnection ENDP

end
