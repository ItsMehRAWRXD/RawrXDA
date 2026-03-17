; RawrXD Pure MASM Core Engine - Ultra-Optimized
; Targets: Completion Cache, Tokenization, Compression, Model Interface
; Goal: <15k LOC total with maximum performance
; Compiled: Dec 28, 2025

.code

; ============================================================
; UNIFIED STRING HASHING - Foundation for all caching
; ============================================================
; Input: rcx = string pointer, rdx = string length
; Output: rax = 64-bit hash
; Clobbers: r8, r9
public rawr_string_hash
rawr_string_hash PROC
    xor rax, rax                ; hash accumulator
    xor r8, r8                  ; counter
    cmp rdx, 0
    jz hash_done
    
hash_loop:
    cmp r8, rdx
    jge hash_done
    
    movzx r9d, byte ptr [rcx + r8]
    rol rax, 13                 ; rotate by 13 bits
    xor rax, r9                 ; mix in byte
    imul rax, 0x9e3779b97f4a7c15 ; FNV-1a prime (64-bit)
    inc r8
    jmp hash_loop
    
hash_done:
    ret
rawr_string_hash ENDP

; ============================================================
; FAST COMPLETION CACHE - HashMap with collision chaining
; ============================================================
; Structure: [hash(8) | prefix_len(4) | results_ptr(8) | next_ptr(8) | prefix_data... | results_data...]
; Cache size: 4MB (65536 slots * 64 bytes/slot)

public rawr_cache_lookup
rawr_cache_lookup PROC
    ; rcx = prefix string, rdx = prefix_len, r8 = cache base, r9 = cache size
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    ; Hash the prefix
    call rawr_string_hash        ; returns hash in rax
    
    ; Bucket index = (hash % cache_size)
    xor rdx, rdx
    mov r10, r9                  ; cache size
    div r10                       ; rax = quotient (index), rdx = remainder
    mov r10, r8                  ; r10 = cache base
    
    ; Get bucket: bucket_ptr = cache_base + (index * SLOT_SIZE)
    mov r11, 64                  ; SLOT_SIZE = 64
    imul rax, r11
    add r10, rax                 ; r10 = current bucket
    
cache_chain_loop:
    mov rax, [r10]              ; load next bucket
    test rax, rax               ; NULL check
    jz cache_miss
    
    ; Check if this matches (would need to compare prefix)
    ; For now, return found (simplified - real impl would verify)
    mov rax, [r10 + 16]         ; load results_ptr
    pop r12
    pop rbx
    pop rbp
    ret
    
cache_miss:
    xor rax, rax                ; return NULL
    pop r12
    pop rbx
    pop rbp
    ret
rawr_cache_lookup ENDP

; ============================================================
; BPE TOKENIZER - Fast Token Encoding (Byte-Pair Encoding)
; ============================================================
; Input: rcx = text, rdx = text_len, r8 = vocab_base, r9 = token_output
; Output: rax = token count
public rawr_bpe_encode
rawr_bpe_encode PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    xor r12, r12                ; token count
    xor r13, r13                ; current text position
    
encode_loop:
    cmp r13, rdx                ; reached end?
    jge encode_done
    
    ; Try longest token match starting at r13
    mov r10, r13                ; r10 = start position
    xor r11, r11                ; r11 = best match length (0 = no match)
    xor rbx, rbx                ; rbx = best token id
    
match_loop:
    cmp r10, rdx                ; end of text?
    jge use_best_match
    
    ; Hash substring [r13:r10]
    mov rcx, [r8 + r13]         ; get vocab entry
    mov rax, [rcx]              ; get token hash
    ; Compare with current substring hash (simplified)
    cmp rax, 0                  ; if valid
    je try_next
    
    ; Found match - update best
    mov rbx, rcx                ; token id
    mov r11, r10                ; length
    
try_next:
    inc r10
    jmp match_loop
    
use_best_match:
    test r11, r11               ; found any match?
    jnz token_found
    
    ; No match - use byte token
    movzx rax, byte ptr [r13]
    mov [r9 + r12 * 8], rax
    inc r13
    inc r12
    jmp encode_loop
    
token_found:
    ; Add token to output
    mov [r9 + r12 * 8], rbx
    add r13, r11
    inc r12
    jmp encode_loop
    
encode_done:
    mov rax, r12                ; return token count
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
rawr_bpe_encode ENDP

; ============================================================
; DEFLATE COMPRESSION - Brutal Fast (MASM native)
; ============================================================
; Input: rcx = input, rdx = input_len, r8 = output, r9 = output_len
; Output: rax = compressed_len
public rawr_deflate_compress
rawr_deflate_compress PROC
    push rbp
    mov rbp, rsp
    push rbx
    
    ; DEFLATE header (fixed Huffman)
    mov byte ptr [r8], 0x78     ; CMF: deflate, default compression
    mov byte ptr [r8 + 1], 0x9C ; FLG
    
    xor r10, r10                ; output position = 2 (after header)
    xor r11, r11                ; input position = 0
    
    ; Process in blocks (65535 bytes max per block)
deflate_block_loop:
    cmp r11, rdx                ; all input processed?
    jge deflate_finalize
    
    ; Calculate block size (min of remaining input and max block size)
    mov rax, rdx
    sub rax, r11
    mov r12, 65535
    cmp rax, r12
    jle deflate_block_size_set
    mov rax, r12
    
deflate_block_size_set:
    mov r12, rax                ; r12 = current block size
    
    ; Write block header (simplified - uncompressed)
    mov byte ptr [r8 + r10], 0  ; BFINAL=0, BTYPE=00 (uncompressed)
    inc r10
    
    ; Write block size (little-endian 16-bit)
    mov ax, r12w
    mov [r8 + r10], ax
    add r10, 2
    
    ; Write one's complement of size
    mov ax, 0xFFFF
    sub ax, r12w
    mov [r8 + r10], ax
    add r10, 2
    
    ; Copy uncompressed block
    mov rcx, r11                ; source offset
    mov r13, r12                ; size to copy
copy_block:
    test r13, r13
    jz deflate_block_done
    
    mov al, byte ptr [rcx + r13]
    mov byte ptr [r8 + r10], al
    inc r10
    dec r13
    jmp copy_block
    
deflate_block_done:
    add r11, r12
    jmp deflate_block_loop
    
deflate_finalize:
    ; Final block marker + checksum (simplified)
    mov byte ptr [r8 + r10], 0x01  ; BFINAL=1, BTYPE=00
    add r10, 5                       ; space for size + checksum
    
    mov rax, r10                ; return compressed length
    pop rbx
    pop rbp
    ret
rawr_deflate_compress ENDP

; ============================================================
; MODEL INFERENCE DISPATCHER - Routes to GGUF or Ollama
; ============================================================
; Input: rcx = tokens (array), rdx = token_count, r8 = model_type
; Output: rax = next_token_id, rdx = confidence
public rawr_infer_next_token
rawr_infer_next_token PROC
    ; Check model type
    cmp r8, 0                   ; type 0 = local GGUF
    je infer_gguf
    
    cmp r8, 1                   ; type 1 = remote Ollama
    je infer_ollama
    
    xor rax, rax                ; error: unknown model
    ret
    
infer_gguf:
    ; For now, return placeholder
    ; Real impl would call GGUF inference (could be C++ bridge)
    mov rax, 1234               ; dummy token
    mov rdx, 85                 ; confidence %
    ret
    
infer_ollama:
    ; For now, return placeholder
    mov rax, 5678
    mov rdx, 92
    ret
    
rawr_infer_next_token ENDP

; ============================================================
; PERFORMANCE METRICS - Ultra-fast metric collection
; ============================================================
; Input: rcx = metric_id, rdx = value
; No output (metrics aggregated in-place)
public rawr_metric_record
rawr_metric_record PROC
    ; Metrics storage: rdx-allocated region (typically RBX for context)
    ; Simple implementation: increment counter at offset (metric_id * 16)
    mov r8, [rel metrics_base]  ; get base (RIP-relative)
    cmp r8, 0
    je metric_skip
    
    mov rax, rcx
    imul rax, 16
    add r8, rax
    
    ; Increment counter at [r8]
    lock inc qword ptr [r8]
    
    ; Add to sum at [r8 + 8]
    lock add qword ptr [r8 + 8], rdx
    
metric_skip:
    ret
rawr_metric_record ENDP

; ============================================================
; SYNCHRONIZATION - Lock-free operations
; ============================================================
public rawr_atomic_increment
rawr_atomic_increment PROC
    ; rcx = pointer to 64-bit value
    xor rax, rax
    mov rdx, 1
    lock xadd [rcx], rdx        ; atomic increment
    mov rax, rdx                ; return old value
    ret
rawr_atomic_increment ENDP

public rawr_atomic_compare_swap
rawr_atomic_compare_swap PROC
    ; rcx = pointer, rdx = expected, r8 = new_value
    ; Output: rax = success (1) or failure (0)
    mov rax, rdx                ; rax = expected
    lock cmpxchg [rcx], r8      ; atomic CAS
    sete al                     ; al = (success ? 1 : 0)
    movzx rax, al
    ret
rawr_atomic_compare_swap ENDP

; ============================================================
; VECTOR/SIMD OPERATIONS - AVX2 optimizations
; ============================================================
public rawr_vec_dot_product_avx2
rawr_vec_dot_product_avx2 PROC
    ; rcx = vec1, rdx = vec2, r8 = count (floats)
    ; Output: xmm0 = dot product
    vxorps ymm0, ymm0, ymm0     ; accumulator
    xor r9, r9                  ; counter
    
    ; Process 8 floats at a time (32 bytes)
dot_loop:
    cmp r9, r8
    jge dot_done
    
    vmovups ymm1, [rcx + r9 * 4]
    vmovups ymm2, [rdx + r9 * 4]
    vmulps ymm1, ymm1, ymm2     ; element-wise multiply
    vaddps ymm0, ymm0, ymm1     ; accumulate
    
    add r9, 8
    jmp dot_loop
    
dot_done:
    ; Horizontal add to get final scalar
    vhaddps ymm0, ymm0, ymm0
    vhaddps ymm0, ymm0, ymm0
    ret
rawr_vec_dot_product_avx2 ENDP

; ============================================================
; ERROR CODES AND CONSTANTS
; ============================================================
.data

metrics_base dq 0  ; Will be set at runtime

ERR_SUCCESS equ 0
ERR_CACHE_MISS equ 1
ERR_OUT_OF_MEMORY equ 2
ERR_INVALID_MODEL equ 3
ERR_TIMEOUT equ 4

.code

end
