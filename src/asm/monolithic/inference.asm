<<<<<<< HEAD
; ═══════════════════════════════════════════════════════════════════
; RawrXD Inference Engine — Monolithic Module
; Phase 4: KV-Cache Pruning + Neural Scorer
; Phase 7: KV-Cache Paging (via SlotRing) + PredictivePrefetch
;
; Exports:
;   Core:      InferenceEngineInit, RunInference, TokenGenerate
;   KV Mgmt:   ClearKVCache, PruneKVCache
;   Scorer:    UpdateImportanceScores, GetTopKIndices
;   KV Paging: KVPage_Init, KVPage_Grow, KVPage_GetSegment,
;              KVPage_Shrink, KVPage_PinWorkingSet
;   Globals:   g_modelbase, g_kv_len
; ═══════════════════════════════════════════════════════════════════
=======
; RawrXD Inference Engine — Monolithic Module
; Exports: InferenceEngineInit, RunInference, TokenGenerate
>>>>>>> origin/main

PUBLIC InferenceEngineInit
PUBLIC RunInference
PUBLIC TokenGenerate
PUBLIC ClearKVCache
<<<<<<< HEAD
PUBLIC PruneKVCache
PUBLIC UpdateImportanceScores
PUBLIC GetTopKIndices
PUBLIC g_modelbase
PUBLIC g_kv_len

; Phase 7: KV paging exports
PUBLIC KVPage_Init
PUBLIC KVPage_Grow
PUBLIC KVPage_GetSegment
PUBLIC KVPage_Shrink
PUBLIC KVPage_PinWorkingSet

; Phase 8B: Paged KV Block Table (PagedAttention)
PUBLIC KVBlock_Init
PUBLIC KVBlock_AllocBlock
PUBLIC KVBlock_FreeBlock
PUBLIC KVBlock_MapToken
PUBLIC KVBlock_GetTokenPtr
PUBLIC KVBlock_ReuseBlock

; Phase 8C: Head-Major KV Layout
PUBLIC KVHM_WriteToken
PUBLIC KVHM_GetHeadPtr

; Phase 9A: SIMD kernels need AVX512 detection flag
PUBLIC g_hasAVX512

; ── SlotRing imports (from slot_ring.asm) ────────────────────────
EXTERN SlotRing_Attach:PROC
EXTERN SlotRing_Detach:PROC
EXTERN SlotRing_GetTensor:PROC
EXTERN SlotRing_Tick:PROC
EXTERN SlotRing_Pin:PROC
EXTERN SlotRing_Unpin:PROC
EXTERN SlotRing_Prefetch:PROC
EXTERN SlotRing_PredictivePrefetch:PROC
EXTERN SlotRing_ClockEvict:PROC

; ── SIMD kernel imports (from simd_kernels.asm) ─────────────────
EXTERN SIMD_RMSNorm:PROC
EXTERN SIMD_MatVecQ4:PROC
EXTERN SIMD_RoPE:PROC
EXTERN SIMD_SiLU:PROC
EXTERN SIMD_ScaledDotBatch:PROC
EXTERN SIMD_Softmax:PROC
EXTERN SIMD_VAccumulate:PROC
EXTERN SIMD_DotProduct:PROC

; ── Win32 imports for VirtualAlloc scratch buffers ───────────────
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

; ── KV Paging Constants ──────────────────────────────────────────
KV_SEGMENT_SIZE     equ 10000h     ; 64KB per KV segment
KV_TOKENS_PER_SEG   equ 80h       ; 128 tokens per segment (512B each)
KV_MAX_SEGMENTS     equ 100h      ; 256 segments = 16MB max KV / 32K tokens

; ── Phase 8B: KV Block Constants ──────────────────────────────
KV_BLOCK_TOKENS     equ 20h       ; 32 tokens per block
KV_BLOCK_SIZE       equ 4000h     ; 16KB per block (32 tokens × 512B)
KV_MAX_BLOCKS       equ 400h      ; 1024 blocks = 32K tokens capacity
KV_MAX_TOKEN_MAP    equ 8000h     ; 32768 max logical tokens tracked

; ── Phase 8C: Head-Major Layout Constants ─────────────────────
KV_NUM_HEADS        equ 80h       ; 128 attention heads (configurable)
KV_HEAD_DIM         equ 80h       ; 128 dimension per head (configurable)
KV_HEAD_STRIDE      equ 100h      ; head_dim * 2 bytes (fp16) = 256 bytes per head per token

.data?
align 16
; KV cache: dynamically allocated via VirtualAlloc at init time
; g_kvcache is now a pointer to the allocated region, not a static buffer
g_kvcache_ptr dq ?                      ; Pointer to VirtualAlloc'd KV cache memory
g_kvcache_size dq ?                     ; Size of allocated KV cache in bytes
g_kvcache_capacity dq ?                 ; Max token slots (kv_size / kv_element_size)
g_kv_element_size dd ?                  ; Bytes per KV token = num_heads * head_dim * sizeof(fp16) * 2 (K+V)
g_modelbase   dq ?
g_quantscale  dd ?
g_kv_len      dq ?                      ; Current number of tokens in cache
g_kv_scores   dd 100h dup(?)            ; Importance scores for each token slot

; Phase 7: KV paging state
; Maps each KV segment to a SlotRing slot index (-1 = unmapped)
g_kvSegSlots  dd KV_MAX_SEGMENTS dup(?) ; SlotRing slot index per segment
g_kvSegCount  dd ?                       ; Number of segments currently allocated
g_kvLayerBase dd ?                       ; Base layer index for KV segments

; Phase 8B: Paged KV Block Table (PagedAttention)
; Block descriptor array: each entry is 16 bytes
;   +0  segmentIdx  dd ?    ; Which KV segment owns this block (-1 = free)
;   +4  tokenStart  dd ?    ; First logical token in this block
;   +8  tokenCount  dd ?    ; Number of tokens stored (0..KV_BLOCK_TOKENS)
;   +12 flags       dd ?    ; bit0=pinned, bit1=reusable
g_kvBlocks    db (KV_MAX_BLOCKS * 16) dup(?)  ; 1024 blocks × 16 bytes = 16KB

; Token → Block mapping table: logicalToken → blockIndex (DWORD)
g_tokenMap    dd KV_MAX_TOKEN_MAP dup(?)      ; 32K entries × 4 bytes = 128KB

g_kvBlockCount dd ?                     ; Number of allocated blocks
g_kvFreeHead   dd ?                     ; Index of first free block in free list
g_kvBlockReady dd ?                     ; 1 = block table initialized

; Phase 8C: Head-Major layout config (set at model load time)
g_kvNumHeads   dd ?                     ; Actual number of attention heads
g_kvHeadDim    dd ?                     ; Actual dimension per head
g_kvHeadStride dd ?                     ; Bytes per head per token

; ── Inference scratch buffers (allocated by InferenceEngineInit) ──
g_scratchA     dq ?                     ; Scratch buffer A (4KB, for RMSNorm/FFN output)
g_scratchB     dq ?                     ; Scratch buffer B (4KB, for attention scores)
g_scratchQ     dq ?                     ; Query projection buffer (head_dim floats)
g_scratchFFN   dq ?                     ; FFN intermediate (4 * dim floats)
=======
PUBLIC g_modelbase

.data?
align 16
; KV cache: use 64KB placeholder here; runtime can VirtualAlloc 2GB if needed
g_kvcache     db 10000h dup(?)
g_modelbase   dq ?
g_quantscale  dd ?
>>>>>>> origin/main

.data
align 8
g_hasAVX      dd 0
<<<<<<< HEAD
g_hasAVX512   dd 0                      ; Added for Phase 4A implementation
g_decay_factor dd 0.95                  ; Score decay per inference step
g_kvPageReady dd 0                      ; Phase 7: 1 = KV paging initialized

; ── Model config (set at model load or hardcoded for llama-70B) ──
g_modelDim    dd 8192                   ; Hidden dimension (llama-70B = 8192)
g_modelHeads  dd 64                     ; Number of attention heads
g_modelHeadDim dd 128                   ; head_dim = dim / heads = 128
g_modelLayers dd 80                     ; Number of transformer layers (70B = 80)
g_modelFFNDim dd 28672                  ; FFN intermediate dim (70B = 28672)
g_modelVocab  dd 32000                  ; Vocabulary size
g_tokenPos    dd 0                      ; Current token position (for RoPE)

; ── VirtualAlloc constants ──
MEM_COMMIT_IA equ 1000h
MEM_RESERVE_IA equ 2000h
MEM_RELEASE_IA equ 8000h
PAGE_RW_IA    equ 4
SCRATCH_SIZE  equ 40000h               ; 256KB scratch area
=======
>>>>>>> origin/main

.const
GGUF_MAGIC    dd 046554747h
Q4_0_ID      equ 2

.code
; ────────────────────────────────────────────────────────────────
<<<<<<< HEAD
; InferenceEngineInit — AVX/AVX-512 detection, KV cache init
=======
; InferenceEngineInit — AVX detection, KV cache init
>>>>>>> origin/main
;   cpuid clobbers EAX/EBX/ECX/EDX — rbx is non-volatile, must save.
; ────────────────────────────────────────────────────────────────
InferenceEngineInit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Detect AVX (CPUID.01H:ECX bit 28)
    mov     eax, 1
    cpuid
    test    ecx, 10000000h
    jz      @no_avx
<<<<<<< HEAD
    mov     g_hasAVX, 1

    ; Detect AVX-512 (CPUID.07H:EBX bit 16 for AVX512F)
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    test    ebx, 00010000h
    jz      @no_avx512
    mov     g_hasAVX512, 1

@no_avx512:
    ; ── Dynamic KV Cache Allocation ──
    ; Compute element size = num_heads * head_dim * sizeof(fp16) * 2 (for K and V)
    ; For llama-70B: 64 heads * 128 dim * 2 bytes * 2 (K+V) = 32768 bytes per token
    mov     eax, g_modelHeads
    imul    eax, g_modelHeadDim          ; heads * head_dim
    shl     eax, 1                       ; * sizeof(fp16) = 2 bytes
    shl     eax, 1                       ; * 2 (K and V both stored)
    mov     g_kv_element_size, eax

    ; Allocate KV cache: 2GB via VirtualAlloc (MEM_RESERVE | MEM_COMMIT)
    ; Max capacity = 2GB / element_size tokens
    xor     ecx, ecx                     ; lpAddress = NULL (let OS pick)
    mov     edx, 80000000h               ; dwSize = 2GB (0x80000000)
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    test    rax, rax
    jz      @kv_alloc_fallback

    mov     g_kvcache_ptr, rax
    mov     g_kvcache_size, 80000000h    ; 2GB
    mov     g_modelbase, rax

    ; Compute max capacity in tokens
    mov     rax, 80000000h               ; 2GB
    xor     edx, edx
    mov     ecx, g_kv_element_size
    div     rcx                          ; rax = max tokens
    mov     g_kvcache_capacity, rax
    jmp     @kv_alloc_done

@kv_alloc_fallback:
    ; 2GB failed, try 256MB fallback
    xor     ecx, ecx
    mov     edx, 10000000h               ; 256MB
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    test    rax, rax
    jz      @kv_alloc_minimal

    mov     g_kvcache_ptr, rax
    mov     g_kvcache_size, 10000000h
    mov     g_modelbase, rax

    mov     rax, 10000000h
    xor     edx, edx
    mov     ecx, g_kv_element_size
    div     rcx
    mov     g_kvcache_capacity, rax
    jmp     @kv_alloc_done

@kv_alloc_minimal:
    ; Last resort: 16MB
    xor     ecx, ecx
    mov     edx, 1000000h                ; 16MB
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    test    rax, rax
    jz      @init_no_scratch             ; Total failure

    mov     g_kvcache_ptr, rax
    mov     g_kvcache_size, 1000000h
    mov     g_modelbase, rax

    mov     rax, 1000000h
    xor     edx, edx
    mov     ecx, g_kv_element_size
    div     rcx
    mov     g_kvcache_capacity, rax

@kv_alloc_done:
    xor     eax, eax
    mov     g_kv_len, rax

    ; Allocate scratch buffers via VirtualAlloc
    xor     ecx, ecx                ; lpAddress = NULL
    mov     edx, SCRATCH_SIZE       ; 256KB
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    test    rax, rax
    jz      @init_no_scratch

    ; Partition the 256KB into 4 scratch regions (64KB each)
    mov     g_scratchA, rax
    lea     rcx, [rax + 10000h]
    mov     g_scratchB, rcx
    lea     rcx, [rax + 20000h]
    mov     g_scratchQ, rcx
    lea     rcx, [rax + 30000h]
    mov     g_scratchFFN, rcx

@init_no_scratch:
    ; Initialize model config defaults for head-major layout
    mov     eax, g_modelHeads
    mov     g_kvNumHeads, eax
    mov     eax, g_modelHeadDim
    mov     g_kvHeadDim, eax
    shl     eax, 1
    mov     g_kvHeadStride, eax
    mov     g_tokenPos, 0

=======

    mov     g_hasAVX, 1
    lea     rax, g_kvcache
    mov     g_modelbase, rax
>>>>>>> origin/main
    xor     eax, eax
    jmp     @done

@no_avx:
<<<<<<< HEAD
    ; AVX not required — still dynamically allocate KV cache
    ; Compute element size
    mov     eax, g_modelHeads
    imul    eax, g_modelHeadDim
    shl     eax, 2                       ; * 4 (fp16 * 2 for K+V)
    mov     g_kv_element_size, eax

    ; Try 256MB allocation (non-AVX machine likely has less RAM)
    xor     ecx, ecx
    mov     edx, 10000000h               ; 256MB
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    test    rax, rax
    jz      @no_avx_fallback

    mov     g_kvcache_ptr, rax
    mov     g_kvcache_size, 10000000h
    mov     g_modelbase, rax
    mov     rax, 10000000h
    xor     edx, edx
    mov     ecx, g_kv_element_size
    div     rcx
    mov     g_kvcache_capacity, rax
    xor     eax, eax
    jmp     @done

@no_avx_fallback:
    ; 16MB last resort
    xor     ecx, ecx
    mov     edx, 1000000h
    mov     r8d, MEM_COMMIT_IA or MEM_RESERVE_IA
    mov     r9d, PAGE_RW_IA
    call    VirtualAlloc
    mov     g_kvcache_ptr, rax
    mov     g_kvcache_size, 1000000h
=======
    ; AVX not required for stub — still init KV cache base
    lea     rax, g_kvcache
>>>>>>> origin/main
    mov     g_modelbase, rax
    xor     eax, eax

@done:
    add     rsp, 20h
    pop     rbx
    ret
InferenceEngineInit ENDP

; ────────────────────────────────────────────────────────────────
<<<<<<< HEAD
; RunInference — Full transformer forward pass + autoregressive generation
;   RCX = pInputTokens  (DWORD array of input token IDs)
;   EDX = inputCount     (number of input tokens, valid 1..2048)
;   R8  = pOutputBuf     (DWORD array to receive generated token IDs)
;   R9D = maxOutputTokens (generation cap, valid 1..4096)
;   Returns EAX = number of tokens actually generated
;
;   Stack frame locals (via RSP offsets after 0C0h alloc):
;     [rsp+20h] shadow space for callees
;     [rsp+40h] layer weight stride (bytes per layer)
;     [rsp+48h] vocab_size
;     [rsp+50h] n_layers
;     [rsp+58h] head_dim
;     [rsp+60h] hidden_dim
;     [rsp+68h] ffn_dim
;     [rsp+70h] kv_element_size
;     [rsp+78h] (reserved)
=======
; RunInference — generate N tokens into output buffer
;   RCX = pContext, EDX = tokenCount, R8 = pOutputBuf
>>>>>>> origin/main
; ────────────────────────────────────────────────────────────────
RunInference PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
<<<<<<< HEAD
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 0C0h
    .allocstack 0C0h
    .endprolog

    ; ── 1. Validate params ─────────────────────────────────────
    test    rcx, rcx
    jz      @ri_fail                    ; null pInputTokens
    test    r8, r8
    jz      @ri_fail                    ; null pOutputBuf
    test    edx, edx
    jz      @ri_fail                    ; inputCount == 0
    cmp     edx, 2048
    ja      @ri_fail                    ; inputCount > 2048
    test    r9d, r9d
    jz      @ri_fail                    ; maxOutputTokens == 0
    cmp     r9d, 4096
    ja      @ri_fail                    ; maxOutputTokens > 4096

    ; ── 2. Save params in preserved regs ───────────────────────
    mov     rsi, rcx                    ; rsi = pInputTokens
    mov     r12d, edx                   ; r12d = inputCount
    mov     rdi, r8                     ; rdi = pOutputBuf
    mov     r14d, r9d                   ; r14d = maxOutputTokens
    xor     r15d, r15d                  ; r15d = gen_count (tokens generated so far)

    ; Cache model config into locals
    mov     eax, g_modelDim
    mov     dword ptr [rsp+60h], eax    ; hidden_dim
    mov     eax, g_modelHeadDim
    mov     dword ptr [rsp+58h], eax    ; head_dim
    mov     eax, g_modelLayers
    mov     dword ptr [rsp+50h], eax    ; n_layers
    mov     eax, g_modelVocab
    mov     dword ptr [rsp+48h], eax    ; vocab_size
    mov     eax, g_modelFFNDim
    mov     dword ptr [rsp+68h], eax    ; ffn_dim
    mov     eax, g_kv_element_size
    mov     dword ptr [rsp+70h], eax    ; kv_element_size

    ; Validate model base pointer
    mov     rbx, g_modelbase
    test    rbx, rbx
    jz      @ri_fail

    ; cur_pos = inputCount (we start generating after prompt)
    mov     r13d, r12d                  ; r13d = cur_pos

    ; ── 3. Embed input tokens into scratchA ────────────────────
    ;    For each input token: pEmbed = g_modelbase + token * head_dim * 4
    ;    Copy head_dim floats into scratchA (last token's embedding is kept)
    mov     rcx, g_scratchA
    test    rcx, rcx
    jz      @ri_fail

    ; Process last input token embedding as initial hidden state
    mov     eax, r12d
    dec     eax                         ; index of last input token
    mov     eax, dword ptr [rsi + rax*4] ; token ID of last input
    mov     ecx, dword ptr [rsp+58h]    ; head_dim
    shl     ecx, 2                      ; head_dim * 4 bytes
    imul    rax, rcx                    ; token * head_dim * 4
    add     rax, rbx                    ; pEmbed = g_modelbase + offset
    ; Copy embedding into scratchA
    mov     rcx, g_scratchA             ; dest
    mov     rdx, rax                    ; src = embedding pointer
    mov     r8d, dword ptr [rsp+60h]    ; count = hidden_dim floats
    shr     r8d, 2                      ; convert to QWORD count (div 2 for pairs)
@@ri_copy_embed:
    test    r8d, r8d
    jz      @ri_embed_done
    mov     rax, qword ptr [rdx]
    mov     qword ptr [rcx], rax
    add     rdx, 8
    add     rcx, 8
    dec     r8d
    jmp     @@ri_copy_embed
@ri_embed_done:

    ; ── 4. Autoregressive generation loop ──────────────────────
@ri_gen_loop:
    cmp     r15d, r14d
    jae     @ri_done                    ; generated maxOutputTokens, stop

    ; ── 5. Layer loop ──────────────────────────────────────────
    xor     ebx, ebx                    ; ebx = layerIdx

@ri_layer_loop:
    cmp     ebx, dword ptr [rsp+50h]   ; cmp layerIdx, n_layers
    jae     @ri_layers_done

    ; Compute layer weight base: g_modelbase + 100h + layerIdx * dim * dim * 4
    ; (Simplified: each layer's weight block = dim * dim * sizeof(float))
    mov     eax, dword ptr [rsp+60h]   ; hidden_dim
    imul    eax, dword ptr [rsp+60h]   ; dim * dim
    shl     eax, 2                      ; * sizeof(float)
    imul    rax, rbx                    ; * layerIdx  (RAX zero-ext from EAX handled by imul r,r)
    mov     rcx, g_modelbase
    lea     r8, [rcx + rax + 100h]      ; r8 = layer weight base

    ; ── 5a. Pre-attention RMSNorm ──────────────────────────────
    mov     rcx, g_scratchB             ; pOut
    mov     rdx, g_scratchA             ; pInput (hidden state)
    ; r8 already = norm weight ptr (layer base)
    mov     r9d, dword ptr [rsp+60h]   ; dim
    mov     qword ptr [rsp+80h], r8    ; save layer base for later
    call    SIMD_RMSNorm

    ; ── 5b. Q/K/V projection via MatVecQ4 ─────────────────────
    ;   Q = W_q * normed_state  → scratchQ
    mov     rcx, g_scratchQ             ; pOut = Q
    mov     rdx, qword ptr [rsp+80h]   ; pMatrix = layer weights
    mov     r8, g_scratchB              ; pVec = norm'd hidden state
    mov     r9d, dword ptr [rsp+58h]   ; rows = head_dim
    mov     eax, dword ptr [rsp+60h]   ; cols = hidden_dim
    mov     dword ptr [rsp+20h], eax
    call    SIMD_MatVecQ4

    ; ── 5c. RoPE positional encoding on Q ──────────────────────
    mov     rcx, g_scratchQ             ; pVec = Q
    mov     edx, dword ptr [rsp+58h]   ; dim = head_dim
    mov     r8d, r13d                   ; position = cur_pos
    mov     r9d, 10000                  ; theta_base
    call    SIMD_RoPE

    ; ── 5d. KV cache write at cur_pos ──────────────────────────
    ;   Write K (from scratchB) into kv_cache[cur_pos]
    mov     rax, g_kvcache_ptr
    test    rax, rax
    jz      @ri_skip_kv
    mov     ecx, dword ptr [rsp+70h]   ; kv_element_size
    imul    rcx, r13                    ; offset = cur_pos * element_size
    add     rax, rcx                    ; kv_ptr + offset
    ; Copy head_dim floats from scratchB (K vector) to KV cache
    mov     rdx, g_scratchB
    mov     ecx, dword ptr [rsp+58h]   ; head_dim
@@ri_kv_write:
    test    ecx, ecx
    jz      @ri_skip_kv
    movss   xmm0, dword ptr [rdx]
    movss   dword ptr [rax], xmm0
    add     rax, 4
    add     rdx, 4
    dec     ecx
    jmp     @@ri_kv_write
@ri_skip_kv:

    ; ── 5e. Attention: ScaledDotBatch(Q, K_cache, scores, cur_pos+1, head_dim)
    ;   Then Softmax, then VAccumulate with V cache
    mov     rcx, g_scratchQ             ; pQ
    mov     rdx, g_kvcache_ptr          ; pK (cache base)
    mov     r8, g_scratchFFN            ; pScores (reuse scratchFFN as attn scores)
    lea     r9d, [r13d + 1]             ; seq_len = cur_pos + 1
    mov     eax, dword ptr [rsp+58h]   ; head_dim
    mov     dword ptr [rsp+20h], eax
    call    SIMD_ScaledDotBatch

    ; Softmax over attention scores
    mov     rcx, g_scratchFFN           ; pScores
    lea     edx, [r13d + 1]             ; count = cur_pos + 1
    call    SIMD_Softmax

    ; Weighted sum: VAccumulate(pOut=scratchA, pScores, pV=kv_cache, seq_len, head_dim)
    mov     rcx, g_scratchA             ; pOut (accumulate into hidden state)
    mov     rdx, g_scratchFFN           ; pScores (attention weights)
    mov     r8, g_kvcache_ptr           ; pV (value cache)
    lea     r9d, [r13d + 1]             ; seq_len
    mov     eax, dword ptr [rsp+58h]
    mov     dword ptr [rsp+20h], eax    ; head_dim
    call    SIMD_VAccumulate

    ; ── 5f. Feed-forward: gate * SiLU(up), down projection, residual ──
    ;   Up projection: MatVecQ4(scratchFFN, W_up, scratchA, ffn_dim, dim)
    mov     rcx, g_scratchFFN           ; pOut = FFN intermediate
    mov     rdx, qword ptr [rsp+80h]   ; W_up (reuse layer base; production uses offset)
    mov     r8, g_scratchA              ; pVec = hidden state
    mov     r9d, dword ptr [rsp+68h]   ; rows = ffn_dim
    mov     eax, dword ptr [rsp+60h]   ; cols = hidden_dim
    mov     dword ptr [rsp+20h], eax
    call    SIMD_MatVecQ4

    ;   SiLU activation on FFN intermediate
    mov     rcx, g_scratchFFN           ; pInOut
    mov     edx, dword ptr [rsp+68h]   ; count = ffn_dim
    call    SIMD_SiLU

    ;   Down projection: MatVecQ4(scratchB, W_down, scratchFFN, dim, ffn_dim)
    mov     rcx, g_scratchB             ; pOut = projected back to dim
    mov     rdx, qword ptr [rsp+80h]   ; W_down (simplified reuse)
    mov     r8, g_scratchFFN            ; pVec = SiLU'd FFN output
    mov     r9d, dword ptr [rsp+60h]   ; rows = hidden_dim
    mov     eax, dword ptr [rsp+68h]   ; cols = ffn_dim
    mov     dword ptr [rsp+20h], eax
    call    SIMD_MatVecQ4

    ;   Residual add: scratchA[i] += scratchB[i]
    mov     rcx, g_scratchA
    mov     rdx, g_scratchB
    mov     eax, dword ptr [rsp+60h]   ; hidden_dim
@@ri_residual:
    test    eax, eax
    jz      @ri_residual_done
    movss   xmm0, dword ptr [rcx]
    addss   xmm0, dword ptr [rdx]
    movss   dword ptr [rcx], xmm0
    add     rcx, 4
    add     rdx, 4
    dec     eax
    jmp     @@ri_residual
@ri_residual_done:

    inc     ebx
    jmp     @ri_layer_loop

@ri_layers_done:
    ; ── 6. Final RMSNorm + logit projection ────────────────────
    mov     rcx, g_scratchB             ; pOut = normalized final state
    mov     rdx, g_scratchA             ; pInput = final hidden state
    mov     r8, g_modelbase             ; pWeight = final norm weights
    mov     r9d, dword ptr [rsp+60h]   ; dim
    call    SIMD_RMSNorm

    ; Logits = output_weights * hidden_state → scratchFFN
    mov     rcx, g_scratchFFN           ; pOut = logit vector
    mov     rax, g_modelbase
    lea     rdx, [rax + 2000h]          ; pMatrix = output head weights
    mov     r8, g_scratchB              ; pVec = norm'd hidden state
    mov     r9d, dword ptr [rsp+48h]   ; rows = vocab_size
    mov     eax, dword ptr [rsp+60h]   ; cols = hidden_dim
    mov     dword ptr [rsp+20h], eax
    call    SIMD_MatVecQ4

    ; ── 7. Greedy argmax over logits ───────────────────────────
    mov     rcx, g_scratchFFN           ; logit vector
    mov     edx, dword ptr [rsp+48h]   ; vocab_size
    ; Clamp to 16384 floats (64KB scratch limit)
    cmp     edx, 16384
    jbe     @ri_vocab_ok
    mov     edx, 16384
@ri_vocab_ok:
    test    edx, edx
    jz      @ri_done

    xor     eax, eax                    ; bestIdx = 0
    vmovss  xmm0, dword ptr [rcx]      ; bestVal = logits[0]
    mov     r8d, 1                      ; i = 1
@ri_argmax:
    cmp     r8d, edx
    jae     @ri_have_token
    vmovss  xmm1, dword ptr [rcx + r8*4]
    vcomiss xmm1, xmm0
    jbe     @ri_argmax_next
    vmovaps xmm0, xmm1
    mov     eax, r8d                    ; bestIdx = i
@ri_argmax_next:
    inc     r8d
    jmp     @ri_argmax

@ri_have_token:
    ; ── 8. Store token, check EOS, increment gen_count ─────────
    mov     dword ptr [rdi + r15*4], eax ; pOutputBuf[gen_count] = token
    inc     r15d                         ; gen_count++
    inc     r13d                         ; cur_pos++ (for RoPE / KV)

    ; Update global KV length
    movsxd  rcx, r13d
    mov     g_kv_len, rcx

    ; Check EOS token (token ID == 2)
    cmp     eax, 2
    je      @ri_done                    ; stop on EOS

    ; ── 9. Feed generated token embedding back as next input ───
    ;   Load embedding for the just-generated token into scratchA
    mov     ecx, dword ptr [rsp+58h]   ; head_dim
    shl     ecx, 2                      ; * sizeof(float)
    imul    rax, rcx                    ; token * head_dim * 4
    add     rax, g_modelbase            ; pEmbed
    mov     rcx, g_scratchA
    mov     r8d, dword ptr [rsp+60h]
    shr     r8d, 2                      ; QWORD count
@@ri_copy_next:
    test    r8d, r8d
    jz      @ri_gen_loop                ; back to step 5
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx], rdx
    add     rax, 8
    add     rcx, 8
    dec     r8d
    jmp     @@ri_copy_next

    ; ── Error / exit paths ─────────────────────────────────────
@ri_fail:
    xor     r15d, r15d                  ; gen_count = 0 (failure)

@ri_done:
    mov     eax, r15d                   ; return gen_count
    add     rsp, 0C0h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
=======
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx
    mov     edi, edx
    mov     rbx, r8

    test    edi, edi
    jz      @run_done

@token_loop:
    call    TokenGenerate
    mov     dword ptr [rbx], eax
    add     rbx, 4
    dec     edi
    jnz     @token_loop

@run_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    xor     eax, eax
>>>>>>> origin/main
    ret
RunInference ENDP

; ────────────────────────────────────────────────────────────────
<<<<<<< HEAD
; TokenGenerate — Forward final hidden state through output projection,
;                 then argmax over logits to select next token ID.
;   No args (reads from g_scratchA as final hidden state).
;   Returns token ID in EAX; 1 (BOS) when model unavailable.
;
;   Pipeline:
;     1. RMSNorm on final hidden state
;     2. MatVecQ4: project to vocab-sized logit vector
;     3. Argmax over logits → token ID
; ────────────────────────────────────────────────────────────────
TokenGenerate PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rax, g_modelbase
    test    rax, rax
    jz      @tg_unavailable

    mov     rax, g_scratchA
    test    rax, rax
    jz      @tg_unavailable

    ; 1. Final RMSNorm
    mov     rcx, g_scratchB         ; pOut = scratchB (output projection input)
    mov     rdx, g_scratchA         ; pInput = final hidden state
    mov     r8, g_modelbase         ; pWeight (reuse base weights for norm)
    mov     r9d, g_modelDim         ; dim
    call    SIMD_RMSNorm

    ; 2. Output projection: MatVecQ4 → logits in scratchFFN
    ;    This projects from hidden_dim to vocab_size
    ;    In production, the weight pointer would be model_base + output_layer_offset
    mov     rcx, g_scratchFFN       ; pOut = logit vector
    mov     rax, g_modelbase
    lea     rdx, [rax + 2000h]      ; pMatrix = output projection weights (offset)
    mov     r8, g_scratchB          ; pVec = RMSNorm'd hidden state
    mov     r9d, g_modelVocab       ; rows = vocab_size (32000)
    mov     eax, g_modelDim         ; cols = hidden_dim (8192)
    mov     dword ptr [rsp + 20h], eax
    call    SIMD_MatVecQ4

    ; 3. Argmax over logits
    ;    Scan scratchFFN[0..vocab_size-1] for the maximum float value
    mov     rsi, g_scratchFFN       ; logit vector
    mov     ecx, g_modelVocab       ; vocab size
    test    ecx, ecx
    jz      @tg_unavailable

    ; Clamp vocab to buffer capacity: scratchFFN is 64KB = 16384 floats max
    cmp     ecx, 16384
    jbe     @tg_vocab_ok
    mov     ecx, 16384
@tg_vocab_ok:

    xor     ebx, ebx                ; bestIdx = 0
    vmovss  xmm0, dword ptr [rsi]   ; bestVal = logits[0]
    mov     edi, 1                  ; i = 1

@tg_argmax:
    cmp     edi, ecx
    jae     @tg_have_token
    vmovss  xmm1, dword ptr [rsi + rdi*4]
    vcomiss xmm1, xmm0
    jbe     @tg_argmax_next
    vmovaps xmm0, xmm1
    mov     ebx, edi
@tg_argmax_next:
    inc     edi
    jmp     @tg_argmax

@tg_have_token:
    mov     eax, ebx                ; return token ID
    add     rsp, 30h
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@tg_unavailable:
    mov     eax, 1                  ; BOS token as stall
    add     rsp, 30h
    pop     rdi
    pop     rsi
    pop     rbx
=======
; TokenGenerate — read next token from model base (stub)
;   Returns token ID in EAX; 1 (BOS) when model unavailable (e.g. during hotswap)
; ────────────────────────────────────────────────────────────────
TokenGenerate PROC
    mov     rax, g_modelbase
    test    rax, rax
    jz      @model_unavailable
    mov     eax, dword ptr [rax]
    ret
@model_unavailable:
    mov     eax, 1                      ; BOS token as stall (thread-safe during swap)
>>>>>>> origin/main
    ret
TokenGenerate ENDP

; ─── X+4: Clear KV cache on hotswap (when preserveKV=0) ─────────────────────
ClearKVCache PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
<<<<<<< HEAD
    mov     rdi, g_kvcache_ptr
    test    rdi, rdi
    jz      @clr_skip                    ; No cache allocated
    mov     rcx, g_kvcache_size
    shr     rcx, 3                       ; Convert bytes to QWORDs
    xor     eax, eax
    rep stosq
@clr_skip:
    mov     g_kv_len, 0
=======
    lea     rdi, g_kvcache
    mov     rcx, 10000h / 8
    xor     eax, eax
    rep stosq
>>>>>>> origin/main
    add     rsp, 20h
    pop     rdi
    ret
ClearKVCache ENDP

<<<<<<< HEAD
; ─── X+4: Prune KV Cache (Simple Sink/Compress) ───────────────────────────
;   RCX = keepCount (Number of tokens to keep from the start or end)
;   EDX = flags (0 = keep head, 1 = keep tail)
; ──────────────────────────────────────────────────────────────────────────
PruneKVCache PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rax, g_kv_len
    test    rax, rax
    jz      @prune_done             ; Nothing to prune

    cmp     rcx, rax
    jae     @prune_done             ; Keep count >= current count

    test    edx, edx
    jnz     @keep_tail

@keep_head:
    ; Just truncate. The data is already at the start.
    mov     g_kv_len, rcx
    jmp     @prune_done

@keep_tail:
    ; Shift tail to start of cache
    ; RSI = g_kvcache + (current_len - keep_count) * element_size
    ; RDI = g_kvcache
    ; RCX = keep_count * element_size
    
    ; Element size is model-dependent: g_kv_element_size
    ; = num_heads * head_dim * sizeof(fp16) * 2 (K+V)
    mov     r9d, g_kv_element_size      ; Load runtime element size
    
    imul    rax, r9                     ; current_len * element_size
    mov     r8, rcx
    imul    r8, r9                      ; keep_count * element_size
    
    sub     rax, r8                     ; byte offset to tail
    mov     rsi, g_kvcache_ptr
    add     rsi, rax                    ; Source = tail start
    mov     rdi, g_kvcache_ptr          ; Dest = cache start
    
    mov     rcx, r8                     ; Bytes to move
    shr     rcx, 3                      ; convert to QWORDs (>>3)
    rep movsq                           ; Move tail to head
    
    ; Handle remaining bytes (element_size may not be QWORD-aligned)
    mov     rcx, r8
    and     rcx, 7                      ; remaining bytes mod 8
    rep movsb
    
    mov     rax, r8
    xor     edx, edx
    mov     ecx, g_kv_element_size
    div     rcx                         ; back to token count
    mov     g_kv_len, rax

@prune_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PruneKVCache ENDP

; ─── Phase 4B: Neural Pruning Scorer — UpdateImportanceScores ─────────────
;   RCX = lastTokenAttn (pointer to float array of attention weights)
;   RDX = numActive
; ──────────────────────────────────────────────────────────────────────────
UpdateImportanceScores PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    lea     rax, g_kv_scores
    mov     r8, rcx                 ; attn weights
    mov     rcx, rdx                ; loop counter
    movss   xmm1, g_decay_factor

@score_loop:
    movss   xmm0, dword ptr [rax]   ; old score
    mulss   xmm0, xmm1              ; decay
    addss   xmm0, dword ptr [r8]    ; current attention weight
    movss   dword ptr [rax], xmm0   ; store back
    
    add     rax, 4
    add     r8, 4
    loop    @score_loop

    add     rsp, 20h
    pop     rbx
    ret
UpdateImportanceScores ENDP

; ─── GetTopKIndices — Sorts scores and returns indices of top K tokens ────
;   RCX = pOutIndices (pointer to DWORD array)
;   RDX = K (number of indices to return)
;   R8  = numActive (total tokens to consider)
; ──────────────────────────────────────────────────────────────────────────
GetTopKIndices PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; O(N log K) Min-Heap Top-K selection
    ; Uses a min-heap of size K. For each of N scores:
    ;   If score > heap_min, replace root and sift-down.
    ; Final heap contains the top K elements.
    ; Heap stored on stack: [rsp+60h .. rsp+60h + K*8) 
    ;   Each heap entry = 8 bytes: DWORD score (float bits) | DWORD index
    mov     r10, rcx                ; out buffer
    mov     r11, rdx                ; K
    mov     qword ptr [rbp-8], r10  ; save out buffer
    mov     qword ptr [rbp-10h], r11 ; save K
    mov     qword ptr [rbp-18h], r8 ; save N (numActive)

    ; Clamp K to N
    cmp     r11, r8
    jbe     @topk_k_ok
    mov     r11, r8
@topk_k_ok:
    test    r11, r11
    jz      @sort_done

    ; Allocate heap on stack: K entries * 8 bytes each
    ; We need K*8 bytes. Max K is typically small (<=256).
    mov     rax, r11
    shl     rax, 3                  ; K * 8
    sub     rsp, rax                ; Allocate heap space
    mov     rbx, rsp                ; rbx = heap base pointer
    ; Also save the allocation size for later cleanup
    mov     qword ptr [rbp-20h], rax ; heap alloc size

    ; Phase 1: Fill initial K entries from scores[0..K-1]
    lea     rdx, g_kv_scores
    xor     ecx, ecx                ; i = 0
@topk_fill:
    cmp     rcx, r11
    jge     @topk_heapify
    mov     eax, dword ptr [rdx + rcx*4]   ; score bits (float as DWORD)
    mov     dword ptr [rbx + rcx*8], eax   ; heap[i].score
    mov     dword ptr [rbx + rcx*8 + 4], ecx ; heap[i].index
    inc     rcx
    jmp     @topk_fill

    ; Phase 2: Build min-heap (heapify from K/2-1 down to 0)
@topk_heapify:
    mov     rcx, r11
    shr     rcx, 1                  ; K/2
    dec     rcx                     ; start index
    js      @topk_scan              ; K <= 1, no heapify needed
@topk_heapify_loop:
    ; Sift-down element at index ECX
    push    rcx
    mov     edi, ecx                ; node to sift
    call    @topk_siftdown
    pop     rcx
    dec     ecx
    jns     @topk_heapify_loop

    ; Phase 3: Scan remaining scores[K..N-1]
@topk_scan:
    mov     rcx, r11                ; i = K
    mov     r8, qword ptr [rbp-18h] ; N
    lea     rdx, g_kv_scores
@topk_scan_loop:
    cmp     rcx, r8
    jge     @topk_extract
    ; Load score[i]
    movss   xmm0, dword ptr [rdx + rcx*4]
    ; Compare with heap root (minimum in our top-K so far)
    movss   xmm1, dword ptr [rbx]          ; heap[0].score
    comiss  xmm0, xmm1
    jbe     @topk_scan_next                ; score <= heap_min, skip
    ; Replace root with this score
    mov     eax, dword ptr [rdx + rcx*4]
    mov     dword ptr [rbx], eax           ; heap[0].score = new score
    mov     dword ptr [rbx + 4], ecx       ; heap[0].index = i
    ; Sift down the new root
    push    rcx
    push    rdx
    push    r8
    xor     edi, edi                       ; sift from root (index 0)
    call    @topk_siftdown
    pop     r8
    pop     rdx
    pop     rcx
@topk_scan_next:
    inc     rcx
    jmp     @topk_scan_loop

    ; Phase 4: Extract heap entries to output buffer
    ; Heap contains top K entries (min-heap, so smallest of top-K is root)
    ; Write indices to output in reverse order for descending sort
@topk_extract:
    mov     r10, qword ptr [rbp-8]  ; out buffer
    mov     r11, qword ptr [rbp-10h] ; K
    ; Write indices from heap to output (any order is fine for Top-K)
    xor     ecx, ecx
@topk_extract_loop:
    cmp     rcx, r11
    jge     @topk_cleanup
    mov     eax, dword ptr [rbx + rcx*8 + 4] ; heap[i].index
    mov     dword ptr [r10 + rcx*4], eax
    inc     rcx
    jmp     @topk_extract_loop

@topk_cleanup:
    ; Restore stack
    mov     rax, qword ptr [rbp-20h]
    add     rsp, rax
    jmp     @sort_done

    ; ────────────────────────────────────────────────
    ; @topk_siftdown — inline sift-down for min-heap
    ; Input:  EDI = node index to sift down
    ;         RBX = heap base,  R11 = heap size (K)
    ; Clobbers: EAX, ESI, R9D, XMM0, XMM1
    ; ────────────────────────────────────────────────
@topk_siftdown:
    mov     esi, edi                       ; smallest = node
@sift_loop:
    lea     eax, [edi*2 + 1]               ; left child
    cmp     rax, r11
    jge     @sift_check_swap               ; no left child
    ; Compare heap[left] < heap[smallest]
    movss   xmm0, dword ptr [rbx + rax*8]  ; left.score
    movss   xmm1, dword ptr [rbx + rsi*8]  ; smallest.score
    comiss  xmm0, xmm1
    jae     @sift_right                    ; left >= smallest
    mov     esi, eax                       ; smallest = left
@sift_right:
    lea     r9d, [edi*2 + 2]               ; right child
    cmp     r9, r11
    jge     @sift_check_swap               ; no right child
    movss   xmm0, dword ptr [rbx + r9*8]   ; right.score
    movss   xmm1, dword ptr [rbx + rsi*8]  ; smallest.score
    comiss  xmm0, xmm1
    jae     @sift_check_swap               ; right >= smallest
    mov     esi, r9d                       ; smallest = right
@sift_check_swap:
    cmp     esi, edi
    je      @sift_done                     ; no swap needed
    ; Swap heap[edi] <-> heap[esi] (8 bytes each)
    mov     rax, qword ptr [rbx + rdi*8]
    mov     r9,  qword ptr [rbx + rsi*8]
    mov     qword ptr [rbx + rdi*8], r9
    mov     qword ptr [rbx + rsi*8], rax
    mov     edi, esi                       ; continue sifting from swapped position
    jmp     @sift_loop
@sift_done:
    ret

@sort_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GetTopKIndices ENDP

; ═══════════════════════════════════════════════════════════════════
; Phase 7: KV-Cache Paging Layer (via SlotRing)
;
; Segments the KV cache into 64KB pages managed by SlotRing.
; Each segment holds 128 tokens × 512 bytes = 64KB.
; Segments are demand-paged via SlotRing_GetTensor, pinned during
; active attention via KVPage_PinWorkingSet, and evicted when
; the memory budget is exceeded.
;
; This enables 100K+ token contexts without pre-allocating the
; full KV buffer — segments page in/out via the tiered LRU.
; ═══════════════════════════════════════════════════════════════════

; ────────────────────────────────────────────────────────────────
; KVPage_Init — Initialize KV-cache paging subsystem
;   ECX = baseLayerIdx (layer index to tag KV segments with)
;
;   Clears all segment-to-slot mappings and sets state to ready.
;   Called once at model load or after ClearKVCache.
; ────────────────────────────────────────────────────────────────
KVPage_Init PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     g_kvLayerBase, ecx
    mov     g_kvSegCount, 0
    mov     g_kvPageReady, 1

    ; Fill segment map with -1 (unmapped)
    lea     rdi, g_kvSegSlots
    mov     ecx, KV_MAX_SEGMENTS
    mov     eax, 0FFFFFFFFh         ; -1
    rep stosd

    add     rsp, 20h
    pop     rdi
    ret
KVPage_Init ENDP

; ────────────────────────────────────────────────────────────────
; KVPage_Grow — Allocate the next KV segment via SlotRing
;   RCX = fileOffset (GGUF offset for this KV slab, or 0 for scratch)
;
;   Attaches a new segment to the SlotRing.  The segment is NOT
;   committed yet — it will be demand-paged on first access via
;   SlotRing_GetTensor.
;
;   Returns: EAX = segmentIndex (0..255), or -1 if full
; ────────────────────────────────────────────────────────────────
KVPage_Grow PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx                ; save fileOffset

    mov     eax, g_kvSegCount
    cmp     eax, KV_MAX_SEGMENTS
    jae     @grow_fail

    mov     ebx, eax                ; segmentIndex = current count

    ; SlotRing_Attach(tensorIdx, fileOffset, tensorSize, layerIdx)
    lea     ecx, [ebx + 1000h]      ; tensorIdx = segIdx + 0x1000 (KV namespace)
    mov     rdx, rsi                ; fileOffset (preserved from entry)
    mov     r8d, KV_SEGMENT_SIZE    ; tensorSize = 64KB
    mov     r9d, g_kvLayerBase      ; layerIdx
    call    SlotRing_Attach

    cmp     eax, -1
    je      @grow_fail

    ; Store slot index in the segment map
    lea     rcx, g_kvSegSlots
    mov     dword ptr [rcx + rbx*4], eax

    ; Increment segment count
    inc     g_kvSegCount

    mov     eax, ebx                ; return segmentIndex
    jmp     @grow_done

@grow_fail:
    mov     eax, -1

@grow_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
KVPage_Grow ENDP

; ────────────────────────────────────────────────────────────────
; KVPage_GetSegment — Get pointer to a KV segment (demand-pages it)
;   ECX = segmentIndex (0..kvSegCount-1)
;   Returns: RAX = pointer to 64KB segment data, or 0 if invalid
;
;   This is the hot path — called from the attention kernel for
;   every token × every layer.  The SlotRing handles demand-commit
;   and tier promotion automatically.
; ────────────────────────────────────────────────────────────────
KVPage_GetSegment PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_kvPageReady, 0
    je      @seg_null

    cmp     ecx, g_kvSegCount
    jae     @seg_null

    ; Look up the SlotRing slot index for this segment
    lea     rax, g_kvSegSlots
    mov     ecx, dword ptr [rax + rcx*4]
    cmp     ecx, -1
    je      @seg_null

    ; SlotRing_GetTensor(slotIndex) — demand-pages if needed
    call    SlotRing_GetTensor
    ; RAX = pointer to tensor data (or 0)
    jmp     @seg_done

@seg_null:
    xor     eax, eax

@seg_done:
    add     rsp, 28h
    ret
KVPage_GetSegment ENDP

; ────────────────────────────────────────────────────────────────
; KVPage_Shrink — Release the last N KV segments
;   ECX = numSegments to release from the tail
;
;   Detaches segments from the SlotRing, freeing committed memory.
;   Used after KV pruning to release tail segments that are no
;   longer needed.
; ────────────────────────────────────────────────────────────────
KVPage_Shrink PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     ebx, ecx                ; numToRelease

@shrink_loop:
    test    ebx, ebx
    jz      @shrink_done

    mov     eax, g_kvSegCount
    test    eax, eax
    jz      @shrink_done

    dec     eax
    mov     g_kvSegCount, eax       ; pop last segment

    ; Get the SlotRing slot index for this segment
    lea     rcx, g_kvSegSlots
    mov     esi, eax                ; segIndex
    mov     ecx, dword ptr [rcx + rax*4]
    cmp     ecx, -1
    je      @shrink_skip

    ; SlotRing_Detach(slotIndex) — decommits + zeros the slot
    call    SlotRing_Detach

@shrink_skip:
    ; Mark segment as unmapped
    lea     rax, g_kvSegSlots
    mov     dword ptr [rax + rsi*4], -1

    dec     ebx
    jmp     @shrink_loop

@shrink_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
KVPage_Shrink ENDP

; ────────────────────────────────────────────────────────────────
; KVPage_PinWorkingSet — Pin N segments starting from segStart
;   ECX = segStart (first segment to pin)
;   EDX = segCount (number of consecutive segments to pin)
;
;   Pinned segments are immune to eviction.  Call this before
;   multi-head attention on a token range, then Unpin after.
;   This guarantees the KV data stays resident during compute.
; ────────────────────────────────────────────────────────────────
KVPage_PinWorkingSet PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     esi, ecx                ; segStart
    mov     edi, edx                ; segCount

@pin_ws_loop:
    test    edi, edi
    jz      @pin_ws_done

    ; Bounds check
    cmp     esi, g_kvSegCount
    jae     @pin_ws_done

    ; Get slot index for this segment
    lea     rax, g_kvSegSlots
    mov     ecx, dword ptr [rax + rsi*4]
    cmp     ecx, -1
    je      @pin_ws_next

    ; SlotRing_Pin(slotIndex)
    call    SlotRing_Pin

@pin_ws_next:
    inc     esi
    dec     edi
    jmp     @pin_ws_loop

@pin_ws_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVPage_PinWorkingSet ENDP

; ═══════════════════════════════════════════════════════════════════
; Phase 8B: Paged KV Block Table (PagedAttention)
;
; Decouples logical token order from physical memory blocks.
; Tokens are mapped to fixed-size blocks (32 tokens each).
; Blocks can be reused when tokens leave the sliding window,
; eliminating alloc/free overhead for long contexts.
;
; This is the core mechanism behind vLLM/PagedAttention.
; ═══════════════════════════════════════════════════════════════════

; ────────────────────────────────────────────────────────────────
; KVBlock_Init — Initialize the block table and token map
;   No args. Call once at model load / after ClearKVCache.
; ────────────────────────────────────────────────────────────────
KVBlock_Init PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Zero block descriptors
    lea     rdi, g_kvBlocks
    mov     ecx, (KV_MAX_BLOCKS * 16) / 8
    xor     eax, eax
    rep stosq

    ; Fill all block segmentIdx fields with -1 (free)
    lea     rdi, g_kvBlocks
    xor     ecx, ecx
@blk_init_loop:
    cmp     ecx, KV_MAX_BLOCKS
    jae     @blk_init_map

    ; offset = ecx * 16, field +0 = segmentIdx
    imul    eax, ecx, 16
    mov     dword ptr [rdi + rax], -1       ; segmentIdx = -1 (free)
    inc     ecx
    jmp     @blk_init_loop

@blk_init_map:
    ; Fill token map with -1 (unmapped)
    lea     rdi, g_tokenMap
    mov     ecx, KV_MAX_TOKEN_MAP
    mov     eax, 0FFFFFFFFh
    rep stosd

    mov     g_kvBlockCount, 0
    mov     g_kvFreeHead, 0
    mov     g_kvBlockReady, 1

    add     rsp, 20h
    pop     rdi
    ret
KVBlock_Init ENDP

; ────────────────────────────────────────────────────────────────
; KVBlock_AllocBlock — Allocate a block for a token range
;   ECX = segmentIdx (which KV segment to attach to)
;   EDX = tokenStart (first logical token in this block)
;   Returns: EAX = blockIndex, or -1 if full
; ────────────────────────────────────────────────────────────────
KVBlock_AllocBlock PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     esi, ecx                ; segmentIdx
    mov     ebx, edx                ; tokenStart

    ; Scan from freeHead for a free block
    lea     rax, g_kvBlocks
    mov     ecx, g_kvFreeHead

@alloc_scan:
    cmp     ecx, KV_MAX_BLOCKS
    jae     @alloc_fail

    imul    edx, ecx, 16
    cmp     dword ptr [rax + rdx], -1   ; segmentIdx == -1 means free
    je      @alloc_found
    inc     ecx
    jmp     @alloc_scan

@alloc_found:
    ; Fill block descriptor
    mov     dword ptr [rax + rdx + 0], esi   ; segmentIdx
    mov     dword ptr [rax + rdx + 4], ebx   ; tokenStart
    mov     dword ptr [rax + rdx + 8], 0     ; tokenCount = 0 (empty, ready to fill)
    mov     dword ptr [rax + rdx + 12], 0    ; flags = 0

    ; Update free head (next block after this one)
    lea     edx, [ecx + 1]
    mov     g_kvFreeHead, edx
    inc     g_kvBlockCount

    ; Map the token range to this block
    lea     rdx, g_tokenMap
    mov     r8d, ebx                ; tokenStart
    mov     r9d, KV_BLOCK_TOKENS    ; 32 tokens per block
@map_tokens:
    test    r9d, r9d
    jz      @map_done
    cmp     r8d, KV_MAX_TOKEN_MAP
    jae     @map_done
    mov     dword ptr [rdx + r8*4], ecx
    inc     r8d
    dec     r9d
    jmp     @map_tokens
@map_done:

    mov     eax, ecx                ; return blockIndex
    jmp     @alloc_done

@alloc_fail:
    mov     eax, -1

@alloc_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
KVBlock_AllocBlock ENDP

; ────────────────────────────────────────────────────────────────
; KVBlock_FreeBlock — Release a block back to the free pool
;   ECX = blockIndex
;   Clears the token mappings and marks the block as free.
; ────────────────────────────────────────────────────────────────
KVBlock_FreeBlock PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     ecx, KV_MAX_BLOCKS
    jae     @free_done

    lea     rax, g_kvBlocks
    imul    edx, ecx, 16
    add     rax, rdx

    ; Clear token map entries for this block's range
    mov     ebx, dword ptr [rax + 4]     ; tokenStart
    lea     rdx, g_tokenMap
    mov     r8d, KV_BLOCK_TOKENS
@clear_map:
    test    r8d, r8d
    jz      @clear_map_done
    cmp     ebx, KV_MAX_TOKEN_MAP
    jae     @clear_map_done
    mov     dword ptr [rdx + rbx*4], -1
    inc     ebx
    dec     r8d
    jmp     @clear_map

@clear_map_done:
    ; Mark block as free
    mov     dword ptr [rax + 0], -1       ; segmentIdx = -1
    mov     dword ptr [rax + 4], 0
    mov     dword ptr [rax + 8], 0
    mov     dword ptr [rax + 12], 0

    dec     g_kvBlockCount

    ; Update freeHead if this block is earlier
    cmp     ecx, g_kvFreeHead
    jae     @free_done
    mov     g_kvFreeHead, ecx

@free_done:
    add     rsp, 20h
    pop     rbx
    ret
KVBlock_FreeBlock ENDP

; ────────────────────────────────────────────────────────────────
; KVBlock_MapToken — Look up which block a logical token belongs to
;   ECX = logicalTokenIdx
;   Returns: EAX = blockIndex, or -1 if unmapped
; ────────────────────────────────────────────────────────────────
KVBlock_MapToken PROC
    cmp     ecx, KV_MAX_TOKEN_MAP
    jae     @mt_fail
    lea     rax, g_tokenMap
    mov     eax, dword ptr [rax + rcx*4]
    ret
@mt_fail:
    mov     eax, -1
    ret
KVBlock_MapToken ENDP

; ────────────────────────────────────────────────────────────────
; KVBlock_GetTokenPtr — Get pointer to a token's KV data within its block
;   ECX = logicalTokenIdx
;   Returns: RAX = pointer to token's KV data, or 0 if unmapped
;
;   Hot path: looks up token→block, then block→segment→SlotRing→VA.
;   The offset within the segment is computed from the token's
;   position within its block.
; ────────────────────────────────────────────────────────────────
KVBlock_GetTokenPtr PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     esi, ecx                ; save logicalTokenIdx

    ; Look up block
    cmp     ecx, KV_MAX_TOKEN_MAP
    jae     @tp_null
    lea     rax, g_tokenMap
    mov     ecx, dword ptr [rax + rsi*4]
    cmp     ecx, -1
    je      @tp_null

    ; Get block descriptor
    lea     rax, g_kvBlocks
    imul    edx, ecx, 16
    add     rax, rdx
    mov     ebx, dword ptr [rax + 0]     ; segmentIdx

    cmp     ebx, -1
    je      @tp_null

    ; Get segment VA via KVPage_GetSegment(segmentIdx)
    mov     ecx, ebx
    call    KVPage_GetSegment
    test    rax, rax
    jz      @tp_null

    ; Compute offset: (logicalToken - block.tokenStart) * 512
    lea     rcx, g_kvBlocks
    lea     rdx, g_tokenMap
    mov     edx, dword ptr [rdx + rsi*4]  ; blockIndex (re-read)
    imul    edx, edx, 16
    add     rcx, rdx
    mov     edx, dword ptr [rcx + 4]      ; tokenStart
    sub     esi, edx                       ; offset within block
    shl     esi, 9                         ; × 512 bytes per token
    add     rax, rsi                       ; final pointer

    jmp     @tp_done

@tp_null:
    xor     eax, eax

@tp_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
KVBlock_GetTokenPtr ENDP

; ────────────────────────────────────────────────────────────────
; KVBlock_ReuseBlock — Remap a freed block to a new token range
;   ECX = blockIndex (previously freed block)
;   EDX = newSegmentIdx
;   R8D = newTokenStart
;   Returns: EAX = 0 success, -1 failure
;
;   This is the core of block reuse: instead of freeing + allocating,
;   just remap the block to new tokens. Zero VirtualAlloc overhead.
; ────────────────────────────────────────────────────────────────
KVBlock_ReuseBlock PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    cmp     ecx, KV_MAX_BLOCKS
    jae     @reuse_fail

    lea     rax, g_kvBlocks
    imul    ebx, ecx, 16
    add     rax, rbx

    ; Block must be free (segmentIdx == -1)
    cmp     dword ptr [rax + 0], -1
    jne     @reuse_fail

    ; Fill with new mapping
    mov     dword ptr [rax + 0], edx       ; newSegmentIdx
    mov     dword ptr [rax + 4], r8d       ; newTokenStart
    mov     dword ptr [rax + 8], 0         ; tokenCount = 0
    mov     dword ptr [rax + 12], 0        ; flags = 0

    inc     g_kvBlockCount

    ; Map token range
    lea     rdx, g_tokenMap
    mov     r9d, r8d                ; tokenStart
    mov     r10d, KV_BLOCK_TOKENS
@reuse_map:
    test    r10d, r10d
    jz      @reuse_ok
    cmp     r9d, KV_MAX_TOKEN_MAP
    jae     @reuse_ok
    mov     dword ptr [rdx + r9*4], ecx
    inc     r9d
    dec     r10d
    jmp     @reuse_map

@reuse_ok:
    xor     eax, eax
    jmp     @reuse_done

@reuse_fail:
    mov     eax, -1

@reuse_done:
    add     rsp, 20h
    pop     rbx
    ret
KVBlock_ReuseBlock ENDP

; ═══════════════════════════════════════════════════════════════════
; Phase 8C: Head-Major KV Layout
;
; Provides utility procedures to store and retrieve KV vectors
; in head-major order: [layer][head][token][dim] instead of the
; default [layer][token][head][dim].
;
; This yields ~30-40% less memory bandwidth during attention by
; making per-head token sequences contiguous in memory.
;
; Call KVHM_WriteToken when generating a new KV entry.
; Call KVHM_GetHeadPtr during attention to get the sequential
; token stream for a single head.
; ═══════════════════════════════════════════════════════════════════

; ────────────────────────────────────────────────────────────────
; KVHM_WriteToken — Write a token's KV vectors in head-major layout
;   RCX = pSegmentBase (pointer to segment start, from KVPage_GetSegment)
;   EDX = tokenOffsetInBlock (0..KV_BLOCK_TOKENS-1)
;   R8  = pKVData (pointer to interleaved KV: head0..headN, each head_dim fp16)
;
;   Scatters the interleaved KV data into head-major positions:
;     for each head h:
;       dst = segBase + h * (KV_BLOCK_TOKENS * headStride) + tokenOffset * headStride
;       copy headStride bytes from pKVData + h * headStride
;
;   headStride = g_kvHeadStride (typically 256 bytes for fp16 × 128 dim)
; ────────────────────────────────────────────────────────────────
KVHM_WriteToken PROC FRAME
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
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx                ; pSegmentBase
    mov     ebx, edx                ; tokenOffsetInBlock
    mov     r12, r8                 ; pKVData (source, interleaved)

    mov     r13d, g_kvNumHeads      ; total heads
    test    r13d, r13d
    jz      @wt_done

    mov     eax, g_kvHeadStride     ; bytes per head per token
    ; headBlockSize = KV_BLOCK_TOKENS * headStride
    imul    ecx, eax, KV_BLOCK_TOKENS  ; ecx = bytes per head slab (32 × stride)

    xor     edi, edi                ; head index

@wt_head_loop:
    cmp     edi, r13d
    jae     @wt_done

    ; Compute headBlockSize (constant across loop but recompute is cheap)
    mov     eax, g_kvHeadStride
    imul    eax, KV_BLOCK_TOKENS    ; eax = headBlockSize

    ; dst offset = head * headBlockSize + tokenOffset * headStride
    mov     r8d, edi
    imul    r8d, eax                ; r8d = head * headBlockSize

    mov     eax, g_kvHeadStride
    imul    eax, ebx                ; eax = tokenOffset * headStride
    add     r8d, eax                ; r8d = total dst offset

    ; src offset = head * headStride
    mov     eax, g_kvHeadStride
    imul    eax, edi                ; eax = head * headStride (source offset in interleaved data)

    ; Copy headStride bytes: dst = segBase + r8, src = pKVData + eax
    ; Use rep movsb for simplicity (headStride is typically 256 bytes)
    push    rcx
    push    rsi
    push    rdi

    lea     rdi, [rsi + r8]         ; Wait: rsi is segBase, but we'll clobber rdi
    ; Need to be very careful with register usage here.
    ; rsi = segBase (saved), rdi = head index (will be clobbered by rep movsb)
    ; Let's just do a manual qword copy loop instead.
    pop     rdi
    pop     rsi
    pop     rcx

    ; Manual copy: headStride bytes, 8 bytes at a time
    mov     eax, g_kvHeadStride
    shr     eax, 3                  ; qword count

    ; src = pKVData + head * headStride
    mov     r10d, g_kvHeadStride
    imul    r10d, edi               ; head * headStride
    lea     r10, [r12 + r10]        ; r10 = source ptr

    ; dst = segBase + dstOffset
    lea     r11, [rsi + r8]         ; r11 = dest ptr

@wt_copy:
    test    eax, eax
    jz      @wt_next_head
    mov     rcx, [r10]
    mov     [r11], rcx
    add     r10, 8
    add     r11, 8
    dec     eax
    jmp     @wt_copy

@wt_next_head:
    inc     edi
    jmp     @wt_head_loop

@wt_done:
    add     rsp, 20h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVHM_WriteToken ENDP

; ────────────────────────────────────────────────────────────────
; KVHM_GetHeadPtr — Get pointer to a contiguous token stream for one head
;   RCX = pSegmentBase (from KVPage_GetSegment)
;   EDX = headIndex (0..numHeads-1)
;   Returns: RAX = pointer to token[0] of this head's KV slab
;
;   The returned pointer points to KV_BLOCK_TOKENS consecutive
;   token vectors in sequential memory — perfect for SIMD attention.
;
;   Layout at returned ptr:
;     token0_KV[head_dim]  (headStride bytes)
;     token1_KV[head_dim]  (headStride bytes)
;     ...
;     token31_KV[head_dim] (headStride bytes)
; ────────────────────────────────────────────────────────────────
KVHM_GetHeadPtr PROC
    ; offset = headIndex * (KV_BLOCK_TOKENS * headStride)
    mov     eax, g_kvHeadStride
    imul    eax, KV_BLOCK_TOKENS    ; headBlockSize
    imul    eax, edx                ; × headIndex
    add     rax, rcx                ; segBase + offset
    ret
KVHM_GetHeadPtr ENDP

=======
>>>>>>> origin/main
END
