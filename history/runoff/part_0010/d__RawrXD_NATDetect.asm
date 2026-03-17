; =============================================================================
; RawrXD_NATDetect.asm - STUN-based NAT Type Detection
; Implements RFC 8489 STUN Binding Request/Response
; =============================================================================

INCLUDE ksamd64.inc

EXTRN   sendto:PROC
EXTRN   recvfrom:PROC
EXTRN   socket:PROC
EXTRN   bind:PROC
EXTRN   closesocket:PROC
EXTRN   inet_addr:PROC
EXTRN   htons:PROC
EXTRN   select:PROC
EXTRN   __imp_RtlIpv4StringToAddressA:QWORD

; STUN constants
STUN_BINDING_REQUEST    EQU 0x0001
STUN_BINDING_RESPONSE   EQU 0x0101
STUN_MAGIC_COOKIE       EQU 0x2112A442

.data
stunServer:
    DQ "stun.l.google.com", 0

.code

PUBLIC DetectNATType
DetectNATType PROC FRAME
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
    sub     rsp, 88
    .allocstack 88
    .endprolog

    mov     rbx, rcx        ; socket
    mov     rdi, rdx        ; server addr
    mov     rsi, r8         ; output addr

    ; Build STUN Binding Request (20 bytes header + 8 bytes attrs)
    ; Message Type: 0x0001 (Binding Request)
    ; Magic Cookie: 0x2112A442
    mov     DWORD PTR [rsp], 00010000h      ; Type + Length
    mov     eax, STUN_MAGIC_COOKIE
    mov     DWORD PTR [rsp+4], eax          ; Magic Cookie
    rdtsc                                   ; Random Transaction ID
    mov     [rsp+8], rax
    rdtsc
    mov     [rsp+16], rax

    ; Send to STUN server
    mov     rcx, rbx
    lea     rdx, [rsp]
    mov     r8d, 20                         ; len
    xor     r9d, r9d                        ; flags
    mov     QWORD PTR [rsp+32], rdi         ; addr
    mov     DWORD PTR [rsp+40], 16          ; addr len
    call    sendto
    cmp     rax, -1
    je      @@fail

    ; Receive with timeout (5 seconds)
    ; Set timeout for select
    mov     DWORD PTR [rsp+48], 0           ; tv_sec = 0
    mov     DWORD PTR [rsp+52], 5000000     ; tv_usec = 5,000,000 (5s)

    lea     rcx, [rsp+56]                   ; fd_set
    mov     DWORD PTR [rcx], 1              ; fd_count
    mov     [rcx+4], rbx                    ; fd_array[0]

    xor     ecx, ecx                        ; nfds
    lea     rdx, [rsp+56]                   ; readfds
    xor     r8d, r8d                        ; writefds
    xor     r9d, r9d                        ; exceptfds
    lea     rax, [rsp+48]                   ; timeout
    mov     QWORD PTR [rsp+32], rax
    call    select

    test    eax, eax
    jle     @@fail

    ; Receive response
    mov     rcx, rbx
    lea     rdx, [rsp+64]                   ; recv buffer
    mov     r8d, 88
    xor     r9d, r9d
    call    recvfrom

    ; Parse XOR-MAPPED-ADDRESS (Attribute Type 0x0020)
    lea     rcx, [rsp+64]
    mov     edx, 20                         ; Skip header
    call    ParseXORMappedAddress
    mov     [rsi], rax                      ; Store mapped IP:port

    ; NAT type heuristics (simplified):
    ; Compare local addr vs mapped addr
    ; If same: Full Cone (or no NAT)
    ; If different: Send to another STUN server to detect Symmetric vs Restricted

    mov     eax, 1          ; Return NAT type (1=FullCone for now)
    jmp     @@exit

@@fail:
    xor     eax, eax        ; Unknown/error

@@exit:
    add     rsp, 88
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
DetectNATType ENDP

; Parse XOR-MAPPED-ADDRESS
; rcx = packet ptr, edx = offset to attributes
ParseXORMappedAddress PROC
    push    rdi
    mov     rdi, rcx
    add     rdi, rdx        ; Point to attributes

@@scan_loop:
    movzx   eax, WORD PTR [rdi]     ; Attribute type
    cmp     ax, 0020h               ; XOR-MAPPED-ADDRESS
    je      @@found
    movzx   ecx, WORD PTR [rdi+2]   ; Attribute length
    add     rdi, 4
    add     rdi, rcx
    add     rdi, 3
    and     rdi, -4                 ; Pad to 4 bytes
    jmp     @@scan_loop

@@found:
    ; XOR decoding: Port ^ (Magic Cookie >> 16)
    ; IP ^ Magic Cookie
    movzx   eax, WORD PTR [rdi+6]   ; X-Port
    xor     ax, 2112h               ; XOR with cookie high bits
    shl     eax, 16

    mov     ecx, DWORD PTR [rdi+8]  ; X-Address (IPv4)
    xor     ecx, STUN_MAGIC_COOKIE  ; XOR with cookie
    or      eax, ecx                ; Return IP:Port in RAX

    pop     rdi
    ret
ParseXORMappedAddress ENDP

END