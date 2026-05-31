/**
 * @file sampling_engine.cpp
 * @brief Advanced sampling with tool calling support
 * 
 * Implements various sampling strategies for language model inference:
 * - Greedy decoding
 * - Temperature sampling
 * - Top-k sampling
 * - Top-p (nucleus) sampling
 * - Min-p sampling
 * - Repetition penalty
 * - Tool calling support with structured output
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "sampling_engine.h"
#include <math>
#include <algorithm>
#include <random>
#include <chrono>
#include <string>

namespace RawrXD::Inference {

// ============================================================================
// SamplingEngine Implementation
// ============================================================================

SamplingEngine::SamplingEngine(const Config& config)
    : m_config(config)
    , m_rng(static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

SamplingEngine::~SamplingEngine() = default;

// ============================================================================
// Main Sampling Entry Point
// ============================================================================

uint32_t SamplingEngine::sample(const float* logits, int vocabSize,
                                const std::vector<uint32_t>& context) {
    // Apply repetition penalty first
    std::vector<float> modifiedLogits(logits, logits + vocabSize);
    
    if (m_config.repetitionPenalty > 1.0f) {
        applyRepetitionPenalty(modifiedLogits.data(), vocabSize, context);
    }
    
    // Apply temperature
    if (m_config.temperature > 0.0f && m_config.temperature != 1.0f) {
        applyTemperature(modifiedLogits.data(), vocabSize, m_config.temperature);
    }
    
    // Apply min-p filtering
    if (m_config.minP > 0.0f) {
        applyMinP(modifiedLogits.data(), vocabSize, m_config.minP);
    }
    
    // Apply top-k filtering
    if (m_config.topK > 0 && m_config.topK < vocabSize) {
        applyTopK(modifiedLogits.data(), vocabSize, m_config.topK);
    }
    
    // Apply top-p filtering
    if (m_config.topP < 1.0f) {
        applyTopP(modifiedLogits.data(), vocabSize, m_config.topP);
    }
    
    // Greedy or temperature sampling
    if (m_config.temperature <= 0.0f) {
        return greedySample(modifiedLogits.data(), vocabSize);
    } else {
        return temperatureSample(modifiedLogits.data(), vocabSize, m_config.temperature);
    }
}

std::vector<uint32_t> SamplingEngine::sampleMultiple(const float* logits, int vocabSize,
                                                     int count,
                                                     const std::vector<uint32_t>& context) {
    std::vector<uint32_t> results;
    results.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        uint32_t token = sample(logits, vocabSize, context);
        results.push_back(token);
    }
    
    return results;
}

// ============================================================================
// Tool Calling Support
// ============================================================================

ToolCallResult SamplingEngine::sampleWithToolCalling(
    const float* logits, int vocabSize,
    const std::vector<uint32_t>& context,
    const ToolRegistry& tools) {
    
    ToolCallResult result;
    
    // Check if we should trigger a tool call
    if (shouldTriggerToolCall(logits, vocabSize, context, tools)) {
        result.isToolCall = true;
        result.toolName = selectTool(logits, vocabSize, tools);
        result.toolParams = generateToolParams(logits, vocabSize, result.toolName, tools);
        return result;
    }
    
    // Regular sampling
    result.isToolCall = false;
    result.token = sample(logits, vocabSize, context);
    return result;
}

bool SamplingEngine::shouldTriggerToolCall(
    const float* logits, int vocabSize,
    const std::vector<uint32_t>& context,
    const ToolRegistry& tools) {
    
    // Check for tool trigger tokens
    for (const auto& tool : tools.getTools()) {
        if (tool.triggerToken < vocabSize && logits[tool.triggerToken] > tool.triggerThreshold) {
            return true;
        }
    }
    
    // Check context for tool call patterns
    if (context.size() >= 2) {
        // Look for patterns like "call:" or "invoke:" in recent context
        // This is a simplified check - production would use more sophisticated detection
        for (const auto& tool : tools.getTools()) {
            if (context.back() == tool.triggerToken) {
                return true;
            }
        }
    }
    
    return false;
}

std::string SamplingEngine::selectTool(const float* logits, int vocabSize,
                                       const ToolRegistry& tools) {
    // Agentic tool selection: optimize based on confidence scores
    printf("[Agentic] Optimizing tool selection...\n");
    
    // Select tool with highest probability among tool trigger tokens
    std::string bestTool;
    float bestProb = -std::numeric_limits<float>::infinity();
    
    for (const auto& tool : tools.getTools()) {
        if (tool.triggerToken < vocabSize && logits[tool.triggerToken] > bestProb) {
            bestProb = logits[tool.triggerToken];
            bestTool = tool.name;
        }
    }
    
    return bestTool.empty() ? "default" : bestTool;
}

std::string SamplingEngine::generateToolParams(const float* logits, int vocabSize,
                                               const std::string& toolName,
                                               const ToolRegistry& tools) {
    // Generate structured parameters for the tool
    // This is a simplified implementation - production would use constrained decoding
    auto tool = tools.findTool(toolName);
    if (!tool) {
        return "{}";
    }
    
    std::string params = "{";
    bool first = true;
    
    for (const auto& param : tool->parameters) {
        if (!first) params += ",";
        first = false;
        
        params += "\"" + param.name + "\":";
        
        switch (param.type) {
            case ToolParameter::Type::String:
                params += "\"" + param.defaultValue + "\"";
                break;
            case ToolParameter::Type::Integer:
                params += param.defaultValue.empty() ? "0" : param.defaultValue;
                break;
            case ToolParameter::Type::Float:
                params += param.defaultValue.empty() ? "0.0" : param.defaultValue;
                break;
            case ToolParameter::Type::Boolean:
                params += param.defaultValue.empty() ? "false" : param.defaultValue;
                break;
            case ToolParameter::Type::Array:
                params += "[]";
                break;
            case ToolParameter::Type::Object:
                params += "{}";
                break;
        }
    }
    
    params += "}";
    return params;
}

// ============================================================================
// Sampling Strategies
// ============================================================================

uint32_t SamplingEngine::greedySample(const float* logits, int vocabSize) {
    uint32_t bestToken = 0;
    float bestLogit = logits[0];
    
    for (int i = 1; i < vocabSize; ++i) {
        if (logits[i] > bestLogit) {
            bestLogit = logits[i];
            bestToken = i;
        }
    }
    
    return bestToken;
}

uint32_t SamplingEngine::temperatureSample(const float* logits, int vocabSize,
                                         float temperature) {
    // Convert logits to probabilities with temperature
    std::vector<float> probs(vocabSize);
    float maxLogit = *std::max_element(logits, logits + vocabSize);
    float sum = 0.0f;
    
    for (int i = 0; i < vocabSize; ++i) {
        probs[i] = std::exp((logits[i] - maxLogit) / temperature);
        sum += probs[i];
    }
    
    // Normalize
    for (int i = 0; i < vocabSize; ++i) {
        probs[i] /= sum;
    }
    
    // Sample from distribution
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(m_rng);
    float cumsum = 0.0f;
    
    for (int i = 0; i < vocabSize; ++i) {
        cumsum += probs[i];
        if (r <= cumsum) {
            return i;
        }
    }
    
    return vocabSize - 1;
}

// ============================================================================
// Filtering Strategies
// ============================================================================

void SamplingEngine::applyTopK(float* logits, int vocabSize, int k) {
    if (k >= vocabSize) return;
    
    // Find k-th largest logit
    std::vector<float> sortedLogits(logits, logits + vocabSize);
    std::nth_element(sortedLogits.begin(), sortedLogits.begin() + (vocabSize - k), sortedLogits.end());
    float kthLogit = sortedLogits[vocabSize - k];
    
    // Mask out tokens below k-th
    for (int i = 0; i < vocabSize; ++i) {
        if (logits[i] < kthLogit) {
            logits[i] = -std::numeric_limits<float>::infinity();
        }
    }
}

void SamplingEngine::applyTopP(float* logits, int vocabSize, float p) {
    if (p >= 1.0f) return;
    
    // Sort logits in descending order
    std::vector<std::pair<float, int>> indexedLogits;
    indexedLogits.reserve(vocabSize);
    for (int i = 0; i < vocabSize; ++i) {
        indexedLogits.emplace_back(logits[i], i);
    }
    
    std::sort(indexedLogits.begin(), indexedLogits.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Convert to probabilities
    float maxLogit = indexedLogits[0].first;
    std::vector<float> probs(vocabSize);
    float sum = 0.0f;
    
    for (int i = 0; i < vocabSize; ++i) {
        probs[i] = std::exp(indexedLogits[i].first - maxLogit);
        sum += probs[i];
    }
    
    // Find cutoff
    float cumsum = 0.0f;
    int cutoff = vocabSize;
    for (int i = 0; i < vocabSize; ++i) {
        cumsum += probs[i] / sum;
        if (cumsum > p) {
            cutoff = i + 1;
            break;
        }
    }
    
    // Mask out tokens beyond cutoff
    for (int i = cutoff; i < vocabSize; ++i) {
        logits[indexedLogits[i].second] = -std::numeric_limits<float>::infinity();
    }
}

void SamplingEngine::applyMinP(float* logits, int vocabSize, float minP) {
    if (minP <= 0.0f) return;
    
    // Find max probability
    float maxLogit = *std::max_element(logits, logits + vocabSize);
    float maxProb = std::exp(maxLogit - maxLogit); // = 1.0 after normalization
    
    // Calculate threshold
    float threshold = std::log(minP * maxProb);
    
    // Mask out tokens below threshold
    for (int i = 0; i < vocabSize; ++i) {
        if (logits[i] - maxLogit < threshold) {
            logits[i] = -std::numeric_limits<float>::infinity();
        }
    }
}

void SamplingEngine::applyTemperature(float* logits, int vocabSize, float temperature) {
    if (temperature <= 0.0f || temperature == 1.0f) return;
    
    for (int i = 0; i < vocabSize; ++i) {
        logits[i] /= temperature;
    }
}

void SamplingEngine::applyRepetitionPenalty(float* logits, int vocabSize,
                                           const std::vector<uint32_t>& context) {
    if (context.empty()) return;
    
    // Count token frequencies in context
    std::unordered_map<uint32_t, int> tokenCounts;
    for (uint32_t token : context) {
        tokenCounts[token]++;
    }
    
    // Apply penalty
    for (const auto& [token, count] : tokenCounts) {
        if (token < static_cast<uint32_t>(vocabSize)) {
            float penalty = std::pow(m_config.repetitionPenalty, count);
            if (logits[token] > 0.0f) {
                logits[token] /= penalty;
            } else {
                logits[token] *= penalty;
            }
        }
    }
}

// ============================================================================
// ToolRegistry Implementation
// ============================================================================

void ToolRegistry::registerTool(const Tool& tool) {
    m_tools[tool.name] = tool;
}

void ToolRegistry::unregisterTool(const std::string& name) {
    m_tools.erase(name);
}

const Tool* ToolRegistry::findTool(const std::string& name) const {
    auto it = m_tools.find(name);
    return (it != m_tools.end()) ? &it->second : nullptr;
}

std::vector<Tool> ToolRegistry::getTools() const {
    std::vector<Tool> result;
    result.reserve(m_tools.size());
    for (const auto& [name, tool] : m_tools) {
        result.push_back(tool);
    }
    return result;
}

} // namespace RawrXD::Inference
