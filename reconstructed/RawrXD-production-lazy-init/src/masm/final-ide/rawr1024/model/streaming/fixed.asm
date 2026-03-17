;==========================================================================
; rawr1024_model_streaming.asm - GGUF Model Streaming & Memory Management
; ==========================================================================
; Smart model loading with streaming for large models, memory management,
; idle model unloading, and cache eviction to prevent memory bloat
;==========================================================================

option casemap:none

;==========================================================================
; MODEL STREAMING CONSTANTS
;==========================================================================
GGUF_MAGIC              EQU 046554747h  ; "GGUF" in little-endian
GGUF_CHUNK_SIZE         EQU 4194304     ; 4MB chunks for streaming
MODEL_MEMORY_THRESHOLD  EQU 536870912   ; 512MB - threshold for streaming
IDLE_TIMEOUT_MS         EQU 300000      ; 5 minutes idle timeout
MAX_LOADED_MODELS       EQU 8
CACHE_EVICT_THRESHOLD   EQU 90          ; 90% memory - trigger eviction

;==========================================================================
; MODEL STREAM STRUCTURE
;==========================================================================
MODEL_STREAM STRUCT
    model_id            DWORD ?         ; Unique model ID
    file_handle         QWORD ?         ; File handle for streaming
    model_size          QWORD ?         ; Total model size in bytes
    loaded_size         QWORD ?         ; Currently loaded bytes
    is_streaming        DWORD ?         ; 1=streaming, 0=fully loaded
    last_access_time    QWORD ?         ; Timestamp of last access
    memory_base         QWORD ?         ; Base memory pointer
    tensor_count        DWORD ?         ; Number of tensors
    quantization_type   DWORD ?         ; Q4_0, Q5_0, etc.
    ref_count           DWORD ?         ; Reference count
    state               DWORD ?         ; 0=idle, 1=active, 2=evicting
MODEL_STREAM ENDS

;==========================================================================
; MODEL MEMORY MANAGER STRUCTURE
;==========================================================================
MODEL_MEMORY_MGR STRUCT
    total_memory        QWORD ?         ; Total available memory
    used_memory         QWORD ?         ; Currently used memory
    models_count        DWORD ?         ; Number of loaded models
    next_model_id       DWORD ?         ; Next model ID to assign
    last_evict_time     QWORD ?         ; Last eviction timestamp
    eviction_policy     DWORD ?         ; 0=LRU, 1=LFU, 2=ARC
    max_models          DWORD ?         ; Max concurrent models
MODEL_MEMORY_MGR ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Model streaming array
    PUBLIC loaded_models
    loaded_models       MODEL_STREAM MAX_LOADED_MODELS DUP (<>)
    
    ; Model memory manager
    PUBLIC model_mgr
    model_mgr           MODEL_MEMORY_MGR <>
    
    ; Memory tracking
    streaming_buffer    QWORD 0         ; Streaming I/O buffer pointer
    streaming_pos       QWORD 0         ; Current position in stream
    
    ; Status messages
    msg_model_load      BYTE "Loading GGUF model: ", 0
    msg_model_stream    BYTE "Streaming model (size=%lld MB)...", 0Ah, 0
    msg_model_unload    BYTE "Unloading idle model ID=%d", 0Ah, 0
    msg_memory_evict    BYTE "Evicting %d%% memory due to pressure", 0Ah, 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;--------------------------------------------------------------------------
; Initialize Model Memory Manager
;--------------------------------------------------------------------------
PUBLIC rawr1024_model_memory_init
rawr1024_model_memory_init PROC
    ; Input: RCX = total_memory_bytes
    ; Output: RAX = manager pointer
    
    push    rbp
    mov     rbp, rsp
    
    lea     rax, model_mgr
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.total_memory, rcx
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory, 0
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.models_count, 0
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id, 1
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.eviction_policy, 0  ; LRU
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.max_models, MAX_LOADED_MODELS
    
    pop     rbp
    ret
rawr1024_model_memory_init ENDP

;--------------------------------------------------------------------------
; Stream GGUF Model with Intelligent Loading
;--------------------------------------------------------------------------
PUBLIC rawr1024_stream_gguf_model
rawr1024_stream_gguf_model PROC
    ; Input: RCX = file_path, RDX = preferred_engine_id (or 0 for auto)
    ; Output: RAX = model_id (0 if failed)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    
    ; Find available model slot
    lea     rsi, loaded_models
    xor     ebx, ebx
find_slot:
    cmp     ebx, MAX_LOADED_MODELS
    jge     stream_fail
    
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.model_id
    cmp     eax, 0
    je      slot_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     ebx
    jmp     find_slot
    
slot_found:
    ; Allocate model ID
    lea     rax, model_mgr
    mov     r12d, DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id
    inc     DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id
    
    mov     DWORD PTR [rsi].MODEL_STREAM.model_id, r12d
    mov     QWORD PTR [rsi].MODEL_STREAM.file_handle, rcx  ; Store path as handle for now
    
    ; Get file size (stub: assume 600MB for demo)
    mov     QWORD PTR [rsi].MODEL_STREAM.model_size, 629145600
    mov     DWORD PTR [rsi].MODEL_STREAM.loaded_size, 0
    
    ; Decide: stream if >512MB, else full load
    cmp     QWORD PTR [rsi].MODEL_STREAM.model_size, MODEL_MEMORY_THRESHOLD
    jle     alloc_exact
    mov     DWORD PTR [rsi].MODEL_STREAM.is_streaming, 1
    mov     rcx, GGUF_CHUNK_SIZE
    
alloc_exact:
    ; Full load: allocate full size (or chunk if too large)
    cmp     rcx, GGUF_CHUNK_SIZE
    jle     alloc_exact_cont
    mov     rcx, GGUF_CHUNK_SIZE
    
alloc_exact_cont:
    ; TODO: custom_malloc call would go here
    ; For now, assume allocation succeeds
    mov     rax, 0x1000000      ; Placeholder address
    mov     QWORD PTR [rsi].MODEL_STREAM.memory_base, rax
    jmp     load_complete
    
alloc_streaming:
    ; Streaming: allocate chunk buffer
    mov     rcx, GGUF_CHUNK_SIZE
    mov     rax, 0x2000000      ; Placeholder address
    mov     QWORD PTR [rsi].MODEL_STREAM.memory_base, rax
    
load_complete:
    ; Set metadata
    mov     DWORD PTR [rsi].MODEL_STREAM.tensor_count, 0
    mov     DWORD PTR [rsi].MODEL_STREAM.quantization_type, 0
    mov     DWORD PTR [rsi].MODEL_STREAM.ref_count, 1
    mov     DWORD PTR [rsi].MODEL_STREAM.state, 1  ; Active
    
    ; Update timestamp
    rdtsc
    mov     QWORD PTR [rsi].MODEL_STREAM.last_access_time, rax
    
    ; Increment model count in manager
    lea     rax, model_mgr
    mov     ecx, DWORD PTR [rax].MODEL_MEMORY_MGR.models_count
    inc     ecx
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.models_count, ecx
    
    ; Update used memory
    mov     rdx, QWORD PTR [rsi].MODEL_STREAM.model_size
    mov     rcx, QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory
    add     rcx, rdx
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory, rcx
    
    ; Return model ID
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.model_id
    jmp     stream_exit
    
stream_fail:
    xor     eax, eax
    
stream_exit:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_stream_gguf_model ENDP

;--------------------------------------------------------------------------
; Check Memory Pressure and Trigger Eviction
;--------------------------------------------------------------------------
PUBLIC rawr1024_check_memory_pressure
rawr1024_check_memory_pressure PROC
    ; Input: None
    ; Output: EAX = percentage used (0-100)
    
    push    rbp
    mov     rbp, rsp
    
    lea     rax, model_mgr
    mov     rcx, QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory
    mov     rdx, QWORD PTR [rax].MODEL_MEMORY_MGR.total_memory
    
    ; Calculate percent: (used * 100) / total
    test    rdx, rdx
    jz      pressure_zero
    
    mov     rax, rcx
    mov     rcx, 100
    mul     rcx
    div     rdx
    mov     ecx, eax
    
    ; Check threshold and trigger eviction if needed
    cmp     ecx, CACHE_EVICT_THRESHOLD
    jl      pressure_exit
    
    ; Trigger eviction (multiple calls until pressure drops)
pressure_evict_loop:
    cmp     ecx, CACHE_EVICT_THRESHOLD
    jl      pressure_exit
    
    ; Find and evict oldest idle model
    push    rcx
    call    rawr1024_evict_idle_models
    pop     rcx
    
    ; Recalculate pressure
    lea     rax, model_mgr
    mov     rcx, QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory
    mov     rdx, QWORD PTR [rax].MODEL_MEMORY_MGR.total_memory
    mov     rax, rcx
    mov     rcx, 100
    mul     rcx
    div     rdx
    mov     ecx, eax
    jmp     pressure_evict_loop
    
pressure_zero:
    xor     ecx, ecx
    
pressure_exit:
    mov     eax, ecx
    pop     rbp
    ret
rawr1024_check_memory_pressure ENDP

;--------------------------------------------------------------------------
; Evict Oldest Idle Model
;--------------------------------------------------------------------------
PUBLIC rawr1024_evict_idle_models
rawr1024_evict_idle_models PROC
    ; Input: None
    ; Output: EAX = evicted_model_id (0 if none)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    
    ; Find model with oldest last_access_time and ref_count == 0
    lea     rsi, loaded_models
    xor     ebx, ebx            ; min_idx
    mov     r12, 0xFFFFFFFFFFFFFFFFh  ; min_time
    
    xor     ecx, ecx            ; loop counter
find_oldest_loop:
    cmp     ecx, MAX_LOADED_MODELS
    jge     evict_found
    
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.model_id
    cmp     eax, 0
    je      evict_skip
    
    ; Check ref_count (should be 0 to evict)
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.ref_count
    cmp     eax, 0
    jne     evict_skip
    
    ; Check state (should not be evicting)
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.state
    cmp     eax, 2
    je      evict_skip
    
    ; Compare access time
    mov     rax, QWORD PTR [rsi].MODEL_STREAM.last_access_time
    cmp     rax, r12
    jge     evict_skip
    
    mov     r12, rax
    mov     ebx, ecx
    
evict_skip:
    add     rsi, SIZEOF MODEL_STREAM
    inc     ecx
    jmp     find_oldest_loop
    
evict_found:
    ; If no model found (min_time still 0xFFFF...), return 0
    cmp     r12, 0xFFFFFFFFFFFFFFFFh
    je      evict_none
    
    ; Evict model at index ebx
    imul    rax, rbx, SIZEOF MODEL_STREAM
    lea     rsi, loaded_models
    add     rsi, rax
    
    ; Set state to evicting
    mov     DWORD PTR [rsi].MODEL_STREAM.state, 2
    
    ; Free memory (TODO: custom_free)
    mov     rcx, QWORD PTR [rsi].MODEL_STREAM.memory_base
    ; call    custom_free
    
    ; Update manager
    lea     rax, model_mgr
    mov     rdx, QWORD PTR [rsi].MODEL_STREAM.model_size
    mov     rcx, QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory
    sub     rcx, rdx
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory, rcx
    
    ; Decrement model count
    mov     edx, DWORD PTR [rax].MODEL_MEMORY_MGR.models_count
    dec     edx
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.models_count, edx
    
    ; Clear slot
    mov     DWORD PTR [rsi].MODEL_STREAM.model_id, 0
    
    ; Return evicted model ID
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.model_id
    jmp     evict_exit
    
evict_none:
    xor     eax, eax
    
evict_exit:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_evict_idle_models ENDP

;--------------------------------------------------------------------------
; Update Model Access Time (Keep Warm)
;--------------------------------------------------------------------------
PUBLIC rawr1024_model_access_update
rawr1024_model_access_update PROC
    ; Input: EAX = model_id
    ; Output: None (sets last_access_time and increments ref_count)
    
    push    rbp
    mov    rbp, rsp
    push    rsi
    
    ; Find model in loaded_models
    lea     rsi, loaded_models
    xor     ecx, ecx
model_access_loop:
    cmp     ecx, MAX_LOADED_MODELS
    jge     access_not_found
    
    cmp     DWORD PTR [rsi].MODEL_STREAM.model_id, eax
    je      access_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     ecx
    jmp     model_access_loop
    
access_found:
    ; Update timestamp
    rdtsc
    mov     QWORD PTR [rsi].MODEL_STREAM.last_access_time, rax
    
    ; Increment ref_count
    mov     ecx, DWORD PTR [rsi].MODEL_STREAM.ref_count
    inc     ecx
    mov     DWORD PTR [rsi].MODEL_STREAM.ref_count, ecx
    
access_not_found:
    pop     rsi
    pop     rbp
    ret
rawr1024_model_access_update ENDP

;--------------------------------------------------------------------------
; Release Model Reference (Allow Eviction)
;--------------------------------------------------------------------------
PUBLIC rawr1024_model_release
rawr1024_model_release PROC
    ; Input: EAX = model_id
    ; Output: EAX = ref_count after release (0xFFFFFFFF if not found)
    
    push    rbp
    mov     rbp, rsp
    push    rsi
    
    ; Find model in loaded_models
    lea     rsi, loaded_models
    xor     ecx, ecx
model_rel_loop:
    cmp     ecx, MAX_LOADED_MODELS
    jge     release_fail
    
    cmp     DWORD PTR [rsi].MODEL_STREAM.model_id, eax
    je      model_rel_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     ecx
    jmp     model_rel_loop
    
model_rel_found:
    ; Decrement ref_count
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.ref_count
    cmp     eax, 0
    jle     release_fail
    dec     eax
    mov     DWORD PTR [rsi].MODEL_STREAM.ref_count, eax
    
    jmp     release_exit
    
release_fail:
    mov     eax, 0xFFFFFFFF
    
release_exit:
    pop     rsi
    pop     rbp
    ret
rawr1024_model_release ENDP

END
