# PHASE 2: MODEL LOADER - API REFERENCE

## Table of Contents

1. [Enums & Constants](#enums--constants)
2. [Structures](#structures)
3. [Main API Class](#main-api-class)
4. [Callbacks](#callbacks)
5. [Utility Functions](#utility-functions)
6. [C Interop Functions](#c-interop-functions)
7. [Error Handling](#error-handling)
8. [Examples](#examples)

---

## Enums & Constants

### FormatType

```cpp
enum class FormatType : uint32_t {
    UNKNOWN = 0,       // Not recognized
    GGUF = 1,          // GGUF format (llama.cpp)
    SAFETENSORS = 2,   // Safetensors (HuggingFace)
    PYTORCH = 3,       // PyTorch checkpoint
    ONNX = 4,          // ONNX format
};
```

**Usage:**
```cpp
auto format = loader->GetFormatType();
if (format == FormatType::GGUF) {
    printf("GGUF model detected\n");
}
```

### RouterType

```cpp
enum class RouterType : uint32_t {
    UNKNOWN = 0,           // Default/error state
    GGUF_LOCAL = 1,        // Local file, full load
    GGUF_MMAP = 2,         // Memory-mapped local file
    HF_HUB = 3,            // Download from HuggingFace
    OLLAMA_API = 4,        // Remote Ollama instance
    MASM_BLOB = 5,         // Embedded model
};
```

**Usage:**
```cpp
auto router = loader->GetRouterType();
printf("Loaded via: %s\n", RouterTypeToString(router));
```

### LoadFlags

```cpp
enum class LoadFlags : uint32_t {
    STREAMING = 0x0001,    // Streaming load (background prefetch)
    MMAP = 0x0002,         // Memory-map file (zero-copy)
    VERIFY = 0x0004,       // Verify SHA-256 checksum
    DECRYPT = 0x0008,      // Decrypt encrypted model
    PROGRESS = 0x0010,     // Enable progress callbacks
    NUMA_AFFINE = 0x0020,  // Allocate on specific NUMA node
    GPU_PIN = 0x0040,      // Pin memory for GPU DMA
};
```

**Usage:**
```cpp
// Load with streaming + verification
uint32_t flags = (uint32_t)LoadFlags::STREAMING | 
                 (uint32_t)LoadFlags::VERIFY;
loader->LoadModel("model.gguf", flags);
```

### TensorState

```cpp
enum class TensorState : uint32_t {
    UNLOADED = 0,  // Not yet in memory
    LOADING = 1,   // Being loaded by background thread
    LOADED = 2,    // Ready to use
    EVICTED = 3,   // Was loaded but evicted to disk
};
```

### GGMLType

```cpp
enum class GGMLType : uint32_t {
    F32 = 0,           // 32-bit float (4 bytes)
    F16 = 1,           // 16-bit float (2 bytes)
    Q4_0 = 2,          // 4-bit quantization v0
    Q4_1 = 3,          // 4-bit quantization v1
    Q5_0 = 6,          // 5-bit quantization v0
    Q5_1 = 7,          // 5-bit quantization v1
    Q8_0 = 8,          // 8-bit quantization v0
    Q8_1 = 9,          // 8-bit quantization v1
    Q2_K = 10,         // 2-bit K-quant
    Q3_K = 11,         // 3-bit K-quant
    Q4_K = 12,         // 4-bit K-quant (most common)
    Q5_K = 13,         // 5-bit K-quant
    Q6_K = 14,         // 6-bit K-quant
    Q8_K = 15,         // 8-bit K-quant
    IQ2_XXS = 16,      // Extreme 2-bit (0.25x size)
    IQ2_XS = 17,       // Very aggressive 2-bit
    IQ3_XXS = 18,      // Extreme 3-bit (0.34x size)
    IQ1_S = 19,        // 1-bit with scaling
    IQ4_NL = 20,       // 4-bit non-linear
    IQ3_S = 21,        // 3-bit with scaling
    IQ2_S = 22,        // 2-bit with scaling
    IQ4_XS = 23,       // 4-bit extra small
    I8 = 24,           // 8-bit integer
    I16 = 25,          // 16-bit integer
    I32 = 26,          // 32-bit integer
    I64 = 27,          // 64-bit integer
    F64 = 28,          // 64-bit float
    IQ1_M = 29,        // 1-bit mixed
};
```

### ModelArch

```cpp
enum class ModelArch : uint32_t {
    UNKNOWN = 0,   // Unknown architecture
    LLAMA = 1,     // Llama / Llama 2 / Code Llama
    MISTRAL = 2,   // Mistral AI
    PHI = 3,       // Microsoft Phi
    GEMMA = 4,     // Google Gemma
    QWEN = 5,      // Alibaba Qwen
};
```

### Constants

```cpp
constexpr uint32_t TENSOR_NAME_MAX = 128;           // Max tensor name length
constexpr uint32_t MAX_TENSORS = 10000;             // Max tensors per model
constexpr size_t CIRCULAR_BUFFER_SIZE = 0x40000000; // 1GB streaming buffer
```

---

## Structures

### TensorMetadata

Complete metadata for a single tensor.

```cpp
struct TensorMetadata {
    // === Identification ===
    char name[TENSOR_NAME_MAX];     // Tensor name (e.g., "layers.0.attn.wq")
    uint64_t name_hash;             // FNV-1a hash for O(1) lookup
    
    // === Shape ===
    uint32_t n_dims;                // Number of dimensions (1-4)
    uint64_t dims[4];               // Individual dimensions
    uint64_t n_elements;            // Total element count (product of dims)
    
    // === Data Layout ===
    uint32_t dtype;                 // GGML quantization type (GGMLType)
    uint32_t type_size;             // Bytes per element (dequantized)
    uint64_t data_size;             // Bytes on disk (quantized)
    
    // === Location ===
    uint64_t file_offset;           // Offset in file
    void* file_handle;              // Win32 file handle
    
    // === Memory ===
    void* host_ptr;                 // CPU memory address
    void* device_ptr;               // GPU memory address
    void* dma_handle;               // Pinned memory handle
    
    // === Quantization ===
    uint32_t quant_type;            // Quantization scheme
    uint32_t quant_block_size;      // Block size for quantization
    void* quant_scale_ptr;          // Pointer to scales/mins
    
    // === State ===
    TensorState state;              // Load state (0-3)
    uint32_t ref_count;             // Reference count for sharing
    uint64_t last_access;           // Timestamp of last access
    
    // === NUMA ===
    uint32_t preferred_node;        // Preferred NUMA node for allocation
    uint32_t actual_node;           // Actually allocated on this node
};
```

**Field Descriptions:**

| Field | Size | Description |
|-------|------|-------------|
| name | 128 | ASCII tensor name, null-terminated |
| name_hash | 8 | For fast O(1) hash table lookup |
| n_dims | 4 | Typically 1-4 for transformer models |
| dims[4] | 32 | [batch, seq_len, hidden, unused] |
| n_elements | 8 | Product of dims (don't compute) |
| dtype | 4 | From GGMLType enum |
| data_size | 8 | Actual file size (may be quantized) |
| host_ptr | 8 | Direct memory access for computation |
| state | 4 | Check before using host_ptr |

### ModelMetadata

Architecture information extracted from model.

```cpp
struct ModelMetadata {
    ModelArch arch_type;                    // Model family
    uint32_t vocab_size;                    // Token vocabulary size
    uint32_t context_length;                // Max sequence length
    uint32_t embedding_length;              // Hidden dimension (d_model)
    uint32_t block_count;                   // Number of transformer blocks
    uint32_t feed_forward_length;           // MLP hidden size (d_ff)
    uint32_t attention_head_count;          // Number of query heads
    uint32_t attention_head_count_kv;       // Number of KV heads (MQA/GQA)
    uint32_t rope_freq_base;                // RoPE frequency base (default 10000)
    uint32_t rope_dim_count;                // RoPE dimensions
};
```

**Example:**
```cpp
auto* meta = loader->GetModelMetadata();
printf("Model: %s %uB\n", 
       ArchToString(meta->arch_type),
       meta->embedding_length * meta->block_count / 1000000000);
// Output: Model: llama 7B
```

---

## Main API Class

### ModelLoader

Primary interface for model loading.

#### Static Creation

```cpp
static ModelLoader* Create(void* phase1_ctx);
```

**Purpose:** Initialize model loader with Phase-1 context.

**Parameters:**
- `phase1_ctx` - Phase-1 Foundation context (from `Phase1::Foundation::GetInstance()`)

**Returns:** Pointer to ModelLoader, or `nullptr` on failure

**Example:**
```cpp
auto* phase1 = Phase1::Foundation::GetInstance();
auto* loader = Phase2::ModelLoader::Create(phase1->GetNativeContext());
if (!loader) {
    printf("Failed to create loader\n");
    return 1;
}
```

#### Destroy

```cpp
void Destroy();
```

**Purpose:** Clean up loader and free resources.

**Note:** Automatically called by destructor in C++ context.

---

#### DetectFormat

```cpp
FormatType DetectFormat(const char* path);
```

**Purpose:** Determine model format from file signature.

**Parameters:**
- `path` - File path to check

**Returns:** FormatType enum value

**Performance:** ~50µs (includes file open)

**Example:**
```cpp
auto fmt = loader->DetectFormat("model.gguf");
printf("Format: %s\n", fmt == FormatType::GGUF ? "GGUF" : "Unknown");
```

---

#### LoadModel

```cpp
bool LoadModel(const char* source, uint32_t flags = 0);
```

**Purpose:** Load model from source (local file, URL, etc.)

**Parameters:**
- `source` - Local path or URL
  - `/path/model.gguf` - Local GGUF file
  - `hf://org/model-name` - HuggingFace Hub
  - `http://...` - Direct HTTP URL
  - `ollama://model-name` - Ollama API
  - `MASM://blob-name` - Embedded model

- `flags` - Bitwise OR of LoadFlags:
  - `0` - Default (local file, full load)
  - `STREAMING` - Background prefetch
  - `MMAP` - Memory-map file
  - `VERIFY` - Checksum verification

**Returns:** `true` on success, `false` on failure

**Side Effects:**
- Allocates memory for tensors
- Opens file and reads data
- May spawn background threads (streaming)

**Example:**
```cpp
// Simple load
if (!loader->LoadModel("models/llama-7b.gguf")) {
    printf("Error: %s\n", loader->GetLastError());
}

// Streaming load
if (!loader->LoadModel("models/llama-800b.gguf", 
                       (uint32_t)LoadFlags::STREAMING)) {
    printf("Streaming setup failed\n");
}

// Memory-mapped
if (!loader->LoadModel("models/llama-7b.gguf",
                       (uint32_t)LoadFlags::MMAP)) {
    printf("Mmap failed\n");
}
```

---

#### LoadModelWithProgress

```cpp
bool LoadModelWithProgress(const char* source, uint32_t flags,
                          ProgressCallback progress_cb, void* progress_ctx);
```

**Purpose:** Load model with progress reporting.

**Parameters:**
- `source` - Model source (same as LoadModel)
- `flags` - Load flags
- `progress_cb` - Callback function: `void(*)(void* ctx, uint64_t bytes, uint64_t total, uint32_t pct)`
- `progress_ctx` - Context passed to callback

**Callback Frequency:** ~100ms intervals or per 10MB loaded

**Example:**
```cpp
void OnProgress(void* ctx, uint64_t bytes_loaded, uint64_t total, uint32_t pct) {
    printf("\r[%3d%%] %llu / %llu MB", pct, 
           bytes_loaded/(1024*1024), total/(1024*1024));
    fflush(stdout);
}

loader->LoadModelWithProgress("model.gguf", 0, OnProgress, nullptr);
printf("\n");
```

---

#### GetTensor

```cpp
TensorMetadata* GetTensor(const char* name);
```

**Purpose:** Get tensor metadata by name (fast hash lookup).

**Parameters:**
- `name` - Exact tensor name

**Returns:** Pointer to TensorMetadata, or `nullptr` if not found

**Performance:** O(1), ~20ns average

**Note:** Returned pointer is valid for lifetime of loader

**Example:**
```cpp
auto* q_proj = loader->GetTensor("layers.0.attention.w_q");
if (q_proj) {
    printf("Q_proj shape: [%llu, %llu]\n", 
           q_proj->dims[0], q_proj->dims[1]);
    printf("State: %d (0=unloaded, 2=loaded)\n", (int)q_proj->state);
}
```

---

#### GetTensorByIndex

```cpp
TensorMetadata* GetTensorByIndex(uint64_t index);
```

**Purpose:** Get tensor by sequential index.

**Parameters:**
- `index` - Index from 0 to GetTensorCount()-1

**Returns:** Pointer to TensorMetadata

**Performance:** O(1), ~10ns

---

#### GetTensorData

```cpp
void* GetTensorData(const char* name);
```

**Purpose:** Get pointer to tensor data for immediate use.

**Parameters:**
- `name` - Tensor name

**Returns:** Pointer to tensor data, or `nullptr` if:
- Tensor not found
- Tensor not loaded yet (check GetTensor()->state)

**Warning:** Pointer may become invalid if tensor is evicted

**Example:**
```cpp
auto* data = loader->GetTensorData("layers.0.attn.w_q");
if (data) {
    float* fp32_data = (float*)data;  // For Q4 models, cast cautiously
    printf("First value: %f\n", fp32_data[0]);
}
```

---

#### GetModelMetadata

```cpp
ModelMetadata* GetModelMetadata();
```

**Purpose:** Get extracted model architecture information.

**Returns:** Pointer to ModelMetadata struct

**Example:**
```cpp
auto* meta = loader->GetModelMetadata();
printf("Vocab: %u, Context: %u, Hidden: %u\n",
       meta->vocab_size, meta->context_length, meta->embedding_length);
```

---

#### GetRouterType

```cpp
RouterType GetRouterType() const;
```

**Purpose:** Query which loading strategy was used.

**Returns:** RouterType enum

**Example:**
```cpp
if (loader->GetRouterType() == RouterType::GGUF_MMAP) {
    printf("Using memory-mapped I/O\n");
}
```

---

#### GetFormatType

```cpp
FormatType GetFormatType() const;
```

**Purpose:** Query model format.

**Returns:** FormatType enum

---

#### GetTensorCount

```cpp
uint64_t GetTensorCount() const;
```

**Purpose:** Get total number of tensors in model.

**Returns:** Number of tensors (e.g., 291 for 7B Llama)

**Example:**
```cpp
for (uint64_t i = 0; i < loader->GetTensorCount(); ++i) {
    auto* tensor = loader->GetTensorByIndex(i);
    printf("%llu: %s\n", i, tensor->name);
}
```

---

#### GetBytesLoaded

```cpp
uint64_t GetBytesLoaded() const;
```

**Purpose:** Get bytes currently in memory.

**Returns:** Bytes in RAM

**Example:**
```cpp
printf("Memory used: %.2f MB\n", loader->GetBytesLoaded() / (1024.0*1024.0));
```

---

#### GetTotalSize

```cpp
uint64_t GetTotalSize() const;
```

**Purpose:** Get total model size (file size).

**Returns:** Bytes

**Example:**
```cpp
auto total_mb = loader->GetTotalSize() / (1024.0*1024.0);
auto loaded_mb = loader->GetBytesLoaded() / (1024.0*1024.0);
printf("%.1f%% loaded\n", 100.0 * loaded_mb / total_mb);
```

---

#### IsTensorLoaded

```cpp
bool IsTensorLoaded(const char* name);
```

**Purpose:** Check if tensor is ready to use.

**Parameters:**
- `name` - Tensor name

**Returns:** `true` if in LOADED state

**Example:**
```cpp
if (loader->IsTensorLoaded("layers.0.attn.w_q")) {
    auto* data = loader->GetTensorData("layers.0.attn.w_q");
    // Use data
} else {
    printf("Tensor not loaded yet\n");
}
```

---

#### PrefetchTensor

```cpp
bool PrefetchTensor(const char* name);
```

**Purpose:** Request tensor be loaded (streaming mode).

**Parameters:**
- `name` - Tensor name

**Returns:** `true` if prefetch queued

**Effect:** Background thread will prioritize this tensor

**Example:**
```cpp
// Load layer 0, then prefetch layer 1 while processing layer 0
loader->PrefetchTensor("layers.1.attn.w_q");
```

---

#### EvictTensor

```cpp
void EvictTensor(const char* name);
```

**Purpose:** Remove tensor from memory to free space.

**Parameters:**
- `name` - Tensor name

**Effect:** Tensor state → EVICTED, memory freed

**Warning:** Using evicted tensor will cause error

**Example:**
```cpp
if (loader->GetBytesLoaded() > max_memory) {
    // Evict old layers
    for (int i = 0; i < 10; ++i) {
        auto name = std::string("layers.") + std::to_string(i) + ".attn.w_q";
        loader->EvictTensor(name.c_str());
    }
}
```

---

#### VerifyChecksum

```cpp
bool VerifyChecksum();
```

**Purpose:** Verify model integrity (SHA-256).

**Returns:** `true` if checksum matches

**Note:** Requires VERIFY flag to have been set during load

**Example:**
```cpp
if (loader->VerifyChecksum()) {
    printf("Model integrity verified\n");
} else {
    printf("ERROR: Model corrupted!\n");
}
```

---

#### GetLastError

```cpp
const char* GetLastError() const;
```

**Purpose:** Get error message from failed operation.

**Returns:** C string (valid for lifetime of loader)

**Example:**
```cpp
if (!loader->LoadModel("model.gguf")) {
    printf("Error: %s\n", loader->GetLastError());
}
```

---

#### GetNativeContext

```cpp
void* GetNativeContext();
```

**Purpose:** Get opaque context pointer for C interop.

**Returns:** `MODEL_LOADER_CONTEXT*` (cast to void*)

**Use Case:** Calling C functions directly

---

## Callbacks

### ProgressCallback

```cpp
using ProgressCallback = void(*)(
    void* ctx,                  // User context
    uint64_t bytes_loaded,      // Bytes in memory
    uint64_t total_bytes,       // Total file size
    uint32_t percentage         // 0-100
);
```

**Called:** Approximately every 10MB or 100ms

**Example:**
```cpp
void MyProgress(void* ctx, uint64_t bytes, uint64_t total, uint32_t pct) {
    auto* ui = (MyUI*)ctx;
    ui->SetProgress(pct);
    ui->SetLabel(std::format("Loading {}%", pct));
}

loader->LoadModelWithProgress(
    "model.gguf", 
    0,
    MyProgress,
    my_ui_instance
);
```

---

## Utility Functions

### FormatTypeToString

```cpp
const char* FormatTypeToString(FormatType type);
```

**Returns:** Human-readable format name

---

### RouterTypeToString

```cpp
const char* RouterTypeToString(RouterType type);
```

**Returns:** Router name (e.g., "GGUF (Local)", "HuggingFace Hub")

---

### GGMLTypeToString

```cpp
const char* GGMLTypeToString(GGMLType type);
```

**Returns:** Quantization type name (e.g., "Q4_K_M")

---

### GetQuantizationRatio

```cpp
double GetQuantizationRatio(GGMLType type);
```

**Returns:** Compression ratio vs F32 (e.g., 0.125 for IQ2_XXS)

---

### CalculateTensorSize

```cpp
uint64_t CalculateTensorSize(uint64_t n_elements, GGMLType type);
```

**Purpose:** Calculate memory size for tensor with quantization.

**Returns:** Bytes needed on disk

**Example:**
```cpp
// 7B model embeddings: 32000 vocab * 4096 hidden = 131M elements
uint64_t emb_size = CalculateTensorSize(131000000, GGMLType::Q4_K);
printf("Embeddings: %.1f MB\n", emb_size / (1024.0*1024.0));
// Output: Embeddings: 49.2 MB
```

---

## C Interop Functions

For C consumers (no C++ features):

### CreatePhase2Loader

```c
void* __stdcall CreatePhase2Loader(void* phase1_ctx);
```

---

### DestroyPhase2Loader

```c
void __stdcall DestroyPhase2Loader(void* loader);
```

---

### LoadModelC

```c
uint32_t __stdcall LoadModelC(void* loader, const char* source, uint32_t flags);
```

**Returns:** 1 on success, 0 on failure

---

### GetTensorDataC

```c
void* __stdcall GetTensorDataC(void* loader, const char* name);
```

---

### GetTensorCountC

```c
uint64_t __stdcall GetTensorCountC(void* loader);
```

---

## Error Handling

### Error Codes

All boolean-returning functions use `false` to indicate failure.

Error messages are queried via `GetLastError()`.

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Model file not found" | Path doesn't exist | Check path, verify file permissions |
| "Unknown model format" | Magic bytes don't match | Verify file isn't corrupted |
| "Memory allocation failed" | Not enough RAM | Use streaming or mmap mode |
| "Tensor count exceeds maximum" | >10000 tensors | Increase MAX_TENSORS in source |
| "Checksum verification failed" | Data corrupted | Re-download model |
| "Network operation failed" | Connection error | Check internet, verify URL |

### Exception-Safe Programming

Phase 2 doesn't throw exceptions. Use return values:

```cpp
auto* loader = Phase2::ModelLoader::Create(phase1_ctx);
if (!loader) {
    printf("Init failed\n");
    return;
}

if (!loader->LoadModel("model.gguf")) {
    printf("Load failed: %s\n", loader->GetLastError());
    loader->Destroy();
    return;
}

// Success
loader->Destroy();
```

---

## Examples

### Example 1: Complete Loading Workflow

```cpp
#include "Phase1_Foundation.h"
#include "Phase2_Foundation.h"

int main() {
    // Initialize Phase 1
    auto* phase1 = Phase1::Foundation::GetInstance();
    if (!phase1->Initialize()) {
        printf("Phase 1 init failed\n");
        return 1;
    }
    
    // Create loader
    auto* loader = Phase2::ModelLoader::Create(phase1->GetNativeContext());
    if (!loader) {
        printf("Loader creation failed\n");
        return 1;
    }
    
    // Load model
    printf("Loading Llama 2 7B...\n");
    if (!loader->LoadModel("models/llama-2-7b-q4_k.gguf")) {
        printf("Error: %s\n", loader->GetLastError());
        loader->Destroy();
        return 1;
    }
    
    // Query model info
    auto* meta = loader->GetModelMetadata();
    printf("Architecture: Llama %uB\n", meta->embedding_length * meta->block_count / 1000000000);
    printf("Tensors: %llu\n", loader->GetTensorCount());
    printf("Memory: %.1f MB\n", loader->GetBytesLoaded() / (1024*1024.0));
    
    // Access specific tensor
    auto* q_proj = loader->GetTensor("layers.0.attention.w_q");
    if (q_proj && q_proj->state == Phase2::TensorState::LOADED) {
        printf("Q_proj dims: [%llu, %llu]\n", q_proj->dims[0], q_proj->dims[1]);
    }
    
    // Cleanup
    loader->Destroy();
    return 0;
}
```

### Example 2: Streaming Large Model

```cpp
// Load 800B model with streaming
auto flags = (uint32_t)Phase2::LoadFlags::STREAMING;
loader->LoadModel("models/llama-800b-q4_k.gguf", flags);

// Process layers sequentially
for (int layer = 0; layer < 120; ++layer) {
    // Prefetch next layer while processing current
    if (layer < 119) {
        loader->PrefetchTensor(
            std::format("layers.{}.attn.w_q", layer + 1).c_str()
        );
    }
    
    // Get current layer tensors
    auto* w_q = loader->GetTensorData(
        std::format("layers.{}.attn.w_q", layer).c_str()
    );
    
    if (w_q && loader->IsTensorLoaded(
        std::format("layers.{}.attn.w_q", layer).c_str())) {
        // Process layer
        ProcessLayer(w_q, layer);
    }
}
```

### Example 3: Progress Reporting

```cpp
void ShowProgress(void* ctx, uint64_t bytes, uint64_t total, uint32_t pct) {
    auto bars = pct / 5;
    printf("[");
    for (int i = 0; i < bars; ++i) printf("=");
    for (int i = bars; i < 20; ++i) printf(" ");
    printf("] %3d%% (%llu MB / %llu MB)\n", 
           pct, bytes/(1024*1024), total/(1024*1024));
}

loader->LoadModelWithProgress("model.gguf", 0, ShowProgress, nullptr);
```

---

## See Also

- `PHASE2_ARCHITECTURE.md` - Design and concepts
- `PHASE2_BUILD_GUIDE.md` - Build instructions
- `Phase1_Foundation.h` - Phase 1 API (required dependency)
- `Phase4_SwarmInference.h` - How Phase 4 uses Phase 2
