// ============================================================================
// ast_completion_integration.hpp — AST Context Wiring for CompletionEngine
// ============================================================================
// Completes the missing link between ASTGraphEngine and SmartCompletionEngine.
// Provides real-time AST context enrichment for context-aware completions.
//
// This solves the "Ghost text is blind to symbol scope" problem by:
// - Capturing AST context from LSP/Language Server
// - Enriching CompletionContext with scope information
// - Providing symbol-aware completion suggestions
// ============================================================================

#pragma once

#include "core/ast_graph_engine.h"
#include "completion/smart_completion.h"
#include "ide/ast_completion_bridge.h"
#include <memory>
#include <functional>

namespace RawrXD {
namespace Integration {

// ============================================================================
// AST-Enriched Completion Context
// ============================================================================
struct ASTEnrichedContext {
    // Base completion context
    Completion::CompletionContext base;
    
    // AST-derived information
    AST::ContextFingerprint fingerprint;
    IDE::ASTContext astContext;
    
    // Enrichment flags
    bool hasScopeInfo = false;
    bool hasTypeInfo = false;
    bool hasSymbolInfo = false;
    
    // Derived suggestions
    std::vector<Completion::CompletionItem> scopeSuggestions;
    std::vector<Completion::CompletionItem> memberSuggestions;
    std::vector<Completion::CompletionItem> typeSuggestions;
};

// ============================================================================
// AST Completion Integration
// ============================================================================
class ASTCompletionIntegration {
public:
    ASTCompletionIntegration();
    ~ASTCompletionIntegration();

    // No copy/move
    ASTCompletionIntegration(const ASTCompletionIntegration&) = delete;
    ASTCompletionIntegration& operator=(const ASTCompletionIntegration&) = delete;

    // Initialize with AST engine and completion bridge
    bool initialize(AST::ASTGraphEngine* astEngine,
                    IDE::ASTCompletionBridge* astBridge);

    // Shutdown
    void shutdown();

    // Enrich completion request with AST context
    // This is the main entry point - call this before getCompletions()
    ASTEnrichedContext enrichCompletionRequest(
        const Completion::CompletionContext& baseContext);

    // Get completions with full AST context
    Completion::CompletionList getCompletionsWithAST(
        const ASTEnrichedContext& enriched);

    // Real-time AST update (call on every keystroke)
    void updateASTContext(uint32_t fileID, uint32_t line, uint32_t column);

    // Pre-fetch AST context for upcoming completion
    void prefetchASTContext(uint32_t fileID, uint32_t line, uint32_t column);

    // Check if AST context is available
    bool hasASTContext(uint32_t fileID) const;

    // Get AST version for cache invalidation
    AST::GraphVersion getASTVersion(uint32_t fileID) const;

    // Statistics
    struct Stats {
        uint64_t enrichmentsPerformed;
        uint64_t astCacheHits;
        uint64_t astCacheMisses;
        double avgEnrichmentTimeMs;
        uint64_t prefetchCount;
    };
    Stats getStats() const;
    void resetStats();

private:
    // Internal methods
    AST::ContextFingerprint buildFingerprint(uint32_t fileID, uint32_t line,
                                              uint32_t column,
                                              const std::string& partial);
    
    IDE::ASTContext captureASTContext(uint32_t fileID, uint32_t line,
                                       uint32_t column);
    
    std::vector<Completion::CompletionItem> mergeCompletions(
        const std::vector<Completion::CompletionItem>& base,
        const std::vector<Completion::CompletionItem>& ast);

    // Components
    AST::ASTGraphEngine* astEngine_ = nullptr;
    IDE::ASTCompletionBridge* astBridge_ = nullptr;

    // State
    bool initialized_ = false;
    mutable std::mutex mutex_;

    // Statistics
    mutable std::atomic<uint64_t> enrichmentsPerformed_{0};
    mutable std::atomic<uint64_t> astCacheHits_{0};
    mutable std::atomic<uint64_t> astCacheMisses_{0};
    mutable std::atomic<uint64_t> totalEnrichmentTimeUs_{0};
    mutable std::atomic<uint64_t> prefetchCount_{0};
};

// ============================================================================
// Global Access
// ============================================================================
ASTCompletionIntegration* getASTCompletionIntegration();

// ============================================================================
// C API for FFI
// ============================================================================
extern "C" {
    typedef void* RawrXD_ASTCompletionIntegration;

    RawrXD_ASTCompletionIntegration rawrxd_ast_completion_create(
        void* astEngine, void* astBridge);
    
    void rawrxd_ast_completion_destroy(RawrXD_ASTCompletionIntegration handle);
    
    int rawrxd_ast_completion_enrich(
        RawrXD_ASTCompletionIntegration handle,
        const char* uri,
        int line, int column,
        const char* partialSymbol);
    
    int rawrxd_ast_completion_has_context(
        RawrXD_ASTCompletionIntegration handle,
        const char* uri);
}

} // namespace Integration
} // namespace RawrXD
