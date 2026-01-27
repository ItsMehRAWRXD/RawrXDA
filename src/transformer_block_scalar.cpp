// Transformer Block Scalar Implementation
// Optimized CPU implementation of transformer blocks for inference

#include "transformer_block_scalar.h"
#include <cstring>
#include <cmath>
#include <immintrin.h>
#include <QDebug>

TransformerBlockScalar::TransformerBlockScalar(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_layerCount(0)
    , m_headCount(0)
    , m_headDim(0)
    , m_hiddenDim(0)
{
}

TransformerBlockScalar::~TransformerBlockScalar()
{
    cleanup();
}

bool TransformerBlockScalar::initialize(uint32_t layerCount, uint32_t headCount, 
                                       uint32_t headDim, uint32_t hiddenDim)
{
    if (m_initialized) {
        cleanup();
    }
    
    m_layerCount = layerCount;
    m_headCount = headCount;
    m_headDim = headDim;
    m_hiddenDim = hiddenDim;
    
    // Allocate memory for weights and activations
    try {
        // Attention weights (Q, K, V, O)
        m_qWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim);
        m_kWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim);
        m_vWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim);
        m_oWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim);
        
        // Feed-forward weights
        m_ffnUpWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim * 4);
        m_ffnDownWeights.resize(m_layerCount * m_hiddenDim * m_hiddenDim * 4);
        
        // Layer normalization parameters
        m_attentionNormWeights.resize(m_layerCount * m_hiddenDim);
        m_attentionNormBiases.resize(m_layerCount * m_hiddenDim);
        m_ffnNormWeights.resize(m_layerCount * m_hiddenDim);
        m_ffnNormBiases.resize(m_layerCount * m_hiddenDim);
        
        // Activation buffers
        m_attentionOutput.resize(m_hiddenDim);
        m_ffnOutput.resize(m_hiddenDim);
        
    } catch (const std::bad_alloc&) {
        qCritical() << "Failed to allocate memory for transformer block";
        return false;
    }
    
    m_initialized = true;
    qDebug() << "TransformerBlockScalar initialized with" << layerCount << "layers";
    return true;
}

void TransformerBlockScalar::cleanup()
{
    m_qWeights.clear();
    m_kWeights.clear();
    m_vWeights.clear();
    m_oWeights.clear();
    m_ffnUpWeights.clear();
    m_ffnDownWeights.clear();
    m_attentionNormWeights.clear();
    m_attentionNormBiases.clear();
    m_ffnNormWeights.clear();
    m_ffnNormBiases.clear();
    m_attentionOutput.clear();
    m_ffnOutput.clear();
    
    m_initialized = false;
}

bool TransformerBlockScalar::forwardPass(const float *input, float *output, 
                                        uint32_t layerIdx, uint32_t seqLen)
{
    if (!m_initialized || layerIdx >= m_layerCount) {
        return false;
    }
    
    // Layer normalization for attention
    if (!layerNorm(input, m_attentionNormWeights.data() + layerIdx * m_hiddenDim,
                   m_attentionNormBiases.data() + layerIdx * m_hiddenDim, 
                   m_attentionOutput.data(), seqLen)) {
        return false;
    }
    
    // Self-attention
    if (!selfAttention(m_attentionOutput.data(), output, layerIdx, seqLen)) {
        return false;
    }
    
    // Residual connection
    for (uint32_t i = 0; i < seqLen * m_hiddenDim; ++i) {
        output[i] += input[i];
    }
    
    // Layer normalization for FFN
    if (!layerNorm(output, m_ffnNormWeights.data() + layerIdx * m_hiddenDim,
                   m_ffnNormBiases.data() + layerIdx * m_hiddenDim, 
                   m_ffnOutput.data(), seqLen)) {
        return false;
    }
    
    // Feed-forward network
    if (!feedForward(m_ffnOutput.data(), output, layerIdx, seqLen)) {
        return false;
    }
    
    // Final residual connection
    for (uint32_t i = 0; i < seqLen * m_hiddenDim; ++i) {
        output[i] += m_attentionOutput.data()[i];
    }
    
    return true;
}

bool TransformerBlockScalar::selfAttention(const float *input, float *output, 
                                          uint32_t layerIdx, uint32_t seqLen)
{
    const float *qWeights = m_qWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim;
    const float *kWeights = m_kWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim;
    const float *vWeights = m_vWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim;
    const float *oWeights = m_oWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim;
    
    // Compute Q, K, V matrices
    std::vector<float> qMatrix(seqLen * m_hiddenDim);
    std::vector<float> kMatrix(seqLen * m_hiddenDim);
    std::vector<float> vMatrix(seqLen * m_hiddenDim);
    
    if (!matMul(input, qWeights, qMatrix.data(), seqLen, m_hiddenDim, m_hiddenDim)) {
        return false;
    }
    if (!matMul(input, kWeights, kMatrix.data(), seqLen, m_hiddenDim, m_hiddenDim)) {
        return false;
    }
    if (!matMul(input, vWeights, vMatrix.data(), seqLen, m_hiddenDim, m_hiddenDim)) {
        return false;
    }
    
    // Compute attention scores: Q * K^T / sqrt(head_dim)
    std::vector<float> attentionScores(seqLen * seqLen);
    float scale = 1.0f / std::sqrt(static_cast<float>(m_headDim));
    
    for (uint32_t i = 0; i < seqLen; ++i) {
        for (uint32_t j = 0; j < seqLen; ++j) {
            float score = 0.0f;
            for (uint32_t k = 0; k < m_hiddenDim; ++k) {
                score += qMatrix[i * m_hiddenDim + k] * kMatrix[j * m_hiddenDim + k];
            }
            attentionScores[i * seqLen + j] = score * scale;
        }
    }
    
    // Apply softmax
    if (!softmax(attentionScores.data(), seqLen, seqLen)) {
        return false;
    }
    
    // Compute weighted sum: attention_scores * V
    std::vector<float> weightedV(seqLen * m_hiddenDim);
    for (uint32_t i = 0; i < seqLen; ++i) {
        for (uint32_t j = 0; j < m_hiddenDim; ++j) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < seqLen; ++k) {
                sum += attentionScores[i * seqLen + k] * vMatrix[k * m_hiddenDim + j];
            }
            weightedV[i * m_hiddenDim + j] = sum;
        }
    }
    
    // Final projection: weightedV * O
    if (!matMul(weightedV.data(), oWeights, output, seqLen, m_hiddenDim, m_hiddenDim)) {
        return false;
    }
    
    return true;
}

bool TransformerBlockScalar::feedForward(const float *input, float *output, 
                                        uint32_t layerIdx, uint32_t seqLen)
{
    const float *upWeights = m_ffnUpWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim * 4;
    const float *downWeights = m_ffnDownWeights.data() + layerIdx * m_hiddenDim * m_hiddenDim * 4;
    
    // First projection: input * up_weights
    std::vector<float> hidden(seqLen * m_hiddenDim * 4);
    if (!matMul(input, upWeights, hidden.data(), seqLen, m_hiddenDim, m_hiddenDim * 4)) {
        return false;
    }
    
    // Activation function (SiLU)
    for (uint32_t i = 0; i < seqLen * m_hiddenDim * 4; ++i) {
        hidden[i] = hidden[i] / (1.0f + std::exp(-hidden[i]));  // SiLU
    }
    
    // Second projection: hidden * down_weights
    if (!matMul(hidden.data(), downWeights, output, seqLen, m_hiddenDim * 4, m_hiddenDim)) {
        return false;
    }
    
    return true;
}

bool TransformerBlockScalar::layerNorm(const float *input, const float *weights, 
                                      const float *biases, float *output, uint32_t seqLen)
{
    const float epsilon = 1e-5f;
    
    for (uint32_t i = 0; i < seqLen; ++i) {
        // Compute mean and variance
        float mean = 0.0f;
        float variance = 0.0f;
        
        for (uint32_t j = 0; j < m_hiddenDim; ++j) {
            mean += input[i * m_hiddenDim + j];
        }
        mean /= m_hiddenDim;
        
        for (uint32_t j = 0; j < m_hiddenDim; ++j) {
            float diff = input[i * m_hiddenDim + j] - mean;
            variance += diff * diff;
        }
        variance /= m_hiddenDim;
        
        // Normalize
        float inv_std = 1.0f / std::sqrt(variance + epsilon);
        
        for (uint32_t j = 0; j < m_hiddenDim; ++j) {
            output[i * m_hiddenDim + j] = weights[j] * 
                (input[i * m_hiddenDim + j] - mean) * inv_std + biases[j];
        }
    }
    
    return true;
}

bool TransformerBlockScalar::matMul(const float *a, const float *b, float *c,
                                   uint32_t m, uint32_t k, uint32_t n)
{
    // Simple matrix multiplication
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (uint32_t l = 0; l < k; ++l) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
    
    return true;
}

bool TransformerBlockScalar::softmax(float *data, uint32_t rows, uint32_t cols)
{
    for (uint32_t i = 0; i < rows; ++i) {
        // Find max for numerical stability
        float max_val = data[i * cols];
        for (uint32_t j = 1; j < cols; ++j) {
            if (data[i * cols + j] > max_val) {
                max_val = data[i * cols + j];
            }
        }
        
        // Compute exponentials and sum
        float sum = 0.0f;
        for (uint32_t j = 0; j < cols; ++j) {
            data[i * cols + j] = std::exp(data[i * cols + j] - max_val);
            sum += data[i * cols + j];
        }
        
        // Normalize
        for (uint32_t j = 0; j < cols; ++j) {
            data[i * cols + j] /= sum;
        }
    }
    
    return true;
}

bool TransformerBlockScalar::loadWeights(const float *weights, uint32_t layerIdx, 
                                        WeightType type)
{
    if (!m_initialized || layerIdx >= m_layerCount) {
        return false;
    }
    
    size_t offset = layerIdx * m_hiddenDim * m_hiddenDim;
    
    switch (type) {
        case WeightType::Q_WEIGHTS:
            std::memcpy(m_qWeights.data() + offset, weights, 
                       m_hiddenDim * m_hiddenDim * sizeof(float));
            break;
        case WeightType::K_WEIGHTS:
            std::memcpy(m_kWeights.data() + offset, weights, 
                       m_hiddenDim * m_hiddenDim * sizeof(float));
            break;
        case WeightType::V_WEIGHTS:
            std::memcpy(m_vWeights.data() + offset, weights, 
                       m_hiddenDim * m_hiddenDim * sizeof(float));
            break;
        case WeightType::O_WEIGHTS:
            std::memcpy(m_oWeights.data() + offset, weights, 
                       m_hiddenDim * m_hiddenDim * sizeof(float));
            break;
        case WeightType::FFN_UP_WEIGHTS:
            std::memcpy(m_ffnUpWeights.data() + offset * 4, weights, 
                       m_hiddenDim * m_hiddenDim * 4 * sizeof(float));
            break;
        case WeightType::FFN_DOWN_WEIGHTS:
            std::memcpy(m_ffnDownWeights.data() + offset * 4, weights, 
                       m_hiddenDim * m_hiddenDim * 4 * sizeof(float));
            break;
        default:
            return false;
    }
    
    return true;
}

bool TransformerBlockScalar::loadNormParams(const float *weights, const float *biases,
                                           uint32_t layerIdx, NormType type)
{
    if (!m_initialized || layerIdx >= m_layerCount) {
        return false;
    }
    
    size_t offset = layerIdx * m_hiddenDim;
    
    switch (type) {
        case NormType::ATTENTION_NORM:
            std::memcpy(m_attentionNormWeights.data() + offset, weights, 
                       m_hiddenDim * sizeof(float));
            std::memcpy(m_attentionNormBiases.data() + offset, biases, 
                       m_hiddenDim * sizeof(float));
            break;
        case NormType::FFN_NORM:
            std::memcpy(m_ffnNormWeights.data() + offset, weights, 
                       m_hiddenDim * sizeof(float));
            std::memcpy(m_ffnNormBiases.data() + offset, biases, 
                       m_hiddenDim * sizeof(float));
            break;
        default:
            return false;
    }
    
    return true;
}