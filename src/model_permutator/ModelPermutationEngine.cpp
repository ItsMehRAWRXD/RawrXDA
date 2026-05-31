#include "ModelPermutationEngine.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <functional> // for std::hash

ModelPermutationEngine::ModelPermutationEngine() : next_permutation_id_(0) {
    log_file_path_ = "d:\\model_permutation_findings.jsonl";
    initialize_parameter_space();
}

void ModelPermutationEngine::initialize_parameter_space() {
    // --- Define the architectural parameters to explore ---

    // Core architecture
    parameter_space_.push_back({"architecture", {"llama", "gemma", "phi"}, "llama"});

    // Layer configuration
    parameter_space_.push_back({"n_layers", {"12", "24", "32", "48", "80"}, "32"});
    parameter_space_.push_back({"n_heads", {"12", "16", "32"}, "32"});
    parameter_space_.push_back({"n_kv_heads", {"4", "8", "16", "32"}, "32"}); // For GQA/MQA
    parameter_space_.push_back({"hidden_dim_multiplier", {"1.0", "1.3", "2.0", "2.6"}, "2.0"}); // Feed-forward network size

    // Activation functions
    parameter_space_.push_back({"activation_function", {"silu", "gelu", "swiglu"}, "silu"});

    // Normalization
    parameter_space_.push_back({"normalization_type", {"rmsnorm", "layernorm"}, "rmsnorm"});

    // Positional embeddings
    parameter_space_.push_back({"positional_embedding", {"rope", "alibi", "none"}, "rope"});
    parameter_space_.push_back({"rope_theta", {"10000.0", "500000.0", "1000000.0"}, "10000.0"});

    // Quantization Strategies (can be combined)
    parameter_space_.push_back({"quantization", {"none", "q4_0", "q5_1", "braided_2bit", "braided_4bit"}, "none"});
    
    // Streaming and Caching
    parameter_space_.push_back({"streaming_strategy", {"none", "atc_lod1", "atc_lod3"}, "none"});
}

std::string ModelPermutationEngine::calculate_fingerprint(const std::vector<ModelParameter>& params) {
    std::stringstream ss;
    for (const auto& p : params) {
        ss << p.name << ":" << p.current_value << ";";
    }
    // Using std::hash to create a simple hash of the string representation.
    return std::to_string(std::hash<std::string>{}(ss.str()));
}

ModelPermutation ModelPermutationEngine::generate_permutation() {
    std::random_device rd;
    std::mt19937 gen(rd());

    ModelPermutation new_perm;
    new_perm.id = next_permutation_id_++;
    
    std::string fingerprint;
    do {
        new_perm.parameters.clear();
        for (const auto& param_template : parameter_space_) {
            std::uniform_int_distribution<> distrib(0, param_template.possible_values.size() - 1);
            ModelParameter new_param = param_template;
            new_param.current_value = new_param.possible_values[distrib(gen)];
            new_perm.parameters.push_back(new_param);
        }
        fingerprint = calculate_fingerprint(new_perm.parameters);
    } while (std::find(tested_fingerprints_.begin(), tested_fingerprints_.end(), fingerprint) != tested_fingerprints_.end());

    new_perm.fingerprint = fingerprint;
    tested_fingerprints_.push_back(fingerprint);

    return new_perm;
}

void ModelPermutationEngine::log_finding(const ModelPermutation& permutation, const BenchmarkResult& result) {
    nlohmann::json log_entry;
    log_entry["permutation_id"] = result.permutation_id;
    log_entry["fingerprint"] = permutation.fingerprint;
    log_entry["tokens_per_second"] = result.tokens_per_second;
    log_entry["time_to_first_token_ms"] = result.time_to_first_token_ms;
    log_entry["response_stream_start_ms"] = result.response_stream_start_ms;
    log_entry["peak_memory_usage_mb"] = result.peak_memory_usage_mb;
    log_entry["model_size_bytes"] = result.model_size_bytes;
    log_entry["prompt_length_chars"] = result.prompt_length_chars;
    log_entry["prompt_length_tokens"] = result.prompt_length_tokens;
    log_entry["generated_tokens"] = result.generated_tokens;
    log_entry["correctness_passed"] = result.correctness_passed;
    log_entry["notes"] = result.notes;

    nlohmann::json params_json;
    for (const auto& p : permutation.parameters) {
        params_json[p.name] = p.current_value;
    }
    log_entry["parameters"] = params_json;
    
    std::ofstream log_file(log_file_path_, std::ios::app);
    if (log_file.is_open()) {
        log_file << log_entry.dump(4) << std::endl; // Pretty print with indent 4
    } else {
        std::cerr << "Error: Could not open log file: " << log_file_path_ << std::endl;
    }
}
