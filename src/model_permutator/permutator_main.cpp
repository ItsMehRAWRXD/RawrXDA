#include "model_permutator/ModelPermutationEngine.hpp"
#include "streaming/atc_benchmark.hpp"
#include <iostream>

namespace {

std::string get_parameter_value(const ModelPermutation& permutation, const std::string& name, const std::string& fallback) {
    for (const auto& parameter : permutation.parameters) {
        if (parameter.name == name) {
            return parameter.current_value;
        }
    }
    return fallback;
}

BenchmarkResult run_benchmark(const ModelPermutation& perm) {
    std::cout << "Testing Permutation ID: " << perm.id << " (" << perm.fingerprint << ")" << std::endl;

    const std::wstring model_path = L"d:\\codestral22b.gguf";
    const std::string prompt =
        "Profile the streamed model path using architecture=" + get_parameter_value(perm, "architecture", "llama") +
        ", quantization=" + get_parameter_value(perm, "quantization", "none") +
        ", and streaming=" + get_parameter_value(perm, "streaming_strategy", "none") + ".";

    ATCBenchmark benchmark;
    ATCBenchmarkResult atc_result = benchmark.run(model_path, prompt, std::string());

    BenchmarkResult result;
    result.permutation_id = perm.id;

    result.tokens_per_second = atc_result.tokens_per_second;
    result.time_to_first_token_ms = atc_result.time_to_first_token_ms;
    result.response_stream_start_ms = atc_result.response_stream_start_ms;
    result.peak_memory_usage_mb = atc_result.peak_working_set_mb;
    result.model_size_bytes = atc_result.model_size_bytes;
    result.prompt_length_chars = atc_result.prompt_length_chars;
    result.prompt_length_tokens = atc_result.prompt_length_tokens;
    result.generated_tokens = atc_result.generated_tokens;
    result.correctness_passed = atc_result.success;
    result.fingerprint = perm.fingerprint;

    if (!atc_result.error_message.empty()) {
        result.notes = atc_result.error_message;
        return result;
    }

    if (result.tokens_per_second > 280 && result.peak_memory_usage_mb < 1500) {
        result.notes = "Potential high-performance, low-memory candidate found!";
        std::cout << ">>>>>> " << result.notes << " <<<<<<" << std::endl;
    } else {
        result.notes = "Measured with real ATC benchmark path.";
    }

    return result;
}

}


int main() {
    ModelPermutationEngine engine;
    int permutations_to_test = 1000; // Run for a large number of iterations

    std::cout << "Starting model permutation testing..." << std::endl;
    std::cout << "Findings will be logged to: d:\\model_permutation_findings.jsonl" << std::endl;

    for (int i = 0; i < permutations_to_test; ++i) {
        ModelPermutation perm = engine.generate_permutation();
        BenchmarkResult result = run_benchmark(perm);
        
        engine.log_finding(perm, result);
    }

    std::cout << "Finished testing " << permutations_to_test << " permutations." << std::endl;

    return 0;
}
