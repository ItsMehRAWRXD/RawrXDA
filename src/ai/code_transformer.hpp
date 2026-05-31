// ============================================================================
// code_transformer.hpp — AI-Powered Code Transformer
// Uses SovereignInferenceClient for intelligent code transformations.
//
// Design: Safe transformation pipeline with validation gates
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured TransformationResult returns only.
// Rule: All threading via std::mutex + std::lock_guard.
// ============================================================================
#pragma once

#ifndef RAWRXD_CODE_TRANSFORMER_HPP
#define RAWRXD_CODE_TRANSFORMER_HPP

#include "transformation_types.hpp"
#include "../agentic/SovereignInferenceClient.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace RawrXD {
namespace AI {

// ============================================================================
// Syntax Validator — Validates transformed code
// ============================================================================
class SyntaxValidator {
public:
    static bool ValidateCppSyntax(const std::string& code);
    static bool ValidateMASM(const std::string& code);
    static bool ValidateCMake(const std::string& code);
    
private:
    static bool HasBalancedBraces(const std::string& code);
    static bool HasBalancedParentheses(const std::string& code);
    static bool HasValidSemicolons(const std::string& code);
};

// ============================================================================
// Type Safety Validator — Ensures type correctness
// ============================================================================
class TypeSafetyValidator {
public:
    static bool ValidateTypeSafety(const std::string& original, 
                                   const std::string& transformed);
    
private:
    static std::vector<std::string> ExtractTypeSignatures(const std::string& code);
};

// ============================================================================
// Code Transformer — Main transformation engine
// ============================================================================
class CodeTransformer {
public:
    explicit CodeTransformer(
        std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> inferenceClient = nullptr);
    ~CodeTransformer();

    // Initialize with learned patterns
    bool Initialize(const std::string& patternsPath = "");
    
    // Core transformation API
    TransformationResult TransformCode(const std::string& code,
                                     TransformationType type,
                                     const TransformationConstraints& constraints);
    
    // Context-aware transformation (with editor context)
    TransformationResult TransformCodeWithContext(const std::string& code,
                                               TransformationType type,
                                               const TransformationConstraints& constraints,
                                               const TransformationContext& context);
    
    // Get AI-suggested transformations
    std::vector<TransformationSuggestion> SuggestTransformations(const std::string& code);
    std::vector<TransformationSuggestion> SuggestTransformationsWithContext(
        const std::string& code, const TransformationContext& context);
    
    // Learning system
    void LearnFromExample(const std::string& before, const std::string& after,
                         TransformationType type);
    void SaveTransformationPattern(const TransformationPattern& pattern);
    void LoadTransformationPatterns(const std::string& path);
    
    // Pattern management
    std::vector<TransformationPattern> GetPatternsByType(TransformationType type) const;
    std::vector<TransformationPattern> GetPatternsByTag(const std::string& tag) const;
    void IncrementPatternUsage(const std::string& patternId);
    
    // Configuration
    void SetMinConfidence(float confidence) { m_minConfidence = confidence; }
    float GetMinConfidence() const { return m_minConfidence; }
    
    void SetMaxTokens(uint32_t tokens) { m_maxTokens = tokens; }
    uint32_t GetMaxTokens() const { return m_maxTokens; }
    
    void SetTemperature(float temp) { m_temperature = temp; }
    float GetTemperature() const { return m_temperature; }
    
    bool IsInitialized() const { return m_initialized; }

private:
    // Prompt building
    std::string BuildTransformationPrompt(const std::string& code,
                                        TransformationType type,
                                        const TransformationConstraints& constraints);
    std::string BuildSuggestionPrompt(const std::string& code,
                                     const TransformationContext& context);
    std::string BuildLearningPrompt(const std::string& before, const std::string& after);
    
    // Safe transformation pipeline
    TransformationResult ApplySafeTransformation(const std::string& original,
                                               const std::string& transformed,
                                               TransformationType type);
    
    // Validation gates
    bool ValidateSyntax(const std::string& code, const std::string& languageId);
    bool ValidateTypeSafety(const std::string& original, const std::string& transformed);
    bool ValidateBehaviorPreservation(const std::string& original, const std::string& transformed);
    
    // Parsing
    std::vector<TransformationSuggestion> ParseSuggestions(const std::string& response);
    TransformationPattern ParsePattern(const std::string& response);
    
    // Metrics
    void CalculateMetrics(TransformationResult& result);
    
    // File extension helper
    std::string GetFileExtension(TransformationType type);

    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> m_inferenceClient;
    std::vector<TransformationPattern> m_learnedPatterns;
    std::unordered_map<std::string, TransformationPattern> m_patternIndex;
    
    float m_minConfidence = 0.7f;
    uint32_t m_maxTokens = 4096;
    float m_temperature = 0.2f;  // Low temperature for deterministic transformations
    bool m_initialized = false;
    
    mutable std::mutex m_mutex;
};

} // namespace AI
} // namespace RawrXD

#endif // RAWRXD_CODE_TRANSFORMER_HPP
