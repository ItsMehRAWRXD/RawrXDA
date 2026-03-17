; ============================================================================
; http_minimal.asm - Minimal Winsock Implementation for Ollama API
; Real socket-based HTTP client for local Ollama communication
; Production-ready: error handling, timeouts, proper cleanup
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include winsock2.inc

includelib kernel32.lib
includelib ws2_32.lib

PUBLIC HttpMin_Init
PUBLIC HttpMin_Connect
PUBLIC HttpMin_Send
PUBLIC HttpMin_Recv
PUBLIC HttpMin_Close
PUBLIC HttpMin_SetTimeout
PUBLIC HttpMin_GetLastError

; Constants
SOCKET_TIMEOUT          EQU 30000        ; 30 seconds
RECV_BUFFER_SIZE        EQU 65536        ; 64KB
HTTP_PORT               EQU 11434        ; Ollama default

; Return codes
HTTP_SUCCESS            EQU 1
HTTP_FAILURE            EQU 0

; Structures
HttpConnection STRUCT
    hSocket             DWORD ?
    dwTimeout           DWORD ?
    szHost[256]         BYTE ?
    dwPort              DWORD ?
    bConnected          DWORD ?
    dwLastError         DWORD ?
HttpConnection ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

    g_Connection HttpConnection <0, SOCKET_TIMEOUT, 0, HTTP_PORT, 0, 0>
    
    szWinSockInitErr    db "Failed to initialize Winsock2", 0
    szSocketCreateErr   db "Failed to create socket", 0
    szConnectErr        db "Failed to connect to server", 0
    szSendErr           db "Failed to send data", 0
    szRecvErr           db "Failed to receive data", 0
    szTimeoutErr        db "Operation timeout", 0

.data?

    g_wsaData           WSADATA <>
    g_RecvBuffer        BYTE RECV_BUFFER_SIZE DUP(?)

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; HttpMin_Init - Initialize Winsock2
; Output: eax = 1 success, 0 failure
; ============================================================================

HttpMin_Init PROC

    push ebx
    
    ; Initialize Winsock2 (version 2.2)
    invoke WSAStartup, 0x202, addr g_wsaData
    test eax, eax
    jnz @fail
    
    mov eax, HTTP_SUCCESS
    pop ebx
    ret

@fail:
    mov [g_Connection.dwLastError], eax
    xor eax, eax
    pop ebx
    ret

HttpMin_Init ENDP

; ============================================================================
; HttpMin_Connect - Connect to Ollama server
; Input: pszHost (e.g., "localhost"), dwPort
; Output: eax = 1 success, 0 failure
; ============================================================================

HttpMin_Connect PROC USES esi edi pszHost:DWORD, dwPort:DWORD

    LOCAL sockaddr:sockaddr_in
    LOCAL hSocket:DWORD
    LOCAL bOptVal:DWORD
    LOCAL cbOptLen:DWORD

    ; Validate input
    test pszHost, pszHost
    jz @fail

    ; Store connection info
    mov eax, pszHost
    lea esi, [g_Connection.szHost]
    mov ecx, 256
    xor edx, edx

@copy_host:
    mov dl, byte ptr [eax]
    test dl, dl
    jz @copy_done
    mov [esi], dl
    inc eax
    inc esi
    loop @copy_host

@copy_done:
    mov byte ptr [esi], 0
    mov eax, dwPort
    mov [g_Connection.dwPort], eax

    ; Create socket (IPv4, TCP)
    invoke socket, AF_INET, SOCK_STREAM, IPPROTO_TCP
    cmp eax, INVALID_SOCKET
    je @fail
    mov hSocket, eax
    mov [g_Connection.hSocket], eax

    ; Set socket timeout using select()
    mov bOptVal, 1
    mov cbOptLen, 4
    invoke setsockopt, hSocket, SOL_SOCKET, SO_RCVTIMEO, addr [g_Connection.dwTimeout], 4
    invoke setsockopt, hSocket, SOL_SOCKET, SO_SNDTIMEO, addr [g_Connection.dwTimeout], 4

    ; Prepare socket address structure
    mov sockaddr.sin_family, AF_INET
    
    ; Convert port to network byte order
    mov eax, dwPort
    xchg al, ah
    mov sockaddr.sin_port, ax

    ; Resolve hostname and convert to IP
    invoke inet_addr, pszHost
    test eax, eax
    jz @resolve_failed

    mov sockaddr.sin_addr, eax
    jmp @connect_socket

@resolve_failed:
    ; Try gethostbyname if inet_addr fails
    invoke gethostbyname, pszHost
    test eax, eax
    jz @fail
    
    ; Copy IP from hostent structure
    mov eax, [eax + 12]         ; h_addr_list[0]
    mov eax, [eax]
    mov eax, [eax]
    mov sockaddr.sin_addr, eax

@connect_socket:
    ; Connect to server
    invoke connect, hSocket, addr sockaddr, 16
    cmp eax, SOCKET_ERROR
    je @connect_fail

    mov [g_Connection.bConnected], 1
    mov eax, HTTP_SUCCESS
    ret

@connect_fail:
    ; Clean up socket on failure
    invoke closesocket, hSocket
    xor eax, eax
    ret

@fail:
    xor eax, eax
    ret

HttpMin_Connect ENDP

; ============================================================================
; HttpMin_Send - Send HTTP request to server
; Input: pszRequest (HTTP request string), cbRequest (length)
; Output: eax = 1 success, 0 failure
; ============================================================================

HttpMin_Send PROC USES esi pszRequest:DWORD, cbRequest:DWORD

    LOCAL dwSent:DWORD
    LOCAL dwTotal:DWORD

    mov esi, [g_Connection.hSocket]
    test esi, esi
    jz @not_connected

    cmp [g_Connection.bConnected], 0
    je @not_connected

    ; Send entire request
    invoke send, esi, pszRequest, cbRequest, 0
    cmp eax, SOCKET_ERROR
    je @send_fail

    ; eax contains number of bytes sent
    mov dwSent, eax
    mov dwTotal, 0

    ; Loop if not all data sent
@send_loop:
    cmp dwSent, cbRequest
    jge @send_success

    ; Send remaining data
    mov eax, pszRequest
    add eax, dwSent
    mov ecx, cbRequest
    sub ecx, dwSent
    invoke send, esi, eax, ecx, 0
    cmp eax, SOCKET_ERROR
    je @send_fail

    add dwSent, eax
    jmp @send_loop

@send_success:
    mov eax, HTTP_SUCCESS
    ret

@send_fail:
    invoke WSAGetLastError
    mov [g_Connection.dwLastError], eax
    xor eax, eax
    ret

@not_connected:
    mov [g_Connection.dwLastError], 0xFFFFFFFF
    xor eax, eax
    ret

HttpMin_Send ENDP

; ============================================================================
; HttpMin_Recv - Receive HTTP response from server
; Input: pszBuffer (destination), cbBuffer (max size)
; Output: eax = bytes received, 0 on failure
; ============================================================================

HttpMin_Recv PROC USES esi edi pszBuffer:DWORD, cbBuffer:DWORD

    LOCAL dwRecv:DWORD
    LOCAL dwTotal:DWORD

    mov esi, [g_Connection.hSocket]
    test esi, esi
    jz @not_connected

    cmp [g_Connection.bConnected], 0
    je @not_connected

    xor edx, edx            ; bytes received counter

@recv_loop:
    cmp edx, cbBuffer
    jge @recv_done

    ; Receive chunk
    mov eax, pszBuffer
    add eax, edx
    mov ecx, cbBuffer
    sub ecx, edx
    
    invoke recv, esi, eax, ecx, 0
    
    ; Check for error or connection closed
    cmp eax, SOCKET_ERROR
    je @recv_fail

    cmp eax, 0
    je @recv_done              ; Connection closed gracefully

    add edx, eax
    jmp @recv_loop

@recv_done:
    mov eax, edx
    ret

@recv_fail:
    invoke WSAGetLastError
    mov [g_Connection.dwLastError], eax
    xor eax, eax
    ret

@not_connected:
    mov [g_Connection.dwLastError], 0xFFFFFFFF
    xor eax, eax
    ret

HttpMin_Recv ENDP

; ============================================================================
; HttpMin_Close - Close connection and cleanup
; ============================================================================

HttpMin_Close PROC

    mov eax, [g_Connection.hSocket]
    test eax, eax
    jz @exit

    cmp [g_Connection.bConnected], 0
    je @cleanup

    ; Shutdown socket gracefully
    invoke shutdown, eax, SD_BOTH

@cleanup:
    ; Close socket
    invoke closesocket, eax
    
    mov [g_Connection.hSocket], 0
    mov [g_Connection.bConnected], 0

@exit:
    ret

HttpMin_Close ENDP

; ============================================================================
; HttpMin_SetTimeout - Set socket timeout (milliseconds)
; Input: dwTimeoutMs
; ============================================================================

HttpMin_SetTimeout PROC dwTimeoutMs:DWORD

    mov eax, dwTimeoutMs
    mov [g_Connection.dwTimeout], eax

    mov eax, [g_Connection.hSocket]
    test eax, eax
    jz @exit

    ; Apply timeout to socket
    invoke setsockopt, eax, SOL_SOCKET, SO_RCVTIMEO, addr [g_Connection.dwTimeout], 4
    invoke setsockopt, eax, SOL_SOCKET, SO_SNDTIMEO, addr [g_Connection.dwTimeout], 4

@exit:
    ret

HttpMin_SetTimeout ENDP

; ============================================================================
; HttpMin_GetLastError - Get last error code
; Output: eax = error code
; ============================================================================

HttpMin_GetLastError PROC

    mov eax, [g_Connection.dwLastError]
    ret

HttpMin_GetLastError ENDP

END
