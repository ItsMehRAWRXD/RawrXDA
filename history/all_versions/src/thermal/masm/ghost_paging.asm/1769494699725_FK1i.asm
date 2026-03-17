; ============================================================================
; GHOST_PAGING.ASM — NVMe Streaming Prefetch Kernel
; Pure MASM64 | Zero-Dependencies | Supports up to 800B Q4
; Compile: ml64 /c /Fo ghost_paging.obj ghost_paging.asm
; ============================================================================
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
OPTION CASEMAP:NONE

; Removed windows.inc dependency for portability
; INCLUDELIB kernel32.lib
; INCLUDELIB ntdll.lib

; External Callbacks (Host Implementation)
; Conformed to x64 ABI (no @ decoration)
EXTERN LoadTensorBlock:PROC            ; void LoadTensorBlock(token_id, slab_id)
EXTERN DispatchComputeStage:PROC       ; void DispatchComputeStage(buffer_id)

.data
GHOST_PREFETCH_OFFSET    DQ 256 * 1024         ; 256KB ahead
NVME_STREAM_QUEUE_COUNT  DQ 16
STREAM_BUFFER_PER_QUEUE  DQ 32 * 1024 * 1024   ; 32MB each
MAX_GHOST_STREAM_BUFFER  DQ 512 * 1024 * 1024  ; 512MB total

; Token-to-slab bitmap (64K tokens = 8KB)
TOKEN_SLAB_BITMAP        DB 8192 DUP(0)

; Hotpin Cache Table (64-way, 256KB slabs = 4GB)
HOTPIN_CACHE             DD 64 DUP(0FFFFFFFFh) ; Invalid slab IDs

.code

; ============================================================================
; GhostPrefetchStart - Initializes the ghost prefetch kernel
; ============================================================================
GhostPrefetchStart PROC
    ; No-op for now (future: IoRing init, stream queues, etc.)
    ret
GhostPrefetchStart ENDP

; ============================================================================
; GhostDispatchToken - Called by GGUF runtime to stream a token
; RCX = token_id
; ============================================================================
GhostDispatchToken PROC
    push rbx
    push rsi
    push rdi

    ; Map token_id (RCX) -> slab_id
    mov eax, ecx                ; token_id
    shr eax, 8                  ; 256KB slabs
    mov ebx, eax                ; slab_id = token_id / 256

    ; Check if slab_id is in HOTPIN_CACHE
    mov rcx, 64
    xor rsi, rsi
@CheckCacheLoop:
    cmp HOTPIN_CACHE[rsi * 4], ebx
    je @CacheHit
    inc rsi
    loop @CheckCacheLoop

    ; MISS: Evict coldest and pin new slab
    mov rdi, 0                  ; Replace index 0 (LRU slot) - TODO: True LRU
    mov HOTPIN_CACHE[rdi * 4], ebx

    ; Issue Prefetch (side-band read)
    ; Call LoadTensorBlock(token_id, slab_id)
    ; RCX = token_id (passed in originally, need to recover or use EAX/EDX)
    
    ; We need token_id in RCX? No, standard x64 calling convention:
    ; RCX = arg1 (token_id)
    ; RDX = arg2 (slab_id)
    
    ; Recover token_id? We have it in EAX (shifted?) No, EAX is invalid now.
    ; Wait, we overwrote RCX in the loop. 
    ; Let's re-save token_id before loop.
    
    ; ERROR IN ORIGINAL LOGIC: RCX was clobbered by loop counter.
    ; FIXED implementation:

    pop rdi     ; Restore momentarily to fix stack if we restart? 
                ; No, let's rewrite the prolog/logic cleanly.
                
    ; RESTART FUNCTION LOGIC
    ; ----------------------
    pop rdi
    pop rsi
    pop rbx
    
    push rbx
    push rsi
    push rdi
    
    mov r10, rcx                ; Save token_id in R10 (volatile, safe inside function before calls)
    
    ; Map token_id -> slab_id
    mov eax, ecx
    shr eax, 8
    mov ebx, eax                ; ebx = slab_id

    ; Check HOTPIN_CACHE
    mov rcx, 64
    xor rsi, rsi
@CheckCacheLoop2:
    cmp HOTPIN_CACHE[rsi * 4], ebx
    je @CacheHit
    inc rsi
    loop @CheckCacheLoop2

    ; MISS
    mov rdi, 0
    mov HOTPIN_CACHE[rdi * 4], ebx

    ; Call LoadTensorBlock(token_id, slab_id)
    mov rcx, r10                ; arg1: token_id
    mov rdx, rbx                ; arg2: slab_id
    sub rsp, 28h                ; shadow space (32 bytes + alignment if needed)
                                ; Stack is 16-byte aligned? 
                                ; Pushes: 3*8 = 24. Return Address = 8. Total 32. Aligned.
                                ; We need to sub 32 (20h) + 8 (padding? no 32 is standard 4 params)
                                ; But stack must be 16 byte aligned *before* call.
                                ; Current: Ret(8) + 3*Push(24) = 32. (Aligned).
                                ; Sub 32 -> 64 (Aligned). Good.
    sub rsp, 20h
    call LoadTensorBlock
    add rsp, 20h
    
    jmp @Dispatch

@CacheHit:
    ; Hit path (Optional: Update LRU bits)

@Dispatch:
    ; Dispatch compute
    ; Call DispatchComputeStage(buffer_id)
    mov rcx, 0                  ; arg1: buffer_id
    sub rsp, 20h
    call DispatchComputeStage
    add rsp, 20h

    pop rdi
    pop rsi
    pop rbx
    ret
GhostDispatchToken ENDP

; ============================================================================
; GhostPrefetchTokenMap
; RCX = token_id
; ============================================================================
GhostPrefetchTokenMap PROC
    mov eax, ecx
    shr eax, 3
    mov rdx, rcx
    and dl, 7
    mov cl, 1
    shl cl, dl
    
    ; Fix: Use LEA for RIP-relative addressing of data
    lea r8, TOKEN_SLAB_BITMAP
    or byte ptr [r8 + rax], cl
    ret
GhostPrefetchTokenMap ENDP

; ============================================================================
; GhostQueryTokenMap
; RCX = token_id
; Returns AL = 0/1
; ============================================================================
GhostQueryTokenMap PROC
    mov eax, ecx
    shr eax, 3
    mov rdx, rcx
    and dl, 7
    
    ; Fix: Use LEA for RIP-relative addressing of data
    lea r8, TOKEN_SLAB_BITMAP
    mov cl, byte ptr [r8 + rax]
    
    shr cl, dl
    and al, 1
    ret
GhostQueryTokenMap ENDP

END
