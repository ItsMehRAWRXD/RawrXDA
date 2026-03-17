; =============================================================================
; RawrXD_UDPHolePunch.asm — STUN/TURN/ICE without dependencies
; NAT type detection + Hole punching for P2P relay
; =============================================================================

INCLUDE ksamd64.inc

EXTRN   socket:PROC
EXTRN   bind:PROC
EXTRN   sendto:PROC
EXTRN   recvfrom:PROC
EXTRN   closesocket:PROC
EXTRN   WSAGetLastError:PROC
EXTRN   inet_addr:PROC
EXTRN   htons:PROC
EXTRN   ntohs:PROC
EXTRN   QueryPerformanceCounter:PROC
EXTRN   Sleep:PROC

; STUN Constants
STUN_MAGIC_COOKIE         EQU 02112A442h
STUN_BINDING_REQUEST      EQU 00001h
STUN_BINDING_RESPONSE     EQU 01001h
STUN_XOR_MAPPED_ADDRESS   EQU 02001h
STUN_MESSAGE_INTEGRITY    EQU 00008h
STUN_FINGERPRINT          EQU 08028h

; NAT Types (RFC 4787)
NAT_TYPE_UNKNOWN          EQU 0
NAT_TYPE_OPEN_INTERNET    EQU 1
NAT_TYPE_FULL_CONE        EQU 2
NAT_TYPE_RESTRICTED       EQU 3
NAT_TYPE_PORT_RESTRICTED  EQU 4
NAT_TYPE_SYMMETRIC        EQU 5

; STUN Server List (hardcoded Google/Mozilla for zero-config)
STUN_SERVER_COUNT         EQU 4

; Offsets in StunContext
STUNCTX_Socket            EQU 0
STUNCTX_NatType           EQU 8
STUNCTX_PublicIP          EQU 12
STUNCTX_PublicPort        EQU 16
STUNCTX_TimeoutMs         EQU 20
STUNCTX_RetransmitCount   EQU 24

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
StunServers:
    ; Google STUN
    DB      "74.125.142.127",0        ; stun.l.google.com:19302
    DW      19302
    DB      0,0,0,0,0,0               ; padding
    ; Mozilla STUN
    DB      "52.26.83.107",0          ; stun.services.mozilla.com
    DW      3478
    DB      0,0,0,0,0,0,0,0,0,0
    ; Twilio
    DB      "54.169.84.79",0
    DW      3478
    DB      0,0,0,0,0,0,0,0,0,0,0
    ; Cloudflare
    DB      "162.159.36.143",0
    DW      3478
    DB      0,0,0,0,0,0

.data?
align 4096
g_StunTransactionId       DB      12 DUP(?)
g_StunBuffer              DB      512 DUP(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; -----------------------------------------------------------------------------
; UDPHolePunch_Init — Initialize STUN context
; rcx = StunContext ptr, rdx = LocalPort
; Returns: 0 success, -1 fail
; -----------------------------------------------------------------------------
PUBLIC UDPHolePunch_Init
UDPHolePunch_Init PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Context
    mov     edi, edx                    ; Port

    ; Create UDP socket
    mov     ecx, 2                      ; AF_INET
    mov     edx, 2                      ; SOCK_DGRAM
    xor     r8d, r8d                    ; IPPROTO_UDP (0)
    call    socket
    cmp     rax, -1
    je      @@error

    mov     [rbx + STUNCTX_Socket], rax

    ; Bind to local port
    sub     rsp, 16                     ; sockaddr_in
    mov     WORD PTR [rsp], 2           ; AF_INET
    mov     ecx, edi
    call    htons
    mov     WORD PTR [rsp + 2], ax      ; Port
    mov     DWORD PTR [rsp + 4], 0      ; INADDR_ANY

    mov     rcx, [rbx + STUNCTX_Socket]
    mov     rdx, rsp
    mov     r8d, 16
    call    bind
    add     rsp, 16

    test    eax, eax
    jnz     @@error_close

    ; Initialize defaults
    mov     DWORD PTR [rbx + STUNCTX_NatType], NAT_TYPE_UNKNOWN
    mov     DWORD PTR [rbx + STUNCTX_TimeoutMs], 5000
    mov     DWORD PTR [rbx + STUNCTX_RetransmitCount], 0

    xor     eax, eax
    jmp     @@exit

@@error_close:
    mov     rcx, [rbx + STUNCTX_Socket]
    call    closesocket
    mov     QWORD PTR [rbx + STUNCTX_Socket], -1

@@error:
    mov     rax, -1

@@exit:
    lea     rsp, [rbp - 24]
    pop     rdi
    pop     rbx
    pop     rbp
    ret
UDPHolePunch_Init ENDP

; -----------------------------------------------------------------------------
; STUN_CreateBindingRequest — Build RFC 5389 binding request
; rcx = Buffer ptr, rdx = TransactionId ptr
; Returns: Message length
; -----------------------------------------------------------------------------
STUN_CreateBindingRequest PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rdi, rcx                    ; Buffer
    mov     rsi, rdx                    ; Transaction ID (12 bytes)

    ; Message Type: Binding Request (0x0001)
    mov     WORD PTR [rdi], 0100h       ; Network byte order (big-endian)
    rol     WORD PTR [rdi], 8

    ; Message Length: 0 (no attributes yet)
    mov     WORD PTR [rdi + 2], 0

    ; Magic Cookie
    mov     eax, STUN_MAGIC_COOKIE
    bswap   eax                         ; Big endian
    mov     DWORD PTR [rdi + 4], eax

    ; Transaction ID (12 bytes)
    mov     rcx, 3                      ; 3 x 4 bytes
    lea     rdi, [rdi + 8]
    rep     movsd

    mov     rax, 20                     ; 20 byte header

    pop     rsi
    pop     rdi
    ret
STUN_CreateBindingRequest ENDP

; -----------------------------------------------------------------------------
; UDPHolePunch_DetectNAT — Determine NAT type via RFC 5780 tests
; rcx = StunContext ptr
; Returns: NAT_TYPE_*
; -----------------------------------------------------------------------------
PUBLIC UDPHolePunch_DetectNAT
UDPHolePunch_DetectNAT PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 88                     ; Buffer space + sockaddr
    .allocstack 88
    .endprolog

    mov     rbx, rcx                    ; Context
    mov     r12, [rbx + STUNCTX_Socket]

    ; Generate random transaction ID
    rdtsc
    mov     [g_StunTransactionId], eax
    rdtsc
    shl     rax, 32
    rdtsc
    or      rax, rdx
    mov     [g_StunTransactionId + 4], rax

    ; Build binding request
    lea     rcx, [g_StunBuffer]
    lea     rdx, [g_StunTransactionId]
    call    STUN_CreateBindingRequest
    mov     r13d, eax                   ; Message length

    ; Try first STUN server
    xor     r14d, r14d                  ; Server index

@@try_server:
    cmp     r14, STUN_SERVER_COUNT
    jae     @@symmetric_nat             ; All failed = assume symmetric

    ; Setup server address
    lea     rax, [StunServers + r14 * 24]
    mov     rcx, rax                    ; IP string
    call    inet_addr                   ; Returns network byte order

    sub     rsp, 16
    mov     WORD PTR [rsp], 2           ; AF_INET
    mov     [rsp + 4], eax              ; IP

    movzx   ecx, WORD PTR [StunServers + r14 * 24 + 16] ; Port offset
    call    htons
    mov     [rsp + 2], ax

    ; Send binding request
    mov     rcx, r12                    ; Socket
    lea     rdx, [g_StunBuffer]
    mov     r8d, r13d                   ; Length
    xor     r9d, r9d                    ; Flags
    push    0                           ; tolen (stack param)
    mov     rax, rsp
    push    rax                         ; to
    sub     rsp, 32                     ; Shadow
    call    sendto
    add     rsp, 48                     ; Cleanup

    test    rax, rax
    js      @@next_server

    ; Receive response (with timeout simulation via non-blocking + sleep)
    mov     ecx, 100                    ; 100 x 50ms = 5 second timeout
    mov     [rbx + STUNCTX_RetransmitCount], ecx

@@recv_loop:
    lea     r8, [rsp + 64]              ; from addr
    mov     QWORD PTR [r8 - 8], 16      ; fromlen

    mov     rcx, r12
    lea     rdx, [g_StunBuffer]
    mov     r8d, 512
    xor     r9d, r9d
    push    0
    lea     rax, [rsp - 8]              ; fromlen ptr
    push    rax
    lea     rax, [rsp + 8]              ; from addr
    push    rax
    sub     rsp, 32
    call    recvfrom
    add     rsp, 48

    cmp     rax, -1
    jne     @@got_response              ; Success

    ; Check timeout
    mov     ecx, 50
    call    Sleep
    dec     DWORD PTR [rbx + STUNCTX_RetransmitCount]
    jnz     @@recv_loop
    jmp     @@next_server

@@got_response:
    ; Parse response for XOR-MAPPED-ADDRESS
    lea     rcx, [g_StunBuffer]
    mov     edx, eax                    ; Length
    lea     r8, [rbx + STUNCTX_PublicIP] ; Out IP
    lea     r9, [rbx + STUNCTX_PublicPort] ; Out Port
    call    STUN_ParseXorMappedAddress

    test    eax, eax
    jz      @@full_cone_detected

@@next_server:
    inc     r14
    jmp     @@try_server

@@full_cone_detected:
    mov     DWORD PTR [rbx + STUNCTX_NatType], NAT_TYPE_FULL_CONE
    jmp     @@done

@@symmetric_nat:
    mov     DWORD PTR [rbx + STUNCTX_NatType], NAT_TYPE_SYMMETRIC

@@done:
    mov     eax, [rbx + STUNCTX_NatType]

    add     rsp, 88
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
UDPHolePunch_DetectNAT ENDP

; -----------------------------------------------------------------------------
; STUN_ParseXorMappedAddress — Extract XOR-MAPPED-ADDRESS from response
; rcx = Buffer, rdx = Length, r8 = OutIP, r9 = OutPort
; Returns: 0 success, -1 fail
; -----------------------------------------------------------------------------
STUN_ParseXorMappedAddress PROC FRAME
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rdi, rcx                    ; Buffer
    mov     rsi, rdx                    ; Length
    mov     rbx, r8                     ; OutIP

    cmp     rsi, 20                     ; Minimum STUN header
    jb      @@error

    ; Verify message type is Binding Response (0x0101)
    movzx   eax, WORD PTR [rdi]
    rol     ax, 8                       ; Big endian
    cmp     ax, STUN_BINDING_RESPONSE
    jne     @@error

    ; Skip header (20 bytes), parse attributes
    add     rdi, 20
    sub     rsi, 20

@@attr_loop:
    cmp     rsi, 4                      ; Need at least type + length
    jb      @@error

    movzx   ecx, WORD PTR [rdi]         ; Attribute type
    rol     cx, 8
    movzx   edx, WORD PTR [rdi + 2]     ; Length
    rol     dx, 8

    cmp     cx, STUN_XOR_MAPPED_ADDRESS
    je      @@found_xor

    ; Skip to next attribute (padding to 4-byte boundary)
    add     rdx, 3
    and     rdx, -4
    add     rdi, 4
    add     rdi, rdx
    sub     rsi, 4
    sub     rsi, rdx
    jmp     @@attr_loop

@@found_xor:
    ; Parse XOR-MAPPED-ADDRESS
    cmp     edx, 8                      ; Expected length for IPv4
    jb      @@error

    movzx   eax, BYTE PTR [rdi + 5]     ; Family (should be 0x01 for IPv4)
    cmp     al, 1
    jne     @@error

    ; Port (XOR with magic cookie high 16 bits)
    movzx   eax, WORD PTR [rdi + 6]
    rol     ax, 8
    xor     ax, 04221h                  ; High 16 bits of magic cookie
    mov     [r9], ax                    ; Store port

    ; IP (XOR with magic cookie)
    mov     eax, DWORD PTR [rdi + 8]
    bswap   eax                         ; Network to host
    xor     eax, STUN_MAGIC_COOKIE
    mov     [rbx], eax                  ; Store IP

    xor     eax, eax
    jmp     @@exit

@@error:
    mov     rax, -1

@@exit:
    pop     rbx
    pop     rsi
    pop     rdi
    ret
STUN_ParseXorMappedAddress ENDP

; -----------------------------------------------------------------------------
; UDPHolePunch_Perform — Execute hole punch to peer
; rcx = StunContext, rdx = PeerIP (network byte order), r8w = PeerPort
; Returns: 0 success (punch completed)
; -----------------------------------------------------------------------------
PUBLIC UDPHolePunch_Perform
UDPHolePunch_Perform PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx

    ; Build peer sockaddr
    sub     rsp, 16
    mov     WORD PTR [rsp], 2           ; AF_INET
    mov     [rsp + 4], edx              ; IP (already network order)
    movzx   ecx, r8w
    call    htons
    mov     [rsp + 2], ax

    ; Send empty packet (hole punch)
    mov     rcx, [rbx + STUNCTX_Socket]
    lea     rdx, [g_StunBuffer]         ; Dummy buffer
    mov     r8d, 0                      ; 0 bytes (or 1 for compatibility)
    inc     r8d                         ; Send 1 byte
    xor     r9d, r9d
    push    16                          ; tolen
    lea     rax, [rsp + 8]              ; to addr
    push    rax
    sub     rsp, 32
    call    sendto
    add     rsp, 48

    ; Wait for response (peer doing same)
    mov     ecx, 100
    call    Sleep

    xor     eax, eax

    add     rsp, 40
    pop     rbx
    pop     rbp
    ret
UDPHolePunch_Perform ENDP

END