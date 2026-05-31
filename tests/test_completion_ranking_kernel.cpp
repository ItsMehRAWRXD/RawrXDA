// ============================================================================
// test_completion_ranking_kernel.cpp — Unit tests for Phase 1b
// ============================================================================

#include "completion/completion_ranking_kernel.hpp"
#include <iostream>
#include <assert>
#include <math>

using namespace rawrxd::completion;
using namespace rawrxd::bridge;

// ============================================================================
// Test Helpers
// ============================================================================

static SymbolCandidate makeCandidate(const std::string& name,
                                      CompletionKind kind,
                                      const std::string& signature = "",
                                      const std::string& docs = "",
                                      const std::string& file = "test.rs") {
    SymbolCandidate cand;
    cand.name = name;
    cand.kind = kind;
    cand.signature = signature;
    cand.documentation = docs;
    cand.source_file = file;
    cand.line = 1;
    cand.column = 1;
    cand.relevance_score = 0.0f;
    return cand;
}

static CompletionContext makeContext(const std::string& prefix,
                                      TriggerKind trigger = TriggerKind::Identifier,
                                      const std::string& file = "test.rs") {
    CompletionContext ctx;
    ctx.prefix = prefix;
    ctx.trigger = trigger;
    ctx.file_path = file;
    ctx.line = 10;
    ctx.column = 5;
    return ctx;
}

static bool floatEq(float a, float b, float epsilon = 0.01f) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Test 1: Initialization
// ============================================================================
void test_initialization() {
    std::cout << "[Test 1] Initialization... ";
    
    CompletionRankingKernel kernel;
    assert(kernel.initialize());
    assert(kernel.isInitialized());
    
    // Check default weights
    auto weights = kernel.getWeights();
    assert(floatEq(weights.lexical_proximity, 1.0f));
    assert(floatEq(weights.ast_distance, 0.8f));
    assert(floatEq(weights.type_affinity, 0.6f));
    
    kernel.shutdown();
    assert(!kernel.isInitialized());
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 2: Lexical scoring
// ============================================================================
void test_lexical_scoring() {
    std::cout << "[Test 2] Lexical scoring... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("print_hello", CompletionKind::Function),
        makeCandidate("println", CompletionKind::Function),
        makeCandidate("private_var", CompletionKind::Variable),
        makeCandidate("main", CompletionKind::Function),
    };
    
    auto ctx = makeContext("pri");
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    
    // "print_hello" and "println" should be top (prefix match)
    // "private_var" should also match
    // "main" should be last
    
    bool found_print_hello = false;
    bool found_main = false;
    size_t main_rank = 0;
    
    for (size_t i = 0; i < ranked.size(); i++) {
        if (ranked[i].symbol.name == "print_hello") {
            found_print_hello = true;
            assert(ranked[i].lexical_score > 0.8f); // prefix match
        }
        if (ranked[i].symbol.name == "main") {
            found_main = true;
            main_rank = i;
        }
    }
    
    assert(found_print_hello);
    assert(found_main);
    assert(main_rank > 0); // main should not be first
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 3: Exact match bonus
// ============================================================================
void test_exact_match() {
    std::cout << "[Test 3] Exact match bonus... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("print", CompletionKind::Function),
        makeCandidate("println", CompletionKind::Function),
        makeCandidate("print_hello", CompletionKind::Function),
    };
    
    auto ctx = makeContext("print");
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    assert(ranked[0].symbol.name == "print"); // exact match should be first
    assert(ranked[0].lexical_score == 1.0f);
    assert((ranked[0].context_flags & CF_EXACT_MATCH) != 0);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 4: Type affinity scoring
// ============================================================================
void test_type_affinity() {
    std::cout << "[Test 4] Type affinity... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("my_func", CompletionKind::Function),
        makeCandidate("my_var", CompletionKind::Variable),
        makeCandidate("MyType", CompletionKind::Type),
        makeCandidate("my_module", CompletionKind::Module),
    };
    
    // After `::` trigger, prefer modules and types
    auto ctx = makeContext("my", TriggerKind::ScopeResolution);
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    
    // Module should score high with :: trigger
    bool found_module = false;
    for (const auto& rc : ranked) {
        if (rc.symbol.name == "my_module") {
            found_module = true;
            assert(rc.trigger_score > 0.5f);
            break;
        }
    }
    assert(found_module);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 5: Trigger strength
// ============================================================================
void test_trigger_strength() {
    std::cout << "[Test 5] Trigger strength... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("method_a", CompletionKind::Function),
        makeCandidate("field_b", CompletionKind::Field),
        makeCandidate("type_c", CompletionKind::Type),
    };
    
    // Method call trigger
    auto ctx_method = makeContext("", TriggerKind::MethodCall);
    auto ranked_method = kernel.rank(candidates, ctx_method);
    
    // Functions and fields should score high
    for (const auto& rc : ranked_method) {
        if (rc.symbol.name == "method_a" || rc.symbol.name == "field_b") {
            assert(rc.trigger_score > 0.5f);
        }
    }
    
    // Type annotation trigger
    auto ctx_type = makeContext("", TriggerKind::TypeAnnotation);
    auto ranked_type = kernel.rank(candidates, ctx_type);
    
    // Types should score high
    for (const auto& rc : ranked_type) {
        if (rc.symbol.name == "type_c") {
            assert(rc.trigger_score > 0.5f);
            break;
        }
    }
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 6: Usage frequency
// ============================================================================
void test_usage_frequency() {
    std::cout << "[Test 6] Usage frequency... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    // Record some acceptances
    kernel.recordAcceptance("hot_symbol", "test.rs");
    kernel.recordAcceptance("hot_symbol", "test.rs");
    kernel.recordAcceptance("hot_symbol", "test.rs");
    kernel.recordAcceptance("hot_symbol", "other.rs");
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("hot_symbol", CompletionKind::Function),
        makeCandidate("cold_symbol", CompletionKind::Function),
    };
    
    auto ctx = makeContext("");
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    
    // hot_symbol should have higher frequency score
    bool found_hot = false;
    bool found_cold = false;
    float hot_freq = 0.0f;
    float cold_freq = 0.0f;
    
    for (const auto& rc : ranked) {
        if (rc.symbol.name == "hot_symbol") {
            found_hot = true;
            hot_freq = rc.frequency_score;
        }
        if (rc.symbol.name == "cold_symbol") {
            found_cold = true;
            cold_freq = rc.frequency_score;
        }
    }
    
    assert(found_hot);
    assert(found_cold);
    assert(hot_freq > cold_freq);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 7: Recency bias
// ============================================================================
void test_recency_bias() {
    std::cout << "[Test 7] Recency bias... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    // Record edit for recent_symbol
    kernel.recordEdit("recent_symbol", "test.rs");
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("recent_symbol", CompletionKind::Function),
        makeCandidate("old_symbol", CompletionKind::Function),
    };
    
    auto ctx = makeContext("");
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    
    // recent_symbol should have higher recency score
    bool found_recent = false;
    float recent_score = 0.0f;
    float old_score = 0.0f;
    
    for (const auto& rc : ranked) {
        if (rc.symbol.name == "recent_symbol") {
            found_recent = true;
            recent_score = rc.recency_score;
        }
        if (rc.symbol.name == "old_symbol") {
            old_score = rc.recency_score;
        }
    }
    
    assert(found_recent);
    assert(recent_score > old_score);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 8: Deterministic tie-breaking
// ============================================================================
void test_deterministic_tie_breaking() {
    std::cout << "[Test 8] Deterministic tie-breaking... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    // Two identical candidates (same score)
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("symbol_a", CompletionKind::Function, "fn a()", "", "file1.rs"),
        makeCandidate("symbol_a", CompletionKind::Function, "fn a()", "", "file2.rs"),
    };
    
    auto ctx = makeContext("symbol_a");
    
    // Rank multiple times
    auto ranked1 = kernel.rank(candidates, ctx);
    auto ranked2 = kernel.rank(candidates, ctx);
    auto ranked3 = kernel.rank(candidates, ctx);
    
    // All should produce identical ordering
    assert(ranked1.size() == ranked2.size());
    assert(ranked2.size() == ranked3.size());
    
    for (size_t i = 0; i < ranked1.size(); i++) {
        assert(ranked1[i].stable_key == ranked2[i].stable_key);
        assert(ranked2[i].stable_key == ranked3[i].stable_key);
        assert(ranked1[i].score == ranked2[i].score);
        assert(ranked2[i].score == ranked3[i].score);
    }
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 9: Context flags
// ============================================================================
void test_context_flags() {
    std::cout << "[Test 9] Context flags... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    std::vector<SymbolCandidate> candidates = {
        makeCandidate("local_fn", CompletionKind::Function, "", "docs here", "test.rs"),
        makeCandidate("other_fn", CompletionKind::Function, "", "", "other.rs"),
    };
    
    auto ctx = makeContext("", TriggerKind::Identifier, "test.rs");
    auto ranked = kernel.rank(candidates, ctx);
    
    assert(!ranked.empty());
    
    // local_fn should have SAME_SCOPE and DOC_AVAILABLE flags
    for (const auto& rc : ranked) {
        if (rc.symbol.name == "local_fn") {
            assert((rc.context_flags & CF_SAME_SCOPE) != 0);
            assert((rc.context_flags & CF_DOC_AVAILABLE) != 0);
        }
    }
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 10: Weight customization
// ============================================================================
void test_weight_customization() {
    std::cout << "[Test 10] Weight customization... ";
    
    CompletionRankingKernel kernel;
    kernel.initialize();
    
    // Set custom weights
    ScoringWeights custom;
    custom.lexical_proximity = 2.0f;
    custom.ast_distance = 0.0f;
    custom.type_affinity = 0.0f;
    custom.usage_frequency = 0.0f;
    custom.recency_bias = 0.0f;
    custom.trigger_strength = 0.0f;
    
    kernel.setWeights(custom);
    
    auto weights = kernel.getWeights();
    assert(floatEq(weights.lexical_proximity, 2.0f));
    assert(floatEq(weights.ast_distance, 0.0f));
    
    std::cout << "PASS\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Completion Ranking Kernel Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_initialization();
        test_lexical_scoring();
        test_exact_match();
        test_type_affinity();
        test_trigger_strength();
        test_usage_frequency();
        test_recency_bias();
        test_deterministic_tie_breaking();
        test_context_flags();
        test_weight_customization();
        
        std::cout << "\n========================================\n";
        std::cout << "All 10 tests PASSED ✅\n";
        std::cout << "========================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n========================================\n";
        std::cerr << "Test FAILED ❌\n";
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "========================================\n";
        return 1;
    }
}
