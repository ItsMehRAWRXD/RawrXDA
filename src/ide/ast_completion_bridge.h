/**
 * @file ast_completion_bridge.h
 * @brief AST Context Bridge - Connects LSP AST to CompletionEngine
 * 
 * Solves the "Ghost text is blind to symbol scope" problem by:
 * - Capturing AST context from LSP
 * - Enriching CompletionContext with scope information
 * - Providing symbol-aware completion suggestions
 * 
 * @author RawrXD IDE Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

namespace RawrXD {

namespace Completion {
    struct CompletionContext;
    struct CompletionItem;
}

namespace IDE {

// Forward declarations
class LanguageServerIntegration;

// ============================================================================
// AST Context Types
// ============================================================================

struct SymbolInfo {
    std::string name;
    std::string type;
    std::string kind;  // "class", "function", "variable", "namespace", etc.
    std::string scope; // Fully qualified scope: "MyNamespace::MyClass"
    int32_t line = 0;      // Fixed-width for FFI stability
    int32_t column = 0;     // Fixed-width for FFI stability
    bool isPublic = true;
    bool isStatic = false;
    bool isConst = false;
    uint8_t _padding[5] = {0}; // Explicit padding for ABI stability across compilers
    std::vector<std::string> parameters; // For functions
};

struct ScopeContext {
    std::string currentScope;           // e.g., "MyNamespace::MyClass::private"
    std::vector<std::string> scopeStack; // Namespace/class hierarchy
    bool inClass = false;
    bool inFunction = false;
    bool inPrivateBlock = false;
    bool inProtectedBlock = false;
    bool inPublicBlock = true; // Default
    uint8_t _padding[3] = {0}; // Explicit padding for ABI stability across compilers
};

struct ASTContext {
    std::string uri;
    std::string language;
    ScopeContext scope;
    std::vector<SymbolInfo> visibleSymbols;
    std::vector<SymbolInfo> localSymbols;
    std::vector<SymbolInfo> memberSymbols;
    std::optional<SymbolInfo> currentClass;
    std::optional<SymbolInfo> currentFunction;
    bool isValid = false;
    uint8_t _padding[7] = {0}; // Explicit padding for ABI stability across compilers
};

// ============================================================================
// AST Completion Bridge
// ============================================================================

class ASTCompletionBridge {
public:
    ASTCompletionBridge();
    ~ASTCompletionBridge();

    // Initialize with LSP integration
    void initialize(std::shared_ptr<LanguageServerIntegration> lsp);

    // Capture AST context from LSP for a given position
    ASTContext captureASTContext(const std::string& uri, 
                               const std::string& language,
                               int line, int column);

    // Enrich completion context with AST information
    void enrichCompletionContext(Completion::CompletionContext& ctx,
                                const ASTContext& ast);

    // Filter completion items based on AST context
    std::vector<Completion::CompletionItem> filterByScope(
        const std::vector<Completion::CompletionItem>& items,
        const ASTContext& ast);

    // Get scope-aware completions
    std::vector<Completion::CompletionItem> getScopeCompletions(
        const ASTContext& ast,
        const std::string& prefix);

    // Check if symbol is accessible from current scope
    bool isSymbolAccessible(const SymbolInfo& symbol, const ScopeContext& scope);

    // Get member completions for a type
    std::vector<Completion::CompletionItem> getMemberCompletions(
        const std::string& typeName,
        const ASTContext& ast);

private:
    std::shared_ptr<LanguageServerIntegration> m_lsp;
    
    // Parse LSP documentSymbol response into ASTContext
    ASTContext parseDocumentSymbols(const nlohmann::json& symbols,
                                   const std::string& uri,
                                   const std::string& language);
    
    // Build scope hierarchy
    void buildScopeHierarchy(ASTContext& ast);
    
    // Get visible symbols from scope
    std::vector<SymbolInfo> getVisibleSymbols(const ScopeContext& scope);
    
    // Cache for AST contexts
    std::unordered_map<std::string, ASTContext> m_contextCache;
    std::mutex m_cacheMutex;
};

// ============================================================================
// C API for FFI
// ============================================================================

extern "C" {
    __declspec(dllexport) void* ASTBridge_Create();
    __declspec(dllexport) void ASTBridge_Destroy(void* bridge);
    __declspec(dllexport) void ASTBridge_Initialize(void* bridge, void* lsp);
    __declspec(dllexport) const char* ASTBridge_CaptureContext(void* bridge,
                                                              const char* uri,
                                                              const char* language,
                                                              int line, int column);
    __declspec(dllexport) void ASTBridge_FreeString(const char* str);
}

} // namespace IDE
} // namespace RawrXD
