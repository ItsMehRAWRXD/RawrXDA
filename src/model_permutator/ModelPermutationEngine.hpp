#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Represents a single configurable parameter in a model architecture.
struct ModelParameter {
    std::string name;
    std::vector<std::string> possible_values;
    std::string current_value;
};

// Represents a full model architecture permutation to be tested.
struct ModelPermutation {
    int id;
    std::vector<ModelParameter> parameters;
    std::string fingerprint; // A unique hash of the parameter combination.
};

// The results of a benchmark run for a specific permutation.
struct BenchmarkResult {
    int permutation_id;
    double tokens_per_second;
    double time_to_first_token_ms;
    double response_stream_start_ms;
    size_t peak_memory_usage_mb;
    size_t model_size_bytes;
    size_t prompt_length_chars;
    size_t prompt_length_tokens;
    size_t generated_tokens;
    bool correctness_passed;
    std::string notes;
    std::string fingerprint; // The fingerprint of the tested permutation.
};

class ModelPermutationEngine {
public:
    ModelPermutationEngine();

    // Generates a new, unique permutation to test.
    ModelPermutation generate_permutation();

    // Logs the results of a benchmark run to a file.
    void log_finding(const ModelPermutation& permutation, const BenchmarkResult& result);

private:
    void initialize_parameter_space();
    std::string calculate_fingerprint(const std::vector<ModelParameter>& params);

    std::vector<ModelParameter> parameter_space_;
    std::vector<std::string> tested_fingerprints_;
    int next_permutation_id_;
    std::string log_file_path_;
};
