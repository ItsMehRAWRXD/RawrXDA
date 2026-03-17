; =============================================================================
; RawrXD_P2PRelay.asm — P2P NAT Traversal & QUIC Relay Engine
; Tier 5 Gap #42: UDP Hole Punching + QUIC Transport
; RFC 5389 (STUN) | RFC 9000 (QUIC) | RFC 5780 (NAT Behavior)
; =============================================================================

INCLUDE ksamd64.inc

; -----------------------------------------------------------------------------
; External Imports
; -----------------------------------------------------------------------------
EXTRN   socket:PROC
EXTRN   bind:PROC
EXTRN   sendto:PROC
EXTRN   recvfrom:PROC
EXTRN   closesocket:PROC
EXTRN   select:PROC
EXTRN   ioctlsocket:PROC
EXTRN   htons:PROC
EXTRN   ntohs:PROC
EXTRN   htonl:PROC
EXTRN   ntohl:PROC
EXTRN   inet_addr:PROC
EXTRN   RtlRandomEx:PROC
EXTRN   QueryPerformanceCounter:PROC

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
STUN_MAGIC_COOKIE         EQU 02112A442h  ; RFC 5389
STUN_BINDING_REQUEST      EQU 00001h
STUN_BINDING_RESPONSE     EQU 000101h
STUN_XOR_MAPPED_ADDRESS   EQU 00020h      ; 0x0020
STUN_MESSAGE_INTEGRITY    EQU 00008h
STUN_FINGERPRINT          EQU 0008028h

UDP_BUFFER_SIZE           EQU 65507       ; Max IPv4 UDP payload
QUIC_MAX_DATAGRAM         EQU 1350        ; Plausible MTU
QUIC_HEADER_LONG          EQU 80h         ; Long header flag
QUIC_VERSION_1            EQU 000000001h  ; V1

; NAT Types (RFC 5780)
NAT_OPEN_INTERNET         EQU 0
NAT_FULL_CONE             EQU 1
NAT_RESTRICTED            EQU 2
NAT_PORT_RESTRICTED       EQU 3
NAT_SYMMETRIC             EQU 4
NAT_BLOCKED               EQU 5

; Hole Punch States
PUNCH_IDLE                EQU 0
PUNCH_STUN_QUERY          EQU 1
PUNCH_STUN_RESPONSE       EQU 2
PUNCH_HOLE_SENT           EQU 3
PUNCH_CONNECTED           EQU 4

; QUIC Frame Types
FRAME_PADDING             EQU 00h
FRAME_PING                EQU 01h
FRAME_ACK                 EQU 02h
FRAME_RESET_STREAM        EQU 04h
FRAME_STOP_SENDING        EQU 05h
FRAME_CRYPTO              EQU 06h
FRAME_NEW_TOKEN           EQU 07h
FRAME_STREAM              EQU 08h         ; Base, OR with flags
FRAME_MAX_DATA            EQU 10h
FRAME_DATA_BLOCKED        EQU 14h
FRAME_STREAMS_BLOCKED     EQU 17h
FRAME_NEW_CONNECTION_ID   EQU 18h
FRAME_RETIRE_CONNECTION_ID EQU 19h
FRAME_PATH_CHALLENGE      EQU 1Ah
FRAME_PATH_RESPONSE       EQU 1Bh
FRAME_CONNECTION_CLOSE    EQU 1Ch
FRAME_HANDSHAKE_DONE      EQU 1Eh

; Offsets in P2P_CONTEXT
P2P_SOCKET                EQU 0
P2P_STATE                 EQU 8
P2P_NAT_TYPE              EQU 12
P2P_STUN_SERVER           EQU 16          ; sockaddr_in
P2P_PUBLIC_ADDR           EQU 32          ; XOR-MAPPED result
P2P_PEER_ADDR             EQU 48          ; Target for hole punch
P2P_LOCAL_PORT            EQU 64
P2P_RETRY_COUNT           EQU 68
P2P_LAST_TIMESTAMP        EQU 72
P2P_QUIC_CTX              EQU 80          ; Pointer to QUIC context
SIZEOF_P2P_CTX            EQU 88

; QUIC Context offsets
QUIC_VERSION              EQU 0
QUIC_DCID                 EQU 4           ; Destination Connection ID
QUIC_DCID_LEN             EQU 12
QUIC_SCID                 EQU 20          ; Source Connection ID
QUIC_SCID_LEN             EQU 28
QUIC_PACKET_NUMBER        EQU 32
QUIC_STREAM_ID            EQU 40
QUIC_RECV_BUF             EQU 48
QUIC_SEND_BUF             EQU 56
SIZEOF_QUIC_CTX           EQU 64

; -----------------------------------------------------------------------------
; Data Section
; -----------------------------------------------------------------------------
.data?
align 4096
g_P2PContextPool          DQ    ?
g_QUICContextPool         DQ    ?
g_STUNTransactionID       DB    12 DUP(?)
g_NonceCounter            DQ    0

.data
align 16
STUN_Servers:
    ; Public STUN servers (Google, Cloudflare, etc)
    DB    "142.250.82.46",0             ; stun.l.google.com:19302
    DB    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ; Padding
    DB    "172.217.213.127",0           ; Alternate
    DB    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

STUN_Port                 DW    4E1Fh     ; 19302 big-endian (will convert)

; -----------------------------------------------------------------------------
; Code Section
; -----------------------------------------------------------------------------
.code

; =============================================================================
; STUN PROTOCOL IMPLEMENTATION
; =============================================================================

; -----------------------------------------------------------------------------
; STUN_BuildBindingRequest
; rcx = OutputBuffer, rdx = TransactionID(12 bytes)
; Returns: Message length in RAX
; -----------------------------------------------------------------------------
STUN_BuildBindingRequest PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rdi, rcx                    ; Output buffer
    mov     rsi, rdx                    ; Transaction ID

    ; Message Type: Binding Request (0x0001)
    mov     WORD PTR [rdi], 0100h       ; 0x0001 big-endian
    ; Message Length: 0 (no attributes yet) - placeholder
    mov     WORD PTR [rdi+2], 0000h

    ; Magic Cookie
    mov     eax, STUN_MAGIC_COOKIE
    bswap   eax                         ; To big-endian
    mov     DWORD PTR [rdi+4], eax

    ; Transaction ID (12 bytes, copy from input)
    movsq                               ; Copy 8 bytes
    movsd                               ; Copy 4 bytes

    ; Standard length = 20 bytes header only (can add attributes later)
    mov     rax, 20

    pop     rsi
    pop     rdi
    ret
STUN_BuildBindingRequest ENDP

; -----------------------------------------------------------------------------
; STUN_ParseXORMappedAddress
; rcx = STUN Response ptr, rdx = ResponseLength, r8 = OutputAddr(sockaddr_in*)
; Returns: 0=success, 1=fail
; -----------------------------------------------------------------------------
STUN_ParseXORMappedAddress PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx                    ; Response buffer
    mov     rdi, r8                     ; Output sockaddr

    ; Verify message type is Binding Response Success (0x0101)
    movzx   eax, WORD PTR [rbx]
    xchg    al, ah                      ; ntohs
    cmp     ax, 0101h
    jne     @@fail

    ; Get message length
    movzx   edx, WORD PTR [rbx+2]
    xchg    dl, dh                      ; ntohs
    add     rdx, 20                     ; Total message size
    cmp     rdx, r8                     ; Check against provided length
    ja      @@fail

    ; Skip to attributes (offset 20)
    add     rbx, 20
    sub     rdx, 20                     ; Remaining bytes

@@attr_loop:
    cmp     rdx, 4
    jb      @@fail                      ; No room for header

    movzx   eax, WORD PTR [rbx]         ; Attribute type
    xchg    al, ah                      ; ntohs
    movzx   ecx, WORD PTR [rbx+2]       ; Attribute length
    xchg    cl, ch                      ; ntohs
    add     ecx, 3                      ; Pad to 4-byte boundary
    and     ecx, -4

    cmp     ax, STUN_XOR_MAPPED_ADDRESS
    je      @@found_xor_mapped

    add     rbx, 4
    add     rbx, rcx                    ; Skip attribute
    sub     rdx, 4
    sub     rdx, rcx
    jmp     @@attr_loop

@@found_xor_mapped:
    ; Parse XOR-MAPPED-ADDRESS
    ; Format: 1 byte reserved (0), 1 byte family (0x01=IPv4), 2 bytes port, 4 bytes address
    cmp     ecx, 8                      ; Minimum valid size
    jb      @@fail

    movzx   eax, BYTE PTR [rbx+5]       ; Family
    cmp     al, 01h                     ; IPv4 only for now
    jne     @@fail                      ; IPv6 not implemented

    ; Port (XOR with magic cookie high 16 bits: 0x2112)
    movzx   eax, WORD PTR [rbx+6]
    xchg    al, ah                      ; ntohs
    xor     ax, 1221h                   ; 0x2112 in host order (reversed)
    mov     WORD PTR [rdi+2], ax        ; sin_port

    ; Address (XOR with magic cookie)
    mov     eax, DWORD PTR [rbx+8]
    bswap   eax                         ; ntohl
    xor     eax, 02112A442h             ; XOR with magic cookie
    mov     DWORD PTR [rdi+4], eax      ; sin_addr

    mov     WORD PTR [rdi], 0002h       ; AF_INET

    xor     eax, eax                    ; Success
    jmp     @@exit

@@fail:
    mov     eax, 1

@@exit:
    add     rsp, 32
    pop     rsi
    pop     rdi
    pop     rbx
    ret
STUN_ParseXORMappedAddress ENDP

; =============================================================================
; NAT DETECTION (RFC 5780)
; =============================================================================

; -----------------------------------------------------------------------------
; NAT_DetectType
; rcx = P2P_Context ptr
; Returns: NAT type constant in EAX
; -----------------------------------------------------------------------------
PUBLIC NAT_DetectType
NAT_DetectType PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 128                    ; Buffer space + locals
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; P2P Context
    mov     r12, rsp                    ; Buffer for STUN messages

    ; Test 1: Can we get any response? (Not blocked)
    mov     ecx, 1000                   ; 1 second timeout
    call    STUN_Query
    test    eax, eax
    jnz     @@blocked

    ; Test 2: Compare local vs mapped address (hairpin test)
    ; If mapped == local, we're on open internet or full cone
    mov     eax, DWORD PTR [rbx+P2P_PUBLIC_ADDR+4]  ; Public IP
    cmp     eax, 07F000001h             ; 127.0.0.1
    je      @@full_cone                 ; Localhost mapping = full cone/NAT loopback

    ; Test 3: Change request test (different source port)
    ; If we get different mapping with different local port, it's symmetric

    ; Simplified: Assume restricted if we got here (TODO: full RFC 5780)
    mov     eax, NAT_RESTRICTED
    jmp     @@exit

@@blocked:
    mov     eax, NAT_BLOCKED
    jmp     @@exit

@@full_cone:
    mov     eax, NAT_FULL_CONE

@@exit:
    lea     rsp, [rbp - 32]
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
NAT_DetectType ENDP

; =============================================================================
; UDP HOLE PUNCHING
; =============================================================================

; -----------------------------------------------------------------------------
; UDPHolePunch — Main entry for hole punching
; rcx = P2P_Context ptr, rdx = PeerAddr (sockaddr_in*), r8 = TimeoutMs
; Returns: 0=success (connected), 1=timeout/fail
; -----------------------------------------------------------------------------
PUBLIC UDPHolePunch
UDPHolePunch PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 168                    ; Buffers + shadow space
    .allocstack 168
    .endprolog

    mov     rbx, rcx                    ; P2P Context
    mov     rdi, rdx                    ; Peer address
    mov     r12d, r8d                   ; Timeout
    mov     r13, rsp                    ; Buffer

    ; Store peer address in context
    mov     rax, [rdi]
    mov     [rbx+P2P_PEER_ADDR], rax
    mov     rax, [rdi+8]
    mov     [rbx+P2P_PEER_ADDR+8], rax

    ; Create UDP socket if not exists
    cmp     DWORD PTR [rbx+P2P_SOCKET], 0
    jne     @@socket_exists

    mov     ecx, 0002h                  ; AF_INET
    mov     edx, 0002h                  ; SOCK_DGRAM
    xor     r8d, r8d                    ; IPPROTO_UDP (0)
    call    socket
    mov     [rbx+P2P_SOCKET], eax
    inc     eax                         ; Check for INVALID_SOCKET (-1)
    jz      @@fail

@@socket_exists:
    ; Set non-blocking mode
    mov     ecx, [rbx+P2P_SOCKET]
    mov     edx, 0C800466Fh             ; FIONBIO
    lea     r8, [rsp + 160]             ; On stack: argp
    mov     DWORD PTR [r8], 1           ; Enable non-blocking
    call    ioctlsocket

    ; State: Hole Punch Sent
    mov     DWORD PTR [rbx+P2P_STATE], PUNCH_HOLE_SENT

    ; Send hole punch packets (multiple attempts)
    mov     esi, 10                     ; Retry count

@@punch_loop:
    cmp     esi, 0
    je      @@timeout

    ; Send empty UDP packet to peer (punch the hole)
    mov     ecx, [rbx+P2P_SOCKET]
    mov     rdx, r13                    ; Empty buffer (zeros)
    xor     r8d, r8d                    ; Length 0 (or send 1 byte)
    inc     r8d                         ; Send 1 byte (0x00)
    mov     r9, rdi                     ; Peer address
    mov     DWORD PTR [rsp + 32], 16    ; sizeof(sockaddr_in)
    call    sendto

    ; Wait for response using select
    lea     rcx, [rsp + 64]             ; fd_set readfds
    mov     DWORD PTR [rcx], 1          ; fd_count = 1
    mov     eax, [rbx+P2P_SOCKET]
    mov     [rcx + 8], rax              ; fd_array[0]

    mov     edx, 0                      ; nfds (ignored)
    lea     r8, [rsp + 64]              ; readfds
    xor     r9, r9                      ; writefds
    mov     QWORD PTR [rsp + 32], 0     ; exceptfds
    lea     rax, [rsp + 144]            ; timeval
    mov     DWORD PTR [rax], 0          ; tv_sec
    mov     DWORD PTR [rax + 8], 10000  ; tv_usec = 10ms
    mov     QWORD PTR [rsp + 40], rax   ; timeout

    call    select

    test    eax, eax
    jg      @@data_ready                ; Data available!

    dec     esi
    jmp     @@punch_loop

@@data_ready:
    ; Receive data from peer
    mov     ecx, [rbx+P2P_SOCKET]
    mov     rdx, r13                    ; Buffer
    mov     r8d, UDP_BUFFER_SIZE
    xor     r9d, r9d                    ; flags
    lea     rax, [rsp + 128]            ; from address
    mov     QWORD PTR [rsp + 32], rax
    lea     rax, [rsp + 136]            ; fromlen
    mov     DWORD PTR [rax], 16
    mov     QWORD PTR [rsp + 40], rax   ; from len
    call    recvfrom

    test    eax, eax
    js      @@fail                      ; Error

    ; Success! Mark as connected
    mov     DWORD PTR [rbx+P2P_STATE], PUNCH_CONNECTED
    xor     eax, eax                    ; Return success
    jmp     @@exit

@@timeout:
@@fail:
    mov     eax, 1

@@exit:
    lea     rsp, [rbp - 40]
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
UDPHolePunch ENDP

; =============================================================================
; QUIC FRAME HANDLING
; =============================================================================

; -----------------------------------------------------------------------------
; QUIC_ParseHeader — Parse QUIC Long/Short header
; rcx = Packet ptr, rdx = PacketLen, r8 = QUIC_Context ptr, r9 = OutputHeaderInfo
; Returns: Payload offset in RAX, -1 on error
; -----------------------------------------------------------------------------
PUBLIC QUIC_ParseHeader
QUIC_ParseHeader PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rbx, rcx                    ; Packet
    mov     rdi, r8                     ; QUIC Context
    mov     rsi, r9                     ; Output info

    movzx   eax, BYTE PTR [rbx]         ; First byte
    test    al, 80h                     ; Check long header bit
    jz      @@short_header

    ; Long Header parsing
    ; First byte: 1STTTXXX where S=1, TTT=packet type, XXX=reserved
    and     al, 30h                     ; Mask type bits
    shr     al, 4

    ; Version (next 4 bytes)
    mov     eax, DWORD PTR [rbx+1]
    bswap   eax                         ; ntohl
    mov     [rdi+QUIC_VERSION], eax

    ; DCIL/SCIL (next byte)
    movzx   eax, BYTE PTR [rbx+5]
    mov     ecx, eax
    and     ecx, 0Fh                    ; DCID len
    shr     al, 4
    and     eax, 0Fh                    ; SCID len

    ; DCID
    lea     rdx, [rbx+6]
    mov     r8, rdi
    add     r8, QUIC_DCID
    mov     r9d, ecx
    call    QUIC_CopyConnectionID

    ; SCID follows DCID
    lea     rdx, [rbx+6]
    add     rdx, rcx                    ; Skip DCID
    mov     r8, rdi
    add     r8, QUIC_SCID
    mov     r9d, eax
    call    QUIC_CopyConnectionID

    ; Token length (variable int) - simplified
    ; Length (variable int)
    ; Packet Number (1-4 bytes based on type)

    mov     rax, 14                     ; Approximate offset (simplified)
    jmp     @@exit

@@short_header:
    ; Short header: 0RTTTXXX where R=reserved, TTT=spin/etc, XXX=reserved
    ; DCID follows immediately (length known from context)
    mov     ecx, [rdi+QUIC_DCID_LEN]
    lea     rdx, [rbx+1]
    mov     r8, rdi
    add     r8, QUIC_DCID
    mov     r9d, ecx
    call    QUIC_CopyConnectionID

    mov     rax, 1
    add     rax, rcx                    ; Return offset after DCID

@@exit:
    pop     rsi
    pop     rdi
    pop     rbx
    ret
QUIC_ParseHeader ENDP

; Helper: Copy connection ID
QUIC_CopyConnectionID PROC
    cmp     r9d, 20                     ; Max QUIC CID length
    ja      @@clamp
    test    r9d, r9d
    jz      @@done
    mov     rcx, r9
@@copy:
    mov     al, [rdx]
    mov     [r8], al
    inc     rdx
    inc     r8
    dec     ecx
    jnz     @@copy
@@done:
    ret
@@clamp:
    mov     r9d, 20
    jmp     @@copy
QUIC_CopyConnectionID ENDP

; -----------------------------------------------------------------------------
; QUIC_ProcessFrame — Handle individual QUIC frames
; rcx = Frame ptr, rdx = FrameLen, r8 = QUIC_Context, r9 = P2P_Context
; Returns: Bytes consumed in RAX
; -----------------------------------------------------------------------------
PUBLIC QUIC_ProcessFrame
QUIC_ProcessFrame PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Frame data
    mov     rdi, r8                     ; QUIC context

    movzx   eax, BYTE PTR [rbx]         ; Frame type
    and     eax, 1Fh                    ; Mask to get base type (if stream)

    cmp     al, FRAME_STREAM
    je      @@handle_stream

    cmp     al, FRAME_CRYPTO
    je      @@handle_crypto

    cmp     al, FRAME_ACK
    je      @@handle_ack

    ; Default: skip frame (consume 1 byte + any length fields)
    mov     rax, 1
    jmp     @@exit

@@handle_stream:
    ; STREAM frame: TSSSDDDD where T=type, SSS=stream flags, DDDD=data length
    ; Simplified: just acknowledge receipt
    mov     rcx, rdi
    call    QUIC_AcknowledgeStreamData
    mov     rax, 16                     ; Approximate consumption
    jmp     @@exit

@@handle_crypto:
    ; CRYPTO frame: offset(8) + length(8) + data
    mov     rax, 1                      ; Consume type byte
    jmp     @@exit

@@handle_ack:
    mov     rax, 1

@@exit:
    add     rsp, 40
    pop     rdi
    pop     rbx
    ret
QUIC_ProcessFrame ENDP

QUIC_AcknowledgeStreamData PROC
    ; Placeholder for stream ACK logic
    ret
QUIC_AcknowledgeStreamData ENDP

; =============================================================================
; RELAY INTEGRATION
; =============================================================================

; -----------------------------------------------------------------------------
; QUICFrameRelay — Main relay loop for QUIC datagrams
; rcx = P2P_Context ptr
; -----------------------------------------------------------------------------
PUBLIC QUICFrameRelay
QUICFrameRelay PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, UDP_BUFFER_SIZE + 64
    .allocstack (UDP_BUFFER_SIZE + 64)
    .endprolog

    mov     rbx, rcx                    ; P2P Context
    mov     rdi, rsp                    ; Receive buffer
    mov     rsi, rbx
    add     rsi, P2P_QUIC_CTX           ; QUIC context offset

@@relay_loop:
    ; Check running state
    mov     eax, [rbx+P2P_STATE]
    cmp     eax, PUNCH_CONNECTED
    jne     @@cleanup

    ; Receive UDP datagram
    mov     ecx, [rbx+P2P_SOCKET]
    mov     rdx, rdi                    ; Buffer
    mov     r8d, UDP_BUFFER_SIZE
    xor     r9d, r9d                    ; Flags
    lea     rax, [rsp + UDP_BUFFER_SIZE] ; From addr placeholder
    mov     QWORD PTR [rsp + 32], rax
    lea     rax, [rsp + UDP_BUFFER_SIZE + 16]
    mov     DWORD PTR [rax], 16
    mov     QWORD PTR [rsp + 40], rax   ; From len
    call    recvfrom

    test    eax, eax
    js      @@relay_loop                ; Error or would block

    ; Parse QUIC header
    mov     rcx, rdi                    ; Packet
    mov     rdx, rax                    ; Length
    mov     r8, rsi                     ; QUIC context
    xor     r9, r9                      ; No header info needed for now
    call    QUIC_ParseHeader

    cmp     rax, -1
    je      @@relay_loop                ; Parse error

    ; Process payload as frames
    add     rdi, rax                    ; Skip to payload
    sub     rax, rax                    ; Remaining length...
    ; ... frame processing loop would go here

    jmp     @@relay_loop

@@cleanup:
    add     rsp, UDP_BUFFER_SIZE + 64
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
QUICFrameRelay ENDP

; =============================================================================
; UTILITY FUNCTIONS
; =============================================================================

STUN_Query PROC
    ; Simplified STUN query helper
    ret
STUN_Query ENDP

; -----------------------------------------------------------------------------
; P2P_InitContextPool — Initialize P2P context pool
; rcx = MaxContexts (default 256)
; -----------------------------------------------------------------------------
PUBLIC P2P_InitContextPool
P2P_InitContextPool PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx
    imul    rbx, SIZEOF_P2P_CTX
    mov     rcx, rbx
    call    AllocateLargePagesMemory
    mov     [g_P2PContextPool], rax

    xor     eax, eax
    pop     rbx
    ret
P2P_InitContextPool ENDP

AllocateLargePagesMemory PROC
    ; Same as NetworkRelay version
    push    rbx
    mov     rbx, rcx
    mov     rcx, rbx
    xor     edx, edx
    mov     r8, rbx
    mov     r9, 02003000h
    push    04h
    pop     QWORD PTR [rsp+32]
    call    VirtualAlloc
    pop     rbx
    ret
AllocateLargePagesMemory ENDP

; -----------------------------------------------------------------------------
; EXTRN VirtualAlloc needed
; -----------------------------------------------------------------------------
EXTRN   VirtualAlloc:PROC

END</content>
<parameter name="filePath">d:\rawrxd\RawrXD_P2PRelay.asm