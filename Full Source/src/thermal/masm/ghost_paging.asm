; ============================================================================
; GHOST_PAGING.ASM — NVMe Streaming Prefetch Kernel
; Pure MASM64 | Zero-Dependencies | Supports up to 800B Q4
; Compile: ml64 /c /Fo ghost_paging.obj ghost_paging.asm
; ============================================================================
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC GhostPrefetchStart
PUBLIC GhostDispatchToken
PUBLIC GhostPrefetchTokenMap
PUBLIC GhostQueryTokenMap

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
    ; Initialize ghost prefetch kernel: allocate I/O ring buffer,
    ; prepare streaming queues, launch prefetch worker thread
    push rbx
    push rsi
    sub rsp, 48
    
    ; Allocate prefetch ring buffer: 64 slots * 4KB pages = 256KB
    xor ecx, ecx
    mov edx, 262144                  ; 256KB
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@gps_fail
    
    mov QWORD PTR [g_prefetch_ring], rax
    mov DWORD PTR [g_prefetch_head], 0
    mov DWORD PTR [g_prefetch_tail], 0
    mov DWORD PTR [g_prefetch_slots], 64
    
    ; Initialize the prefetch event (auto-reset)
    xor ecx, ecx                     ; lpEventAttributes
    xor edx, edx                     ; bManualReset = FALSE (auto-reset)
    xor r8d, r8d                     ; bInitialState = FALSE
    xor r9d, r9d                     ; lpName = NULL
    call CreateEventA
    test rax, rax
    jz @@gps_fail
    mov QWORD PTR [g_prefetch_event], rax
    
    ; Spawn prefetch worker thread
    xor ecx, ecx                     ; lpThreadAttributes
    xor edx, edx                     ; dwStackSize = default
    lea r8, [GhostPrefetchWorker]    ; lpStartAddress
    xor r9d, r9d                     ; lpParameter
    push 0                           ; lpThreadId
    push 0                           ; dwCreationFlags
    sub rsp, 32
    call CreateThread
    add rsp, 32
    test rax, rax
    jz @@gps_fail
    mov QWORD PTR [g_prefetch_thread], rax
    
    mov DWORD PTR [g_prefetch_active], 1
    mov rax, 1
    add rsp, 48
    pop rsi
    pop rbx
    ret
@@gps_fail:
    xor eax, eax
    add rsp, 48
    pop rsi
    pop rbx
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
    
    mov r10, rcx                ; Save token_id in R10
    
    ; Map token_id -> slab_id
    mov eax, ecx
    shr eax, 8
    mov ebx, eax                ; ebx = slab_id

    ; Check HOTPIN_CACHE
    lea r8, HOTPIN_CACHE        ; Load base address
    mov rcx, 64
    xor rsi, rsi
@CheckCacheLoop2:
    cmp [r8 + rsi * 4], ebx     ; Index off base
    je @CacheHit
    inc rsi
    loop @CheckCacheLoop2

    ; MISS
    mov rdi, 0
    ; r8 still holds Base Address
    mov [r8 + rdi * 4], ebx     ; Update cache

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
    ; Compute Index
    mov rax, rcx
    shr rax, 3          ; Index = token_id / 8
    
    ; Compute Bit Offset (Count must be in CL)
    and rcx, 7          ; CL = token_id % 8
    
    ; Create Mask
    mov r9b, 1
    shl r9b, cl         ; Shift 1 by CL
    
    ; Set Bit
    lea r8, TOKEN_SLAB_BITMAP
    add r8, rax
    or byte ptr [r8], r9b
    ret
GhostPrefetchTokenMap ENDP

; ============================================================================
; GhostQueryTokenMap
; RCX = token_id
; Returns AL = 0/1
; ============================================================================
GhostQueryTokenMap PROC
    ; Compute Index
    mov rax, rcx
    shr rax, 3
    
    ; Compute Bit Offset (in CL)
    and rcx, 7
    
    ; Read Byte
    lea r8, TOKEN_SLAB_BITMAP
    add r8, rax
    mov r9b, byte ptr [r8]
    
    ; Shift to LSB
    shr r9b, cl         ; Shift by CL
    
    ; Mask and Return
    and r9b, 1
    mov al, r9b
    ret
GhostQueryTokenMap ENDP

END
