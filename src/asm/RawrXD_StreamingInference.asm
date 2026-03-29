; RawrXD_StreamingInference.asm
; Zero-copy streaming inference with chunked KV cache
; Assemble: ml64 /c /Zi /O2 /arch:AVX512 RawrXD_StreamingInference.asm

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN AllocateUserPhysicalPages:PROC
EXTERN FreeUserPhysicalPages:PROC
EXTERN memcpy:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC

MEM_RESERVE EQU 2000h
MEM_COMMIT EQU 1000h
MEM_PHYSICAL EQU 400000h
PAGE_READWRITE EQU 04h

;=============================================================================
; CONSTANTS
;=============================================================================
CHUNK_SIZE          EQU 64 * 1024 * 1024    ; 64MB streaming chunks
KV_CACHE_SLOTS      EQU 4096                  ; Max sequence length
MAX_BATCH           EQU 1                     ; Single-user IDE
INFERENCE_MAGIC     EQU 0x52415752494445      ; "RAWRIDE"

; Error codes
RAW_OK              EQU 0
RAW_ERR_NOMEM       EQU 8
RAW_ERR_NOBACKEND   EQU 0xC0000001
RAW_ERR_INVALID     EQU 0xC0000002

;=============================================================================
; STRUCTURES
;=============================================================================
InferenceContext STRUCT 8
    magic           DQ ?
    modelHandle     DQ ?
    kvCachePtr      DQ ?
    kvCacheSize     DQ ?
    tokenBuffer     DQ ?
    maxTokens       DD ?
    currentPos      DD ?
    temperature     DD ?
    topP            DD ?
    topK            DD ?
    rngState        DQ ?
    flags           DD ?
    reserved        DD ?
InferenceContext ENDS

SubmitRequest STRUCT 8
    promptTokens    DQ ?      ; Input token IDs
    promptCount     DD ?
    maxNewTokens    DD ?
    temperature     DD ?
    topP            DD ?
    topK            DD ?
    streamCallback  DQ ?      ; void (*onToken)(int token, float prob, bool last)
    userData        DQ ?
SubmitRequest ENDS

;=============================================================================
; DATA SECTION
;=============================================================================
.DATA
ALIGN 16
g_inferenceLock     DQ 0          ; SRWLOCK
g_activeContext     DQ 0          ; Current InferenceContext*
g_chunkBuffer       DQ 0          ; 64MB staging buffer
g_chunkPhysical     DQ 0          ; Physical allocation for AWE

; Function pointers (filled by GPU backend init)
g_pSubmitInference  DQ 0
g_pGetResult        DQ 0
g_pAllocKVCache     DQ 0
g_pFreeKVCache      DQ 0

;=============================================================================
; CODE SECTION
;=============================================================================
.CODE

;=============================================================================
; InferenceEngine_CreateContext
; Creates streaming inference context with chunked memory mapping
;=============================================================================
InferenceEngine_CreateContext PROC FRAME
    ; rcx = modelHandle, rdx = maxTokens, r8 = flags
    
    push rbx
    push rsi
    push rdi
    push r12
    .allocstack 32
    .endprolog
    
    mov rbx, rcx                    ; modelHandle
    mov r12d, edx                   ; maxTokens
    mov esi, r8d                    ; flags
    
    ; Allocate context structure
    mov ecx, SIZEOF InferenceContext
    call malloc
    test rax, rax
    jz @@error_nomem
    mov rdi, rax                    ; rdi = context
    
    ; Initialize magic and handle
    mov [rdi].InferenceContext.magic, INFERENCE_MAGIC
    mov [rdi].InferenceContext.modelHandle, rbx
    mov [rdi].InferenceContext.maxTokens, r12d
    mov [rdi].InferenceContext.flags, esi
    
    ; Calculate KV cache size: 2 * layers * heads * dim * seq_len * sizeof(fp16)
    ; For now: 2 * 32 * 32 * 128 * 4096 * 2 = ~2GB max, but we chunk it
    mov rax, r12
    shl rax, 12                     ; * 4096 (rough estimate)
    mov [rdi].InferenceContext.kvCacheSize, rax
    
    ; Allocate chunked KV cache using VirtualAlloc with MEM_RESERVE | MEM_COMMIT strategy
    mov rcx, rax
    call InferenceEngine_AllocChunkedKV
    test rax, rax
    jz @@free_context
    
    mov [rdi].InferenceContext.kvCachePtr, rax
    
    ; Allocate token buffer (ring buffer for streaming)
    mov ecx, r12d
    shl ecx, 2                      ; * 4 bytes per token
    add ecx, 4096                   ; padding
    call malloc
    mov [rdi].InferenceContext.tokenBuffer, rax
    
    ; Default sampling params
    mov DWORD PTR [rdi].InferenceContext.temperature, 03f4ccccdh
    mov DWORD PTR [rdi].InferenceContext.topP, 03f733333h
    mov [rdi].InferenceContext.topK, 40
    mov [rdi].InferenceContext.currentPos, 0
    
    ; Register as active context
    lea rcx, g_inferenceLock
    call AcquireSRWLockExclusive
    mov g_activeContext, rdi
    lea rcx, g_inferenceLock
    call ReleaseSRWLockExclusive
    
    mov rax, rdi                    ; Return context
    jmp @@done
    
@@free_context:
    mov rcx, rdi
    call free
    
@@error_nomem:
    mov eax, RAW_ERR_NOMEM

    
@@done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
InferenceEngine_CreateContext ENDP

;=============================================================================
; InferenceEngine_AllocChunkedKV
; Allocates KV cache with demand-paged commit strategy
;=============================================================================
InferenceEngine_AllocChunkedKV PROC FRAME
    ; rcx = totalSize
    
    push rbx
    push rsi
    .allocstack 16
    .endprolog
    
    mov rbx, rcx                    ; Total requested size
    
    ; Strategy: Reserve full address space, commit only working set
    ; This allows "infinite" KV cache that grows as needed
    
    ; Reserve address space (no physical backing yet)
    xor ecx, ecx                    ; lpAddress = let system choose
    mov rdx, rbx                    ; dwSize
    mov r8d, MEM_RESERVE            ; flAllocationType
    mov r9d, PAGE_READWRITE         ; flProtect
    call VirtualAlloc
    test rax, rax
    jz @@fallback
    
    ; Commit first chunk immediately (64MB)
    mov rsi, rax                    ; rsi = reserved base
    mov rcx, rax                    ; lpAddress
    mov rdx, CHUNK_SIZE             ; dwSize (initial commit)
    mov r8d, MEM_COMMIT             ; flAllocationType
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@reserve_failed
    
    ; Success - return reserved region
    mov rax, rsi
    jmp @@done
    
@@reserve_failed:
    ; Clean up reservation
    mov rcx, rsi
    xor edx, edx                    ; dwFreeType = MEM_RELEASE (0x8000)
    mov r8d, 0x8000
    call VirtualFree
    
@@fallback:
    ; Fallback: Allocate physical chunks with AWE if available
    call InferenceEngine_TryAWEAllocation
    test rax, rax
    jnz @@done
    
    ; Last resort: Standard allocation with smaller working set
    shr rbx, 2                      ; Quarter size
    xor ecx, ecx                    ; lpAddress = NULL (let OS choose)
    mov rdx, rbx                    ; dwSize = quartered size
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
@@done:
    pop rsi
    pop rbx
    ret
InferenceEngine_AllocChunkedKV ENDP

;=============================================================================
; InferenceEngine_TryAWEAllocation
; Attempts Address Windowing Extensions for large physical pages
;=============================================================================
InferenceEngine_TryAWEAllocation PROC FRAME
    .endprolog
    ; Check for AWE privilege (SeLockMemoryPrivilege)
    ; Simplified: Try allocation, fall back gracefully
    
    push rbx
    
    ; Allocate physical page frame array
    mov ecx, 1024 * 8                 ; 1024 pages * 8 bytes
    call malloc
    test rax, rax
    jz @@failed
    mov rbx, rax
    
    ; Allocate user physical pages
    mov rcx, rbx                      ; pageArray
    mov edx, 1024                     ; numberOfPages
    mov r8d, 0                        ; pageSize (default)
    call AllocateUserPhysicalPages
    test rax, rax
    jz @@cleanup
    
    ; Map physical pages to virtual address space
    xor ecx, ecx                      ; lpAddress
    mov edx, 1024 * 4096              ; dwSize (4MB window)
    mov r8d, MEM_RESERVE or MEM_PHYSICAL
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@cleanup_phys
    
    ; Success - store page array for later mapping
    ; (Would need to track this for unmap)
    jmp @@done
    
@@cleanup_phys:
    mov rcx, rbx
    mov edx, 1024
    call FreeUserPhysicalPages
    
@@cleanup:
    mov rcx, rbx
    call free
    
@@failed:
    xor eax, eax
    
@@done:
    pop rbx
    ret
InferenceEngine_TryAWEAllocation ENDP

;=============================================================================
; InferenceEngine_SubmitInference [EXPORT]
; Main entry point for inference requests - STREAMING implementation
;=============================================================================
InferenceEngine_SubmitInference PROC EXPORT FRAME
    ; rcx = InferenceContext*, rdx = SubmitRequest*
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    .allocstack 48
    .endprolog
    
    mov rbx, rcx                    ; Context
    mov rsi, rdx                    ; Request
    
    ; Validate context
    test rbx, rbx
    jz @@error_invalid
    cmp [rbx].InferenceContext.magic, INFERENCE_MAGIC
    jne @@error_invalid
    
    ; Check if GPU backend is available
    mov rax, g_pSubmitInference
    test rax, rax
    jnz @@dispatch_to_gpu           ; GPU backend exists, use it
    
    ; === SOFTWARE FALLBACK: Streaming CPU inference ===
    
    mov edi, [rsi].SubmitRequest.promptCount
    mov r12d, [rsi].SubmitRequest.maxNewTokens
    mov r13, [rsi].SubmitRequest.streamCallback
    mov r14, [rsi].SubmitRequest.userData
    
    ; Copy prompt tokens to context buffer
    mov rcx, [rbx].InferenceContext.tokenBuffer
    mov rdx, [rsi].SubmitRequest.promptTokens
    mov r8d, edi
    shl r8d, 2                      ; * sizeof(int32)
    call memcpy
    
    ; Streaming generation loop
    xor ecx, ecx                    ; token index
    
@@generation_loop:
    cmp ecx, r12d
    jge @@generation_done
    
    ; Ensure KV cache page is committed for this position
    mov eax, ecx
    shr eax, 14                     ; / 16384 tokens per 64MB chunk
    call InferenceEngine_EnsureKVChunkCommitted
    
    ; === Single token forward pass (simplified) ===
    ; In production: Dispatch to GGML/llama.cpp kernel or custom AVX-512 matmul
    
    push rcx
    push r12
    
    ; Call model-specific forward (would be function pointer from model)
    mov rcx, rbx                    ; context
    mov edx, [rbx].InferenceContext.currentPos ; position
    mov r8, [rbx].InferenceContext.tokenBuffer
    mov r9d, [rbx].InferenceContext.currentPos
    call InferenceEngine_ForwardToken_CPU
    
    pop r12
    pop rcx
    
    ; rax = sampled token, xmm0 = probability
    
    ; Store token
    mov rdx, [rbx].InferenceContext.tokenBuffer
    mov r8d, [rbx].InferenceContext.currentPos
    mov [rdx + r8*4], eax          ; Store new token at currentPos
    inc [rbx].InferenceContext.currentPos
    
    ; Call streaming callback if provided
    test r13, r13
    jz @@no_callback
    
    push rcx
    push rax
    
    ; Prepare callback args: token, prob, is_last
    mov ecx, eax                    ; token
    movaps xmm1, xmm0               ; probability (second float arg in Win64 ABI)
    movd eax, xmm0                  ; probability bits (from xmm0)
    mov rdx, r14                    ; userData
    call r13                        ; streamCallback(token, prob, is_last, userData)
    
    pop rax
    pop rcx
    
@@no_callback:
    inc ecx
    jmp @@generation_loop
    
@@generation_done:
    mov eax, RAW_OK
    jmp @@done
    
@@dispatch_to_gpu:
    ; GPU backend available - dispatch to it
    mov rcx, rbx
    mov rdx, rsi
    call rax                        ; g_pSubmitInference(context, request)
    jmp @@done
    
@@error_invalid:
    mov eax, RAW_ERR_INVALID

    
@@done:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
InferenceEngine_SubmitInference ENDP

;=============================================================================
; InferenceEngine_GetResult [EXPORT]
; Retrieves generation results (blocking for non-streaming)
;=============================================================================
InferenceEngine_GetResult PROC EXPORT FRAME
    ; rcx = InferenceContext*, rdx = outputBuffer, r8 = maxTokens, r9 = actualTokens*
    
    push rbx
    push rsi
    push rdi
    .allocstack 24
    .endprolog
    
    mov rbx, rcx
    mov rdi, rdx                    ; output buffer
    mov esi, r8d                    ; max tokens to copy
    
    ; Validate
    test rbx, rbx
    jz @@error
    cmp [rbx].InferenceContext.magic, INFERENCE_MAGIC
    jne @@error
    
    ; Copy generated tokens to output
    mov rax, [rbx].InferenceContext.currentPos
    cmp eax, esi
    cmova eax, esi                  ; min(actual, max)
    mov esi, eax
    
    test esi, esi
    jz @@empty
    
    mov rcx, rdi
    mov rdx, [rbx].InferenceContext.tokenBuffer
    mov r8d, esi
    shl r8d, 2
    call memcpy
    
@@empty:
    ; Return actual count
    test r9, r9
    jz @@no_count
    mov [r9], esi
    
@@no_count:
    mov eax, esi                    ; Return token count
    jmp @@done
    
@@error:
    mov eax, -1
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
InferenceEngine_GetResult ENDP

;=============================================================================
; InferenceEngine_ForwardToken_CPU
; Software fallback: Single token forward pass
;=============================================================================
InferenceEngine_ForwardToken_CPU PROC FRAME
    .endprolog
    ; rcx = context, edx = position, r8 = token buffer, r9d = current length
    
    ; Placeholder: Would contain actual transformer forward pass
    ; For now: Return position as token (echo mode for testing)
    
InferenceEngine_ForwardToken_CPU PROC
    add eax, 1                      ; Next token = position + 1
    
    ret
InferenceEngine_ForwardToken_CPU ENDP

;=============================================================================
; InferenceEngine_EnsureKVChunkCommitted
; Demand-page commit for KV cache chunks
;=============================================================================
InferenceEngine_EnsureKVChunkCommitted PROC FRAME
    ; ecx = chunk index
    
    push rbx
    push rsi
    .allocstack 16
    .endprolog
    
    mov ebx, ecx
    
    ; Calculate address for this chunk
    mov rax, g_activeContext
    mov rsi, [rax].InferenceContext.kvCachePtr
    
    mov eax, ebx
    mov edx, CHUNK_SIZE
    mul edx                         ; rax = chunk index * CHUNK_SIZE
    add rsi, rax                    ; rsi = target address
    
    ; Check if already committed (page guard or tracking bitmap)
    ; Simplified: Always try commit (VirtualAlloc is idempotent for already-committed)
    
    mov rcx, rsi
    mov rdx, CHUNK_SIZE
    mov r8d, MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    pop rsi
    pop rbx
    ret
InferenceEngine_EnsureKVChunkCommitted ENDP

;=============================================================================
; InferenceEngine_DestroyContext
;=============================================================================
InferenceEngine_DestroyContext PROC EXPORT FRAME
    ; rcx = InferenceContext*
    
    push rbx
    .allocstack 8
    .endprolog
    
    mov rbx, rcx
    test rbx, rbx
    jz @@done
    
    ; Unregister
    lea rcx, g_inferenceLock
    call AcquireSRWLockExclusive
    mov g_activeContext, 0
    lea rcx, g_inferenceLock
    call ReleaseSRWLockExclusive
    
    ; Free KV cache
    mov rcx, [rbx].InferenceContext.kvCachePtr
    test rcx, rcx
    jz @@no_kv
    xor edx, edx
    mov r8d, 0x8000                 ; MEM_RELEASE
    call VirtualFree
    
@@no_kv:
    ; Free token buffer
    mov rcx, [rbx].InferenceContext.tokenBuffer
    test rcx, rcx
    jz @@no_tokens
    call free
    
@@no_tokens:
    ; Poison magic
    mov [rbx].InferenceContext.magic, 0
    
    ; Free context
    mov rcx, rbx
    call free
    
@@done:
    pop rbx
    ret
InferenceEngine_DestroyContext ENDP

;=============================================================================
; EXPORTS
;=============================================================================
PUBLIC InferenceEngine_CreateContext
PUBLIC InferenceEngine_SubmitInference
PUBLIC InferenceEngine_GetResult
PUBLIC InferenceEngine_DestroyContext

END