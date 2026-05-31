// ============================================================================
// code_transformer_integration.cpp — LSP/IDE Integration Implementation
// ============================================================================
#include "code_transformer_integration.hpp"
#include <fstream>
#include <sstream>

namespace RawrXD {
namespace AI {

CodeTransformerIntegration::CodeTransformerIntegration(
    std::shared_ptr<CodeTransformer> transformer)
    : m_transformer(transformer) {}

CodeTransformerIntegration::~CodeTransformerIntegration() = default;

bool CodeTransformerIntegration::Initialize() {
    if (!m_transformer) return false;
    m_initialized = true;
    return true;
}

void CodeTransformerIntegration::Shutdown() {
    m_initialized = false;
}

void CodeTransformerIntegration::RegisterWithLSP(RawrXD::LSP::DiagnosticProvider& provider) {
    // Register as code action provider
    auto source = [this](const std::string& uri, const std::string& content)
        -> std::vector<RawrXD::LSP::Diagnostic> {
        // Code transformer doesn't produce diagnostics directly
        // It provides code actions based on existing diagnostics
        return {};
    };
    
    provider.registerDiagnosticSource("code_transformer", source);
}

std::vector<LSPCodeAction> CodeTransformerIntegration::ProvideTransformations(
    const std::string& filePath,
    uint32_t startLine, uint32_t startCol,
    uint32_t endLine, uint32_t endCol) {
    
    std::vector<LSPCodeAction> actions;
    
    if (!m_transformer || !m_initialized) return actions;
    
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) return actions;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Extract selected code
    std::istringstream stream(content);
    std::string line;
    std::string selectedCode;
    uint32_t currentLine = 1;
    
    while (std::getline(stream, line)) {
        if (currentLine >= startLine && currentLine <= endLine) {
            if (currentLine == startLine && currentLine == endLine) {
                // Single line selection
                if (startCol < line.size() && endCol <= line.size()) {
                    selectedCode += line.substr(startCol, endCol - startCol);
                }
            } else if (currentLine == startLine) {
                // Start of multi-line selection
                if (startCol < line.size()) {
                    selectedCode += line.substr(startCol) + "\n";
                }
            } else if (currentLine == endLine) {
                // End of multi-line selection
                if (endCol <= line.size()) {
                    selectedCode += line.substr(0, endCol);
                }
            } else {
                // Middle lines
                selectedCode += line + "\n";
            }
        }
        currentLine++;
    }
    
    if (selectedCode.empty()) return actions;
    
    // Get suggestions
    TransformationContext context;
    context.filePath = filePath;
    context.startLine = startLine;
    context.endLine = endLine;
    context.startColumn = startCol;
    context.endColumn = endCol;
    context.selectedCode = selectedCode;
    
    auto suggestions = m_transformer->SuggestTransformationsWithContext(selectedCode, context);
    
    for (const auto& suggestion : suggestions) {
        LSPCodeAction action;
        action.title = suggestion.description;
        action.kind = "refactor";
        action.command = TransformationTypeToString(suggestion.type);
        action.confidence = suggestion.confidence;
        
        // Create edit description
        std::stringstream edit;
        edit << "{\"range\":{\"start\":{\"line\":" << startLine << ",\"character\":" << startCol 
             << "},\"end\":{\"line\":" << endLine << ",\"character\":" << endCol 
             << "}},\"newText\":\"" << suggestion.preview << "\"}";
        action.edit = edit.str();
        
        actions.push_back(action);
    }
    
    return actions;
}

std::vector<LSPCodeAction> CodeTransformerIntegration::ProvideQuickFixes(
    const std::string& filePath,
    uint32_t line, uint32_t col,
    const std::string& diagnosticMessage) {
    
    std::vector<LSPCodeAction> actions;
    
    if (!m_transformer || !m_initialized) return actions;
    
    // Analyze diagnostic message and suggest fixes
    if (diagnosticMessage.find("circular include") != std::string::npos) {
        LSPCodeAction action;
        action.title = "Break circular dependency";
        action.kind = "quickfix";
        action.command = "REFACTOR_REMOVE_DUPLICATION";
        action.confidence = 0.9f;
        actions.push_back(action);
    }
    
    if (diagnosticMessage.find("null pointer") != std::string::npos ||
        diagnosticMessage.find("null check") != std::string::npos) {
        LSPCodeAction action;
        action.title = "Add null check";
        action.kind = "quickfix";
        action.command = "SECURITY_NULL_CHECK";
        action.confidence = 0.95f;
        actions.push_back(action);
    }
    
    if (diagnosticMessage.find("bounds") != std::string::npos ||
        diagnosticMessage.find("array index") != std::string::npos) {
        LSPCodeAction action;
        action.title = "Add bounds check";
        action.kind = "quickfix";
        action.command = "SECURITY_BOUNDS_CHECK";
        action.confidence = 0.95f;
        actions.push_back(action);
    }
    
    return actions;
}

TransformationResult CodeTransformerIntegration::ExecuteTransformation(
    const std::string& filePath,
    TransformationType type,
    const TransformationConstraints& constraints) {
    
    if (!m_transformer || !m_initialized) {
        TransformationResult result;
        result.errorMessage = "Transformer not initialized";
        return result;
    }
    
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) {
        TransformationResult result;
        result.errorMessage = "Cannot read file: " + filePath;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Apply transformation
    TransformationContext context;
    context.filePath = filePath;
    context.languageId = "cpp";  // Detect from extension in production
    
    auto result = m_transformer->TransformCodeWithContext(content, type, constraints, context);
    
    if (result.success) {
        // Write transformed code back
        std::ofstream outFile(filePath);
        if (outFile.is_open()) {
            outFile << result.transformedCode;
            outFile.close();
        }
    }
    
    return result;
}

TransformationResult CodeTransformerIntegration::PreviewTransformation(
    const std::string& filePath,
    TransformationType type,
    const TransformationConstraints& constraints) {
    
    if (!m_transformer || !m_initialized) {
        TransformationResult result;
        result.errorMessage = "Transformer not initialized";
        return result;
    }
    
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) {
        TransformationResult result;
        result.errorMessage = "Cannot read file: " + filePath;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Preview transformation (don't write back)
    TransformationContext context;
    context.filePath = filePath;
    context.languageId = "cpp";
    
    return m_transformer->TransformCodeWithContext(content, type, constraints, context);
}

} // namespace AI
} // namespace RawrXD
