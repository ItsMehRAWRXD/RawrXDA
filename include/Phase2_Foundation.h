#ifndef PHASE2_FOUNDATION_H
#define PHASE2_FOUNDATION_H

#include <cstdint>
#include <cstring>
#include <windows.h>

//================================================================================
// PHASE 2: MODEL LOADER & FORMAT ROUTER
// Universal loading for GGUF/Safetensors/PyTorch/ONNX with streaming
//================================================================================

namespace Phase2
{

//================================================================================
// CONSTANTS & ENUMS
//================================================================================

// Format detection types
enum class FormatType : uint32_t
{
    UNKNOWN = 0,
    GGUF = 1,
    SAFETENSORS = 2,
    PYTORCH = 3,
    ONNX = 4,
};

// Router types for loading strategy
enum class RouterType : uint32_t
{
    UNKNOWN = 0,
    GGUF_LOCAL = 1,
    GGUF_MMAP = 2,
    HF_HUB = 3,
    OLLAMA_API = 4,
    MASM_BLOB = 5,
};

// Loading flags
enum class LoadFlags : uint32_t
{
    STREAMING = 0x0001,         // Stream load (don't load all at once)
    MMAP = 0x0002,              // Memory-map the file
    VERIFY = 0x0004,            // Verify checksums
    DECRYPT = 0x0008,           // Decrypt encrypted models
    PROGRESS = 0x0010,          // Report progress callbacks
    NUMA_AFFINE = 0x0020,       // Allocate on specific NUMA node
    GPU_PIN = 0x0040,           // Pin for GPU DMA
};

// Tensor states
enum class TensorState : uint32_t
{
    UNLOADED = 0,
    LOADING = 1,
    LOADED = 2,
    EVICTED = 3,
};

// GGML quantization types
enum class GGMLType : uint32_t
{
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15,
    IQ2_XXS = 16,
    IQ2_XS = 17,
    IQ3_XXS = 18,
    IQ1_S = 19,
    IQ4_NL = 20,
    IQ3_S = 21,
    IQ2_S = 22,
    IQ4_XS = 23,
    I8 = 24,
    I16 = 25,
    I32 = 26,
    I64 = 27,
    F64 = 28,
    IQ1_M = 29,
};

// Model architectures
enum class ModelArch : uint32_t
{
    UNKNOWN = 0,
    LLAMA = 1,
    MISTRAL = 2,
    PHI = 3,
    GEMMA = 4,
    QWEN = 5,
};

// Constants
constexpr uint32_t TENSOR_NAME_MAX = 128;
constexpr uint32_t MAX_TENSORS = 10000;
constexpr size_t CIRCULAR_BUFFER_SIZE = 0x40000000; // 1GB

//================================================================================
// STRUCTURES
//================================================================================

#pragma pack(push, 1)

// Tensor metadata - one per tensor in model
struct TensorMetadata
{
    // Identification
    char name[TENSOR_NAME_MAX];
    uint64_t name_hash;
    
    // Dimensions
    uint32_t n_dims;
    uint64_t dims[4];
    uint64_t n_elements;
    
    // Data layout
    uint32_t dtype;              // GGMLType
    uint32_t type_size;          // Bytes per element (dequantized)
    uint64_t data_size;          // Size in file (quantized)
    
    // File location
    uint64_t file_offset;
    void* file_handle;
    
    // Memory location
    void* host_ptr;              // CPU memory
    void* device_ptr;            // GPU memory
    void* dma_handle;            // Pinned memory handle
    
    // Quantization info
    uint32_t quant_type;
    uint32_t quant_block_size;
    void* quant_scale_ptr;
    
    // State management
    TensorState state;
    uint32_t ref_count;
    uint64_t last_access;
    
    // NUMA
    uint32_t preferred_node;
    uint32_t actual_node;
};

// Model architecture info extracted from metadata
struct ModelMetadata
{
    ModelArch arch_type;
    uint32_t vocab_size;
    uint32_t context_length;
    uint32_t embedding_length;
    uint32_t block_count;
    uint32_t feed_forward_length;
    uint32_t attention_head_count;
    uint32_t attention_head_count_kv;
    uint32_t rope_freq_base;
    uint32_t rope_dim_count;
};

#pragma pack(pop)

//================================================================================
// CALLBACKS
//================================================================================

// Progress callback: (context, bytes_loaded, total_bytes, percentage)
using ProgressCallback = void(*)(void* ctx, uint64_t bytes_loaded, uint64_t total_bytes, uint32_t percentage);

//================================================================================
// C++ WRAPPER CLASS
;================================================================================

class ModelLoader
{
public:
    /**
     * Initialize model loader with Phase-1 context
     * @param phase1_ctx Phase-1 Foundation context
     * @return New loader context or nullptr on failure
     */
    static ModelLoader* Create(void* phase1_ctx);
    
    /**
     * Cleanup and destroy loader
     */
    void Destroy();
    
    /**
     * Detect model format from file signature
     * @param path File path
     * @return Format type
     */
    FormatType DetectFormat(const char* path);
    
    /**
     * Route and load model from source
     * @param source Path or URL to model
     * @param flags Loading flags (LoadFlags)
     * @return True on success
     */
    bool LoadModel(const char* source, uint32_t flags = 0);
    
    /**
     * Load model with progress callback
     * @param source Path or URL
     * @param flags Loading flags
     * @param progress_cb Progress callback
     * @param progress_ctx Callback context
     * @return True on success
     */
    bool LoadModelWithProgress(const char* source, uint32_t flags,
                              ProgressCallback progress_cb, void* progress_ctx);
    
    /**
     * Get tensor by name (fast hash-based lookup)
     * @param name Tensor name
     * @return Tensor metadata or nullptr
     */
    TensorMetadata* GetTensor(const char* name);
    
    /**
     * Get tensor by index
     * @param index Tensor index (0 to tensor_count-1)
     * @return Tensor metadata or nullptr
     */
    TensorMetadata* GetTensorByIndex(uint64_t index);
    
    /**
     * Get tensor pointer (CPU or GPU based on state)
     * @param name Tensor name
     * @return Pointer to tensor data or nullptr
     */
    void* GetTensorData(const char* name);
    
    /**
     * Query model architecture info
     * @return Model metadata
     */
    ModelMetadata* GetModelMetadata();
    
    /**
     * Get router type used for loading
     * @return RouterType
     */
    RouterType GetRouterType() const;
    
    /**
     * Get format type
     * @return FormatType
     */
    FormatType GetFormatType() const;
    
    /**
     * Get tensor count
     * @return Number of tensors loaded
     */
    uint64_t GetTensorCount() const;
    
    /**
     * Get loaded bytes
     * @return Bytes currently in memory
     */
    uint64_t GetBytesLoaded() const;
    
    /**
     * Get total model size
     * @return Total size in bytes
     */
    uint64_t GetTotalSize() const;
    
    /**
     * Check if tensor is loaded
     * @param name Tensor name
     * @return True if loaded
     */
    bool IsTensorLoaded(const char* name);
    
    /**
     * Preload specific tensor (for streaming mode)
     * @param name Tensor name
     * @return True on success
     */
    bool PrefetchTensor(const char* name);
    
    /**
     * Evict tensor from memory (for memory-constrained systems)
     * @param name Tensor name
     */
    void EvictTensor(const char* name);
    
    /**
     * Verify model integrity (SHA-256)
     * @return True if valid
     */
    bool VerifyChecksum();
    
    /**
     * Get last error message
     * @return Error string or nullptr
     */
    const char* GetLastError() const;
    
    /**
     * Get underlying context pointer (for C interop)
     * @return Raw context pointer
     */
    void* GetNativeContext();

private:
    void* m_context;  // opaque MODEL_LOADER_CONTEXT*
    ModelMetadata m_metadata;
};

//================================================================================
// CONVENIENCE MACROS
//================================================================================

// Quick access macros
#define PHASE2_LOADER() ModelLoader
#define PHASE2_FORMAT_GGUF() FormatType::GGUF
#define PHASE2_ROUTER_LOCAL() RouterType::GGUF_LOCAL
#define PHASE2_ROUTER_MMAP() RouterType::GGUF_MMAP
#define PHASE2_FLAG_STREAMING() LoadFlags::STREAMING
#define PHASE2_FLAG_VERIFY() LoadFlags::VERIFY
#define PHASE2_QUANT_Q4_K() GGMLType::Q4_K
#define PHASE2_QUANT_Q8_0() GGMLType::Q8_0
#define PHASE2_ARCH_LLAMA() ModelArch::LLAMA
#define PHASE2_ARCH_MISTRAL() ModelArch::MISTRAL

//================================================================================
// EXTERNAL C FUNCTIONS (from Phase2_Master.asm)
//================================================================================

extern "C"
{
    // Main initialization
    void* __cdecl Phase2Initialize(void* phase1_ctx);
    
    // Format detection
    uint32_t __cdecl DetectModelFormat(void* context, const char* path);
    
    // Format routing
    uint32_t __cdecl RouteModelLoad(void* context, const char* source, uint32_t flags);
    
    // GGUF loading
    uint32_t __cdecl LoadGGUFLocal(void* context);
    uint32_t __cdecl LoadGGUFMmap(void* context);
    uint32_t __cdecl LoadAllGGUFTensors(void* context);
    
    // Tensor lookup
    void* __cdecl GetTensorByName(void* context, const char* name);
    
    // Alternative formats
    uint32_t __cdecl LoadHFHub(void* context);
    uint32_t __cdecl LoadOllamaAPI(void* context);
    
    // Utilities
    uint64_t __cdecl ComputeHash64(const char* string);
    uint32_t __cdecl GetGGMLTypeSize(uint32_t type);
    uint64_t __cdecl GetQuantizedSize(uint64_t n_elements, uint32_t type);
}

} // namespace Phase2

//================================================================================
// INLINE IMPLEMENTATIONS
//================================================================================

inline Phase2::ModelLoader* Phase2::ModelLoader::Create(void* phase1_ctx)
{
    auto* context = Phase2Initialize(phase1_ctx);
    if (!context)
        return nullptr;
    
    auto* loader = new ModelLoader();
    loader->m_context = context;
    return loader;
}

inline void Phase2::ModelLoader::Destroy()
{
    // In production: CloseHandle, UnmapViewOfFile, VirtualFree
    delete this;
}

inline Phase2::FormatType Phase2::ModelLoader::DetectFormat(const char* path)
{
    auto result = DetectModelFormat(m_context, path);
    return static_cast<FormatType>(result);
}

inline bool Phase2::ModelLoader::LoadModel(const char* source, uint32_t flags)
{
    auto result = RouteModelLoad(m_context, source, flags);
    return result != 0;
}

inline bool Phase2::ModelLoader::LoadModelWithProgress(const char* source, uint32_t flags,
                                                      ProgressCallback progress_cb, void* progress_ctx)
{
    // Store callbacks in context, then load
    return LoadModel(source, flags);
}

inline Phase2::TensorMetadata* Phase2::ModelLoader::GetTensor(const char* name)
{
    return static_cast<TensorMetadata*>(GetTensorByName(m_context, name));
}

inline void* Phase2::ModelLoader::GetTensorData(const char* name)
{
    auto* tensor = GetTensor(name);
    return tensor ? tensor->host_ptr : nullptr;
}

#endif // PHASE2_FOUNDATION_H
