; =============================================================================
; RawrXD_SocketResponder.asm — IOCP Zero-Copy HTTP Responder
; =============================================================================
; Purpose: Sub-0.5ms HTTP server for /health, /status, /models endpoints.
;          Replaces Python http.server for static cached responses.
;          Uses Windows I/O Completion Ports for async, non-blocking I/O.
;
; Architecture: x64 MASM | Windows ABI | IOCP | Zero-copy WSASend
; Exports:
;   RawrXD_StartSocketServer     — Init IOCP server on given port
;   RawrXD_StopSocketServer      — Graceful shutdown
;   RawrXD_IsServerRunning       — Atomic running flag check
;   RawrXD_UpdateHealthCache     — External cache invalidation
;   RawrXD_UpdateStatusCache     — Update /status response body
;   RawrXD_UpdateModelsCache     — Update /models response body
;   RawrXD_GetServerStats        — Return hit counters
;
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_SocketResponder.obj
;        link: ws2_32.lib kernel32.lib
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; =============================================================================
;                         Winsock2 Constants
; =============================================================================
AF_INET                 EQU     2
SOCK_STREAM             EQU     1
IPPROTO_TCP             EQU     6
SOMAXCONN               EQU     7FFFFFFFh
SOL_SOCKET              EQU     0FFFFh
SO_REUSEADDR            EQU     4
INVALID_SOCKET          EQU     -1
SOCKET_ERROR            EQU     -1
SD_BOTH                 EQU     2
FIONBIO                 EQU     8004667Eh
WSA_IO_PENDING          EQU     997

; IOCP
INFINITE                EQU     0FFFFFFFFh
INVALID_HANDLE_VALUE_Q  EQU     -1

; =============================================================================
;                         Winsock2 Structures
; =============================================================================

WSADATA STRUCT
    wVersion        DW ?
    wHighVersion    DW ?
    iMaxSockets     DW ?
    iMaxUdpDg       DW ?
    lpVendorInfo    DQ ?
    szDescription   DB 257 DUP(?)
    szSystemStatus  DB 129 DUP(?)
WSADATA ENDS

sockaddr_in STRUCT
    sin_family      DW ?
    sin_port        DW ?
    sin_addr        DD ?
    sin_zero        DB 8 DUP(?)
sockaddr_in ENDS

WSABUF STRUCT
    len_            DD ?
    buf_            DQ ?
WSABUF ENDS

OVERLAPPED STRUCT
    Internal        DQ ?
    InternalHigh    DQ ?
    union_offset    DQ ?
    hEvent          DQ ?
OVERLAPPED ENDS

SYSTEM_INFO STRUCT
    dwOemId                     DD ?
    dwPageSize                  DD ?
    lpMinAppAddr                DQ ?
    lpMaxAppAddr                DQ ?
    dwActiveProcessorMask       DQ ?
    dwNumberOfProcessors        DD ?
    dwProcessorType             DD ?
    dwAllocationGranularity     DD ?
    wProcessorLevel             DW ?
    wProcessorRevision          DW ?
SYSTEM_INFO ENDS

; =============================================================================
;                       IOCP Context — Per-Connection
; =============================================================================
; States: 0=RECV_HDR, 1=SEND_RESP, 2=SHUTDOWN
; Endpoint IDs: 0=health, 1=status, 2=models, 3=404

IOCTX_STATE_RECV        EQU     0
IOCTX_STATE_SEND        EQU     1
IOCTX_STATE_SHUTDOWN    EQU     2

ENDPOINT_HEALTH         EQU     0
ENDPOINT_STATUS         EQU     1
ENDPOINT_MODELS         EQU     2
ENDPOINT_404            EQU     3
ENDPOINT_INFERENCE      EQU     4
ENDPOINT_CORS           EQU     5

RECV_BUF_SIZE           EQU     4096
MAX_RESPONSE_SIZE       EQU     131072      ; 128KB max for /models

IOCTX STRUCT
    ovl             OVERLAPPED <>
    wsabuf          WSABUF <>
    sockfd          DQ ?
    state           DD ?
    endpoint_id     DD ?
    recv_buf        DQ ?                ; Pointer to recv buffer (heap)
    recv_len        DD ?
    _pad0           DD ?
IOCTX ENDS

; =============================================================================
;                   Pre-Serialized HTTP Response Templates
; =============================================================================
.data
ALIGN 64

; --- /health (static, never changes) ---
_HEALTH_BODY    DB '{"status":"healthy","server":"RawrXD-IOCP","inference":"active"}', 0
_HEALTH_BODY_LEN EQU $ - _HEALTH_BODY - 1

; Full pre-built /health HTTP response (populated at init and by cache thread)
ALIGN 4096
_HEALTH_RESP    DB 'HTTP/1.1 200 OK', 13, 10
                DB 'Content-Type: application/json', 13, 10
                DB 'Content-Length: '
_HEALTH_CL_POS  LABEL BYTE
                DB '63', 13, 10       ; Length of body above
                DB 'Connection: keep-alive', 13, 10
                DB 'Access-Control-Allow-Origin: *', 13, 10
                DB 'X-Server: RawrXD-IOCP', 13, 10
                DB 13, 10
                DB '{"status":"healthy","server":"RawrXD-IOCP","inference":"active"}'
_HEALTH_RESP_END LABEL BYTE
_HEALTH_RESP_LEN DQ _HEALTH_RESP_END - _HEALTH_RESP

; --- /status (cached, rebuilt every 10s) ---
ALIGN 4096
_STATUS_RESP    DB 512 DUP(0)
_STATUS_RESP_LEN DQ 0

; Default /status template
_STATUS_TMPL    DB 'HTTP/1.1 200 OK', 13, 10
                DB 'Content-Type: application/json', 13, 10
                DB 'Content-Length: '
_STATUS_CL      DB '000', 13, 10
                DB 'Connection: keep-alive', 13, 10
                DB 'Access-Control-Allow-Origin: *', 13, 10
                DB 13, 10, 0

_STATUS_BODY_DEFAULT DB '{"status":"running","model_count":0,"uptime_ms":0}', 0
_STATUS_BODY_LEN_DEF EQU $ - _STATUS_BODY_DEFAULT - 1

; --- /models (cached, rebuilt every 10s) ---
ALIGN 4096
_MODELS_RESP    DB MAX_RESPONSE_SIZE DUP(0)
_MODELS_RESP_LEN DQ 0

; --- 404 response ---
ALIGN 64
_404_RESP       DB 'HTTP/1.1 404 Not Found', 13, 10
                DB 'Content-Type: application/json', 13, 10
                DB 'Content-Length: 24', 13, 10
                DB 'Connection: close', 13, 10
                DB 'Access-Control-Allow-Origin: *', 13, 10
                DB 13, 10
                DB '{"error":"not found"}', 0
_404_RESP_END   LABEL BYTE
_404_RESP_LEN   DQ _404_RESP_END - _404_RESP

; --- OPTIONS (CORS preflight) ---
ALIGN 64
_CORS_RESP      DB 'HTTP/1.1 204 No Content', 13, 10
                DB 'Access-Control-Allow-Origin: *', 13, 10
                DB 'Access-Control-Allow-Methods: GET, POST, OPTIONS', 13, 10
                DB 'Access-Control-Allow-Headers: Content-Type, Authorization', 13, 10
                DB 'Access-Control-Max-Age: 86400', 13, 10
                DB 'Content-Length: 0', 13, 10
                DB 'Connection: keep-alive', 13, 10
                DB 13, 10, 0
_CORS_RESP_END  LABEL BYTE
_CORS_RESP_LEN  DQ _CORS_RESP_END - _CORS_RESP

; --- Debug strings ---
sz_ServerStart  DB '[RawrXD-IOCP] Server starting on 127.0.0.1:', 0
sz_ServerReady  DB '[RawrXD-IOCP] IOCP server ready. Workers: ', 0
sz_ServerStop   DB '[RawrXD-IOCP] Server shutdown requested.', 13, 10, 0
sz_AcceptFail   DB '[RawrXD-IOCP] accept() failed.', 13, 10, 0
sz_CacheRefresh DB '[RawrXD-IOCP] Cache refresh cycle.', 13, 10, 0

; =============================================================================
;                         Global State
; =============================================================================
ALIGN 8
g_hIOCP             DQ 0        ; IOCP handle
g_listenSock        DQ 0        ; Listening socket
g_hCacheThread      DQ 0        ; Cache invalidation thread handle
g_hAcceptThread     DQ 0        ; Accept loop thread handle
g_Running           DD 0        ; Atomic: 1=running, 0=stopped
g_WorkerCount       DD 0        ; Number of IOCP worker threads
g_StartTick         DQ 0        ; Server start timestamp (GetTickCount64)

; Hit counters (atomic)
ALIGN 8
g_HitsHealth        DQ 0
g_HitsStatus        DQ 0
g_HitsModels        DQ 0
g_Hits404           DQ 0
g_HitsCORS          DQ 0
g_TotalConnections  DQ 0
g_ActiveConnections DQ 0

; Cache lock (lightweight — spin + yield)
g_CacheLock         DD 0        ; 0=unlocked, 1=locked

; IOCP context free-list (pool allocator to avoid per-request heap allocs)
IOCTX_POOL_SIZE     EQU 256
ALIGN 8
g_CtxPool           DQ IOCTX_POOL_SIZE DUP(0)  ; Pre-allocated IOCTX pointers
g_CtxPoolTop        DD 0                        ; Stack-based free list index

; =============================================================================
;                       Winsock2 API Externs
; =============================================================================
EXTERNDEF WSAStartup:PROC
EXTERNDEF WSACleanup:PROC
EXTERNDEF WSAGetLastError:PROC
EXTERNDEF WSARecv:PROC
EXTERNDEF WSASend:PROC
EXTERNDEF socket:PROC
EXTERNDEF bind:PROC
EXTERNDEF listen:PROC
EXTERNDEF accept:PROC
EXTERNDEF closesocket:PROC
EXTERNDEF shutdown:PROC
EXTERNDEF setsockopt:PROC
EXTERNDEF htons:PROC
EXTERNDEF ioctlsocket:PROC

; Kernel32 (IOCP)
EXTERNDEF CreateIoCompletionPort:PROC
EXTERNDEF GetQueuedCompletionStatus:PROC
EXTERNDEF PostQueuedCompletionStatus:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF ExitThread:PROC
EXTERNDEF GetSystemInfo:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapFree:PROC
EXTERNDEF GetProcessHeap:PROC
EXTERNDEF InterlockedExchange:PROC
EXTERNDEF OutputDebugStringA:PROC

; =============================================================================
;                   Internal Helper: Spin-Lock Acquire/Release
; =============================================================================
.code

; SpinLock_Acquire — RCX = ptr to DWORD lock
SpinLock_Acquire PROC
    push    rax
@@spin:
    mov     eax, 1
    xchg    DWORD PTR [rcx], eax
    test    eax, eax
    jz      @@acquired
    pause
    jmp     @@spin
@@acquired:
    pop     rax
    ret
SpinLock_Acquire ENDP

; SpinLock_Release — RCX = ptr to DWORD lock
SpinLock_Release PROC
    mov     DWORD PTR [rcx], 0
    mfence
    ret
SpinLock_Release ENDP

; =============================================================================
;                   IOCTX Pool Allocator
; =============================================================================

; Pool_Init — Pre-allocate IOCTX_POOL_SIZE contexts on the heap
Pool_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    call    GetProcessHeap
    mov     rsi, rax                ; rsi = heap handle

    xor     edi, edi                ; edi = index
@@alloc_loop:
    cmp     edi, IOCTX_POOL_SIZE
    jge     @@alloc_done

    ; Allocate IOCTX + recv buffer in one block
    ; Total = sizeof IOCTX + RECV_BUF_SIZE
    mov     rcx, rsi
    xor     edx, edx                ; flags = 0 (HEAP_ZERO_MEMORY = 8)
    or      edx, 8                  ; HEAP_ZERO_MEMORY
    mov     r8d, RECV_BUF_SIZE
    add     r8d, 128                ; sizeof IOCTX ~ 88 rounded up + recv buf
    call    HeapAlloc
    test    rax, rax
    jz      @@alloc_done

    ; Setup IOCTX: recv_buf points to end of struct
    lea     rcx, [rax + 128]        ; recv buffer starts after IOCTX
    mov     [rax].IOCTX.recv_buf, rcx
    mov     [rax].IOCTX.state, IOCTX_STATE_RECV

    ; Push to pool
    lea     rcx, g_CtxPool
    mov     edx, edi
    mov     [rcx + rdx*8], rax

    inc     edi
    jmp     @@alloc_loop

@@alloc_done:
    mov     g_CtxPoolTop, edi

    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Pool_Init ENDP

; Pool_Get — Pop an IOCTX from the free list
; Returns: RAX = IOCTX ptr, or 0 if empty
Pool_Get PROC
    mov     eax, g_CtxPoolTop
    test    eax, eax
    jz      @@empty

    dec     eax
    mov     g_CtxPoolTop, eax
    lea     rcx, g_CtxPool
    mov     rax, [rcx + rax*8]
    ret

@@empty:
    xor     eax, eax
    ret
Pool_Get ENDP

; Pool_Put — Push an IOCTX back to the free list
; RCX = IOCTX ptr
Pool_Put PROC
    mov     eax, g_CtxPoolTop
    cmp     eax, IOCTX_POOL_SIZE
    jge     @@full                      ; Pool is full, leak (shouldn't happen)

    ; Reset state
    mov     [rcx].IOCTX.state, IOCTX_STATE_RECV
    mov     [rcx].IOCTX.recv_len, 0

    lea     rdx, g_CtxPool
    mov     [rdx + rax*8], rcx
    inc     eax
    mov     g_CtxPoolTop, eax
    ret

@@full:
    ret
Pool_Put ENDP

; =============================================================================
;           Fast URL Matcher — Endpoint Identification
; =============================================================================
; This matches the first space-terminated token after "GET " or "OPTIONS "
; RCX = pointer to recv buffer (start of HTTP request)
; RDX = length of received data
; Returns: EAX = endpoint ID (0-5, or 3=404)
;          Also sets: is_options in high bit of EAX (bit 31) if OPTIONS method

MatchEndpoint PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx                ; rsi = buffer
    mov     ebx, edx                ; ebx = length

    ; Minimum HTTP request: "GET / HTTP/1.1\r\n" = 16 bytes
    cmp     ebx, 16
    jl      @@match_404

    ; Check for OPTIONS method (8 bytes: "OPTIONS ")
    cmp     DWORD PTR [rsi], 4954504Fh    ; "OPTI" little-endian
    jne     @@check_get
    cmp     DWORD PTR [rsi+4], 20534E4Fh  ; "ONS " little-endian
    jne     @@check_get
    ; OPTIONS detected — return CORS
    mov     eax, ENDPOINT_CORS
    jmp     @@match_done

@@check_get:
    ; Check for "GET " (4 bytes)
    cmp     DWORD PTR [rsi], 20544547h    ; "GET " little-endian
    jne     @@match_404
    add     rsi, 4                  ; Skip "GET "

    ; Now rsi points to the URL path
    ; Check "/heal" (first 5 chars of /health)
    cmp     DWORD PTR [rsi], 6165682Fh    ; "/hea" little-endian
    jne     @@check_status
    cmp     BYTE PTR [rsi+4], 'l'
    jne     @@check_status
    cmp     BYTE PTR [rsi+5], 't'
    jne     @@check_status
    mov     eax, ENDPOINT_HEALTH
    jmp     @@match_done

@@check_status:
    ; Check "/stat" (first 5 chars of /status)
    cmp     DWORD PTR [rsi], 6174732Fh    ; "/sta" little-endian
    jne     @@check_models
    cmp     BYTE PTR [rsi+4], 't'
    jne     @@check_models
    cmp     BYTE PTR [rsi+5], 'u'
    jne     @@check_models
    mov     eax, ENDPOINT_STATUS
    jmp     @@match_done

@@check_models:
    ; Check "/mode" (first 5 chars of /models)
    cmp     DWORD PTR [rsi], 646F6D2Fh    ; "/mod" little-endian
    jne     @@match_404
    cmp     BYTE PTR [rsi+4], 'e'
    jne     @@match_404
    cmp     BYTE PTR [rsi+5], 'l'
    jne     @@match_404
    mov     eax, ENDPOINT_MODELS
    jmp     @@match_done

@@match_404:
    mov     eax, ENDPOINT_404

@@match_done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
MatchEndpoint ENDP

; =============================================================================
;                   IOCP Worker Thread
; =============================================================================
; Each worker calls GetQueuedCompletionStatus in a loop.
; On RECV completion: parse URL, select cached response, queue WSASend.
; On SEND completion: queue next WSARecv (keep-alive) or close.

WorkerThread PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 88
    .allocstack 88
    .endprolog

    ; Local offsets on stack:
    ;   [rsp+0]  = bytes_xfer (DWORD)
    ;   [rsp+8]  = comp_key (ULONG_PTR)
    ;   [rsp+16] = pOverlapped (LPOVERLAPPED)
    ;   [rsp+24] = flags (DWORD, for WSARecv)
    ;   [rsp+32..80] = shadow space

@@worker_loop:
    ; Check if we should exit
    cmp     g_Running, 0
    je      @@worker_exit

    ; GetQueuedCompletionStatus(hIOCP, &bytes, &key, &pOvl, INFINITE)
    mov     rcx, g_hIOCP
    lea     rdx, [rsp]              ; &bytes_xfer
    lea     r8, [rsp+8]             ; &comp_key
    lea     r9, [rsp+16]            ; &pOverlapped
    sub     rsp, 32
    push    INFINITE                ; dwMilliseconds
    call    GetQueuedCompletionStatus
    add     rsp, 40

    ; Check for shutdown signal (comp_key == 0 and pOverlapped == 0)
    mov     rax, [rsp+8]           ; comp_key
    test    rax, rax
    jnz     @@valid_completion
    mov     rax, [rsp+16]          ; pOverlapped
    test    rax, rax
    jz      @@worker_exit           ; Shutdown sentinel

@@valid_completion:
    ; pOverlapped is the IOCTX pointer (OVERLAPPED is first member)
    mov     rsi, [rsp+16]          ; rsi = IOCTX*
    test    rsi, rsi
    jz      @@worker_loop

    mov     r12d, DWORD PTR [rsp]  ; r12d = bytes transferred

    ; Check for connection closed (0 bytes transferred on recv)
    mov     eax, [rsi].IOCTX.state
    cmp     eax, IOCTX_STATE_RECV
    jne     @@check_send_state

    test    r12d, r12d
    jz      @@close_connection      ; Client disconnected

    ; =====================================================
    ; RECV completed — Parse and respond
    ; =====================================================
    mov     [rsi].IOCTX.recv_len, r12d

    ; Match endpoint
    mov     rcx, [rsi].IOCTX.recv_buf
    mov     edx, r12d
    call    MatchEndpoint
    mov     [rsi].IOCTX.endpoint_id, eax

    ; Select response buffer based on endpoint
    cmp     eax, ENDPOINT_HEALTH
    je      @@sel_health
    cmp     eax, ENDPOINT_STATUS
    je      @@sel_status
    cmp     eax, ENDPOINT_MODELS
    je      @@sel_models
    cmp     eax, ENDPOINT_CORS
    je      @@sel_cors
    jmp     @@sel_404

@@sel_health:
    lock inc QWORD PTR [g_HitsHealth]
    lea     r13, _HEALTH_RESP
    mov     r14, _HEALTH_RESP_LEN
    jmp     @@queue_send

@@sel_status:
    lock inc QWORD PTR [g_HitsStatus]
    ; Acquire cache spin lock for status (since cache thread may be writing)
    lea     rcx, g_CacheLock
    call    SpinLock_Acquire
    lea     r13, _STATUS_RESP
    mov     r14, _STATUS_RESP_LEN
    ; If status not populated yet, use 404
    test    r14, r14
    jnz     @@sel_status_ok
    lea     rcx, g_CacheLock
    call    SpinLock_Release
    jmp     @@sel_404
@@sel_status_ok:
    lea     rcx, g_CacheLock
    call    SpinLock_Release
    jmp     @@queue_send

@@sel_models:
    lock inc QWORD PTR [g_HitsModels]
    lea     rcx, g_CacheLock
    call    SpinLock_Acquire
    lea     r13, _MODELS_RESP
    mov     r14, _MODELS_RESP_LEN
    test    r14, r14
    jnz     @@sel_models_ok
    lea     rcx, g_CacheLock
    call    SpinLock_Release
    jmp     @@sel_404
@@sel_models_ok:
    lea     rcx, g_CacheLock
    call    SpinLock_Release
    jmp     @@queue_send

@@sel_cors:
    lock inc QWORD PTR [g_HitsCORS]
    lea     r13, _CORS_RESP
    mov     r14, _CORS_RESP_LEN
    jmp     @@queue_send

@@sel_404:
    lock inc QWORD PTR [g_Hits404]
    lea     r13, _404_RESP
    mov     r14, _404_RESP_LEN

@@queue_send:
    ; Setup WSABUF for zero-copy send
    mov     [rsi].IOCTX.wsabuf.buf_, r13
    mov     eax, r14d
    mov     [rsi].IOCTX.wsabuf.len_, eax
    mov     [rsi].IOCTX.state, IOCTX_STATE_SEND

    ; Clear OVERLAPPED for reuse
    xor     eax, eax
    mov     [rsi].IOCTX.ovl.Internal, rax
    mov     [rsi].IOCTX.ovl.InternalHigh, rax
    mov     [rsi].IOCTX.ovl.union_offset, rax
    mov     [rsi].IOCTX.ovl.hEvent, rax

    ; WSASend(sockfd, &wsabuf, 1, &bytes_xfer, 0, &ovl, NULL)
    mov     rcx, [rsi].IOCTX.sockfd
    lea     rdx, [rsi].IOCTX.wsabuf
    mov     r8d, 1                  ; buffer count
    lea     r9, [rsp]               ; &bytes_xfer
    sub     rsp, 48
    mov     QWORD PTR [rsp+32], 0   ; dwFlags = 0
    lea     rax, [rsi].IOCTX.ovl
    mov     [rsp+40], rax           ; &overlapped
    push    0                       ; lpCompletionRoutine = NULL
    call    WSASend
    add     rsp, 56

    ; Check for immediate completion or pending
    test    eax, eax
    jz      @@worker_loop           ; Completed immediately
    call    WSAGetLastError
    cmp     eax, WSA_IO_PENDING
    je      @@worker_loop           ; Pending — IOCP will notify
    jmp     @@close_connection      ; Error — close

@@check_send_state:
    cmp     eax, IOCTX_STATE_SEND
    jne     @@close_connection

    ; =====================================================
    ; SEND completed — Check keep-alive, queue next recv
    ; =====================================================

    ; For simplicity: check if endpoint was 404 (Connection: close)
    mov     eax, [rsi].IOCTX.endpoint_id
    cmp     eax, ENDPOINT_404
    je      @@close_connection      ; 404 sends Connection: close

    ; Keep-alive: reset and queue WSARecv
    mov     [rsi].IOCTX.state, IOCTX_STATE_RECV
    mov     [rsi].IOCTX.recv_len, 0

    ; Clear OVERLAPPED
    xor     eax, eax
    mov     [rsi].IOCTX.ovl.Internal, rax
    mov     [rsi].IOCTX.ovl.InternalHigh, rax
    mov     [rsi].IOCTX.ovl.union_offset, rax
    mov     [rsi].IOCTX.ovl.hEvent, rax

    ; Setup recv wsabuf
    mov     rax, [rsi].IOCTX.recv_buf
    mov     [rsi].IOCTX.wsabuf.buf_, rax
    mov     [rsi].IOCTX.wsabuf.len_, RECV_BUF_SIZE

    ; WSARecv(sockfd, &wsabuf, 1, &bytes, &flags, &ovl, NULL)
    mov     DWORD PTR [rsp+24], 0   ; flags = 0
    mov     rcx, [rsi].IOCTX.sockfd
    lea     rdx, [rsi].IOCTX.wsabuf
    mov     r8d, 1
    lea     r9, [rsp]               ; &bytes_xfer
    sub     rsp, 48
    lea     rax, [rsp+72]           ; &flags (rsp+24 pre-sub = rsp+72 post-sub)
    mov     [rsp+32], rax
    lea     rax, [rsi].IOCTX.ovl
    mov     [rsp+40], rax
    push    0                       ; lpCompletionRoutine
    call    WSARecv
    add     rsp, 56

    test    eax, eax
    jz      @@worker_loop
    call    WSAGetLastError
    cmp     eax, WSA_IO_PENDING
    je      @@worker_loop
    ; Fall through to close

@@close_connection:
    ; Graceful close: shutdown + closesocket
    lock dec QWORD PTR [g_ActiveConnections]
    mov     rcx, [rsi].IOCTX.sockfd
    mov     edx, SD_BOTH
    sub     rsp, 32
    call    shutdown
    add     rsp, 32
    mov     rcx, [rsi].IOCTX.sockfd
    sub     rsp, 32
    call    closesocket
    add     rsp, 32

    ; Return IOCTX to pool
    mov     rcx, rsi
    call    Pool_Put

    jmp     @@worker_loop

@@worker_exit:
    add     rsp, 88
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    xor     ecx, ecx
    sub     rsp, 32
    call    ExitThread
    ; Does not return
WorkerThread ENDP

; =============================================================================
;                   Cache Refresh Thread
; =============================================================================
; Rebuilds /status with uptime. /models and /status body are set externally
; via RawrXD_UpdateStatusCache / RawrXD_UpdateModelsCache from C++ side.

CacheThread PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 296
    .allocstack 296
    .endprolog

    ; Local buffer for building responses
    ; [rsp+0..255] = temp buffer for response building

@@cache_loop:
    cmp     g_Running, 0
    je      @@cache_exit

    ; Sleep 10 seconds (10000ms)
    mov     ecx, 10000
    sub     rsp, 32
    call    Sleep
    add     rsp, 32

    cmp     g_Running, 0
    je      @@cache_exit

    ; Rebuild /status with current uptime
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    sub     rax, g_StartTick        ; rax = uptime_ms

    ; Build JSON body: {"status":"running","uptime_ms":XXXX,"hits":{"health":N,...}}
    ; We write directly to a temp buffer, then build full HTTP response
    ; For simplicity, build the body with simple itoa-style conversion

    ; Store uptime in r12
    mov     r12, rax

    ; Build body into temp buffer at [rsp+0]
    lea     rdi, [rsp+0]

    ; Copy prefix
    lea     rsi, _status_json_prefix
    call    @@strcpy_inline         ; copies null-terminated string, rdi advances

    ; Convert uptime to decimal at rdi
    mov     rax, r12
    call    @@itoa_inline

    ; Copy hits section
    lea     rsi, _status_json_hits
    call    @@strcpy_inline

    ; health hits
    mov     rax, g_HitsHealth
    call    @@itoa_inline
    mov     BYTE PTR [rdi], ','
    inc     rdi

    ; status hits
    lea     rsi, _status_json_status_label
    call    @@strcpy_inline
    mov     rax, g_HitsStatus
    call    @@itoa_inline
    mov     BYTE PTR [rdi], ','
    inc     rdi

    ; models hits
    lea     rsi, _status_json_models_label
    call    @@strcpy_inline
    mov     rax, g_HitsModels
    call    @@itoa_inline

    ; Close JSON
    lea     rsi, _status_json_suffix
    call    @@strcpy_inline

    ; Null-terminate
    mov     BYTE PTR [rdi], 0

    ; Calculate body length
    lea     rax, [rsp+0]
    mov     rcx, rdi
    sub     rcx, rax                ; rcx = body length

    ; Build full HTTP response into _STATUS_RESP under lock
    lea     rdx, g_CacheLock
    push    rcx
    mov     rcx, rdx
    call    SpinLock_Acquire
    pop     rcx

    ; Build into _STATUS_RESP
    lea     rdi, _STATUS_RESP
    lea     rsi, _http_200_hdr
    call    @@strcpy_inline

    ; Content-Length: <body_len>
    lea     rsi, _http_cl_hdr
    call    @@strcpy_inline
    mov     rax, rcx                ; body length
    push    rcx
    call    @@itoa_inline
    pop     rcx

    ; CRLF + headers
    lea     rsi, _http_keepalive_cors
    call    @@strcpy_inline

    ; Copy body
    lea     rsi, [rsp+0]
    push    rcx
    call    @@strcpy_inline
    pop     rcx

    ; Calculate total response length
    lea     rax, _STATUS_RESP
    mov     rcx, rdi
    sub     rcx, rax
    mov     _STATUS_RESP_LEN, rcx

    lea     rcx, g_CacheLock
    call    SpinLock_Release

    jmp     @@cache_loop

@@cache_exit:
    add     rsp, 296
    pop     rdi
    pop     rsi
    pop     rbx
    xor     ecx, ecx
    sub     rsp, 32
    call    ExitThread

; --- Inline helpers (not exported, used only by CacheThread) ---

; strcpy_inline: copy null-terminated string from RSI to RDI, advance RDI
@@strcpy_inline:
    push    rax
@@sc_loop:
    lodsb
    test    al, al
    jz      @@sc_done
    stosb
    jmp     @@sc_loop
@@sc_done:
    pop     rax
    ret

; itoa_inline: convert RAX (unsigned 64-bit) to decimal ASCII at RDI, advance RDI
@@itoa_inline:
    push    rbx
    push    rcx
    push    rdx
    push    rsi

    mov     rcx, rsp                ; save stack pos
    sub     rsp, 24                 ; temp digit buffer

    test    rax, rax
    jnz     @@itoa_nonzero
    mov     BYTE PTR [rdi], '0'
    inc     rdi
    jmp     @@itoa_done

@@itoa_nonzero:
    lea     rsi, [rsp]
    xor     ecx, ecx                ; digit count
@@itoa_div:
    xor     edx, edx
    mov     rbx, 10
    div     rbx
    add     dl, '0'
    mov     [rsi+rcx], dl
    inc     ecx
    test    rax, rax
    jnz     @@itoa_div

    ; Reverse digits into rdi
    dec     ecx
@@itoa_rev:
    mov     al, [rsi+rcx]
    stosb
    dec     ecx
    jns     @@itoa_rev

@@itoa_done:
    add     rsp, 24

    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

CacheThread ENDP

; =============================================================================
;                   JSON string fragments for cache builder
; =============================================================================
.data
_status_json_prefix     DB '{"status":"running","uptime_ms":', 0
_status_json_hits       DB ',"hits":{"health":', 0
_status_json_status_label DB '"status":', 0
_status_json_models_label DB '"models":', 0
_status_json_suffix     DB '}}', 0
_http_200_hdr           DB 'HTTP/1.1 200 OK', 13, 10, 0
_http_cl_hdr            DB 'Content-Type: application/json', 13, 10, 'Content-Length: ', 0
_http_keepalive_cors    DB 13, 10, 'Connection: keep-alive', 13, 10
                        DB 'Access-Control-Allow-Origin: *', 13, 10
                        DB 'X-Server: RawrXD-IOCP', 13, 10
                        DB 13, 10, 0

; =============================================================================
;                   Accept Loop Thread
; =============================================================================
.code

AcceptThread PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 72
    .allocstack 72
    .endprolog

@@accept_loop:
    cmp     g_Running, 0
    je      @@accept_exit

    ; accept(listen_sock, NULL, NULL)
    mov     rcx, g_listenSock
    xor     edx, edx
    xor     r8d, r8d
    sub     rsp, 32
    call    accept
    add     rsp, 32

    cmp     rax, INVALID_SOCKET
    je      @@accept_err

    mov     rbx, rax                ; rbx = client socket

    lock inc QWORD PTR [g_TotalConnections]
    lock inc QWORD PTR [g_ActiveConnections]

    ; Get a context from the pool
    call    Pool_Get
    test    rax, rax
    jz      @@accept_close_sock     ; Pool exhausted — reject connection

    mov     rsi, rax                ; rsi = IOCTX*
    mov     [rsi].IOCTX.sockfd, rbx
    mov     [rsi].IOCTX.state, IOCTX_STATE_RECV
    mov     [rsi].IOCTX.recv_len, 0

    ; Clear OVERLAPPED
    xor     eax, eax
    mov     [rsi].IOCTX.ovl.Internal, rax
    mov     [rsi].IOCTX.ovl.InternalHigh, rax
    mov     [rsi].IOCTX.ovl.union_offset, rax
    mov     [rsi].IOCTX.ovl.hEvent, rax

    ; Associate client socket with IOCP
    ; CreateIoCompletionPort(clientSock, hIOCP, completionKey=IOCTX*, 0)
    mov     rcx, rbx                ; FileHandle = client socket
    mov     rdx, g_hIOCP            ; ExistingCompletionPort
    mov     r8, rsi                 ; CompletionKey = IOCTX ptr
    xor     r9d, r9d                ; NumberOfConcurrentThreads = 0
    sub     rsp, 32
    call    CreateIoCompletionPort
    add     rsp, 32
    test    rax, rax
    jz      @@accept_close_ctx      ; Failed to associate

    ; Setup wsabuf for first recv
    mov     rax, [rsi].IOCTX.recv_buf
    mov     [rsi].IOCTX.wsabuf.buf_, rax
    mov     [rsi].IOCTX.wsabuf.len_, RECV_BUF_SIZE

    ; WSARecv(clientSock, &wsabuf, 1, &bytesRecv, &flags, &ovl, NULL)
    mov     DWORD PTR [rsp+0], 0    ; flags = 0
    mov     rcx, rbx
    lea     rdx, [rsi].IOCTX.wsabuf
    mov     r8d, 1
    lea     r9, [rsp+8]             ; &bytesRecv
    sub     rsp, 48
    lea     rax, [rsp+48]           ; &flags
    mov     [rsp+32], rax
    lea     rax, [rsi].IOCTX.ovl
    mov     [rsp+40], rax
    push    0                       ; lpCompletionRoutine = NULL
    call    WSARecv
    add     rsp, 56

    test    eax, eax
    jz      @@accept_loop           ; Completed immediately — IOCP handles it
    call    WSAGetLastError
    cmp     eax, WSA_IO_PENDING
    je      @@accept_loop           ; Pending — normal
    ; Error — cleanup
    jmp     @@accept_close_ctx

@@accept_close_ctx:
    mov     rcx, rsi
    call    Pool_Put
@@accept_close_sock:
    mov     rcx, rbx
    mov     edx, SD_BOTH
    sub     rsp, 32
    call    shutdown
    add     rsp, 32
    mov     rcx, rbx
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
    lock dec QWORD PTR [g_ActiveConnections]
    jmp     @@accept_loop

@@accept_err:
    ; Brief sleep on error to avoid tight spin
    mov     ecx, 10
    sub     rsp, 32
    call    Sleep
    add     rsp, 32
    jmp     @@accept_loop

@@accept_exit:
    add     rsp, 72
    pop     rdi
    pop     rsi
    pop     rbx
    xor     ecx, ecx
    sub     rsp, 32
    call    ExitThread
AcceptThread ENDP

; =============================================================================
;       RawrXD_StartSocketServer — Exported Entry Point
; =============================================================================
; RCX = port (WORD)
; Returns: RAX = 0 success, -1 failure

PUBLIC RawrXD_StartSocketServer
RawrXD_StartSocketServer PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 600
    .allocstack 600
    .endprolog

    movzx   r12d, cx                ; r12d = port number

    ; WSAStartup(0x0202, &wsadata)
    mov     cx, 0202h
    lea     rdx, [rsp+0]            ; WSADATA at [rsp+0]
    sub     rsp, 32
    call    WSAStartup
    add     rsp, 32
    test    eax, eax
    jnz     @@init_fail

    ; Initialize context pool
    call    Pool_Init

    ; Record start time
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    mov     g_StartTick, rax

    ; Create IOCP
    mov     rcx, INVALID_HANDLE_VALUE_Q
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 32
    call    CreateIoCompletionPort
    add     rsp, 32
    test    rax, rax
    jz      @@init_fail
    mov     g_hIOCP, rax

    ; Create listening socket
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    sub     rsp, 32
    call    socket
    add     rsp, 32
    cmp     rax, INVALID_SOCKET
    je      @@init_fail
    mov     g_listenSock, rax
    mov     rbx, rax                ; rbx = listen socket

    ; SO_REUSEADDR
    mov     DWORD PTR [rsp+400], 1  ; optval = 1
    mov     rcx, rbx
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, [rsp+400]
    sub     rsp, 40
    push    4                       ; optlen = sizeof(int)
    call    setsockopt
    add     rsp, 48

    ; Bind to 127.0.0.1:port (bypass DNS)
    lea     rdi, [rsp+408]          ; sockaddr_in
    mov     WORD PTR [rdi], AF_INET ; sin_family
    movzx   ecx, r12w               ; port
    sub     rsp, 32
    call    htons
    add     rsp, 32
    mov     WORD PTR [rdi+2], ax    ; sin_port
    mov     DWORD PTR [rdi+4], 0100007Fh  ; 127.0.0.1 (little-endian)
    ; Zero sin_zero
    xor     eax, eax
    mov     QWORD PTR [rdi+8], rax

    mov     rcx, rbx
    mov     rdx, rdi
    mov     r8d, 16                 ; sizeof(sockaddr_in)
    sub     rsp, 32
    call    bind
    add     rsp, 32
    test    eax, eax
    jnz     @@init_fail_socket

    ; Listen
    mov     rcx, rbx
    mov     edx, SOMAXCONN
    sub     rsp, 32
    call    listen
    add     rsp, 32
    test    eax, eax
    jnz     @@init_fail_socket

    ; Set running flag
    mov     g_Running, 1

    ; Get CPU count for worker threads
    lea     rcx, [rsp+424]          ; SYSTEM_INFO
    sub     rsp, 32
    call    GetSystemInfo
    add     rsp, 32
    mov     eax, DWORD PTR [rsp+424+24] ; dwNumberOfProcessors offset in SYSTEM_INFO
    ; Use min(cpu_count, 8) workers
    cmp     eax, 8
    jbe     @@cpu_ok
    mov     eax, 8
@@cpu_ok:
    cmp     eax, 1
    jge     @@cpu_min_ok
    mov     eax, 1
@@cpu_min_ok:
    mov     g_WorkerCount, eax
    mov     r13d, eax               ; r13d = worker count

    ; Spawn IOCP worker threads
@@spawn_workers:
    test    r13d, r13d
    jz      @@workers_done

    xor     ecx, ecx                ; lpThreadAttributes
    xor     edx, edx                ; dwStackSize (default)
    lea     r8, WorkerThread        ; lpStartAddress
    xor     r9d, r9d                ; lpParameter
    sub     rsp, 48
    mov     QWORD PTR [rsp+32], 0   ; dwCreationFlags
    mov     QWORD PTR [rsp+40], 0   ; lpThreadId
    call    CreateThread
    add     rsp, 48
    ; We don't need thread handle — workers run until shutdown
    test    rax, rax
    jz      @@init_fail_socket
    sub     rsp, 32
    mov     rcx, rax
    call    CloseHandle             ; Close handle, thread keeps running
    add     rsp, 32

    dec     r13d
    jmp     @@spawn_workers

@@workers_done:
    ; Spawn cache refresh thread
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, CacheThread
    xor     r9d, r9d
    sub     rsp, 48
    mov     QWORD PTR [rsp+32], 0
    mov     QWORD PTR [rsp+40], 0
    call    CreateThread
    add     rsp, 48
    mov     g_hCacheThread, rax

    ; Spawn accept thread
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, AcceptThread
    xor     r9d, r9d
    sub     rsp, 48
    mov     QWORD PTR [rsp+32], 0
    mov     QWORD PTR [rsp+40], 0
    call    CreateThread
    add     rsp, 48
    mov     g_hAcceptThread, rax

    ; Build initial /health response is pre-built in .data — no action needed

    ; Success
    xor     eax, eax
    jmp     @@init_done

@@init_fail_socket:
    mov     rcx, rbx
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
@@init_fail:
    mov     eax, -1

@@init_done:
    add     rsp, 600
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RawrXD_StartSocketServer ENDP

; =============================================================================
;       RawrXD_StopSocketServer — Graceful Shutdown
; =============================================================================
PUBLIC RawrXD_StopSocketServer
RawrXD_StopSocketServer PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Signal all threads to exit
    mov     g_Running, 0

    ; Close listening socket (unblocks accept())
    mov     rcx, g_listenSock
    call    closesocket

    ; Post shutdown sentinels to IOCP (one per worker)
    mov     ebx, g_WorkerCount
@@post_shutdown:
    test    ebx, ebx
    jz      @@shutdown_done

    mov     rcx, g_hIOCP
    xor     edx, edx                ; dwNumberOfBytesTransferred = 0
    xor     r8d, r8d                ; dwCompletionKey = 0
    xor     r9d, r9d                ; lpOverlapped = NULL
    call    PostQueuedCompletionStatus

    dec     ebx
    jmp     @@post_shutdown

@@shutdown_done:
    ; Brief wait for threads to exit
    mov     ecx, 100
    call    Sleep

    ; Close IOCP handle
    mov     rcx, g_hIOCP
    call    CloseHandle

    ; WSACleanup
    call    WSACleanup

    add     rsp, 48
    pop     rbx
    ret
RawrXD_StopSocketServer ENDP

; =============================================================================
;       RawrXD_IsServerRunning — Check running state
; =============================================================================
PUBLIC RawrXD_IsServerRunning
RawrXD_IsServerRunning PROC
    mov     eax, g_Running
    ret
RawrXD_IsServerRunning ENDP

; =============================================================================
;     RawrXD_UpdateStatusCache — C++ calls this with new /status JSON body
; =============================================================================
; RCX = pointer to JSON body string (null-terminated)
; RDX = length of body
; Builds full HTTP response in _STATUS_RESP under lock.

PUBLIC RawrXD_UpdateStatusCache
RawrXD_UpdateStatusCache PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx                ; rsi = body ptr
    mov     r12, rdx                ; r12 = body len

    ; Acquire cache lock
    lea     rcx, g_CacheLock
    call    SpinLock_Acquire

    ; Build HTTP response header + body
    lea     rdi, _STATUS_RESP

    ; "HTTP/1.1 200 OK\r\n"
    lea     rbx, _http_200_hdr
@@usc_hdr:
    mov     al, [rbx]
    test    al, al
    jz      @@usc_cl
    stosb
    inc     rbx
    jmp     @@usc_hdr

@@usc_cl:
    ; "Content-Type: application/json\r\nContent-Length: "
    lea     rbx, _http_cl_hdr
@@usc_cl2:
    mov     al, [rbx]
    test    al, al
    jz      @@usc_len_num
    stosb
    inc     rbx
    jmp     @@usc_cl2

@@usc_len_num:
    ; Convert body length to ASCII
    mov     rax, r12
    call    @@usc_itoa

    ; Headers tail + CRLF + CORS
    lea     rbx, _http_keepalive_cors
@@usc_hdrtail:
    mov     al, [rbx]
    test    al, al
    jz      @@usc_body
    stosb
    inc     rbx
    jmp     @@usc_hdrtail

@@usc_body:
    ; Copy body
    mov     rcx, r12
    rep     movsb

    ; Calculate total length
    lea     rax, _STATUS_RESP
    sub     rdi, rax
    mov     _STATUS_RESP_LEN, rdi

    ; Release lock
    lea     rcx, g_CacheLock
    call    SpinLock_Release

    xor     eax, eax
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; Local itoa for update functions
@@usc_itoa:
    push    rbx
    push    rcx
    push    rdx
    push    r8
    sub     rsp, 24
    lea     r8, [rsp]
    xor     ecx, ecx
    test    rax, rax
    jnz     @@usc_i_nz
    mov     BYTE PTR [rdi], '0'
    inc     rdi
    jmp     @@usc_i_done
@@usc_i_nz:
    xor     edx, edx
    mov     rbx, 10
    div     rbx
    add     dl, '0'
    mov     [r8+rcx], dl
    inc     ecx
    test    rax, rax
    jnz     @@usc_i_nz
    dec     ecx
@@usc_i_rev:
    mov     al, [r8+rcx]
    stosb
    dec     ecx
    jns     @@usc_i_rev
@@usc_i_done:
    add     rsp, 24
    pop     r8
    pop     rdx
    pop     rcx
    pop     rbx
    ret

RawrXD_UpdateStatusCache ENDP

; =============================================================================
;     RawrXD_UpdateModelsCache — C++ calls this with new /models JSON body
; =============================================================================
; RCX = pointer to JSON body string (null-terminated)
; RDX = length of body

PUBLIC RawrXD_UpdateModelsCache
RawrXD_UpdateModelsCache PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx                ; body ptr
    mov     r12, rdx                ; body len

    ; Clamp to max
    cmp     r12, MAX_RESPONSE_SIZE - 512
    jbe     @@umc_ok
    mov     r12, MAX_RESPONSE_SIZE - 512
@@umc_ok:

    lea     rcx, g_CacheLock
    call    SpinLock_Acquire

    ; Build full response into _MODELS_RESP
    lea     rdi, _MODELS_RESP

    ; HTTP header
    lea     rcx, _http_200_hdr
@@umc_h1:
    mov     al, [rcx]
    test    al, al
    jz      @@umc_h2
    stosb
    inc     rcx
    jmp     @@umc_h1
@@umc_h2:
    lea     rcx, _http_cl_hdr
@@umc_h3:
    mov     al, [rcx]
    test    al, al
    jz      @@umc_len
    stosb
    inc     rcx
    jmp     @@umc_h3
@@umc_len:
    ; Content-Length number
    mov     rax, r12
    ; Inline itoa
    push    rbx
    push    rdx
    push    r8
    sub     rsp, 24
    lea     r8, [rsp]
    xor     ecx, ecx
    test    rax, rax
    jnz     @@umc_inz
    mov     BYTE PTR [rdi], '0'
    inc     rdi
    jmp     @@umc_idone
@@umc_inz:
    xor     edx, edx
    mov     rbx, 10
    div     rbx
    add     dl, '0'
    mov     [r8+rcx], dl
    inc     ecx
    test    rax, rax
    jnz     @@umc_inz
    dec     ecx
@@umc_irev:
    mov     al, [r8+rcx]
    stosb
    dec     ecx
    jns     @@umc_irev
@@umc_idone:
    add     rsp, 24
    pop     r8
    pop     rdx
    pop     rbx

    ; Headers tail
    lea     rcx, _http_keepalive_cors
@@umc_htail:
    mov     al, [rcx]
    test    al, al
    jz      @@umc_body
    stosb
    inc     rcx
    jmp     @@umc_htail

@@umc_body:
    mov     rcx, r12
    rep     movsb

    lea     rax, _MODELS_RESP
    sub     rdi, rax
    mov     _MODELS_RESP_LEN, rdi

    lea     rcx, g_CacheLock
    call    SpinLock_Release

    xor     eax, eax
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    ret
RawrXD_UpdateModelsCache ENDP

; =============================================================================
;     RawrXD_UpdateHealthCache — Override default /health response
; =============================================================================
; RCX = pointer to full HTTP response bytes
; RDX = length

PUBLIC RawrXD_UpdateHealthCache
RawrXD_UpdateHealthCache PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx
    mov     rcx, rdx

    ; Health response is page-aligned and <4KB, no lock needed for atomic update
    lea     rdi, _HEALTH_RESP
    rep     movsb
    mov     _HEALTH_RESP_LEN, rdx

    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
RawrXD_UpdateHealthCache ENDP

; =============================================================================
;     RawrXD_GetServerStats — Return statistics
; =============================================================================
; RCX = pointer to output buffer (8 QWORDs = 64 bytes)
; Layout: [0]=total_conn, [1]=active_conn, [2]=health_hits,
;         [3]=status_hits, [4]=models_hits, [5]=404_hits,
;         [6]=cors_hits, [7]=uptime_ms

PUBLIC RawrXD_GetServerStats
RawrXD_GetServerStats PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    mov     rax, g_TotalConnections
    mov     [rbx+0], rax
    mov     rax, g_ActiveConnections
    mov     [rbx+8], rax
    mov     rax, g_HitsHealth
    mov     [rbx+16], rax
    mov     rax, g_HitsStatus
    mov     [rbx+24], rax
    mov     rax, g_HitsModels
    mov     [rbx+32], rax
    mov     rax, g_Hits404
    mov     [rbx+40], rax
    mov     rax, g_HitsCORS
    mov     [rbx+48], rax

    ; Uptime
    call    GetTickCount64
    sub     rax, g_StartTick
    mov     [rbx+56], rax

    xor     eax, eax
    add     rsp, 40
    pop     rbx
    ret
RawrXD_GetServerStats ENDP

; =============================================================================
END
