// ============================================================================
// transformation_types.hpp — Code Transformation Type System
// Defines all transformation types, constraints, results, and patterns.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured results only.
// ============================================================================
#pragma once

#ifndef RAWRXD_TRANSFORMATION_TYPES_HPP
#define RAWRXD_TRANSFORMATION_TYPES_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace RawrXD {
namespace AI {

// ============================================================================
// Transformation Type — What kind of code transformation to perform
// ============================================================================
enum class TransformationType : uint32_t {
    Unknown = 0,
    
    // Refactoring transformations
    RefactorExtractMethod = 1,
    RefactorInlineVariable = 2,
    RefactorRenameSymbol = 3,
    RefactorExtractClass = 4,
    RefactorMoveMethod = 5,
    RefactorIntroduceParameter = 6,
    RefactorRemoveDuplication = 7,
    
    // Optimization transformations
    OptimizePerformance = 100,
    OptimizeMemory = 101,
    OptimizeSIMD = 102,
    OptimizeCacheLocality = 103,
    OptimizeBranchPrediction = 104,
    
    // Security transformations
    SecurityHardening = 200,
    SecurityBoundsCheck = 201,
    SecurityNullCheck = 202,
    SecurityConstCorrectness = 203,
    
    // Platform transformations
    PlatformPorting = 300,
    PlatformAVX2ToAVX512 = 301,
    PlatformCPUGPU = 302,
    PlatformWin32ToPOSIX = 303,
    
    // Style transformations
    CodeStyleConsistency = 400,
    CodeStyleModernize = 401,
    CodeStyleNaming = 402,
    
    // Generation transformations
    TestGeneration = 500,
    DocumentationGeneration = 501,
    StubGeneration = 502,
    
    // Architecture transformations
    ArchitectureLayering = 600,
    ArchitectureDependencyInjection = 601,
    ArchitectureInterfaceExtraction = 602,
};

std::string TransformationTypeToString(TransformationType type);
TransformationType StringToTransformationType(const std::string& str);

// ============================================================================
// Transformation Constraints — Rules that must be respected
// ============================================================================
struct TransformationConstraints {
    // Key-value constraint pairs
    std::unordered_map<std::string, std::string> constraints;
    
    // Common constraint helpers
    void SetMethodName(const std::string& name) { constraints["method_name"] = name; }
    void SetClassName(const std::string& name) { constraints["class_name"] = name; }
    void SetVariableName(const std::string& name) { constraints["variable_name"] = name; }
    void SetTargetPlatform(const std::string& platform) { constraints["target_platform"] = platform; }
    void SetOptimizationLevel(const std::string& level) { constraints["optimization_level"] = level; }
    void SetPreserveAPI(bool preserve) { constraints["preserve_api"] = preserve ? "true" : "false"; }
    void SetPreserveBehavior(bool preserve) { constraints["preserve_behavior"] = preserve ? "true" : "false"; }
    void SetMaxComplexity(uint32_t complexity) { constraints["max_complexity"] = std::to_string(complexity); }
    void SetLanguageStandard(const std::string& standard) { constraints["language_standard"] = standard; }
    
    // Convert to prompt string for AI
    std::string ToPromptString() const;
    
    // Check if constraint exists
    bool HasConstraint(const std::string& key) const;
    std::string GetConstraint(const std::string& key, const std::string& defaultValue = "") const;
};

// ============================================================================
// Transformation Result — Structured output, no exceptions
// ============================================================================
struct TransformationResult {
    bool        success = false;
    std::string originalCode;
    std::string transformedCode;
    TransformationType type = TransformationType::Unknown;
    std::string errorMessage;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    
    // Metrics
    uint32_t    linesAdded = 0;
    uint32_t    linesRemoved = 0;
    uint32_t    linesModified = 0;
    float       confidence = 0.0f;
    std::chrono::milliseconds duration{0};
    
    // Safety checks
    bool syntaxValid = false;
    bool typeSafe = false;
    bool behaviorPreserved = false;
};

// ============================================================================
// Transformation Suggestion — AI-suggested transformation
// ============================================================================
struct TransformationSuggestion {
    std::string description;
    TransformationType type;
    float confidence;
    std::string preview;
    std::string explanation;
    std::vector<std::string> prerequisites;
};

// ============================================================================
// Transformation Pattern — Learned from examples
// ============================================================================
struct TransformationPattern {
    std::string patternId;
    std::string beforePattern;
    std::string afterPattern;
    TransformationType type;
    float confidence = 0.0f;
    uint32_t usageCount = 0;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point lastUsed;
};

// ============================================================================
// Transformation Context — Editor/IDE context
// ============================================================================
struct TransformationContext {
    std::string filePath;
    uint32_t    startLine = 0;
    uint32_t    endLine = 0;
    uint32_t    startColumn = 0;
    uint32_t    endColumn = 0;
    std::string selectedCode;
    std::string surroundingCode;
    std::string languageId;  // "cpp", "asm", "cmake", etc.
};

} // namespace AI
} // namespace RawrXD

#endif // RAWRXD_TRANSFORMATION_TYPES_HPP
