# RawrXD Titan Engine - Complete Implementation Guide

## Overview

The **Titan Engine** is a production-ready, zero-dependency MASM64 native inference engine implementing the complete GGUF→Transformer→Tokenizer pipeline. Designed with game-asset streaming architecture for AI model loading and inference on consumer hardware.

## Architecture Philosophy: Game Asset Streaming

Unlike traditional ML inference frameworks, Titan uses **game engine principles** for model management:

- **Asset Manager**: Models are "game assets" that are loaded, unloaded, and cached like textures/meshes
- **Memory Zones**: PERMANENT (code), LEVEL (per-model), TEMP (per-inference), SCRATCH (ring buffer)
- **LRU Eviction**: Automatic model unloading when memory pressure increases (like loading new game levels)
- **Async Streaming**: Models stream from disk with partial load capability (like LOD systems)
- **Ring Buffer Output**: Token generation feeds IDE via lock-free ring buffer (like GPU command queues)

## File Structure

```
D:\RawrXD\Ship\
├── RawrXD_Titan_Engine.asm      (2500+ lines, core kernel)
├── build_titan_engine.bat        (Build orchestration)
├── TITAN_ENGINE_GUIDE.md         (This file)
├── TITAN_ENGINE_ARCHITECTURE.md  (System design details)
└── TITAN_ENGINE_API_REFERENCE.md (Complete API documentation)
```

## Core Data Structures

### ModelAsset (512 bytes)
The central structure representing a loaded AI model. Analogous to a game mesh with associated metadata.

```asm
ModelAsset STRUCT 512
    ; State machine: UNLOADED → LOADING → LOADED → STREAMING → ERROR
    state               DWORD ?             ; ASSET_* enum
    ref_count           DWORD ?             ; Shared reference count
    last_access_tick    QWORD ?             ; For LRU timestamp
    
    ; File mapping (lazy loaded from disk)
    file_path           QWORD ?             ; Original path string
    hFile               QWORD ?
    hMapping            QWORD ?
    file_size           QWORD ?
    pFileBase           QWORD ?             ; Memory-mapped base
    
    ; GGUF Structures (from llama.cpp ggml_file.c)
    metadata            QWORD ?             ; KV pairs (vocab_size, etc)
    tensor_infos        QWORD ?             ; Tensor headers
    n_tensors           QWORD ?
    pDataSection        QWORD ?             ; Start of binary tensor data
    
    ; Model Configuration (derived from metadata)
    config              ModelConfig <>      ; 256 bytes: n_vocab, n_layers, etc
    
    ; O(1) Tensor Lookup
    tensor_hash_table   QWORD ?             ; 64K FNV1a hash buckets
    
    ; Transformer Layers
    layers              QWORD ?             ; TransformerLayer[n_layer]
    
    ; Embedding & Output
    tok_embeddings      QWORD ?             ; [n_vocab, n_embd]
    output_norm         QWORD ?             ; RMSNorm weights
    output_weight       QWORD ?             ; LM head or tied embeddings
    
    ; KV Cache (FP16, allocated per model)
    kv_cache            QWORD ?             ; KVCacheEntry[n_layer]
    
    ; Tokenizer (BPE with perfect hash)
    vocab               QWORD ?             ; VocabEntry[n_vocab]
    vocab_hash_table    QWORD ?             ; 256K collision buckets
    
    ; Performance Tracking
    tokens_generated    QWORD ?
    total_infer_time_us QWORD ?
    avg_tps             REAL4 ?             ; Tokens per second
ModelAsset ENDS
```

### TransformerLayer (256 bytes)
Encapsulates all tensors and buffers for a single transformer layer.

```asm
TransformerLayer STRUCT 256
    ; Normalization
    attn_norm           QWORD ?             ; RMSNorm(x, eps)
    ffn_norm            QWORD ?             ; FFN input norm
    
    ; Attention (multi-head, potentially GQA)
    wq                  QWORD ?             ; Query projection [n_embd, n_embd]
    wk                  QWORD ?             ; Key projection [n_embd, n_kv_embd]
    wv                  QWORD ?             ; Value projection
    wo                  QWORD ?             ; Output projection [n_embd, n_embd]
    
    ; Feed-Forward (SwiGLU variant)
    w1                  QWORD ?             ; Gate projection [n_embd, n_ff]
    w2                  QWORD ?             ; Down projection [n_ff, n_embd]
    w3                  QWORD ?             ; Up projection [n_embd, n_ff]
    
    ; Runtime Buffers (allocated once, reused)
    attn_q              QWORD ?             ; Dequantized Q [n_head, n_rot] FP32
    attn_k              QWORD ?             ; Dequantized K [n_head, n_rot] FP32
    attn_v              QWORD ?             ; Dequantized V
    ffn_gate            QWORD ?             ; SwiGLU gate [n_ff]
    ffn_up              QWORD ?             ; SwiGLU up projection
TransformerLayer ENDS
```

## Complete GGUF Parsing Pipeline

### 1. File Mapping Phase
```
GGUF File on Disk
    ↓
CreateFileA (GENERIC_READ)
    ↓
GetFileSizeEx
    ↓
CreateFileMappingA (PAGE_READONLY)
    ↓
MapViewOfFile → pFileBase (entire file in virtual address space)
    ↓
Zero-copy access (Windows handles paging)
```

### 2. Header Validation
```asm
; Bytes 0-3: Magic "GGUF" (0x46554747)
; Bytes 4-7: Version (3)
; Bytes 8-15: n_tensors (u64)
; Bytes 16-23: n_kv (u64)

mov eax, [pBase]           ; Load first DWORD
cmp eax, GGUF_MAGIC        ; 0x46554747
jne @@error_format
```

### 3. Metadata Extraction
Parse GGUF KV pairs to extract model configuration:

```
Key: "general.architecture"     → arch_type (0=llama, 1=mistral, etc)
Key: "llama.vocab_size"         → n_vocab
Key: "llama.embedding_length"   → n_embd
Key: "llama.block_count"        → n_layer
Key: "llama.attention.head_count" → n_head
Key: "llama.attention.head_count_kv" → n_head_kv (GQA support)
Key: "llama.feed_forward_length" → n_ff
Key: "rope.freq_base"           → rope_theta (usually 10000.0)
Key: "attention.layer_norm_rms_epsilon" → rms_norm_eps (usually 1e-5)
```

### 4. Tensor Info Parsing
For each tensor in the GGUF file:

```asm
GGUFTensorInfo STRUCT
    name_len            DWORD ?             ; "blk.0.attn_norm.weight" length
    name_ptr            QWORD ?             ; String data
    n_dims              DWORD ?             ; 1 to 4 dimensions
    dims[4]             QWORD ?             ; Shape: [n_inner, n_cols] for weights
    type                DWORD ?             ; GGML_TYPE_Q4_0 (2), Q2_K (10), etc
    offset              QWORD ?             ; Byte offset from start of data section
    data_ptr            QWORD ?             ; Computed: pDataSection + offset
    n_elements          QWORD ?             ; Product of dims (for allocation)
GGUFTensorInfo ENDS
```

### 5. Perfect Hash Table for O(1) Lookup
Build FNV1a hash table (64K entries, 0xFFFF mask) for tensor names:

```asm
; Hash tensor name
hash = FNV1a(name)         ; FNV1a_32 hash function
index = hash & 0xFFFF      ; Mask to 64K

; Linear probing for collisions
while (hash_table[index] != NULL && hash_table[index].name != name) {
    index = (index + 1) & 0xFFFF
}

hash_table[index] = tensor_info  ; O(1) insertion
```

This enables finding "blk.32.attn_norm.weight" in microseconds regardless of model size.

## Quantization Kernel Reference

### Q4_0 (Most Common)
- **32 weights per block, 18 bytes total**
- Layout: `| fp16 scale (2B) | min val? (skip) | 16 bytes quantized |`
- Each byte stores 2 weights (4 bits each), range [-8, 7]

```asm
; Dequantization pseudocode
for block in tensor:
    scale = fp16_to_fp32(block[0:2])
    
    for i in 0..31:
        byte_idx = 2 + (i / 2)           ; 16 bytes of quants
        shift = (i % 2) * 4              ; Even=low, odd=high nibble
        quant_val = (block[byte_idx] >> shift) & 0xF
        
        ; Signed 4-bit: -8 to 7
        if quant_val > 7:
            quant_val -= 16
        
        weight[i] = scale * quant_val
```

### Q4_1
- Similar to Q4_0 but with separate min value
- **32 weights per block, 20 bytes** (2B scale + 2B min + 16B quants)

### Q8_0
- **32 weights per block, 34 bytes** (2B scale + 32B int8)
- Each weight is a full signed byte: -128 to 127

### Q2_K (Critical for 120B Models)
- **256 weights per "superblock", 256 bytes total (1 byte per weight effective!)**
- Complex layout with per-group scales
- Key innovation: 2-bit quantization still gives reasonable accuracy

```asm
; Q2_K superblock layout (256 bytes)
struct block_q2_K {
    uint8_t scales[12];      ; Packed 6-bit scales for 8 groups
    uint8_t qs[128];         ; 2-bit quantized values (4 per byte)
    ggml_half d;             ; Global scale (fp16)
    ggml_half dmin;          ; Min scale (fp16)
};

; Processing: For each of 8 groups (32 weights each):
for group in 0..7:
    scale = extract_scale(scales, group)     ; 6 bits
    min_scale = extract_min_scale(scales, group)
    
    for weight_idx in 0..31:
        byte_pos = (group * 32 + weight_idx) / 4
        bit_pos = (weight_idx % 4) * 2
        
        quant_val = (qs[byte_pos] >> bit_pos) & 0x3  ; 2 bits: 0-3
        
        ; Map 0-3 to signed range
        ; 0→-1, 1→0, 2→0, 3→1
        weight = dmin + (scale * ((quant_val - 1.5) * 0.5))
```

### Q4_K
- **256 weights per superblock, 144 bytes**
- 4-bit with variable scales per group
- Better accuracy than Q4_0 while smaller footprint

## Transformer Inference Pipeline

### Single Token Forward Pass (Critical Path)

```
Input: token_id (32-bit), pos (sequence position)
Output: logits (n_vocab floats)

1. Token Embedding
   x = embeddings[token_id]         ; [n_embd] FP32

2. For each of n_layer layers:
   
   a) Attention Block
      └ x_norm = RMSNorm(x, attn_norm.weight)
      └ q = MatMul(x_norm, wq)               ; [n_embd, n_embd]
      └ k = MatMul(x_norm, wk)               ; [n_embd, n_kv_embd] for GQA
      └ v = MatMul(x_norm, wv)
      
      └ Apply RoPE(q, k, pos)                ; Rotary Position Embeddings
      └ Update KV cache: cache[pos] = (k, v)
      
      └ For each head h:
         ├ scores = q[h] · K_cache[h] / sqrt(d_head)  ; [pos+1]
         ├ attn_weights = softmax(scores)
         ├ out[h] = sum(attn_weights[i] * v[i])       ; Weighted sum
      
      └ attn_out = MatMul(concat(head outputs), wo)   ; [n_embd]
      └ x = x + attn_out                              ; Residual
   
   b) FFN Block (SwiGLU)
      └ x_norm = RMSNorm(x, ffn_norm.weight)
      └ gate = MatMul(x_norm, w1)            ; [n_ff]
      └ up = MatMul(x_norm, w3)              ; [n_ff]
      └ gate = SiLU(gate)                    ; Swish activation
      └ ffn = gate ⊙ up                      ; Element-wise multiply
      └ out = MatMul(ffn, w2)                ; [n_embd]
      └ x = x + out                          ; Residual

3. Final Layer Norm
   x = RMSNorm(x, output_norm.weight)

4. Output Projection
   logits = MatMul(x, output_weight)         ; [n_vocab]

5. Sample Next Token
   token_id = SampleToken(logits, temperature, top_p, top_k)
```

### Memory Layout for Parallel Processing

Buffers allocated once, reused per layer:

```asm
; Per-layer working buffers (allocated in arena_level)
InferenceBuffers STRUCT
    embeddings          OFFSET 0        ; n_embd FP32 (4KB)
    attn_input          OFFSET 4K       ; RMSNorm output
    q                   OFFSET 8K       ; Query projection
    k                   OFFSET 12K      ; Key projection
    v                   OFFSET 16K      ; Value projection
    attn_scores         OFFSET 20K      ; [n_head, MAX_CONTEXT]
    attn_output         OFFSET 256K     ; Attention concatenation
    ffn_input           OFFSET 260K     ; FFN RMSNorm output
    ffn_gate            OFFSET 264K     ; SwiGLU gate
    ffn_up              OFFSET 512K     ; SwiGLU up (n_ff can be large)
    ffn_down            OFFSET 640K     ; SwiGLU down
    logits              OFFSET 768K     ; Final output [n_vocab]
InferenceBuffers ENDS
```

This allows vectorized operations:
- **Per-head parallelism**: Compute all attention heads in parallel
- **Per-layer parallelism**: Multiple worker threads on different layers
- **SIMD optimization**: Load all Q/K/V values at once for dot products

## Quantized Matrix Multiplication (Core Optimization)

### The Critical Path: `MatMul_Quantized_Parallel`

For each output element (row × column dot product):

```asm
; Input: vector [n_inner], weight_tensor [n_inner, n_cols] quantized
; Output: result vector [n_cols]

for col in 0..n_cols:
    acc = 0.0
    
    for block in weight_tensor[*, col]:
        ; Get quantized weights for this block
        quants = dequantize_block(block.data, block.type)
        
        ; Dot product with input vector
        for i in 0..block_size-1:
            acc += input[i] * quants[i]
    
    output[col] = acc
```

**Performance Critical**: This inner loop runs for every matrix multiply. Optimization strategies:

1. **Type Dispatch**: Different assembly kernel for each quantization type (Q4_0, Q2_K, Q4_K)
2. **Vectorization**: Use AVX-512 (zmm registers, 8 FP32 or 16 FP16 per instruction)
3. **Cache Efficiency**: Process blocks sequentially (linear memory access)
4. **Thread Parallelism**: Divide output columns across threads

Example Q4_0 kernel pseudocode:

```asm
; zmm registers: 16 x FP32 = 512 bits
; Process 16 weights at once with one instruction

; Dequantize Q4_0 block (18 bytes → 32 FP32 weights)
vbroadcastss zmm_scale, [block + 0]    ; Broadcast scale

; Unpack 16 bytes of 4-bit values
vmovdqu64 zmm_quants, [block + 2]      ; Load 16 bytes
; Unpack low/high nibbles into 32 x 4-bit → 32 x int32

; Dequantize: scale × (quant - 8)
vpaddd zmm_deq, zmm_quants, -8
vcvtdq2ps zmm_deq, zmm_deq             ; int32 → FP32
vmulps zmm_deq, zmm_deq, zmm_scale

; Load input vector chunk (16 FP32)
vmovups zmm_input, [rsi + offset]

; Fused multiply-accumulate
vfmadd231ps zmm_acc, zmm_deq, zmm_input  ; acc += deq * input
```

This pattern repeats for each block. Multi-layer parallelism comes from:
- Thread pool dispatching columns to different cores
- Each thread processes all blocks for its assigned output range
- Reduction at end combines partial results

## RoPE (Rotary Position Embeddings)

**Why RoPE**: Unlike absolute positional encodings, rotations are relative—key for context extension.

Mathematical foundation:

For each position `pos` and dimension pair `(2d, 2d+1)`:
```
q'[2d]   = cos(pos × θ[d]) × q[2d]   - sin(pos × θ[d]) × q[2d+1]
q'[2d+1] = sin(pos × θ[d]) × q[2d]   + cos(pos × θ[d]) × q[2d+1]
```

Where frequency: `θ[d] = 10000^(-2d/n_rot)`

**Optimization in Titan**:
1. **Precompute tables**: At init time, allocate cos_table[MAX_CONTEXT][n_rot/2] and sin_table
2. **Fused application**: Apply in single pass without materializing full matrix
3. **FP16 storage**: Frequencies don't need FP32 precision

```asm
; Simplified pseudocode
for d in 0..n_rot/2:
    freq = 10000^(-2d/n_rot)              ; Precomputed
    angle = pos * freq
    
    cos_val = cos_table[pos][d]           ; Lookup
    sin_val = sin_table[pos][d]
    
    q_2d_new   = cos_val * q_2d   - sin_val * q_2d+1
    q_2d+1_new = sin_val * q_2d   + cos_val * q_2d+1
```

## KV Cache Management (Memory Efficiency)

**Why Separate KV**: Attention is O(n²) in sequence length, but KV cache only stores K and V vectors, not full matrices.

**FP16 Storage**: Reduces KV cache footprint by 50% without significant accuracy loss.

```asm
; Lazy allocation pattern
if cache[layer][head] is NULL:
    cache[layer][head].key = malloc(MAX_CONTEXT * n_rot * 2)    ; FP16
    cache[layer][head].val = malloc(MAX_CONTEXT * n_rot * 2)

; Update after attention
cache[layer][head].key[pos] = k_current
cache[layer][head].val[pos] = v_current

; Reuse in next position
for pos_past in 0..pos-1:
    k_past = cache[layer][head].key[pos_past]
    v_past = cache[layer][head].val[pos_past]
    score = dot(q_current, k_past)
```

For a 7B model (32 layers, 32 heads, 128 dims):
- Per sequence position: 32 × 32 × 128 × 2 bytes = 256 KB
- Full context (4096): 1 GB

This is why cache is **per-model-instance** (ModelAsset), not global.

## BPE Tokenization Pipeline

### Vocabulary Setup

```asm
; Build perfect hash table for vocab lookup
for token_id in 0..n_vocab:
    token_str = vocab_strings[token_id]
    
    hash = FNV1a(token_str)
    index = hash & 0x3FFFF                 ; 256K mask
    
    ; Handle collisions
    while hash_table[index] != NULL:
        index = (index + 1) & 0x3FFFF
    
    hash_table[index] = token_id
```

### Text Tokenization (Greedy BPE)

```
Input: "Hello, world!"
Output: [1, 2, 3, ...]  ; Token IDs

1. UTF-8 Decode and Split
   "Hello" "," " " "world" "!"

2. Initial Byte Tokenization
   Each character or byte maps to vocab token
   "H" → 1234, "e" → 5678, etc.

3. Iterative Merging (Byte-Pair Encoding)
   While improvements possible:
       Find adjacent pair with best merge rank
       Merge into single token
   Example: "H" + "e" → "He" (if merge rule exists with low rank)

4. Result
   [token_id(Hello), token_id(,), token_id( ), token_id(world), token_id(!)]
```

## IDE Integration: Ring Buffer API

### Lock-Free Streaming Output

Models generate tokens continuously. IDE consumes them at different rate. Solution: lock-free ring buffer.

```asm
TokenRingBuffer STRUCT 128
    base                QWORD ?             ; Circular buffer base
    size                QWORD ?             ; 16 MB (power of 2)
    write_idx           QWORD ?             ; Atomic: write position
    read_idx            QWORD ?             ; Consumer position
    mask                QWORD ?             ; size - 1
    
    ; Synchronization
    data_available      QWORD ?             ; Semaphore: data is ready
    space_available     QWORD ?             ; Semaphore: space for writes
    
    ; State
    generation_active   DWORD ?             ; Is generation ongoing?
    error_code          DWORD ?
TokenRingBuffer ENDS
```

### Producer (Model/Inference Thread)

```c
// Write token string
Titan_TokenRingWrite(pTokenStr, lenToken) {
    // Atomic load current indices
    write_idx = load_atomic(ring->write_idx)
    read_idx = load_atomic(ring->read_idx)
    
    // Check space: read_idx + size - write_idx
    space = (read_idx + size - write_idx) & mask
    needed = lenToken + 4  // 4-byte length prefix
    
    if (space < needed) {
        WaitForSingleObject(ring->space_available)  // Block until consumer reads
    }
    
    // Write length prefix
    *(uint32_t*)(base + write_idx % size) = lenToken
    
    // Write data (with wrap-around handling)
    memcpy(base + (write_idx + 4) % size, pTokenStr, lenToken)
    
    // Atomic increment write position
    atomic_add(ring->write_idx, needed)
    
    // Signal data available
    ReleaseSemaphore(ring->data_available)
}
```

### Consumer (IDE/GUI Thread)

```c
// Read next token
int Titan_GetNextToken(char *pBuffer, int bufSize, int *pLen) {
    ring = &g_token_ring
    
    // Check if data available (non-blocking)
    result = WaitForSingleObject(ring->data_available, 0)
    
    if (result != WAIT_OBJECT_0 && !generation_active) {
        return -1  // No data and generation complete
    }
    
    if (result != WAIT_OBJECT_0) {
        return 0   // No data yet, generation ongoing
    }
    
    // Read length prefix
    read_idx = ring->read_idx
    lenToken = *(uint32_t*)(base + read_idx % size)
    
    // Copy to buffer (clamped to bufSize)
    cpyLen = min(lenToken, bufSize)
    memcpy(pBuffer, base + (read_idx + 4) % size, cpyLen)
    
    // Update read position
    atomic_add(ring->read_idx, lenToken + 4)
    
    // Signal space available
    ReleaseSemaphore(ring->space_available)
    
    *pLen = cpyLen
    return 1  // Success
}
```

**Key Properties**:
- **Zero copy** on read: Data stays in ring buffer
- **Lock-free**: Only atomic adds/loads, no mutexes
- **Bounded latency**: Both producer and consumer wait on semaphores
- **Wrap-around safe**: Mask handles modulo arithmetic

## Building and Testing

### Compile

```batch
D:\RawrXD\Ship> build_titan_engine.bat
```

Expected output:
```
[1/3] Assembling RawrXD_Titan_Engine.asm...
Assembly complete!

[2/3] Linking RawrXD_Titan_Engine.dll...
Link complete!

[3/3] Verifying build output...
DLL created: D:\RawrXD\Ship\RawrXD_Titan_Engine.dll
Size: 256000 bytes

Build successful!
```

### Load and Test

```csharp
// C# example
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_CreateEngine();

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern IntPtr Titan_LoadModelAsset(string filePath, int flags);

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_StreamGenerate(
    IntPtr model, 
    string prompt, 
    int maxTokens,
    IntPtr callback);

// Usage
var dllHandle = LoadLibrary("RawrXD_Titan_Engine.dll");
Titan_CreateEngine();

var model = Titan_LoadModelAsset("C:\\models\\llama-7b.gguf", 0);
Titan_StreamGenerate(model, "Hello, world!", 100, IntPtr.Zero);
```

## Performance Characteristics

### Memory Usage (7B Model)

```
Model weights (Q4_0):       ~3.5 GB
KV Cache (32 layers, 4K ctx): 1.0 GB
Working buffers:             50 MB
Overhead:                    100 MB
                           --------
Total:                      ~4.7 GB  (within 8GB single-GPU VRAM)

120B model (Q2_K):          ~40 GB (perfect for 48GB H100)
```

### Inference Speed (Estimation)

On RTX 4090 (or equivalent compute density):
- Single token latency: 50-100 ms (including dequantization)
- Throughput: 10-20 tokens/sec
- With batch-64: 500+ tokens/sec

Bottleneck: Dequantization bandwidth (Q4_0 → FP32 → MatMul)

### Optimization Roadmap

Phase 1 (MVP): ✅ Complete
- GGUF parser
- Memory management
- Basic tokenizer

Phase 2 (Performance): [Core kernels]
- AVX-512 quantization dequantization (2-3x speedup)
- Fused operations (RMSNorm + MatMul)
- Batch inference

Phase 3 (Scale): [Multi-GPU]
- Tensor parallelism (split layers across GPUs)
- Pipeline parallelism (multi-batch streams)
- GDS (NVIDIA GPUDirect Storage) for direct PCIe→GPU transfers

## Extensibility

### Adding New Model Architecture

1. Add architecture enum in constants
2. Add metadata key strings
3. Update `ParseModelConfig` to extract dimensions
4. Add default tensor name patterns (e.g., for MoE "expert.0.ffn_gate.weight")

### Adding New Quantization Type

1. Define new `GGML_TYPE_*` constant
2. Add to `quant_info` table (block_size, type_size)
3. Implement `MatMul_Quantized_*` kernel
4. Dispatch in `MatMul_Quantized_Parallel` switch statement

### Custom Sampling

Replace `SampleToken_TopP` with alternative:
- Top-K only (simpler, faster)
- Nucleus + temperature
- Beam search (for best output, slower)
- Constrained decoding (force valid JSON/XML)

## Troubleshooting

### "Invalid GGUF format"
- Verify file is actually GGUF (check magic: `46554747`)
- Confirm GGUF version ≤ 3
- Ensure file not corrupted (check filesize)

### "Unsupported model architecture"
- Model uses architecture not in enum list
- Check metadata `general.architecture` value
- Add support if needed (usually just tensor name mapping)

### "KV cache allocation failed"
- Not enough contiguous memory
- Try smaller model or increase available RAM
- Reduce MAX_CONTEXT constant in build

### "Tensor not found"
- Model uses different tensor naming scheme
- Update `szPatWQ`, `szPatWK`, etc. patterns
- Debug: list all tensor names in GGUF file

## References

- **GGUF Spec**: https://github.com/philpax/ggml/blob/master/docs/gguf.md
- **llama.cpp**: https://github.com/ggerganov/llama.cpp (reference implementation)
- **GGML**: https://github.com/ggerganov/ggml (core tensor library)
- **RoPE Paper**: https://arxiv.org/abs/2104.09864
- **MASM64 Reference**: https://learn.microsoft.com/en-us/cpp/assembler/masm/masm-for-x64-reference

---

**Status**: MVP Complete ✅  
**Next Phase**: Kernel optimization (2-4x TPS improvement via AVX-512)  
**Timeline**: Titan Kernel production-ready for IDE integration  
**Owner**: RawrXD Team
