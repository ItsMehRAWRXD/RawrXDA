// ============================================================================
// completion_ranking_kernel.hpp — Phase 1b: Intent-Aware Completion Ranking
// ============================================================================
// Turns symbol candidates into ranked predictions using multi-signal scoring.
//
// Design:
//   - Multi-signal scoring (lexical, AST, type, frequency, recency, trigger)
//   - Context window fusion (AST node, enclosing scope, token history)
//   - Deterministic tie-breaking (stable sort, symbol ID hash)
//   - Zero runtime randomness
//
// Pipeline:
//   SymbolIndexBridge → CompletionRankingKernel → GhostTextRenderer
// ============================================================================

#pragma once

#include "bridge/symbol_index_bridge.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <string>
#include <array>
#include <functional>

namespace rawrxd {
namespace completion {

// ============================================================================
// Scoring Weights (tunable)
// ============================================================================

struct ScoringWeights {
    float lexical_proximity      = 1.0f;   // prefix / substring match
    float ast_distance           = 0.8f;   // same scope, parent, sibling
    float type_affinity          = 0.6f;   // function vs variable vs macro
    float usage_frequency        = 0.5f;   // hot symbols in file/project
    float recency_bias           = 0.4f;   // recently edited symbols
    float trigger_strength       = 0.3f;   // ., ::, -> weighting
    float documentation_bonus    = 0.2f;   // symbols with docs
};

// ============================================================================
// Context Signals
// ============================================================================

struct ASTContext {
    std::string enclosing_function;   // function name or empty
    std::string enclosing_class;      // class/struct name or empty
    std::string enclosing_module;     // module name or empty
    size_t scope_depth{0};            // nesting level
    std::vector<std::string> sibling_symbols; // symbols in same scope
};

struct TokenHistory {
    std::array<std::string, 8> last_tokens; // last 8 tokens before cursor
    size_t token_count{0};                    // how many valid tokens
    std::string last_type_annotation;         // e.g., "i32", "String"
};

struct CompletionContext {
    std::string file_path;
    size_t line{0};
    size_t column{0};
    std::string prefix;
    TriggerKind trigger{TriggerKind::None};
    ASTContext ast;
    TokenHistory tokens;
};

// ============================================================================
// Ranked Completion Output
// ============================================================================

struct RankedCompletion {
    SymbolCandidate symbol;
    float score{0.0f};
    float lexical_score{0.0f};
    float ast_score{0.0f};
    float type_score{0.0f};
    float frequency_score{0.0f};
    float recency_score{0.0f};
    float trigger_score{0.0f};
    uint32_t context_flags{0};
    uint64_t stable_key{0}; // deterministic tie-breaker
};

// Context flags (bitmask)
enum ContextFlags : uint32_t {
    CF_SAME_SCOPE      = 1 << 0,
    CF_PARENT_SCOPE    = 1 << 1,
    CF_SIBLING_SYMBOL  = 1 << 2,
    CF_HOT_SYMBOL      = 1 << 3,
    CF_RECENT_EDIT     = 1 << 4,
    CF_TYPE_MATCH      = 1 << 5,
    CF_DOC_AVAILABLE   = 1 << 6,
    CF_EXACT_MATCH     = 1 << 7,
};

// ============================================================================
// Completion Ranking Kernel
// ============================================================================

class CompletionRankingKernel {
public:
    CompletionRankingKernel();
    ~CompletionRankingKernel();

    // Lifecycle
    bool initialize(const ScoringWeights& weights = ScoringWeights{});
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // Main entry: rank candidates into predictions
    std::vector<RankedCompletion> rank(
        const std::vector<SymbolCandidate>& candidates,
        const CompletionContext& context);

    // Update usage statistics (call when user accepts a completion)
    void recordAcceptance(const std::string& symbol_name,
                          const std::string& file_path);

    // Update recency (call when symbol is edited)
    void recordEdit(const std::string& symbol_name,
                    const std::string& file_path);

    // Get/set weights
    ScoringWeights getWeights() const;
    void setWeights(const ScoringWeights& weights);

    // Statistics
    size_t getTotalRankings() const { return total_rankings_; }
    size_t getTotalAcceptances() const { return total_acceptances_; }

private:
    // Scoring functions
    float scoreLexical(const SymbolCandidate& cand, const CompletionContext& ctx);
    float scoreAST(const SymbolCandidate& cand, const CompletionContext& ctx);
    float scoreTypeAffinity(const SymbolCandidate& cand, const CompletionContext& ctx);
    float scoreFrequency(const SymbolCandidate& cand, const CompletionContext& ctx);
    float scoreRecency(const SymbolCandidate& cand, const CompletionContext& ctx);
    float scoreTrigger(const SymbolCandidate& cand, const CompletionContext& ctx);

    // Helpers
    uint32_t computeContextFlags(const SymbolCandidate& cand,
                                  const CompletionContext& ctx);
    uint64_t computeStableKey(const SymbolCandidate& cand);
    float normalizeScore(float raw_score);

    // Usage tracking
    struct UsageStats {
        uint32_t accept_count{0};
        uint32_t show_count{0};
        uint64_t last_accepted_ms{0};
        uint64_t last_edited_ms{0};
    };

    std::unordered_map<std::string, UsageStats> usage_stats_;
    std::unordered_map<std::string, std::unordered_map<std::string, UsageStats>> file_stats_;

    // State
    bool initialized_{false};
    ScoringWeights weights_;
    uint64_t total_rankings_{0};
    uint64_t total_acceptances_{0};
    uint64_t start_time_ms_{0};

    // Thread safety
    mutable std::shared_mutex mutex_;
};

} // namespace completion
} // namespace rawrxd

// ============================================================================
// C API
// ============================================================================

extern "C" {

typedef struct RawrXD_CompletionRankingKernel RawrXD_CompletionRankingKernel;

RawrXD_CompletionRankingKernel* rawrxd_ranking_create();
void rawrxd_ranking_destroy(RawrXD_CompletionRankingKernel* handle);

int rawrxd_ranking_initialize(RawrXD_CompletionRankingKernel* handle,
                                 const float* weights, size_t weight_count);

// Rank candidates
// Input: array of SymbolCandidate (from symbol_index_bridge)
// Output: array of RankedCompletion (caller frees)
struct RawrXD_RankedCompletion {
    RawrXD_SymbolCandidate symbol;
    float score;
    float lexical_score;
    float ast_score;
    float type_score;
    float frequency_score;
    float recency_score;
    float trigger_score;
    uint32_t context_flags;
};

RawrXD_RankedCompletion* rawrxd_ranking_rank(
    RawrXD_CompletionRankingKernel* handle,
    const RawrXD_SymbolCandidate* candidates,
    size_t candidate_count,
    const char* prefix,
    int trigger_kind,
    size_t line,
    size_t column,
    size_t* out_count);

void rawrxd_ranked_completions_free(RawrXD_RankedCompletion* completions, size_t count);

// Record acceptance
void rawrxd_ranking_record_accept(RawrXD_CompletionRankingKernel* handle,
                                    const char* symbol_name,
                                    const char* file_path);

} // extern "C"
