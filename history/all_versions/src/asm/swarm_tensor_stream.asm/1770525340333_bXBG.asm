; =============================================================================
; swarm_tensor_stream.asm — Zero-copy tensor network streaming
; =============================================================================
; Target: x64, AVX-512 for optional compression before transmit
; Implements: scatter/gather layer shards across TCP sockets for
;             Phase 21 Distributed Swarm Inference (800B model sharding)
;
; Wire Protocol:
;   8-byte header: [magic:4][type:2][size:2] + variable payload
;   Magic: 'RAWR' = 0x52574152
;   TCP Port 7947 for data, UDP Port 7946 for discovery
;
; Responsibilities:
;   1. swarm_stream_layer — Serialize a layer to a TCP socket with header framing
;   2. swarm_receive_header — Non-blocking header read for event loop integration
;   3. swarm_compute_layer_crc32 — CRC32 checksum for layer data integrity
;   4. swarm_compress_chunk_rle — Simple RLE compression for sparse layers
;   5. swarm_build_discovery_packet — Construct UDP discovery beacon
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT
; Dependencies: ws2_32.dll (via C++ winsock2 init)
; Build: ml64.exe /c /Zi /Zd /Fo swarm_tensor_stream.obj swarm_tensor_stream.asm
; Link:  Linked into RawrEngine.exe / RawrXD-Win32IDE.exe
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

.CODE
ALIGN 64

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC swarm_stream_layer
PUBLIC swarm_receive_header
PUBLIC swarm_compute_layer_crc32
PUBLIC swarm_compress_chunk_rle
PUBLIC swarm_build_discovery_packet

; =============================================================================
;                        SWARM PROTOCOL CONSTANTS
; =============================================================================
RAWX_MAGIC              EQU     052574152h      ; 'RAWR' in little-endian

; Message types (uint16_t)
MSG_LAYER_REQUEST       EQU     1               ; Request layer from peer
MSG_LAYER_PAYLOAD       EQU     2               ; Layer data follows
MSG_VRAM_PRESSURE       EQU     3               ; Node pressure update
MSG_HEARTBEAT           EQU     4               ; Keepalive
MSG_REBALANCE_CMD       EQU     5               ; Coordinator migration order
MSG_DISCOVERY           EQU     6               ; LAN discovery beacon
MSG_LAYER_ACK           EQU     7               ; Acknowledgment after layer received
MSG_SHUTDOWN            EQU     0FFFFh          ; Shutdown signal

; Header layout (32 bytes)
; [0:3]   magic       (uint32_t)
; [4:5]   msg_type    (uint16_t)
; [6:7]   header_size (uint16_t) — always 32
; [8:15]  payload_size (uint64_t)
; [16:19] quant_type  (uint32_t)
; [20:23] crc32       (uint32_t)
; [24:31] reserved    (uint64_t)
HEADER_SIZE             EQU     32
CHUNK_SIZE              EQU     65536           ; 64KB transmit chunks

; =============================================================================
; EXTERN: WinSock2 send/recv (resolved at link time via ws2_32.lib)
; =============================================================================
EXTERN __imp_send:PROC
EXTERN __imp_recv:PROC

; =============================================================================
; swarm_stream_layer — Serialize layer to TCP socket with header + chunked data
; =============================================================================
; int64_t swarm_stream_layer(
;     uint64_t socket_handle,   ; RCX — SOCKET (from accept/connect)
;     void* layer_data,         ; RDX — pointer to layer tensor data
;     uint64_t layer_size,      ; R8  — total layer size in bytes
;     uint32_t quant_type       ; R9D — quantization type for metadata
; );
;
; Returns: RAX = total bytes sent (header + payload), -1 on error
; Clobbers: R10-R15, XMM0-XMM3
;
; Protocol:
;   1. Build 32-byte header with magic, MSG_LAYER_PAYLOAD, payload size, quant type
;   2. Compute CRC32 of layer data
;   3. Send header (32 bytes)
;   4. Stream payload in 64KB chunks via send()
;   5. Return total bytes transferred
; =============================================================================
swarm_stream_layer PROC PUBLIC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
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
    push    r15
    .pushreg r15
    sub     rsp, 96             ; 32-byte header + shadow space + alignment
    .allocstack 96
    .endprolog

    ; Save parameters
    mov     r12, rcx            ; socket
    mov     r13, rdx            ; layer_data
    mov     r14, r8             ; layer_size
    mov     r15d, r9d           ; quant_type

    ; Validate socket
    cmp     r12, -1             ; INVALID_SOCKET
    je      @@stream_error

    ; Validate data pointer
    test    r13, r13
    jz      @@stream_error

    ; ─── Compute CRC32 of layer data ───
    mov     rcx, r13            ; data pointer
    mov     rdx, r14            ; size
    call    swarm_compute_layer_crc32
    mov     ebx, eax            ; Save CRC32 in EBX

    ; ─── Build header on stack [rsp+0..31] ───
    mov     DWORD PTR [rsp+0], RAWX_MAGIC           ; magic
    mov     WORD PTR [rsp+4], MSG_LAYER_PAYLOAD      ; msg_type
    mov     WORD PTR [rsp+6], HEADER_SIZE            ; header_size = 32
    mov     QWORD PTR [rsp+8], r14                   ; payload_size
    mov     DWORD PTR [rsp+16], r15d                 ; quant_type
    mov     DWORD PTR [rsp+20], ebx                  ; crc32
    mov     QWORD PTR [rsp+24], 0                    ; reserved

    ; ─── Send header (32 bytes) ───
    ; int send(SOCKET s, const char* buf, int len, int flags);
    ; Windows x64 ABI: RCX=socket, RDX=buf, R8=len, R9=flags
    mov     rcx, r12
    lea     rdx, [rsp+0]
    mov     r8d, HEADER_SIZE
    xor     r9d, r9d            ; flags = 0
    call    QWORD PTR [__imp_send]

    ; Check for error
    cmp     eax, HEADER_SIZE
    jne     @@stream_error

    ; Track total bytes sent
    mov     rsi, HEADER_SIZE    ; total_sent = 32 (header already sent)

    ; ─── Stream payload in 64KB chunks ───
    mov     rdi, r14            ; remaining = layer_size
    mov     rbx, r13            ; current_ptr = layer_data

ALIGN 16
@@chunk_loop:
    ; Check if all data sent
    test    rdi, rdi
    jz      @@stream_success

    ; Compute chunk size = min(remaining, CHUNK_SIZE)
    mov     r8, CHUNK_SIZE
    cmp     rdi, r8
    cmovb   r8, rdi             ; r8 = actual chunk size

    ; send(socket, current_ptr, chunk_size, 0)
    mov     rcx, r12            ; socket
    mov     rdx, rbx            ; buffer
    ; r8 already set to chunk size
    xor     r9d, r9d            ; flags = 0
    call    QWORD PTR [__imp_send]

    ; Check for error
    test    eax, eax
    jle     @@stream_error      ; 0 = connection closed, <0 = error

    ; Advance pointers
    ; NOTE: send() may return less than requested (partial send)
    ; We use the actual bytes sent (in EAX)
    movsxd  rax, eax            ; Sign-extend send result
    add     rbx, rax            ; Advance buffer pointer
    sub     rdi, rax            ; Decrease remaining
    add     rsi, rax            ; Increase total sent
    jmp     @@chunk_loop

@@stream_success:
    mov     rax, rsi            ; Return total bytes sent
    jmp     @@stream_cleanup

@@stream_error:
    mov     rax, -1             ; Return -1 on error

@@stream_cleanup:
    add     rsp, 96
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

swarm_stream_layer ENDP

; =============================================================================
; swarm_receive_header — Read and validate a 32-byte header from socket
; =============================================================================
; int swarm_receive_header(
;     uint64_t socket_handle,    ; RCX — SOCKET
;     void* header_buffer        ; RDX — pointer to 32-byte output buffer
; );
;
; Returns: RAX = 0 success (valid header), 1 incomplete read, -1 error/invalid
; =============================================================================
swarm_receive_header PROC PUBLIC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32             ; Shadow space
    .allocstack 32
    .endprolog

    mov     rbx, rcx            ; socket
    mov     rsi, rdx            ; header buffer

    ; Validate inputs
    cmp     rbx, -1
    je      @@recv_error
    test    rsi, rsi
    jz      @@recv_error

    ; recv(socket, buffer, 32, 0)
    mov     rcx, rbx            ; socket
    mov     rdx, rsi            ; buffer
    mov     r8d, HEADER_SIZE    ; 32 bytes
    xor     r9d, r9d            ; flags = 0
    call    QWORD PTR [__imp_recv]

    ; Check result
    cmp     eax, HEADER_SIZE
    je      @@recv_validate

    ; Partial read or error
    test    eax, eax
    jg      @@recv_incomplete   ; Got some bytes but not enough
    jmp     @@recv_error        ; 0 = closed, <0 = error

@@recv_validate:
    ; Verify magic bytes
    mov     eax, DWORD PTR [rsi]
    cmp     eax, RAWX_MAGIC
    jne     @@recv_error        ; Invalid magic = corrupted/wrong protocol

    ; Verify header_size field matches
    movzx   eax, WORD PTR [rsi+6]
    cmp     eax, HEADER_SIZE
    jne     @@recv_error        ; Unexpected header size

    ; Verify msg_type is in valid range
    movzx   eax, WORD PTR [rsi+4]
    cmp     eax, MSG_SHUTDOWN
    ja      @@recv_error        ; Unknown message type (unless shutdown)

    ; Header is valid
    xor     rax, rax            ; Return 0 = success
    jmp     @@recv_cleanup

@@recv_incomplete:
    mov     rax, 1              ; Return 1 = incomplete
    jmp     @@recv_cleanup

@@recv_error:
    mov     rax, -1             ; Return -1 = error

@@recv_cleanup:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

swarm_receive_header ENDP

; =============================================================================
; swarm_compute_layer_crc32 — CRC32 checksum of layer data
; =============================================================================
; uint32_t swarm_compute_layer_crc32(
;     const void* data,          ; RCX — data pointer
;     uint64_t size              ; RDX — data size in bytes
; );
;
; Returns: EAX = CRC32C value
; Uses: Hardware CRC32 instruction (SSE4.2)
; =============================================================================
swarm_compute_layer_crc32 PROC PUBLIC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Validate
    test    rcx, rcx
    jz      @@crc_zero
    test    rdx, rdx
    jz      @@crc_zero

    ; Initial CRC value
    mov     eax, 0FFFFFFFFh     ; Standard CRC32 initial value

    ; Process 8 bytes at a time using CRC32 instruction
    mov     rbx, rdx
    shr     rbx, 3              ; Number of 8-byte chunks

    test    rbx, rbx
    jz      @@crc_bytes

ALIGN 16
@@crc_qword_loop:
    crc32   rax, QWORD PTR [rcx]
    add     rcx, 8
    dec     rbx
    jnz     @@crc_qword_loop

@@crc_bytes:
    ; Process remaining bytes (0-7)
    and     rdx, 7              ; remaining = size % 8
    test    rdx, rdx
    jz      @@crc_finalize

@@crc_byte_loop:
    crc32   eax, BYTE PTR [rcx]
    inc     rcx
    dec     rdx
    jnz     @@crc_byte_loop

@@crc_finalize:
    xor     eax, 0FFFFFFFFh     ; Final XOR
    pop     rbx
    ret

@@crc_zero:
    xor     eax, eax
    pop     rbx
    ret

swarm_compute_layer_crc32 ENDP

; =============================================================================
; swarm_compress_chunk_rle — RLE compression for sparse layer data
; =============================================================================
; uint64_t swarm_compress_chunk_rle(
;     const void* src,           ; RCX — source data
;     uint64_t src_size,         ; RDX — source size in bytes
;     void* dst,                 ; R8  — destination buffer (must be >= src_size * 2)
;     uint64_t dst_capacity      ; R9  — destination buffer capacity
; );
;
; Returns: RAX = compressed size in bytes, 0 if compression would expand data
;
; Format: [count:1][value:1] pairs for repeated bytes
;         count=0 means literal run follows: [0][length:1][bytes...]
;         Max run length: 255
; =============================================================================
swarm_compress_chunk_rle PROC PUBLIC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@rle_zero
    test    rdx, rdx
    jz      @@rle_zero
    test    r8, r8
    jz      @@rle_zero

    mov     rsi, rcx            ; src pointer
    mov     rbx, rdx            ; src_size
    mov     rdi, r8             ; dst pointer
    mov     r12, r9             ; dst_capacity

    xor     r8, r8              ; src_offset = 0
    xor     r9, r9              ; dst_offset = 0

ALIGN 16
@@rle_loop:
    cmp     r8, rbx
    jge     @@rle_done

    ; Count consecutive identical bytes
    movzx   eax, BYTE PTR [rsi+r8]     ; Current byte value
    mov     rcx, r8
    inc     rcx                         ; Start counting from next byte
    xor     edx, edx                    ; run_length = 0

@@count_run:
    cmp     rcx, rbx
    jge     @@run_counted
    cmp     edx, 254                    ; Max run = 255
    jge     @@run_counted
    movzx   r10d, BYTE PTR [rsi+rcx]
    cmp     r10d, eax
    jne     @@run_counted
    inc     edx
    inc     rcx
    jmp     @@count_run

@@run_counted:
    ; Check if we have room in dst
    lea     r10, [r9+2]
    cmp     r10, r12
    jge     @@rle_zero          ; Overflow — abandon compression

    ; Write [count+1][value]
    inc     edx                 ; Total count (including first byte)
    mov     BYTE PTR [rdi+r9], dl
    mov     BYTE PTR [rdi+r9+1], al
    add     r9, 2
    add     r8, rdx             ; Advance src by run_length

    jmp     @@rle_loop

@@rle_done:
    ; Check if compression actually saved space
    cmp     r9, rbx
    jge     @@rle_zero          ; Compressed is bigger — return 0

    mov     rax, r9             ; Return compressed size
    jmp     @@rle_exit

@@rle_zero:
    xor     rax, rax            ; Return 0 = no compression benefit

@@rle_exit:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

swarm_compress_chunk_rle ENDP

; =============================================================================
; swarm_build_discovery_packet — Build UDP discovery beacon
; =============================================================================
; uint32_t swarm_build_discovery_packet(
;     void* buffer,              ; RCX — output buffer (min 128 bytes)
;     uint32_t buffer_size,      ; EDX — buffer capacity
;     uint64_t total_vram,       ; R8  — total VRAM in bytes
;     uint64_t free_vram,        ; R9  — free VRAM in bytes
;     [rsp+40] uint32_t role     ; node role (0=Coordinator, 1=Worker, 2=Hybrid)
;     [rsp+48] uint32_t max_layers ; max layers this node can host
; );
;
; Returns: EAX = packet size in bytes, 0 on error
;
; Packet layout (64 bytes):
;   [0:3]   magic ('RAWR')
;   [4:5]   msg_type (MSG_DISCOVERY)
;   [6:7]   packet_size
;   [8:15]  total_vram
;   [16:23] free_vram
;   [24:27] role
;   [28:31] max_layers
;   [32:39] timestamp (RDTSC)
;   [40:63] reserved (zero)
; =============================================================================
swarm_build_discovery_packet PROC PUBLIC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Validate buffer
    test    rcx, rcx
    jz      @@disc_error
    cmp     edx, 64
    jl      @@disc_error

    mov     rbx, rcx            ; buffer pointer

    ; Write header
    mov     DWORD PTR [rbx+0], RAWX_MAGIC
    mov     WORD PTR [rbx+4], MSG_DISCOVERY
    mov     WORD PTR [rbx+6], 64        ; packet_size

    ; Write VRAM info
    mov     QWORD PTR [rbx+8], r8       ; total_vram
    mov     QWORD PTR [rbx+16], r9      ; free_vram

    ; Write role and max_layers from stack args
    mov     eax, DWORD PTR [rsp+40]     ; role (adjusted for push rbx = +8)
    mov     DWORD PTR [rbx+24], eax
    mov     eax, DWORD PTR [rsp+48]     ; max_layers
    mov     DWORD PTR [rbx+28], eax

    ; Timestamp via RDTSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rbx+32], rax

    ; Zero reserved area
    xor     rax, rax
    mov     QWORD PTR [rbx+40], rax
    mov     QWORD PTR [rbx+48], rax
    mov     QWORD PTR [rbx+56], rax

    ; Return packet size
    mov     eax, 64
    pop     rbx
    ret

@@disc_error:
    xor     eax, eax
    pop     rbx
    ret

swarm_build_discovery_packet ENDP

END
