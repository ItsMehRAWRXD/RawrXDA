; ============================================================================
; zccf_upload.asm — ZCCF MASM64 Fence-Ordered Upload Path
; ============================================================================
; Provides a fast, non-temporal store path for uploading ZCCF context handles
; from the local arena/table into the LLM-facing payload buffer.
;
; Key property: VMOVNTDQ (non-temporal) stores bypass the L1/L2 caches and
; write directly to memory, avoiding cache pollution when populating large
; payload buffers that will not be immediately re-read by the CPU.
;
; An SFENCE after the non-temporal stores ensures the writes are globally
; visible before the agent dispatch thread reads the payload.
;
; Calling convention: Microsoft x64 ABI
;
; Exported functions:
;
;   ZCCF_UploadHandles
;     RCX = ptr to source handle array (uint32_t*)     — ZCCF handles
;     RDX = ptr to destination payload buffer (void*)  — 16-byte aligned
;     R8  = handle count (uint64_t)
;     Returns RAX = bytes written
;
;   ZCCF_FlushPayloadFence
;     No arguments. Emits SFENCE to order preceding non-temporal stores.
;
; NOTE: VMOVNTDQ requires AVX2. A CPUID check should gate this path; the
; C++ fallback in file_cache.cpp uses memcpy when AVX2 is absent.
; ============================================================================

OPTION DOTNAME

.CODE

; ============================================================================
; ZCCF_UploadHandles
; ============================================================================
; Copies R8 uint32_t handles from [RCX] to [RDX] using:
;   - 128-bit non-temporal stores (MOVNTDQ) for 16-byte aligned destination
;   - scalar stores for the tail
;
; RCX = source  (handle array, naturally aligned)
; RDX = dest    (payload buffer, must be 16-byte aligned)
; R8  = count   (in uint32_t units)
; Returns RAX   = bytes written (= count * 4)
; ============================================================================
ZCCF_UploadHandles PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32                     ; Shadow space

    mov     rsi, rcx                    ; source
    mov     rdi, rdx                    ; dest
    mov     rbx, r8                     ; count (uint32_t units)

    ; Compute byte length into RAX for return value
    mov     rax, rbx
    shl     rax, 2                      ; * 4

    ; Process 4 handles (16 bytes) at a time with MOVNTDQ
    mov     rcx, rbx
    shr     rcx, 2                      ; rcx = groups of 4

.nt_loop:
    test    rcx, rcx
    jz      .scalar_tail

    VMOVDQU xmm0, XMMWORD PTR [rsi]    ; load 16 bytes (4 handles)
    VMOVNTDQ XMMWORD PTR [rdi], xmm0   ; non-temporal store
    add     rsi, 16
    add     rdi, 16
    dec     rcx
    jmp     .nt_loop

.scalar_tail:
    ; Handle remaining < 4 elements
    and     rbx, 3                      ; remainder
.tail_loop:
    test    rbx, rbx
    jz      .done
    mov     ecx, DWORD PTR [rsi]
    mov     DWORD PTR [rdi], ecx
    add     rsi, 4
    add     rdi, 4
    dec     rbx
    jmp     .tail_loop

.done:
    ; RAX already holds byte count
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ZCCF_UploadHandles ENDP

; ============================================================================
; ZCCF_FlushPayloadFence
; ============================================================================
; Orders non-temporal stores before any subsequent reads by the consumer.
; Must be called once after all VMOVNTDQ writes to the payload buffer are done.
; ============================================================================
ZCCF_FlushPayloadFence PROC
    SFENCE
    xor     eax, eax
    ret
ZCCF_FlushPayloadFence ENDP

END
