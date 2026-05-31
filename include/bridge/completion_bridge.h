#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace RawrXD {
namespace Bridge {

// Forward declarations
struct ASTNode;
struct ScopeContext;

enum class AccessModifier {
    Public,
    Protected,
    Private
};

// Completion item enriched with AST context
struct EnrichedCompletionItem {
    std::string label;
    std::string detail;
    std::string documentation;
    int kind = 0;  // LSP CompletionItemKind
    
    // AST context
    bool isAccessible = true;
    bool isTypeCorrect = true;
    int scopeDepth = 0;
    std::string scopePath;
    
    // Fingerprint for caching
    uint64_t contextFingerprint = 0;
};

// AST-enriched completion context
struct ASTEnrichedContext {
    std::vector<EnrichedCompletionItem> items;
    uint64_t fingerprint = 0;
    int cursorLine = 0;
    int cursorColumn = 0;
    std::string filePath;
    
    // Scope information
    std::string currentScope;
    std::vector<std::string> scopeStack;
    bool inClassScope = false;
    bool inFunctionScope = false;
    AccessModifier currentAccess = AccessModifier::Public;
};

// C-API for FFI
extern "C" {
    typedef struct RawrXD_ASTContext RawrXD_ASTContext;
    typedef struct RawrXD_CompletionResult RawrXD_CompletionResult;
    
    RawrXD_ASTContext* RawrXD_ast_context_create(const char* filePath);
    void RawrXD_ast_context_destroy(RawrXD_ASTContext* ctx);
    
    RawrXD_CompletionResult* RawrXD_ast_completion_enrich(
        RawrXD_ASTContext* ctx,
        const char* prefix,
        int line,
        int column
    );
    
    void RawrXD_completion_result_destroy(RawrXD_CompletionResult* result);
    int RawrXD_completion_result_count(RawrXD_CompletionResult* result);
    const char* RawrXD_completion_result_item_label(RawrXD_CompletionResult* result, int index);
    int RawrXD_completion_result_item_is_accessible(RawrXD_CompletionResult* result, int index);
}

// Main completion bridge class
class CompletionBridge {
public:
    CompletionBridge();
    ~CompletionBridge();
    
    // Initialize with file content
    bool Initialize(const std::string& filePath, const std::string& content);
    
    // Get enriched completions at cursor position
    ASTEnrichedContext GetCompletions(const std::string& prefix, int line, int column);
    
    // Update content incrementally
    void UpdateContent(const std::string& newContent);
    
    // Check if context is stale
    bool IsContextStale(uint64_t fingerprint) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Scope-aware symbol filter
class ScopeAwareFilter {
public:
    static std::vector<EnrichedCompletionItem> FilterByScope(
        const std::vector<EnrichedCompletionItem>& items,
        const std::string& currentScope,
        AccessModifier currentAccess,
        bool inClassScope
    );
    
    static bool IsAccessible(
        const EnrichedCompletionItem& item,
        const std::string& currentScope,
        AccessModifier currentAccess
    );
};

// Context fingerprint generator
class ContextFingerprint {
public:
    static uint64_t Generate(const std::string& scope, const std::string& prefix, int line);
    static uint64_t Combine(uint64_t a, uint64_t b);
};

} // namespace Bridge
} // namespace RawrXD
