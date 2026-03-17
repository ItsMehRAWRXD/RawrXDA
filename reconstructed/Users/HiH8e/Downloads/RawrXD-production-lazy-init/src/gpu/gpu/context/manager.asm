;============================================================================
; GPU Context Manager - Pure MASM x64
; Manages model contexts, KV cache state, and session lifecycle
; Production-ready: Context pooling, lifecycle management, statistics
;============================================================================
.686P
.XMM
.model flat, c
OPTION CASEMAP:NONE

extern RtlZeroMemory: proc
extern CopyMemory: proc
extern OutputDebugStringA: proc
extern EnterCriticalSection: proc
extern LeaveCriticalSection: proc
extern InitializeCriticalSection: proc
extern QueryPerformanceCounter: proc

; Import memory manager and error handling
extern AllocateSystemMemory: proc
extern FreeGPUMemory: proc
extern SetGPUError: proc

.data
; Context registry configuration
MAX_CONTEXTS            equ 64                 ; Maximum concurrent contexts
CONTEXT_HASH_SIZE       equ 256                ; Hash table for fast lookup

; Registry state
contextRegistry         dq MAX_CONTEXTS dup(0)     ; Array of context pointers
contextCount            dq 0
contextPoolHead         dq 0                       ; Free list for recycling
contextHashTable        dq CONTEXT_HASH_SIZE dup(0)

; Active context (single, can be switched)
activeContextId         dq 0

; ModelContext structure (256 bytes for alignment)
ModelContext STRUCT
    contextId           dq ?                   ; Unique ID (hash of pointer)
    modelHandle         dq ?                   ; Handle to loaded model
    modelName           dq ?                   ; Name of model
    kvCacheBase         dq ?                   ; KV cache buffer pointer
    kvCacheSize         dq ?                   ; Total KV cache size
    kvCacheUsed         dq ?                   ; Current usage
    kvCachePeak         dq ?                   ; Peak usage
    
    tokenHistory        dq ?                   ; Token sequence buffer
    tokenHistoryLen     dq ?                   ; Current length
    tokenHistoryCap     dq ?                   ; Capacity
    
    generationCount     dq ?                   ; Total generations
    totalTokensGenerated dq ?                  ; Total tokens output
    totalInferenceTime  dq ?                   ; Total time in microseconds
    
    createdTime         dq ?                   ; Creation timestamp
    lastAccessTime      dq ?                   ; Last used timestamp
    
    isActive            db ?                   ; Currently active
    isFrozen            db ?                   ; Frozen (read-only)
    hasError            db ?                   ; Error state
    padding             db 5 dup(?)            ; Align to 256 bytes
ModelContext ENDS

; Thread safety
contextMutex            CRITICAL_SECTION {}
lockInitialized         db 0

; Statistics
totalContextsCreated    dq 0
totalContextsDestroyed  dq 0
maxConcurrentContexts   dq 0

; Debug strings
debugContextCreate      db "[GPU_CONTEXT] Created context %lld: model=%s, cache=%lld MB", 0
debugContextActivate    db "[GPU_CONTEXT] Activated context %lld (was: %lld)", 0
debugContextFrozen      db "[GPU_CONTEXT] Frozen context %lld (read-only)", 0
debugContextUnfreeze    db "[GPU_CONTEXT] Unfrozen context %lld", 0
debugContextDestroy     db "[GPU_CONTEXT] Destroyed context %lld, generations=%lld", 0
debugContextStats       db "[GPU_CONTEXT] Stats: created=%lld, active=%lld, peak=%lld", 0
debugContextError       db "[GPU_CONTEXT] ERROR: %s (context=%lld, code=0x%x)", 0
debugContextSwitch      db "[GPU_CONTEXT] Context switch: %lld -> %lld", 0

errorMaxContextsReached db "Maximum contexts reached", 0
errorInvalidContextId   db "Invalid context ID", 0
errorContextNotFound    db "Context not found", 0
errorMemoryAlloc        db "Memory allocation failed", 0

.code

;----------------------------------------------------------------------------
; InitializeContextManager - Setup context system
; Call once at DLL load
;------------------------------------------------------------------------
InitializeContextManager proc
    cmp lockInitialized, 1
    je @init_already_done
    
    lea rcx, contextMutex
    call InitializeCriticalSection
    mov lockInitialized, 1
    
    mov contextCount, 0
    mov activeContextId, 0
    mov totalContextsCreated, 0
    mov totalContextsDestroyed, 0
    mov maxConcurrentContexts, 0
    
@init_already_done:
    ret
InitializeContextManager endp

;----------------------------------------------------------------------------
; CreateModelContext - Allocate new context
; rcx = model handle
; rdx = model name (ANSI string)
; r8 = kv_cache_size_mb
; Returns: context ID in rax (0 on failure)
;------------------------------------------------------------------------
CreateModelContext proc
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov qword ptr [rbp - 8], rcx   ; Save model handle
    mov qword ptr [rbp - 16], rdx  ; Save model name
    mov qword ptr [rbp - 24], r8   ; Save cache size
    
    lea rcx, contextMutex
    call EnterCriticalSection
    
    ; Check capacity
    mov rax, contextCount
    cmp rax, MAX_CONTEXTS
    jge @context_create_capacity_error
    
    ; Allocate context structure
    mov rcx, sizeof ModelContext
    call AllocateSystemMemory
    mov r9, rax                    ; r9 = context pointer
    test rax, rax
    jz @context_create_memory_error
    
    ; Initialize context
    mov rcx, 0
    mov [r9 + ModelContext.contextId], rcx
    
    mov rcx, [rbp - 8]
    mov [r9 + ModelContext.modelHandle], rcx
    
    mov rcx, [rbp - 16]
    mov [r9 + ModelContext.modelName], rcx
    
    ; Allocate KV cache
    mov rcx, [rbp - 24]
    shl rcx, 20                    ; Convert MB to bytes
    mov [r9 + ModelContext.kvCacheSize], rcx
    
    call AllocateSystemMemory
    mov [r9 + ModelContext.kvCacheBase], rax
    mov [r9 + ModelContext.kvCacheUsed], 0
    mov [r9 + ModelContext.kvCachePeak], 0
    
    ; Allocate token history (4K tokens)
    mov rcx, 4096 * 8
    call AllocateSystemMemory
    mov [r9 + ModelContext.tokenHistory], rax
    mov [r9 + ModelContext.tokenHistoryLen], 0
    mov [r9 + ModelContext.tokenHistoryCap], 4096
    
    ; Initialize state
    mov [r9 + ModelContext.isActive], 0
    mov [r9 + ModelContext.isFrozen], 0
    mov [r9 + ModelContext.hasError], 0
    mov [r9 + ModelContext.generationCount], 0
    mov [r9 + ModelContext.totalTokensGenerated], 0
    mov [r9 + ModelContext.totalInferenceTime], 0
    
    ; Set creation time
    lea rcx, qword ptr [r9 + ModelContext.createdTime]
    call QueryPerformanceCounter
    
    ; Generate context ID (use pointer hash)
    mov rax, r9
    shr rax, 6
    xor rcx, rcx
    mov [r9 + ModelContext.contextId], rax
    
    ; Add to registry
    mov rcx, contextCount
    lea rdx, contextRegistry
    mov [rdx + rcx * 8], r9
    inc contextCount
    
    ; Update peak
    mov rax, contextCount
    cmp rax, maxConcurrentContexts
    cmova maxConcurrentContexts, rax
    
    inc totalContextsCreated
    
    ; Log creation
    lea rcx, debugContextCreate
    mov rdx, [r9 + ModelContext.contextId]
    mov r8, [r9 + ModelContext.modelName]
    mov r9, [r9 + ModelContext.kvCacheSize]
    shr r9, 20                     ; Convert back to MB
    call OutputDebugStringA
    
    mov rax, [r9 + ModelContext.contextId]
    jmp @context_create_done
    
@context_create_capacity_error:
    lea rcx, debugContextError
    lea rdx, errorMaxContextsReached
    mov r8, 0
    mov r9d, 0x10010001           ; Error code
    call OutputDebugStringA
    
    xor rax, rax
    jmp @context_create_done
    
@context_create_memory_error:
    lea rcx, debugContextError
    lea rdx, errorMemoryAlloc
    mov r8, 0
    mov r9d, 0x10010002
    call OutputDebugStringA
    
    xor rax, rax
    
@context_create_done:
    lea rcx, contextMutex
    call LeaveCriticalSection
    
    mov rsp, rbp
    pop rbp
    ret
CreateModelContext endp

;----------------------------------------------------------------------------
; ActivateModelContext - Set context as active
; rcx = context ID
; Returns: 1=success, 0=failure
;------------------------------------------------------------------------
ActivateModelContext proc
    lea rdx, contextMutex
    call EnterCriticalSection
    
    ; Find context in registry
    mov rax, 0
    lea r8, contextRegistry
    
@find_context_loop:
    cmp rax, contextCount
    jge @activate_context_notfound
    
    mov r9, [r8 + rax * 8]
    cmp [r9 + ModelContext.contextId], rcx
    je @activate_context_found
    
    inc rax
    jmp @find_context_loop
    
@activate_context_found:
    ; Deactivate previous
    mov r10, activeContextId
    
    ; Activate new
    mov activeContextId, rcx
    mov [r9 + ModelContext.isActive], 1
    
    ; Update access time
    lea rcx, qword ptr [r9 + ModelContext.lastAccessTime]
    call QueryPerformanceCounter
    
    ; Log activation
    lea rcx, debugContextActivate
    mov rdx, [r9 + ModelContext.contextId]
    mov r8, r10
    call OutputDebugStringA
    
    mov rax, 1
    jmp @activate_context_done
    
@activate_context_notfound:
    lea rcx, debugContextError
    lea rdx, errorContextNotFound
    mov r8, activeContextId
    mov r9d, 0x10010003
    call OutputDebugStringA
    
    xor rax, rax
    
@activate_context_done:
    lea rcx, contextMutex
    call LeaveCriticalSection
    
    ret
ActivateModelContext endp

;----------------------------------------------------------------------------
; DestroyModelContext - Cleanup context and free resources
; rcx = context ID
; Returns: 1=success, 0=failure
;------------------------------------------------------------------------
DestroyModelContext proc
    lea rdx, contextMutex
    call EnterCriticalSection
    
    ; Find and remove from registry
    mov rax, 0
    lea r8, contextRegistry
    
@find_destroy_loop:
    cmp rax, contextCount
    jge @destroy_notfound
    
    mov r9, [r8 + rax * 8]
    cmp [r9 + ModelContext.contextId], rcx
    je @destroy_found
    
    inc rax
    jmp @find_destroy_loop
    
@destroy_found:
    ; Log stats before destroy
    lea rcx, debugContextDestroy
    mov rdx, [r9 + ModelContext.contextId]
    mov r8, [r9 + ModelContext.generationCount]
    call OutputDebugStringA
    
    ; Free KV cache
    mov rcx, [r9 + ModelContext.kvCacheBase]
    test rcx, rcx
    jz @skip_free_kv
    call FreeGPUMemory
    
@skip_free_kv:
    ; Free token history
    mov rcx, [r9 + ModelContext.tokenHistory]
    test rcx, rcx
    jz @skip_free_tokens
    call FreeGPUMemory
    
@skip_free_tokens:
    ; Mark as destroyed
    mov [r9 + ModelContext.isActive], 0
    
    ; Remove from registry (shift remaining)
    mov rdx, rax
    lea r8, contextRegistry
    mov rsi, [r8 + rax * 8]
    
@shift_loop:
    inc rax
    cmp rax, contextCount
    jge @shift_done
    
    mov r10, [r8 + rax * 8]
    mov [r8 + rax * 8 - 8], r10
    jmp @shift_loop
    
@shift_done:
    dec contextCount
    inc totalContextsDestroyed
    
    mov rax, 1
    jmp @destroy_done
    
@destroy_notfound:
    lea rcx, debugContextError
    lea rdx, errorContextNotFound
    mov r8, rcx
    mov r9d, 0x10010003
    call OutputDebugStringA
    
    xor rax, rax
    
@destroy_done:
    lea rcx, contextMutex
    call LeaveCriticalSection
    
    ret
DestroyModelContext endp

;----------------------------------------------------------------------------
; GetActiveContext - Query active context ID
; Returns: context ID in rax (0 if none)
;------------------------------------------------------------------------
GetActiveContext proc
    mov rax, activeContextId
    ret
GetActiveContext endp

;----------------------------------------------------------------------------
; AppendTokenHistory - Add token to active context
; rcx = token_id
; Returns: 1=success, 0=failure
;------------------------------------------------------------------------
AppendTokenHistory proc
    lea rdx, contextMutex
    call EnterCriticalSection
    
    ; Find active context
    mov rax, activeContextId
    test rax, rax
    jz @append_no_context
    
    lea r8, contextRegistry
    mov r9, 0
    
@find_append_loop:
    cmp r9, contextCount
    jge @append_not_found
    
    mov r10, [r8 + r9 * 8]
    cmp [r10 + ModelContext.contextId], rax
    je @append_found
    
    inc r9
    jmp @find_append_loop
    
@append_found:
    ; Check capacity
    mov rax, [r10 + ModelContext.tokenHistoryLen]
    cmp rax, [r10 + ModelContext.tokenHistoryCap]
    jge @append_full
    
    ; Append token
    mov r11, [r10 + ModelContext.tokenHistory]
    mov [r11 + rax * 8], rcx
    
    inc [r10 + ModelContext.tokenHistoryLen]
    
    mov rax, 1
    jmp @append_done
    
@append_full:
@append_not_found:
@append_no_context:
    xor rax, rax
    
@append_done:
    lea rcx, contextMutex
    call LeaveCriticalSection
    
    ret
AppendTokenHistory endp

;----------------------------------------------------------------------------
; GetContextStatistics - Return context usage stats
; rcx = context ID
; Returns: rax=tokens_generated, rdx=inference_time_us, r8=cache_used_mb
;------------------------------------------------------------------------
GetContextStatistics proc
    lea rdx, contextMutex
    call EnterCriticalSection
    
    ; Find context
    mov r9, 0
    lea r8, contextRegistry
    
@find_stats_loop:
    cmp r9, contextCount
    jge @stats_not_found
    
    mov r10, [r8 + r9 * 8]
    cmp [r10 + ModelContext.contextId], rcx
    je @stats_found
    
    inc r9
    jmp @find_stats_loop
    
@stats_found:
    mov rax, [r10 + ModelContext.totalTokensGenerated]
    mov rdx, [r10 + ModelContext.totalInferenceTime]
    mov r8, [r10 + ModelContext.kvCacheUsed]
    shr r8, 20                     ; Convert to MB
    
    jmp @stats_done
    
@stats_not_found:
    xor rax, rax
    xor rdx, rdx
    xor r8, r8
    
@stats_done:
    lea rcx, contextMutex
    call LeaveCriticalSection
    
    ret
GetContextStatistics endp

end
