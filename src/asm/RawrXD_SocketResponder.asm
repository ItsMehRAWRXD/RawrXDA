; =============================================================================
; RawrXD_SocketResponder.asm — IOCP Zero-Copy HTTP Responder
; =============================================================================
; Purpose: Sub-0.5ms HTTP server for /health, /status, /models endpoints.
;          Uses I/O Completion Ports (IOCP) for async socket handling,
;          pre-serialized static responses, pool allocator, and zero heap
;          allocation in the hot path.
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT in hot path
;
; Key Design:
;   - IOCP with N worker threads (1 per logical CPU)
;   - Pool allocator: 256 pre-allocated IOCTX structs (zero heap per request)
;   - SpinLock for pool (no kernel transition)
;   - Pre-serialized responses in .data — WorkerThread does zero formatting
;   - Lock-free hit counters via `lock inc`
;   - CacheThread rebuilds /status JSON every 10s (uptime + counters)
;
; Exports:
;   RawrXD_StartSocketServer(port)    — Init Winsock, IOCP, threads
;   RawrXD_StopSocketServer()         — Signal exit, cleanup
;   RawrXD_IsServerRunning()          — Poll running flag
;   RawrXD_UpdateHealthCache(data,len)
;   RawrXD_UpdateStatusCache(data,len)
;   RawrXD_UpdateModelsCache(data,len)
;   RawrXD_GetServerStats(pStats)     — Fill stats struct
;
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_SocketResponder.obj
;        link: ws2_32.lib kernel32.lib
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; =============================================================================
;                     Winsock Constants
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
INFINITE_WAIT           EQU     0FFFFFFFFh
WSA_IO_PENDING          EQU     997
ERROR_IO_PENDING        EQU     997

; =============================================================================
;                     Structures
; =============================================================================

WSADATA STRUCT
    wVersion        DW  ?
    wHighVersion    DW  ?
    iMaxSockets     DW  ?
    iMaxUdpDg       DW  ?
    lpVendorInfo    DQ  ?
    szDescription   DB  257 DUP(?)
    szSystemStatus  DB  129 DUP(?)
    _pad            DB  6 DUP(?)     ; Align to 8
WSADATA ENDS

sockaddr_in STRUCT
    sin_family      DW  ?
    sin_port        DW  ?
    sin_addr        DD  ?
    sin_zero        DB  8 DUP(0)
sockaddr_in ENDS

WSABUF STRUCT
    len_            DD  ?
    buf_            DQ  ?
WSABUF ENDS

; Must be FIRST member of IOCTX for IOCP casting
OVERLAPPED STRUCT
    Internal        DQ  ?
    InternalHigh    DQ  ?
    OffsetLow       DD  ?
    OffsetHigh      DD  ?
    hEvent          DQ  ?
OVERLAPPED ENDS

SYSTEM_INFO STRUCT
    wProcessorArchitecture    DW  ?
    wReserved                 DW  ?
    dwPageSize                DD  ?
    lpMinApplicationAddr      DQ  ?
    lpMaxApplicationAddr      DQ  ?
    dwActiveProcessorMask     DQ  ?
    dwNumberOfProcessors      DD  ?
    dwProcessorType           DD  ?
    dwAllocationGranularity   DD  ?
    wProcessorLevel           DW  ?
    wProcessorRevision        DW  ?
SYSTEM_INFO ENDS

; Per-connection I/O context
; OVERLAPPED must be at offset 0 so we can cast from IOCP result
RECV_BUF_SIZE   EQU     4096

IOCTX STRUCT
    ovl             OVERLAPPED  <>  ; Must be first — IOCP requirement
    wsaBuf          WSABUF      <>
    sockfd          DQ  ?
    state           DD  ?       ; 0=RECV, 1=SEND, 2=CLOSING
    endpoint_id     DD  ?       ; 0=health, 1=status, 2=models, 3=404, 5=CORS
    pNext           DQ  ?       ; Free-list link
    recv_buf        DB  RECV_BUF_SIZE DUP(?)
IOCTX ENDS

; State constants
STATE_RECV          EQU     0
STATE_SEND          EQU     1
STATE_CLOSING       EQU     2

; Endpoint IDs
EP_HEALTH           EQU     0
EP_STATUS           EQU     1
EP_MODELS           EQU     2
EP_404              EQU     3
EP_OPTIONS_CORS     EQU     5

; Pool size
MAX_POOL_SIZE       EQU     256

; =============================================================================
;                     Global Data
; =============================================================================
.data
ALIGN 16

; Server state
g_Running           DD  0
g_ListenSocket      DQ  INVALID_SOCKET
g_hIOCP             DQ  0
g_hAcceptThread     DQ  0
g_hCacheThread      DQ  0
g_WorkerThreads     DQ  64 DUP(0)      ; up to 64 worker thread handles
g_WorkerCount       DD  0
g_ServerPort        DD  0
g_StartTick         DQ  0              ; GetTickCount64 at start

; Atomic hit counters (lock-free via `lock inc`)
g_HitsHealth        DQ  0
g_HitsStatus        DQ  0
g_HitsModels        DQ  0
g_Hits404           DQ  0
g_HitsCORS          DQ  0
g_TotalConnections  DQ  0
g_ActiveConnections DQ  0

; Pool allocator
g_PoolBase          DQ  0              ; VirtualAlloc base
g_PoolFreeHead      DQ  0              ; Free-list head
g_PoolLock          DD  0              ; Spinlock

; Pre-serialized responses (page-aligned for zero-copy WSASend)
ALIGN 16
_HEALTH_HDR     DB  'HTTP/1.1 200 OK', 13, 10
                DB  'Content-Type: application/json', 13, 10
                DB  'Access-Control-Allow-Origin: *', 13, 10
                DB  'Connection: close', 13, 10
                DB  'Content-Length: 15', 13, 10
                DB  13, 10
_HEALTH_BODY    DB  '{"status":"ok"}', 0
_HEALTH_END     LABEL BYTE
HEALTH_RESP_LEN EQU (_HEALTH_END - _HEALTH_HDR)

ALIGN 16
_STATUS_HDR     DB  'HTTP/1.1 200 OK', 13, 10
                DB  'Content-Type: application/json', 13, 10
                DB  'Access-Control-Allow-Origin: *', 13, 10
                DB  'Connection: close', 13, 10
                DB  'Content-Length:     ', 13, 10  ; Patched by CacheThread
                DB  13, 10
_STATUS_BODY    DB  512 DUP(0)                      ; Rebuilt by CacheThread
_STATUS_END     LABEL BYTE

ALIGN 16
_MODELS_HDR     DB  'HTTP/1.1 200 OK', 13, 10
                DB  'Content-Type: application/json', 13, 10
                DB  'Access-Control-Allow-Origin: *', 13, 10
                DB  'Connection: close', 13, 10
                DB  'Content-Length:          ', 13, 10  ; Patched by update call
                DB  13, 10
_MODELS_BODY    DB  131072 DUP(0)                   ; 128KB max models JSON
_MODELS_END     LABEL BYTE
g_ModelsRespLen DD  0                               ; Actual length

ALIGN 16
_404_RESP       DB  'HTTP/1.1 404 Not Found', 13, 10
                DB  'Content-Type: text/plain', 13, 10
                DB  'Content-Length: 9', 13, 10
                DB  'Connection: close', 13, 10
                DB  13, 10
                DB  'Not Found', 0
_404_END        LABEL BYTE
RESP_404_LEN    EQU (_404_END - _404_RESP)

ALIGN 16
_CORS_RESP      DB  'HTTP/1.1 204 No Content', 13, 10
                DB  'Access-Control-Allow-Origin: *', 13, 10
                DB  'Access-Control-Allow-Methods: GET, POST, OPTIONS', 13, 10
                DB  'Access-Control-Allow-Headers: Content-Type, Authorization', 13, 10
                DB  'Access-Control-Max-Age: 86400', 13, 10
                DB  'Connection: close', 13, 10
                DB  13, 10, 0
_CORS_END       LABEL BYTE
RESP_CORS_LEN   EQU (_CORS_END - _CORS_RESP)

; Status response template (updated by CacheThread)
g_StatusRespLen DD  0

; Debug messages
sz_ServerStart  DB  '[SocketResponder] IOCP server starting on port ', 0
sz_ServerStop   DB  '[SocketResponder] Server stopped.', 13, 10, 0
sz_WorkerStart  DB  '[SocketResponder] Worker thread started.', 13, 10, 0
sz_AcceptStart  DB  '[SocketResponder] Accept thread started.', 13, 10, 0
sz_CacheStart   DB  '[SocketResponder] Cache thread started.', 13, 10, 0

; =============================================================================
;                     External API Declarations
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

EXTERNDEF CreateIoCompletionPort:PROC
EXTERNDEF GetQueuedCompletionStatus:PROC
EXTERNDEF PostQueuedCompletionStatus:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF GetSystemInfo:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF GetProcessHeap:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapFree:PROC

; =============================================================================
;                       CODE
; =============================================================================
.code

; =============================================================================
;     SpinLock_Acquire / SpinLock_Release
; =============================================================================
SpinLock_Acquire PROC
    ; RCX = pointer to lock DWORD
@@spin:
    xor     eax, eax
    mov     edx, 1
    lock cmpxchg DWORD PTR [rcx], edx
    jnz     @@spin_wait
    ret
@@spin_wait:
    pause
    cmp     DWORD PTR [rcx], 0
    jne     @@spin_wait
    jmp     @@spin
SpinLock_Acquire ENDP

SpinLock_Release PROC
    ; RCX = pointer to lock DWORD
    mov     DWORD PTR [rcx], 0
    ret
SpinLock_Release ENDP

; =============================================================================
;     Pool_Init — Pre-allocate IOCTX array, build free list
; =============================================================================
Pool_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; VirtualAlloc(NULL, MAX_POOL_SIZE * SIZEOF IOCTX, COMMIT|RESERVE, RW)
    xor     ecx, ecx
    mov     edx, MAX_POOL_SIZE
    imul    edx, SIZEOF IOCTX
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    sub     rsp, 32
    call    VirtualAlloc
    add     rsp, 32
    test    rax, rax
    jz      @@pi_fail

    mov     g_PoolBase, rax
    mov     rbx, rax             ; current IOCTX pointer
    
    ; Build free list: each IOCTX.pNext -> next, last -> NULL
    xor     esi, esi             ; counter
@@pi_link:
    cmp     esi, MAX_POOL_SIZE - 1
    jge     @@pi_last
    
    lea     rax, [rbx + SIZEOF IOCTX]   ; next node
    mov     [rbx].IOCTX.pNext, rax
    mov     [rbx].IOCTX.state, 0
    mov     [rbx].IOCTX.sockfd, INVALID_SOCKET
    add     rbx, SIZEOF IOCTX
    inc     esi
    jmp     @@pi_link

@@pi_last:
    mov     QWORD PTR [rbx].IOCTX.pNext, 0
    mov     [rbx].IOCTX.state, 0
    mov     [rbx].IOCTX.sockfd, INVALID_SOCKET

    ; Head of free list
    mov     rax, g_PoolBase
    mov     g_PoolFreeHead, rax
    
    xor     eax, eax             ; success
    jmp     @@pi_done

@@pi_fail:
    mov     eax, -1

@@pi_done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
Pool_Init ENDP

; =============================================================================
;     Pool_Get — Pop an IOCTX from free list (spinlock-protected)
; =============================================================================
; Returns: RAX = IOCTX* or NULL
Pool_Get PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    lea     rcx, g_PoolLock
    call    SpinLock_Acquire

    mov     rax, g_PoolFreeHead
    test    rax, rax
    jz      @@pg_empty

    ; Pop head
    mov     rbx, [rax].IOCTX.pNext
    mov     g_PoolFreeHead, rbx

    ; Zero the OVERLAPPED portion
    mov     QWORD PTR [rax].IOCTX.ovl.Internal, 0
    mov     QWORD PTR [rax].IOCTX.ovl.InternalHigh, 0
    mov     DWORD PTR [rax].IOCTX.ovl.OffsetLow, 0
    mov     DWORD PTR [rax].IOCTX.ovl.OffsetHigh, 0
    mov     QWORD PTR [rax].IOCTX.ovl.hEvent, 0
    mov     DWORD PTR [rax].IOCTX.state, STATE_RECV
    mov     QWORD PTR [rax].IOCTX.pNext, 0

    push    rax
    lea     rcx, g_PoolLock
    call    SpinLock_Release
    pop     rax
    jmp     @@pg_done

@@pg_empty:
    lea     rcx, g_PoolLock
    call    SpinLock_Release
    xor     eax, eax

@@pg_done:
    add     rsp, 32
    pop     rbx
    ret
Pool_Get ENDP

; =============================================================================
;     Pool_Put — Push an IOCTX back to free list
; =============================================================================
; RCX = IOCTX* to return
Pool_Put PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx             ; save IOCTX*

    ; Close socket if still open
    mov     rcx, [rbx].IOCTX.sockfd
    cmp     rcx, INVALID_SOCKET
    je      @@pp_closed
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
    mov     [rbx].IOCTX.sockfd, INVALID_SOCKET
@@pp_closed:

    lea     rcx, g_PoolLock
    call    SpinLock_Acquire

    ; Push to head of free list
    mov     rax, g_PoolFreeHead
    mov     [rbx].IOCTX.pNext, rax
    mov     [rbx].IOCTX.state, 0
    mov     g_PoolFreeHead, rbx

    lea     rcx, g_PoolLock
    call    SpinLock_Release

    lock dec QWORD PTR [g_ActiveConnections]

    add     rsp, 32
    pop     rbx
    ret
Pool_Put ENDP

; =============================================================================
;     MatchEndpoint — Parse HTTP request, return endpoint ID
; =============================================================================
; RCX = recv buffer pointer, EDX = received length
; Returns: EAX = endpoint ID (EP_HEALTH, EP_STATUS, etc.)

MatchEndpoint PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx

    ; Check for OPTIONS method (CORS preflight)
    cmp     DWORD PTR [rbx], 'OPTI'
    jne     @@me_get

    mov     eax, EP_OPTIONS_CORS
    jmp     @@me_done

@@me_get:
    ; Must start with "GET "
    cmp     DWORD PTR [rbx], 'GET '
    je      @@me_parse_path

    ; POST or other — skip method to find space before path
    xor     ecx, ecx
@@me_find_space:
    cmp     ecx, edx
    jge     @@me_404
    cmp     BYTE PTR [rbx + rcx], ' '
    je      @@me_found_space
    inc     ecx
    jmp     @@me_find_space

@@me_found_space:
    inc     ecx
    lea     rbx, [rbx + rcx]
    jmp     @@me_check_path

@@me_parse_path:
    add     rbx, 4               ; skip "GET "

@@me_check_path:
    ; Check /health (7 chars)
    cmp     DWORD PTR [rbx], '/hea'
    jne     @@me_check_status
    cmp     WORD PTR [rbx+4], 'lt'
    jne     @@me_check_status
    cmp     BYTE PTR [rbx+6], 'h'
    jne     @@me_check_status
    mov     eax, EP_HEALTH
    jmp     @@me_done

@@me_check_status:
    ; Check /status (7 chars)
    cmp     DWORD PTR [rbx], '/sta'
    jne     @@me_check_models
    cmp     WORD PTR [rbx+4], 'tu'
    jne     @@me_check_models
    cmp     BYTE PTR [rbx+6], 's'
    jne     @@me_check_models
    mov     eax, EP_STATUS
    jmp     @@me_done

@@me_check_models:
    ; Check /models (7 chars) or /v1/models (10 chars)
    cmp     DWORD PTR [rbx], '/mod'
    jne     @@me_check_v1models
    cmp     WORD PTR [rbx+4], 'el'
    jne     @@me_check_v1models
    cmp     BYTE PTR [rbx+6], 's'
    jne     @@me_check_v1models
    mov     eax, EP_MODELS
    jmp     @@me_done

@@me_check_v1models:
    cmp     DWORD PTR [rbx], '/v1/'
    jne     @@me_404
    cmp     DWORD PTR [rbx+4], 'mode'
    jne     @@me_404
    cmp     WORD PTR [rbx+8], 'ls'
    jne     @@me_404
    mov     eax, EP_MODELS
    jmp     @@me_done

@@me_404:
    mov     eax, EP_404

@@me_done:
    add     rsp, 32
    pop     rbx
    ret
MatchEndpoint ENDP

; =============================================================================
;     WorkerThread — IOCP worker loop
; =============================================================================
; LPVOID parameter (unused) in RCX

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
    sub     rsp, 120
    .allocstack 120
    .endprolog

@@wt_loop:
    cmp     g_Running, 0
    je      @@wt_exit

    ; GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &compKey, &pOvl, INFINITE)
    mov     rcx, g_hIOCP
    lea     rdx, [rsp + 0]          ; &bytesTransferred (DWORD)
    lea     r8, [rsp + 8]           ; &completionKey (ULONG_PTR)
    lea     r9, [rsp + 16]          ; &pOverlapped
    sub     rsp, 48
    mov     DWORD PTR [rsp + 32], INFINITE_WAIT
    call    GetQueuedCompletionStatus
    add     rsp, 48

    ; Check for shutdown sentinel (compKey == 0xDEAD)
    mov     r12, [rsp + 8]          ; completionKey
    cmp     r12, 0DEADh
    je      @@wt_exit

    ; Check for error
    test    eax, eax
    jz      @@wt_error

    mov     r13d, [rsp + 0]         ; bytesTransferred
    mov     r14, [rsp + 16]         ; pOverlapped = IOCTX*

    ; If zero bytes transferred on receive, client disconnected
    test    r13d, r13d
    jz      @@wt_disconnect

    ; IOCTX* is r14 (OVERLAPPED is at offset 0)
    cmp     [r14].IOCTX.state, STATE_RECV
    je      @@wt_process_recv
    cmp     [r14].IOCTX.state, STATE_SEND
    je      @@wt_send_done
    jmp     @@wt_loop

@@wt_process_recv:
    ; Parse the HTTP request to find endpoint
    lea     rcx, [r14].IOCTX.recv_buf
    mov     edx, r13d
    call    MatchEndpoint
    mov     [r14].IOCTX.endpoint_id, eax

    ; Select response buffer and length based on endpoint
    cmp     eax, EP_HEALTH
    je      @@wt_resp_health
    cmp     eax, EP_STATUS
    je      @@wt_resp_status
    cmp     eax, EP_MODELS
    je      @@wt_resp_models
    cmp     eax, EP_OPTIONS_CORS
    je      @@wt_resp_cors
    jmp     @@wt_resp_404

@@wt_resp_health:
    lock inc QWORD PTR [g_HitsHealth]
    lea     rsi, _HEALTH_HDR
    mov     edi, HEALTH_RESP_LEN
    jmp     @@wt_do_send

@@wt_resp_status:
    lock inc QWORD PTR [g_HitsStatus]
    lea     rsi, _STATUS_HDR
    mov     edi, g_StatusRespLen
    test    edi, edi
    jnz     @@wt_do_send
    ; Fallback to health if status not yet built
    lea     rsi, _HEALTH_HDR
    mov     edi, HEALTH_RESP_LEN
    jmp     @@wt_do_send

@@wt_resp_models:
    lock inc QWORD PTR [g_HitsModels]
    lea     rsi, _MODELS_HDR
    mov     edi, g_ModelsRespLen
    test    edi, edi
    jnz     @@wt_do_send
    ; Empty models list fallback
    lea     rsi, _404_RESP
    mov     edi, RESP_404_LEN
    jmp     @@wt_do_send

@@wt_resp_cors:
    lock inc QWORD PTR [g_HitsCORS]
    lea     rsi, _CORS_RESP
    mov     edi, RESP_CORS_LEN
    jmp     @@wt_do_send

@@wt_resp_404:
    lock inc QWORD PTR [g_Hits404]
    lea     rsi, _404_RESP
    mov     edi, RESP_404_LEN

@@wt_do_send:
    ; Set up WSABUF for send (zero-copy from static data)
    mov     [r14].IOCTX.wsaBuf.buf_, rsi
    mov     [r14].IOCTX.wsaBuf.len_, edi
    mov     [r14].IOCTX.state, STATE_SEND

    ; Zero OVERLAPPED for reuse
    mov     QWORD PTR [r14].IOCTX.ovl.Internal, 0
    mov     QWORD PTR [r14].IOCTX.ovl.InternalHigh, 0
    mov     DWORD PTR [r14].IOCTX.ovl.OffsetLow, 0
    mov     DWORD PTR [r14].IOCTX.ovl.OffsetHigh, 0
    mov     QWORD PTR [r14].IOCTX.ovl.hEvent, 0

    ; WSASend(sockfd, &wsaBuf, 1, NULL, 0, &ovl, NULL)
    mov     rcx, [r14].IOCTX.sockfd
    lea     rdx, [r14].IOCTX.wsaBuf
    mov     r8d, 1                  ; buffer count
    xor     r9d, r9d                ; lpBytesSent (NULL for async)
    sub     rsp, 56
    mov     DWORD PTR [rsp + 32], 0 ; flags
    lea     rax, [r14].IOCTX.ovl
    mov     [rsp + 40], rax         ; lpOverlapped
    mov     QWORD PTR [rsp + 48], 0 ; lpCompletionRoutine
    call    WSASend
    add     rsp, 56

    cmp     eax, SOCKET_ERROR
    jne     @@wt_loop
    sub     rsp, 32
    call    WSAGetLastError
    add     rsp, 32
    cmp     eax, WSA_IO_PENDING
    je      @@wt_loop
    ; Send failed — recycle context
    mov     rcx, r14
    call    Pool_Put
    jmp     @@wt_loop

@@wt_send_done:
    ; Send complete — close connection and recycle
    mov     rcx, [r14].IOCTX.sockfd
    mov     edx, SD_BOTH
    sub     rsp, 32
    call    shutdown
    add     rsp, 32
    mov     rcx, r14
    call    Pool_Put
    jmp     @@wt_loop

@@wt_disconnect:
    mov     rcx, r14
    call    Pool_Put
    jmp     @@wt_loop

@@wt_error:
    ; GQCS failed — if pOverlapped is non-NULL, recycle it
    mov     rax, [rsp + 16]
    test    rax, rax
    jz      @@wt_loop
    mov     rcx, rax
    call    Pool_Put
    jmp     @@wt_loop

@@wt_exit:
    xor     eax, eax
    add     rsp, 120
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
WorkerThread ENDP

; =============================================================================
;     AcceptThread — Accept loop, assign to IOCP
; =============================================================================

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

@@at_loop:
    cmp     g_Running, 0
    je      @@at_exit

    ; accept(listenSocket, NULL, NULL)
    mov     rcx, g_ListenSocket
    xor     edx, edx
    xor     r8d, r8d
    sub     rsp, 32
    call    accept
    add     rsp, 32
    cmp     rax, INVALID_SOCKET
    je      @@at_loop              ; accept can fail on shutdown

    mov     rbx, rax               ; client socket
    lock inc QWORD PTR [g_TotalConnections]
    lock inc QWORD PTR [g_ActiveConnections]

    ; Get an IOCTX from pool
    call    Pool_Get
    test    rax, rax
    jz      @@at_pool_full

    mov     rsi, rax               ; IOCTX*
    mov     [rsi].IOCTX.sockfd, rbx
    mov     [rsi].IOCTX.state, STATE_RECV

    ; Associate client socket with IOCP
    ; CreateIoCompletionPort(clientSocket, hIOCP, compKey, 0)
    mov     rcx, rbx               ; FileHandle (socket)
    mov     rdx, g_hIOCP           ; ExistingCompletionPort
    mov     r8, rsi                ; CompletionKey = IOCTX*
    xor     r9d, r9d               ; 0 threads
    sub     rsp, 32
    call    CreateIoCompletionPort
    add     rsp, 32
    test    rax, rax
    jz      @@at_assoc_fail

    ; Set up initial WSARecv
    lea     rax, [rsi].IOCTX.recv_buf
    mov     [rsi].IOCTX.wsaBuf.buf_, rax
    mov     [rsi].IOCTX.wsaBuf.len_, RECV_BUF_SIZE

    ; WSARecv(sockfd, &wsaBuf, 1, NULL, &flags, &ovl, NULL)
    mov     rcx, rbx
    lea     rdx, [rsi].IOCTX.wsaBuf
    mov     r8d, 1                  ; bufferCount
    xor     r9d, r9d                ; lpBytesRecvd (NULL for async)
    sub     rsp, 56
    lea     rax, [rsp + 56 + 0]     ; safe stack slot for flags DWORD
    mov     DWORD PTR [rax], 0
    mov     [rsp + 32], rax         ; lpFlags
    lea     rax, [rsi].IOCTX.ovl
    mov     [rsp + 40], rax         ; lpOverlapped
    mov     QWORD PTR [rsp + 48], 0 ; lpCompletionRoutine
    call    WSARecv
    add     rsp, 56

    cmp     eax, SOCKET_ERROR
    jne     @@at_loop
    sub     rsp, 32
    call    WSAGetLastError
    add     rsp, 32
    cmp     eax, WSA_IO_PENDING
    je      @@at_loop
    ; Recv failed immediately
    mov     rcx, rsi
    call    Pool_Put
    jmp     @@at_loop

@@at_pool_full:
    ; Pool exhausted — close the client socket
    mov     rcx, rbx
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
    lock dec QWORD PTR [g_ActiveConnections]
    jmp     @@at_loop

@@at_assoc_fail:
    ; IOCP association failed
    mov     rcx, rsi
    call    Pool_Put
    jmp     @@at_loop

@@at_exit:
    xor     eax, eax
    add     rsp, 72
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AcceptThread ENDP

; =============================================================================
;     CacheThread — Rebuild /status JSON every 10 seconds
; =============================================================================

CacheThread PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 88
    .allocstack 88
    .endprolog

@@ct_loop:
    cmp     g_Running, 0
    je      @@ct_exit

    ; Sleep 10000ms (10s)
    mov     ecx, 10000
    sub     rsp, 32
    call    Sleep
    add     rsp, 32

    cmp     g_Running, 0
    je      @@ct_exit

    ; Compute uptime
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    sub     rax, g_StartTick
    xor     edx, edx
    mov     rcx, 1000
    div     rcx                     ; RAX = uptime seconds
    mov     rbx, rax                ; save uptime

    ; Build JSON: {"uptime":N,"health":N,"status":N,"models":N,"conns":N}
    lea     rdi, _STATUS_BODY
    
    ; Write prefix
    lea     rsi, @@ct_json_pre
    call    @@ct_strcpy

    ; Append uptime value
    mov     rax, rbx
    call    @@ct_itoa64
    
    ; ,"health":
    lea     rsi, @@ct_health_key
    call    @@ct_strcpy
    mov     rax, g_HitsHealth
    call    @@ct_itoa64

    ; ,"status":
    lea     rsi, @@ct_status_key
    call    @@ct_strcpy
    mov     rax, g_HitsStatus
    call    @@ct_itoa64

    ; ,"models":
    lea     rsi, @@ct_models_key
    call    @@ct_strcpy
    mov     rax, g_HitsModels
    call    @@ct_itoa64

    ; ,"conns":
    lea     rsi, @@ct_conns_key
    call    @@ct_strcpy
    mov     rax, g_ActiveConnections
    call    @@ct_itoa64

    ; Closing brace
    mov     BYTE PTR [rdi], '}'
    inc     rdi
    mov     BYTE PTR [rdi], 0

    ; Calculate body length
    lea     rax, _STATUS_BODY
    mov     rcx, rdi
    sub     rcx, rax                ; body length in ecx

    ; Build full response length = header size + body size
    lea     rax, _STATUS_BODY
    lea     rdx, _STATUS_HDR
    sub     rax, rdx                ; header length
    add     eax, ecx               ; total length

    mov     g_StatusRespLen, eax

    jmp     @@ct_loop

@@ct_exit:
    xor     eax, eax
    add     rsp, 88
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; Local helper: copy string RSI->RDI, advance RDI
@@ct_strcpy:
    lodsb
    test    al, al
    jz      @@ct_sc_done
    stosb
    jmp     @@ct_strcpy
@@ct_sc_done:
    ret

; Local helper: write RAX as decimal to [RDI], advance RDI
@@ct_itoa64:
    push    rbx
    push    rcx
    push    rdx
    xor     ecx, ecx               ; digit count
    test    rax, rax
    jnz     @@ct_i_divloop
    ; Zero case
    mov     BYTE PTR [rdi], '0'
    inc     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    ret
@@ct_i_divloop:
    test    rax, rax
    jz      @@ct_i_emit
    xor     edx, edx
    mov     rbx, 10
    div     rbx                     ; RAX = quotient, RDX = remainder
    add     dl, '0'
    push    rdx                     ; push digit char
    inc     ecx
    jmp     @@ct_i_divloop
@@ct_i_emit:
    test    ecx, ecx
    jz      @@ct_i_done
    pop     rdx
    mov     BYTE PTR [rdi], dl
    inc     rdi
    dec     ecx
    jmp     @@ct_i_emit
@@ct_i_done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Inline string data for JSON builder
@@ct_json_pre       DB '{"uptime":', 0
@@ct_health_key     DB ',"health":', 0
@@ct_status_key     DB ',"status":', 0
@@ct_models_key     DB ',"models":', 0
@@ct_conns_key      DB ',"conns":', 0

CacheThread ENDP

; =============================================================================
;     RawrXD_StartSocketServer — Main entry point
; =============================================================================
; ECX = port number
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
    sub     rsp, 136
    .allocstack 136
    .endprolog

    mov     g_ServerPort, ecx
    mov     r12d, ecx               ; save port

    ; Record start time
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    mov     g_StartTick, rax

    ; Initialize pool allocator
    call    Pool_Init
    test    eax, eax
    jnz     @@ss_fail

    ; WSAStartup(MAKEWORD(2,2), &wsaData)
    mov     ecx, 0202h
    lea     rdx, [rsp + 0]          ; WSADATA on stack
    sub     rsp, 32
    call    WSAStartup
    add     rsp, 32
    test    eax, eax
    jnz     @@ss_fail

    ; Create listen socket
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    sub     rsp, 32
    call    socket
    add     rsp, 32
    cmp     rax, INVALID_SOCKET
    je      @@ss_fail_wsa
    mov     g_ListenSocket, rax
    mov     rbx, rax

    ; setsockopt(SO_REUSEADDR)
    mov     ecx, 1
    mov     [rsp + 0], ecx          ; optval = 1
    mov     rcx, rbx
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, [rsp + 0]
    sub     rsp, 48
    mov     DWORD PTR [rsp + 32], 4
    call    setsockopt
    add     rsp, 48

    ; Set up sockaddr_in for bind
    lea     rsi, [rsp + 16]
    mov     WORD PTR [rsi], AF_INET ; sin_family
    movzx   ecx, WORD PTR r12d
    sub     rsp, 32
    call    htons
    add     rsp, 32
    lea     rsi, [rsp + 16]
    mov     WORD PTR [rsi + 2], ax  ; sin_port
    mov     DWORD PTR [rsi + 4], 0  ; INADDR_ANY
    mov     QWORD PTR [rsi + 8], 0  ; sin_zero

    ; bind(listenSocket, &addr, sizeof(sockaddr_in))
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, 16
    sub     rsp, 32
    call    bind
    add     rsp, 32
    cmp     eax, SOCKET_ERROR
    je      @@ss_fail_sock

    ; listen(listenSocket, SOMAXCONN)
    mov     rcx, rbx
    mov     edx, SOMAXCONN
    sub     rsp, 32
    call    listen
    add     rsp, 32
    cmp     eax, SOCKET_ERROR
    je      @@ss_fail_sock

    ; Create IOCP
    mov     rcx, INVALID_HANDLE_VALUE
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 32
    call    CreateIoCompletionPort
    add     rsp, 32
    test    rax, rax
    jz      @@ss_fail_sock
    mov     g_hIOCP, rax

    ; Associate listen socket with IOCP
    mov     rcx, rbx
    mov     rdx, g_hIOCP
    xor     r8d, r8d
    xor     r9d, r9d
    sub     rsp, 32
    call    CreateIoCompletionPort
    add     rsp, 32

    ; Get CPU count for worker threads
    lea     rcx, [rsp + 48]         ; SYSTEM_INFO on stack
    sub     rsp, 32
    call    GetSystemInfo
    add     rsp, 32
    mov     eax, DWORD PTR [rsp + 48 + 24]  ; dwNumberOfProcessors
    cmp     eax, 64
    jbe     @@ss_cpu_ok
    mov     eax, 64
@@ss_cpu_ok:
    cmp     eax, 2
    jge     @@ss_cpu_min
    mov     eax, 2
@@ss_cpu_min:
    mov     g_WorkerCount, eax
    mov     esi, eax

    ; Mark server as running BEFORE spawning threads
    mov     g_Running, 1

    ; Spawn worker threads
    xor     edi, edi
@@ss_spawn_workers:
    cmp     edi, esi
    jge     @@ss_accept_thread

    xor     ecx, ecx                ; lpThreadAttributes
    xor     edx, edx                ; dwStackSize
    lea     r8, WorkerThread        ; lpStartAddress
    xor     r9, r9                  ; lpParameter
    sub     rsp, 48
    mov     DWORD PTR [rsp + 32], 0 ; dwCreationFlags
    mov     QWORD PTR [rsp + 40], 0 ; lpThreadId
    call    CreateThread
    add     rsp, 48
    mov     [g_WorkerThreads + rdi*8], rax
    inc     edi
    jmp     @@ss_spawn_workers

@@ss_accept_thread:
    ; Spawn accept thread
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, AcceptThread
    xor     r9, r9
    sub     rsp, 48
    mov     DWORD PTR [rsp + 32], 0
    mov     QWORD PTR [rsp + 40], 0
    call    CreateThread
    add     rsp, 48
    mov     g_hAcceptThread, rax

    ; Spawn cache thread
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, CacheThread
    xor     r9, r9
    sub     rsp, 48
    mov     DWORD PTR [rsp + 32], 0
    mov     QWORD PTR [rsp + 40], 0
    call    CreateThread
    add     rsp, 48
    mov     g_hCacheThread, rax

    xor     eax, eax                ; success
    jmp     @@ss_done

@@ss_fail_sock:
    mov     rcx, g_ListenSocket
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
    mov     g_ListenSocket, INVALID_SOCKET

@@ss_fail_wsa:
    sub     rsp, 32
    call    WSACleanup
    add     rsp, 32

@@ss_fail:
    mov     eax, -1

@@ss_done:
    add     rsp, 136
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RawrXD_StartSocketServer ENDP

; =============================================================================
;     RawrXD_StopSocketServer
; =============================================================================

PUBLIC RawrXD_StopSocketServer
RawrXD_StopSocketServer PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 56
    .allocstack 56
    .endprolog

    mov     g_Running, 0

    ; Close listen socket to unblock accept()
    mov     rcx, g_ListenSocket
    cmp     rcx, INVALID_SOCKET
    je      @@stop_no_listen
    sub     rsp, 32
    call    closesocket
    add     rsp, 32
    mov     g_ListenSocket, INVALID_SOCKET
@@stop_no_listen:

    ; Post shutdown sentinels to all workers
    mov     esi, g_WorkerCount
    xor     ebx, ebx
@@stop_post:
    cmp     ebx, esi
    jge     @@stop_wait
    mov     rcx, g_hIOCP
    xor     edx, edx
    mov     r8, 0DEADh              ; completion key = sentinel
    xor     r9, r9
    sub     rsp, 32
    call    PostQueuedCompletionStatus
    add     rsp, 32
    inc     ebx
    jmp     @@stop_post

@@stop_wait:
    ; Brief sleep to let threads exit
    mov     ecx, 500
    sub     rsp, 32
    call    Sleep
    add     rsp, 32

    ; Close worker thread handles
    xor     ebx, ebx
@@stop_close_workers:
    cmp     ebx, esi
    jge     @@stop_close_accept
    mov     rcx, [g_WorkerThreads + rbx*8]
    test    rcx, rcx
    jz      @@stop_next_worker
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
@@stop_next_worker:
    inc     ebx
    jmp     @@stop_close_workers

@@stop_close_accept:
    mov     rcx, g_hAcceptThread
    test    rcx, rcx
    jz      @@stop_close_cache
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
@@stop_close_cache:
    mov     rcx, g_hCacheThread
    test    rcx, rcx
    jz      @@stop_close_iocp
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32

@@stop_close_iocp:
    mov     rcx, g_hIOCP
    test    rcx, rcx
    jz      @@stop_free_pool
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     g_hIOCP, 0

@@stop_free_pool:
    ; Free pool memory
    mov     rcx, g_PoolBase
    test    rcx, rcx
    jz      @@stop_wsa
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    sub     rsp, 32
    call    VirtualFree
    add     rsp, 32
    mov     g_PoolBase, 0

@@stop_wsa:
    sub     rsp, 32
    call    WSACleanup
    add     rsp, 32

    xor     eax, eax
    add     rsp, 56
    pop     rsi
    pop     rbx
    ret
RawrXD_StopSocketServer ENDP

; =============================================================================
;     RawrXD_IsServerRunning
; =============================================================================

PUBLIC RawrXD_IsServerRunning
RawrXD_IsServerRunning PROC
    mov     eax, g_Running
    ret
RawrXD_IsServerRunning ENDP

; =============================================================================
;     RawrXD_UpdateHealthCache
; =============================================================================
; RCX = data (ANSI JSON), EDX = length

PUBLIC RawrXD_UpdateHealthCache
RawrXD_UpdateHealthCache PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 24
    .allocstack 24
    .endprolog

    mov     rsi, rcx
    mov     ecx, edx
    lea     rdi, _HEALTH_BODY
    rep     movsb
    mov     BYTE PTR [rdi], 0

    xor     eax, eax
    add     rsp, 24
    pop     rdi
    pop     rsi
    ret
RawrXD_UpdateHealthCache ENDP

; =============================================================================
;     RawrXD_UpdateStatusCache
; =============================================================================
; RCX = data (ANSI JSON), EDX = length

PUBLIC RawrXD_UpdateStatusCache
RawrXD_UpdateStatusCache PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 24
    .allocstack 24
    .endprolog

    mov     rsi, rcx
    mov     ecx, edx
    push    rdx                     ; save original length
    lea     rdi, _STATUS_BODY
    rep     movsb
    mov     BYTE PTR [rdi], 0
    pop     rdx

    ; Update total response length: header_size + body_size
    lea     rax, _STATUS_BODY
    lea     rcx, _STATUS_HDR
    sub     rax, rcx                ; header size
    add     eax, edx               ; + body size
    mov     g_StatusRespLen, eax

    xor     eax, eax
    add     rsp, 24
    pop     rdi
    pop     rsi
    ret
RawrXD_UpdateStatusCache ENDP

; =============================================================================
;     RawrXD_UpdateModelsCache
; =============================================================================
; RCX = data (ANSI JSON), EDX = length (max 128KB)

PUBLIC RawrXD_UpdateModelsCache
RawrXD_UpdateModelsCache PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 24
    .allocstack 24
    .endprolog

    ; Clamp to 128KB
    cmp     edx, 131072
    jbe     @@umc_ok
    mov     edx, 131072
@@umc_ok:
    mov     rsi, rcx
    mov     ecx, edx
    push    rcx
    lea     rdi, _MODELS_BODY
    rep     movsb
    mov     BYTE PTR [rdi], 0
    pop     rcx

    ; Calculate total response length
    lea     rax, _MODELS_BODY
    lea     rdx, _MODELS_HDR
    sub     rax, rdx               ; header length
    add     eax, ecx               ; + body length
    mov     g_ModelsRespLen, eax

    xor     eax, eax
    add     rsp, 24
    pop     rdi
    pop     rsi
    ret
RawrXD_UpdateModelsCache ENDP

; =============================================================================
;     RawrXD_GetServerStats — Fill 64-byte stats structure
; =============================================================================
; RCX = pointer to stats struct:
;   [+0]  hitsHealth (u64), [+8] hitsStatus, [+16] hitsModels,
;   [+24] hits404, [+32] hitsCORS, [+40] totalConnections,
;   [+48] activeConnections, [+56] uptimeSeconds

PUBLIC RawrXD_GetServerStats
RawrXD_GetServerStats PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    mov     rax, g_HitsHealth
    mov     [rbx + 0], rax
    mov     rax, g_HitsStatus
    mov     [rbx + 8], rax
    mov     rax, g_HitsModels
    mov     [rbx + 16], rax
    mov     rax, g_Hits404
    mov     [rbx + 24], rax
    mov     rax, g_HitsCORS
    mov     [rbx + 32], rax
    mov     rax, g_TotalConnections
    mov     [rbx + 40], rax
    mov     rax, g_ActiveConnections
    mov     [rbx + 48], rax

    ; Compute uptime
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    sub     rax, g_StartTick
    xor     edx, edx
    mov     rcx, 1000
    div     rcx
    mov     [rbx + 56], rax

    xor     eax, eax
    add     rsp, 40
    pop     rbx
    ret
RawrXD_GetServerStats ENDP

; =============================================================================
END
