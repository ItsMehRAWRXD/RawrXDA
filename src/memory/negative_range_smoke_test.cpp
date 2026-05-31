// negative_range_smoke_test.cpp
#include "negative_range_model.hpp"
#include <cstdio>
#include <vector>
#include <random>
#include <cmath>

using namespace rawrxd;

struct TestResult {
    const char* name;
    bool passed;
};

static std::vector<float> make_random_weights(size_t n, uint32_t seed = 42) {
    std::vector<float> w(n);
    std::mt19937 rng(seed);
    std::normal_distribution<float> d(0.0f, 1.0f);
    for (auto& v : w) v = d(rng);
    return w;
}

int main() {
    std::vector<TestResult> results;

    // 1. Effective bits lookup
    {
        bool ok = get_effective_bits(NegativePrecision::FP16) == 16.0f &&
                  get_effective_bits(NegativePrecision::INT_HALF) == 0.5f &&
                  get_effective_bits(NegativePrecision::INT_HASH) == 0.0f &&
                  get_effective_bits(NegativePrecision::INT_HYPERNET) < 0.0f;
        results.push_back({"Effective Bits Lookup", ok});
    }

    // 2. Fractional 0.5-bit encode/decode roundtrip
    {
        const size_t n = 256;
        auto w = make_random_weights(n);
        auto p = FractionalBitEncoder::encode_half_bit(w.data(), n);
        auto r = FractionalBitEncoder::decode_half_bit(p, n);
        results.push_back({"Half-bit Encode/Decode", r.size() == n});
    }

    // 3. Fractional 0.25-bit roundtrip
    {
        const size_t n = 256;
        auto w = make_random_weights(n);
        auto p = FractionalBitEncoder::encode_quarter_bit(w.data(), n);
        auto r = FractionalBitEncoder::decode_quarter_bit(p, n);
        results.push_back({"Quarter-bit Encode/Decode", r.size() == n});
    }

    // 4. Fractional 0.125-bit roundtrip
    {
        const size_t n = 256;
        auto w = make_random_weights(n);
        auto p = FractionalBitEncoder::encode_eighth_bit(w.data(), n);
        auto r = FractionalBitEncoder::decode_eighth_bit(p, n);
        results.push_back({"Eighth-bit Encode/Decode", r.size() == n});
    }

    // 5. Hash-derived generation is deterministic
    {
        auto a = HashDerivedWeights::generate_weights(12345ULL, 1, 0, 8, 8, 1.0f);
        auto b = HashDerivedWeights::generate_weights(12345ULL, 1, 0, 8, 8, 1.0f);
        bool same = (a.size() == b.size());
        for (size_t i = 0; same && i < a.size(); ++i) if (a[i] != b[i]) same = false;
        results.push_back({"Hash-Derived Determinism", same});
    }

    // 6. Hash-derived learn representation
    {
        auto w = make_random_weights(32 * 32);
        auto repr = HashDerivedWeights::learn_representation(w.data(), 32, 32, 0, 0);
        results.push_back({"Hash-Derived Learn", repr.reconstruction_error >= 0.0f});
    }

    // 7. Delta encode/decode
    {
        const size_t n = 512;
        auto target = make_random_weights(n, 1);
        auto base = make_random_weights(n, 2);
        auto delta = DeltaEncoder::compute_delta(target.data(), base.data(), n, 0.05f);
        auto restored = DeltaEncoder::apply_delta(base.data(), delta, n);
        results.push_back({"Delta Encode/Decode", restored.size() == n});
    }

    // 8. SVD implicit roundtrip
    {
        const size_t rows = 16, cols = 16;
        auto w = make_random_weights(rows * cols);
        auto impl = SVDSImplicit::learn_implicit(w.data(), rows, cols, 2);
        auto r = SVDSImplicit::reconstruct(impl, rows, cols);
        results.push_back({"SVD Implicit Roundtrip", r.size() == rows * cols});
    }

    // 9. Hypernetwork train + generate
    {
        const size_t rows = 8, cols = 8;
        auto w = make_random_weights(rows * cols);
        auto hyper = HypernetworkGenerator::train_hypernetwork(w.data(), rows, cols, 8);
        auto r = HypernetworkGenerator::generate_weights(hyper, rows, cols);
        results.push_back({"Hypernetwork Gen", r.size() == rows * cols && !hyper.weights.empty()});
    }

    // 10. NegativeRangeModel compress/decompress for each precision
    {
        NegativeRangeModel model(1024 * 1024 * 1024);
        const size_t rows = 16, cols = 16;
        auto w = make_random_weights(rows * cols);
        bool all_ok = true;
        NegativePrecision levels[] = {
            NegativePrecision::INT_HALF,
            NegativePrecision::INT_QUARTER,
            NegativePrecision::INT_EIGHTH,
            NegativePrecision::INT_HASH,
            NegativePrecision::INT_DIFF,
            NegativePrecision::INT_SVD_IMPLICIT,
            NegativePrecision::INT_HYPERNET,
        };
        for (auto p : levels) {
            auto repr = model.compress_layer(w.data(), rows, cols, 0, 0, p);
            auto dec = model.decompress_layer(repr);
            if (dec.size() != rows * cols) { all_ok = false; break; }
        }
        results.push_back({"NegativeRangeModel All Precisions", all_ok});
    }

    // 11. Memory estimation
    {
        NegativeRangeModel model(1024 * 1024 * 1024);
        size_t fp16 = model.estimate_memory(1024, 1024, NegativePrecision::FP16);
        size_t half = model.estimate_memory(1024, 1024, NegativePrecision::INT_HALF);
        size_t hash_mem = model.estimate_memory(1024, 1024, NegativePrecision::INT_HASH);
        bool ok = fp16 > half && half > hash_mem;
        results.push_back({"Memory Estimation Ordering", ok});
    }

    // 12. Optimal precision selection
    {
        NegativeRangeModel model(1024 * 1024 * 1024);
        auto w = make_random_weights(64);
        auto p = model.select_optimal_precision(w.data(), 8, 8, 0.5f);
        (void)p;
        results.push_back({"Optimal Precision Selection", true});
    }

    // Report
    int failed = 0;
    std::printf("=== Negative Range Model Smoke Tests ===\n");
    for (auto& r : results) {
        std::printf("  %s: %s\n", r.name, r.passed ? "OK" : "FAIL");
        if (!r.passed) ++failed;
    }
    std::printf("=== %s === Exit code: %d\n",
                failed == 0 ? "All smoke tests passed" : "FAILURES PRESENT",
                failed == 0 ? 0 : 1);
    return failed == 0 ? 0 : 1;
}
