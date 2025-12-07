#pragma once

#include <QObject>
#include <vector>
#include <cstdint>

class TransformerBlockScalar : public QObject
{
    Q_OBJECT

public:
    enum class WeightType {
        Q_WEIGHTS,
        K_WEIGHTS,
        V_WEIGHTS,
        O_WEIGHTS,
        FFN_UP_WEIGHTS,
        FFN_DOWN_WEIGHTS
    };
    
    enum class NormType {
        ATTENTION_NORM,
        FFN_NORM
    };
    
    explicit TransformerBlockScalar(QObject *parent = nullptr);
    ~TransformerBlockScalar();
    
    bool initialize(uint32_t layerCount, uint32_t headCount, 
                   uint32_t headDim, uint32_t hiddenDim);
    void cleanup();
    
    bool forwardPass(const float *input, float *output, 
                    uint32_t layerIdx, uint32_t seqLen);
    
    bool loadWeights(const float *weights, uint32_t layerIdx, WeightType type);
    bool loadNormParams(const float *weights, const float *biases,
                       uint32_t layerIdx, NormType type);
    
    bool isInitialized() const { return m_initialized; }
    uint32_t getLayerCount() const { return m_layerCount; }
    uint32_t getHeadCount() const { return m_headCount; }
    uint32_t getHeadDim() const { return m_headDim; }
    uint32_t getHiddenDim() const { return m_hiddenDim; }

private:
    bool selfAttention(const float *input, float *output, 
                      uint32_t layerIdx, uint32_t seqLen);
    bool feedForward(const float *input, float *output, 
                    uint32_t layerIdx, uint32_t seqLen);
    bool layerNorm(const float *input, const float *weights, 
                  const float *biases, float *output, uint32_t seqLen);
    bool matMul(const float *a, const float *b, float *c,
               uint32_t m, uint32_t k, uint32_t n);
    bool softmax(float *data, uint32_t rows, uint32_t cols);
    
    bool m_initialized;
    uint32_t m_layerCount;
    uint32_t m_headCount;
    uint32_t m_headDim;
    uint32_t m_hiddenDim;
    
    // Weight storage
    std::vector<float> m_qWeights;
    std::vector<float> m_kWeights;
    std::vector<float> m_vWeights;
    std::vector<float> m_oWeights;
    std::vector<float> m_ffnUpWeights;
    std::vector<float> m_ffnDownWeights;
    
    // Normalization parameters
    std::vector<float> m_attentionNormWeights;
    std::vector<float> m_attentionNormBiases;
    std::vector<float> m_ffnNormWeights;
    std::vector<float> m_ffnNormBiases;
    
    // Activation buffers
    std::vector<float> m_attentionOutput;
    std::vector<float> m_ffnOutput;
};