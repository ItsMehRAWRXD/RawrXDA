// cli_benchmark_main.cpp
// A command-line entry point for running performance and correctness benchmarks.

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "streaming/atc_benchmark.hpp"
#include "quantization/braided_quantizer_fingerprint.hpp"

void print_usage() {
    std::cout << "RawrXD Benchmark Runner\n";
    std::cout << "Usage: benchmark_cli.exe [test_name]\n\n";
    std::cout << "Available tests:\n";
    std::cout << "  atc           - Runs the Adaptive Tensor Codec (ATC) streaming benchmark.\n";
    std::cout << "  braided       - Runs the Braided Quantizer fingerprint test.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string test_name = argv[1];

    if (test_name == "atc") {
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "--- Running Adaptive Tensor Codec (ATC) Benchmark ---\n";
        ATCBenchmark benchmark;
        
        // In a real CLI, you'd parse this from args. For now, it's hardcoded.
        std::wstring model_path = L"d:\\codestral22b.gguf";
        std::string prompt = "Test";
        std::string reference_output;
        for (int i = 0; i < 100; ++i) {
            reference_output += static_cast<char>(('T' + i) % 256);
        }

        ATCBenchmarkResult result = benchmark.run(model_path, prompt, reference_output);

        if (result.success) {
            std::cout << "Result: SUCCESS\n";
            std::cout << "  Time to First Token: " << result.time_to_first_token_ms << " ms\n";
            std::cout << "  Response Stream Start: " << result.response_stream_start_ms << " ms\n";
            std::cout << "  Tokens per Second: " << result.tokens_per_second << " TPS\n";
            std::cout << "  Model Size: " << result.model_size_bytes << " bytes\n";
            std::cout << "  Prompt Length: " << result.prompt_length_chars << " chars / " << result.prompt_length_tokens << " tokens\n";
            std::cout << "  Generated Tokens: " << result.generated_tokens << "\n";
            std::cout << "  Peak Working Set: " << result.peak_working_set_mb << " MB\n";
            std::cout << "  Fingerprint Match: " << (result.fingerprint_match ? "Yes" : "No") << "\n";
        } else {
            std::cout << "Result: FAILED\n";
            std::cout << "  Error: " << result.error_message << "\n";
        }

    } else if (test_name == "braided") {
        std::cout << "--- Running Braided Quantizer Fingerprint Test ---\n";
        BraidedQuantizerFingerprintResult result = BraidedQuantizerFingerprint::run_test();
        
        std::cout << result.message;

        if (!result.passed) {
            return 1; // Return error code on failure
        }

    } else {
        std::cout << "Error: Unknown test '" << test_name << "'\n";
        print_usage();
        return 1;
    }

    return 0;
}
