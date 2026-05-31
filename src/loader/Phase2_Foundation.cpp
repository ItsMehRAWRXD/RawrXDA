#include "..\include\Phase2_Foundation.h"
#include <cstdio>
#include <cstdlib>

//================================================================================
// PHASE 2: MODEL LOADER - C++ IMPLEMENTATION
// Wrappers around Phase2_Master.asm assembly functions
//================================================================================

namespace Phase2
{

//================================================================================
// INTERNAL HELPERS
//================================================================================

namespace Internal
{

/**
 * Forward declaration - Phase1 context structure
 * From Phase1_Foundation.h
 */
struct Phase1Context
{
    void* reserved;
};

} // namespace Internal

//================================================================================
// ModelLoader IMPLEMENTATION
//================================================================================

/**
 * Get tensor by index
 */
TensorMetadata* ModelLoader::GetTensorByIndex(uint64_t index)
{
    if (index >= GetTensorCount())
        return nullptr;
    
    // Access tensor table from context
    // In production: Would access internal context structure
    return nullptr;
}

/**
 * Query model architecture info
 */
ModelMetadata* ModelLoader::GetModelMetadata()
{
    return &m_metadata;
}

/**
 * Get router type used for loading
 */
RouterType ModelLoader::GetRouterType() const
{
    // Would extract from internal context
    return RouterType::UNKNOWN;
}

/**
 * Get format type
 */
FormatType ModelLoader::GetFormatType() const
{
    // Would extract from internal context
    return FormatType::UNKNOWN;
}

/**
 * Get tensor count (stub - would be in context)
 */
uint64_t ModelLoader::GetTensorCount() const
{
    // Production: return ((MODEL_LOADER_CONTEXT*)m_context)->tensor_count;
    return 0;
}

/**
 * Get loaded bytes
 */
uint64_t ModelLoader::GetBytesLoaded() const
{
    // Production: return ((MODEL_LOADER_CONTEXT*)m_context)->bytes_loaded;
    return 0;
}

/**
 * Get total model size
 */
uint64_t ModelLoader::GetTotalSize() const
{
    // Production: return ((MODEL_LOADER_CONTEXT*)m_context)->file_size;
    return 0;
}

/**
 * Check if tensor is loaded
 */
bool ModelLoader::IsTensorLoaded(const char* name)
{
    auto* tensor = GetTensor(name);
    if (!tensor)
        return false;
    
    return tensor->state == TensorState::LOADED;
}

/**
 * Preload specific tensor
 */
bool ModelLoader::PrefetchTensor(const char* name)
{
    auto* tensor = GetTensor(name);
    if (!tensor || tensor->state == TensorState::LOADED)
        return false;
    
    // In production: Would trigger async loading
    return true;
}

/**
 * Evict tensor from memory
 */
void ModelLoader::EvictTensor(const char* name)
{
    auto* tensor = GetTensor(name);
    if (!tensor)
        return;
    
    // Mark for eviction, notify GPU if necessary
    tensor->state = TensorState::EVICTED;
}

/**
 * Verify model integrity
 */
bool ModelLoader::VerifyChecksum()
{
    // Would call assembly SHA-256 verification
    return true;
}

/**
 * Get last error message
 */
const char* ModelLoader::GetLastError() const
{
    // Production: return ((MODEL_LOADER_CONTEXT*)m_context)->last_error_string;
    return "No error";
}

/**
 * Get underlying context pointer
 */
void* ModelLoader::GetNativeContext()
{
    return m_context;
}

//================================================================================
// UTILITY FUNCTIONS
//================================================================================

/**
 * Get string representation of format type
 */
const char* FormatTypeToString(FormatType type)
{
    switch (type)
    {
    case FormatType::GGUF: return "GGUF";
    case FormatType::SAFETENSORS: return "Safetensors";
    case FormatType::PYTORCH: return "PyTorch";
    case FormatType::ONNX: return "ONNX";
    default: return "Unknown";
    }
}

/**
 * Get string representation of router type
 */
const char* RouterTypeToString(RouterType type)
{
    switch (type)
    {
    case RouterType::GGUF_LOCAL: return "GGUF (Local)";
    case RouterType::GGUF_MMAP: return "GGUF (MMap)";
    case RouterType::HF_HUB: return "HuggingFace Hub";
    case RouterType::OLLAMA_API: return "Ollama API";
    case RouterType::MASM_BLOB: return "MASM Blob";
    default: return "Unknown";
    }
}

/**
 * Get string representation of quantization type
 */
const char* GGMLTypeToString(GGMLType type)
{
    switch (type)
    {
    case GGMLType::F32: return "F32";
    case GGMLType::F16: return "F16";
    case GGMLType::Q4_0: return "Q4_0";
    case GGMLType::Q4_1: return "Q4_1";
    case GGMLType::Q4_K: return "Q4_K_M";
    case GGMLType::Q8_0: return "Q8_0";
    case GGMLType::Q8_K: return "Q8_K";
    case GGMLType::IQ2_XXS: return "IQ2_XXS";
    case GGMLType::IQ3_XXS: return "IQ3_XXS";
    default: return "Unknown";
    }
}

/**
 * Get size multiplier for quantization type
 */
double GetQuantizationRatio(GGMLType type)
{
    switch (type)
    {
    case GGMLType::F32: return 1.0;
    case GGMLType::F16: return 0.5;
    case GGMLType::Q4_0: return 0.5625;     // 18 bytes / 32 elements
    case GGMLType::Q4_1: return 0.625;      // 20 bytes / 32 elements
    case GGMLType::Q4_K: return 0.375;      // ~6 bytes / 16 elements
    case GGMLType::Q8_0: return 1.0625;     // 34 bytes / 32 elements
    case GGMLType::Q8_K: return 1.125;      // 36 bytes / 32 elements
    case GGMLType::IQ2_XXS: return 0.25;    // Extremely aggressive
    default: return 1.0;
    }
}

/**
 * Calculate total tensor size with quantization
 */
uint64_t CalculateTensorSize(uint64_t n_elements, GGMLType type)
{
    uint32_t base_size = 4; // Assume float32
    
    switch (type)
    {
    case GGMLType::F32:
        return n_elements * 4;
    case GGMLType::F16:
        return n_elements * 2;
    case GGMLType::Q4_0:
        // 4 bits per element + 2 bytes scale per 32 elements = 18/32 = 0.5625
        return (n_elements * 18) / 32;
    case GGMLType::Q4_1:
        // 4 bits per element + 4 bytes per 32 elements = 20/32 = 0.625
        return (n_elements * 20) / 32;
    case GGMLType::Q4_K:
        // Approximate: ~375 bytes per 1024 elements
        return (n_elements * 375) / 1024;
    case GGMLType::Q8_0:
        // 8 bits per element + 2 bytes scale per 32 elements = 34/32 = 1.0625
        return (n_elements * 34) / 32;
    case GGMLType::Q8_K:
        // 8 bits per element + 4 bytes per 32 elements = 36/32 = 1.125
        return (n_elements * 36) / 32;
    case GGMLType::IQ2_XXS:
        // Extreme compression: ~256 bytes per 1024 elements
        return (n_elements * 256) / 1024;
    default:
        return n_elements * base_size;
    }
}

} // namespace Phase2

//================================================================================
// C WRAPPER FUNCTIONS FOR TESTING
//================================================================================

extern "C"
{

/**
 * Create Phase 2 loader from C
 */
void* __stdcall CreatePhase2Loader(void* phase1_ctx)
{
    return Phase2::ModelLoader::Create(phase1_ctx);
}

/**
 * Destroy Phase 2 loader
 */
void __stdcall DestroyPhase2Loader(void* loader)
{
    auto* ml = static_cast<Phase2::ModelLoader*>(loader);
    if (ml)
        ml->Destroy();
}

/**
 * Load model from C
 */
uint32_t __stdcall LoadModelC(void* loader, const char* source, uint32_t flags)
{
    auto* ml = static_cast<Phase2::ModelLoader*>(loader);
    if (!ml)
        return 0;
    
    return ml->LoadModel(source, flags) ? 1 : 0;
}

/**
 * Get tensor data from C
 */
void* __stdcall GetTensorDataC(void* loader, const char* name)
{
    auto* ml = static_cast<Phase2::ModelLoader*>(loader);
    if (!ml)
        return nullptr;
    
    return ml->GetTensorData(name);
}

/**
 * Get tensor count from C
 */
uint64_t __stdcall GetTensorCountC(void* loader)
{
    auto* ml = static_cast<Phase2::ModelLoader*>(loader);
    if (!ml)
        return 0;
    
    return ml->GetTensorCount();
}

} // extern "C"
