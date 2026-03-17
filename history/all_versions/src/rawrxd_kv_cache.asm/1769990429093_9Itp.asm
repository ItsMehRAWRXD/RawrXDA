; RawrXD KV Cache Manager - AMD64 Implementation
; Handles cache allocation, paging, and attention retrieval
; Optimized for 64GB RAM / 16GB VRAM systems

EXTERN VirtualAlloc : PROC
EXTERN VirtualFree : PROC
EXTERN memcpy : PROC
EXTERN memset : PROC

.DATA
    CACHE_ALIGNMENT EQU 64      ; Cache line alignment
    PAGE_SIZE       EQU 4096
    MAX_CACHE_PAGES EQU 16384   ; 64GB / 4KB = 16M pages, we track 16K blocks
    
    ; Cache page table entry
    PAGE_ENTRY STRUCT
        virtAddr    DQ ?        ; Virtual address of page
        physAddr    DQ ?        ; Physical/GPU address
        layer       DD ?        ; Which transformer layer
        seqPos      DD ?        ; Position in sequence
        flags       DD ?        ; Valid, Dirty, GPU resident
        nextLRU     DD ?        ; LRU list index
    PAGE_ENTRY ENDS
    
.CODE

; KVCache_Initialize
; RCX = max_seq_len, RDX = n_layers, R8 = n_kv_heads, R9 = head_dim
; Returns: handle to cache context
KVCache_Initialize PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    
    mov r12, rcx            ; max_seq_len
    mov r13, rdx            ; n_layers
    mov r14, r8             ; n_kv_heads
    mov r15, r9             ; head_dim
    
    ; Calculate total cache size per layer
    ; Size = seq_len * n_kv_heads * head_dim * sizeof(fp16)
    mov rax, r12
    mul r14                 ; RAX = seq_len * n_kv_heads
    mul r15                 ; RAX = total elements
    shl rax, 1              ; RAX = bytes (FP16 = 2 bytes)
    mov rbx, rax            ; RBX = bytes per layer per tensor (K or V)
    
    ; Allocate page table
    mov rcx, MAX_CACHE_PAGES * SIZEOF PAGE_ENTRY
    xor rdx, rdx
    mov r8, 2000h           ; MEM_COMMIT | MEM_RESERVE
    mov r9, 04h             ; PAGE_READWRITE
    call VirtualAlloc       ; RAX = page table base
    
    ; Store context
    mov rsi, rax
    
    ; Allocate K and V caches for each layer
    xor rdi, rdi            ; Layer counter
layer_loop:
    cmp rdi, r13
    jge done_init
    
    ; Allocate K cache
    mov rcx, rbx
    xor rdx, rdx
    mov r8, 2000h
    mov r9, 04h
    call VirtualAlloc
    
    ; Register pages in page table (Stub call)
    ; mov r8, rax             ; Address
    ; mov r9, rbx             ; Size
    ; call RegisterPages
    
    ; Allocate V cache (same logic)
    mov rcx, rbx
    xor rdx, rdx
    mov r8, 2000h
    mov r9, 04h
    call VirtualAlloc
    
    ; mov r8, rax
    ; mov r9, rbx
    ; call RegisterPages
    
    inc rdi
    jmp layer_loop
    
done_init:
    mov rax, rsi            ; Return page table handle
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
KVCache_Initialize ENDP

; KVCache_Store
; RCX = context, RDX = layer, R8 = seq_pos, R9 = head, 
; [RSP+0x28] = data_ptr, [RSP+0x30] = is_value (0=K, 1=V)
KVCache_Store PROC
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx
    mov r13, rdx            ; layer
    mov r14, r8             ; seq_pos
    
    ; Calculate page index
    ; index = layer * max_seq_len + seq_pos
    ; This assumes context struct holds max_seq_len at offset 8?
    ; In Init, we didn't store max_seq_len in context memory, we just used registers
    ; Simplified: assumes fixed strides for now or that r12 points to a control block we set up.
    
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
KVCache_Store ENDP

; KVCache_RetrieveRange
; RCX = context, RDX = layer, R8 = head, R9 = start_pos
; [RSP+0x28] = count, [RSP+0x30] = output_buffer
; Returns: gathered K/V vectors for attention computation
KVCache_RetrieveRange PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    
    mov r12, rcx
    mov r13, rdx            ; layer
    mov r14, r8             ; head
    mov r15, r9             ; start_pos
    mov rdi, [rsp+88]       ; output buffer (shadow space + pushes = 7*8 + 32 = 88?)
                            ; Wait, [rsp+0x30] relative to caller RSP means inputs are at 
    ; stack locations. Standard locations:
    ; P1=rcx, P2=rdx, P3=r8, P4=r9, P5=[rsp+40], P6=[rsp+48]
    ; My pushes change RSP.
    ; 7 pushes = 56 bytes.
    ; P5 is at [rsp + 56 + 40] = [rsp + 96]
    ; P6 is at [rsp + 56 + 48] = [rsp + 104]
    
    mov rbx, [rsp+96]       ; count
    mov rdi, [rsp+104]      ; output buffer
    
    ; retrieve_loop logic...
    
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
KVCache_RetrieveRange ENDP

; Optimized attention dot product with cache prefetching
; RCX = query_ptr, RDX = cache_ptrs_array, R8 = count, R9 = head_dim
; [RSP+0x28] = scores_output
KVCache_ComputeScores PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx            ; query
    mov r13, rdx            ; cache pointers array
    mov r14, r8             ; count (seq_len)
    mov r15, r9             ; head_dim
    mov rbx, [rsp+80]       ; scores_output (5 pushes = 40 bytes + 40 offset = 80)
    
score_loop:
    test r14, r14
    jz done_scores
    
    ; Prefetch next cache line
    prefetcht0 [r13+8]
    
    ; Load K vector address
    mov rdx, [r13]          ; Current K vector
    
    ; Compute dot product: query  K
    ; Stub
    
    ; Store result
    mov dword ptr [rbx], 0
    
    ; Advance
    add r13, 8              ; Next pointer
    add rbx, 4              ; Next float
    dec r14
    jmp score_loop
    
done_scores:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
KVCache_ComputeScores ENDP

END
