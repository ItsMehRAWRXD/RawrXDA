// ============================================================================
// architecture_consistency_validator.hpp — Phase 1: Architecture Consistency Guardian
// Validates codebase adherence to architectural principles defined in docs/ARCHITECTURE.md
// Uses SovereignInferenceClient for semantic analysis of architectural drift.
//
// Design: Static snapshot validation + temporal drift detection
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured ValidationResult returns only.
// Rule: All threading via std::mutex + std::lock_guard.
// ============================================================================
#pragma once

#ifndef RAWRXD_ARCHITECTURE_CONSISTENCY_VALIDATOR_HPP
#define RAWRXD_ARCHITECTURE_CONSISTENCY_VALIDATOR_HPP

#include "../agentic/SovereignInferenceClient.h"
#include "../lsp/diagnostic_provider.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace RawrXD {
namespace AI {

// ============================================================================
// Architectural Principle — Extracted from docs/ARCHITECTURE.md
// ============================================================================
struct ArchitecturalPrinciple {
    std::string id;           // e.g., "HOTPATCH-01", "MASM-02"
    std::string category;      // e.g., "Hotpatch", "MASM64", "Build", "Security"
    std::string description;   // Human-readable principle
    std::string rule;          // Enforceable rule pattern
    uint32_t    severity;     // 1=Error, 2=Warning, 3=Information
};

// ============================================================================
// Code Entity — Represents a structural element in the codebase
// ============================================================================
struct CodeEntity {
    enum class Type {
        Unknown = 0,
        Namespace,
        Class,
        Struct,
        Function,
        MASMKernel,
        Include,
        CallSite,
        GlobalVariable
    };

    Type        type = Type::Unknown;
    std::string name;
    std::string filePath;
    uint32_t    lineNumber = 0;
    std::string parentScope;  // Enclosing namespace/class
    std::vector<std::string> dependencies;  // Includes, calls, inherits
};

// ============================================================================
// Semantic Graph — Architecture representation of the codebase
// ============================================================================
class SemanticGraph {
public:
    void AddEntity(const CodeEntity& entity);
    void AddEdge(const std::string& from, const std::string& to, const std::string& relation);
    
    std::vector<CodeEntity> FindByType(CodeEntity::Type type) const;
    std::vector<CodeEntity> FindByScope(const std::string& scope) const;
    std::vector<std::string> GetDependencies(const std::string& entityName) const;
    
    bool HasEntity(const std::string& name) const;
    size_t EntityCount() const { return m_entities.size(); }
    
    void Clear();

private:
    std::unordered_map<std::string, CodeEntity> m_entities;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m_edges;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Validation Result — Structured result, no exceptions
// ============================================================================
struct ArchitectureValidationResult {
    bool        success = false;
    std::string error_message;
    
    struct Inconsistency {
        std::string principleId;
        std::string description;
        std::string filePath;
        uint32_t    lineNumber = 0;
        uint32_t    severity = 2;  // 1=Error, 2=Warning, 3=Info
        std::string suggestedFix;
        float       confidence = 0.0f;  // AI confidence score
    };
    
    std::vector<Inconsistency> inconsistencies;
    float architectureScore = 0.0f;  // 0.0 - 1.0
    std::chrono::milliseconds duration{0};
};

// ============================================================================
// Architecture Consistency Validator
// ============================================================================
class ArchitectureConsistencyValidator {
public:
    explicit ArchitectureConsistencyValidator(
        std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> inferenceClient = nullptr);
    ~ArchitectureConsistencyValidator();

    // Initialize with architecture principles from docs/ARCHITECTURE.md
    bool Initialize(const std::string& architectureDocPath = "docs/ARCHITECTURE.md");
    
    // Build semantic graph from source tree
    bool BuildSemanticGraph(const std::string& sourceRoot = "src/");
    
    // Validate current snapshot against principles
    ArchitectureValidationResult ValidateSnapshot();
    
    // Detect drift from baseline graph
    ArchitectureValidationResult DetectDrift(const SemanticGraph& baseline);
    
    // Register as LSP diagnostic source
    void RegisterWithLSP(RawrXD::LSP::DiagnosticProvider& provider);
    
    // Async validation with callback
    using ValidationCallback = std::function<void(const ArchitectureValidationResult&)>;
    void ValidateAsync(ValidationCallback callback);
    
    // Get/set configuration
    void SetMinConfidence(float confidence) { m_minConfidence = confidence; }
    float GetMinConfidence() const { return m_minConfidence; }
    
    bool IsInitialized() const { return m_initialized; }

private:
    // Principle loading
    bool LoadPrinciplesFromMarkdown(const std::string& path);
    
    // Graph building
    void ParseSourceFile(const std::string& filePath);
    void ParseCppFile(const std::string& filePath, const std::string& content);
    void ParseMASMFile(const std::string& filePath, const std::string& content);
    void ParseCMakeFile(const std::string& filePath, const std::string& content);
    
    // AI-powered validation
    ArchitectureValidationResult RunAIValidation();
    std::string BuildValidationPrompt(const std::vector<CodeEntity>& entities);
    std::vector<ArchitectureValidationResult::Inconsistency> ParseAIResponse(
        const std::string& response);
    
    // Rule-based validation (fast path)
    ArchitectureValidationResult RunRuleBasedValidation();
    
    // LSP integration
    std::vector<RawrXD::LSP::Diagnostic> ConvertToLSPDiagnostics(
        const ArchitectureValidationResult& result);

    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> m_inferenceClient;
    std::vector<ArchitecturalPrinciple> m_principles;
    SemanticGraph m_currentGraph;
    SemanticGraph m_baselineGraph;
    
    float m_minConfidence = 0.7f;
    bool m_initialized = false;
    bool m_useAI = true;
    
    mutable std::mutex m_mutex;
};

} // namespace AI
} // namespace RawrXD

#endif // RAWRXD_ARCHITECTURE_CONSISTENCY_VALIDATOR_HPP
