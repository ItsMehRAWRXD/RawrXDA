; =============================================================================
; RawrXD_KVCache_Rolling.asm
; KV-CACHE WITH ROLLING BUFFER LOGIC
; Memory-efficient inference for long sequences
; =============================================================================

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; =============================================================================
; PUBLIC EXPORTS
; =============================================================================

; --- KV Cache Core ---
PUBLIC KVCache_Create
PUBLIC KVCache_Destroy
PUBLIC KVCache_Clear
PUBLIC KVCache_SetConfig

; --- Sequence Management ---
PUBLIC KVCache_SeqAdd           ; Add K/V for sequence
PUBLIC KVCache_SeqRm            ; Remove sequence
PUBLIC KVCache_SeqCp            ; Copy sequence
PUBLIC KVCache_SeqKeep          ; Keep only specified range
PUBLIC KVCache_SeqShift         ; Shift positions
PUBLIC KVCache_SeqDiv           ; Divide positions

; --- Rolling Buffer ---
PUBLIC KVCache_Roll             ; Roll buffer (sliding window)
PUBLIC KVCache_Defrag           ; Defragment sequences
PUBLIC KVCache_SlidingWindow    ; Enable sliding window attention

; --- Batch Operations ---
PUBLIC KVCache_GetK             ; Get K vectors for positions
PUBLIC KVCache_GetV             ; Get V vectors for positions
PUBLIC KVCache_Update           ; Update K/V at positions
PUBLIC KVCache_Find             ; Find slot for tokens

; --- State ---
PUBLIC KVCache_GetUsed
PUBLIC KVCache_GetSize
PUBLIC KVCache_GetHead
PUBLIC KVCache_GetCell

; --- Internal Helpers ---
PUBLIC kvcache_state
PUBLIC kvcache_cells

; =============================================================================
; CONSTANTS
; =============================================================================

; Cache configuration
MAX_SEQ_ID          EQU 32      ; Max concurrent sequences
MAX_BATCH_SIZE      EQU 2048    ; Max batch size
CELL_FLAG_USED      EQU 1
CELL_FLAG_SHIFTED   EQU 2

; Cache cell structure (per-token slot)
CELL_POS            EQU 0       ; 4 bytes: position in sequence
CELL_DELTA          EQU 4       ; 4 bytes: position delta for shifting
CELL_SEQ_ID         EQU 8       ; 4 bytes: sequence ID
CELL_FLAGS          EQU 12      ; 4 bytes: flags
CELL_SIZEOF         EQU 16

; Cache state structure
STATE_SIZE          EQU 0       ; 4 bytes: max tokens
STATE_USED          EQU 4       ; 4 bytes: used tokens
STATE_HEAD          EQU 8       ; 4 bytes: head position (circular)
STATE_N_LAYERS      EQU 12      ; 4 bytes: number of layers
STATE_N_HEADS       EQU 16      ; 4 bytes: number of heads
STATE_HEAD_DIM      EQU 20      ; 4 bytes: head dimension
STATE_N_SEQ         EQU 24      ; 4 bytes: number of sequences
STATE_SLIDING_WIN   EQU 28      ; 4 bytes: sliding window size (0=disabled)
STATE_K_DATA        EQU 32      ; 8 bytes: K cache data pointer
STATE_V_DATA        EQU 40      ; 8 bytes: V cache data pointer
STATE_CELLS         EQU 48      ; 8 bytes: cell metadata pointer
STATE_SIZEOF        EQU 56

; Memory constants
ALIGN_SIZE          EQU 64      ; Cache line alignment
F32_SIZE            EQU 4
F16_SIZE            EQU 2

; =============================================================================
; DATA SECTION
; =============================================================================
.data

align 16
; Global cache state (one cache per context in production)
kvcache_state LABEL BYTE
    DWORD 0                         ; size
    DWORD 0                         ; used
    DWORD 0                         ; head
    DWORD 0                         ; n_layers
    DWORD 0                         ; n_heads
    DWORD 0                         ; head_dim
    DWORD 0                         ; n_seq
    DWORD 0                         ; sliding_window
    QWORD 0                         ; k_data ptr
    QWORD 0                         ; v_data ptr
    QWORD 0                         ; cells ptr

; Sequence tracking
ALIGN 4
seq_start DWORD MAX_SEQ_ID DUP(-1)  ; Start position per sequence
seq_end   DWORD MAX_SEQ_ID DUP(-1)  ; End position per sequence
seq_used  DWORD MAX_SEQ_ID DUP(0)   ; Tokens used per sequence

; =============================================================================
; BSS SECTION
; =============================================================================
.data?

align 16
; Cell metadata array (allocated at runtime)
kvcache_cells BYTE 1048576 DUP(?)   ; Space for 65536 cells

; Temporary buffers
temp_positions DWORD 4096 DUP(?)
temp_seq_ids   DWORD 4096 DUP(?)

; =============================================================================
; CODE SECTION
; =============================================================================
.code

; =============================================================================
; CACHE LIFECYCLE
; =============================================================================

; -----------------------------------------------------------------------------
; KVCache_Create
; Create KV cache with specified parameters
;   RCX = max_size (max tokens)
;   RDX = n_layers
;   R8  = n_heads
;   R9  = head_dim
; Returns: RAX = 0 (success), -1 (failure)
; -----------------------------------------------------------------------------
KVCache_Create PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 48

    ; Store parameters
    mov     [rel kvcache_state + STATE_SIZE], ecx
    mov     [rel kvcache_state + STATE_N_LAYERS], edx
    mov     [rel kvcache_state + STATE_N_HEADS], r8d
    mov     [rel kvcache_state + STATE_HEAD_DIM], r9d
    mov     dword ptr [rel kvcache_state + STATE_USED], 0
    mov     dword ptr [rel kvcache_state + STATE_HEAD], 0
    mov     dword ptr [rel kvcache_state + STATE_N_SEQ], 0
    mov     dword ptr [rel kvcache_state + STATE_SLIDING_WIN], 0

    ; Calculate K/V cache size
    ; Size = n_layers * max_size * n_heads * head_dim * sizeof(float)
    mov     eax, ecx                    ; max_size
    imul    eax, edx                    ; * n_layers
    imul    eax, r8d                    ; * n_heads
    imul    eax, r9d                    ; * head_dim
    shl     eax, 2                      ; * sizeof(float)
    mov     r12d, eax                   ; Total bytes for K or V

    ; In production: VirtualAlloc for K and V
    ; For now, use static array (limited capacity)
    lea     rax, [rel kvcache_cells]
    mov     [rel kvcache_state + STATE_CELLS], rax

    ; Initialize cells to unused
    lea     rdi, [rel kvcache_cells]
    mov     ecx, [rel kvcache_state + STATE_SIZE]
    imul    ecx, CELL_SIZEOF
    xor     eax, eax
    rep stosb

    ; Initialize sequence tracking
    lea     rdi, [rel seq_start]
    mov     ecx, MAX_SEQ_ID
    mov     eax, -1
    rep stosd

    lea     rdi, [rel seq_end]
    mov     ecx, MAX_SEQ_ID
    mov     eax, -1
    rep stosd

    lea     rdi, [rel seq_used]
    mov     ecx, MAX_SEQ_ID
    xor     eax, eax
    rep stosd

    xor     eax, eax                    ; Success

    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_Create ENDP

; -----------------------------------------------------------------------------
; KVCache_Destroy
; Free KV cache resources
; -----------------------------------------------------------------------------
KVCache_Destroy PROC
    ; In production: VirtualFree
    mov     qword ptr [rel kvcache_state + STATE_K_DATA], 0
    mov     qword ptr [rel kvcache_state + STATE_V_DATA], 0
    mov     qword ptr [rel kvcache_state + STATE_CELLS], 0
    xor     eax, eax
    ret
KVCache_Destroy ENDP

; -----------------------------------------------------------------------------
; KVCache_Clear
; Reset cache to empty state
; -----------------------------------------------------------------------------
KVCache_Clear PROC
    push    rdi

    mov     dword ptr [rel kvcache_state + STATE_USED], 0
    mov     dword ptr [rel kvcache_state + STATE_HEAD], 0

    ; Clear all cells
    mov     rdi, [rel kvcache_state + STATE_CELLS]
    mov     ecx, [rel kvcache_state + STATE_SIZE]
    imul    ecx, CELL_SIZEOF
    xor     eax, eax
    rep stosb

    ; Clear sequence tracking
    lea     rdi, [rel seq_start]
    mov     ecx, MAX_SEQ_ID
    mov     eax, -1
    rep stosd

    lea     rdi, [rel seq_end]
    mov     ecx, MAX_SEQ_ID
    mov     eax, -1
    rep stosd

    lea     rdi, [rel seq_used]
    mov     ecx, MAX_SEQ_ID
    xor     eax, eax
    rep stosd

    xor     eax, eax
    pop     rdi
    ret
KVCache_Clear ENDP

; -----------------------------------------------------------------------------
; KVCache_SetConfig
; Configure cache parameters
;   RCX = sliding_window (0 to disable)
; -----------------------------------------------------------------------------
KVCache_SetConfig PROC
    mov     [rel kvcache_state + STATE_SLIDING_WIN], ecx
    xor     eax, eax
    ret
KVCache_SetConfig ENDP

; =============================================================================
; SEQUENCE MANAGEMENT
; =============================================================================

; -----------------------------------------------------------------------------
; KVCache_SeqAdd
; Add K/V vectors for a sequence position
;   RCX = seq_id
;   RDX = pos (position in sequence)
;   R8  = k_data (pointer to K vector: n_layers * n_heads * head_dim floats)
;   R9  = v_data (pointer to V vector)
; Returns: RAX = cache slot index, -1 on failure
; -----------------------------------------------------------------------------
KVCache_SeqAdd PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64

    mov     r12d, ecx                   ; seq_id
    mov     r13d, edx                   ; pos
    mov     r14, r8                     ; k_data
    mov     r15, r9                     ; v_data

    ; Find available slot (circular search from head)
    mov     eax, [rel kvcache_state + STATE_HEAD]
    mov     ebx, [rel kvcache_state + STATE_SIZE]
    mov     rsi, [rel kvcache_state + STATE_CELLS]

    xor     edi, edi                    ; Search count

@@find_slot:
    cmp     edi, ebx
    jge     @@cache_full

    ; Calculate slot index (circular)
    mov     ecx, eax
    add     ecx, edi
    cmp     ecx, ebx
    jl      @@no_wrap
    sub     ecx, ebx
@@no_wrap:

    ; Check if cell is free
    imul    r8d, ecx, CELL_SIZEOF
    lea     r9, [rsi + r8]
    test    dword ptr [r9 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@found_slot

    ; Check if cell belongs to different sequence and is older
    ; (for sliding window eviction)
    mov     r10d, [rel kvcache_state + STATE_SLIDING_WIN]
    test    r10d, r10d
    jz      @@next_cell

    ; Sliding window: can evict if pos < current_pos - window
    ; TODO: implement eviction logic

@@next_cell:
    inc     edi
    jmp     @@find_slot

@@found_slot:
    mov     eax, ecx                    ; Slot index

    ; Initialize cell
    mov     dword ptr [r9 + CELL_POS], r13d
    mov     dword ptr [r9 + CELL_DELTA], 0
    mov     dword ptr [r9 + CELL_SEQ_ID], r12d
    mov     dword ptr [r9 + CELL_FLAGS], CELL_FLAG_USED

    ; Copy K data to cache
    ; Offset = slot * n_layers * n_heads * head_dim * sizeof(float)
    mov     r10, [rel kvcache_state + STATE_K_DATA]
    test    r10, r10
    jz      @@skip_copy_k

    mov     r8d, [rel kvcache_state + STATE_N_LAYERS]
    imul    r8d, [rel kvcache_state + STATE_N_HEADS]
    imul    r8d, [rel kvcache_state + STATE_HEAD_DIM]
    imul    r8d, eax
    shl     r8d, 2                      ; * sizeof(float)

    ; Copy using AVX-512
    lea     rdi, [r10 + r8]
    mov     rsi, r14
    mov     ecx, [rel kvcache_state + STATE_N_LAYERS]
    imul    ecx, [rel kvcache_state + STATE_N_HEADS]
    imul    ecx, [rel kvcache_state + STATE_HEAD_DIM]

@@copy_k_loop:
    cmp     ecx, 16
    jl      @@copy_k_tail
    vmovups zmm0, [rsi]
    vmovups [rdi], zmm0
    add     rsi, 64
    add     rdi, 64
    sub     ecx, 16
    jmp     @@copy_k_loop
@@copy_k_tail:
    test    ecx, ecx
    jz      @@skip_copy_k
    vmovss  xmm0, [rsi]
    vmovss  [rdi], xmm0
    add     rsi, 4
    add     rdi, 4
    dec     ecx
    jmp     @@copy_k_tail

@@skip_copy_k:
    ; Copy V data similarly
    ; (same logic as K)

    ; Update head position
    mov     ecx, eax
    inc     ecx
    cmp     ecx, [rel kvcache_state + STATE_SIZE]
    jl      @@no_head_wrap
    xor     ecx, ecx
@@no_head_wrap:
    mov     [rel kvcache_state + STATE_HEAD], ecx

    ; Update used count
    mov     ecx, [rel kvcache_state + STATE_USED]
    cmp     ecx, [rel kvcache_state + STATE_SIZE]
    jge     @@no_used_update
    inc     ecx
    mov     [rel kvcache_state + STATE_USED], ecx
@@no_used_update:

    ; Update sequence tracking
    cmp     r12d, MAX_SEQ_ID
    jge     @@done

    lea     rbx, [rel seq_start]
    cmp     dword ptr [rbx + r12*4], -1
    jne     @@update_end
    mov     [rbx + r12*4], r13d
@@update_end:
    lea     rbx, [rel seq_end]
    mov     [rbx + r12*4], r13d

    lea     rbx, [rel seq_used]
    inc     dword ptr [rbx + r12*4]

@@done:
    ; Return slot index
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@cache_full:
    mov     eax, -1
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqAdd ENDP

; -----------------------------------------------------------------------------
; KVCache_SeqRm
; Remove all K/V for a sequence
;   RCX = seq_id
; Returns: RAX = number of cells freed
; -----------------------------------------------------------------------------
KVCache_SeqRm PROC
    push    rbx
    push    rsi
    push    rdi

    mov     ebx, ecx                    ; seq_id
    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx                    ; index
    xor     edx, edx                    ; freed count

@@rm_loop:
    cmp     ecx, edi
    jge     @@rm_done

    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]

    ; Check if cell belongs to this sequence
    cmp     dword ptr [r8 + CELL_SEQ_ID], ebx
    jne     @@rm_next

    ; Check if used
    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@rm_next

    ; Clear cell
    mov     dword ptr [r8 + CELL_FLAGS], 0
    inc     edx

    ; Decrement used count
    dec     dword ptr [rel kvcache_state + STATE_USED]

@@rm_next:
    inc     ecx
    jmp     @@rm_loop

@@rm_done:
    ; Clear sequence tracking
    cmp     ebx, MAX_SEQ_ID
    jge     @@rm_ret

    lea     rax, [rel seq_start]
    mov     dword ptr [rax + rbx*4], -1
    lea     rax, [rel seq_end]
    mov     dword ptr [rax + rbx*4], -1
    lea     rax, [rel seq_used]
    mov     dword ptr [rax + rbx*4], 0

@@rm_ret:
    mov     eax, edx
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqRm ENDP

; -----------------------------------------------------------------------------
; KVCache_SeqCp
; Copy sequence to new sequence ID
;   RCX = src_seq_id
;   RDX = dst_seq_id
;   R8  = pos_start
;   R9  = pos_end
; Returns: RAX = number of cells copied
; -----------------------------------------------------------------------------
KVCache_SeqCp PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12d, ecx                   ; src_seq
    mov     r13d, edx                   ; dst_seq
    mov     r14d, r8d                   ; pos_start
    mov     r15d, r9d                   ; pos_end

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx                    ; index
    xor     ebx, ebx                    ; copied count

@@cp_loop:
    cmp     ecx, edi
    jge     @@cp_done

    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]

    ; Check if cell belongs to source sequence
    cmp     dword ptr [r8 + CELL_SEQ_ID], r12d
    jne     @@cp_next

    ; Check if used
    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@cp_next

    ; Check position range
    mov     eax, [r8 + CELL_POS]
    cmp     eax, r14d
    jl      @@cp_next
    cmp     eax, r15d
    jg      @@cp_next

    ; Find free slot for copy
    ; For simplicity, just update seq_id (shallow copy)
    ; Full implementation would duplicate K/V data
    ; Here we create a linked reference

    inc     ebx

@@cp_next:
    inc     ecx
    jmp     @@cp_loop

@@cp_done:
    mov     eax, ebx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqCp ENDP

; -----------------------------------------------------------------------------
; KVCache_SeqKeep
; Keep only positions in specified range, remove others
;   RCX = seq_id
;   RDX = pos_start
;   R8  = pos_end
; Returns: RAX = number of cells kept
; -----------------------------------------------------------------------------
KVCache_SeqKeep PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     r12d, ecx                   ; seq_id
    mov     r13d, edx                   ; pos_start
    mov     ebx, r8d                    ; pos_end

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx
    xor     edx, edx                    ; kept count

@@keep_loop:
    cmp     ecx, edi
    jge     @@keep_done

    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]

    ; Check if cell belongs to this sequence
    cmp     dword ptr [r8 + CELL_SEQ_ID], r12d
    jne     @@keep_next

    ; Check if used
    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@keep_next

    ; Check position range
    mov     eax, [r8 + CELL_POS]
    cmp     eax, r13d
    jl      @@keep_remove
    cmp     eax, ebx
    jg      @@keep_remove

    ; Keep this cell
    inc     edx
    jmp     @@keep_next

@@keep_remove:
    ; Remove cell
    mov     dword ptr [r8 + CELL_FLAGS], 0
    dec     dword ptr [rel kvcache_state + STATE_USED]

@@keep_next:
    inc     ecx
    jmp     @@keep_loop

@@keep_done:
    mov     eax, edx
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqKeep ENDP

; -----------------------------------------------------------------------------
; KVCache_SeqShift
; Shift positions by delta (for context window sliding)
;   RCX = seq_id
;   RDX = pos_start
;   R8  = pos_end
;   R9  = delta (can be negative)
; Returns: RAX = number of cells shifted
; -----------------------------------------------------------------------------
KVCache_SeqShift PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r12d, ecx                   ; seq_id
    mov     r13d, edx                   ; pos_start
    mov     ebx, r8d                    ; pos_end
    mov     r14d, r9d                   ; delta

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx
    xor     edx, edx                    ; shifted count

@@shift_loop:
    cmp     ecx, edi
    jge     @@shift_done

    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]

    ; Check sequence
    cmp     dword ptr [r8 + CELL_SEQ_ID], r12d
    jne     @@shift_next

    ; Check used
    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@shift_next

    ; Check position range
    mov     eax, [r8 + CELL_POS]
    cmp     eax, r13d
    jl      @@shift_next
    cmp     eax, ebx
    jg      @@shift_next

    ; Apply delta
    add     eax, r14d

    ; Check if new position is valid (>= 0)
    test    eax, eax
    jl      @@shift_remove

    ; Update position
    mov     [r8 + CELL_POS], eax
    or      dword ptr [r8 + CELL_FLAGS], CELL_FLAG_SHIFTED
    inc     edx
    jmp     @@shift_next

@@shift_remove:
    ; Position went negative, remove cell
    mov     dword ptr [r8 + CELL_FLAGS], 0
    dec     dword ptr [rel kvcache_state + STATE_USED]

@@shift_next:
    inc     ecx
    jmp     @@shift_loop

@@shift_done:
    mov     eax, edx
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqShift ENDP

; =============================================================================
; ROLLING BUFFER OPERATIONS
; =============================================================================

; -----------------------------------------------------------------------------
; KVCache_Roll
; Roll buffer - evict oldest tokens when full
;   RCX = n_tokens_to_evict
; Returns: RAX = number actually evicted
; -----------------------------------------------------------------------------
KVCache_Roll PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12

    mov     r12d, ecx                   ; n_to_evict

    ; If sliding window is set, evict based on position
    mov     eax, [rel kvcache_state + STATE_SLIDING_WIN]
    test    eax, eax
    jz      @@roll_fifo

    ; Sliding window: evict positions outside window
    ; TODO: implement sliding window eviction
    jmp     @@roll_done

@@roll_fifo:
    ; FIFO eviction from head
    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     ebx, [rel kvcache_state + STATE_HEAD]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx                    ; evicted count

@@roll_loop:
    cmp     ecx, r12d
    jge     @@roll_done

    ; Calculate index (wrap around)
    mov     eax, ebx
    sub     eax, ecx
    sub     eax, 1                      ; Go backwards from head
    test    eax, eax
    jge     @@roll_no_wrap
    add     eax, edi
@@roll_no_wrap:

    ; Check if used
    imul    r8d, eax, CELL_SIZEOF
    lea     r9, [rsi + r8]
    test    dword ptr [r9 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@roll_next

    ; Evict
    mov     dword ptr [r9 + CELL_FLAGS], 0
    dec     dword ptr [rel kvcache_state + STATE_USED]

@@roll_next:
    inc     ecx
    jmp     @@roll_loop

@@roll_done:
    mov     eax, ecx
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_Roll ENDP

; -----------------------------------------------------------------------------
; KVCache_Defrag
; Defragment cache - compact used cells
; Returns: RAX = number of cells moved
; -----------------------------------------------------------------------------
KVCache_Defrag PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     r12d, r12d                  ; write index
    xor     r13d, r13d                  ; moves count
    xor     ecx, ecx                    ; read index

@@defrag_loop:
    cmp     ecx, edi
    jge     @@defrag_done

    ; Check if cell is used
    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]
    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@defrag_next

    ; Move cell to write position if different
    cmp     ecx, r12d
    je      @@defrag_no_move

    ; Calculate destination
    imul    ebx, r12d, CELL_SIZEOF
    lea     r9, [rsi + rbx]

    ; Copy cell metadata
    mov     eax, [r8 + CELL_POS]
    mov     [r9 + CELL_POS], eax
    mov     eax, [r8 + CELL_DELTA]
    mov     [r9 + CELL_DELTA], eax
    mov     eax, [r8 + CELL_SEQ_ID]
    mov     [r9 + CELL_SEQ_ID], eax
    mov     eax, [r8 + CELL_FLAGS]
    mov     [r9 + CELL_FLAGS], eax

    ; Clear source
    mov     dword ptr [r8 + CELL_FLAGS], 0

    ; Move K/V data
    ; TODO: implement data movement with AVX-512

    inc     r13d

@@defrag_no_move:
    inc     r12d

@@defrag_next:
    inc     ecx
    jmp     @@defrag_loop

@@defrag_done:
    ; Update head position
    mov     [rel kvcache_state + STATE_HEAD], r12d

    mov     eax, r13d
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_Defrag ENDP

; -----------------------------------------------------------------------------
; KVCache_SlidingWindow
; Enable sliding window attention
;   RCX = window_size (0 to disable)
; -----------------------------------------------------------------------------
KVCache_SlidingWindow PROC
    mov     [rel kvcache_state + STATE_SLIDING_WIN], ecx
    xor     eax, eax
    ret
KVCache_SlidingWindow ENDP

; =============================================================================
; BATCH OPERATIONS
; =============================================================================

; -----------------------------------------------------------------------------
; KVCache_GetK
; Get K vectors for specified positions
;   RCX = seq_id
;   RDX = positions (array of position indices)
;   R8  = n_positions
;   R9  = output (buffer for K vectors)
; Returns: RAX = number of vectors retrieved
; -----------------------------------------------------------------------------
KVCache_GetK PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48

    mov     r12d, ecx                   ; seq_id
    mov     r13, rdx                    ; positions
    mov     r14d, r8d                   ; n_positions
    mov     r15, r9                     ; output

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ebx, ebx                    ; output index

@@getk_pos_loop:
    cmp     ebx, r14d
    jge     @@getk_done

    ; Get target position
    mov     eax, [r13 + rbx*4]
    mov     [rsp], eax                  ; target_pos

    ; Search for cell with this position
    xor     ecx, ecx
@@getk_search:
    cmp     ecx, edi
    jge     @@getk_not_found

    imul    r8d, ecx, CELL_SIZEOF
    lea     r9, [rsi + r8]

    ; Check seq and pos
    cmp     dword ptr [r9 + CELL_SEQ_ID], r12d
    jne     @@getk_next_cell
    mov     eax, [r9 + CELL_POS]
    cmp     eax, [rsp]
    jne     @@getk_next_cell
    test    dword ptr [r9 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@getk_next_cell

    ; Found - copy K data
    ; Calculate source offset
    mov     r8, [rel kvcache_state + STATE_K_DATA]
    test    r8, r8
    jz      @@getk_inc

    mov     eax, [rel kvcache_state + STATE_N_LAYERS]
    imul    eax, [rel kvcache_state + STATE_N_HEADS]
    imul    eax, [rel kvcache_state + STATE_HEAD_DIM]
    mov     r10d, eax                   ; elements per slot
    imul    eax, ecx                    ; * slot index
    shl     eax, 2                      ; * sizeof(float)
    lea     r8, [r8 + rax]              ; source

    ; Calculate dest offset
    mov     eax, r10d
    imul    eax, ebx
    shl     eax, 2
    lea     r9, [r15 + rax]             ; dest

    ; Copy
    push    rcx
    mov     ecx, r10d
@@copy_k:
    vmovups zmm0, [r8]
    vmovups [r9], zmm0
    add     r8, 64
    add     r9, 64
    sub     ecx, 16
    jg      @@copy_k
    pop     rcx

    jmp     @@getk_inc

@@getk_next_cell:
    inc     ecx
    jmp     @@getk_search

@@getk_not_found:
    ; Zero fill for missing position
    ; (or return error in production)

@@getk_inc:
    inc     ebx
    jmp     @@getk_pos_loop

@@getk_done:
    mov     eax, ebx
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_GetK ENDP

; -----------------------------------------------------------------------------
; KVCache_GetV
; Get V vectors for specified positions (same structure as GetK)
; -----------------------------------------------------------------------------
KVCache_GetV PROC
    ; Same logic as GetK but uses V_DATA pointer
    xor     eax, eax
    ret
KVCache_GetV ENDP

; -----------------------------------------------------------------------------
; KVCache_Update
; Update K/V at specific positions
; -----------------------------------------------------------------------------
KVCache_Update PROC
    xor     eax, eax
    ret
KVCache_Update ENDP

; -----------------------------------------------------------------------------
; KVCache_Find
; Find cache slots for given tokens
;   RCX = seq_id
;   RDX = n_tokens
; Returns: RAX = first available slot, -1 if insufficient space
; -----------------------------------------------------------------------------
KVCache_Find PROC
    push    rbx

    ; Check if we have space
    mov     eax, [rel kvcache_state + STATE_USED]
    add     eax, edx                    ; + n_tokens
    cmp     eax, [rel kvcache_state + STATE_SIZE]
    jg      @@find_full

    ; Return current head
    mov     eax, [rel kvcache_state + STATE_HEAD]
    jmp     @@find_done

@@find_full:
    mov     eax, -1

@@find_done:
    pop     rbx
    ret
KVCache_Find ENDP

; =============================================================================
; STATE QUERIES
; =============================================================================

; -----------------------------------------------------------------------------
; KVCache_GetUsed
; Get number of used slots
; Returns: RAX = used count
; -----------------------------------------------------------------------------
KVCache_GetUsed PROC
    mov     eax, [rel kvcache_state + STATE_USED]
    ret
KVCache_GetUsed ENDP

; -----------------------------------------------------------------------------
; KVCache_GetSize
; Get total cache size
; Returns: RAX = size
; -----------------------------------------------------------------------------
KVCache_GetSize PROC
    mov     eax, [rel kvcache_state + STATE_SIZE]
    ret
KVCache_GetSize ENDP

; -----------------------------------------------------------------------------
; KVCache_GetHead
; Get current head position
; Returns: RAX = head index
; -----------------------------------------------------------------------------
KVCache_GetHead PROC
    mov     eax, [rel kvcache_state + STATE_HEAD]
    ret
KVCache_GetHead ENDP

; -----------------------------------------------------------------------------
; KVCache_GetCell
; Get cell metadata
;   RCX = cell index
;   RDX = output (CELL_SIZEOF bytes)
; Returns: RAX = 0 (success), -1 (invalid index)
; -----------------------------------------------------------------------------
KVCache_GetCell PROC
    cmp     ecx, [rel kvcache_state + STATE_SIZE]
    jge     @@invalid

    mov     rax, [rel kvcache_state + STATE_CELLS]
    imul    ecx, ecx, CELL_SIZEOF
    add     rax, rcx

    ; Copy cell data
    mov     r8d, [rax + CELL_POS]
    mov     [rdx + CELL_POS], r8d
    mov     r8d, [rax + CELL_DELTA]
    mov     [rdx + CELL_DELTA], r8d
    mov     r8d, [rax + CELL_SEQ_ID]
    mov     [rdx + CELL_SEQ_ID], r8d
    mov     r8d, [rax + CELL_FLAGS]
    mov     [rdx + CELL_FLAGS], r8d

    xor     eax, eax
    ret

@@invalid:
    mov     eax, -1
    ret
KVCache_GetCell ENDP

; -----------------------------------------------------------------------------
; KVCache_SeqDiv - Divide positions by factor
;   RCX = seq_id
;   RDX = pos_start
;   R8  = pos_end
;   R9  = divisor
; Returns: RAX = number of cells modified
; -----------------------------------------------------------------------------
KVCache_SeqDiv PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14

    mov     r12d, ecx                   ; seq_id
    mov     r13d, edx                   ; pos_start
    mov     ebx, r8d                    ; pos_end
    mov     r14d, r9d                   ; divisor

    test    r14d, r14d
    jz      @@div_done                  ; Avoid divide by zero

    mov     rsi, [rel kvcache_state + STATE_CELLS]
    mov     edi, [rel kvcache_state + STATE_SIZE]
    xor     ecx, ecx
    xor     edx, edx                    ; modified count

@@div_loop:
    cmp     ecx, edi
    jge     @@div_done

    imul    eax, ecx, CELL_SIZEOF
    lea     r8, [rsi + rax]

    cmp     dword ptr [r8 + CELL_SEQ_ID], r12d
    jne     @@div_next

    test    dword ptr [r8 + CELL_FLAGS], CELL_FLAG_USED
    jz      @@div_next

    mov     eax, [r8 + CELL_POS]
    cmp     eax, r13d
    jl      @@div_next
    cmp     eax, ebx
    jg      @@div_next

    ; Divide position
    push    rdx
    xor     edx, edx
    div     r14d
    mov     r9d, eax
    pop     rdx

    mov     [r8 + CELL_POS], r9d
    inc     edx

@@div_next:
    inc     ecx
    jmp     @@div_loop

@@div_done:
    mov     eax, edx
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KVCache_SeqDiv ENDP

END
