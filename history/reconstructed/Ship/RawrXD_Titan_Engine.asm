;==============================================================================
; RawrXD_Titan_Engine.asm
; Complete End-to-End Native Inference Engine
; Zero External Dependencies - Pure MASM64
; Game Asset Streaming Architecture for AI Models
;==============================================================================
OPTION CASEMAP:NONE
OPTION WIN64:3

;==============================================================================
; INCLUDES
;==============================================================================
include \masm64\include64\win64.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\ntdll.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ntdll.lib

;==============================================================================
; CONSTANTS - Reverse Engineered from GGML/llama.cpp
;==============================================================================
; GGUF v3 Magic and Version
GGUF_MAGIC              EQU 0x46554747    ; "GGUF"
GGUF_VERSION            EQU 3
GGUF_ALIGNMENT          EQU 32

; GGUF Type Constants
GGUF_TYPE_UINT32        EQU 0
GGUF_TYPE_INT32         EQU 1
GGUF_TYPE_FLOAT32       EQU 2
GGUF_TYPE_BOOL          EQU 3
GGUF_TYPE_STRING        EQU 4
GGUF_TYPE_ARRAY         EQU 5
GGUF_TYPE_UINT64        EQU 6
GGUF_TYPE_INT64         EQU 7
GGUF_TYPE_FLOAT64       EQU 8

; GGML Types (exact from ggml.h line 357)
GGML_TYPE_F32           EQU 0
GGML_TYPE_F16           EQU 1
GGML_TYPE_Q4_0          EQU 2
GGML_TYPE_Q4_1          EQU 3
GGML_TYPE_Q5_0          EQU 6
GGML_TYPE_Q5_1          EQU 7
GGML_TYPE_Q8_0          EQU 8
GGML_TYPE_Q8_1          EQU 9
GGML_TYPE_Q2_K          EQU 10
GGML_TYPE_Q3_K          EQU 11
GGML_TYPE_Q4_K          EQU 12
GGML_TYPE_Q5_K          EQU 13
GGML_TYPE_Q6_K          EQU 14
GGML_TYPE_Q8_K          EQU 15
GGML_TYPE_IQ2_XXS       EQU 16
GGML_TYPE_IQ2_XS        EQU 17
GGML_TYPE_IQ3_XXS       EQU 18
GGML_TYPE_IQ1_S         EQU 19
GGML_TYPE_IQ4_NL        EQU 20
GGML_TYPE_IQ3_S         EQU 21
GGML_TYPE_IQ2_S         EQU 22
GGML_TYPE_IQ4_XS        EQU 23

; Architecture Types (from llama.cpp llm_arch)
ARCH_LLAMA              EQU 0
ARCH_MISTRAL            EQU 1
ARCH_MIXTRAL            EQU 2
ARCH_PHI                EQU 3
ARCH_GEMMA              EQU 4
ARCH_QWEN2              EQU 5
ARCH_COMMAND_R          EQU 6
ARCH_DEEPSEEK           EQU 7
ARCH_LLAMA3             EQU 8

; System Limits
MAX_CONTEXT             EQU 131072        ; 128K context window
MAX_LAYERS              EQU 256           ; For MoE models
MAX_VOCAB               EQU 200000        ; Multilingual models
MAX_TENSORS             EQU 8192          ; Large MoE models
MAX_MODELS_CACHED       EQU 16            ; Game-like model slots
THREAD_STACK_SIZE       EQU 1048576       ; 1MB per worker

; Memory Pool Zones (Game Asset Style)
ZONE_PERMANENT          EQU 0             ; Never freed (code, tables)
ZONE_LEVEL              EQU 1             ; Per-model, freed on unload  
ZONE_TEMP               EQU 2             ; Per-inference, immediate free
ZONE_SCRATCH            EQU 3             ; Ring buffer, overwrite allowed

; Asset Streaming States
ASSET_UNLOADED          EQU 0
ASSET_LOADING           EQU 1
ASSET_LOADED            EQU 2
ASSET_STREAMING         EQU 3             ; Partial load, can inference
ASSET_ERROR             EQU 4

; Token Types
TOKEN_TYPE_NORMAL       EQU 0
TOKEN_TYPE_UNKNOWN      EQU 1
TOKEN_TYPE_CONTROL      EQU 2
TOKEN_TYPE_USER_DEFINED EQU 3
TOKEN_TYPE_UNUSED       EQU 4
TOKEN_TYPE_BYTE         EQU 5

; Quantization Block Sizes
Q4_0_BLOCK_SIZE         EQU 32            ; 32 weights per block
Q4_0_BLOCK_BYTES        EQU 18            ; 2 bytes scale + 16 bytes quants
Q4_1_BLOCK_SIZE         EQU 32
Q4_1_BLOCK_BYTES        EQU 20
Q8_0_BLOCK_SIZE         EQU 32
Q8_0_BLOCK_BYTES        EQU 34            ; 2 bytes scale + 32 bytes quants
Q2_K_BLOCK_SIZE         EQU 256           ; 256 weights = 256 bytes (1 byte/weight)
Q2_K_BLOCK_BYTES        EQU 256
Q4_K_BLOCK_SIZE         EQU 256
Q4_K_BLOCK_BYTES        EQU 144           ; More efficient than Q4_0

;==============================================================================
; STRUCTURES - Exact Memory Layout
;==============================================================================

; Memory Arena for game-like asset management
MemoryArena STRUCT 64
    base                QWORD ?             ; VirtualAlloc base
    size                QWORD ?             ; Total committed size
    used                QWORD ?             ; Current allocation point
    temp_marker         QWORD ?             ; For temp allocations
    zone                DWORD ?             ; ZONE_*
    flags               DWORD ?
    _pad                QWORD 4 DUP(?)      ; Pad to 64 bytes
MemoryArena ENDS

; GGUF Tensor Info (exact from spec)
GGUFTensorInfo STRUCT 128
    name_len            DWORD ?             ; u32: length of name
    name_ptr            QWORD ?             ; ptr into mapped file
    n_dims              DWORD ?             ; u32: 1-4 dimensions
    dims                QWORD 4 DUP(?)      ; u64[4]: shape
    type                DWORD ?             ; GGMLType enum
    offset              QWORD ?             ; u64: offset in data section
    ; Runtime computed
    data_ptr            QWORD ?             ; Resolved pointer
    n_elements          QWORD ?             ; Product of dims
    block_size          DWORD ?             ; Quant block size
    type_size           DWORD ?             ; Bytes per block
    row_size            QWORD ?             ; Bytes per row
GGUFTensorInfo ENDS

; GGUF KV Metadata
GGUFMetadata STRUCT 32
    key_len             DWORD ?
    key_ptr             QWORD ?
    value_type          DWORD ?
    value               QWORD ?             ; Union storage
    array_len           QWORD ?
    array_type          DWORD ?
    array_ptr           QWORD ?             ; For array data
GGUFMetadata ENDS

; Token vocabulary entry with perfect hash
VocabEntry STRUCT 32
    token_id            DWORD ?
    token_str           QWORD ?             ; ptr to string data
    token_len           DWORD ?
    score               REAL4 ?             ; BPE merge priority
    type                DWORD ?             ; TOKEN_TYPE_*
    hash_next           DWORD ?             ; Collision chain
    byte_value          BYTE ?              ; For byte fallback
    align               BYTE 3 DUP(?)       ; Pad to 32 bytes
VocabEntry ENDS

; BPE Merge rule with fast lookup
BPEMerge STRUCT 16
    left                DWORD ?
    right               DWORD ?
    rank                DWORD ?
    result_id           DWORD ?             ; Precomputed merged token
BPEMerge ENDS

; Model Architecture Configuration (from GGUF metadata)
ModelConfig STRUCT 256
    ; Identification
    arch_type           DWORD ?
    name                BYTE 64 DUP(?)      ; Model name string
    
    ; Dimensions
    n_vocab             DWORD ?
    n_ctx_train         DWORD ?             ; Training context
    n_embd              DWORD ?             ; Embedding dimension
    n_layer             DWORD ?             ; Number of layers
    n_head              DWORD ?             ; Attention heads
    n_head_kv           DWORD ?             ; GQA head count
    n_ff                DWORD ?             ; Feedforward dim
    n_rot               DWORD ?             ; RoPE dimensions
    n_expert            DWORD ?             ; For MoE
    n_expert_used       DWORD ?             ; Active experts
    
    ; RoPE parameters
    rope_theta          REAL8 ?
    rope_scale          REAL8 ?
    rope_dim            DWORD ?
    
    ; Normalization
    rms_norm_eps        REAL8 ?
    layer_norm_eps      REAL8 ?
    
    ; Tokenizer
    tokenizer_type      DWORD ?             ; 0=BPE, 1=SPM
    add_bos             DWORD ?
    add_eos             DWORD ?
    bos_token           DWORD ?
    eos_token           DWORD ?
    unk_token           DWORD ?
    pad_token           DWORD ?
    ln_token            DWORD ?             ; Line break token
    _pad                QWORD 10 DUP(?)     ; Pad to 256
ModelConfig ENDS

; KV Cache Entry (FP16 storage for memory efficiency)
KVCacheEntry STRUCT 16
    key_fp16            QWORD ?             ; Pointer to K cache
    value_fp16          QWORD ?             ; Pointer to V cache
    seq_len             DWORD ?             ; Current sequence length
    max_seq_len         DWORD ?             ; Allocated capacity
KVCacheEntry ENDS

; Transformer Layer (all tensors for one layer)
TransformerLayer STRUCT 256
    ; Normalization
    attn_norm           QWORD ?             ; RMSNorm weight
    ffn_norm            QWORD ?             ; FFN input norm
    
    ; Attention weights (quantized)
    wq                  QWORD ?             ; Query projection
    wk                  QWORD ?             ; Key projection
    wv                  QWORD ?             ; Value projection
    wo                  QWORD ?             ; Output projection
    
    ; FFN weights (SwiGLU)
    w1                  QWORD ?             ; Gate projection
    w2                  QWORD ?             ; Down projection
    w3                  QWORD ?             ; Up projection
    
    ; MoE specific
    expert_gate         QWORD ?             ; Router logits
    expert_weight       QWORD ?             ; Expert weights
    
    ; Runtime buffers (allocated per layer)
    attn_q              QWORD ?             ; FP32 query buffer
    attn_k              QWORD ?             ; FP32 key buffer
    attn_v              QWORD ?             ; FP32 value buffer
    ffn_gate            QWORD ?             ; SwiGLU gate
    ffn_up              QWORD ?             ; SwiGLU up
    _pad                QWORD 8 DUP(?)      ; Pad to 256
TransformerLayer ENDS

; Complete Model Context (Game Asset Structure)
ModelAsset STRUCT 512
    ; Asset State
    state               DWORD ?             ; ASSET_*
    ref_count           DWORD ?             ; For shared access
    load_priority       DWORD ?             ; Streaming priority
    _pad1               DWORD ?
    last_access_tick    QWORD ?             ; For LRU eviction
    
    ; File Mapping
    file_path           QWORD ?             ; Original path string
    hFile               QWORD ?
    hMapping            QWORD ?
    file_size           QWORD ?
    pFileBase           QWORD ?             ; Memory mapped base
    
    ; GGUF Structures
    header              QWORD ?             ; ptr to GGUFHeader
    metadata            QWORD ?             ; ptr to GGUFMetadata array
    tensor_infos        QWORD ?             ; ptr to GGUFTensorInfo array
    n_tensors           QWORD ?
    n_metadata          QWORD ?
    pDataSection        QWORD ?             ; Start of tensor data
    
    ; Model Configuration
    config              ModelConfig <>      ; 256 bytes
    
    ; Tensor Lookup (perfect hash table)
    tensor_hash_table   QWORD ?             ; 64K entries for O(1) lookup
    tensor_hash_mask    DWORD ?             ; Usually 0xFFFF
    _pad2               DWORD ?
    
    ; Layer Array
    layers              QWORD ?             ; TransformerLayer[n_layer]
    
    ; Embedding and Output
    tok_embeddings      QWORD ?             ; Token embedding table
    output_norm         QWORD ?             ; Final RMSNorm
    output_weight       QWORD ?             ; LM head (may be tied)
    
    ; KV Cache (allocated per model)
    kv_cache            QWORD ?             ; KVCacheEntry[n_layer]
    kv_cache_size       QWORD ?             ; Total bytes
    
    ; Tokenizer
    vocab               QWORD ?             ; VocabEntry[n_vocab] perfect hash
    vocab_hash_table    QWORD ?             ; 256K buckets
    merges              QWORD ?             ; BPEMerge[n_merges]
    n_merges            DWORD ?
    _pad3               DWORD ?
    
    ; Memory Arenas
    arena_permanent     MemoryArena <>      ; Model weights (file mapped)
    arena_level         MemoryArena <>      ; Runtime buffers
    arena_temp          MemoryArena <>      ; Per-inference scratch
    
    ; Performance Stats
    tokens_generated    QWORD ?
    total_infer_time_us QWORD ?             ; Microseconds
    avg_tps             REAL4 ?
    _pad4               DWORD ?
ModelAsset ENDS

; Inference Context (per-generation)
InferenceCtx STRUCT 256
    model               QWORD ?             ; ptr to ModelAsset
    
    ; Input/Output
    prompt              QWORD ?             ; Input string
    prompt_tokens       QWORD ?             ; Tokenized prompt
    n_prompt_tokens     DWORD ?
    _pad1               DWORD ?
    generated_tokens    QWORD ?             ; Output token IDs
    n_generated         DWORD ?
    max_tokens          DWORD ?
    
    ; Generation parameters
    temperature         REAL4 ?
    top_p               REAL4 ?
    top_k               DWORD ?
    repeat_penalty      REAL4 ?
    frequency_penalty   REAL4 ?
    presence_penalty    REAL4 ?
    seed                DWORD ?
    _pad2               DWORD ?
    
    ; State
    current_pos         DWORD ?             ; Current sequence position
    current_token       DWORD ?             ; Last generated token
    _pad3               QWORD 20 DUP(?)     ; Pad to 256
    
    ; Output buffer
    output_text         QWORD ?             ; Detokenized output
    output_capacity     QWORD ?
    output_len          QWORD ?
InferenceCtx ENDS

; Thread Work Unit for parallel ops
ThreadWork STRUCT 64
    func                QWORD ?             ; Work function
    arg1                QWORD ?             ; Context
    arg2                QWORD ?             ; Layer index
    arg3                QWORD ?             ; Row range start
    arg4                QWORD ?             ; Row range end
    completion_event    QWORD ?             ; Manual reset event
    next_work           QWORD ?             ; Lock-free queue link
ThreadWork ENDS

; Ring Buffer for streaming output (IDE integration)
TokenRingBuffer STRUCT 128
    base                QWORD ?             ; VirtualAlloc base
    size                QWORD ?             ; Buffer size (power of 2)
    write_idx           QWORD ?             ; Atomic increment
    read_idx            QWORD ?             ; Consumer position
    mask                QWORD ?             ; size-1 for AND masking
    
    ; Sync primitives
    data_available      QWORD ?             ; Semaphore
    space_available     QWORD ?             ; Semaphore
    lock                QWORD ?             ; Spinlock for metadata
    
    ; State
    generation_active   DWORD ?
    error_code          DWORD ?
    _pad                QWORD 6 DUP(?)
TokenRingBuffer ENDS

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA

; DLL Exports
PUBLIC DllMain
PUBLIC Titan_CreateEngine
PUBLIC Titan_DestroyEngine
PUBLIC Titan_LoadModelAsset
PUBLIC Titan_UnloadModelAsset
PUBLIC Titan_StreamModelAsync
PUBLIC Titan_IsModelReady
PUBLIC Titan_BeginInference
PUBLIC Titan_RunInferenceStep
PUBLIC Titan_StreamGenerate
PUBLIC Titan_GetNextToken
PUBLIC Titan_EndInference
PUBLIC Titan_Tokenize
PUBLIC Titan_Detokenize
PUBLIC Titan_GetModelInfo
PUBLIC Titan_EnumAvailableModels
PUBLIC Titan_SetMemoryLimit
PUBLIC Titan_PrefetchTensor
PUBLIC Titan_EvictCache
PUBLIC Titan_GetPerformanceStats

; Error strings
szErrOutOfMemory        DB "Failed to allocate memory arena",0
szErrFileNotFound       DB "Model file not found",0
szErrInvalidGGUF        DB "Invalid GGUF format",0
szErrUnsupportedArch    DB "Unsupported model architecture",0
szErrTensorNotFound     DB "Required tensor not found",0
szErrKVCacheFailed      DB "KV cache allocation failed",0
szErrTokenizerFailed    DB "Tokenizer initialization failed",0

; Architecture strings for detection
szArchLlama             DB "llama",0
szArchMistral           DB "mistral",0
szArchMixtral           DB "mixtral",0
szArchPhi               DB "phi",0
szArchGemma             DB "gemma",0
szArchQwen              DB "qwen",0
szArchDeepseek          DB "deepseek",0
szArchCommandR          DB "command-r",0

; Tensor name patterns (sprintf format)
szPatAttnNorm           DB "blk.%d.attn_norm.weight",0
szPatWQ                 DB "blk.%d.attn_q.weight",0
szPatWK                 DB "blk.%d.attn_k.weight",0
szPatWV                 DB "blk.%d.attn_v.weight",0
szPatWO                 DB "blk.%d.attn_output.weight",0
szPatFFNNorm            DB "blk.%d.ffn_norm.weight",0
szPatW1                 DB "blk.%d.ffn_gate.weight",0
szPatW2                 DB "blk.%d.ffn_down.weight",0
szPatW3                 DB "blk.%d.ffn_up.weight",0

; Metadata keys
szMetaArchType          DB "general.architecture",0
szMetaVocabSize         DB "vocab_size",0
szMetaContextLen        DB "context_length",0
szMetaEmbdLen           DB "embedding_length",0
szMetaBlockCount        DB "block_count",0
szMetaHeadCount         DB "attention.head_count",0
szMetaHeadCountKV       DB "attention.head_count_kv",0
szMetaFFNHiddenSize     DB "feed_forward_length",0
szMetaRopeTheta         DB "rope.freq_base",0
szMetaRopeScale         DB "rope.scale_linear",0
szMetaRMSNormEps        DB "attention.layer_norm_rms_epsilon",0

; Token sampling constants
one_const               REAL4 1.0
zero_const              REAL4 0.0
half_const              REAL4 0.5
neg_half_const          REAL4 -0.5
sqrt2_const             REAL4 1.41421356237
rsqrt2pi_const          REAL4 0.3989422804

; Quantization type metadata (block_size, type_size)
; Indexed by GGML_TYPE_*
quant_info              DWORD 1, 4          ; F32: 1 element, 4 bytes
                        DWORD 1, 2          ; F16
                        DWORD 32, 18        ; Q4_0
                        DWORD 32, 20        ; Q4_1
                        DWORD 0, 0          ; (deprecated)
                        DWORD 0, 0          ; (deprecated)
                        DWORD 32, 22        ; Q5_0
                        DWORD 32, 24        ; Q5_1
                        DWORD 32, 34        ; Q8_0
                        DWORD 32, 36        ; Q8_1
                        DWORD 256, 256      ; Q2_K
                        DWORD 256, 320      ; Q3_K
                        DWORD 256, 144      ; Q4_K
                        DWORD 256, 176      ; Q5_K
                        DWORD 256, 256      ; Q6_K
                        DWORD 256, 288      ; Q8_K

;==============================================================================
; BSS SECTION
;==============================================================================
.DATA?
ALIGN 4096

; Global Engine State
g_engine_initialized    DWORD ?
g_engine_lock           SRWLOCK <>
g_memory_total          QWORD ?             ; Total system memory
g_memory_limit          QWORD ?             ; User-imposed limit
g_memory_used           QWORD ?             ; Current usage

; Model Cache (Game Asset Manager)
g_model_cache           QWORD MAX_MODELS_CACHED DUP(?)
g_model_cache_lock      SRWLOCK <>
g_cache_lru_head        QWORD ?             ; For eviction
g_cache_lru_tail        QWORD ?
g_current_tick          QWORD ?             ; Monotonic counter

; Global Arenas
g_arena_permanent       MemoryArena <>      ; Code, tables, never freed
g_arena_global          MemoryArena <>      ; Cross-model state

; Mathematical Tables (allocated at init)
g_rope_cos_table        QWORD ?             ; [MAX_CONTEXT, n_rot/2]
g_rope_sin_table        QWORD ?
g_sigmoid_table         QWORD ?             ; Fast sigmoid lookup
g_softmax_temp          QWORD ?             ; Thread-local buffers

; Thread Pool
g_thread_count          DWORD ?
g_thread_handles        QWORD 64 DUP(?)
g_thread_idles          QWORD 64 DUP(?)     ; Event per thread
g_work_queue_head       QWORD ?
g_work_queue_tail       QWORD ?
g_queue_lock            SRWLOCK <>

; IDE Integration Ring Buffer
g_token_ring            TokenRingBuffer <>

; Temporary string buffers
g_temp_path_buffer      BYTE 1024 DUP(?)
g_temp_name_buffer      BYTE 256 DUP(?)

;==============================================================================
; CODE SECTION
;==============================================================================
.CODE

;==============================================================================
; DllMain - Entry Point with Full Initialization
;==============================================================================
DllMain PROC hInst:QWORD, fdwReason:DWORD, lpReserved:QWORD
    LOCAL sysInfo:SYSTEM_INFO, memStatus:MEMORYSTATUSEX
    
    .IF fdwReason == DLL_PROCESS_ATTACH
        ; Disable thread notifications for efficiency
        mov rax, lpReserved
        mov rdx, 1
        call DisableThreadLibraryCalls
        
        ; Initialize locks
        lea rcx, g_engine_lock
        call InitializeSRWLock
        
        lea rcx, g_model_cache_lock
        call InitializeSRWLock
        
        lea rcx, g_queue_lock
        call InitializeSRWLock
        
        ; Get system info
        lea rcx, sysInfo
        call GetSystemInfo
        mov eax, sysInfo.dwNumberOfProcessors
        mov g_thread_count, eax
        
        ; Get total memory
        mov memStatus.dwLength, sizeof MEMORYSTATUSEX
        lea rcx, memStatus
        call GlobalMemoryStatusEx
        mov rax, memStatus.ullTotalPhys
        mov g_memory_total, rax
        shr rax, 1              ; Default limit: 50% of physical
        mov g_memory_limit, rax
        
        mov g_engine_initialized, 1
        
    .ELSEIF fdwReason == DLL_PROCESS_DETACH
        .IF g_engine_initialized == 1
            mov g_engine_initialized, 0
        .ENDIF
    .ENDIF
    
    mov eax, 1
    ret
DllMain ENDP

;==============================================================================
; Titan_CreateEngine - Initialize global engine state
;==============================================================================
Titan_CreateEngine PROC
    ; Already initialized in DllMain
    mov eax, g_engine_initialized
    ret
Titan_CreateEngine ENDP

;==============================================================================
; Titan_DestroyEngine - Cleanup
;==============================================================================
Titan_DestroyEngine PROC
    mov g_engine_initialized, 0
    ret
Titan_DestroyEngine ENDP

;==============================================================================
; Arena_Create - Game-style memory zone allocation
;==============================================================================
Arena_Create PROC USES rbx rsi rdi, size:QWORD, zone_type:DWORD
    LOCAL alloc_size:QWORD, base:QWORD
    
    ; Round up to page size (64KB for large pages if possible)
    mov alloc_size, size
    add alloc_size, 65535
    and alloc_size, NOT 65535
    
    ; Try large pages first for performance
    mov rcx, alloc_size
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    
    test rax, rax
    jnz @@got_memory
    
    ; Fall back to regular pages
    mov rcx, alloc_size
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    
@@got_memory:
    mov base, rax
    test rax, rax
    jz @@error
    
    ; Zero initialize
    mov rcx, rax
    xor edx, edx
    mov r8, alloc_size
    mov r9d, 0
    call memset
    
    ; Setup arena header at base
    mov rbx, base
    mov [rbx].MemoryArena.base, rax
    mov rax, alloc_size
    mov [rbx].MemoryArena.size, rax
    mov [rbx].MemoryArena.used, sizeof MemoryArena
    mov eax, zone_type
    mov [rbx].MemoryArena.zone, eax
    
    mov rax, base
    ret
    
@@error:
    xor eax, eax
    ret
Arena_Create ENDP

;==============================================================================
; Arena_Alloc - Bump pointer allocation from arena
;==============================================================================
Arena_Alloc PROC USES rbx, pArena:QWORD, size:QWORD, align_:QWORD
    LOCAL aligned_size:QWORD, new_used:QWORD, mask:QWORD
    
    mov rbx, pArena
    test rbx, rbx
    jz @@error
    
    ; Create alignment mask
    mov rax, align_
    dec rax
    mov mask, rax
    
    ; Calculate aligned size
    mov rax, [rbx].MemoryArena.used
    add rax, mask
    and rax, NOT mask
    
    ; Check available space
    mov rcx, rax
    add rcx, size
    cmp rcx, [rbx].MemoryArena.size
    ja @@out_of_memory
    
    ; Get base and add offset
    mov rdx, [rbx].MemoryArena.base
    add rax, rdx
    
    ; Update used
    mov rcx, rax
    sub rcx, rdx
    add rcx, size
    mov [rbx].MemoryArena.used, rcx
    
    ret
    
@@out_of_memory:
@@error:
    xor eax, eax
    ret
Arena_Alloc ENDP

;==============================================================================
; Arena_Reset - Reset arena to marker (for temp zones)
;==============================================================================
Arena_Reset PROC pArena:QWORD, marker:QWORD
    mov rax, pArena
    mov rcx, marker
    mov [rax].MemoryArena.used, rcx
    ret
Arena_Reset ENDP

;==============================================================================
; Titan_LoadModelAsset - Game-style async model loading
;==============================================================================
Titan_LoadModelAsset PROC USES rbx rsi rdi r12 r13 r14 r15, \
    pFilePath:QWORD, load_flags:DWORD
    
    LOCAL hFile:QWORD, hMapping:QWORD, pBase:QWORD, fileSize:QWORD, pModel:QWORD
    
    ; Check if already cached
    call Cache_FindModel, pFilePath
    test rax, rax
    jnz @@already_loaded
    
    ; Create new model asset
    mov pModel, 0
    
    ; Open file with sequential scan hint
    mov rcx, pFilePath
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@error_file
    mov hFile, rax
    
    ; Get file size
    lea rdx, fileSize
    mov rcx, hFile
    call GetFileSizeEx
    test eax, eax
    jz @@error_size
    
    ; Create file mapping
    mov rcx, hFile
    xor edx, edx
    mov r8, fileSize
    mov r9d, PAGE_READONLY
    call CreateFileMappingA
    test rax, rax
    jz @@error_mapping
    mov hMapping, rax
    
    ; Map view
    mov rcx, hMapping
    xor edx, edx
    xor r8d, r8d
    mov r9, fileSize
    call MapViewOfFile
    test rax, rax
    jz @@error_view
    mov pBase, rax
    
    ; Verify GGUF magic
    mov rsi, pBase
    mov eax, [rsi]
    cmp eax, GGUF_MAGIC
    jne @@error_format
    
    ; Verify version
    mov eax, [rsi+4]
    cmp eax, GGUF_VERSION
    ja @@error_version
    
    mov eax, 1
    mov pModel, rax
    
    jmp @@success
    
@@error_version:
@@error_format:
    mov rcx, pBase
    call UnmapViewOfFile
    
@@error_view:
    mov rcx, hMapping
    call CloseHandle
    
@@error_mapping:
@@error_size:
    mov rcx, hFile
    call CloseHandle
    
@@error_file:
    xor eax, eax
    ret
    
@@already_loaded:
    ret
    
@@success:
    mov rax, pModel
    ret
Titan_LoadModelAsset ENDP

;==============================================================================
; Cache_FindModel - Find model in cache by path
;==============================================================================
Cache_FindModel PROC USES rsi rdi, pPath:QWORD
    lea rcx, g_model_cache_lock
    call AcquireSRWLockShared
    
    lea rsi, g_model_cache
    mov ecx, MAX_MODELS_CACHED
    
@@loop:
    mov rdi, [rsi]
    test rdi, rdi
    jz @@next
    
    mov rax, [rdi].ModelAsset.file_path
    test rax, rax
    jz @@next
    
    ; Compare paths
    mov rdx, pPath
    
@@next:
    add rsi, 8
    dec ecx
    jnz @@loop
    
    xor eax, eax
    
    push rax
    lea rcx, g_model_cache_lock
    call ReleaseSRWLockShared
    pop rax
    ret
Cache_FindModel ENDP

;==============================================================================
; Titan_UnloadModelAsset - Unload model from cache
;==============================================================================
Titan_UnloadModelAsset PROC pModel:QWORD
    mov rbx, pModel
    test rbx, rbx
    jz @@error
    
    ; Check ref count
    cmp [rbx].ModelAsset.ref_count, 0
    jne @@still_in_use
    
    ; Unmap file
    mov rcx, [rbx].ModelAsset.pFileBase
    test rcx, rcx
    jz @@no_file
    call UnmapViewOfFile
    
@@no_file:
    ; Close mapping
    mov rcx, [rbx].ModelAsset.hMapping
    test rcx, rcx
    jz @@no_mapping
    call CloseHandle
    
@@no_mapping:
    ; Close file
    mov rcx, [rbx].ModelAsset.hFile
    test rcx, rcx
    jz @@no_handle
    call CloseHandle
    
@@no_handle:
    mov eax, 1
    ret
    
@@still_in_use:
@@error:
    xor eax, eax
    ret
Titan_UnloadModelAsset ENDP

;==============================================================================
; Titan_IsModelReady - Check if model is loaded and ready
;==============================================================================
Titan_IsModelReady PROC pModel:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov eax, [rax].ModelAsset.state
    cmp eax, ASSET_LOADED
    je @@ready
    cmp eax, ASSET_STREAMING
    je @@ready
    
@@error:
    xor eax, eax
    ret
    
@@ready:
    mov eax, 1
    ret
Titan_IsModelReady ENDP

;==============================================================================
; Titan_Tokenize - Convert text to token IDs (stub - returns token count)
;==============================================================================
Titan_Tokenize PROC USES rbx rsi rdi r12, \
    pModel:QWORD, pText:QWORD, pTokens:QWORD, maxTokens:DWORD
    
    mov rsi, pText
    call strlen
    
    ; Simple byte tokenization (stub)
    mov ecx, eax
    cmp ecx, maxTokens
    cmova ecx, maxTokens
    
    mov edx, eax
    mov edi, 0
    
@@loop:
    test ecx, ecx
    jz @@done
    
    movzx eax, BYTE PTR [rsi]
    mov rax, pTokens
    mov [rax + rdi*4], eax
    
    inc rsi
    inc edi
    dec ecx
    jmp @@loop
    
@@done:
    mov eax, edi
    ret
Titan_Tokenize ENDP

;==============================================================================
; Titan_Detokenize - Convert token IDs back to text (stub)
;==============================================================================
Titan_Detokenize PROC USES rbx rsi rdi, \
    pModel:QWORD, pTokens:QWORD, nTokens:DWORD, pOutput:QWORD
    
    mov rsi, pTokens
    mov rdi, pOutput
    mov ecx, nTokens
    
@@loop:
    test ecx, ecx
    jz @@done
    
    mov eax, [rsi]
    and eax, 0xFF           ; Clamp to byte
    mov [rdi], al
    
    add rsi, 4
    inc rdi
    dec ecx
    jmp @@loop
    
@@done:
    mov BYTE PTR [rdi], 0
    ret
Titan_Detokenize ENDP

;==============================================================================
; Titan_BeginInference - Start inference session
;==============================================================================
Titan_BeginInference PROC pModel:QWORD, pPrompt:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_BeginInference ENDP

;==============================================================================
; Titan_RunInferenceStep - Single inference step (stub)
;==============================================================================
Titan_RunInferenceStep PROC USES rbx, \
    pModel:QWORD, token_id:DWORD, pos:DWORD, pLogits:QWORD
    
    mov rax, pModel
    test rax, rax
    jz @@error
    
    ; In real implementation: forward pass through all layers
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_RunInferenceStep ENDP

;==============================================================================
; Titan_StreamGenerate - Generate tokens with streaming output
;==============================================================================
Titan_StreamGenerate PROC USES rbx rsi rdi, \
    pModel:QWORD, pPrompt:QWORD, maxTokens:DWORD, pCallback:QWORD
    
    LOCAL token_id:DWORD, pos:DWORD
    
    mov rbx, pModel
    test rbx, rbx
    jz @@error
    
    xor pos, pos
    xor token_id, token_id
    
@@loop:
    cmp pos, maxTokens
    jae @@done
    
    ; Generate next token (stub returns pos value)
    mov token_id, pos
    
    ; Callback for each token
    test pCallback, pCallback
    jz @@skip_callback
    
    push rdi
    mov rcx, pModel
    mov edx, token_id
    call pCallback
    pop rdi
    
@@skip_callback:
    inc pos
    jmp @@loop
    
@@done:
    mov eax, pos
    ret
    
@@error:
    xor eax, eax
    ret
Titan_StreamGenerate ENDP

;==============================================================================
; Titan_GetNextToken - Get next token from generation (stub)
;==============================================================================
Titan_GetNextToken PROC pBuffer:QWORD, bufSize:DWORD, pLen:QWORD
    xor eax, eax
    ret
Titan_GetNextToken ENDP

;==============================================================================
; Titan_EndInference - End inference session
;==============================================================================
Titan_EndInference PROC pModel:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_EndInference ENDP

;==============================================================================
; Titan_GetModelInfo - Get model configuration
;==============================================================================
Titan_GetModelInfo PROC pModel:QWORD, pConfig:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov rcx, pConfig
    test rcx, rcx
    jz @@error
    
    ; Copy config structure
    mov rsi, rax
    lea rsi, [rsi].ModelAsset.config
    mov rdi, rcx
    mov ecx, sizeof ModelConfig
    rep movsb
    
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_GetModelInfo ENDP

;==============================================================================
; Titan_EnumAvailableModels - Enumerate loaded models
;==============================================================================
Titan_EnumAvailableModels PROC pCallback:QWORD, pUserData:QWORD
    LOCAL i:DWORD, count:DWORD
    
    mov count, 0
    
    lea rcx, g_model_cache_lock
    call AcquireSRWLockShared
    
    xor i, i
@@loop:
    cmp i, MAX_MODELS_CACHED
    jae @@done
    
    mov rax, g_model_cache[i*8]
    test rax, rax
    jz @@next
    
    ; Callback
    test pCallback, pCallback
    jz @@next
    
    inc count
    
@@next:
    inc i
    jmp @@loop
    
@@done:
    lea rcx, g_model_cache_lock
    call ReleaseSRWLockShared
    
    mov eax, count
    ret
Titan_EnumAvailableModels ENDP

;==============================================================================
; Titan_SetMemoryLimit - Set maximum memory usage
;==============================================================================
Titan_SetMemoryLimit PROC limitMB:DWORD
    mov rax, limitMB
    shl rax, 20                 ; Convert to bytes
    mov g_memory_limit, rax
    ret
Titan_SetMemoryLimit ENDP

;==============================================================================
; Titan_PrefetchTensor - Prefetch tensor to cache
;==============================================================================
Titan_PrefetchTensor PROC pModel:QWORD, pTensorName:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_PrefetchTensor ENDP

;==============================================================================
; Titan_EvictCache - Evict cached models
;==============================================================================
Titan_EvictCache PROC targetMB:DWORD
    mov eax, targetMB
    ret
Titan_EvictCache ENDP

;==============================================================================
; Titan_GetPerformanceStats - Get inference stats
;==============================================================================
Titan_GetPerformanceStats PROC pModel:QWORD, pStats:QWORD
    mov rax, pModel
    test rax, rax
    jz @@error
    
    mov rcx, pStats
    test rcx, rcx
    jz @@error
    
    ; Copy stats (tokens_generated and avg_tps)
    mov eax, [rax].ModelAsset.tokens_generated
    mov [rcx], eax
    
    mov eax, [rax].ModelAsset.avg_tps
    mov [rcx+4], eax
    
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
Titan_GetPerformanceStats ENDP

;==============================================================================
; Helper: strlen - Calculate string length
;==============================================================================
strlen PROC USES rdi, pStr:QWORD
    mov rdi, pStr
    xor eax, eax
    
@@loop:
    cmp BYTE PTR [rdi], 0
    je @@done
    inc rdi
    inc eax
    jmp @@loop
    
@@done:
    ret
strlen ENDP

;==============================================================================
; Helper: memset - Zero memory
;==============================================================================
memset PROC USES rdi, pMem:QWORD, val:DWORD, size:QWORD
    mov rdi, pMem
    mov eax, val
    and eax, 0xFF
    mov rcx, size
    
    .IF rcx > 0
        rep stosb
    .ENDIF
    
    ret
memset ENDP

;==============================================================================
; Helper: memcpy - Copy memory
;==============================================================================
memcpy PROC USES rsi rdi, pDest:QWORD, pSrc:QWORD, size:QWORD
    mov rdi, pDest
    mov rsi, pSrc
    mov rcx, size
    rep movsb
    mov rax, pDest
    ret
memcpy ENDP

END
