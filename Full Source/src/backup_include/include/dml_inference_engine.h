#pragma once
/**
 * @file dml_inference_engine.h
 * @brief DirectML-backed inference engine — API for dml_inference_engine.cpp.
 */
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <mutex>
#include <atomic>
#include <map>

struct AIBackendManager;

namespace RawrXD {

struct BPETokenizer {
    std::vector<std::string> vocab;
    std::vector<float> scores;
    int32_t bosToken = 0;
    int32_t eosToken = 0;
    int32_t padToken = 0;
    int32_t unkToken = 0;
    std::map<std::string, int32_t> tokenToId;
    bool loaded = false;
};

class DMLInferenceEngine {
public:
    DMLInferenceEngine();
    ~DMLInferenceEngine();

    bool LoadModel(const std::string& modelPath);
    bool LoadSecondModel(const std::string& modelPath);
    void UnloadModel();

    std::vector<int32_t> Tokenize(const std::string& text);
    std::vector<int32_t> bpeEncode(const std::string& text);
    std::string Detokenize(const std::vector<int32_t>& tokens);
    std::string bpeDecode(const std::vector<int32_t>& tokens);

    std::vector<int32_t> Generate(const std::vector<int32_t>& inputTokens, int maxTokens);
    std::vector<float> Eval(const std::vector<int32_t>& inputTokens);

    void GenerateStreaming(
        const std::vector<int32_t>& inputTokens,
        int maxTokens,
        std::function<void(const std::string&)> tokenCallback,
        std::function<void()> completeCallback,
        std::function<void(int32_t)> tokenIdCallback);

    size_t GetMemoryUsage() const;
    void ClearCache();
    void RegisterAsBackend(AIBackendManager* mgr);
    std::string GetDiagnostics() const;

private:
    bool loadTokenizer(const std::string& ggufPath);
    int32_t sampleToken(float* logits, int vocabSize, const std::vector<int32_t>& context);
    void applyTemperature(float* logits, int vocabSize, float temp);
    void topKFilter(float* logits, int vocabSize, int k);
    void topPFilter(float* logits, int vocabSize, float p);
    void applyRepetitionPenalty(float* logits, int vocabSize, const std::vector<int32_t>& context, float penalty, uint32_t window);
    void softmax(float* logits, int vocabSize);

    std::mutex m_mutex;
    std::atomic<bool> m_modelLoaded{false};
    void* m_dmlCompute = nullptr;
    void* m_ggufBridge = nullptr;
    uint32_t m_sessionId = 0;
    int m_vocabSize = 0;
    int m_embeddingDim = 0;
    int m_numLayers = 0;
    int m_numHeads = 0;
    int m_numKVHeads = 0;
    int m_maxSeqLen = 0;
    std::string m_architecture;
    std::string m_modelPath;
    BPETokenizer m_tokenizer;
    std::vector<float> m_logitsBuffer;
    struct { uint32_t promptTokens = 0, generatedTokens = 0; double promptMs = 0, generationMs = 0, totalMs = 0, tokensPerSec = 0; } m_lastStats;
};

} // namespace RawrXD
