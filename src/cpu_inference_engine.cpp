// ============================================================================
// cpu_inference_engine.cpp — CPUInferenceEngine implementation
// Delegates actual GGUF/inference work to RawrXDInference pipeline.
// Matches the monolithic class layout declared in cpu_inference_engine.h.
// ============================================================================
#include "cpu_inference_engine.h"
#include "rawrxd_inference.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>

namespace RawrXD {

// ============================================================================
// File-scope inference backend (the REAL compute chain)
// ============================================================================
static RawrXDInference s_inferenceBackend;

// ============================================================================
// Singleton
// ============================================================================
static CPUInferenceEngine* s_instance = nullptr;

CPUInferenceEngine* CPUInferenceEngine::getInstance() {
    if (!s_instance) s_instance = new CPUInferenceEngine();
    return s_instance;
}

// ============================================================================
// Lifecycle
// ============================================================================
CPUInferenceEngine::CPUInferenceEngine() = default;
CPUInferenceEngine::~CPUInferenceEngine() {
    ClearCache();
    if (m_hTitanDLL) {
        FreeLibrary(static_cast<HMODULE>(m_hTitanDLL));
        m_hTitanDLL = nullptr;
    }
}

// ============================================================================
// Model Loading — delegates to RawrXDInference::Initialize
// ============================================================================
bool CPUInferenceEngine::LoadModel(const std::string& model_path) {
    m_lastLoadErrorMessage.clear();
    if (model_path.empty()) {
        m_lastLoadErrorMessage = "empty model path";
        return false;
    }

    // UTF-8 → wchar_t for the loader
    std::wstring wpath;
    int len = MultiByteToWideChar(CP_UTF8, 0, model_path.c_str(), -1, nullptr, 0);
    if (len > 0) {
        wpath.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, model_path.c_str(), -1, &wpath[0], len);
        if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
    }

    // Locate tokenizer files alongside the model
    namespace fs = std::filesystem;
    fs::path modelDir = fs::path(model_path).parent_path();
    std::string vocabPath  = (modelDir / "tokenizer.json").string();
    std::string mergesPath = (modelDir / "merges.txt").string();

    // Fallback: check current directory
    if (!fs::exists(vocabPath))  vocabPath  = "tokenizer.json";
    if (!fs::exists(mergesPath)) mergesPath = "merges.txt";

    printf("[CPUInferenceEngine] Loading model: %s\n", model_path.c_str());

    if (s_inferenceBackend.Initialize(wpath.c_str(), vocabPath.c_str(), mergesPath.c_str())) {
        m_modelLoaded = true;
        m_lastLoadErrorMessage.clear();

        // Propagate metadata from backend to facade members
        int bvs = s_inferenceBackend.getVocabSize();
        int bdim = s_inferenceBackend.getDim();
        int blay = s_inferenceBackend.getLayers();
        int bhd = s_inferenceBackend.getHeads();
        m_vocabSize    = (bvs > 0)  ? bvs  : 32000;
        m_embeddingDim = (bdim > 0) ? bdim : 4096;
        m_numLayers    = (blay > 0) ? blay : 32;
        m_numHeads     = (bhd > 0)  ? bhd  : 32;

        // Try to load Titan ASM DLL if available
        if (m_useTitanAssembly && !m_hTitanDLL) {
            HMODULE hDll = LoadLibraryA("RawrXD_Titan.dll");
            if (hDll) {
                m_hTitanDLL = hDll;
                fnTitan_Initialize   = (FTitan_Initialize)GetProcAddress(hDll,   "Titan_Initialize");
                fnTitan_LoadModel    = (FTitan_LoadModel)GetProcAddress(hDll,    "Titan_LoadModel");
                fnTitan_RunInferenceStep = (FTitan_RunInferenceStep)GetProcAddress(hDll, "Titan_RunInferenceStep");

                if (fnTitan_Initialize) {
                    fnTitan_Initialize(&m_pTitanContext);
                    printf("[CPUInferenceEngine] Titan ASM engine loaded\n");
                }
                if (fnTitan_LoadModel && m_pTitanContext) {
                    fnTitan_LoadModel(m_pTitanContext, model_path.c_str());
                }
            }
        }

        printf("[CPUInferenceEngine] Model loaded successfully\n");
        return true;
    }

    m_lastLoadErrorMessage = s_inferenceBackend.GetLastLoadErrorMessage();
    if (m_lastLoadErrorMessage.empty()) {
        m_lastLoadErrorMessage = "RawrXDInference::Initialize returned false without detail";
    }
    printf("[CPUInferenceEngine] Failed to load model\n");
    return false;
}

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>& tensors) {
    m_weights = tensors;
    // Extract model dimensions from weight shapes if available
    auto it = m_weights.find("token_emb.weight");
    if (it != m_weights.end() && it->second.shape.size() >= 2) {
        m_vocabSize    = static_cast<int>(it->second.shape[0]);
        m_embeddingDim = static_cast<int>(it->second.shape[1]);
    }
    m_modelLoaded = true;
    return true;
}

// ============================================================================
// Tokenization — delegates to RawrXDInference → RawrXDTokenizer
// ============================================================================
std::vector<int> CPUInferenceEngine::Tokenize(const std::string& text) {
    if (!m_modelLoaded || text.empty()) return {};
    auto u32_toks = s_inferenceBackend.Tokenize(text);
    return std::vector<int>(u32_toks.begin(), u32_toks.end());
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int>& tokens) {
    if (!m_modelLoaded || tokens.empty()) return "";
    std::vector<uint32_t> u32_toks(tokens.begin(), tokens.end());
    return s_inferenceBackend.Detokenize(u32_toks);
}

// ============================================================================
// Inference — delegates to RawrXDInference::Generate
// ============================================================================
std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens) {
    if (!m_modelLoaded) return {};
    if (input_tokens.empty()) return {};

    std::vector<uint32_t> toks(input_tokens.begin(), input_tokens.end());
    auto logits = s_inferenceBackend.ForwardTokens(toks, static_cast<uint32_t>(m_currentPos));
    if (!logits.empty()) {
        m_lastState = logits;
    }
    return m_lastState;
}

void CPUInferenceEngine::GenerateStreaming(
    const std::vector<int32_t>& input_tokens,
    int max_tokens,
    std::function<void(const std::string&)> token_callback,
    std::function<void()> complete_callback,
    std::function<void(int32_t)> token_id_callback)
{
    if (!m_modelLoaded) {
        if (complete_callback) complete_callback();
        return;
    }
    if (max_tokens <= 0 || input_tokens.empty()) {
        if (complete_callback) complete_callback();
        return;
    }
    max_tokens = std::min(max_tokens, 8192);

    auto start = std::chrono::high_resolution_clock::now();
    m_currentPos = static_cast<int>(input_tokens.size());

    // Stream directly from token IDs to avoid detokenize->retokenize drift.
    std::vector<uint32_t> u32_toks(input_tokens.begin(), input_tokens.end());
    s_inferenceBackend.GenerateFromTokens(
        u32_toks, static_cast<uint32_t>(max_tokens),
        [&](uint32_t tok, const std::string& piece) {
            if (token_callback && !piece.empty()) token_callback(piece);
            if (token_id_callback) token_id_callback(static_cast<int32_t>(tok));
            m_currentPos++;
        });
    m_lastState = s_inferenceBackend.LastLogits();

    if (complete_callback) complete_callback();

    auto end = std::chrono::high_resolution_clock::now();
    m_inferenceCount++;
    m_totalInferenceTime += std::chrono::duration<double>(end - start).count();
}

// ============================================================================
// AI Mode setters
// ============================================================================
void CPUInferenceEngine::SetMaxMode(bool enabled) {
    m_maxMode = enabled;
}

void CPUInferenceEngine::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
}

void CPUInferenceEngine::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
}

// ============================================================================
// Context and memory management
// ============================================================================
void CPUInferenceEngine::SetContextSize(size_t size) {
    m_contextLimit = size;
}

size_t CPUInferenceEngine::GetMemoryUsage() const {
    return m_totalMemoryAllocated;
}

void CPUInferenceEngine::ClearCache() {
    m_kv_cache.clear();
    m_memoryPool.clear();
    m_totalMemoryAllocated = 0;
}

// ============================================================================
// Training stubs (weight updates)
// ============================================================================
void CPUInferenceEngine::UpdateWeights(
    const std::vector<std::vector<float>>& layer_gradients, float learning_rate) {
    // Training mode: apply gradient updates to layer weights
    // Not yet implemented — inference-only for now
}

void CPUInferenceEngine::UpdateOutputWeights(
    const std::vector<float>& gradients, float learningRate) {
    // Training mode: update output projection weights
    // Not yet implemented — inference-only for now
}

// ============================================================================
// Context & Memory Management
// ============================================================================
void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin) {
    m_memoryPlugins.push_back(std::move(plugin));
}

// ============================================================================
// Memory Allocation
// ============================================================================
float* CPUInferenceEngine::AllocateTensor(size_t size) {
    auto ptr = std::make_unique<float[]>(size);
    float* raw = ptr.get();
    m_totalMemoryAllocated += size * sizeof(float);
    m_memoryPool.push_back(std::move(ptr));
    return raw;
}

void CPUInferenceEngine::DeallocateTensor(float* ptr) {
    // Pool-managed — deallocated on ClearCache()
    (void)ptr;
}

// ============================================================================
// KV Cache
// ============================================================================
void CPUInferenceEngine::InitKVCache() {
    m_kv_cache.resize(m_numLayers);
    for (auto& layer : m_kv_cache) {
        layer.keys.resize(m_contextLimit * m_embeddingDim, 0.0f);
        layer.values.resize(m_contextLimit * m_embeddingDim, 0.0f);
    }
}

// ============================================================================
// Private Math Operations
// These are available for direct-call mode but the primary path goes through
// the RawrXDTransformer pipeline (which has its own optimized implementations).
// ============================================================================
void CPUInferenceEngine::MatMul(const float* A, const float* B, float* C, int m, int n, int k) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++) {
                sum += A[i * k + p] * B[p * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

void CPUInferenceEngine::ApplySoftmax(float* data, int size) {
    if (!data || size <= 0) return;

    float maxVal = *std::max_element(data, data + size);
    float sum = 0.0f;

    for (int i = 0; i < size; i++) {
        data[i] = std::exp(data[i] - maxVal);
        sum += data[i];
    }

    if (!(sum > 0.0f) || !std::isfinite(sum)) {
        const float uniform = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; i++) data[i] = uniform;
        return;
    }

    for (int i = 0; i < size; i++) {
        data[i] /= sum;
    }
}

void CPUInferenceEngine::LayerNorm(float* data, int size, float epsilon) {
    float mean = 0.0f;
    for (int i = 0; i < size; i++) mean += data[i];
    mean /= size;
    float var = 0.0f;
    for (int i = 0; i < size; i++) var += (data[i] - mean) * (data[i] - mean);
    var /= size;
    float inv = 1.0f / std::sqrt(var + epsilon);
    for (int i = 0; i < size; i++) data[i] = (data[i] - mean) * inv;
}

void CPUInferenceEngine::GELU(float* data, int size) {
    for (int i = 0; i < size; i++) {
        float x = data[i];
        data[i] = 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159265f) * (x + 0.044715f * x * x * x)));
    }
}

void CPUInferenceEngine::RMSNorm(float* data, int size, float epsilon) {
    float ss = 0.0f;
    for (int i = 0; i < size; i++) ss += data[i] * data[i];
    ss = 1.0f / std::sqrt(ss / size + epsilon);
    for (int i = 0; i < size; i++) data[i] *= ss;
}

void CPUInferenceEngine::RoPE(float* data, int dim, int pos, int rotary_dim) {
    for (int i = 0; i < rotary_dim; i += 2) {
        float freq = 1.0f / std::pow(10000.0f, static_cast<float>(i) / rotary_dim);
        float val  = pos * freq;
        float cos_val = std::cos(val);
        float sin_val = std::sin(val);
        float v0 = data[i];
        float v1 = data[i + 1];
        data[i]     = v0 * cos_val - v1 * sin_val;
        data[i + 1] = v0 * sin_val + v1 * cos_val;
    }
}

void CPUInferenceEngine::MultiHeadAttention(
    const float* query, const float* key, const float* value,
    float* output, int seq_len, int embed_dim, int num_heads, int layer_idx)
{
    int head_dim = embed_dim / num_heads;
    std::vector<float> attn_scores(seq_len * seq_len);
    float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));

    // Q*K^T scaled
    for (int h = 0; h < num_heads; h++) {
        int offset = h * head_dim;
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j <= i; j++) {  // Causal mask
                float score = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    score += query[i * embed_dim + offset + d] * key[j * embed_dim + offset + d];
                }
                attn_scores[i * seq_len + j] = score * scale;
            }
            for (int j = i + 1; j < seq_len; j++) {
                attn_scores[i * seq_len + j] = -1e9f;  // Mask future
            }
        }
        // ApplySoftmax per row, then attn * V
        for (int i = 0; i < seq_len; i++) {
            ApplySoftmax(&attn_scores[i * seq_len], seq_len);
            for (int d = 0; d < head_dim; d++) {
                float sum = 0.0f;
                for (int j = 0; j < seq_len; j++) {
                    sum += attn_scores[i * seq_len + j] * value[j * embed_dim + offset + d];
                }
                output[i * embed_dim + offset + d] = sum;
            }
        }
    }
}

void CPUInferenceEngine::FeedForward(const float* input, float* output, int dim) {
    // Simple projection stub — real path goes through RawrXDTransformer
    std::memcpy(output, input, dim * sizeof(float));
}

void CPUInferenceEngine::TransformerLayer(
    const float* input, float* output, int layer_idx, int seq_len, uint32_t deviceId)
{
    // Fallback math path only; primary production path is RawrXDInference::ForwardTokens.
    (void)layer_idx;
    (void)deviceId;
    if (!input || !output || seq_len <= 0) {
        return;
    }
    int dim = m_embeddingDim > 0 ? m_embeddingDim : 4096;
    size_t sz = static_cast<size_t>(seq_len) * static_cast<size_t>(dim);
    std::memcpy(output, input, sz * sizeof(float));
    for (int t = 0; t < seq_len; ++t) {
        RMSNorm(output + static_cast<size_t>(t) * dim, dim);
    }
}

void CPUInferenceEngine::ApplyNorm(const std::string& name, float* data) {
    int dim = m_embeddingDim > 0 ? m_embeddingDim : 4096;
    if (name.find("rms") != std::string::npos) {
        RMSNorm(data, dim);
    } else {
        LayerNorm(data, dim);
    }
}

// ============================================================================
// Forward declarations for CPUOps dequantization functions
// ============================================================================
namespace CPUOps {
    void DequantizeQ4_0(const uint8_t* quantized, float* output, int size);
    void DequantizeQ8_0(const uint8_t* quantized, float* output, int size);
    void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements);
    void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements);
    void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements);
    void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements);
    void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements);
    void DequantizeF16(const uint8_t* quantized, float* output, int num_elements);
}

void CPUInferenceEngine::DequantizeTensor(
    const std::vector<uint8_t>& src, float* dst, size_t size, TensorType type)
{
    switch (type) {
        case TensorType::Q4_0: CPUOps::DequantizeQ4_0(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q8_0: CPUOps::DequantizeQ8_0(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q4_K: CPUOps::DequantizeQ4_K(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q5_K: CPUOps::DequantizeQ5_K(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q6_K: CPUOps::DequantizeQ6_K(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q2_K: CPUOps::DequantizeQ2_K(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::Q3_K: CPUOps::DequantizeQ3_K(src.data(), dst, static_cast<int>(size)); break;
        case TensorType::F16:  CPUOps::DequantizeF16(src.data(), dst, static_cast<int>(size));  break;
        case TensorType::F32:
            std::memcpy(dst, src.data(), size * sizeof(float));
            break;
        default:
            std::memset(dst, 0, size * sizeof(float));
            break;
    }
}

// ============================================================================
// CPUOps Namespace — Standalone math utilities
// ============================================================================
namespace CPUOps {

void MatMul(const float* A, const float* B, float* C, int m, int n, int k) {
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++) {
            float s = 0.0f;
            for (int p = 0; p < k; p++) s += A[i * k + p] * B[p * n + j];
            C[i * n + j] = s;
        }
}

void VectorAdd(const float* a, const float* b, float* c, int size) {
    for (int i = 0; i < size; i++) c[i] = a[i] + b[i];
}
void VectorMul(const float* a, const float* b, float* c, int size) {
    for (int i = 0; i < size; i++) c[i] = a[i] * b[i];
}
void VectorScale(float* data, float scale, int size) {
    for (int i = 0; i < size; i++) data[i] *= scale;
}

void Softmax(float* data, int size) {
    if (!data || size <= 0) return;
    float mx = *std::max_element(data, data + size);
    float sum = 0.0f;
    for (int i = 0; i < size; i++) { data[i] = std::exp(data[i] - mx); sum += data[i]; }
    if (!(sum > 0.0f) || !std::isfinite(sum)) {
        const float uniform = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; i++) data[i] = uniform;
        return;
    }
    for (int i = 0; i < size; i++) data[i] /= sum;
}

void GELU(float* data, int size) {
    for (int i = 0; i < size; i++) {
        float x = data[i];
        data[i] = 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159265f) * (x + 0.044715f * x * x * x)));
    }
}

void SiLU(float* data, int size) {
    for (int i = 0; i < size; i++) {
        data[i] = data[i] / (1.0f + std::exp(-data[i]));
    }
}

void LayerNorm(float* data, int size, float epsilon) {
    float mean = 0.0f;
    for (int i = 0; i < size; i++) mean += data[i];
    mean /= size;
    float var = 0.0f;
    for (int i = 0; i < size; i++) var += (data[i] - mean) * (data[i] - mean);
    var /= size;
    float inv = 1.0f / std::sqrt(var + epsilon);
    for (int i = 0; i < size; i++) data[i] = (data[i] - mean) * inv;
}

void RMSNorm(float* data, int size, float epsilon) {
    float ss = 0.0f;
    for (int i = 0; i < size; i++) ss += data[i] * data[i];
    ss = 1.0f / std::sqrt(ss / size + epsilon);
    for (int i = 0; i < size; i++) data[i] *= ss;
}

// ---- Dequantization ----
void DequantizeQ4_0(const uint8_t* quantized, float* output, int size) {
    // Q4_0: 32 elements per block. Block = 2-byte scale (f16) + 16 bytes of 4-bit data
    int nblocks = size / 32;
    for (int b = 0; b < nblocks; b++) {
        const uint8_t* block = quantized + b * 18;  // 2 + 16
        // Read f16 scale (simplified: treat as raw uint16 → float approximation)
        uint16_t raw_scale;
        std::memcpy(&raw_scale, block, 2);
        // F16 → F32 (simplified)
        int exp = (raw_scale >> 10) & 0x1F;
        int frac = raw_scale & 0x3FF;
        float scale = (exp == 0) ? (frac / 1024.0f / 16384.0f) :
                      std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (raw_scale & 0x8000) scale = -scale;

        const uint8_t* nibbles = block + 2;
        for (int i = 0; i < 16; i++) {
            uint8_t byte = nibbles[i];
            int lo = (byte & 0x0F) - 8;
            int hi = ((byte >> 4) & 0x0F) - 8;
            output[b * 32 + i * 2]     = lo * scale;
            output[b * 32 + i * 2 + 1] = hi * scale;
        }
    }
}

void DequantizeQ8_0(const uint8_t* quantized, float* output, int size) {
    // Q8_0: 32 elements per block. Block = 2-byte scale (f16) + 32 bytes of int8 data
    int nblocks = size / 32;
    for (int b = 0; b < nblocks; b++) {
        const uint8_t* block = quantized + b * 34;  // 2 + 32
        uint16_t raw_scale;
        std::memcpy(&raw_scale, block, 2);
        int exp = (raw_scale >> 10) & 0x1F;
        int frac = raw_scale & 0x3FF;
        float scale = (exp == 0) ? (frac / 1024.0f / 16384.0f) :
                      std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (raw_scale & 0x8000) scale = -scale;

        const int8_t* vals = reinterpret_cast<const int8_t*>(block + 2);
        for (int i = 0; i < 32; i++) {
            output[b * 32 + i] = vals[i] * scale;
        }
    }
}

void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements) {
    // K-quant super-blocks (256 elements). Simplified dequant.
    int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++) {
        for (int i = 0; i < 256; i++) {
            output[b * 256 + i] = 0.0f;  // Placeholder — full K-quant decode is complex
        }
    }
}

void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements) {
    int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
        for (int i = 0; i < 256; i++)
            output[b * 256 + i] = 0.0f;
}

void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements) {
    int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
        for (int i = 0; i < 256; i++)
            output[b * 256 + i] = 0.0f;
}

void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements) {
    int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
        for (int i = 0; i < 256; i++)
            output[b * 256 + i] = 0.0f;
}

void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements) {
    int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
        for (int i = 0; i < 256; i++)
            output[b * 256 + i] = 0.0f;
}

void DequantizeF16(const uint8_t* quantized, float* output, int num_elements) {
    const uint16_t* src = reinterpret_cast<const uint16_t*>(quantized);
    for (int i = 0; i < num_elements; i++) {
        uint16_t h = src[i];
        int exp = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float val = (exp == 0) ? (frac / 1024.0f / 16384.0f) :
                    std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (h & 0x8000) val = -val;
        output[i] = val;
    }
}

void EnableAVX2(bool enable) { /* TODO: runtime dispatch */ }
void EnableMultiThreading(bool enable) { /* TODO: thread pool toggle */ }

} // namespace CPUOps

} // namespace RawrXD
