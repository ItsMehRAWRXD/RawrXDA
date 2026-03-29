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
#include "gguf_loader_diagnostics.h"

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

    // Initialize diagnostics
    auto& diag = RawrXD::GGUFLoaderDiagnostics::Instance();
    diag.StartLoad(model_path);

    // Check file accessibility first
    std::ifstream testFile(model_path, std::ios::binary | std::ios::ate);
    if (!testFile.is_open()) {
        diag.RecordStage(RawrXD::GGUFLoaderDiagnostics::LoadStage::kFileOpen, false,
                        "Cannot open model file", "File not found or access denied");
        m_lastLoadErrorMessage = "Cannot open model file: " + model_path;
        return false;
    }
    uint64_t fileSize = testFile.tellg();
    testFile.close();

    diag.RecordStage(RawrXD::GGUFLoaderDiagnostics::LoadStage::kFileOpen, true,
                    "File opened successfully (" + std::to_string(fileSize) + " bytes)");

    if (s_inferenceBackend.Initialize(wpath.c_str(), vocabPath.c_str(), mergesPath.c_str())) {
        diag.RecordStage(RawrXD::GGUFLoaderDiagnostics::LoadStage::kComplete, true,
                        "Model loaded successfully");

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

        diag.EndLoad();
        return true;
    }

    m_lastLoadErrorMessage = s_inferenceBackend.GetLastLoadErrorMessage();
    if (m_lastLoadErrorMessage.empty()) {
        m_lastLoadErrorMessage = "RawrXDInference::Initialize returned false without detail";
    }

    // Record failure in diagnostics
    diag.RecordStage(RawrXD::GGUFLoaderDiagnostics::LoadStage::kComplete, false,
                    "Model load failed", m_lastLoadErrorMessage);
    diag.EndLoad();

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
// Training (weight updates) — SGD gradient descent on F32 tensors
// ============================================================================
void CPUInferenceEngine::UpdateWeights(
    const std::vector<std::vector<float>>& layer_gradients, float learning_rate) {
    // Apply gradient updates to weight tensors in iteration order
    size_t idx = 0;
    for (auto& [name, tensor] : m_weights) {
        if (idx >= layer_gradients.size()) break;
        if (tensor.type != TensorType::F32) { ++idx; continue; }

        const auto& grads = layer_gradients[idx];
        float* w = reinterpret_cast<float*>(tensor.data.data());
        size_t count = std::min(grads.size(), tensor.data.size() / sizeof(float));
        for (size_t i = 0; i < count; ++i) {
            w[i] -= learning_rate * grads[i];
        }
        ++idx;
    }
}

void CPUInferenceEngine::UpdateOutputWeights(
    const std::vector<float>& gradients, float learningRate) {
    // Update the output projection tensor
    if (m_weights.empty()) return;
    auto it = m_weights.find("output");
    if (it == m_weights.end()) it = m_weights.find("lm_head");
    if (it == m_weights.end()) return; // No output layer found
    auto& tensor = it->second;
    if (tensor.type != TensorType::F32) return;

    float* w = reinterpret_cast<float*>(tensor.data.data());
    size_t count = std::min(gradients.size(), tensor.data.size() / sizeof(float));
    for (size_t i = 0; i < count; ++i) {
        w[i] -= learningRate * gradients[i];
    }
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
    // SwiGLU-style FFN activation: apply GELU non-linearity
    // Full weighted path goes through RawrXDInference::ForwardTokens;
    // this is the direct-call fallback for callers using the math API.
    std::memcpy(output, input, dim * sizeof(float));
    GELU(output, dim);
}

void CPUInferenceEngine::TransformerLayer(
    const float* input, float* output, int layer_idx, int seq_len, uint32_t deviceId)
{
    // Fallback math path; primary production path is RawrXDInference::ForwardTokens.
    (void)deviceId;
    if (!input || !output || seq_len <= 0) return;

    int dim = m_embeddingDim > 0 ? m_embeddingDim : 4096;
    size_t sz = static_cast<size_t>(seq_len) * static_cast<size_t>(dim);

    // Copy input → output (accumulate residuals in-place)
    std::memcpy(output, input, sz * sizeof(float));

    // --- Attention sub-block ---
    // Pre-attention RMSNorm per token
    std::vector<float> normed(sz);
    std::memcpy(normed.data(), output, sz * sizeof(float));
    for (int t = 0; t < seq_len; ++t) {
        RMSNorm(normed.data() + static_cast<size_t>(t) * dim, dim);
    }

    // Self-attention (identity-projected Q/K/V — weights handled by RawrXDInference)
    std::vector<float> attn_out(sz, 0.0f);
    MultiHeadAttention(normed.data(), normed.data(), normed.data(),
                       attn_out.data(), seq_len, dim, m_numHeads > 0 ? m_numHeads : 32, layer_idx);

    // Residual connection
    for (size_t i = 0; i < sz; ++i) output[i] += attn_out[i];

    // --- FFN sub-block ---
    // Post-attention RMSNorm per token
    std::memcpy(normed.data(), output, sz * sizeof(float));
    for (int t = 0; t < seq_len; ++t) {
        RMSNorm(normed.data() + static_cast<size_t>(t) * dim, dim);
    }

    // Feed-forward + residual per token
    std::vector<float> ffn_out(dim);
    for (int t = 0; t < seq_len; ++t) {
        FeedForward(normed.data() + static_cast<size_t>(t) * dim,
                    ffn_out.data(), dim);
        for (int d = 0; d < dim; ++d)
            output[static_cast<size_t>(t) * dim + d] += ffn_out[d];
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

// Helper: fp16 raw bits to float (same logic used in DequantizeQ4_0/Q8_0)
static inline float fp16_to_fp32(uint16_t raw) {
    int exp = (raw >> 10) & 0x1F;
    int frac = raw & 0x3FF;
    float f = (exp == 0) ? (frac / 1024.0f / 16384.0f)
                         : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
    return (raw & 0x8000) ? -f : f;
}

void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements) {
    // Q4_K super-block: 144 bytes per 256 elements
    // Layout: d(fp16,2) dmin(fp16,2) scales[12] qs[128]
    // 8 sub-blocks of 32 elements each
    // Formula: output = d * sc * q - dmin * m
    const int QK_K = 256;
    int nblocks = num_elements / QK_K;
    const uint8_t* src = quantized;

    for (int b = 0; b < nblocks; b++) {
        float d    = fp16_to_fp32(*(const uint16_t*)(src + 0));
        float dmin = fp16_to_fp32(*(const uint16_t*)(src + 2));
        const uint8_t* scales = src + 4;   // 12 bytes of packed 6-bit scale/min
        const uint8_t* qs     = src + 16;  // 128 bytes of 4-bit quants

        float* dst = output + b * QK_K;

        for (int j = 0; j < QK_K / 32; j++) {
            // Unpack 6-bit scale (sc) and minimum (m) for sub-block j
            uint8_t sc, m;
            if (j < 4) {
                sc = scales[j] & 0x3F;
                m  = scales[j + 4] & 0x3F;
            } else {
                sc = (scales[j + 4] & 0x0F) | ((scales[j - 4] >> 6) << 4);
                m  = (scales[j + 4] >>   4) | ((scales[j]     >> 6) << 4);
            }

            float scale_val = d * sc;
            float min_val   = dmin * m;

            for (int i = 0; i < 32; i++) {
                int qi = j * 16 + i / 2;
                uint8_t q = (i % 2 == 0) ? (qs[qi] & 0x0F) : (qs[qi] >> 4);
                dst[j * 32 + i] = scale_val * q - min_val;
            }
        }
        src += 144;
    }
}

void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements) {
    // Q5_K super-block: 176 bytes per 256 elements
    // Layout: d(fp16,2) dmin(fp16,2) scales[12] qs[128] qh[32]
    // 5-bit quants: 4 low bits from qs[], 1 high bit from qh[]
    const int QK_K = 256;
    int nblocks = num_elements / QK_K;
    const uint8_t* src = quantized;

    for (int b = 0; b < nblocks; b++) {
        float d    = fp16_to_fp32(*(const uint16_t*)(src + 0));
        float dmin = fp16_to_fp32(*(const uint16_t*)(src + 2));
        const uint8_t* scales = src + 4;
        const uint8_t* qs     = src + 16;
        const uint8_t* qh     = src + 144;  // 32 bytes of high bits

        float* dst = output + b * QK_K;

        for (int j = 0; j < QK_K / 32; j++) {
            uint8_t sc, m;
            if (j < 4) {
                sc = scales[j] & 0x3F;
                m  = scales[j + 4] & 0x3F;
            } else {
                sc = (scales[j + 4] & 0x0F) | ((scales[j - 4] >> 6) << 4);
                m  = (scales[j + 4] >>   4) | ((scales[j]     >> 6) << 4);
            }

            float scale_val = d * sc;
            float min_val   = dmin * m;

            for (int i = 0; i < 32; i++) {
                int elem_idx = j * 32 + i;
                int qi = j * 16 + i / 2;
                uint8_t q4 = (i % 2 == 0) ? (qs[qi] & 0x0F) : (qs[qi] >> 4);
                // High bit from qh
                int qh_byte = elem_idx / 8;
                int qh_bit  = elem_idx % 8;
                uint8_t hb = (qh[qh_byte] >> qh_bit) & 1;
                uint8_t q = q4 | (hb << 4);  // 5-bit quant
                dst[elem_idx] = scale_val * q - min_val;
            }
        }
        src += 176;
    }
}

void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements) {
    // Q6_K super-block: 210 bytes per 256 elements
    // Layout: ql[128] qh[64] scales[16] d(fp16 at offset 208)
    // 16 sub-blocks of 16 elements, 6-bit quants (signed, q - 32)
    const int QK_K = 256;
    int nblocks = num_elements / QK_K;
    const uint8_t* src = quantized;

    for (int b = 0; b < nblocks; b++) {
        const uint8_t* ql     = src;         // 128 bytes: low 4 bits (2 per byte)
        const uint8_t* qh     = src + 128;   // 64 bytes: high 2 bits (4 per byte)
        const int8_t*  scales = (const int8_t*)(src + 192); // 16 int8 scales
        float d = fp16_to_fp32(*(const uint16_t*)(src + 208));

        float* dst = output + b * QK_K;

        for (int j = 0; j < 16; j++) {
            float sc = d * scales[j];

            for (int i = 0; i < 16; i++) {
                int elem = j * 16 + i;
                // Low 4 bits from ql
                int ql_idx = elem / 2;
                uint8_t q4 = (elem % 2 == 0) ? (ql[ql_idx] & 0x0F) : (ql[ql_idx] >> 4);
                // High 2 bits from qh
                int qh_idx = elem / 4;
                int qh_shift = (elem % 4) * 2;
                uint8_t q2 = (qh[qh_idx] >> qh_shift) & 0x03;
                int q = (int)(q4 | (q2 << 4)) - 32;  // 6-bit signed
                dst[elem] = sc * q;
            }
        }
        src += 210;
    }
}

void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements) {
    // Q2_K super-block: 84 bytes per 256 elements
    // Layout: scales[16] d(fp16@16) dmin(fp16@18) qs[64]
    // 16 sub-blocks of 16 elements, 2-bit quants (4 per byte)
    // Formula: d * scale * q - dmin * min
    const int QK_K = 256;
    int nblocks = num_elements / QK_K;
    const uint8_t* src = quantized;

    for (int b = 0; b < nblocks; b++) {
        const uint8_t* sc_raw = src;        // scales[16]: packed scale(4b) + min(4b)
        float d    = fp16_to_fp32(*(const uint16_t*)(src + 16));
        float dmin = fp16_to_fp32(*(const uint16_t*)(src + 18));
        const uint8_t* qs = src + 20;       // 64 bytes of 2-bit quants

        float* dst = output + b * QK_K;

        for (int j = 0; j < 16; j++) {
            uint8_t sc  = sc_raw[j] & 0x0F;    // low nibble = scale
            uint8_t m   = sc_raw[j] >> 4;       // high nibble = min
            float scale_val = d * sc;
            float min_val   = dmin * m;

            for (int i = 0; i < 16; i++) {
                int elem = j * 16 + i;
                int byte_idx = elem / 4;
                int bit_shift = (elem % 4) * 2;
                uint8_t q = (qs[byte_idx] >> bit_shift) & 0x03;
                dst[elem] = scale_val * q - min_val;
            }
        }
        src += 84;
    }
}

void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements) {
    // Q3_K super-block: 110 bytes per 256 elements
    // Layout: hmask[32] qs[64] scales[12] d(fp16@108)
    // 16 sub-blocks of 16 elements, 3-bit quants (2 from qs + 1 from hmask), signed (q - 4)
    // Formula: d * scale * (q - 4)
    const int QK_K = 256;
    int nblocks = num_elements / QK_K;
    const uint8_t* src = quantized;

    for (int b = 0; b < nblocks; b++) {
        const uint8_t* hmask  = src;          // 32 bytes: high bits
        const uint8_t* qs     = src + 32;     // 64 bytes: low 2-bit quants
        const uint8_t* sc_raw = src + 96;     // 12 bytes: packed 6-bit scales
        float d = fp16_to_fp32(*(const uint16_t*)(src + 108));

        float* dst = output + b * QK_K;

        for (int j = 0; j < 16; j++) {
            // Decode 6-bit scale from packed 12 bytes
            int sc_int;
            if (j < 8) {
                int idx = j / 2;
                sc_int = (j % 2 == 0) ? (sc_raw[idx] & 0x0F) : (sc_raw[idx] >> 4);
                // High 2 bits from bytes 8..11
                int hi_idx = j / 4;
                int hi_shift = (j % 4) * 2;
                sc_int |= ((sc_raw[8 + hi_idx] >> hi_shift) & 0x03) << 4;
            } else {
                int idx = (j - 8) / 2 + 4;
                sc_int = ((j - 8) % 2 == 0) ? (sc_raw[idx] & 0x0F) : (sc_raw[idx] >> 4);
                int hi_idx = (j - 8) / 4;
                int hi_shift = ((j - 8) % 4) * 2;
                sc_int |= ((sc_raw[8 + 2 + hi_idx] >> hi_shift) & 0x03) << 4;
            }
            sc_int -= 32;  // center: signed scale (-32..31)

            float scale_val = d * sc_int;

            for (int i = 0; i < 16; i++) {
                int elem = j * 16 + i;
                // Low 2 bits from qs
                int qs_byte = elem / 4;
                int qs_shift = (elem % 4) * 2;
                uint8_t q2 = (qs[qs_byte] >> qs_shift) & 0x03;
                // High bit from hmask
                int hm_byte = elem / 8;
                int hm_bit  = elem % 8;
                uint8_t hb = (hmask[hm_byte] >> hm_bit) & 1;
                int q = (int)(q2 | (hb << 2)) - 4;  // 3-bit signed
                dst[elem] = scale_val * q;
            }
        }
        src += 110;
    }
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
