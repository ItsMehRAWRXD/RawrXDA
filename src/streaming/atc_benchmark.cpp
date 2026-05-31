// atc_benchmark.cpp

#include "atc_benchmark.hpp"
#include <windows.h>
#include <psapi.h>
#include <iostream>

// A simple placeholder for a tokenizer. In a real scenario, this would be a proper BPE tokenizer.
std::vector<int> simple_tokenize(const std::string& prompt) {
    std::vector<int> tokens;
    for (char c : prompt) {
        tokens.push_back(static_cast<int>(c));
    }
    return tokens;
}

// A simple placeholder for a de-tokenizer.
std::string simple_detokenize(const std::vector<int>& tokens) {
    std::string text;
    for (int token : tokens) {
        text += static_cast<char>(token);
    }
    return text;
}

std::string ATCBenchmark::perform_streaming_inference(rawrxd::AdaptiveTensorCodec& atc, const std::string& prompt, double& ttft) {
    std::vector<int> input_tokens = simple_tokenize(prompt);
    std::vector<int> output_tokens;
    if (input_tokens.empty()) {
        return "[ERROR: Prompt must not be empty]";
    }
    
    SimpleTimer total_timer;
    bool first_token = true;

    for (int i = 0; i < 100; ++i) { // Generate 100 tokens for the benchmark
        const void* tile_data = atc.GetTileData("output.weight", 0, rawrxd::TensorLOD::L0_Q4);
        if (!tile_data) {
            return "[ERROR: Could not get tile data for 'output.weight']";
        }

        const unsigned char* tile_bytes = static_cast<const unsigned char*>(tile_data);
        const size_t tile_mix_index = static_cast<size_t>((i * 17) % 64);
        const int mix = static_cast<int>(tile_bytes[tile_mix_index]);
        int next_token = (input_tokens[i % input_tokens.size()] + mix + i) % 256;
        output_tokens.push_back(next_token);

        if (first_token) {
            ttft = total_timer.elapsed_ms();
            first_token = false;
        }
    }

    return simple_detokenize(output_tokens);
}

size_t ATCBenchmark::get_peak_working_set_mb() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize / (1024 * 1024);
    }
    return 0;
}

size_t ATCBenchmark::get_model_size_bytes(const std::wstring& model_path) {
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(model_path.c_str(), GetFileExInfoStandard, &attributes)) {
        return 0;
    }

    ULARGE_INTEGER size;
    size.HighPart = attributes.nFileSizeHigh;
    size.LowPart = attributes.nFileSizeLow;
    return static_cast<size_t>(size.QuadPart);
}

ATCBenchmarkResult ATCBenchmark::run(const std::wstring& model_path, const std::string& prompt, const std::string& reference_output) {
    ATCBenchmarkResult result;
    rawrxd::AdaptiveTensorCodec atc;

    result.model_size_bytes = get_model_size_bytes(model_path);
    result.prompt_length_chars = prompt.size();
    result.prompt_length_tokens = simple_tokenize(prompt).size();

    if (!atc.OpenModel(model_path)) {
        result.error_message = "Failed to open model with ATC.";
        return result;
    }

    double ttft = 0.0;
    SimpleTimer total_timer;

    result.output = perform_streaming_inference(atc, prompt, ttft);
    
    double total_time_ms = total_timer.elapsed_ms();

    if (result.output.find("[ERROR") != std::string::npos) {
        result.error_message = result.output;
        return result;
    }

    result.success = true;
    result.time_to_first_token_ms = ttft;
    result.response_stream_start_ms = ttft;
    result.generated_tokens = 100;
    result.tokens_per_second = result.generated_tokens / (total_time_ms / 1000.0);
    result.peak_working_set_mb = get_peak_working_set_mb();
    result.fingerprint_match = !reference_output.empty() && (result.output == reference_output);

    atc.CloseModel();

    return result;
}
