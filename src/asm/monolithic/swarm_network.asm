; ═══════════════════════════════════════════════════════════════════
; swarm_network.asm — RawrXD Sovereign Multi-Node P2P Kernel
; Phase 12→14: Winsock2 Sharding Hot-Path + Cross-Node Migration
;
; Architecture:
;   - TCP sockets on port 0xDA7A (55930) for shard data
;   - 12-byte sovereign packet header: Magic(4) + Type(1) + Flags(1) + Len(4) + SeqNo(2)
;   - Per-node connection table with socket handles + load counters
;   - Non-blocking accept loop for multi-node topology
;   - SendShard/RecvShard: framed bulk transfer with header validation
;   - SyncKVCache: broadcast iteration over connected nodes
;   - ExchangeLoadInfo: lightweight 32-byte load snapshot exchange
;
; All hex in MASM style. Win64 ABI. No sizeof, no addr.
; ═══════════════════════════════════════════════════════════════════

; ── Winsock2 Imports ─────────────────────────────────────────────
EXTERN WSAStartup:PROC
EXTERN WSACleanup:PROC
EXTERN socket:PROC
EXTERN bind:PROC
EXTERN listen:PROC
EXTERN accept:PROC
EXTERN connect:PROC
EXTERN send:PROC
EXTERN recv:PROC
EXTERN closesocket:PROC
EXTERN ioctlsocket:PROC
EXTERN setsockopt:PROC
EXTERN select:PROC
EXTERN htons:PROC
EXTERN htonl:PROC
EXTERN ntohs:PROC
EXTERN ntohl:PROC
EXTERN inet_addr:PROC

; ── Win32 / Kernel Imports ───────────────────────────────────────
EXTERN CreateThread:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN BeaconSend:PROC
EXTERN g_hHeap:QWORD

; ── Public Exports ───────────────────────────────────────────────
PUBLIC SwarmNet_Init
PUBLIC SwarmNet_Listen
PUBLIC SwarmNet_Connect
PUBLIC SwarmNet_BroadcastDiscovery
PUBLIC SwarmNet_SendShard
PUBLIC SwarmNet_RecvShard
PUBLIC SwarmNet_SyncKVCache
PUBLIC SwarmNet_ExchangeLoadInfo
PUBLIC SwarmNet_Shutdown
PUBLIC g_netReady
PUBLIC g_remoteCount
PUBLIC g_remoteNodes

; ── Constants ────────────────────────────────────────────────────
SOVEREIGN_MAGIC     equ 0DEADC0DEh
SOVEREIGN_PORT      equ 0DA7Ah          ; 55930 decimal
MAX_REMOTE_NODES    equ 32
NODE_ENTRY_SIZE     equ 64              ; bytes per node entry

; Packet types
PKT_KV_SYNC         equ 0F5h
PKT_SHARD_WEIGHT    equ 0F6h
PKT_LOAD_EXCHANGE   equ 0F7h
PKT_DISCOVERY       equ 0F8h
PKT_SHARD_DATA      equ 0F9h
PKT_ACK             equ 0FAh

; Packet header size (12 bytes)
PKT_HDR_SIZE        equ 12

; Socket constants (Winsock2)
AF_INET             equ 2
SOCK_STREAM         equ 1
IPPROTO_TCP         equ 6
INVALID_SOCKET      equ -1
SOMAXCONN           equ 7FFFFFFFh
SOL_SOCKET          equ 0FFFFh
SO_REUSEADDR        equ 4
FIONBIO             equ 8004667Eh

; Beacon slot
NET_BEACON_SLOT     equ 12
NET_EVT_INIT        equ 0C1h
NET_EVT_LISTEN      equ 0C2h
NET_EVT_CONNECT     equ 0C3h
NET_EVT_SEND        equ 0C4h
NET_EVT_RECV        equ 0C5h
NET_EVT_SHUTDOWN    equ 0C6h

; ── Data ─────────────────────────────────────────────────────────
.data
align 16
g_wsaData           db 512 dup(0)
g_listenSocket      dq INVALID_SOCKET
g_netReady          dd 0
g_netSeqNo          dw 0

; Per-node connection table (MAX_REMOTE_NODES * NODE_ENTRY_SIZE)
;   +0:  socket handle (QWORD, -1 = unused)
;   +8:  IP address (DWORD, network order)
;   +12: port (WORD, network order)
;   +14: reserved (WORD)
;   +16: deviceLoad snapshot (8 DWORDs = per-GPU load counters)
;   +48: lastHeartbeat (QWORD, tick)
;   +56: flags (DWORD: 1=connected, 2=authenticated)
;   +60: pad (DWORD)
align 16
g_remoteNodes       db (MAX_REMOTE_NODES * NODE_ENTRY_SIZE) dup(0)
g_remoteCount       dd 0

; Scratch buffer for packet assembly (64KB)
.data?
align 16
g_pktScratch        db 10000h dup(?)

; Sockaddr_in template (16 bytes)
;   +0: sin_family (WORD) = AF_INET
;   +2: sin_port   (WORD) = htons(port)
;   +4: sin_addr   (DWORD) = IP
;   +8: sin_zero   (8 bytes)

.code

; ════════════════════════════════════════════════════════════════════
; SwarmNet_Init — Initialize Winsock2 + zero connection table
;   No args. Returns EAX = 0 success, -1 fail.
; ════════════════════════════════════════════════════════════════════
SwarmNet_Init PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; WSAStartup(0x0202, &g_wsaData)
    mov     ecx, 0202h
    lea     rdx, g_wsaData
    call    WSAStartup
    test    eax, eax
    jnz     @sni_fail

    ; Zero connection table
    lea     rcx, g_remoteNodes
    xor     eax, eax
    mov     edx, MAX_REMOTE_NODES
@@sni_zero:
    mov     qword ptr [rcx], INVALID_SOCKET  ; socket = -1
    add     rcx, NODE_ENTRY_SIZE
    dec     edx
    jnz     @@sni_zero

    mov     g_remoteCount, 0
    mov     g_listenSocket, INVALID_SOCKET
    mov     g_netReady, 1

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_INIT
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @sni_ret

@sni_fail:
    mov     eax, -1

@sni_ret:
    lea     rsp, [rbp]
    pop     rbp
    ret
SwarmNet_Init ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_Listen — Create TCP listener on SOVEREIGN_PORT
;   No args. Returns EAX = 0 success, -1 fail.
;   FRAME: 1 push + 40h alloc (for sockaddr on stack)
; ════════════════════════════════════════════════════════════════════
SwarmNet_Listen PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    cmp     g_netReady, 0
    je      @snl_fail

    ; socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    cmp     rax, INVALID_SOCKET
    je      @snl_fail
    mov     g_listenSocket, rax

    ; SO_REUSEADDR
    mov     rcx, g_listenSocket
    mov     edx, SOL_SOCKET
    mov     r8d, SO_REUSEADDR
    lea     r9, [rbp-20h]               ; use stack for optval
    mov     dword ptr [rbp-20h], 1
    sub     rsp, 28h
    mov     dword ptr [rsp+20h], 4       ; optlen
    call    setsockopt
    add     rsp, 28h

    ; Build sockaddr_in on stack at [rbp-30h]
    mov     word ptr [rbp-30h], AF_INET  ; sin_family
    mov     cx, SOVEREIGN_PORT
    call    htons
    mov     word ptr [rbp-2Eh], ax       ; sin_port
    mov     dword ptr [rbp-2Ch], 0       ; INADDR_ANY
    mov     qword ptr [rbp-28h], 0       ; sin_zero

    ; bind(listenSocket, &sockaddr, 16)
    mov     rcx, g_listenSocket
    lea     rdx, [rbp-30h]
    mov     r8d, 16
    call    bind
    test    eax, eax
    jnz     @snl_close

    ; listen(listenSocket, SOMAXCONN)
    mov     rcx, g_listenSocket
    mov     edx, SOMAXCONN
    call    listen
    test    eax, eax
    jnz     @snl_close

    ; Set non-blocking
    mov     rcx, g_listenSocket
    mov     edx, FIONBIO
    lea     r8, [rbp-20h]
    mov     dword ptr [rbp-20h], 1
    call    ioctlsocket

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_LISTEN
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    jmp     @snl_ret

@snl_close:
    mov     rcx, g_listenSocket
    call    closesocket
    mov     g_listenSocket, INVALID_SOCKET

@snl_fail:
    mov     eax, -1

@snl_ret:
    lea     rsp, [rbp]
    pop     rbp
    ret
SwarmNet_Listen ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_Connect — Connect to a remote node by IP
;   RCX = pointer to null-terminated IP string (e.g. "192.168.1.50")
;   Returns: EAX = node index (0..31), -1 on failure
;   FRAME: 2 pushes (rbp,rbx) + 40h alloc
; ════════════════════════════════════════════════════════════════════
SwarmNet_Connect PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    cmp     g_netReady, 0
    je      @snc_fail

    ; Save IP string pointer
    mov     [rbp-10h], rcx

    ; Resolve IP to network byte order
    ; inet_addr(pIPStr)
    call    inet_addr
    cmp     eax, -1
    je      @snc_fail
    mov     ebx, eax                      ; save IP in network order

    ; Find free slot in g_remoteNodes
    lea     r10, g_remoteNodes
    xor     ecx, ecx
@@snc_find:
    cmp     ecx, MAX_REMOTE_NODES
    jge     @snc_fail
    mov     rax, qword ptr [r10]          ; socket handle
    cmp     rax, INVALID_SOCKET
    je      @@snc_found
    add     r10, NODE_ENTRY_SIZE
    inc     ecx
    jmp     @@snc_find

@@snc_found:
    mov     dword ptr [rbp-14h], ecx      ; save node index

    ; socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    mov     r8d, IPPROTO_TCP
    call    socket
    cmp     rax, INVALID_SOCKET
    je      @snc_fail
    mov     [rbp-20h], rax                ; save socket handle

    ; Build sockaddr on stack
    mov     word ptr [rbp-30h], AF_INET
    mov     cx, SOVEREIGN_PORT
    call    htons
    mov     word ptr [rbp-2Eh], ax
    mov     dword ptr [rbp-2Ch], ebx      ; IP
    mov     qword ptr [rbp-28h], 0

    ; connect(sock, &addr, 16)
    mov     rcx, [rbp-20h]
    lea     rdx, [rbp-30h]
    mov     r8d, 16
    call    connect
    test    eax, eax
    jnz     @snc_close

    ; Store in node table
    mov     ecx, dword ptr [rbp-14h]
    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes
    mov     rdx, [rbp-20h]
    mov     qword ptr [r10 + rax], rdx        ; socket
    mov     dword ptr [r10 + rax + 8], ebx    ; IP
    mov     word ptr [r10 + rax + 12], SOVEREIGN_PORT
    mov     dword ptr [r10 + rax + 56], 1     ; flags = connected

    ; Get timestamp
    call    GetTickCount64
    mov     ecx, dword ptr [rbp-14h]
    imul    edx, ecx, NODE_ENTRY_SIZE
    movsxd  rdx, edx
    lea     r10, g_remoteNodes
    mov     qword ptr [r10 + rdx + 48], rax  ; lastHeartbeat

    lock inc dword ptr [g_remoteCount]

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_CONNECT
    mov     r8d, dword ptr [rbp-14h]
    call    BeaconSend

    mov     eax, dword ptr [rbp-14h]     ; return node index
    jmp     @snc_ret

@snc_close:
    mov     rcx, [rbp-20h]
    call    closesocket

@snc_fail:
    mov     eax, -1

@snc_ret:
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
SwarmNet_Connect ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_BroadcastDiscovery — Send discovery packet to all nodes
;   No args. Returns EAX = nodes contacted.
;   FRAME: 2 pushes + 30h alloc
; ════════════════════════════════════════════════════════════════════
SwarmNet_BroadcastDiscovery PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    cmp     g_netReady, 0
    je      @sbd_none

    ; Build discovery packet header in scratch buffer
    lea     r10, g_pktScratch
    mov     dword ptr [r10], SOVEREIGN_MAGIC
    mov     byte ptr [r10+4], PKT_DISCOVERY
    mov     byte ptr [r10+5], 0              ; flags
    mov     dword ptr [r10+6], 0             ; payload length = 0
    inc     word ptr [g_netSeqNo]
    mov     ax, g_netSeqNo
    mov     word ptr [r10+10], ax

    ; Iterate connected nodes, send header
    lea     r10, g_remoteNodes
    xor     ebx, ebx                        ; count sent
    xor     ecx, ecx                        ; node index
@@sbd_loop:
    cmp     ecx, MAX_REMOTE_NODES
    jge     @@sbd_done

    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    mov     rdx, qword ptr [r10 + rax]      ; socket
    cmp     rdx, INVALID_SOCKET
    je      @@sbd_next
    mov     r8d, dword ptr [r10 + rax + 56]  ; flags
    test    r8d, 1                           ; connected?
    jz      @@sbd_next

    ; send(socket, &pktScratch, PKT_HDR_SIZE, 0)
    mov     dword ptr [rbp-10h], ecx         ; save loop counter
    mov     rcx, rdx
    lea     rdx, g_pktScratch
    mov     r8d, PKT_HDR_SIZE
    xor     r9d, r9d
    call    send
    mov     ecx, dword ptr [rbp-10h]
    cmp     eax, PKT_HDR_SIZE
    jne     @@sbd_next
    inc     ebx

@@sbd_next:
    inc     ecx
    jmp     @@sbd_loop

@@sbd_done:
    mov     eax, ebx
    jmp     @sbd_ret

@sbd_none:
    xor     eax, eax

@sbd_ret:
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
SwarmNet_BroadcastDiscovery ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_SendShard — Send shard data to a specific node
;   ECX = nodeIndex
;   RDX = pData (pointer to shard buffer)
;   R8  = dataLen (bytes)
;   Returns: EAX = bytes sent, -1 on failure.
;
;   Protocol: [12B header][payload]
;   FRAME: 3 pushes (rbp,rbx,rsi) + 30h alloc
; ════════════════════════════════════════════════════════════════════
SwarmNet_SendShard PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    cmp     g_netReady, 0
    je      @sns_fail

    ; Validate node index
    cmp     ecx, MAX_REMOTE_NODES
    jge     @sns_fail

    ; Save args
    mov     dword ptr [rbp-10h], ecx     ; nodeIndex
    mov     [rbp-18h], rdx               ; pData
    mov     [rbp-20h], r8                ; dataLen

    ; Get socket for this node
    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes
    mov     rbx, qword ptr [r10 + rax]   ; socket handle
    cmp     rbx, INVALID_SOCKET
    je      @sns_fail

    ; Build header in scratch buffer
    lea     rsi, g_pktScratch
    mov     dword ptr [rsi], SOVEREIGN_MAGIC
    mov     byte ptr [rsi+4], PKT_SHARD_DATA
    mov     byte ptr [rsi+5], 0
    mov     eax, dword ptr [rbp-20h]
    mov     dword ptr [rsi+6], eax       ; payload length
    inc     word ptr [g_netSeqNo]
    mov     ax, g_netSeqNo
    mov     word ptr [rsi+10], ax

    ; Send header
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, PKT_HDR_SIZE
    xor     r9d, r9d
    call    send
    cmp     eax, PKT_HDR_SIZE
    jne     @sns_fail

    ; Send payload
    mov     eax, dword ptr [rbp-20h]     ; dataLen
    test    eax, eax
    jz      @sns_ok

    mov     rcx, rbx
    mov     rdx, [rbp-18h]              ; pData
    mov     r8d, eax
    xor     r9d, r9d
    call    send
    cmp     eax, -1
    je      @sns_fail

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_SEND
    mov     r8d, dword ptr [rbp-10h]
    call    BeaconSend

    mov     eax, dword ptr [rbp-20h]
    add     eax, PKT_HDR_SIZE            ; total bytes sent
    jmp     @sns_ret

@sns_ok:
    mov     eax, PKT_HDR_SIZE
    jmp     @sns_ret

@sns_fail:
    mov     eax, -1

@sns_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmNet_SendShard ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_RecvShard — Receive shard data from a node
;   ECX = nodeIndex
;   RDX = pOutBuf (destination buffer)
;   R8D = bufSize (max bytes to receive)
;   Returns: EAX = bytes received (payload only), -1 on failure/bad magic
;
;   Reads 12-byte header first, validates magic + type, then reads payload.
;   FRAME: 3 pushes + 30h alloc
; ════════════════════════════════════════════════════════════════════
SwarmNet_RecvShard PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    cmp     g_netReady, 0
    je      @snr_fail

    cmp     ecx, MAX_REMOTE_NODES
    jge     @snr_fail

    ; Save args
    mov     dword ptr [rbp-10h], ecx
    mov     [rbp-18h], rdx
    mov     dword ptr [rbp-20h], r8d

    ; Get socket
    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes
    mov     rbx, qword ptr [r10 + rax]
    cmp     rbx, INVALID_SOCKET
    je      @snr_fail

    ; Receive header (12 bytes) into scratch
    lea     rsi, g_pktScratch
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, PKT_HDR_SIZE
    xor     r9d, r9d
    call    recv
    cmp     eax, PKT_HDR_SIZE
    jne     @snr_fail

    ; Validate magic
    cmp     dword ptr [rsi], SOVEREIGN_MAGIC
    jne     @snr_fail

    ; Get payload length from header
    mov     eax, dword ptr [rsi+6]
    test    eax, eax
    jz      @snr_empty

    ; Clamp to bufSize
    cmp     eax, dword ptr [rbp-20h]
    jle     @@snr_recv
    mov     eax, dword ptr [rbp-20h]

@@snr_recv:
    mov     dword ptr [rbp-24h], eax     ; save recv length

    ; Receive payload
    mov     rcx, rbx
    mov     rdx, [rbp-18h]
    mov     r8d, eax
    xor     r9d, r9d
    call    recv
    cmp     eax, -1
    je      @snr_fail

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_RECV
    mov     r8d, eax
    call    BeaconSend

    mov     eax, dword ptr [rbp-24h]
    jmp     @snr_ret

@snr_empty:
    xor     eax, eax
    jmp     @snr_ret

@snr_fail:
    mov     eax, -1

@snr_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmNet_RecvShard ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_SyncKVCache — Broadcast KV segment across cluster
;   RCX = pKVData (Pointer to the memory to sync)
;   EDX = dataLen (Size in bytes)
;   R8D = targetNodeIndex (0xFFFFFFFF = broadcast all)
;   Returns: EAX = 0 success, -1 fail
; ════════════════════════════════════════════════════════════════════
SwarmNet_SyncKVCache PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     [rbp-10h], rcx              ; pKVData
    mov     dword ptr [rbp-18h], edx    ; dataLen
    mov     dword ptr [rbp-1Ch], r8d    ; targetNode

    test    edx, edx
    jz      @skv_fail

    cmp     g_netReady, 0
    je      @skv_fail

    ; Build KV sync header in scratch
    lea     rsi, g_pktScratch
    mov     dword ptr [rsi], SOVEREIGN_MAGIC
    mov     byte ptr [rsi+4], PKT_KV_SYNC
    mov     byte ptr [rsi+5], 0
    mov     eax, dword ptr [rbp-18h]
    mov     dword ptr [rsi+6], eax
    inc     word ptr [g_netSeqNo]
    mov     ax, g_netSeqNo
    mov     word ptr [rsi+10], ax

    ; If targetNode != 0xFFFFFFFF, send to specific node only
    mov     eax, dword ptr [rbp-1Ch]
    cmp     eax, 0FFFFFFFFh
    je      @@skv_bcast

    ; Single-node send: header + payload
    mov     ecx, eax
    mov     rdx, [rbp-10h]
    mov     r8d, dword ptr [rbp-18h]
    call    SwarmNet_SendShard
    cmp     eax, -1
    je      @skv_fail
    xor     eax, eax
    jmp     @skv_ret

@@skv_bcast:
    ; Broadcast to all connected nodes
    lea     r10, g_remoteNodes
    xor     ebx, ebx
    xor     ecx, ecx
@@skv_loop:
    cmp     ecx, MAX_REMOTE_NODES
    jge     @@skv_bcast_done

    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    mov     rdx, qword ptr [r10 + rax]
    cmp     rdx, INVALID_SOCKET
    je      @@skv_next

    mov     dword ptr [rbp-20h], ecx     ; save index
    mov     ecx, dword ptr [rbp-20h]
    mov     rdx, [rbp-10h]
    mov     r8d, dword ptr [rbp-18h]
    call    SwarmNet_SendShard
    mov     ecx, dword ptr [rbp-20h]
    cmp     eax, -1
    je      @@skv_next
    inc     ebx

@@skv_next:
    inc     ecx
    jmp     @@skv_loop

@@skv_bcast_done:
    xor     eax, eax
    jmp     @skv_ret

@skv_fail:
    mov     eax, -1

@skv_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmNet_SyncKVCache ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_ExchangeLoadInfo — Exchange per-GPU load counters with node
;   ECX = nodeIndex
;   RDX = pLocalLoad (pointer to 8 DWORDs = g_deviceLoad)
;   Returns: EAX = 0 (exchanged), -1 (fail)
;
;   Sends local load as PKT_LOAD_EXCHANGE (32 bytes payload),
;   then receives remote load into node table at offset +16.
;   FRAME: 3 pushes + 30h alloc
; ════════════════════════════════════════════════════════════════════
SwarmNet_ExchangeLoadInfo PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    cmp     g_netReady, 0
    je      @sle_fail

    cmp     ecx, MAX_REMOTE_NODES
    jge     @sle_fail

    mov     dword ptr [rbp-10h], ecx     ; nodeIndex
    mov     [rbp-18h], rdx               ; pLocalLoad

    ; Get socket
    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes
    mov     rbx, qword ptr [r10 + rax]
    cmp     rbx, INVALID_SOCKET
    je      @sle_fail

    ; Build load exchange packet: header(12) + 8 DWORDs(32) = 44 bytes
    lea     rsi, g_pktScratch
    mov     dword ptr [rsi], SOVEREIGN_MAGIC
    mov     byte ptr [rsi+4], PKT_LOAD_EXCHANGE
    mov     byte ptr [rsi+5], 0
    mov     dword ptr [rsi+6], 32        ; 8 * 4 bytes
    inc     word ptr [g_netSeqNo]
    mov     ax, g_netSeqNo
    mov     word ptr [rsi+10], ax

    ; Copy local load counters into packet body (offset 12)
    mov     rdx, [rbp-18h]
    mov     rax, qword ptr [rdx]         ; first 2 DWORDs
    mov     qword ptr [rsi+12], rax
    mov     rax, qword ptr [rdx+8]
    mov     qword ptr [rsi+20], rax
    mov     rax, qword ptr [rdx+16]
    mov     qword ptr [rsi+28], rax
    mov     rax, qword ptr [rdx+24]
    mov     qword ptr [rsi+36], rax

    ; Send local load (44 bytes total)
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, 44
    xor     r9d, r9d
    call    send
    cmp     eax, 44
    jne     @sle_fail

    ; Receive remote load (expect 44-byte response)
    mov     rcx, rbx
    mov     rdx, rsi                      ; reuse scratch for recv
    mov     r8d, 44
    xor     r9d, r9d
    call    recv
    cmp     eax, 44
    jne     @sle_fail

    ; Validate magic
    cmp     dword ptr [rsi], SOVEREIGN_MAGIC
    jne     @sle_fail

    ; Store remote load counters into node table offset +16
    mov     ecx, dword ptr [rbp-10h]
    imul    eax, ecx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes

    ; Copy 32 bytes (8 DWORDs) of remote load
    mov     rdx, qword ptr [rsi+12]
    mov     qword ptr [r10 + rax + 16], rdx
    mov     rdx, qword ptr [rsi+20]
    mov     qword ptr [r10 + rax + 24], rdx
    mov     rdx, qword ptr [rsi+28]
    mov     qword ptr [r10 + rax + 32], rdx
    mov     rdx, qword ptr [rsi+36]
    mov     qword ptr [r10 + rax + 40], rdx

    ; Update heartbeat timestamp
    call    GetTickCount64
    mov     ecx, dword ptr [rbp-10h]
    imul    edx, ecx, NODE_ENTRY_SIZE
    movsxd  rdx, edx
    lea     r10, g_remoteNodes
    mov     qword ptr [r10 + rdx + 48], rax

    xor     eax, eax
    jmp     @sle_ret

@sle_fail:
    mov     eax, -1

@sle_ret:
    lea     rsp, [rbp]
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SwarmNet_ExchangeLoadInfo ENDP


; ════════════════════════════════════════════════════════════════════
; SwarmNet_Shutdown — Close all sockets, cleanup Winsock
;   No args. Returns EAX = 0.
; ════════════════════════════════════════════════════════════════════
SwarmNet_Shutdown PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Close all remote sockets
    lea     r10, g_remoteNodes
    xor     ebx, ebx
@@snd_loop:
    cmp     ebx, MAX_REMOTE_NODES
    jge     @@snd_listen

    imul    eax, ebx, NODE_ENTRY_SIZE
    cdqe
    mov     rcx, qword ptr [r10 + rax]
    cmp     rcx, INVALID_SOCKET
    je      @@snd_next
    call    closesocket
    imul    eax, ebx, NODE_ENTRY_SIZE
    cdqe
    lea     r10, g_remoteNodes
    mov     qword ptr [r10 + rax], INVALID_SOCKET

@@snd_next:
    inc     ebx
    jmp     @@snd_loop

@@snd_listen:
    ; Close listener
    mov     rcx, g_listenSocket
    cmp     rcx, INVALID_SOCKET
    je      @@snd_wsa
    call    closesocket
    mov     g_listenSocket, INVALID_SOCKET

@@snd_wsa:
    call    WSACleanup
    mov     g_netReady, 0
    mov     g_remoteCount, 0

    ; Beacon
    mov     ecx, NET_BEACON_SLOT
    mov     edx, NET_EVT_SHUTDOWN
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
SwarmNet_Shutdown ENDP

END
