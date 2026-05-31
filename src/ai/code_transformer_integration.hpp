// ============================================================================
// code_transformer_integration.hpp — LSP/IDE Integration for Code Transformer
// Provides code actions, quick fixes, and refactoring support.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured results only.
// ============================================================================
#pragma once

#ifndef RAWRXD_CODE_TRANSFORMER_INTEGRATION_HPP
#define RAWRXD_CODE_TRANSFORMER_INTEGRATION_HPP

#include "code_transformer.hpp"
#include "../lsp/diagnostic_provider.h"
#include <memory>
#include <functional>

namespace RawrXD {
namespace AI {

// ============================================================================
// LSP Code Action — Represents a transformation action
// ============================================================================
struct LSPCodeAction {
    std::string title;
    std::string kind;  // "refactor", "quickfix", "source"
    std::string command;
    std::string edit;  // JSON edit description
    float confidence;
};

// ============================================================================
// Code Transformer Integration
// ============================================================================
class CodeTransformerIntegration {
public:
    explicit CodeTransformerIntegration(
        std::shared_ptr<CodeTransformer> transformer);
    ~CodeTransformerIntegration();

    bool Initialize();
    void Shutdown();

    // Register with LSP for code actions
    void RegisterWithLSP(RawrXD::LSP::DiagnosticProvider& provider);
    
    // Provide transformations for selected code
    std::vector<LSPCodeAction> ProvideTransformations(const std::string& filePath,
                                                      uint32_t startLine, uint32_t startCol,
                                                      uint32_t endLine, uint32_t endCol);
    
    // Provide quick fixes for diagnostics
    std::vector<LSPCodeAction> ProvideQuickFixes(const std::string& filePath,
                                                 uint32_t line, uint32_t col,
                                                 const std::string& diagnosticMessage);
    
    // Execute transformation
    TransformationResult ExecuteTransformation(const std::string& filePath,
                                          TransformationType type,
                                          const TransformationConstraints& constraints);
    
    // Preview transformation (without applying)
    TransformationResult PreviewTransformation(const std::string& filePath,
                                            TransformationType type,
                                            const TransformationConstraints& constraints);
    
    // Status
    bool IsInitialized() const { return m_initialized; }

private:
    std::shared_ptr<CodeTransformer> m_transformer;
    bool m_initialized = false;
};

} // namespace AI
} // namespace RawrXD

#endif // RAWRXD_CODE_TRANSFORMER_INTEGRATION_HPP
