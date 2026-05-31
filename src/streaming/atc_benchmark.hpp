// atc_benchmark.hpp

#pragma once

#include "streaming/AdaptiveTensorCodec.hpp"
#include <string>
#include <chrono>
#include <vector>

// A structure to hold the results of the ATC benchmark.
struct ATCBenchmarkResult {
    bool success = false;
    double time_to_first_token_ms = 0.0;
    double response_stream_start_ms = 0.0;
    double tokens_per_second = 0.0;
    size_t peak_working_set_mb = 0;
    size_t model_size_bytes = 0;
    size_t prompt_length_chars = 0;
    size_t prompt_length_tokens = 0;
    size_t generated_tokens = 0;
    bool fingerprint_match = false;
    std::string output;
    std::string error_message;
};

// A simple timer class.
class SimpleTimer {
public:
    SimpleTimer() : start_time_(std::chrono::high_resolution_clock::now()) {}

    // Returns elapsed time in milliseconds.
    double elapsed_ms() {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end_time - start_time_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

// The main class for running the ATC benchmark.
class ATCBenchmark {
public:
    ATCBenchmarkResult run(const std::wstring& model_path, const std::string& prompt, const std::string& reference_output);

private:
    // Simulates an inference loop using the ATC.
    std::string perform_streaming_inference(rawrxd::AdaptiveTensorCodec& atc, const std::string& prompt, double& ttft);
    
    // Gets the current process's memory usage.
    size_t get_peak_working_set_mb();

    // Gets the file size of the model on disk.
    size_t get_model_size_bytes(const std::wstring& model_path);
};
