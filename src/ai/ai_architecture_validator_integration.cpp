// ============================================================================
// ai_architecture_validator_integration.cpp — Integration Bridge Implementation
// ============================================================================
#include "ai_architecture_validator_integration.hpp"
#include <chrono>

namespace RawrXD {
namespace AI {

ArchitectureValidatorIntegration::ArchitectureValidatorIntegration(
    std::shared_ptr<Agent::SovereignInferenceClient> inferenceClient)
    : m_inferenceClient(inferenceClient) {}

ArchitectureValidatorIntegration::~ArchitectureValidatorIntegration() = default;

bool ArchitectureValidatorIntegration::Initialize(const ValidatorIntegrationConfig& config) {
    m_config = config;
    
    m_validator = std::make_unique<ArchitectureConsistencyValidator>(m_inferenceClient);
    
    if (!m_validator->Initialize()) {
        return false;
    }
    
    if (!m_validator->BuildSemanticGraph("src/")) {
        // Non-fatal: can still validate individual files
    }
    
    m_validator->SetMinConfidence(config.minConfidence);
    m_initialized = true;
    return true;
}

void ArchitectureValidatorIntegration::Shutdown() {
    m_validator.reset();
    m_initialized = false;
}

void ArchitectureValidatorIntegration::RegisterLSPProvider(RawrXD::LSP::DiagnosticProvider& provider) {
    if (!m_validator) return;
    m_validator->RegisterWithLSP(provider);
}

ArchitectureValidationResult ArchitectureValidatorIntegration::ValidateCurrentFile(
    const std::string& filePath) {
    if (!m_validator) {
        ArchitectureValidationResult result;
        result.error_message = "Validator not initialized";
        return result;
    }
    
    // Rebuild graph for just this file
    // (In production, would use incremental updates)
    m_validator->BuildSemanticGraph(filePath);
    
    return m_validator->ValidateSnapshot();
}

ArchitectureValidationResult ArchitectureValidatorIntegration::ValidateWorkspace() {
    if (!m_validator) {
        ArchitectureValidationResult result;
        result.error_message = "Validator not initialized";
        return result;
    }
    
    m_validator->BuildSemanticGraph("src/");
    auto result = m_validator->ValidateSnapshot();
    m_lastScore = result.architectureScore;
    
    return result;
}

std::string ArchitectureValidatorIntegration::ExplainIssue(
    const ArchitectureValidationResult::Inconsistency& issue) {
    if (!m_inferenceClient || !m_inferenceClient->IsLoaded()) {
        return "Architecture issue [" + issue.principleId + "]: " + issue.description +
               "\nSuggestion: " + issue.suggestedFix;
    }
    
    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"system", 
        "You are an architecture expert. Explain the following issue and suggest fixes."});
    
    std::string prompt = "Explain this architectural inconsistency:\n\n";
    prompt += "Principle: " + issue.principleId + "\n";
    prompt += "Description: " + issue.description + "\n";
    prompt += "File: " + issue.filePath + "\n";
    prompt += "Suggested fix: " + issue.suggestedFix + "\n\n";
    prompt += "Provide a clear explanation and actionable remediation steps.";
    
    messages.push_back({"user", prompt});
    
    auto result = m_inferenceClient->ChatSync(messages);
    if (result.success) {
        return result.response;
    }
    
    return "Issue: " + issue.description + "\nSuggestion: " + issue.suggestedFix;
}

bool ArchitectureValidatorIntegration::IsBuildAllowed() {
    if (!m_config.enableBuildGate) return true;
    
    auto result = ValidateWorkspace();
    
    // Block build if architecture score is too low or critical errors exist
    if (result.architectureScore < 0.5f) return false;
    
    for (const auto& inconsistency : result.inconsistencies) {
        if (inconsistency.severity == 1 && inconsistency.confidence > 0.9f) {
            return false;  // Critical violation with high confidence
        }
    }
    
    return true;
}

} // namespace AI
} // namespace RawrXD
