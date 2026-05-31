// ============================================================================
// ast_completion_integration.cpp — AST Context Wiring Implementation
// ============================================================================
// Implements the bridge between ASTGraphEngine and SmartCompletionEngine.
// ============================================================================

#include "integration/ast_completion_integration.hpp"
#include <chrono>
#include <algorithm>

namespace RawrXD {
namespace Integration {

// Global singleton
static std::unique_ptr<ASTCompletionIntegration> g_astCompletionIntegration;

ASTCompletionIntegration* getASTCompletionIntegration() {
    if (!g_astCompletionIntegration) {
        g_astCompletionIntegration = std::make_unique<ASTCompletionIntegration>();
    }
    return g_astCompletionIntegration.get();
}

// ============================================================================
// Construction / Destruction
// ============================================================================
ASTCompletionIntegration::ASTCompletionIntegration() = default;

ASTCompletionIntegration::~ASTCompletionIntegration() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================
bool ASTCompletionIntegration::initialize(AST::ASTGraphEngine* astEngine,
                                          IDE::ASTCompletionBridge* astBridge) {
    if (initialized_) {
        shutdown();
    }

    if (!astEngine || !astBridge) {
        return false;
    }

    astEngine_ = astEngine;
    astBridge_ = astBridge;

    // Reset statistics
    resetStats();

    initialized_ = true;
    return true;
}

void ASTCompletionIntegration::shutdown() {
    if (!initialized_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    
    astEngine_ = nullptr;
    astBridge_ = nullptr;
    initialized_ = false;
}

// ============================================================================
// Core Enrichment
// ============================================================================
ASTEnrichedContext ASTCompletionIntegration::enrichCompletionRequest(
    const Completion::CompletionContext& baseContext) {
    
    auto t_start = std::chrono::high_resolution_clock::now();
    
    ASTEnrichedContext enriched;
    enriched.base = baseContext;

    if (!initialized_ || !astEngine_ || !astBridge_) {
        enriched.hasScopeInfo = false;
        return enriched;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Extract file ID from URI
    // In production, this would map URI to registered file ID
    uint32_t fileID = 1; // Placeholder

    // Build context fingerprint
    enriched.fingerprint = buildFingerprint(
        fileID,
        baseContext.position.line,
        baseContext.position.column,
        "" // partial symbol would be extracted from context
    );

    // Capture AST context
    enriched.astContext = captureASTContext(
        fileID,
        baseContext.position.line,
        baseContext.position.column
    );

    enriched.hasScopeInfo = enriched.astContext.isValid;
    enriched.hasTypeInfo = !enriched.astContext.visibleSymbols.empty();
    enriched.hasSymbolInfo = enriched.astContext.currentFunction.has_value();

    // Get scope-aware suggestions
    if (enriched.hasScopeInfo && astBridge_) {
        // Enrich the base context
        Completion::CompletionContext tempContext = baseContext;
        astBridge_->enrichCompletionContext(tempContext, enriched.astContext);
        
        // Get scope completions
        enriched.scopeSuggestions = astBridge_->getScopeCompletions(
            enriched.astContext, "");
        
        // Get member completions if in class context
        if (enriched.astContext.scope.inClass && 
            enriched.astContext.currentClass) {
            enriched.memberSuggestions = astBridge_->getMemberCompletions(
                enriched.astContext.currentClass->name, enriched.astContext);
        }
    }

    // Update statistics
    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        t_end - t_start);
    
    enrichmentsPerformed_.fetch_add(1);
    totalEnrichmentTimeUs_.fetch_add(duration.count());

    return enriched;
}

// ============================================================================
// Get Completions with AST
// ============================================================================
Completion::CompletionList ASTCompletionIntegration::getCompletionsWithAST(
    const ASTEnrichedContext& enriched) {
    
    Completion::CompletionList result;
    result.isIncomplete = false;
    result.timestamp = std::chrono::system_clock::now();

    // Start with base completions
    std::vector<Completion::CompletionItem> allItems;

    // Add scope suggestions (highest priority)
    for (const auto& item : enriched.scopeSuggestions) {
        auto scopedItem = item;
        scopedItem.score += 0.3f; // Boost scope items
        scopedItem.isRecommended = true;
        allItems.push_back(scopedItem);
    }

    // Add member suggestions (high priority for member access)
    for (const auto& item : enriched.memberSuggestions) {
        auto memberItem = item;
        memberItem.score += 0.25f; // Boost member items
        memberItem.isRecommended = true;
        allItems.push_back(memberItem);
    }

    // Sort by score
    std::sort(allItems.begin(), allItems.end(),
        [](const Completion::CompletionItem& a, const Completion::CompletionItem& b) {
            return a.score > b.score;
        });

    // Deduplicate
    std::unordered_set<std::string> seen;
    for (const auto& item : allItems) {
        if (seen.insert(item.label).second) {
            result.items.push_back(item);
        }
    }

    return result;
}

// ============================================================================
// AST Context Updates
// ============================================================================
void ASTCompletionIntegration::updateASTContext(uint32_t fileID,
                                                uint32_t line,
                                                uint32_t column) {
    if (!initialized_ || !astEngine_) return;

    // Trigger incremental AST update
    // This would be called on every keystroke
    
    // In production, this would:
    // 1. Get current file content
    // 2. Compute diff from last version
    // 3. Apply incremental update
    // 4. Invalidate affected completion caches
}

void ASTCompletionIntegration::prefetchASTContext(uint32_t fileID,
                                                  uint32_t line,
                                                  uint32_t column) {
    if (!initialized_ || !astEngine_) return;

    prefetchCount_.fetch_add(1);

    // Pre-build context fingerprint and cache it
    auto fingerprint = buildFingerprint(fileID, line, column, "");
    
    // Check if already cached
    auto cached = astEngine_->getCachedCompletions(fingerprint);
    if (cached) {
        astCacheHits_.fetch_add(1);
    } else {
        astCacheMisses_.fetch_add(1);
        // Pre-compute and cache
        auto ctx = captureASTContext(fileID, line, column);
        if (ctx.isValid) {
            // Cache would be populated here
        }
    }
}

// ============================================================================
// Queries
// ============================================================================
bool ASTCompletionIntegration::hasASTContext(uint32_t fileID) const {
    if (!initialized_ || !astEngine_) return false;
    
    // Check if file is registered with AST engine
    auto node = astEngine_->getFileRoot(fileID);
    return node != AST::INVALID_NODE_ID;
}

AST::GraphVersion ASTCompletionIntegration::getASTVersion(uint32_t fileID) const {
    if (!initialized_ || !astEngine_) return 0;
    
    return astEngine_->getCurrentVersion();
}

// ============================================================================
// Internal Methods
// ============================================================================
AST::ContextFingerprint ASTCompletionIntegration::buildFingerprint(
    uint32_t fileID, uint32_t line, uint32_t column, const std::string& partial) {
    
    if (!astEngine_) return {};
    
    return astEngine_->buildFingerprint(fileID, line, column, partial);
}

IDE::ASTContext ASTCompletionIntegration::captureASTContext(uint32_t fileID,
                                                            uint32_t line,
                                                            uint32_t column) {
    
    if (!astBridge_) return {};
    
    // Get file path from AST engine
    // In production, this would map fileID to path
    std::string uri = "file:///workspace/file.cpp";
    std::string language = "cpp";
    
    return astBridge_->captureASTContext(uri, language, static_cast<int>(line),
                                          static_cast<int>(column));
}

std::vector<Completion::CompletionItem> ASTCompletionIntegration::mergeCompletions(
    const std::vector<Completion::CompletionItem>& base,
    const std::vector<Completion::CompletionItem>& ast) {
    
    std::vector<Completion::CompletionItem> merged;
    std::unordered_set<std::string> seen;

    // Add AST completions first (higher priority)
    for (const auto& item : ast) {
        if (seen.insert(item.label).second) {
            merged.push_back(item);
        }
    }

    // Add base completions
    for (const auto& item : base) {
        if (seen.insert(item.label).second) {
            merged.push_back(item);
        }
    }

    return merged;
}

// ============================================================================
// Statistics
// ============================================================================
ASTCompletionIntegration::Stats ASTCompletionIntegration::getStats() const {
    Stats stats;
    stats.enrichmentsPerformed = enrichmentsPerformed_.load();
    stats.astCacheHits = astCacheHits_.load();
    stats.astCacheMisses = astCacheMisses_.load();
    
    uint64_t enrichments = enrichmentsPerformed_.load();
    if (enrichments > 0) {
        stats.avgEnrichmentTimeMs = 
            static_cast<double>(totalEnrichmentTimeUs_.load()) / enrichments / 1000.0;
    } else {
        stats.avgEnrichmentTimeMs = 0.0;
    }
    
    stats.prefetchCount = prefetchCount_.load();
    return stats;
}

void ASTCompletionIntegration::resetStats() {
    enrichmentsPerformed_.store(0);
    astCacheHits_.store(0);
    astCacheMisses_.store(0);
    totalEnrichmentTimeUs_.store(0);
    prefetchCount_.store(0);
}

// ============================================================================
// C API Implementation
// ============================================================================
extern "C" {

RawrXD_ASTCompletionIntegration rawrxd_ast_completion_create(
    void* astEngine, void* astBridge) {
    
    auto* integration = new ASTCompletionIntegration();
    if (!integration->initialize(
            static_cast<AST::ASTGraphEngine*>(astEngine),
            static_cast<IDE::ASTCompletionBridge*>(astBridge))) {
        delete integration;
        return nullptr;
    }
    return integration;
}

void rawrxd_ast_completion_destroy(RawrXD_ASTCompletionIntegration handle) {
    if (handle) {
        auto* integration = static_cast<ASTCompletionIntegration*>(handle);
        integration->shutdown();
        delete integration;
    }
}

int rawrxd_ast_completion_enrich(
    RawrXD_ASTCompletionIntegration handle,
    const char* uri,
    int line, int column,
    const char* partialSymbol) {
    
    if (!handle || !uri) return -1;
    
    auto* integration = static_cast<ASTCompletionIntegration*>(handle);
    
    Completion::CompletionContext ctx;
    ctx.uri = uri;
    ctx.position.line = static_cast<uint32_t>(line);
    ctx.position.column = static_cast<uint32_t>(column);
    
    auto enriched = integration->enrichCompletionRequest(ctx);
    return enriched.hasScopeInfo ? 1 : 0;
}

int rawrxd_ast_completion_has_context(
    RawrXD_ASTCompletionIntegration handle,
    const char* uri) {
    
    if (!handle || !uri) return 0;
    
    auto* integration = static_cast<ASTCompletionIntegration*>(handle);
    // In production, map URI to fileID
    return integration->hasASTContext(1) ? 1 : 0;
}

} // extern "C"

} // namespace Integration
} // namespace RawrXD
