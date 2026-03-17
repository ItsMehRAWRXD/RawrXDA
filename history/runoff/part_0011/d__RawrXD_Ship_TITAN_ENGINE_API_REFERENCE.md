# RawrXD Titan Engine - Complete API Reference

## Table of Contents

1. [Engine Lifecycle](#engine-lifecycle)
2. [Model Management](#model-management)
3. [Inference Operations](#inference-operations)
4. [Tokenization](#tokenization)
5. [Performance & Diagnostics](#performance--diagnostics)
6. [Memory Management](#memory-management)
7. [Structures & Constants](#structures--constants)
8. [C# Interop Examples](#c-interop-examples)

---

## Engine Lifecycle

### Titan_CreateEngine

Initialize the Titan Engine. Must be called once before any other operations.

**Signature**
```c
int Titan_CreateEngine(void);
```

**Parameters**: None

**Return Value**
- `1`: Success, engine ready for use
- `0`: Failed to initialize

**Description**
- Initializes global state (memory limits, thread pool, mathematical tables)
- Queries system resources (CPU count, physical memory)
- Sets default memory limit to 50% of physical RAM

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_CreateEngine();

if (Titan_CreateEngine() == 1) {
    Console.WriteLine("Engine ready");
}
```

**Thread Safety**: Safe to call from any thread (idempotent)

---

### Titan_DestroyEngine

Shutdown the Titan Engine and free all resources.

**Signature**
```c
int Titan_DestroyEngine(void);
```

**Parameters**: None

**Return Value**
- `1`: Success
- `0`: Failed

**Description**
- Unloads all cached models
- Frees memory arenas
- Stops thread pool
- Closes file handles

**Important**: After calling this, all IntPtr handles become invalid.

**Example**
```csharp
Titan_DestroyEngine();
```

---

## Model Management

### Titan_LoadModelAsset

Load a GGUF model file from disk into the cache.

**Signature**
```c
void* Titan_LoadModelAsset(const char* pFilePath, int load_flags);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pFilePath` | const char* | Full path to GGUF file (null-terminated) |
| `load_flags` | int | Reserved (currently unused, pass 0) |

**Return Value**
- Non-NULL: Pointer to ModelAsset (opaque handle)
- NULL: Load failed

**Load Failures**
- File not found
- Invalid GGUF format
- Unsupported model architecture
- Out of memory

**Description**

This is the **core function** that:
1. Opens GGUF file and memory-maps it
2. Validates magic number and version
3. Parses all metadata (architecture, hyperparameters)
4. Builds perfect hash table for tensor lookup (O(1))
5. Loads and parses all layer tensors
6. Initializes KV cache (FP16, lazy allocation)
7. Sets up tokenizer (BPE vocabulary)
8. Adds model to LRU cache

Models are cached—calling with same path returns existing pointer and increments ref count.

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern IntPtr Titan_LoadModelAsset(
    [MarshalAs(UnmanagedType.LPStr)] string pFilePath, 
    int load_flags);

// Load model
IntPtr model = Titan_LoadModelAsset("C:\\models\\llama-7b.gguf", 0);
if (model == IntPtr.Zero) {
    throw new Exception("Failed to load model");
}

// Later, loading same path is instant (cached)
IntPtr model2 = Titan_LoadModelAsset("C:\\models\\llama-7b.gguf", 0);
// model2 == model (same pointer)
```

**Performance**
- First load: ~100ms (memory mapping)
- Cache hit: <1ms (pointer lookup)
- Memory: 3.5 GB for 7B Q4_0 model

**Thread Safety**: Safe for concurrent loads of different models

---

### Titan_UnloadModelAsset

Unload a model from cache and free its resources.

**Signature**
```c
int Titan_UnloadModelAsset(void* pModel);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle from `Titan_LoadModelAsset` |

**Return Value**
- `1`: Successfully unloaded
- `0`: Model still in use (ref_count > 0)

**Description**
- Decrements reference count
- If ref_count reaches 0, unmaps file and frees KV cache
- Removes from LRU cache
- Handle becomes invalid after unload

**Important**: Do NOT call this while inference is active on this model!

**Example**
```csharp
Titan_UnloadModelAsset(model);
// model pointer is now invalid
```

**Thread Safety**: NOT safe. Call only when no inference active.

---

### Titan_IsModelReady

Check if a model is fully loaded and ready for inference.

**Signature**
```c
int Titan_IsModelReady(void* pModel);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |

**Return Value**
- `1`: Ready (state == ASSET_LOADED or ASSET_STREAMING)
- `0`: Not ready or invalid

**Description**
- Returns immediately (no blocking)
- Allows polling for model load completion
- Useful with async streaming loads

**Example**
```csharp
while (Titan_IsModelReady(model) == 0) {
    Thread.Sleep(100);
}
Console.WriteLine("Model ready for inference");
```

---

### Titan_GetModelInfo

Get model configuration details.

**Signature**
```c
int Titan_GetModelInfo(void* pModel, ModelConfig* pConfig);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pConfig` | ModelConfig* | Output buffer (256 bytes) |

**Return Value**
- `1`: Success
- `0`: Invalid handle

**ModelConfig Structure** (256 bytes)
```c
struct ModelConfig {
    uint32_t arch_type;              // 0=LLAMA, 1=MISTRAL, etc
    char name[64];                   // Model name string
    
    uint32_t n_vocab;                // Vocabulary size
    uint32_t n_ctx_train;            // Training context length
    uint32_t n_embd;                 // Embedding dimension
    uint32_t n_layer;                // Number of transformer layers
    uint32_t n_head;                 // Number of attention heads
    uint32_t n_head_kv;              // KV heads (for GQA)
    uint32_t n_ff;                   // Feedforward hidden size
    uint32_t n_rot;                  // RoPE dimension
    uint32_t n_expert;               // Number of experts (MoE)
    uint32_t n_expert_used;          // Active experts (MoE)
    
    double rope_theta;               // RoPE frequency base
    double rope_scale;               // Context extension scale
    uint32_t rope_dim;               // Rotated dimensions
    
    double rms_norm_eps;             // Layer norm epsilon
    double layer_norm_eps;
    
    uint32_t tokenizer_type;         // 0=BPE, 1=SPM
    uint32_t add_bos;                // Add BOS token?
    uint32_t add_eos;                // Add EOS token?
    uint32_t bos_token;              // BOS token ID
    uint32_t eos_token;              // EOS token ID
    uint32_t unk_token;              // Unknown token ID
    uint32_t pad_token;              // Padding token ID
    uint32_t ln_token;               // Line break token ID
};
```

**Example**
```csharp
[StructLayout(LayoutKind.Sequential)]
public struct ModelConfig {
    public uint arch_type;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
    public byte[] name;
    public uint n_vocab;
    public uint n_ctx_train;
    public uint n_embd;
    public uint n_layer;
    // ... more fields
}

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_GetModelInfo(IntPtr pModel, ref ModelConfig pConfig);

ModelConfig config = new ModelConfig();
if (Titan_GetModelInfo(model, ref config) == 1) {
    Console.WriteLine($"Layers: {config.n_layer}");
    Console.WriteLine($"Heads: {config.n_head}");
    Console.WriteLine($"Vocab: {config.n_vocab}");
}
```

---

### Titan_EnumAvailableModels

Enumerate all currently cached models.

**Signature**
```c
int Titan_EnumAvailableModels(void (*callback)(const char*, const char*, void*), void* pUserData);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `callback` | function pointer | Called for each model |
| `pUserData` | void* | User context passed to callback |

**Callback Signature**
```c
void callback(const char* pPath, const char* pName, void* pUserData)
```

**Return Value**
- Count of enumerated models

**Example**
```csharp
public delegate void EnumCallback(
    [MarshalAs(UnmanagedType.LPStr)] string path,
    [MarshalAs(UnmanagedType.LPStr)] string name,
    IntPtr userData);

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_EnumAvailableModels(EnumCallback callback, IntPtr pUserData);

EnumCallback cb = (path, name, userData) => {
    Console.WriteLine($"Cached: {path}");
};

int count = Titan_EnumAvailableModels(cb, IntPtr.Zero);
Console.WriteLine($"Total cached: {count}");
```

---

## Inference Operations

### Titan_BeginInference

Prepare for a new inference session (token generation).

**Signature**
```c
int Titan_BeginInference(void* pModel, const char* pPrompt);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pPrompt` | const char* | Initial prompt text |

**Return Value**
- `1`: Session created
- `0`: Failed

**Description**
- Tokenizes prompt
- Preallocates output buffers
- Fills KV cache with prompt tokens (without generating)

**Example**
```csharp
Titan_BeginInference(model, "Write a short story about");
```

---

### Titan_StreamGenerate

Generate tokens with streaming output via callback or ring buffer.

**Signature**
```c
int Titan_StreamGenerate(
    void* pModel, 
    const char* pPrompt, 
    int maxTokens, 
    void (*pCallback)(void*, int));
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pPrompt` | const char* | Input text to complete |
| `maxTokens` | int | Maximum tokens to generate |
| `pCallback` | function* | Token callback (NULL for ring buffer) |

**Callback Signature**
```c
void callback(void* pModel, int token_id)
```
Called for each generated token. If NULL, tokens go to ring buffer instead.

**Return Value**
- Number of tokens generated (may be less than maxTokens if EOS token generated)
- -1 on error

**Description**
- Autoregressive generation loop
- Each iteration: forward pass → sample → append to output
- Callback or ring buffer receives tokens as they're generated
- Supports early stopping on EOS token

**Performance**
- First token: ~500ms (entire prompt processed)
- Subsequent tokens: ~50ms each
- ~10-20 tokens/sec on consumer GPU

**Example (Callback)**
```csharp
public delegate void TokenCallback(IntPtr pModel, int token_id);

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_StreamGenerate(
    IntPtr pModel, 
    [MarshalAs(UnmanagedType.LPStr)] string pPrompt,
    int maxTokens,
    TokenCallback pCallback);

List<int> generated = new List<int>();

TokenCallback cb = (model, token_id) => {
    generated.Add(token_id);
    Console.Write(token_id + " ");
};

int count = Titan_StreamGenerate(model, "Hello", 50, cb);
Console.WriteLine($"\nGenerated {count} tokens");
```

**Example (Ring Buffer)**
```csharp
// Pass NULL callback to use ring buffer
int count = Titan_StreamGenerate(model, "Hello", 50, null);

// Read tokens from ring buffer
while (true) {
    byte[] buffer = new byte[256];
    int len = 0;
    
    int result = Titan_GetNextToken(buffer, buffer.Length, ref len);
    if (result == -1) break;  // Generation complete
    if (result == 0) continue; // No data yet
    
    string token = Encoding.UTF8.GetString(buffer, 0, len);
    Console.Write(token);
}
```

---

### Titan_RunInferenceStep

Single-step inference for fine-grained control.

**Signature**
```c
int Titan_RunInferenceStep(void* pModel, int token_id, int pos, float* pLogits);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `token_id` | int | Token ID to process |
| `pos` | int | Position in sequence |
| `pLogits` | float* | Output logits array [n_vocab] |

**Return Value**
- `1`: Success
- `0`: Failed

**Description**
- Low-level API: runs one transformer forward pass
- Allows custom sampling logic
- Caller responsible for:
  - Iterating positions
  - Sampling next token
  - Updating KV cache

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_RunInferenceStep(
    IntPtr pModel, int token_id, int pos, float[] pLogits);

// Manual inference loop
float[] logits = new float[50000];  // n_vocab

for (int pos = 0; pos < maxTokens; pos++) {
    int result = Titan_RunInferenceStep(model, current_token, pos, logits);
    if (result != 1) break;
    
    // Custom sampling
    int next_token = SampleToken(logits, temperature: 0.8f);
    current_token = next_token;
}
```

---

### Titan_EndInference

Terminate inference session and clean up buffers.

**Signature**
```c
int Titan_EndInference(void* pModel);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |

**Return Value**
- `1`: Success
- `0`: Failed

**Description**
- Frees output buffers
- Clears ring buffer
- Does NOT unload model (still cached)

---

### Titan_GetNextToken

Non-blocking read from generation ring buffer.

**Signature**
```c
int Titan_GetNextToken(
    char* pBuffer, 
    int bufSize, 
    int* pLen);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pBuffer` | char* | Output buffer (unmanaged) |
| `bufSize` | int | Buffer size in bytes |
| `pLen` | int* | Output: actual bytes written |

**Return Value**
- `1`: Token string available in pBuffer
- `0`: No data available (generation ongoing)
- `-1`: No data and generation complete (done)

**Description**
- Poll for tokens as they're generated
- Non-blocking: returns immediately if no data
- Multiple consumers can read (but not thread-safe)

**Lock-Free Ring Buffer**:
- Write pointer advanced by generator
- Read pointer advanced by consumer
- Atomics ensure no data loss
- Semaphores provide backpressure

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_GetNextToken(
    byte[] pBuffer, int bufSize, ref int pLen);

// Start generation (non-blocking)
Task.Run(() => {
    Titan_StreamGenerate(model, "Hello", 100, null);
});

// Poll for tokens
StringBuilder output = new StringBuilder();
while (true) {
    byte[] buffer = new byte[256];
    int len = 0;
    
    int result = Titan_GetNextToken(buffer, buffer.Length, ref len);
    
    if (result == 1) {
        output.Append(Encoding.UTF8.GetString(buffer, 0, len));
    } else if (result == -1) {
        break;  // Generation complete
    } else {
        Thread.Sleep(10);  // Backoff while waiting
    }
}

Console.WriteLine(output.ToString());
```

---

## Tokenization

### Titan_Tokenize

Convert text string to token IDs.

**Signature**
```c
int Titan_Tokenize(
    void* pModel, 
    const char* pText, 
    int* pTokens, 
    int maxTokens);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pText` | const char* | Input text |
| `pTokens` | int* | Output token ID array |
| `maxTokens` | int | Max tokens to produce |

**Return Value**
- Number of tokens generated
- -1 on error

**Description**

Implements full BPE tokenization pipeline:
1. UTF-8 decode with proper multibyte handling
2. Initial byte-level tokenization
3. Iterative BPE merges (highest priority first)
4. Clamping to model vocabulary

This is **critical for inference quality**: different tokenizers produce different token sequences, affecting model output.

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_Tokenize(
    IntPtr pModel,
    [MarshalAs(UnmanagedType.LPStr)] string pText,
    int[] pTokens,
    int maxTokens);

int[] tokens = new int[512];
int count = Titan_Tokenize(model, "Hello, world!", tokens, 512);

Console.WriteLine($"Tokens: {string.Join(", ", tokens.Take(count))}");
```

**Tokenizer Variants**:
- **BPE** (most models): Byte-pair encoding, greedy merging
- **SPM** (some models): SentencePiece, prefix-based
- **Unigram** (rare): Probabilistic tokenization

---

### Titan_Detokenize

Convert token IDs back to text string.

**Signature**
```c
int Titan_Detokenize(
    void* pModel, 
    const int* pTokens, 
    int nTokens, 
    char* pOutput);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pTokens` | const int* | Input token ID array |
| `nTokens` | int | Number of tokens |
| `pOutput` | char* | Output buffer (null-terminated) |

**Return Value**
- Length of output string
- -1 on error

**Description**
- Looks up token strings in vocabulary
- Handles special tokens (BOS, EOS, padding)
- Removes spurious spaces (tokenizer normalization)

**Note**: Detokenization is lossy! `text → tokenize → detokenize` may not equal original text due to merges.

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_Detokenize(
    IntPtr pModel,
    int[] pTokens,
    int nTokens,
    byte[] pOutput);

int[] tokens = { 1, 2, 3 };
byte[] output = new byte[1024];

int len = Titan_Detokenize(model, tokens, 3, output);
string text = Encoding.UTF8.GetString(output, 0, len);
Console.WriteLine(text);
```

---

## Performance & Diagnostics

### Titan_GetPerformanceStats

Retrieve inference performance metrics.

**Signature**
```c
int Titan_GetPerformanceStats(void* pModel, PerformanceStats* pStats);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pStats` | PerformanceStats* | Output structure |

**Return Value**
- `1`: Success
- `0`: Failed

**PerformanceStats Structure** (32 bytes)
```c
struct PerformanceStats {
    uint64_t tokens_generated;       // Cumulative token count
    uint64_t total_infer_time_us;    // Total time in microseconds
    float avg_tps;                   // Average tokens/second
    uint32_t layer_time_us;          // Last layer time
};
```

**Example**
```csharp
[StructLayout(LayoutKind.Sequential)]
public struct PerformanceStats {
    public ulong tokens_generated;
    public ulong total_infer_time_us;
    public float avg_tps;
    public uint layer_time_us;
}

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_GetPerformanceStats(
    IntPtr pModel, ref PerformanceStats pStats);

PerformanceStats stats = new PerformanceStats();
Titan_GetPerformanceStats(model, ref stats);

Console.WriteLine($"Tokens: {stats.tokens_generated}");
Console.WriteLine($"Speed: {stats.avg_tps:F1} tokens/sec");
```

---

## Memory Management

### Titan_SetMemoryLimit

Set global memory usage limit.

**Signature**
```c
int Titan_SetMemoryLimit(int limitMB);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `limitMB` | int | Memory limit in megabytes |

**Return Value**
- `1`: Success
- `0`: Invalid limit

**Description**
- Default: 50% of physical RAM
- When exceeded, LRU model eviction triggers
- Set to -1 for unlimited

**Example**
```csharp
[DllImport("RawrXD_Titan_Engine.dll")]
public static extern int Titan_SetMemoryLimit(int limitMB);

// Limit to 8GB
Titan_SetMemoryLimit(8192);
```

---

### Titan_EvictCache

Manually trigger model cache eviction.

**Signature**
```c
int Titan_EvictCache(int targetMB);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `targetMB` | int | Try to free this many MB |

**Return Value**
- Actual MB freed

**Description**
- Evicts least-recently-used models until target met
- Respects reference counts (won't evict in-use models)
- Returns immediately if already below limit

**Example**
```csharp
// Force free 2GB
int freed = Titan_EvictCache(2048);
Console.WriteLine($"Freed {freed} MB");
```

---

### Titan_PrefetchTensor

Hint to prefetch tensor to cache.

**Signature**
```c
int Titan_PrefetchTensor(void* pModel, const char* pTensorName);
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pModel` | void* | Model handle |
| `pTensorName` | const char* | Tensor name (e.g., "blk.0.attn_norm.weight") |

**Return Value**
- `1`: Prefetch initiated
- `0`: Tensor not found

**Description**
- Async hint to load tensor into L3 cache
- Useful for custom inference patterns
- Non-blocking (returns immediately)

**Example**
```csharp
// Prefetch first layer attention weights
Titan_PrefetchTensor(model, "blk.0.attn_norm.weight");
```

---

### Titan_StreamModelAsync

Async model loading (for large models).

**Signature**
```c
void* Titan_StreamModelAsync(const char* pFilePath, void (*pCallback)(int));
```

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `pFilePath` | const char* | Model path |
| `pCallback` | function* | Progress callback (0-100%) |

**Return Value**
- Model handle (may not be ready immediately)

**Description**
- Returns model handle before loading completes
- Callback periodically reports progress
- Call `Titan_IsModelReady` to poll for completion

**Example**
```csharp
public delegate void ProgressCallback(int progress);

[DllImport("RawrXD_Titan_Engine.dll")]
public static extern IntPtr Titan_StreamModelAsync(
    [MarshalAs(UnmanagedType.LPStr)] string pFilePath,
    ProgressCallback pCallback);

ProgressCallback cb = (progress) => {
    Console.Write($"\rLoading: {progress}% ");
};

IntPtr model = Titan_StreamModelAsync("C:\\models\\model.gguf", cb);

// Wait for ready
while (Titan_IsModelReady(model) == 0) {
    Thread.Sleep(100);
}
Console.WriteLine("\nReady!");
```

---

## Structures & Constants

### Quantization Types

```c
#define GGML_TYPE_F32           0    // 32-bit float (4 bytes/weight)
#define GGML_TYPE_F16           1    // 16-bit float (2 bytes/weight)
#define GGML_TYPE_Q4_0          2    // 4-bit, 32 weights/block (18 bytes/block)
#define GGML_TYPE_Q4_1          3    // 4-bit with min value
#define GGML_TYPE_Q5_0          6    // 5-bit, 32 weights/block
#define GGML_TYPE_Q5_1          7    // 5-bit with min value
#define GGML_TYPE_Q8_0          8    // 8-bit (lossless with scaling)
#define GGML_TYPE_Q8_1          9    // 8-bit with min value
#define GGML_TYPE_Q2_K          10   // 2-bit K-quant (256 weights/block)
#define GGML_TYPE_Q3_K          11   // 3-bit K-quant
#define GGML_TYPE_Q4_K          12   // 4-bit K-quant (256 weights/block)
#define GGML_TYPE_Q5_K          13   // 5-bit K-quant
#define GGML_TYPE_Q6_K          14   // 6-bit K-quant
```

**Compression Ratios**:
```
F32:    1.0x (4 bytes/weight)
F16:    0.5x (2 bytes/weight)
Q8_0:   0.5x (quantized int8)
Q4_0:   0.125x (32 weights in 18 bytes) ← Most common
Q2_K:   0.0625x (256 weights in 256 bytes) ← For huge models
```

---

### Architecture Types

```c
#define ARCH_LLAMA        0    // Meta LLaMA
#define ARCH_MISTRAL      1    // Mistral AI
#define ARCH_MIXTRAL      2    // Mixtral MoE
#define ARCH_PHI          3    // Microsoft Phi
#define ARCH_GEMMA        4    // Google Gemma
#define ARCH_QWEN2        5    // Alibaba Qwen2
#define ARCH_COMMAND_R    6    // Cohere Command-R
#define ARCH_DEEPSEEK     7    // DeepSeek
#define ARCH_LLAMA3       8    // Meta LLaMA 3
```

---

## C# Interop Examples

### Complete Example: Load, Generate, Display

```csharp
using System;
using System.Runtime.InteropServices;
using System.Text;

class TitanTest {
    [DllImport("RawrXD_Titan_Engine.dll")]
    private static extern int Titan_CreateEngine();
    
    [DllImport("RawrXD_Titan_Engine.dll")]
    private static extern IntPtr Titan_LoadModelAsset(
        [MarshalAs(UnmanagedType.LPStr)] string path, int flags);
    
    [DllImport("RawrXD_Titan_Engine.dll")]
    private static extern int Titan_StreamGenerate(
        IntPtr model,
        [MarshalAs(UnmanagedType.LPStr)] string prompt,
        int maxTokens,
        TokenCallback cb);
    
    [DllImport("RawrXD_Titan_Engine.dll")]
    private static extern int Titan_Detokenize(
        IntPtr model, int[] tokens, int nTokens, byte[] output);
    
    public delegate void TokenCallback(IntPtr model, int tokenId);
    
    static void Main() {
        // Initialize
        if (Titan_CreateEngine() != 1) {
            Console.Error.WriteLine("Failed to create engine");
            return;
        }
        
        // Load model
        IntPtr model = Titan_LoadModelAsset("C:\\models\\llama-7b.gguf", 0);
        if (model == IntPtr.Zero) {
            Console.Error.WriteLine("Failed to load model");
            return;
        }
        
        // Generate with callback
        int[] tokens = new int[1000];
        int tokenCount = 0;
        
        TokenCallback cb = (m, tid) => {
            if (tokenCount < tokens.Length) {
                tokens[tokenCount++] = tid;
                Console.Write(tid + " ");
            }
        };
        
        Console.WriteLine("Generating...");
        int generated = Titan_StreamGenerate(
            model,
            "What is machine learning?",
            100,
            cb);
        
        Console.WriteLine($"\n\nGenerated {generated} tokens");
        
        // Detokenize result
        byte[] output = new byte[2048];
        int len = Titan_Detokenize(model, tokens, tokenCount, output);
        string result = Encoding.UTF8.GetString(output, 0, len);
        Console.WriteLine($"Output:\n{result}");
    }
}
```

---

## Error Codes

| Code | Meaning |
|------|---------|
| 0 | Generic failure |
| 1 | Success |
| -1 | Out of memory |
| -2 | File not found |
| -3 | Invalid format |
| -4 | Unsupported architecture |
| -5 | Tensor lookup failed |

---

**Version**: 1.0  
**Updated**: January 2026  
**Compatibility**: Windows x64, GGUF v3 format  
**Dependencies**: None (zero external DLLs at runtime)
