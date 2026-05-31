// ============================================================================
// ai_architecture_validator_integration.hpp — Integration Bridge
// Connects ArchitectureConsistencyValidator to existing IDE features:
//   - AI Completion Provider (shows architectural warnings inline)
//   - AI Debugger (validates architecture during debugging)
//   - LSP Diagnostics (real-time validation)
//   - Chat Panel (explains architectural issues)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured results only.
// ============================================================================
#pragma once

#ifndef RAWRXD_AI_ARCHITECTURE_VALIDATOR_INTEGRATION_HPP
#define RAWRXD_AI_ARCHITECTURE_VALIDATOR_INTEGRATION_HPP

#include "architecture_consistency_validator.hpp"
#include "../agentic/SovereignInferenceClient.h"
#include "../lsp/diagnostic_provider.h"
#include <memory>
#include <functional>

namespace RawrXD {
namespace AI {

// ============================================================================
// Integration Configuration
// ============================================================================
struct ValidatorIntegrationConfig {
    bool enableInlineHints = true;        // Show hints in editor
    bool enableChatIntegration = true;    // Explain issues in chat
    bool enableDebuggerIntegration = true; // Validate during debug
    bool enableBuildGate = false;         // Block build on severe violations
    float minConfidence = 0.7f;           // Minimum AI confidence
    uint32_t maxIssuesPerFile = 10;       // Limit noise
};

// ============================================================================
// Architecture Validator Integration
// ============================================================================
class ArchitectureValidatorIntegration {
public:
    explicit ArchitectureValidatorIntegration(
        std::shared_ptr<Agent::SovereignInferenceClient> inferenceClient);
    ~ArchitectureValidatorIntegration();

    bool Initialize(const ValidatorIntegrationConfig& config = {});
    void Shutdown();

    // Register with LSP for real-time diagnostics
    void RegisterLSPProvider(RawrXD::LSP::DiagnosticProvider& provider);
    
    // Register with AI Completion for inline hints
    void RegisterCompletionProvider(class AICompletionProviderReal& completionProvider);
    
    // Register with AI Debugger for architecture-aware debugging
    void RegisterDebugger(class AIDebugAgent& debugger);
    
    // Manual validation trigger
    ArchitectureValidationResult ValidateCurrentFile(const std::string& filePath);
    ArchitectureValidationResult ValidateWorkspace();
    
    // Chat integration - explain architectural issues
    std::string ExplainIssue(const ArchitectureValidationResult::Inconsistency& issue);
    
    // Build gate - check if build should proceed
    bool IsBuildAllowed();
    
    // Status
    bool IsInitialized() const { return m_initialized; }
    float GetLastArchitectureScore() const { return m_lastScore; }

private:
    std::unique_ptr<ArchitectureConsistencyValidator> m_validator;
    std::shared_ptr<Agent::SovereignInferenceClient> m_inferenceClient;
    ValidatorIntegrationConfig m_config;
    
    float m_lastScore = 0.0f;
    bool m_initialized = false;
};

} // namespace AI
} // namespace RawrXD

#endif // RAWRXD_AI_ARCHITECTURE_VALIDATOR_INTEGRATION_HPP
