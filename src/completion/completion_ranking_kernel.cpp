// ============================================================================
// completion_ranking_kernel.cpp — Phase 1b: Intent-Aware Completion Ranking
// ============================================================================

#include "completion_ranking_kernel.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace rawrxd {
namespace completion {

// ============================================================================
// Helpers
// ============================================================================

static uint64_t hires_now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        high_resolution_clock::now().time_since_epoch()
    ).count();
}

static uint64_t fnv1a_hash(const std::string& s) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// ============================================================================
// Lifecycle
// ============================================================================

CompletionRankingKernel::CompletionRankingKernel() = default;
CompletionRankingKernel::~CompletionRankingKernel() = default;

bool CompletionRankingKernel::initialize(const ScoringWeights& weights) {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    weights_ = weights;
    start_time_ms_ = hires_now_ms();
    total_rankings_ = 0;
    total_acceptances_ = 0;
    usage_stats_.clear();
    file_stats_.clear();
    
    initialized_ = true;
    return true;
}

void CompletionRankingKernel::shutdown() {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    usage_stats_.clear();
    file_stats_.clear();
    initialized_ = false;
}

// ============================================================================
// Main Ranking Entry
// ============================================================================

std::vector<RankedCompletion> CompletionRankingKernel::rank(
    const std::vector<SymbolCandidate>& candidates,
    const CompletionContext& context) {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (!initialized_ || candidates.empty()) {
        return {};
    }
    
    std::vector<RankedCompletion> ranked;
    ranked.reserve(candidates.size());
    
    for (const auto& cand : candidates) {
        RankedCompletion rc;
        rc.symbol = cand;
        
        // Compute individual scores
        rc.lexical_score    = scoreLexical(cand, context);
        rc.ast_score        = scoreAST(cand, context);
        rc.type_score       = scoreTypeAffinity(cand, context);
        rc.frequency_score  = scoreFrequency(cand, context);
        rc.recency_score    = scoreRecency(cand, context);
        rc.trigger_score    = scoreTrigger(cand, context);
        
        // Weighted sum
        rc.score =
            weights_.lexical_proximity   * rc.lexical_score +
            weights_.ast_distance        * rc.ast_score +
            weights_.type_affinity       * rc.type_score +
            weights_.usage_frequency     * rc.frequency_score +
            weights_.recency_bias        * rc.recency_score +
            weights_.trigger_strength    * rc.trigger_score;
        
        // Documentation bonus
        if (!cand.documentation.empty()) {
            rc.score += weights_.documentation_bonus;
        }
        
        // Context flags
        rc.context_flags = computeContextFlags(cand, context);
        
        // Stable tie-breaker
        rc.stable_key = computeStableKey(cand);
        
        ranked.push_back(rc);
    }
    
    // Stable sort by score (descending), then by stable_key (ascending)
    std::stable_sort(ranked.begin(), ranked.end(),
        [](const RankedCompletion& a, const RankedCompletion& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.stable_key < b.stable_key;
        });
    
    total_rankings_++;
    return ranked;
}

// ============================================================================
// Scoring Functions
// ============================================================================

float CompletionRankingKernel::scoreLexical(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    const std::string& prefix = ctx.prefix;
    const std::string& name = cand.name;
    
    if (prefix.empty() || name.empty()) return 0.0f;
    
    // Exact match
    if (name == prefix) return 1.0f;
    
    // Prefix match
    if (name.find(prefix) == 0) {
        float ratio = static_cast<float>(prefix.length()) / name.length();
        return 0.8f + 0.2f * ratio;
    }
    
    // Substring match
    if (name.find(prefix) != std::string::npos) {
        return 0.5f;
    }
    
    // No match
    return 0.0f;
}

float CompletionRankingKernel::scoreAST(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    float score = 0.0f;
    
    // Same enclosing function
    if (!ctx.ast.enclosing_function.empty() &&
        cand.name.find(ctx.ast.enclosing_function) != std::string::npos) {
        score += 0.4f;
    }
    
    // Same enclosing class
    if (!ctx.ast.enclosing_class.empty() &&
        cand.name.find(ctx.ast.enclosing_class) != std::string::npos) {
        score += 0.3f;
    }
    
    // Same module
    if (!ctx.ast.enclosing_module.empty() &&
        cand.source_file.find(ctx.ast.enclosing_module) != std::string::npos) {
        score += 0.2f;
    }
    
    // Sibling symbol
    for (const auto& sibling : ctx.ast.sibling_symbols) {
        if (cand.name == sibling) {
            score += 0.3f;
            break;
        }
    }
    
    // Scope depth penalty (deeper = less likely)
    score -= static_cast<float>(ctx.ast.scope_depth) * 0.05f;
    
    return std::max(0.0f, std::min(1.0f, score));
}

float CompletionRankingKernel::scoreTypeAffinity(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    switch (cand.kind) {
        case CompletionKind::Function:
            // Functions are generally most useful
            return 0.9f;
            
        case CompletionKind::Variable:
            // Variables are context-dependent
            if (!ctx.tokens.last_type_annotation.empty()) {
                // If we have a type annotation, prefer matching types
                if (cand.signature.find(ctx.tokens.last_type_annotation) != std::string::npos) {
                    return 0.8f;
                }
            }
            return 0.6f;
            
        case CompletionKind::Type:
            // Types are useful after `:` or in declarations
            if (ctx.trigger == TriggerKind::TypeAnnotation) {
                return 0.9f;
            }
            return 0.5f;
            
        case CompletionKind::Module:
            // Modules after `use` or `::`
            if (ctx.trigger == TriggerKind::ScopeResolution) {
                return 0.8f;
            }
            return 0.4f;
            
        case CompletionKind::Field:
            // Fields after `.` or `->`
            if (ctx.trigger == TriggerKind::MethodCall) {
                return 0.9f;
            }
            return 0.5f;
            
        default:
            return 0.5f;
    }
}

float CompletionRankingKernel::scoreFrequency(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Global usage
    auto global_it = usage_stats_.find(cand.name);
    uint32_t global_count = (global_it != usage_stats_.end()) 
        ? global_it->second.accept_count 
        : 0;
    
    // File-local usage
    uint32_t file_count = 0;
    auto file_it = file_stats_.find(ctx.file_path);
    if (file_it != file_stats_.end()) {
        auto sym_it = file_it->second.find(cand.name);
        if (sym_it != file_it->second.end()) {
            file_count = sym_it->second.accept_count;
        }
    }
    
    // Combine (file-local weighted higher)
    float score = static_cast<float>(file_count) * 0.7f +
                  static_cast<float>(global_count) * 0.3f;
    
    // Normalize (cap at 1.0)
    return std::min(1.0f, score / 10.0f);
}

float CompletionRankingKernel::scoreRecency(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = usage_stats_.find(cand.name);
    if (it == usage_stats_.end()) return 0.0f;
    
    uint64_t now = hires_now_ms();
    uint64_t last_edit = it->second.last_edited_ms;
    uint64_t last_accept = it->second.last_accepted_ms;
    
    if (last_edit == 0 && last_accept == 0) return 0.0f;
    
    // Use most recent interaction
    uint64_t last_interaction = std::max(last_edit, last_accept);
    uint64_t age_ms = now - last_interaction;
    
    // Exponential decay: score = exp(-age / half_life)
    // half_life = 5 minutes = 300,000 ms
    constexpr float half_life_ms = 300000.0f;
    float score = std::exp(-static_cast<float>(age_ms) / half_life_ms);
    
    return score;
}

float CompletionRankingKernel::scoreTrigger(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    switch (ctx.trigger) {
        case TriggerKind::ScopeResolution: // ::
            // Prefer modules, types, associated functions
            if (cand.kind == CompletionKind::Module ||
                cand.kind == CompletionKind::Type ||
                cand.kind == CompletionKind::Function) {
                return 1.0f;
            }
            return 0.3f;
            
        case TriggerKind::MethodCall: // . or ->
            // Prefer methods and fields
            if (cand.kind == CompletionKind::Function ||
                cand.kind == CompletionKind::Field) {
                return 1.0f;
            }
            return 0.2f;
            
        case TriggerKind::TypeAnnotation: // :
            // Prefer types
            if (cand.kind == CompletionKind::Type) {
                return 1.0f;
            }
            return 0.2f;
            
        case TriggerKind::Identifier:
            // Neutral - let other signals decide
            return 0.5f;
            
        default:
            return 0.0f;
    }
}

// ============================================================================
// Context Flags
// ============================================================================

uint32_t CompletionRankingKernel::computeContextFlags(
    const SymbolCandidate& cand, const CompletionContext& ctx) {
    
    uint32_t flags = 0;
    
    // Same scope
    if (cand.source_file == ctx.file_path) {
        flags |= CF_SAME_SCOPE;
    }
    
    // Sibling symbol
    for (const auto& sibling : ctx.ast.sibling_symbols) {
        if (cand.name == sibling) {
            flags |= CF_SIBLING_SYMBOL;
            break;
        }
    }
    
    // Hot symbol (frequently used)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = usage_stats_.find(cand.name);
        if (it != usage_stats_.end() && it->second.accept_count >= 5) {
            flags |= CF_HOT_SYMBOL;
        }
    }
    
    // Recent edit
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = usage_stats_.find(cand.name);
        if (it != usage_stats_.end() && it->second.last_edited_ms > 0) {
            uint64_t age = hires_now_ms() - it->second.last_edited_ms;
            if (age < 300000) { // 5 minutes
                flags |= CF_RECENT_EDIT;
            }
        }
    }
    
    // Type match
    if (!ctx.tokens.last_type_annotation.empty() &&
        cand.signature.find(ctx.tokens.last_type_annotation) != std::string::npos) {
        flags |= CF_TYPE_MATCH;
    }
    
    // Documentation available
    if (!cand.documentation.empty()) {
        flags |= CF_DOC_AVAILABLE;
    }
    
    // Exact match
    if (cand.name == ctx.prefix) {
        flags |= CF_EXACT_MATCH;
    }
    
    return flags;
}

// ============================================================================
// Stable Key (deterministic tie-breaker)
// ============================================================================

uint64_t CompletionRankingKernel::computeStableKey(const SymbolCandidate& cand) {
    // Deterministic hash of symbol name + signature + source file
    std::string key = cand.name + "|" + cand.signature + "|" + cand.source_file;
    return fnv1a_hash(key);
}

// ============================================================================
// Usage Tracking
// ============================================================================

void CompletionRankingKernel::recordAcceptance(
    const std::string& symbol_name,
    const std::string& file_path) {
    
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    uint64_t now = hires_now_ms();
    
    // Global stats
    auto& global = usage_stats_[symbol_name];
    global.accept_count++;
    global.last_accepted_ms = now;
    
    // File-local stats
    auto& file_sym = file_stats_[file_path][symbol_name];
    file_sym.accept_count++;
    file_sym.last_accepted_ms = now;
    
    total_acceptances_++;
}

void CompletionRankingKernel::recordEdit(
    const std::string& symbol_name,
    const std::string& file_path) {
    
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    uint64_t now = hires_now_ms();
    
    auto& global = usage_stats_[symbol_name];
    global.last_edited_ms = now;
    
    auto& file_sym = file_stats_[file_path][symbol_name];
    file_sym.last_edited_ms = now;
}

// ============================================================================
// Weight Management
// ============================================================================

ScoringWeights CompletionRankingKernel::getWeights() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return weights_;
}

void CompletionRankingKernel::setWeights(const ScoringWeights& weights) {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    weights_ = weights;
}

} // namespace completion
} // namespace rawrxd

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

RawrXD_CompletionRankingKernel* rawrxd_ranking_create() {
    auto* kernel = new rawrxd::completion::CompletionRankingKernel();
    if (!kernel->initialize()) {
        delete kernel;
        return nullptr;
    }
    return reinterpret_cast<RawrXD_CompletionRankingKernel*>(kernel);
}

void rawrxd_ranking_destroy(RawrXD_CompletionRankingKernel* handle) {
    if (handle) {
        auto* kernel = reinterpret_cast<rawrxd::completion::CompletionRankingKernel*>(handle);
        kernel->shutdown();
        delete kernel;
    }
}

int rawrxd_ranking_initialize(RawrXD_CompletionRankingKernel* handle,
                               const float* weights, size_t weight_count) {
    if (!handle) return 0;
    
    auto* kernel = reinterpret_cast<rawrxd::completion::CompletionRankingKernel*>(handle);
    
    rawrxd::completion::ScoringWeights sw;
    if (weight_count > 0) sw.lexical_proximity = weights[0];
    if (weight_count > 1) sw.ast_distance = weights[1];
    if (weight_count > 2) sw.type_affinity = weights[2];
    if (weight_count > 3) sw.usage_frequency = weights[3];
    if (weight_count > 4) sw.recency_bias = weights[4];
    if (weight_count > 5) sw.trigger_strength = weights[5];
    if (weight_count > 6) sw.documentation_bonus = weights[6];
    
    return kernel->initialize(sw) ? 1 : 0;
}

RawrXD_RankedCompletion* rawrxd_ranking_rank(
    RawrXD_CompletionRankingKernel* handle,
    const RawrXD_SymbolCandidate* candidates,
    size_t candidate_count,
    const char* prefix,
    int trigger_kind,
    size_t line,
    size_t column,
    size_t* out_count) {
    
    if (!handle || !candidates || !prefix || !out_count) return nullptr;
    
    auto* kernel = reinterpret_cast<rawrxd::completion::CompletionRankingKernel*>(handle);
    
    // Build context
    rawrxd::completion::CompletionContext ctx;
    ctx.prefix = prefix;
    ctx.trigger = static_cast<rawrxd::bridge::TriggerKind>(trigger_kind);
    ctx.line = line;
    ctx.column = column;
    
    // Build candidates
    std::vector<rawrxd::bridge::SymbolCandidate> cands;
    cands.reserve(candidate_count);
    
    for (size_t i = 0; i < candidate_count; i++) {
        rawrxd::bridge::SymbolCandidate cand;
        cand.name = candidates[i].name ? candidates[i].name : "";
        cand.signature = candidates[i].signature ? candidates[i].signature : "";
        cand.documentation = candidates[i].documentation ? candidates[i].documentation : "";
        cand.kind = static_cast<rawrxd::bridge::CompletionKind>(candidates[i].kind);
        cand.relevance_score = candidates[i].relevance_score;
        cand.source_file = candidates[i].source_file ? candidates[i].source_file : "";
        cand.line = candidates[i].line;
        cand.column = candidates[i].column;
        cands.push_back(cand);
    }
    
    // Rank
    auto ranked = kernel->rank(cands, ctx);
    
    *out_count = ranked.size();
    if (ranked.empty()) return nullptr;
    
    // Allocate output
    auto* result = new RawrXD_RankedCompletion[ranked.size()];
    for (size_t i = 0; i < ranked.size(); i++) {
        const auto& rc = ranked[i];
        
        // Copy symbol
        result[i].symbol.name = strdup(rc.symbol.name.c_str());
        result[i].symbol.signature = strdup(rc.symbol.signature.c_str());
        result[i].symbol.documentation = strdup(rc.symbol.documentation.c_str());
        result[i].symbol.kind = static_cast<int>(rc.symbol.kind);
        result[i].symbol.relevance_score = rc.symbol.relevance_score;
        result[i].symbol.source_file = strdup(rc.symbol.source_file.c_str());
        result[i].symbol.line = rc.symbol.line;
        result[i].symbol.column = rc.symbol.column;
        
        // Copy scores
        result[i].score = rc.score;
        result[i].lexical_score = rc.lexical_score;
        result[i].ast_score = rc.ast_score;
        result[i].type_score = rc.type_score;
        result[i].frequency_score = rc.frequency_score;
        result[i].recency_score = rc.recency_score;
        result[i].trigger_score = rc.trigger_score;
        result[i].context_flags = rc.context_flags;
    }
    
    return result;
}

void rawrxd_ranked_completions_free(RawrXD_RankedCompletion* completions, size_t count) {
    if (!completions) return;
    
    for (size_t i = 0; i < count; i++) {
        free(const_cast<char*>(completions[i].symbol.name));
        free(const_cast<char*>(completions[i].symbol.signature));
        free(const_cast<char*>(completions[i].symbol.documentation));
        free(const_cast<char*>(completions[i].symbol.source_file));
    }
    delete[] completions;
}

void rawrxd_ranking_record_accept(RawrXD_CompletionRankingKernel* handle,
                                   const char* symbol_name,
                                   const char* file_path) {
    if (!handle || !symbol_name || !file_path) return;
    
    auto* kernel = reinterpret_cast<rawrxd::completion::CompletionRankingKernel*>(handle);
    kernel->recordAcceptance(symbol_name, file_path);
}

} // extern "C"
