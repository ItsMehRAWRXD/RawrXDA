// benchmark_runner_stub.cpp — IDE build variant: parity stub (stub holder; see UNFINISHED_FEATURES.md).
// unique_ptr<BenchmarkRunner> and members can be destroyed; full impl linked in benchmark build.

#include "../../include/benchmark_runner.hpp"
#include "../../include/inference_engine_stub.hpp"
#include "../engine/gguf_core.h"
#include "../../include/transformer_block_scalar.h"
#include <windows.h>

// IDE build variant: minimal RealTimeCompletionEngine so unique_ptr can destroy in ~BenchmarkRunner.
// Full implementation in include/real_time_completion_engine.h (linked in benchmark build).
struct RealTimeCompletionEngine {
    ~RealTimeCompletionEngine() {}
};

BenchmarkRunner::~BenchmarkRunner() = default;

void BenchmarkRunner::runBenchmarks(const std::vector<std::string>&, const std::string&, bool, bool) {}

void BenchmarkRunner::stop() {}

const std::vector<BenchmarkTestResult>& BenchmarkRunner::getResults() const {
    static const std::vector<BenchmarkTestResult> empty;
    return empty;
}

// InferenceEngine destructor (IDE build variant; required so unique_ptr can destroy it)
InferenceEngine::~InferenceEngine() = default;

// TransformerBlockScalar destructor — declared in header, no other .cpp defines it
TransformerBlockScalar::~TransformerBlockScalar() = default;
