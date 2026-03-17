#include "cpu_inference_engine.h"
#include "streaming_gguf_loader.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <immintrin.h>
#include <omp.h>

namespace CPUInference {

// ============================================================================
// TOKENIZATION
// ============================================================================

CPUInferenceEngine::CPUInferenceEngine()
    : m_modelLoaded(false), m_vocabSize(0), m_embeddingDim(0),
      m_numLayers(0), m_numHeads(0), m_threadCount(1)
{
    m_threadCount = std::thread::hardware_concurrency();
}

CPUInferenceEngine::~CPUInferenceEngine() {
    // Cleanup
}

bool CPUInferenceEngine::LoadModel(const std::string& model_path) {
    return loadModel(model_path);
}

bool CPUInferenceEngine::loadModel(const std::string& path) {
    m_loader = std::make_unique<StreamingGGUFLoader>();
    if (!m_loader->Open(path)) {
        std::cerr << "Failed to open model: " << path << std::endl;
        return false;
    }
    
    // Get metadata
    auto metadata = m_loader->GetMetadata();
    m_vocabSize = metadata.vocab_size;
    m_embeddingDim = metadata.embedding_dim;
    m_numLayers = metadata.layer_count;
    m_numHeads = metadata.head_count;
    
    // Initialize vocabulary from metadata
    if (!metadata.tokens.empty()) {
        m_vocab = metadata.tokens;
    }
    
    m_modelLoaded = true;
    m_contextLimit = metadata.context_length;
    InitKVCache();
    
    return true;
}

std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    size_t i = 0;
    
    while (i < text.size()) {
        int best_id = -1;
        size_t best_len = 0;
        size_t max_search = std::min(text.size() - i, (size_t)64);
        
        // Simple BPE approximation - greedy matching
        for (int v = 0; v < (int)m_vocab.size(); ++v) {
            const auto& vocab_token = m_vocab[v];
            if (vocab_token.size() > best_len && vocab_token.size() <= max_search) {
                if (text.substr(i, vocab_token.size()) == vocab_token) {
                    best_len = vocab_token.size();
                    best_id = v;
                }
            }
        }
        
        if (best_id != -1) {
            tokens.push_back(best_id);
            i += best_len;
        } else {
            i++;
        }
    }
    
    return tokens;
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    for (auto token_id : tokens) {
        if (token_id >= 0 && token_id < (int)m_vocab.size()) {
            result += m_vocab[token_id];
        }
    }
    return result;
}

// ============================================================================
// EVALUATION
// ============================================================================

std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens) {
    if (input_tokens.empty() || !m_modelLoaded) {
        return std::vector<float>(m_vocabSize, 0.0f);
    }
    
    // Simplified evaluation:
    // 1. Get embedding for last token
    // 2. Run through transformer layers
    // 3. Compute logits
    
    int last_token_id = input_tokens.back();
    std::vector<float> embedding(m_embeddingDim, 0.1f);  // Placeholder
    
    // Load embedding if possible
    std::vector<uint8_t> emb_data;
    if (m_loader && m_loader->LoadTensorZone("token_embd.weight", emb_data)) {
        size_t row_bytes = m_embeddingDim * sizeof(float);
        if (last_token_id * row_bytes + row_bytes <= emb_data.size()) {
            const float* emb_ptr = reinterpret_cast<const float*>(emb_data.data() + last_token_id * row_bytes);
            std::copy(emb_ptr, emb_ptr + m_embeddingDim, embedding.begin());
        }
    }
    
    // Apply transformer layers (simplified - just pass through)
    std::vector<float> state = embedding;
    
    // Simple RMS norm
    float rms = std::sqrt(std::accumulate(state.begin(), state.end(), 0.0f,
        [](float a, float b) { return a + b * b; }) / state.size());
    if (rms > 1e-6f) {
        for (auto& x : state) x /= rms;
    }
    
    // Project to vocabulary
    std::vector<float> logits(m_vocabSize, 0.0f);
    for (int i = 0; i < std::min((int)m_vocabSize, 32); ++i) {
        logits[i] = state[i % m_embeddingDim] * 0.5f;
    }
    
    return logits;
}

std::vector<int32_t> CPUInferenceEngine::Generate(const std::vector<int32_t>& input_tokens, int max_tokens) {
    std::vector<int32_t> result = input_tokens;
    
    for (int i = 0; i < max_tokens; ++i) {
        auto logits = Eval(result);
        
        // Argmax sampling
        int next_token = 0;
        float max_logit = logits[0];
        for (int j = 1; j < (int)logits.size(); ++j) {
            if (logits[j] > max_logit) {
                max_logit = logits[j];
                next_token = j;
            }
        }
        
        result.push_back(next_token);
        if (next_token == 2) break;  // EOS token
    }
    
    return result;
}

void CPUInferenceEngine::GenerateStreaming(
    const std::vector<int32_t>& input_tokens,
    int max_tokens,
    std::function<void(const std::string&)> token_callback,
    std::function<void()> complete_callback,
    std::function<void(int32_t)> token_id_callback)
{
    if (input_tokens.empty()) return;
    
    std::vector<int32_t> context = input_tokens;
    
    for (int i = 0; i < max_tokens; ++i) {
        auto logits = Eval(context);
        
        // Sample token
        int next_token = 0;
        float max_logit = logits[0];
        for (int j = 1; j < (int)logits.size(); ++j) {
            if (logits[j] > max_logit) {
                max_logit = logits[j];
                next_token = j;
            }
        }
        
        // Callback
        if (next_token >= 0 && next_token < (int)m_vocab.size()) {
            if (token_callback) token_callback(m_vocab[next_token]);
        }
        if (token_id_callback) token_id_callback(next_token);
        
        // Add to context and check for EOS
        context.push_back(next_token);
        if (next_token == 2) break;
    }
    
    if (complete_callback) complete_callback();
}

// ============================================================================
// MEMORY & KV CACHE
// ============================================================================

void CPUInferenceEngine::InitKVCache() {
    m_kv_cache.clear();
    for (int i = 0; i < m_numLayers; ++i) {
        KVCache cache;
        cache.k.shape = {1, m_contextLimit, m_embeddingDim / m_numHeads};
        cache.v.shape = {1, m_contextLimit, m_embeddingDim / m_numHeads};
        m_kv_cache.push_back(cache);
    }
}

void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<::RawrXD::IMemoryPlugin> plugin) {
    if (plugin) {
        m_memoryPlugin = plugin;
    }
}

// ============================================================================
// MODEL INFO
// ============================================================================

int CPUInferenceEngine::GetVocabSize() const { return m_vocabSize; }
int CPUInferenceEngine::GetEmbeddingDim() const { return m_embeddingDim; }
int CPUInferenceEngine::GetNumLayers() const { return m_numLayers; }
int CPUInferenceEngine::GetNumHeads() const { return m_numHeads; }

} // namespace CPUInference
