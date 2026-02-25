// ============================================================================
// dml_inference_engine.cpp — DirectML Inference Engine Implementation
// ============================================================================
// Implements the server-compatible inference engine backed by DirectML.
// Provides Tokenize/Generate/Detokenize matching CPUInferenceEngine API.
//
// Inference pipeline per Generate() call:
//   1. Tokenize (BPE from GGUF vocab)
//   2. runModelForward() via DirectMLCompute → logits
//   3. Sample token (temperature → top-k → top-p → multinomial)
//   4. Append to KV cache, repeat until EOS or maxTokens
//   5. Detokenize result
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "dml_inference_engine.h"
#include "core/directml_compute.h"
#include "core/gguf_dml_bridge.h"
#include "ai_backend.h"

#include "logging/logger.h"
static Logger s_logger("dml_inference_engine");

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>

namespace RawrXD {

// ============================================================================
// GGUF Tokenizer Constants
// ============================================================================
static const uint32_t GGUF_MAGIC_TOK     = 0x46554747;
static const char*    GGUF_KEY_VOCAB      = "tokenizer.ggml.tokens";
static const char*    GGUF_KEY_SCORES     = "tokenizer.ggml.scores";
static const char*    GGUF_KEY_BOS        = "tokenizer.ggml.bos_token_id";
static const char*    GGUF_KEY_EOS        = "tokenizer.ggml.eos_token_id";
static const char*    GGUF_KEY_PAD        = "tokenizer.ggml.padding_token_id";
static const char*    GGUF_KEY_UNK        = "tokenizer.ggml.unknown_token_id";

// GGUF metadata value types (same as in gguf_dml_bridge.cpp)
enum GGUFMetaTypeTok : uint32_t {
    META_UINT8    = 0,  META_INT8   = 1,  META_UINT16  = 2,  META_INT16  = 3,
    META_UINT32   = 4,  META_INT32  = 5,  META_FLOAT32 = 6,  META_BOOL   = 7,
    META_STRING   = 8,  META_ARRAY  = 9,  META_UINT64  = 10, META_INT64  = 11,
    META_FLOAT64  = 12,
};

// ============================================================================
// Safe mmap reader for tokenizer extraction
// ============================================================================
class TokReader {
public:
    TokReader(const uint8_t* base, uint64_t size) : m_base(base), m_size(size), m_pos(0) {}
    bool ok(uint64_t n) const { return m_pos + n <= m_size; }
    uint64_t pos() const { return m_pos; }
    void seek(uint64_t p) { m_pos = p; }

    template<typename T> T read() {
        T val{}; if (ok(sizeof(T))) { memcpy(&val, m_base + m_pos, sizeof(T)); m_pos += sizeof(T); } return val;
    }

    std::string readStr() {
        uint64_t len = read<uint64_t>();
        std::string s;
        if (len > 0 && ok(len)) { s.assign((const char*)(m_base + m_pos), (size_t)len); m_pos += len; }
        return s;
    }

    void skipMeta(uint32_t type) {
        switch (type) {
            case META_UINT8: case META_INT8: case META_BOOL: m_pos += 1; break;
            case META_UINT16: case META_INT16: m_pos += 2; break;
            case META_UINT32: case META_INT32: case META_FLOAT32: m_pos += 4; break;
            case META_UINT64: case META_INT64: case META_FLOAT64: m_pos += 8; break;
            case META_STRING: readStr(); break;
            case META_ARRAY: {
                uint32_t et = read<uint32_t>(); uint64_t c = read<uint64_t>();
                for (uint64_t i = 0; i < c; ++i) skipMeta(et);
                break;
            }
        }
    }

private:
    const uint8_t* m_base;
    uint64_t m_size, m_pos;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
DMLInferenceEngine::DMLInferenceEngine() = default;

DMLInferenceEngine::~DMLInferenceEngine() {
    UnloadModel();
}

// ============================================================================
// Model Loading
// ============================================================================
bool DMLInferenceEngine::LoadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_modelLoaded.load()) {
        UnloadModel();
    }

    // Get DML singletons
    auto& dmlCompute = DML::getDirectMLCompute();
    auto& bridge     = DML::getGGUFDMLBridge();

    m_dmlCompute = &dmlCompute;
    m_ggufBridge = &bridge;

    // Initialize DML if needed
    if (!dmlCompute.isInitialized()) {
        auto r = dmlCompute.initialize();
        if (!r.success) {
            s_logger.error( "[DMLEngine] DML init failed: " << r.detail << std::endl;
            return false;
        }
    }

    // Configure bridge
    bridge.setDirectMLCompute(m_dmlCompute);
    bridge.setDequantOnCPU(true);
    bridge.setStreamingEnabled(true);
    bridge.setComputeDataType(DML::TensorDataType::Float16);

    // Open GGUF model
    auto r = bridge.openModel(modelPath.c_str(), m_sessionId);
    if (!r.success) {
        s_logger.error( "[DMLEngine] openModel failed: " << r.detail << std::endl;
        return false;
    }

    // Get model config
    auto& config = bridge.getModelConfig(m_sessionId);
    m_vocabSize    = config.vocabSize;
    m_embeddingDim = config.hiddenSize;
    m_numLayers    = config.numLayers;
    m_numHeads     = config.numHeads;
    m_numKVHeads   = config.numKVHeads;
    m_maxSeqLen    = config.maxSeqLen;
    m_architecture = config.architecture;
    m_modelPath    = modelPath;

    // Load tokenizer from GGUF file
    if (!loadTokenizer(modelPath)) {
        s_logger.error( "[DMLEngine] Tokenizer load failed (continuing with limited tokenizer)"
                  << std::endl;
    }

    // Load tensors: try full load, fall back to fixed + streaming
    if (config.estimatedVRAM <= bridge.getVRAMBudget(m_sessionId)) {
        auto r2 = bridge.loadAllTensors(m_sessionId);
        if (!r2.success) {
            s_logger.error( "[DMLEngine] Full load failed, using streaming: " << r2.detail
                      << std::endl;
            bridge.loadFixedTensors(m_sessionId);
        }
    } else {
        bridge.loadFixedTensors(m_sessionId);
    }

    // Allocate logits buffer
    m_logitsBuffer.resize(m_vocabSize, 0.0f);

    m_modelLoaded.store(true);

    s_logger.info("[DMLEngine] Model loaded: ");

    return true;
}

bool DMLInferenceEngine::LoadSecondModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_modelLoaded.load()) {
        s_logger.error( "[DMLEngine] No primary model loaded" << std::endl;
        return false;
    }

    auto r = m_ggufBridge->loadSecondModel(modelPath.c_str(), m_sessionId + 1);
    if (!r.success) {
        s_logger.error( "[DMLEngine] Second model load failed: " << r.detail << std::endl;
        return false;
    }

    s_logger.info("[DMLEngine] Second model loaded: ");
    return true;
}

void DMLInferenceEngine::UnloadModel() {
    if (m_ggufBridge) {
        m_ggufBridge->closeAllModels();
    }
    m_modelLoaded.store(false);
    m_logitsBuffer.clear();
    m_tokenizer = BPETokenizer{};
}

// ============================================================================
// Tokenizer Loading from GGUF
// ============================================================================
bool DMLInferenceEngine::loadTokenizer(const std::string& ggufPath) {
    // Memory-map the file to read tokenizer metadata
    HANDLE hFile = CreateFileA(ggufPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fsize;
    GetFileSizeEx(hFile, &fsize);
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) { CloseHandle(hFile); return false; }

    void* base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(hMap); CloseHandle(hFile); return false; }

    TokReader reader(static_cast<const uint8_t*>(base), fsize.QuadPart);

    // Validate magic + version
    uint32_t magic = reader.read<uint32_t>();
    uint32_t version = reader.read<uint32_t>();
    if (magic != GGUF_MAGIC_TOK) {
        UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile);
        return false;
    }

    uint64_t tensorCount = reader.read<uint64_t>();
    uint64_t metaCount   = reader.read<uint64_t>();

    // Scan metadata for tokenizer keys
    for (uint64_t i = 0; i < metaCount; ++i) {
        if (!reader.ok(12)) break;

        std::string key = reader.readStr();
        uint32_t valueType = reader.read<uint32_t>();

        if (key == GGUF_KEY_VOCAB && valueType == META_ARRAY) {
            // Array of strings
            uint32_t elemType = reader.read<uint32_t>();
            uint64_t count = reader.read<uint64_t>();
            m_tokenizer.vocab.reserve((size_t)count);
            for (uint64_t j = 0; j < count; ++j) {
                m_tokenizer.vocab.push_back(reader.readStr());
            }
        }
        else if (key == GGUF_KEY_SCORES && valueType == META_ARRAY) {
            uint32_t elemType = reader.read<uint32_t>();
            uint64_t count = reader.read<uint64_t>();
            m_tokenizer.scores.resize((size_t)count);
            for (uint64_t j = 0; j < count; ++j) {
                m_tokenizer.scores[j] = reader.read<float>();
            }
        }
        else if (key == GGUF_KEY_BOS) {
            m_tokenizer.bosToken = reader.read<int32_t>();
        }
        else if (key == GGUF_KEY_EOS) {
            m_tokenizer.eosToken = reader.read<int32_t>();
        }
        else if (key == GGUF_KEY_PAD) {
            m_tokenizer.padToken = reader.read<int32_t>();
        }
        else if (key == GGUF_KEY_UNK) {
            m_tokenizer.unkToken = reader.read<int32_t>();
        }
        else {
            reader.skipMeta(valueType);
        }
    }

    // Build reverse mapping
    for (int32_t i = 0; i < (int32_t)m_tokenizer.vocab.size(); ++i) {
        m_tokenizer.tokenToId[m_tokenizer.vocab[i]] = i;
    }

    m_tokenizer.loaded = !m_tokenizer.vocab.empty();

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    if (m_tokenizer.loaded) {
        m_vocabSize = (int)m_tokenizer.vocab.size();
        s_logger.info("[DMLEngine] Tokenizer loaded: ");
    }

    return m_tokenizer.loaded;
}

// ============================================================================
// Tokenize (BPE Encode)
// ============================================================================
std::vector<int32_t> DMLInferenceEngine::Tokenize(const std::string& text) {
    if (!m_tokenizer.loaded) {
        // Fallback: character-level tokenization
        std::vector<int32_t> tokens;
        tokens.push_back(m_tokenizer.bosToken);
        for (char c : text) {
            std::string s(1, c);
            auto it = m_tokenizer.tokenToId.find(s);
            if (it != m_tokenizer.tokenToId.end()) {
                tokens.push_back(it->second);
            } else {
                tokens.push_back(m_tokenizer.unkToken);
            }
        }
        return tokens;
    }

    return bpeEncode(text);
}

std::vector<int32_t> DMLInferenceEngine::bpeEncode(const std::string& text) {
    std::vector<int32_t> tokens;
    tokens.push_back(m_tokenizer.bosToken);

    // Pre-tokenize: split into characters/bytes as initial tokens
    std::vector<std::string> symbols;
    size_t pos = 0;
    while (pos < text.size()) {
        // Try to find longest matching token
        size_t bestLen = 0;
        int32_t bestId = m_tokenizer.unkToken;

        // Try lengths from max down to 1
        size_t maxTry = std::min(text.size() - pos, (size_t)32);
        for (size_t len = maxTry; len >= 1; --len) {
            std::string candidate = text.substr(pos, len);
            auto it = m_tokenizer.tokenToId.find(candidate);
            if (it != m_tokenizer.tokenToId.end()) {
                bestLen = len;
                bestId = it->second;
                break;
            }
        }

        if (bestLen > 0) {
            tokens.push_back(bestId);
            pos += bestLen;
        } else {
            // Single byte as unknown
            tokens.push_back(m_tokenizer.unkToken);
            pos++;
        }
    }

    // BPE merge pass: iteratively merge the highest-scoring adjacent pairs
    if (!m_tokenizer.scores.empty() && tokens.size() > 2) {
        bool merged = true;
        while (merged) {
            merged = false;
            float bestScore = -1e30f;
            size_t bestIdx = 0;
            int32_t bestMerge = -1;

            // Find best merge
            for (size_t i = 1; i < tokens.size() - 1; ++i) {
                // Construct merged token string
                if (tokens[i] < 0 || tokens[i] >= (int32_t)m_tokenizer.vocab.size()) continue;
                if (tokens[i + 1] < 0 || tokens[i + 1] >= (int32_t)m_tokenizer.vocab.size()) continue;

                std::string merged_str = m_tokenizer.vocab[tokens[i]] +
                                         m_tokenizer.vocab[tokens[i + 1]];
                auto it = m_tokenizer.tokenToId.find(merged_str);
                if (it != m_tokenizer.tokenToId.end()) {
                    int32_t mergeId = it->second;
                    float score = (mergeId < (int32_t)m_tokenizer.scores.size())
                                  ? m_tokenizer.scores[mergeId] : 0.0f;
                    if (score > bestScore) {
                        bestScore = score;
                        bestIdx = i;
                        bestMerge = mergeId;
                    }
                }
            }

            if (bestMerge >= 0) {
                tokens[bestIdx] = bestMerge;
                tokens.erase(tokens.begin() + bestIdx + 1);
                merged = true;
            }
        }
    }

    return tokens;
}

// ============================================================================
// Detokenize (BPE Decode)
// ============================================================================
std::string DMLInferenceEngine::Detokenize(const std::vector<int32_t>& tokens) {
    return bpeDecode(tokens);
}

std::string DMLInferenceEngine::bpeDecode(const std::vector<int32_t>& tokens) {
    std::string result;
    result.reserve(tokens.size() * 4);  // Rough estimate

    for (int32_t tokenId : tokens) {
        // Skip special tokens
        if (tokenId == m_tokenizer.bosToken ||
            tokenId == m_tokenizer.eosToken ||
            tokenId == m_tokenizer.padToken) {
            continue;
        }

        if (tokenId >= 0 && tokenId < (int32_t)m_tokenizer.vocab.size()) {
            const std::string& tok = m_tokenizer.vocab[tokenId];
            // Handle SentencePiece-style space marker
            if (!tok.empty() && tok[0] == '\xe2' && tok.size() >= 3 &&
                tok[1] == '\x96' && tok[2] == '\x81') {
                result += ' ';
                result += tok.substr(3);
            } else {
                result += tok;
            }
        }
    }

    return result;
}

// ============================================================================
// Generate (Blocking)
// ============================================================================
std::vector<int32_t> DMLInferenceEngine::Generate(const std::vector<int32_t>& inputTokens,
                                                    int maxTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_modelLoaded.load() || !m_dmlCompute || !m_ggufBridge) {
        return {};
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Resize logits buffer
    m_logitsBuffer.resize(m_vocabSize, 0.0f);

    std::vector<int32_t> allTokens = inputTokens;
    std::vector<int32_t> generated;

    auto promptEnd = std::chrono::high_resolution_clock::now();

    for (int step = 0; step < maxTokens; ++step) {
        // Run forward pass: get logits for next token prediction
        auto r = m_dmlCompute->runModelForward(
            m_sessionId,
            allTokens.data(),
            static_cast<uint32_t>(allTokens.size()),
            m_logitsBuffer.data(),
            m_vocabSize
        );

        if (!r.success) {
            s_logger.error( "[DMLEngine] Forward pass failed at step " << step
                      << ": " << r.detail << std::endl;
            break;
        }

        // Sample next token
        int32_t nextToken = sampleToken(m_logitsBuffer.data(), m_vocabSize, allTokens);

        // Check for EOS
        if (nextToken == m_tokenizer.eosToken) break;

        generated.push_back(nextToken);
        allTokens.push_back(nextToken);

        // Context limit check
        if (allTokens.size() >= (size_t)m_maxSeqLen) break;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double promptMs = std::chrono::duration<double, std::milli>(promptEnd - startTime).count();
    double totalMs  = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    double genMs    = totalMs - promptMs;

    m_lastStats.promptTokens    = static_cast<uint32_t>(inputTokens.size());
    m_lastStats.generatedTokens = static_cast<uint32_t>(generated.size());
    m_lastStats.promptMs        = promptMs;
    m_lastStats.generationMs    = genMs;
    m_lastStats.totalMs         = totalMs;
    m_lastStats.tokensPerSec    = (genMs > 0) ? (generated.size() / (genMs / 1000.0)) : 0.0;

    return generated;
}

// ============================================================================
// Eval (return logits only)
// ============================================================================
std::vector<float> DMLInferenceEngine::Eval(const std::vector<int32_t>& inputTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<float> logits(m_vocabSize, 0.0f);
    if (!m_modelLoaded.load()) return logits;

    m_dmlCompute->runModelForward(
        m_sessionId,
        inputTokens.data(),
        static_cast<uint32_t>(inputTokens.size()),
        logits.data(),
        m_vocabSize
    );

    return logits;
}

// ============================================================================
// Streaming Generation
// ============================================================================
void DMLInferenceEngine::GenerateStreaming(
    const std::vector<int32_t>& inputTokens,
    int maxTokens,
    std::function<void(const std::string&)> tokenCallback,
    std::function<void()> completeCallback,
    std::function<void(int32_t)> tokenIdCallback)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_modelLoaded.load() || !m_dmlCompute) {
        if (completeCallback) completeCallback();
        return;
    }

    m_logitsBuffer.resize(m_vocabSize, 0.0f);
    std::vector<int32_t> allTokens = inputTokens;

    auto startTime = std::chrono::high_resolution_clock::now();
    uint32_t genCount = 0;

    for (int step = 0; step < maxTokens; ++step) {
        auto r = m_dmlCompute->runModelForward(
            m_sessionId,
            allTokens.data(),
            static_cast<uint32_t>(allTokens.size()),
            m_logitsBuffer.data(),
            m_vocabSize
        );

        if (!r.success) break;

        int32_t nextToken = sampleToken(m_logitsBuffer.data(), m_vocabSize, allTokens);
        if (nextToken == m_tokenizer.eosToken) break;

        allTokens.push_back(nextToken);
        genCount++;

        // Stream the token back
        if (tokenIdCallback) tokenIdCallback(nextToken);
        if (tokenCallback) {
            std::string tokenStr;
            if (nextToken >= 0 && nextToken < (int32_t)m_tokenizer.vocab.size()) {
                tokenStr = m_tokenizer.vocab[nextToken];
                // Handle SentencePiece space marker
                if (tokenStr.size() >= 3 && tokenStr[0] == '\xe2' &&
                    tokenStr[1] == '\x96' && tokenStr[2] == '\x81') {
                    tokenStr = " " + tokenStr.substr(3);
                }
            }
            tokenCallback(tokenStr);
        }

        if (allTokens.size() >= (size_t)m_maxSeqLen) break;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_lastStats.generatedTokens = genCount;
    m_lastStats.totalMs = totalMs;
    m_lastStats.tokensPerSec = (totalMs > 0) ? (genCount / (totalMs / 1000.0)) : 0.0;

    if (completeCallback) completeCallback();
}

// ============================================================================
// Sampling
// ============================================================================
int32_t DMLInferenceEngine::sampleToken(float* logits, int vocabSize,
                                          const std::vector<int32_t>& prevTokens) {
    if (m_samplingParams.greedy) {
        // Greedy: argmax
        int32_t bestId = 0;
        float bestVal = logits[0];
        for (int i = 1; i < vocabSize; ++i) {
            if (logits[i] > bestVal) {
                bestVal = logits[i];
                bestId = i;
            }
        }
        return bestId;
    }

    // Apply repetition penalty
    if (m_samplingParams.repeatPenalty != 1.0f) {
        applyRepetitionPenalty(logits, vocabSize, prevTokens,
                               m_samplingParams.repeatPenalty,
                               m_samplingParams.repeatWindow);
    }

    // Apply temperature
    if (m_samplingParams.temperature > 0.0f && m_samplingParams.temperature != 1.0f) {
        applyTemperature(logits, vocabSize, m_samplingParams.temperature);
    }

    // Apply top-k
    if (m_samplingParams.topK > 0 && m_samplingParams.topK < (uint32_t)vocabSize) {
        topKFilter(logits, vocabSize, m_samplingParams.topK);
    }

    // Apply top-p
    if (m_samplingParams.topP > 0.0f && m_samplingParams.topP < 1.0f) {
        topPFilter(logits, vocabSize, m_samplingParams.topP);
    }

    // Softmax → probabilities
    softmax(logits, vocabSize);

    // Multinomial sampling
    static thread_local std::mt19937 rng(
        m_samplingParams.seed != 0 ? m_samplingParams.seed
                                    : std::random_device{}()
    );

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng);
    float cumSum = 0.0f;

    for (int i = 0; i < vocabSize; ++i) {
        cumSum += logits[i];
        if (cumSum >= r) return i;
    }

    return vocabSize - 1;
}

void DMLInferenceEngine::applyTemperature(float* logits, int vocabSize, float temp) {
    float invTemp = 1.0f / temp;
    for (int i = 0; i < vocabSize; ++i) {
        logits[i] *= invTemp;
    }
}

void DMLInferenceEngine::topKFilter(float* logits, int vocabSize, int k) {
    // Find the k-th largest value
    std::vector<float> sorted(logits, logits + vocabSize);
    std::nth_element(sorted.begin(), sorted.begin() + k - 1, sorted.end(), std::greater<float>());
    float threshold = sorted[k - 1];

    // Zero out everything below threshold
    for (int i = 0; i < vocabSize; ++i) {
        if (logits[i] < threshold) logits[i] = -1e30f;
    }
}

void DMLInferenceEngine::topPFilter(float* logits, int vocabSize, float p) {
    // Sort indices by logit value descending
    std::vector<int> indices(vocabSize);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [logits](int a, int b) { return logits[a] > logits[b]; });

    // Compute softmax for sorted logits
    float maxLogit = logits[indices[0]];
    float sumExp = 0.0f;
    for (int i = 0; i < vocabSize; ++i) {
        sumExp += expf(logits[indices[i]] - maxLogit);
    }

    float cumProb = 0.0f;
    bool cutoff = false;
    for (int i = 0; i < vocabSize; ++i) {
        if (cutoff) {
            logits[indices[i]] = -1e30f;
        } else {
            float prob = expf(logits[indices[i]] - maxLogit) / sumExp;
            cumProb += prob;
            if (cumProb > p) cutoff = true;
        }
    }
}

void DMLInferenceEngine::applyRepetitionPenalty(float* logits, int vocabSize,
                                                  const std::vector<int32_t>& prevTokens,
                                                  float penalty, uint32_t window) {
    uint32_t start = (prevTokens.size() > window)
                     ? static_cast<uint32_t>(prevTokens.size() - window) : 0;

    for (uint32_t i = start; i < prevTokens.size(); ++i) {
        int32_t tok = prevTokens[i];
        if (tok >= 0 && tok < vocabSize) {
            if (logits[tok] > 0) {
                logits[tok] /= penalty;
            } else {
                logits[tok] *= penalty;
            }
        }
    }
}

void DMLInferenceEngine::softmax(float* logits, int vocabSize) {
    float maxVal = *std::max_element(logits, logits + vocabSize);
    float sum = 0.0f;
    for (int i = 0; i < vocabSize; ++i) {
        logits[i] = expf(logits[i] - maxVal);
        sum += logits[i];
    }
    float invSum = 1.0f / sum;
    for (int i = 0; i < vocabSize; ++i) {
        logits[i] *= invSum;
    }
}

// ============================================================================
// Memory & Cache
// ============================================================================
size_t DMLInferenceEngine::GetMemoryUsage() const {
    if (!m_ggufBridge) return 0;
    return static_cast<size_t>(m_ggufBridge->getVRAMUsed(m_sessionId));
}

void DMLInferenceEngine::ClearCache() {
    // Clear KV cache by resetting sequence length
    if (m_dmlCompute) {
        auto* session = m_dmlCompute->getSession(m_sessionId);
        if (session) session->kvSeqLen = 0;
    }
}

// ============================================================================
// Backend Registration
// ============================================================================
void DMLInferenceEngine::RegisterAsBackend(AIBackendManager* mgr) {
    if (!mgr) return;

    AIBackendConfig cfg;
    cfg.id          = "directml";
    cfg.displayName = "DirectML GPU";
    cfg.type        = AIBackendType::Custom;
    cfg.endpoint    = "";   // Local, no endpoint
    cfg.model       = m_modelPath;
    cfg.maxTokens   = 2048;
    cfg.temperature = m_samplingParams.temperature;
    cfg.enabled     = true;

    mgr->addBackend(cfg);
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string DMLInferenceEngine::GetDiagnostics() const {
    std::ostringstream ss;
    ss << "=== DML Inference Engine ===\n";
    ss << "Model: " << m_modelPath << "\n";
    ss << "Architecture: " << m_architecture << "\n";
    ss << "Loaded: " << (m_modelLoaded.load() ? "Yes" : "No") << "\n";
    ss << "Vocab: " << m_vocabSize << "\n";
    ss << "Layers: " << m_numLayers << "\n";
    ss << "Heads: " << m_numHeads << " (KV: " << m_numKVHeads << ")\n";
    ss << "Hidden: " << m_embeddingDim << "\n";
    ss << "Max Seq: " << m_maxSeqLen << "\n";
    ss << "Context: " << m_contextSize << "\n";
    ss << "\nLast Generation:\n";
    ss << "  Prompt tokens: " << m_lastStats.promptTokens << "\n";
    ss << "  Generated: " << m_lastStats.generatedTokens << "\n";
    ss << "  Total time: " << m_lastStats.totalMs << " ms\n";
    ss << "  Tokens/sec: " << m_lastStats.tokensPerSec << "\n";

    if (m_ggufBridge) {
        ss << "\n" << m_ggufBridge->getDiagnostics();
    }
    if (m_dmlCompute) {
        ss << "\n" << m_dmlCompute->getDiagnosticsString();
    }

    return ss.str();
}

} // namespace RawrXD
