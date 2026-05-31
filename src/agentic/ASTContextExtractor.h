// ============================================================================
// AST Context Extractor v1 - Symbol-Aware Code Chunking
// ============================================================================
// Provides intelligent context window management for AI code assistance.
// Extracts focused symbol maps instead of dumping entire files into prompts.
//
// Design goals:
//   - Unbounded capacity (64-bit sizes, no hard limits)
//   - Model-aware token budgeting (scales with model context window)
//   - AST-level symbol extraction (functions, classes, structs)
//   - Focus window: cursor line ± N lines + relevant symbol definitions
//   - Safe/unsafe execution mode gating
//
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <regex>

namespace RawrXD::Agentic {

// ============================================================================
// Symbol Types
// ============================================================================
enum class SymbolType {
    Function,
    Class,
    Struct,
    Enum,
    Namespace,
    Variable,
    Typedef,
    Macro,
    Unknown
};

// ============================================================================
// Symbol Record
// ============================================================================
struct Symbol {
    std::string name;
    SymbolType type;
    uint64_t lineStart;
    uint64_t lineEnd;
    std::string signature;      // e.g., "void foo(int x)"
    std::string body;           // Full body text
    std::string docComment;     // /** ... */ or /// ...
    std::vector<std::string> dependencies;  // Other symbols referenced
    uint64_t tokenCount;
};

// ============================================================================
// Focus Window
// ============================================================================
struct FocusWindow {
    uint64_t cursorLine;
    uint64_t contextLinesBefore;
    uint64_t contextLinesAfter;
    std::string surroundingCode;
    std::vector<Symbol> relevantSymbols;
    std::string errorContext;   // Compiler error at this line, if any
};

// ============================================================================
// Model Capability Profile
// ============================================================================
struct ModelCapabilityProfile {
    std::string name;
    uint64_t parameterCount;        // e.g., 22_000_000_000 for 22B
    uint64_t contextWindowTokens;   // e.g., 32_768 for Codestral
    uint64_t maxOutputTokens;
    bool supportsCodeCompletion;
    bool supportsFunctionCalling;
    bool supportsToolUse;
    float recommendedTemperature;
    float recommendedTopP;
};

// ============================================================================
// Execution Mode
// ============================================================================
enum class ExecutionMode {
    Safe,       // Shadow mode: propose only, no auto-apply
    Normal,     // Standard: apply after user confirmation
    Unsafe,     // Direct apply: no confirmation, auto-execute
    Kernel      // System-level: requires explicit compile-time gate
};

// ============================================================================
// AST Context Extractor
// ============================================================================
class ASTContextExtractor {
public:
    ASTContextExtractor();
    ~ASTContextExtractor();

    // Configuration
    void setModelProfile(const ModelCapabilityProfile& profile);
    void setExecutionMode(ExecutionMode mode);
    void setFocusWindowSize(uint64_t linesBefore, uint64_t linesAfter);
    void setMaxPromptTokens(uint64_t maxTokens);

    // Symbol extraction
    std::vector<Symbol> extractSymbols(const std::string& code, const std::string& language);
    Symbol extractSymbolAtLine(const std::string& code, const std::string& language, uint64_t line);

    // Focus window construction
    FocusWindow buildFocusWindow(const std::string& code, const std::string& language,
                                  uint64_t cursorLine, const std::string& errorContext = "");

    // Prompt assembly
    std::string assemblePrompt(const std::string& task, const FocusWindow& focus,
                                const std::vector<Symbol>& globalSymbols,
                                const std::string& additionalContext = "");

    // Context compaction for large files
    std::string compactContext(const std::string& context, uint64_t targetTokens);

    // Execution mode checks
    bool canAutoApply() const;
    bool requiresConfirmation() const;
    bool isKernelMode() const;

    // Model-aware sizing
    uint64_t getOptimalContextLines() const;
    uint64_t getMaxPromptTokens() const;
    uint64_t estimateTokens(const std::string& text) const;

    // Unbounded model support (up to 1.8T+ parameters)
    bool canHandleModelSize(uint64_t parameterCount) const;
    uint64_t getRecommendedBatchSize(uint64_t parameterCount) const;
    uint64_t getRecommendedContextWindow(uint64_t parameterCount) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Global instance accessor
// ============================================================================
ASTContextExtractor& GetASTContextExtractor();

// ============================================================================
// Utility: Quick symbol extraction for C/C++
// ============================================================================
std::vector<Symbol> QuickExtractCppSymbols(const std::string& code);

// ============================================================================
// Utility: Token estimation (model-aware)
// ============================================================================
uint64_t EstimateTokenCount(const std::string& text, uint64_t modelParameterCount = 0);

} // namespace RawrXD::Agentic
