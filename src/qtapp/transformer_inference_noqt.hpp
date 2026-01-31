/**
 * transformer_inference_noqt.hpp
 * Pure C++ transformer inference engine without Qt dependencies
 */

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cstdint>
#include <ggml.h>

struct LayerWeights {
    struct ggml_tensor* attn_q = nullptr;
    struct ggml_tensor* attn_k = nullptr;
    struct ggml_tensor* attn_v = nullptr;
    struct ggml_tensor* attn_out = nullptr;
    struct ggml_tensor* attn_norm = nullptr;
    
    struct ggml_tensor* mlp_gate = nullptr;
    struct ggml_tensor* mlp_up = nullptr;
    struct ggml_tensor* mlp_down = nullptr;
    struct ggml_tensor* mlp_norm = nullptr;
};

class TransformerInference {
public:
    TransformerInference();
    ~TransformerInference();
    
    // Weight loading using std::map and std::vector instead of QHash/QByteArray
    bool loadWeights(const std::map<std::string, std::pair<std::vector<uint8_t>, int>>& tensorCache,
                     int nLayers, int nEmbd, int nHead, int nVocab);
    bool loadWeightsWithTypes(const std::map<std::string, std::pair<std::vector<uint8_t>, int>>& tensorCache,
                              int nLayers, int nEmbd, int nHead, int nVocab);
    
    // Inference
    std::vector<int32_t> generateTokens(const std::vector<int32_t>& inputTokens, int maxTokens);
    std::vector<float> getTokenEmbedding(int32_t tokenId);
    std::vector<float> getLogits(const std::vector<int32_t>& inputTokens);
    
    // Utility
    void freeContext();
    bool isReady() const { return m_ready; }
    void markReadyForGGUFInference() { m_ready = true; }
    
private:
    struct ggml_context* m_ctx = nullptr;
    struct ggml_context* m_kvCtx = nullptr;
    
    struct ggml_tensor* m_tokenEmbed = nullptr;
    struct ggml_tensor* m_outputWeight = nullptr;
    struct ggml_tensor* m_posEmbed = nullptr;
    
    std::vector<LayerWeights> m_layers;
    
    int m_nLayers = 0;
    int m_nEmbd = 0;
    int m_nHead = 0;
    int m_nVocab = 0;
    
    bool m_ready = false;
    
    // Helper methods
    struct ggml_tensor* createTensorFromCache(const std::vector<uint8_t>& data,
                                              int ggmlType,
                                              const std::vector<int64_t>& shape);
    struct ggml_tensor* createTensorRef(const uint8_t* data,
                                        int ggmlType,
                                        const std::vector<int64_t>& shape);
    
    std::vector<float> forward(const std::vector<int32_t>& inputTokens);
    std::vector<int32_t> sample(const std::vector<float>& logits, int topK = 40, float topP = 0.95f);
};
