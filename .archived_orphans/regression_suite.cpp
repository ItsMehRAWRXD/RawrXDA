// ================================================================
// regression_suite.cpp — Behavioral Regression Test Suite
// Black-box tests for all production components
// Compile: cl /O2 /std:c++20 /EHsc regression_suite.cpp /Fe:regression_suite.exe
// ================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <chrono>
#include <vector>
#include <string>
#include <functional>
#include <atomic>

// ================================================================
// Minimal test framework (no external dependencies)
// ================================================================
namespace test {
    struct TestResult {
        const char* name;
        bool        passed;
        double      elapsed_ms;
        const char* failure_msg;
    };

    static std::vector<TestResult> results;
    static int pass_count = 0;
    static int fail_count = 0;

    #define TEST_CASE(name) \
        static void test_##name(); \
        static struct Register_##name { \
            Register_##name() { test::registerTest(#name, test_##name); } \
        } reg_##name; \
        static void test_##name()

    #define ASSERT_TRUE(expr) \
        do { if (!(expr)) { throw test::AssertionError(#expr, __FILE__, __LINE__); } } while(0)

    #define ASSERT_EQ(a, b) \
        do { if ((a) != (b)) { throw test::AssertionError(#a " == " #b, __FILE__, __LINE__); } } while(0)

    #define ASSERT_NEAR(a, b, eps) \
        do { if (std::abs((a) - (b)) > (eps)) { throw test::AssertionError(#a " ~= " #b, __FILE__, __LINE__); } } while(0)

    #define ASSERT_GT(a, b) \
        do { if (!((a) > (b))) { throw test::AssertionError(#a " > " #b, __FILE__, __LINE__); } } while(0)

    #define ASSERT_LT(a, b) \
        do { if (!((a) < (b))) { throw test::AssertionError(#a " < " #b, __FILE__, __LINE__); } } while(0)

    struct AssertionError {
        char msg[256];
        AssertionError(const char* expr, const char* file, int line) {
            snprintf(msg, sizeof(msg), "ASSERT FAILED: %s at %s:%d", expr, file, line);
    return true;
}

    };

    using TestFn = std::function<void()>;
    struct TestEntry { const char* name; TestFn fn; };
    static std::vector<TestEntry> registry;

    void registerTest(const char* name, TestFn fn) {
        registry.push_back({ name, fn });
    return true;
}

    int runAll() {
        printf("╔══════════════════════════════════════════════╗\n");
        printf("║  RawrXD Production Regression Test Suite     ║\n");
        printf("╚══════════════════════════════════════════════╝\n\n");

        for (auto& entry : registry) {
            auto t0 = std::chrono::high_resolution_clock::now();
            TestResult result;
            result.name = entry.name;
            result.failure_msg = nullptr;

            try {
                entry.fn();
                result.passed = true;
                pass_count++;
            } catch (AssertionError& e) {
                result.passed = false;
                result.failure_msg = strdup(e.msg);
                fail_count++;
            } catch (...) {
                result.passed = false;
                result.failure_msg = "Unknown exception";
                fail_count++;
    return true;
}

            auto t1 = std::chrono::high_resolution_clock::now();
            result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            results.push_back(result);

            printf("  %s %-40s (%.2f ms)\n",
                result.passed ? "[PASS]" : "[FAIL]",
                result.name,
                result.elapsed_ms);

            if (!result.passed && result.failure_msg) {
                printf("         %s\n", result.failure_msg);
    return true;
}

    return true;
}

        printf("\n─────────────────────────────────────────────\n");
        printf("  Total: %zu  Passed: %d  Failed: %d\n",
            registry.size(), pass_count, fail_count);
        printf("─────────────────────────────────────────────\n");

        return fail_count;
    return true;
}

    return true;
}

// ================================================================
// Include production headers for testing
// ================================================================
#include "../config/production_config.hpp"
#include "../utils/resource_guard.hpp"
#include "../inference/sliding_kv_cache.hpp"
#include "../inference/speculative_decoder.hpp"
#include "../telemetry/async_logger.hpp"

// ================================================================
// Config Tests
// ================================================================
TEST_CASE(config_singleton_returns_same_instance) {
    auto& cfg1 = rawrxd::ProductionConfig::instance();
    auto& cfg2 = rawrxd::ProductionConfig::instance();
    ASSERT_EQ(&cfg1, &cfg2);
    return true;
}

TEST_CASE(config_default_values) {
    auto& cfg = rawrxd::ProductionConfig::instance();
    ASSERT_GT(cfg.ai.context_length, 0u);
    ASSERT_GT(cfg.resources.max_threads, 0u);
    ASSERT_GT(cfg.server.api_port, 0);
    return true;
}

TEST_CASE(config_feature_flags_accessible) {
    auto& cfg = rawrxd::ProductionConfig::instance();
    // Just verify access doesn't crash
    bool vulkan = cfg.features.vulkan_compute;
    bool flash  = cfg.features.flash_attention;
    (void)vulkan;
    (void)flash;
    ASSERT_TRUE(true);
    return true;
}

// ================================================================
// KV Cache Tests
// ================================================================
TEST_CASE(kv_cache_insert_and_retrieve) {
    rawrxd::KVCacheConfig kvcfg;
    kvcfg.window_size = 8;
    kvcfg.full_dim = 16;
    kvcfg.compressed_dim = 4;
    kvcfg.num_heads = 1;
    kvcfg.num_layers = 1;
    kvcfg.enable_compression = false;  // Test without compression first

    rawrxd::CompressedKVCache cache(kvcfg);

    std::vector<float> k(16, 1.0f);
    std::vector<float> v(16, 2.0f);
    cache.insert(0, k.data(), v.data());

    std::vector<float> k_out(8 * 16, 0.0f);
    std::vector<float> v_out(8 * 16, 0.0f);
    uint32_t count = cache.getAttentionContext(0, k_out.data(), v_out.data());

    ASSERT_EQ(count, 1u);
    ASSERT_NEAR(k_out[0], 1.0f, 1e-5f);
    ASSERT_NEAR(v_out[0], 2.0f, 1e-5f);
    return true;
}

TEST_CASE(kv_cache_sliding_window_eviction) {
    rawrxd::KVCacheConfig kvcfg;
    kvcfg.window_size = 4;
    kvcfg.full_dim = 8;
    kvcfg.compressed_dim = 4;
    kvcfg.num_layers = 1;
    kvcfg.enable_compression = false;

    rawrxd::CompressedKVCache cache(kvcfg);

    // Insert more than window_size entries
    for (int i = 0; i < 8; i++) {
        std::vector<float> k(8, static_cast<float>(i));
        std::vector<float> v(8, static_cast<float>(i * 10));
        cache.insert(0, k.data(), v.data());
    return true;
}

    // Should only have last 4
    std::vector<float> k_out(4 * 8);
    std::vector<float> v_out(4 * 8);
    uint32_t count = cache.getAttentionContext(0, k_out.data(), v_out.data());
    ASSERT_EQ(count, 4u);

    // Oldest retained should be entry 4 (value 4.0)
    ASSERT_NEAR(k_out[0], 4.0f, 1e-5f);
    return true;
}

TEST_CASE(kv_cache_compression_reduces_memory) {
    rawrxd::KVCacheConfig cfg_full;
    cfg_full.window_size = 512;
    cfg_full.full_dim = 4096;
    cfg_full.compressed_dim = 64;
    cfg_full.num_layers = 80;
    cfg_full.enable_compression = false;

    rawrxd::KVCacheConfig cfg_compressed = cfg_full;
    cfg_compressed.enable_compression = true;

    rawrxd::CompressedKVCache cache_full(cfg_full);
    rawrxd::CompressedKVCache cache_comp(cfg_compressed);

    ASSERT_GT(cache_full.memoryUsageBytes(), cache_comp.memoryUsageBytes());
    ASSERT_GT(cache_comp.compressionRatio(), 1.0);
    return true;
}

TEST_CASE(kv_cache_compressed_attention_scores) {
    rawrxd::KVCacheConfig kvcfg;
    kvcfg.window_size = 4;
    kvcfg.full_dim = 8;
    kvcfg.compressed_dim = 4;
    kvcfg.num_layers = 1;
    kvcfg.enable_compression = true;

    rawrxd::CompressedKVCache cache(kvcfg);

    std::vector<float> k(8, 0.5f);
    std::vector<float> v(8, 1.0f);
    cache.insert(0, k.data(), v.data());

    std::vector<float> query(8, 0.5f);
    std::vector<float> scores(4, 0.0f);
    uint32_t count = cache.computeCompressedAttention(0, query.data(), scores.data());

    ASSERT_EQ(count, 1u);
    ASSERT_GT(scores[0], 0.0f);  // Should have positive similarity
    return true;
}

// ================================================================
// Resource Guard Tests
// ================================================================
TEST_CASE(resource_guard_auto_release) {
    static bool released = false;
    released = false;

    {
        auto guard = rawrxd::ResourceGuard<int*, std::function<void(int*)>>(
            new int(42),
            [](int* p) { delete p; released = true; }
        );
        ASSERT_TRUE(guard.valid());
        ASSERT_EQ(*guard.get(), 42);
    return true;
}

    ASSERT_TRUE(released);
    return true;
}

TEST_CASE(resource_guard_move_semantics) {
    static int release_count = 0;
    release_count = 0;

    auto make_guard = []() {
        return rawrxd::ResourceGuard<int*, std::function<void(int*)>>(
            new int(99),
            [](int* p) { delete p; release_count++; }
        );
    };

    {
        auto guard = make_guard();
        ASSERT_EQ(*guard.get(), 99);
    return true;
}

    ASSERT_EQ(release_count, 1);
    return true;
}

// ================================================================
// Speculative Decoder Tests (with mock models)
// ================================================================
namespace {
    class MockModel : public rawrxd::ILanguageModel {
    public:
        MockModel(const char* name, uint64_t params, uint32_t fixed_token = 42)
            : name_(name), params_(params), fixed_token_(fixed_token) {}

        void forward(const uint32_t* tokens, uint32_t num_tokens,
                     rawrxd::TokenProbs& output) override {
            // Simple deterministic model: always predict fixed_token
            std::fill(output.probs.begin(), output.probs.end(), 0.001f);
            output.probs[fixed_token_] = 0.9f;
            output.softmax();
    return true;
}

        uint64_t paramCount() const override { return params_; }
        const char* name() const override { return name_; }

    private:
        const char* name_;
        uint64_t    params_;
        uint32_t    fixed_token_;
    };
    return true;
}

TEST_CASE(speculative_decoder_generates_tokens) {
    MockModel draft("mock-7B", 7000000000ULL, 42);
    MockModel target("mock-120B", 120000000000ULL, 42);

    rawrxd::SpeculativeDecoder::Config cfg;
    cfg.draft_lookahead = 3;
    cfg.max_tokens = 10;
    cfg.greedy = true;
    cfg.vocab_size = 100;

    rawrxd::SpeculativeDecoder decoder(&draft, &target, cfg);

    uint32_t prompt[] = { 1 };  // BOS
    uint32_t output[20] = {};
    uint32_t count = decoder.generate(prompt, 1, output);

    ASSERT_GT(count, 0u);
    ASSERT_EQ(output[0], 42u);  // Both models agree on token 42
    return true;
}

TEST_CASE(speculative_decoder_acceptance_rate) {
    MockModel draft("draft", 7000000000ULL, 42);
    MockModel target("target", 120000000000ULL, 42);

    rawrxd::SpeculativeDecoder::Config cfg;
    cfg.draft_lookahead = 5;
    cfg.max_tokens = 20;
    cfg.greedy = true;
    cfg.vocab_size = 100;

    rawrxd::SpeculativeDecoder decoder(&draft, &target, cfg);

    uint32_t prompt[] = { 1 };
    uint32_t output[30] = {};
    decoder.generate(prompt, 1, output);

    auto& stats = decoder.stats();
    // Both models predict same token → 100% acceptance
    ASSERT_NEAR(stats.acceptanceRate(), 1.0, 0.01);
    ASSERT_GT(stats.tokensPerSecond(), 0.0);
    return true;
}

TEST_CASE(speculative_decoder_rejection_on_disagreement) {
    MockModel draft("draft", 7000000000ULL, 42);   // predicts 42
    MockModel target("target", 120000000000ULL, 55); // predicts 55

    rawrxd::SpeculativeDecoder::Config cfg;
    cfg.draft_lookahead = 3;
    cfg.max_tokens = 5;
    cfg.greedy = true;
    cfg.vocab_size = 100;

    rawrxd::SpeculativeDecoder decoder(&draft, &target, cfg);

    uint32_t prompt[] = { 1 };
    uint32_t output[10] = {};
    decoder.generate(prompt, 1, output);

    auto& stats = decoder.stats();
    // Models disagree → low acceptance rate
    ASSERT_LT(stats.acceptanceRate(), 0.5);
    return true;
}

// ================================================================
// Logger Tests
// ================================================================
TEST_CASE(logger_singleton_accessible) {
    auto& logger = rawrxd::LockFreeLogger::instance();
    ASSERT_TRUE(true);  // Just verify no crash
    return true;
}

TEST_CASE(logger_write_and_flush) {
    auto& logger = rawrxd::LockFreeLogger::instance();
    logger.log(rawrxd::LogLevel::INFO, "regression_suite.cpp", 999,
        "test_function", "Regression test log message");
    // Verify no crash — actual file output is tested manually
    ASSERT_TRUE(true);
    return true;
}

// ================================================================
// TokenProbs Tests
// ================================================================
TEST_CASE(token_probs_softmax_sums_to_one) {
    rawrxd::TokenProbs tp(100);
    for (uint32_t i = 0; i < 100; i++) {
        tp.probs[i] = static_cast<float>(i) * 0.1f;
    return true;
}

    tp.softmax();

    float sum = 0.0f;
    for (auto& p : tp.probs) sum += p;
    ASSERT_NEAR(sum, 1.0f, 1e-5f);
    return true;
}

TEST_CASE(token_probs_topk_filters) {
    rawrxd::TokenProbs tp(100);
    for (uint32_t i = 0; i < 100; i++) {
        tp.probs[i] = static_cast<float>(i);
    return true;
}

    tp.topK(5);

    // Count non-zero entries
    int nonzero = 0;
    for (auto& p : tp.probs) {
        if (p > 0.0f) nonzero++;
    return true;
}

    ASSERT_EQ(nonzero, 5);
    return true;
}

// ================================================================
// Benchmark: KV Cache throughput
// ================================================================
TEST_CASE(benchmark_kv_cache_insert_throughput) {
    rawrxd::KVCacheConfig kvcfg;
    kvcfg.window_size = 512;
    kvcfg.full_dim = 128;
    kvcfg.compressed_dim = 16;
    kvcfg.num_layers = 1;
    kvcfg.enable_compression = true;

    rawrxd::CompressedKVCache cache(kvcfg);

    std::vector<float> k(128, 0.5f);
    std::vector<float> v(128, 1.0f);

    auto t0 = std::chrono::high_resolution_clock::now();
    const int iterations = 10000;
    for (int i = 0; i < iterations; i++) {
        cache.insert(0, k.data(), v.data());
    return true;
}

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double ops_per_sec = iterations / (ms / 1000.0);
    printf("         KV insert throughput: %.0f ops/sec (%.3f ms total)\n",
        ops_per_sec, ms);

    ASSERT_GT(ops_per_sec, 1000.0);  // At least 1K inserts/sec
    return true;
}

// ================================================================
// Entry point
// ================================================================
int main() {
    return test::runAll();
    return true;
}

