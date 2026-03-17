; =============================================================================
; rtp_stream_parser.asm — x64 MASM — RTP stream parser (Batch 2)
; Incremental byte parser for RTP packets in token/output streams.
; =============================================================================
OPTION CASEMAP:NONE

PUBLIC RTP_StreamParser_Init
PUBLIC RTP_StreamParser_Reset
PUBLIC RTP_StreamParser_PushByte
PUBLIC RTP_StreamParser_GetPacket
PUBLIC RTP_StreamParser_GetState

RTP_PACKET_MAGIC        equ 021505452h ; 'RTP!'
RTP_PACKET_HEADER_SIZE  equ 52
RTP_PACKET_VERSION      equ 1
RTP_STREAM_CAP          equ 8192

RTPP_magic              equ 0
RTPP_version            equ 4
RTPP_header_size        equ 6
RTPP_payload_size       equ 24

RTP_SP_SYNC0            equ 0
RTP_SP_SYNC1            equ 1
RTP_SP_SYNC2            equ 2
RTP_SP_SYNC3            equ 3
RTP_SP_HEADER           equ 4
RTP_SP_PAYLOAD          equ 5
RTP_SP_READY            equ 6

.data
ALIGN 4
g_spState               dd RTP_SP_SYNC0
g_spPos                 dd 0
g_spExpected            dd RTP_PACKET_HEADER_SIZE
g_spPacketSize          dd 0

ALIGN 8
g_spBuffer              db RTP_STREAM_CAP dup(0)

.code

RTP_StreamParser_Reset PROC
    mov     dword ptr [g_spState], RTP_SP_SYNC0
    mov     dword ptr [g_spPos], 0
    mov     dword ptr [g_spExpected], RTP_PACKET_HEADER_SIZE
    mov     dword ptr [g_spPacketSize], 0
    ret
RTP_StreamParser_Reset ENDP

RTP_StreamParser_Init PROC
    jmp     RTP_StreamParser_Reset
RTP_StreamParser_Init ENDP

RTP_StreamParser_GetState PROC
    mov     eax, dword ptr [g_spState]
    ret
RTP_StreamParser_GetState ENDP

RTP_StreamParser_PushByte PROC
    ; RCX = byte value (low 8 bits)
    movzx   eax, cl
    mov     edx, dword ptr [g_spState]

    cmp     edx, RTP_SP_READY
    je      @@ready

    cmp     edx, RTP_SP_SYNC0
    je      @@sync0
    cmp     edx, RTP_SP_SYNC1
    je      @@sync1
    cmp     edx, RTP_SP_SYNC2
    je      @@sync2
    cmp     edx, RTP_SP_SYNC3
    je      @@sync3
    cmp     edx, RTP_SP_HEADER
    je      @@header
    cmp     edx, RTP_SP_PAYLOAD
    je      @@payload

    call    RTP_StreamParser_Reset
    xor     eax, eax
    ret

@@sync0:
    cmp     al, 'R'
    jne     @@no_progress
    mov     byte ptr [g_spBuffer + 0], al
    mov     dword ptr [g_spPos], 1
    mov     dword ptr [g_spState], RTP_SP_SYNC1
    xor     eax, eax
    ret
@@sync1:
    cmp     al, 'T'
    jne     @@resync_r
    mov     byte ptr [g_spBuffer + 1], al
    mov     dword ptr [g_spPos], 2
    mov     dword ptr [g_spState], RTP_SP_SYNC2
    xor     eax, eax
    ret
@@sync2:
    cmp     al, 'P'
    jne     @@resync_r
    mov     byte ptr [g_spBuffer + 2], al
    mov     dword ptr [g_spPos], 3
    mov     dword ptr [g_spState], RTP_SP_SYNC3
    xor     eax, eax
    ret
@@sync3:
    cmp     al, '!'
    jne     @@resync_r
    mov     byte ptr [g_spBuffer + 3], al
    mov     dword ptr [g_spPos], 4
    mov     dword ptr [g_spExpected], RTP_PACKET_HEADER_SIZE
    mov     dword ptr [g_spState], RTP_SP_HEADER
    xor     eax, eax
    ret

@@header:
    mov     ecx, dword ptr [g_spPos]
    cmp     ecx, RTP_STREAM_CAP
    jae     @@hard_reset
    mov     byte ptr [g_spBuffer + rcx], al
    inc     ecx
    mov     dword ptr [g_spPos], ecx
    cmp     ecx, RTP_PACKET_HEADER_SIZE
    jb      @@no_progress

    ; Validate base header fields
    mov     eax, dword ptr [g_spBuffer + RTPP_magic]
    cmp     eax, RTP_PACKET_MAGIC
    jne     @@hard_reset

    movzx   eax, word ptr [g_spBuffer + RTPP_version]
    cmp     eax, RTP_PACKET_VERSION
    jne     @@hard_reset

    movzx   eax, word ptr [g_spBuffer + RTPP_header_size]
    cmp     eax, RTP_PACKET_HEADER_SIZE
    jne     @@hard_reset

    mov     eax, dword ptr [g_spBuffer + RTPP_payload_size]
    add     eax, RTP_PACKET_HEADER_SIZE
    cmp     eax, RTP_STREAM_CAP
    ja      @@hard_reset
    mov     dword ptr [g_spPacketSize], eax
    mov     dword ptr [g_spExpected], eax

    cmp     eax, RTP_PACKET_HEADER_SIZE
    je      @@mark_ready

    mov     dword ptr [g_spState], RTP_SP_PAYLOAD
    xor     eax, eax
    ret

@@payload:
    mov     ecx, dword ptr [g_spPos]
    cmp     ecx, RTP_STREAM_CAP
    jae     @@hard_reset
    mov     byte ptr [g_spBuffer + ecx], al
    inc     ecx
    mov     dword ptr [g_spPos], ecx
    cmp     ecx, dword ptr [g_spExpected]
    jb      @@no_progress

@@mark_ready:
    mov     dword ptr [g_spState], RTP_SP_READY
    mov     eax, 1
    ret

@@resync_r:
    ; if current byte is 'R', treat it as first sync byte; else reset
    cmp     al, 'R'
    jne     @@hard_reset
    mov     byte ptr [g_spBuffer + 0], al
    mov     dword ptr [g_spPos], 1
    mov     dword ptr [g_spState], RTP_SP_SYNC1
    xor     eax, eax
    ret

@@hard_reset:
    call    RTP_StreamParser_Reset
    mov     eax, -1
    ret

@@no_progress:
    xor     eax, eax
    ret

@@ready:
    mov     eax, 1
    ret
RTP_StreamParser_PushByte ENDP

RTP_StreamParser_GetPacket PROC
    ; RCX = out_buf, RDX = out_cap, R8 = out_written ptr (optional)
    test    rcx, rcx
    jz      @@bad
    cmp     dword ptr [g_spState], RTP_SP_READY
    jne     @@not_ready

    mov     eax, dword ptr [g_spPacketSize]
    test    eax, eax
    jz      @@bad
    cmp     eax, edx
    ja      @@short

    mov     r9d, eax
    xor     r10d, r10d
@@copy:
    mov     al, byte ptr [g_spBuffer + r10]
    mov     byte ptr [rcx + r10], al
    inc     r10d
    cmp     r10d, r9d
    jb      @@copy

    test    r8, r8
    jz      @@ret_ok
    mov     dword ptr [r8], r9d
@@ret_ok:
    call    RTP_StreamParser_Reset
    mov     eax, 0
    ret
@@not_ready:
    mov     eax, 1
    ret
@@short:
    mov     eax, -2
    ret
@@bad:
    mov     eax, -1
    ret
RTP_StreamParser_GetPacket ENDP

END
