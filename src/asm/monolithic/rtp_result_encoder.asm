; =============================================================================
; rtp_result_encoder.asm — x64 MASM — RTP tool-result frame encoder (Batch 2)
; Encodes binary result frames for model/tool feedback path.
; =============================================================================
OPTION CASEMAP:NONE

PUBLIC RTP_EncodeToolResultFrame

RTP_RESULT_MAGIC        equ 052505452h ; 'RTPR'
RTP_RESULT_VERSION      equ 1
RTP_RESULT_HEADER_SIZE  equ 24

RTPR_magic              equ 0
RTPR_version            equ 4
RTPR_header_size        equ 6
RTPR_call_id            equ 8
RTPR_status             equ 16
RTPR_payload_size       equ 20

.code

RTP_EncodeToolResultFrame PROC FRAME
    ; Win64 ABI
    ; RCX = call_id
    ; RDX = status_code
    ; R8  = payload ptr
    ; R9  = payload_size
    ; [rsp+40] = out_buf
    ; [rsp+48] = out_cap
    ; [rsp+56] = out_written ptr

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     r10, qword ptr [rsp + 32 + 8]   ; out_buf
    mov     r11d, dword ptr [rsp + 32 + 16] ; out_cap
    mov     rbx, qword ptr [rsp + 32 + 24]  ; out_written ptr

    test    r10, r10
    jz      @@bad

    mov     eax, r9d
    add     eax, RTP_RESULT_HEADER_SIZE
    cmp     eax, r11d
    ja      @@short

    ; Header
    mov     dword ptr [r10 + RTPR_magic], RTP_RESULT_MAGIC
    mov     word ptr  [r10 + RTPR_version], RTP_RESULT_VERSION
    mov     word ptr  [r10 + RTPR_header_size], RTP_RESULT_HEADER_SIZE
    mov     qword ptr [r10 + RTPR_call_id], rcx
    mov     dword ptr [r10 + RTPR_status], edx
    mov     dword ptr [r10 + RTPR_payload_size], r9d

    ; Payload copy (optional)
    test    r8, r8
    jz      @@done_copy
    test    r9d, r9d
    jz      @@done_copy

    lea     rsi, [r8]
    lea     rdi, [r10 + RTP_RESULT_HEADER_SIZE]
    mov     ecx, r9d
@@copy:
    mov     al, byte ptr [rsi]
    mov     byte ptr [rdi], al
    inc     rsi
    inc     rdi
    dec     ecx
    jnz     @@copy

@@done_copy:
    mov     eax, r9d
    add     eax, RTP_RESULT_HEADER_SIZE
    test    rbx, rbx
    jz      @@ok
    mov     dword ptr [rbx], eax
@@ok:
    xor     eax, eax
    jmp     @@ret

@@short:
    mov     eax, -2
    jmp     @@ret
@@bad:
    mov     eax, -1

@@ret:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RTP_EncodeToolResultFrame ENDP

END
