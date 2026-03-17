# Real DEFLATE MASM Implementation Guide

## Current vs Proposed Architecture

### Current deflate_brutal_masm.asm (Stored Blocks Only)
```
Input Data (100MB)
        ↓
    Gzip Header (10 bytes)
        ↓
    Store in 65KB blocks
        ↓
    Block Header (5 bytes)
        ↓
    Rep movsb (raw memcpy)
        ↓
    Output: 100MB + overhead
    Compression Ratio: 0% (1.0x) ❌
```

### Proposed deflate_brutal_masm_v2.asm (Real DEFLATE)
```
Input Data (100MB with patterns)
        ↓
    Gzip Header (10 bytes)
        ↓
    LZ77 Sliding Window
    • Hash table of 64K entries
    • Find best match (3-258 bytes)
    • Encode as (dist, len)
        ↓
    Huffman Tree Construction
    • Analyze literal/length frequencies
    • Analyze distance frequencies
    • Build canonical Huffman codes
        ↓
    Deflate Blocks (with Huffman)
    • Encode literals/lengths/distances
    • Block format: RFC 1951
        ↓
    Gzip Footer
    • CRC32 + ISIZE
        ↓
    Output: 25-40MB (depending on data)
    Compression Ratio: 60-75% (2.5-4x) ✅
```

---

## Detailed MASM DEFLATE Implementation

### Component 1: Hash Table Initialization

```asm
; Hash table for LZ77 matching (65536 entries)
; Each entry: [head_idx, prev_idx, match_len, match_dist]

; Hash function: Hash = ((src[i] << 5) ^ src[i+1] ^ (src[i+2] << 8)) & 0xFFFF
hash_table LABEL QWORD
    REPEAT 65536
        dq 0  ; Store head position for each hash
    ENDM

; Circular buffer for previous matches
prev_buffer LABEL QWORD
    REPEAT 32768  ; Max matches to track
        dw 0      ; Previous match index
    ENDM
```

### Component 2: LZ77 Matching Core

```asm
; Input: rsi = src, rbx = len, r14 = max_dist (32KB window)
; Output: Fills match table with (distance, length) pairs

lz77_match_find PROC
    mov     r8, 0           ; cur_pos
    mov     r9, 0           ; match_count

_match_loop:
    cmp     r8, rbx
    jae     _match_done
    
    ; Calculate hash for position r8
    lea     rax, [rsi + r8]
    movzx   ecx, byte ptr [rax]
    shl     ecx, 5
    movzx   edx, byte ptr [rax + 1]
    xor     ecx, edx
    movzx   edx, byte ptr [rax + 2]
    shl     edx, 8
    xor     ecx, edx
    and     ecx, 0xFFFF     ; hash in ecx
    
    ; Get current head for this hash
    lea     rax, [hash_table + rcx*8]
    mov     r10, [rax]      ; head_idx in r10
    
    ; Find best match by checking previous entries
    mov     r11, 0          ; best_len
    mov     r12, 0          ; best_dist
    mov     r13, 0          ; check_count
    
_check_match_loop:
    cmp     r10, 0
    je      _no_more_matches
    cmp     r13, 256        ; Max checks per position
    jae     _no_more_matches
    cmp     r8, r10         ; cur_pos > prev_pos?
    jbe     _no_more_matches
    
    mov     rax, r8
    sub     rax, r10        ; distance
    cmp     rax, r14        ; > max_window?
    ja      _no_more_matches
    
    ; Compare at rsi+r10 with rsi+r8 (up to 258 bytes)
    mov     rcx, 258
    cmp     rcx, rbx
    jbe     $+4
    mov     rcx, rbx        ; Clamp to remaining data
    sub     rcx, r8
    
    ; Fast compare: check first 4 bytes
    mov     r15d, [rsi + r10]
    mov     eax, [rsi + r8]
    cmp     r15d, eax
    jne     _next_match
    
    ; Count full match length (SIMD-like)
    xor     edx, edx        ; match_len
_count_match:
    cmp     edx, rcx
    jae     _match_complete
    mov     al, byte ptr [rsi + r10 + rdx]
    cmp     al, byte ptr [rsi + r8 + rdx]
    jne     _match_complete
    inc     edx
    jmp     _count_match
    
_match_complete:
    cmp     edx, r11        ; edx > best_len?
    jbe     _next_match
    mov     r11, rdx        ; Update best_len
    mov     r12, rax        ; Update best_dist (rax = distance from earlier)
    
_next_match:
    ; Follow chain to previous match
    lea     rax, [prev_buffer + r10*2]
    movzx   r10, word ptr [rax]
    inc     r13
    jmp     _check_match_loop
    
_no_more_matches:
    ; Store match result if len >= 3
    cmp     r11, 3
    jb      _no_match_store
    
    ; Store in match table: [pos, dist, len]
    mov     rax, r9
    imul    rax, 16         ; 16 bytes per entry
    lea     r15, [match_table]
    mov     qword ptr [r15 + rax], r8        ; pos
    mov     dword ptr [r15 + rax + 8], r12d ; dist
    mov     dword ptr [r15 + rax + 12], r11d ; len
    inc     r9
    
_no_match_store:
    ; Update hash table for this position
    lea     rax, [hash_table + rcx*8]
    mov     rdx, r10
    mov     [rax], r8       ; Update head
    
    mov     rax, r9
    cmp     rax, 32768      ; Limit matches
    jae     _match_done
    
    inc     r8
    jmp     _match_loop
    
_match_done:
    ret
lz77_match_find ENDP
```

### Component 3: Huffman Tree Construction

```asm
; Input: Literal/length frequency table (256 literals + 29 lengths)
;        Distance frequency table (32 distances)
; Output: Canonical Huffman codes

huffman_build_tree PROC
    ; Step 1: Sort frequencies by count (Counting sort on bit length)
    mov     r8, 0           ; num_codes
    
    ; Create code length array (simple: prefix matching)
    mov     rcx, 288        ; 256 literals + 32 lengths
_freq_loop:
    lea     rax, [literal_freq + rcx*4]
    mov     edx, [rax]      ; frequency
    test    edx, edx
    je      _skip_freq
    
    ; Calculate bit length: ceil(log2(max_freq / freq))
    ; Simplified: use 8-bit length for now
    mov     dword ptr [code_length + rcx*4], 8
    inc     r8
    
_skip_freq:
    loop    _freq_loop
    
    ; Step 2: Generate canonical codes (sequential assignment)
    xor     r9, 0           ; code = 0
    xor     r10, 0          ; last_len = 0
    
    ; Sort by code length first (simplified: all 8-bit)
    mov     rcx, 0
_code_gen_loop:
    cmp     rcx, 288
    jae     _code_gen_done
    
    mov     eax, dword ptr [code_length + rcx*4]
    cmp     eax, r10
    je      _same_length
    
    ; Length changed: shift code
    mov     edx, eax
    sub     edx, r10d
    shl     r9, cl          ; << (new_len - old_len)
    mov     r10, rax
    
_same_length:
    mov     dword ptr [huffman_code + rcx*4], r9d
    inc     r9
    inc     rcx
    jmp     _code_gen_loop
    
_code_gen_done:
    ret
huffman_build_tree ENDP
```

### Component 4: Deflate Block Encoding

```asm
; Encode uncompressed data with Huffman codes into deflate blocks

deflate_encode_block PROC
    ; Input: r8 = match_count, r9 = match_table, rbx = src_len
    
    mov     rdi, 0          ; output_pos
    mov     r14, 0          ; input_pos
    
_encode_loop:
    cmp     r14, rbx
    jae     _encode_done
    
    ; Check if this position has a match
    mov     r10, 0          ; match_idx
    xor     r11, 0          ; found_match = false
    
_find_match_at_pos:
    cmp     r10, r8
    jae     _no_match_found
    
    mov     rax, r10
    imul    rax, 16
    mov     rcx, [match_table + rax]
    cmp     rcx, r14
    je      _match_found_here
    
    inc     r10
    jmp     _find_match_at_pos
    
_match_found_here:
    ; Encode (distance, length) using Huffman
    mov     edx, dword ptr [match_table + rax + 8]  ; dist
    mov     ecx, dword ptr [match_table + rax + 12] ; len
    
    ; Convert (dist, len) to (dist_code, len_code) per RFC 1951
    ; This requires table lookups - simplified here
    
    ; Encode length using Huffman
    mov     eax, ecx
    cmp     eax, 256
    jb      _len_short
    mov     eax, 256 + ((eax - 256) / 8)  ; Extra bits encoded separately
_len_short:
    mov     eax, dword ptr [huffman_code + rax*4]  ; Huffman code
    ; Output eax with appropriate bit width
    
    ; Encode distance using Huffman  
    mov     eax, edx
    cmp     eax, 4
    jb      _dist_short
    mov     eax, 2 + ((eax - 4) / 8)
_dist_short:
    mov     eax, dword ptr [huffman_code + rax*4]
    ; Output eax
    
    ; Skip over matched bytes
    add     r14, rcx
    mov     r11, 1
    
_no_match_found:
    ; Encode literal byte using Huffman
    test    r11, r11
    jnz     _skip_literal
    
    movzx   eax, byte ptr [rsi + r14]
    mov     eax, dword ptr [huffman_code + rax*4]
    ; Output eax
    
    inc     r14
    
_skip_literal:
    jmp     _encode_loop
    
_encode_done:
    ; Add end-of-block marker (code 256)
    mov     eax, dword ptr [huffman_code + 256*4]
    ; Output eax
    
    ret
deflate_encode_block ENDP
```

### Component 5: Bit-Level Output Stream

```asm
; Helper: Output codes as variable-length bit stream

bits_put PROC
    ; Input: eax = code, ecx = num_bits
    ; Output to rdi, tracking bit_pos in r15
    
    mov     edx, eax
    shl     edx, cl         ; Align to MSB
    or      r15d, edx       ; Accumulate bits
    add     r15, rcx        ; Track position
    
    ; Output bytes when we have >= 8 bits
    cmp     r15, 8
    jb      _bits_put_done
    
    mov     al, r15b
    mov     byte ptr [rdi], al
    inc     rdi
    shr     r15, 8          ; Remove output bits
    
_bits_put_done:
    ret
bits_put ENDP
```

---

## Integration Points

### 1. Activation Compression Hook

```cpp
// After each transformer layer
void compressActivations(const Tensor& x, CompressedTensor& out) {
    // Call MASM deflate on activation values
    uint8_t* compressed = deflate_brutal_masm_v2(
        reinterpret_cast<const uint8_t*>(x.data()),
        x.size_bytes(),
        &out.compressed_size
    );
    out.data = std::move(compressed);
}

// Before next layer
void decompressActivations(const CompressedTensor& in, Tensor& x) {
    uint8_t* decompressed = inflate_brutal_masm(
        in.data,
        in.compressed_size,
        &x.size_bytes()
    );
    x.setData(decompressed);
}
```

### 2. Tier Hopping Integration

```cpp
// In agentic_copilot_bridge.cpp hotpatchToModelTier():

// Pre-compress all tier weights
void precompressTiers(const Model& model) {
    for (int tier : {TIER_70B, TIER_21B, TIER_6B, TIER_2B}) {
        auto weights = model.getTierWeights(tier);
        uint64_t compressed_size;
        uint8_t* compressed = deflate_brutal_masm_v2(
            weights.data(),
            weights.size_bytes(),
            &compressed_size
        );
        tier_compressed_cache[tier] = std::move(compressed);
        tier_compressed_sizes[tier] = compressed_size;
    }
}

// During tier hop
bool hotpatchToModelTier(ModelTier target) {
    // Decompress target tier
    uint64_t decompressed_size;
    uint8_t* decompressed = inflate_brutal_masm(
        tier_compressed_cache[target],
        tier_compressed_sizes[target],
        &decompressed_size
    );
    
    // Swap in memory (very fast now!)
    current_model = decompressed;
    current_tier = target;
    
    return true;
}
```

### 3. KV Cache Compression

```cpp
// After each inference step
void compressKVCache() {
    // Compress key/value tensors
    for (int layer = 0; layer < num_layers; layer++) {
        auto& kv = current_kv_cache[layer];
        
        // Quantize to int8 first
        std::vector<int8_t> quantized = quantize_int8(kv);
        
        // Compress quantized data
        uint64_t compressed_size;
        uint8_t* compressed = deflate_brutal_masm_v2(
            reinterpret_cast<uint8_t*>(quantized.data()),
            quantized.size(),
            &compressed_size
        );
        
        kv_compressed[layer] = std::move(compressed);
        kv_compressed_sizes[layer] = compressed_size;
    }
}
```

---

## Benchmarking & Validation

### Test Cases

```cpp
// test_deflate_v2.cpp

TEST(DeflateV2, CompressRandom) {
    std::vector<uint8_t> random(1024*1024);
    // Random data compresses poorly
    auto [compressed, size] = deflate_brutal_masm_v2(random.data(), random.size());
    EXPECT_GT(size, random.size() * 0.95);  // Should expand
    EXPECT_LE(size, random.size() * 1.05);  // But not by much
    free(compressed);
}

TEST(DeflateV2, CompressRepetitive) {
    std::vector<uint8_t> repetitive(1024*1024, 0xAA);
    auto [compressed, size] = deflate_brutal_masm_v2(repetitive.data(), repetitive.size());
    EXPECT_LT(size, repetitive.size() * 0.1);  // Should compress to <10%
    EXPECT_GT(size, 10000);  // But at least some overhead
}

TEST(DeflateV2, CompressDecompress) {
    // Test data (transformer weights)
    std::vector<uint8_t> original = loadTestWeights("test_weights.gguf");
    
    auto [compressed, comp_size] = deflate_brutal_masm_v2(original.data(), original.size());
    auto [decompressed, decomp_size] = inflate_brutal_masm(compressed, comp_size);
    
    EXPECT_EQ(decomp_size, original.size());
    EXPECT_EQ(std::memcmp(decompressed, original.data(), original.size()), 0);
    
    double ratio = 100.0 * comp_size / original.size();
    printf("Compression ratio: %.1f%%\n", ratio);
    EXPECT_LT(ratio, 40.0);  // Target: <40% for weights
    
    free(compressed);
    free(decompressed);
}

TEST(DeflateV2, Throughput) {
    std::vector<uint8_t> data(100*1024*1024);  // 100MB
    auto start = std::chrono::high_resolution_clock::now();
    
    uint64_t comp_size;
    uint8_t* compressed = deflate_brutal_masm_v2(data.data(), data.size(), &comp_size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (100.0 / ms) * 1000;  // MB/s
    printf("Compression throughput: %.1f MB/s\n", throughput);
    EXPECT_GT(throughput, 50.0);  // Target: >50 MB/s
    
    free(compressed);
}
```

### Performance Targets

| Test | Target | Status |
|---|---|---|
| Repetitive data | 10% ratio | TBD |
| Weights (transformer) | 35-40% ratio | TBD |
| Random data | <105% ratio | TBD |
| Throughput | >50 MB/s | TBD |
| Memory overhead | <2% | TBD |

---

## Implementation Checklist

- [ ] Hash table initialization and collision handling
- [ ] LZ77 match finding core loop with max_dist windowing
- [ ] Huffman tree construction from frequency analysis
- [ ] Canonical Huffman code generation
- [ ] Deflate block encoding with variable-length codes
- [ ] Bit-stream output with alignment
- [ ] CRC32 calculation
- [ ] Integration with inflate for decompression
- [ ] Comprehensive test suite vs zlib
- [ ] Performance benchmarking on real model data
- [ ] Fallback to stored blocks for incompressible data
- [ ] Multi-threaded compression for large blocks
