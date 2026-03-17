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
    ; Output: RAX = 1 on success, 0 on failure
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    ; Initialize model manager
    lea     rax, model_mgr
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.total_memory, rcx
    mov     QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory, 0
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.models_count, 0
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id, 1
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.eviction_policy, 0  ; LRU
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.max_models, MAX_LOADED_MODELS
    
    ; Zero out model array
    lea     rsi, loaded_models
    xor     ecx, ecx
zero_models_loop:
    cmp     ecx, MAX_LOADED_MODELS
    jge     zero_done
    
    mov     DWORD PTR [rsi].MODEL_STREAM.model_id, 0
    mov     DWORD PTR [rsi].MODEL_STREAM.is_streaming, 0
    mov     DWORD PTR [rsi].MODEL_STREAM.state, 0
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     ecx
    jmp     zero_models_loop
    
zero_done:
    mov     rax, 1
    pop     rbx
    pop     rbp
    ret
rawr1024_model_memory_init ENDP

;--------------------------------------------------------------------------
; Load GGUF Model with Smart Streaming
;--------------------------------------------------------------------------
PUBLIC rawr1024_stream_gguf_model
rawr1024_stream_gguf_model PROC
    ; Input: RCX = file_path, RDX = engine_id
    ; Output: RAX = model_id (or 0 on failure)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    
    mov     r12, rcx            ; file_path
    mov     r13d, edx           ; engine_id
    
    ; Check if we have space for another model
    lea     rax, model_mgr
    mov     ecx, DWORD PTR [rax].MODEL_MEMORY_MGR.models_count
    cmp     ecx, MAX_LOADED_MODELS
    jge     stream_model_fail
    
    ; Find first available model slot
    lea     rsi, loaded_models
    xor     ebx, ebx
find_slot_loop:
    cmp     ebx, MAX_LOADED_MODELS
    jge     stream_model_fail
    
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.model_id
    cmp     eax, 0
    je      slot_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     ebx
    jmp     find_slot_loop
    
slot_found:
    ; Assign model ID from manager
    lea     rax, model_mgr
    mov     ecx, DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id
    mov     DWORD PTR [rsi].MODEL_STREAM.model_id, ecx
    
    ; Increment next model ID
    inc     ecx
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.next_model_id, ecx
    
    ; Get file size
    ; For now, assume we can query file size
    mov     rcx, r12
    call    get_file_size       ; Returns RDX = file_size
    test    rdx, rdx
    jz      stream_model_fail
    
    mov     QWORD PTR [rsi].MODEL_STREAM.model_size, rdx
    mov     QWORD PTR [rsi].MODEL_STREAM.loaded_size, 0
    
    ; Decide: stream if > threshold, else load fully
    cmp     rdx, MODEL_MEMORY_THRESHOLD
    jg      use_streaming
    
    ; Full load
    mov     DWORD PTR [rsi].MODEL_STREAM.is_streaming, 0
    jmp     allocate_model_mem
    
use_streaming:
    ; Streaming load
    mov     DWORD PTR [rsi].MODEL_STREAM.is_streaming, 1
    
allocate_model_mem:
    ; Allocate memory for model
    mov     rcx, rdx
    cmp     DWORD PTR [rsi].MODEL_STREAM.is_streaming, 0
    jne     alloc_streaming
    
    ; Full load: allocate full size (or chunk if too large)
    cmp     rcx, GGUF_CHUNK_SIZE
    jle     alloc_exact
    mov     rcx, GGUF_CHUNK_SIZE
    
alloc_exact:
    ; TODO: custom_malloc call would go here
    ; For now, assume allocation succeeds
    mov     rax, 0x1000         ; Placeholder address
    mov     [rsi + MODEL_STREAM.memory_base], rax
    jmp     load_complete
    
alloc_streaming:
    ; Streaming: allocate chunk buffer
    mov     rcx, GGUF_CHUNK_SIZE
    mov     rax, 0x2000         ; Placeholder address
    mov     [rsi + MODEL_STREAM.memory_base], rax
    
load_complete:
    ; Set metadata
    mov     [rsi + MODEL_STREAM.tensor_count], DWORD 0
    mov     [rsi + MODEL_STREAM.quantization_type], DWORD 0
    mov     [rsi + MODEL_STREAM.ref_count], DWORD 1
    mov     [rsi + MODEL_STREAM.state], DWORD 1  ; Active
    
    ; Update timestamp
    rdtsc
    mov     [rsi + MODEL_STREAM.last_access_time], rax
    
    ; Increment model count in manager
    lea     rax, model_mgr
    mov     ecx, [rax + MODEL_MEMORY_MGR.models_count]
    inc     ecx
    mov     [rax + MODEL_MEMORY_MGR.models_count], ecx
    
    ; Update used memory
    mov     rdx, [rsi + MODEL_STREAM.model_size]
    mov     rcx, [rax + MODEL_MEMORY_MGR.used_memory]
    add     rcx, rdx
    mov     [rax + MODEL_MEMORY_MGR.used_memory], rcx
    
    ; Return model ID
    mov     eax, [rsi + MODEL_STREAM.model_id]
    jmp     stream_model_exit
    
stream_model_fail:
    xor     eax, eax
    
stream_model_exit:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
rawr1024_stream_gguf_model ENDP

;--------------------------------------------------------------------------
; Check Memory Pressure and Evict Idle Models
;--------------------------------------------------------------------------
PUBLIC rawr1024_check_memory_pressure
rawr1024_check_memory_pressure PROC
    ; Input: None
    ; Output: RAX = memory_usage_percent (0-100)
    
    push    rbp
    mov     rbp, rsp
    push    rbx
    
    lea     rax, model_mgr
    mov     rdx, QWORD PTR [rax].MODEL_MEMORY_MGR.used_memory
    mov     rcx, QWORD PTR [rax].MODEL_MEMORY_MGR.total_memory
    
    test    rcx, rcx
    jz      memory_check_zero
    
    ; Calculate percent: (used * 100) / total
    mov     rax, rdx
    mov     rdx, 100
    imul    rax, rdx
    xor     edx, edx
    div     rcx
    
    ; Check if exceeds threshold
    cmp     eax, CACHE_EVICT_THRESHOLD
    jl      memory_check_done
    
    ; Trigger eviction
    call    rawr1024_evict_idle_models
    
memory_check_done:
    pop     rbx
    pop     rbp
    ret
    
memory_check_zero:
    xor     eax, eax
    pop     rbx
    pop     rbp
    ret
rawr1024_check_memory_pressure ENDP

;--------------------------------------------------------------------------
; Evict Oldest Idle Model
;--------------------------------------------------------------------------
PUBLIC rawr1024_evict_idle_models
rawr1024_evict_idle_models PROC
    ; Input: None
    ; Output: RAX = evicted_model_id (or 0 if none)
    
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
    
    mov     eax, [rsi + MODEL_STREAM.model_id]
    cmp     eax, 0
    je      evict_skip
    
    ; Check ref_count (should be 0 to evict)
    mov     eax, [rsi + MODEL_STREAM.ref_count]
    cmp     eax, 0
    jne     evict_skip
    
    ; Check state (should not be evicting)
    mov     eax, [rsi + MODEL_STREAM.state]
    cmp     eax, 2
    je      evict_skip
    
    ; Compare access time
    mov     rax, [rsi + MODEL_STREAM.last_access_time]
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
    mov     [rsi + MODEL_STREAM.state], DWORD 2
    
    ; Free memory (TODO: custom_free)
    mov     rcx, [rsi + MODEL_STREAM.memory_base]
    ; call    custom_free
    
    ; Update manager
    lea     rax, model_mgr
    mov     rdx, [rsi + MODEL_STREAM.model_size]
    mov     rcx, [rax + MODEL_MEMORY_MGR.used_memory]
    sub     rcx, rdx
    mov     [rax + MODEL_MEMORY_MGR.used_memory], rcx
    
    mov     ecx, DWORD PTR [rax].MODEL_MEMORY_MGR.models_count
    dec     ecx
    mov     DWORD PTR [rax].MODEL_MEMORY_MGR.models_count, ecx
    
    ; Clear model slot
    mov     DWORD PTR [rsi].MODEL_STREAM.model_id, 0
    mov     DWORD PTR [rsi].MODEL_STREAM.state, 0
    
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
; Update Model Access Time (on use)
;--------------------------------------------------------------------------
PUBLIC rawr1024_model_access_update
rawr1024_model_access_update PROC
    ; Input: RCX = model_id
    ; Output: RAX = 1 on success, 0 on failure
    
    push    rbp
    mov     rbp, rsp
    push    rsi
    
    ; Find model by ID
    lea     rsi, loaded_models
    xor     eax, eax
find_model_loop:
    cmp     eax, MAX_LOADED_MODELS
    jge     access_fail
    
    mov     edx, DWORD PTR [rsi].MODEL_STREAM.model_id
    cmp     edx, ecx
    je      model_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     eax
    jmp     find_model_loop
    
model_found:
    ; Update timestamp to current time
    rdtsc
    mov     QWORD PTR [rsi].MODEL_STREAM.last_access_time, rax
    
    ; Increment ref_count
    mov     eax, DWORD PTR [rsi].MODEL_STREAM.ref_count
    inc     eax
    mov     DWORD PTR [rsi].MODEL_STREAM.ref_count, eax
    
    mov     rax, 1
    jmp     access_exit
    
access_fail:
    xor     rax, rax
    
access_exit:
    pop     rsi
    pop     rbp
    ret
rawr1024_model_access_update ENDP

;--------------------------------------------------------------------------
; Decrement Model Reference Count
;--------------------------------------------------------------------------
PUBLIC rawr1024_model_release
rawr1024_model_release PROC
    ; Input: RCX = model_id
    ; Output: RAX = remaining_ref_count
    
    push    rbp
    mov     rbp, rsp
    push    rsi
    
    lea     rsi, loaded_models
    xor     eax, eax
find_model_rel_loop:
    cmp     eax, MAX_LOADED_MODELS
    jge     release_fail
    
    mov     edx, DWORD PTR [rsi].MODEL_STREAM.model_id
    cmp     edx, ecx
    je      model_rel_found
    
    add     rsi, SIZEOF MODEL_STREAM
    inc     eax
    jmp     find_model_rel_loop
    
model_rel_found:
    ; Decrement ref_count
    mov     eax, [rsi + MODEL_STREAM.ref_count]
    cmp     eax, 0
    jle     release_fail
    dec     eax
    mov     [rsi + MODEL_STREAM.ref_count], eax
    
    jmp     release_exit
    
release_fail:
    mov     eax, 0xFFFFFFFF
    
release_exit:
    pop     rsi
    pop     rbp
    ret
rawr1024_model_release ENDP

;--------------------------------------------------------------------------
; Get File Size Stub
;--------------------------------------------------------------------------
get_file_size PROC
    ; Input: RCX = file_path
    ; Output: RDX = file_size (or 0 on failure)
    ; TODO: Implement actual file size query
    mov     rdx, 104857600      ; Placeholder: 100MB
    ret
get_file_size ENDP

END
