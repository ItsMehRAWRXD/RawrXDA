/**
 * @file sampling_engine.h
 * @brief Advanced sampling with tool calling support
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>

namespace RawrXD::Inference {

// ============================================================================
// Tool Parameter Definition
// ============================================================================

struct ToolParameter {
    enum class Type {
        String,
        Integer,
        Float,
        Boolean,
        Array,
        Object
    };
    
    std::string name;
    Type type;
    std::string description;
    std::string defaultValue;
    bool required = false;
};

// ============================================================================
// Tool Definition
// ============================================================================

struct Tool {
    std::string name;
    std::string description;
    uint32_t triggerToken = 0;
    float triggerThreshold = 0.5f;
    std::vector<ToolParameter> parameters;
};

// ============================================================================
// Tool Registry
// ============================================================================

class ToolRegistry {
public:
    void registerTool(const Tool& tool);
    void unregisterTool(const std::string& name);
    const Tool* findTool(const std::string& name) const;
    std::vector<Tool> getTools() const;
    
private:
    std::unordered_map<std::string, Tool> m_tools;
};

// ============================================================================
// Tool Call Result
// ============================================================================

struct ToolCallResult {
    bool isToolCall = false;
    uint32_t token = 0;
    std::string toolName;
    std::string toolParams;
};

// ============================================================================
// Sampling Configuration
// ============================================================================

struct SamplingConfig {
    float temperature = 0.8f;
    int topK = 40;
    float topP = 0.95f;
    float minP = 0.05f;
    float repetitionPenalty = 1.1f;
    int repetitionContextSize = 64;
};

// ============================================================================
// Sampling Engine
// ============================================================================

class SamplingEngine {
public:
    explicit SamplingEngine(const Config& config);
    ~SamplingEngine();
    
    // Main sampling entry point
    uint32_t sample(const float* logits, int vocabSize,
                    const std::vector<uint32_t>& context);
    
    // Sample multiple tokens
    std::vector<uint32_t> sampleMultiple(const float* logits, int vocabSize,
                                         int count,
                                         const std::vector<uint32_t>& context);
    
    // Tool calling support
    ToolCallResult sampleWithToolCalling(const float* logits, int vocabSize,
                                         const std::vector<uint32_t>& context,
                                         const ToolRegistry& tools);
    
    // Configuration
    void setConfig(const Config& config) { m_config = config; }
    const Config& getConfig() const { return m_config; }
    
private:
    // Sampling strategies
    uint32_t greedySample(const float* logits, int vocabSize);
    uint32_t temperatureSample(const float* logits, int vocabSize, float temperature);
    
    // Filtering strategies
    void applyTopK(float* logits, int vocabSize, int k);
    void applyTopP(float* logits, int vocabSize, float p);
    void applyMinP(float* logits, int vocabSize, float minP);
    void applyTemperature(float* logits, int vocabSize, float temperature);
    void applyRepetitionPenalty(float* logits, int vocabSize,
                               const std::vector<uint32_t>& context);
    
    // Tool calling helpers
    bool shouldTriggerToolCall(const float* logits, int vocabSize,
                              const std::vector<uint32_t>& context,
                              const ToolRegistry& tools);
    std::string selectTool(const float* logits, int vocabSize,
                          const ToolRegistry& tools);
    std::string generateToolParams(const float* logits, int vocabSize,
                                  const std::string& toolName,
                                  const ToolRegistry& tools);
    
    Config m_config;
    std::mt19937_64 m_rng;
};

} // namespace RawrXD::Inference
