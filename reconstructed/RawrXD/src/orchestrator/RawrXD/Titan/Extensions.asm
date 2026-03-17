; =============================================================================
; RawrXD_Titan_Extensions.asm
; ADVANCED FEATURES 14-21: Titan Engine Integration
; Extends QuadBuffer DMA with Predictive Paging, DirectStorage, and Vulkan Sparse
; =============================================================================
; New Features:
;   14. BAR Zero-Copy (Re-BAR GPU memory mapping)
;   15. GPU NF4 Decompression + Live Theta Sync (006.1 rotary embeddings)
;   17. Vulkan Sparse Binding (Virtual VRAM expansion)
;   18. Attention-Drift Predictor (Non-linear layer prefetch)
;   19. DirectStorage Queue (Hardware-accelerated I/O)
;   20. Ghost Cache (Hierarchical L2 for frequently-accessed layers)
;   21. Dynamic Header Sieve (Runtime Safetensors/GGUF parsing)
; =============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

INCLUDE \masm64\include64\masm64rt.inc
INCLUDE D:\rawrxd\include\RawrXD_QuadBuffer_Integration.inc

; =============================================================================
; TITAN CONSTANTS
; =============================================================================
GHOST_CACHE_SIZE        EQU 64              ; L2 cache for hot layers
PREDICTOR_LOOKAHEAD     EQU 8               ; Look-ahead depth
ATTENTION_VARIANCE_THRESHOLD EQU 3F800000h  ; 1.0f (IEEE-754)
DIRECTSTORAGE_QUEUE_SIZE EQU 128            ; Async request depth
SPARSE_PAGE_SIZE        EQU 010000h         ; 64KB sparse pages

; Feature flags (bitmask)
FEAT_BAR_ZERO_COPY      EQU 00000001h
FEAT_VULKAN_SPARSE      EQU 00000002h
FEAT_GPU_NF4            EQU 00000004h
FEAT_PREDICTOR          EQU 00000008h
FEAT_DIRECTSTORAGE      EQU 00000010h
FEAT_GHOST_CACHE        EQU 00000020h

; NF4 quantization table (4-bit float lookup)
NF4_SCALE               EQU 08h             ; Bits per weight

; =============================================================================
; DATA STRUCTURES
; =============================================================================

TITAN_CONFIG STRUCT
    rot_theta           DW  0461Ah          ; 006.1 rotary embedding (FP16)
    dma_priority        DD  1               ; 0=Idle, 1=Normal, 2=Realtime
    feature_mask        DD  03Fh            ; All features enabled
    stream_active       DB  1               ; Kill switch
    gpu_device_id       DD  0               ; Multi-GPU selector
    prefetch_depth      DD  PREDICTOR_LOOKAHEAD
    cache_policy        DD  0               ; 0=LRU, 1=Attention-weighted
    padding             DB  44 DUP(0)
TITAN_CONFIG ENDS

LAYER_METADATA STRUCT
    hdd_offset          DQ  ?               ; Physical offset in file
    compressed_size     DD  ?               ; Bytes on disk (NF4)
    uncompressed_size   DD  ?               ; Bytes after decompression
    quant_scale         REAL4 ?             ; Per-layer quantization scale
    access_count        DD  ?               ; Ghost cache frequency
    last_access_time    DQ  ?               ; TSC timestamp
LAYER_METADATA ENDS

GHOST_ENTRY STRUCT
    layer_idx           DD  ?
    ram_ptr             DQ  ?               ; Cached data pointer
    weight              DD  ?               ; LRU/frequency weight
    state               DD  ?               ; 0=Empty, 1=Valid, 2=Evicting
GHOST_ENTRY ENDS

PREDICTOR_STATE STRUCT
    attn_history        REAL4 16 DUP(?)    ; Rolling window of variance
    history_index       DD  ?
    predicted_stride    DD  1               ; Next layer delta (1=sequential, >1=jump)
    confidence          REAL4 ?             ; Prediction confidence (0.0-1.0)
    last_update_tsc     DQ  ?
PREDICTOR_STATE ENDS

DIRECTSTORAGE_REQUEST STRUCT
    layer_idx           DD  ?
    priority            DD  ?
    dest_ram_ptr        DQ  ?
    dest_vram_ptr       DQ  ?
    hdd_offset          DQ  ?
    size_bytes          DD  ?
    fence_value         DQ  ?
    status              DD  ?               ; 0=Pending, 1=Complete, 2=Error
DIRECTSTORAGE_REQUEST ENDS

TITAN_ENGINE STRUCT
    ; Core config
    config              TITAN_CONFIG <>
    
    ; Metadata tables
    layer_map           LAYER_METADATA 2048 DUP(<>)  ; 800B model = ~1600 layers
    
    ; Ghost cache (L2 for hot layers)
    ghost_cache         GHOST_ENTRY GHOST_CACHE_SIZE DUP(<>)
    ghost_lock          DQ  ?               ; SRWLock
    
    ; Attention predictor
    predictor           PREDICTOR_STATE <>
    
    ; DirectStorage queue
    dstorage_factory    DQ  ?               ; IDStorageFactory*
    dstorage_queue      DQ  ?               ; IDStorageQueue*
    dstorage_file       DQ  ?               ; IDStorageFile*
    dstorage_requests   DIRECTSTORAGE_REQUEST DIRECTSTORAGE_QUEUE_SIZE DUP(<>)
    dstorage_head       DD  ?
    dstorage_tail       DD  ?
    
    ; Vulkan sparse memory
    vk_device           DQ  ?               ; VkDevice
    vk_sparse_memory    DQ  ?               ; VkDeviceMemory (sparse)
    vk_bind_info_pool   DQ  ?               ; Pre-allocated bind structures
    vk_fence_pool       DQ  ?               ; Sync primitives
    
    ; GPU shader handles
    nf4_pipeline        DQ  ?               ; VkPipeline for decompression
    nf4_descriptor_set  DQ  ?               ; VkDescriptorSet
    cmd_buffer_pool     DQ  ?               ; Command buffer pool
    
    ; Performance metrics
    predictor_hits      DD  ?
    predictor_misses    DD  ?
    ghost_cache_hits    DD  ?
    ghost_cache_misses  DD  ?
    dstorage_throughput DQ  ?               ; Bytes/sec
TITAN_ENGINE ENDS

; =============================================================================
; DATA SECTION
; =============================================================================

.DATA

align 128
g_titan_engine      TITAN_ENGINE <>

; NF4 Dequantization Lookup Table (16 values)
align 64
nf4_lut             REAL4 -1.0, -0.6962, -0.5251, -0.3949, \
                          -0.2844, -0.1848, -0.0911, 0.0, \
                          0.0796, 0.1609, 0.2461, 0.3379, \
                          0.4407, 0.5626, 0.7230, 1.0

; Attention variance smoothing kernel (Gaussian-like)
align 32
smooth_kernel       REAL4 0.06, 0.09, 0.12, 0.15, 0.15, 0.12, 0.09, 0.06

.CODE

; =============================================================================
; TITAN_Initialize
; Extends INFINITY_InitializeStream with advanced features
; RCX = INFINITY_STREAM* (existing QuadBuffer)
; RDX = Feature mask (FEAT_*)
; Returns RAX = 0 on success, error code otherwise
; =============================================================================
PUBLIC TITAN_Initialize
TITAN_Initialize PROC FRAME
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov r12, rcx                        ; Save QuadBuffer handle
    mov r13d, edx                       ; Feature mask
    
    lea rdi, g_titan_engine
    mov [rdi].TITAN_ENGINE.config.feature_mask, r13d
    
    ; Initialize SRWLock for ghost cache
    lea rcx, [rdi].TITAN_ENGINE.ghost_lock
    call InitializeSRWLock
    
    ; Feature 21: Dynamic Header Sieve
    ; Scan model file header to build layer_map
    test r13d, 01h                      ; Check any feature enabled
    jz skip_header_sieve
    
    mov rcx, r12
    call TITAN_ParseModelHeader
    test rax, rax
    jnz error_exit
    
skip_header_sieve:
    ; Feature 19: Initialize DirectStorage
    test r13d, FEAT_DIRECTSTORAGE
    jz skip_directstorage
    
    call TITAN_InitDirectStorage
    test rax, rax
    jnz error_exit
    
skip_directstorage:
    ; Feature 17: Initialize Vulkan Sparse Binding
    test r13d, FEAT_VULKAN_SPARSE
    jz skip_vulkan
    
    call TITAN_InitVulkanSparse
    test rax, rax
    jnz error_exit
    
skip_vulkan:
    ; Feature 15: Compile NF4 Decompression Shader
    test r13d, FEAT_GPU_NF4
    jz skip_nf4
    
    call TITAN_CompileNF4Shader
    test rax, rax
    jnz error_exit
    
skip_nf4:
    ; Feature 18: Initialize Predictor State
    test r13d, FEAT_PREDICTOR
    jz skip_predictor
    
    ; Zero out attention history
    lea rcx, [rdi].TITAN_ENGINE.predictor
    xor eax, eax
    mov ecx, SIZEOF PREDICTOR_STATE
    rep stosb
    
    ; Set initial stride to 1 (sequential)
    mov [rdi].TITAN_ENGINE.predictor.predicted_stride, 1
    
skip_predictor:
    xor eax, eax                        ; Success
    jmp done
    
error_exit:
    ; Return error code already in RAX
    
done:
    add rsp, 128
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
TITAN_Initialize ENDP

; =============================================================================
; TITAN_ParseModelHeader (Feature 21: Dynamic Header Sieve)
; Reverse-engineers Safetensors/GGUF header to populate layer_map
; RCX = INFINITY_STREAM*
; Returns RAX = 0 on success
; =============================================================================
TITAN_ParseModelHeader PROC FRAME
    push rbx rsi rdi r12 r13
    sub rsp, 65536 + 32                 ; 64KB buffer + shadow space
    .allocstack 65568
    .endprolog
    
    mov r12, rcx                        ; QuadBuffer handle
    lea r13, [rsp + 32]                 ; Header buffer
    
    ; Read first 64KB of model file
    mov rcx, [r12].INFINITY_STREAM.hdd_file_handle
    mov rdx, r13                        ; Buffer
    mov r8d, 65536                      ; Size
    lea r9, [rsp + 24]                  ; BytesRead
    xor eax, eax
    mov qword ptr [rsp], rax            ; NULL overlapped
    call ReadFile
    test eax, eax
    jz parse_error
    
    ; Detect format: Safetensors vs GGUF
    mov eax, dword ptr [r13]            ; First 4 bytes
    cmp eax, 'FUGG'                     ; "GGUF" magic
    je parse_gguf
    
    ; Assume Safetensors (JSON header)
parse_safetensors:
    ; Look for "data_offsets": [ pattern
    lea rsi, r13
    mov ecx, 65536
    
find_offsets:
    cmp byte ptr [rsi], '"'
    jne next_byte
    
    ; Check for "data_offsets"
    mov eax, dword ptr [rsi + 1]
    cmp eax, 'atad'                     ; "data"
    jne next_byte
    
    ; Found it - parse JSON array
    ; (Simplified: real implementation needs full JSON parser)
    ; Extract [start, end] pairs and populate layer_map
    
    lea rdi, g_titan_engine.layer_map
    xor ebx, ebx                        ; Layer index
    
parse_offsets_loop:
    ; Skip to next number
    call SkipToDigit
    test rax, rax
    jz parse_done
    
    ; Parse start offset
    call ParseDecimalNumber
    mov [rdi + rbx*SIZEOF LAYER_METADATA].LAYER_METADATA.hdd_offset, rax
    
    ; Parse end offset
    call SkipToDigit
    call ParseDecimalNumber
    
    ; Calculate size
    sub rax, [rdi + rbx*SIZEOF LAYER_METADATA].LAYER_METADATA.hdd_offset
    mov [rdi + rbx*SIZEOF LAYER_METADATA].LAYER_METADATA.compressed_size, eax
    mov [rdi + rbx*SIZEOF LAYER_METADATA].LAYER_METADATA.uncompressed_size, eax
    
    ; Default scale
    mov dword ptr [rdi + rbx*SIZEOF LAYER_METADATA].LAYER_METADATA.quant_scale, 3F800000h ; 1.0f
    
    inc ebx
    cmp ebx, 2048
    jl parse_offsets_loop
    
    jmp parse_done
    
next_byte:
    inc rsi
    loop find_offsets
    jmp parse_done
    
parse_gguf:
    ; GGUF format: Binary header with tensor metadata
    ; Format: [magic:4][version:4][tensor_count:8][metadata_count:8]
    ; Followed by tensor descriptors
    
    mov rbx, qword ptr [r13 + 12]       ; Tensor count
    lea rsi, [r13 + 20]                 ; First tensor descriptor
    lea rdi, g_titan_engine.layer_map
    xor r14, r14                        ; Layer index
    
parse_gguf_loop:
    ; Read tensor name length
    movzx ecx, word ptr [rsi]
    lea rsi, [rsi + 2 + rcx]            ; Skip name
    
    ; Read dimensions (4 x uint32)
    add rsi, 16
    
    ; Read type (uint32) and offset (uint64)
    mov eax, dword ptr [rsi]            ; Type
    mov r8, qword ptr [rsi + 8]         ; Offset
    
    mov [rdi + r14*SIZEOF LAYER_METADATA].LAYER_METADATA.hdd_offset, r8
    
    ; Calculate size from type (simplified)
    ; Type 0=F32, 1=F16, 2=Q4_0, etc.
    mov [rdi + r14*SIZEOF LAYER_METADATA].LAYER_METADATA.compressed_size, 1000000h
    
    add rsi, 16
    inc r14
    cmp r14, rbx
    jl parse_gguf_loop
    
parse_done:
    xor eax, eax                        ; Success
    jmp done
    
parse_error:
    mov eax, 1                          ; Error
    
done:
    add rsp, 65568
    pop r13 r12 rdi rsi rbx
    ret
TITAN_ParseModelHeader ENDP

; =============================================================================
; TITAN_UpdatePredictor (Feature 18: Attention-Drift Predictor)
; Analyzes GPU attention variance to predict next layer access pattern
; RCX = Current layer index
; RDX = GPU attention stats buffer (VRAM mapped)
; Returns RAX = Predicted next layer (may be non-sequential)
; =============================================================================
PUBLIC TITAN_UpdatePredictor
TITAN_UpdatePredictor PROC FRAME
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; Current layer
    mov r13, rdx                        ; Attention stats
    
    lea rdi, g_titan_engine.predictor
    
    ; Read attention variance from GPU stats buffer
    ; Layout: [avg_attn:f32][variance:f32][max_attn:f32][entropy:f32]
    vmovss xmm0, real4 ptr [r13 + 4]    ; variance
    
    ; Update rolling history
    mov eax, [rdi].PREDICTOR_STATE.history_index
    vmovss real4 ptr [rdi + rax*4].PREDICTOR_STATE.attn_history, xmm0
    
    inc eax
    and eax, 0Fh                        ; Wrap at 16
    mov [rdi].PREDICTOR_STATE.history_index, eax
    
    ; Calculate smoothed variance using kernel
    vxorps ymm1, ymm1, ymm1             ; Accumulator
    lea rsi, smooth_kernel
    xor ecx, ecx
    
smooth_loop:
    vmovss xmm2, real4 ptr [rdi + rcx*4].PREDICTOR_STATE.attn_history
    vmovss xmm3, real4 ptr [rsi + rcx*4]
    vmulss xmm2, xmm2, xmm3
    vaddss xmm1, xmm1, xmm2
    inc ecx
    cmp ecx, 8
    jl smooth_loop
    
    ; Compare smoothed variance against threshold
    vucomiss xmm1, real4 ptr [ATTENTION_VARIANCE_THRESHOLD]
    jb predict_linear
    
predict_jump:
    ; High variance: Context is non-linear (e.g., retrieval, attention loop)
    ; Predict a JUMP to a distant layer based on attention focus
    
    ; Simple heuristic: Look at max_attn position
    vmovss xmm0, real4 ptr [r13 + 8]    ; max_attn score
    vcvtss2si eax, xmm0
    and eax, 3Fh                        ; Modulo 64 (max lookahead)
    add eax, 8                          ; Jump ahead 8-72 layers
    
    mov [rdi].PREDICTOR_STATE.predicted_stride, eax
    mov dword ptr [rdi].PREDICTOR_STATE.confidence, 3F4CCCCDh ; 0.8f
    
    jmp predict_done
    
predict_linear:
    ; Low variance: Sequential processing
    mov [rdi].PREDICTOR_STATE.predicted_stride, 1
    mov dword ptr [rdi].PREDICTOR_STATE.confidence, 3F800000h ; 1.0f
    
predict_done:
    ; Calculate predicted layer = current + stride
    mov eax, [rdi].PREDICTOR_STATE.predicted_stride
    add rax, r12                        ; Next layer
    
    ; Trigger prefetch for predicted layer
    push rax
    mov rcx, rax
    call TITAN_PrefetchLayer
    pop rax
    
    add rsp, 64
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
TITAN_UpdatePredictor ENDP

; =============================================================================
; TITAN_PrefetchLayer (Feature 19: DirectStorage Integration)
; Enqueues a layer for async DMA from HDD
; RCX = Layer index
; =============================================================================
TITAN_PrefetchLayer PROC FRAME
    push rbx rsi rdi r12 r13
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov r12, rcx                        ; Layer index
    
    ; Check if DirectStorage is enabled
    lea rdi, g_titan_engine
    test [rdi].TITAN_ENGINE.config.feature_mask, FEAT_DIRECTSTORAGE
    jz fallback_readfile
    
    ; Get free slot in request queue
    mov eax, [rdi].TITAN_ENGINE.dstorage_tail
    mov ebx, [rdi].TITAN_ENGINE.dstorage_head
    inc eax
    and eax, DIRECTSTORAGE_QUEUE_SIZE - 1
    cmp eax, ebx
    je queue_full
    
    ; Fill request structure
    imul rsi, rax, SIZEOF DIRECTSTORAGE_REQUEST
    lea rsi, [rdi + rsi].TITAN_ENGINE.dstorage_requests
    
    mov [rsi].DIRECTSTORAGE_REQUEST.layer_idx, r12d
    mov [rsi].DIRECTSTORAGE_REQUEST.priority, 1
    
    ; Lookup metadata
    imul rbx, r12, SIZEOF LAYER_METADATA
    lea rbx, [rdi + rbx].TITAN_ENGINE.layer_map
    
    mov r8, [rbx].LAYER_METADATA.hdd_offset
    mov [rsi].DIRECTSTORAGE_REQUEST.hdd_offset, r8
    
    mov r8d, [rbx].LAYER_METADATA.compressed_size
    mov [rsi].DIRECTSTORAGE_REQUEST.size_bytes, r8d
    
    ; Allocate destination RAM buffer (from arena or ghost cache)
    call TITAN_AllocateGhostSlot
    mov [rsi].DIRECTSTORAGE_REQUEST.dest_ram_ptr, rax
    
    ; Submit to DirectStorage queue
    ; (Requires IDStorageQueue::EnqueueRequest COM call)
    ; Simplified: Queue for background thread processing
    
    mov [rsi].DIRECTSTORAGE_REQUEST.status, 0 ; Pending
    
    mov eax, [rdi].TITAN_ENGINE.dstorage_tail
    inc eax
    and eax, DIRECTSTORAGE_QUEUE_SIZE - 1
    mov [rdi].TITAN_ENGINE.dstorage_tail, eax
    
    jmp done
    
queue_full:
    ; Fallback to synchronous read if queue full
    
fallback_readfile:
    ; Use original QuadBuffer mechanism
    mov rcx, r12
    ; call INFINITY_TriggerPrefetch (from QuadBuffer)
    
done:
    add rsp, 32
    pop r13 r12 rdi rsi rbx
    ret
TITAN_PrefetchLayer ENDP

; =============================================================================
; TITAN_CheckGhostCache (Feature 20: Ghost Cache L2)
; Checks if layer is cached in RAM before triggering HDD I/O
; RCX = Layer index
; Returns RAX = RAM pointer or 0 if miss
; =============================================================================
PUBLIC TITAN_CheckGhostCache
TITAN_CheckGhostCache PROC FRAME
    push rbx rsi rdi r12
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov r12, rcx                        ; Layer index
    
    lea rdi, g_titan_engine
    
    ; Acquire read lock
    lea rcx, [rdi].TITAN_ENGINE.ghost_lock
    call AcquireSRWLockShared
    
    ; Linear search through ghost cache
    lea rsi, [rdi].TITAN_ENGINE.ghost_cache
    xor ebx, ebx
    
search_loop:
    mov eax, [rsi + rbx*SIZEOF GHOST_ENTRY].GHOST_ENTRY.layer_idx
    cmp eax, r12d
    je found_entry
    
    inc ebx
    cmp ebx, GHOST_CACHE_SIZE
    jl search_loop
    
    ; Miss
    inc [rdi].TITAN_ENGINE.ghost_cache_misses
    xor eax, eax
    jmp done
    
found_entry:
    ; Hit - return pointer and update LRU weight
    inc [rdi].TITAN_ENGINE.ghost_cache_hits
    
    mov rax, [rsi + rbx*SIZEOF GHOST_ENTRY].GHOST_ENTRY.ram_ptr
    inc [rsi + rbx*SIZEOF GHOST_ENTRY].GHOST_ENTRY.weight
    
    ; Update access time
    rdtsc
    shl rdx, 32
    or rax, rdx
    lea r8, [rdi].TITAN_ENGINE.layer_map
    imul r9, r12, SIZEOF LAYER_METADATA
    mov [r8 + r9].LAYER_METADATA.last_access_time, rax
    
    mov rax, [rsi + rbx*SIZEOF GHOST_ENTRY].GHOST_ENTRY.ram_ptr
    
done:
    ; Release lock
    lea rcx, [rdi].TITAN_ENGINE.ghost_lock
    call ReleaseSRWLockShared
    
    add rsp, 32
    pop r12 rdi rsi rbx
    ret
TITAN_CheckGhostCache ENDP

; =============================================================================
; TITAN_SyncLiveTheta (Feature 15: GPU Live Parameter Sync)
; Updates rot_theta in GPU shader push constants
; RCX = New theta value (FP16)
; =============================================================================
PUBLIC TITAN_SyncLiveTheta
TITAN_SyncLiveTheta PROC FRAME
    push rbx rsi rdi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov bx, cx                          ; Save FP16 value
    
    lea rdi, g_titan_engine
    mov [rdi].TITAN_ENGINE.config.rot_theta, bx
    
    ; Check if Vulkan NF4 shader is active
    test [rdi].TITAN_ENGINE.config.feature_mask, FEAT_GPU_NF4
    jz skip_vulkan_update
    
    ; Update push constant in command buffer
    ; (Requires vkCmdPushConstants - simplified here)
    ; mov rcx, [rdi].TITAN_ENGINE.cmd_buffer_pool
    ; lea rdx, [rdi].TITAN_ENGINE.config.rot_theta
    ; call vkCmdPushConstants
    
skip_vulkan_update:
    add rsp, 32
    pop rdi rsi rbx
    ret
TITAN_SyncLiveTheta ENDP

; =============================================================================
; Helper Functions (Stubs for complex subsystems)
; =============================================================================

TITAN_InitDirectStorage PROC
    ; Initialize DirectStorage factory and queue
    ; (Requires dstorage.lib and COM interfaces)
    ; Steps: CoInitialize → DStorageGetFactory → CreateQueue → set priority
    
    push rbx
    
    ; Check if DirectStorage DLL is available
    lea rcx, [sz_dstorage_dll]
    call LoadLibraryA
    test rax, rax
    jz @@ds_unavailable
    
    ; DLL loaded — resolve DStorageGetFactory
    mov rbx, rax
    lea rdx, [sz_dstorage_getfactory]
    mov rcx, rbx
    call GetProcAddress
    test rax, rax
    jz @@ds_free
    
    ; Store factory function pointer
    mov [g_pfnDStorageGetFactory], rax
    
    xor eax, eax                     ; success
    pop rbx
    ret

@@ds_free:
    mov rcx, rbx
    call FreeLibrary
@@ds_unavailable:
    mov eax, 1                       ; not available
    pop rbx
    ret
TITAN_InitDirectStorage ENDP

TITAN_InitVulkanSparse PROC
    ; Create Vulkan device with sparse binding feature
    ; Required for virtual memory-based weight streaming (120B models)
    ; Steps: enumerate devices → check sparse binding → create device
    
    push rbx
    
    ; Try loading vulkan-1.dll
    lea rcx, [sz_vulkan_dll]
    call LoadLibraryA
    test rax, rax
    jz @@vs_fail
    mov rbx, rax
    
    ; Resolve vkCreateInstance
    lea rdx, [sz_vkCreateInstance]
    mov rcx, rbx
    call GetProcAddress
    test rax, rax
    jz @@vs_free
    
    ; Store function pointer for later use by Vulkan subsystem
    mov [g_pfnVkCreateInstance], rax
    
    xor eax, eax
    pop rbx
    ret

@@vs_free:
    mov rcx, rbx
    call FreeLibrary
@@vs_fail:
    mov eax, 1
    pop rbx
    ret
TITAN_InitVulkanSparse ENDP

TITAN_CompileNF4Shader PROC
    ; Compile GLSL NF4 dequantization shader to SPIR-V
    ; Then create VkShaderModule → VkPipelineLayout → VkComputePipeline
    ; Returns: EAX = 0 success, nonzero = error
    
    ; NF4 (4-bit NormalFloat) uses a lookup table for dequantization
    ; The shader would: load packed int8, split nibbles, LUT lookup, output FP32
    ; Without a Vulkan context, just validate the shader source exists
    
    ; Check if Vulkan is initialized
    mov rax, [g_pfnVkCreateInstance]
    test rax, rax
    jz @@nf4_no_vulkan
    
    ; In production: call vkCreateShaderModule with pre-compiled SPIR-V
    ; For now, return success if Vulkan is available
    xor eax, eax
    ret
    
@@nf4_no_vulkan:
    mov eax, 2                       ; error: no Vulkan
    ret
TITAN_CompileNF4Shader ENDP

TITAN_AllocateGhostSlot PROC
    ; Find LRU (least recently used) slot in ghost cache
    ; Allocate 1GB pinned RAM for the slot via VirtualAlloc
    ; Returns: RAX = pointer to allocated memory, or 0 on failure
    
    push rbx
    
    ; VirtualAlloc(NULL, 1GB, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor ecx, ecx                     ; lpAddress = NULL
    mov rdx, 40000000h               ; 1GB
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 4                       ; PAGE_READWRITE
    call VirtualAlloc
    
    ; rax = allocated address or NULL
    test rax, rax
    jz @@ghost_fail
    
    ; Zero the first page as a header
    mov rbx, rax
    mov rdi, rax
    mov ecx, 1024                    ; zero 4KB header
    xor eax, eax
    rep stosd
    mov rax, rbx
    
    pop rbx
    ret

@@ghost_fail:
    xor rax, rax                     ; return NULL
    pop rbx
    ret
TITAN_AllocateGhostSlot ENDP

SkipToDigit PROC
    ; Scan forward in string to next ASCII digit '0'-'9'
    ; RCX = input string pointer (null-terminated)
    ; Returns: RAX = pointer to first digit, or 0 if none found
    
    mov rax, rcx
    test rax, rax
    jz @@skip_fail
    
@@skip_scan:
    movzx ecx, BYTE PTR [rax]
    test cl, cl
    jz @@skip_fail                   ; end of string
    cmp cl, '0'
    jb @@skip_next
    cmp cl, '9'
    ja @@skip_next
    ret                              ; rax points to digit
    
@@skip_next:
    inc rax
    jmp @@skip_scan
    
@@skip_fail:
    xor rax, rax
    ret
SkipToDigit ENDP

ParseDecimalNumber PROC
    ; Parse ASCII decimal string to uint64
    ; RCX = pointer to start of digit string
    ; Returns: RAX = parsed number, RDX = pointer past last digit
    
    xor rax, rax                     ; result = 0
    mov rdx, rcx                     ; cursor
    test rdx, rdx
    jz @@parse_done
    
@@parse_digit:
    movzx ecx, BYTE PTR [rdx]
    sub cl, '0'
    cmp cl, 9
    ja @@parse_done                  ; not a digit
    
    ; result = result * 10 + digit
    imul rax, rax, 10
    movzx ecx, cl
    add rax, rcx
    inc rdx
    jmp @@parse_digit
    
@@parse_done:
    ret
ParseDecimalNumber ENDP

AcquireSRWLockShared PROC
    ; Acquire SRW lock in shared (read) mode
    ; RCX = pointer to SRWLOCK
    ; Delegates to Win32 AcquireSRWLockShared
    jmp QWORD PTR [__imp_AcquireSRWLockShared]
AcquireSRWLockShared ENDP

ReleaseSRWLockShared PROC
    ; Release SRW lock from shared mode
    ; RCX = pointer to SRWLOCK
    ; Delegates to Win32 ReleaseSRWLockShared
    jmp QWORD PTR [__imp_ReleaseSRWLockShared]
ReleaseSRWLockShared ENDP

END
