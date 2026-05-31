#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <map>
#include <variant>

// Forward declaration for the evaluation result
struct ModelEvaluationResult;

// Define the parameter space for the model generator
namespace ModelParams {
    // Using std::variant to hold different types of parameters
    using ParameterValue = std::variant<int, float, std::string>;

    // A map to store the configuration of a model
    using ModelConfiguration = std::map<std::string, ParameterValue>;

    // Define the possible values for each parameter
    const std::map<std::string, std::vector<ParameterValue>> PARAMETER_SPACE = {
        {"num_layers", {12, 24, 32, 48, 64}},
        {"hidden_size", {768, 1024, 2048, 4096}},
        {"num_attention_heads", {8, 12, 16, 32}},
        {"activation_function", {"relu", "gelu", "swiglu"}},
        {"quantization_bits", {2, 3, 4, 5, 6, 8}},
        {"quantization_group_size", {32, 64, 128, 256}},
        {"streaming_strategy", {"atc", "full_map", "none"}}
    };
} // namespace ModelParams

class ModelIdeaGenerator {
public:
    ModelIdeaGenerator() : rng_(std::random_device{}()) {}

    // Generate a new random model configuration
    ModelParams::ModelConfiguration generate_idea() {
        ModelParams::ModelConfiguration config;
        for (const auto& pair : ModelParams::PARAMETER_SPACE) {
            const auto& key = pair.first;
            const auto& values = pair.second;
            std::uniform_int_distribution<size_t> dist(0, values.size() - 1);
            config[key] = values[dist(rng_)];
        }
        return config;
    }

    // Placeholder for the evaluation function
    ModelEvaluationResult evaluate_idea(const ModelParams::ModelConfiguration& config) {
        // This will be implemented later.
        // For now, it returns a dummy result.
        return {};
    }

private:
    std::mt19937 rng_;
};

// A structure to hold the results of a model evaluation
struct ModelEvaluationResult {
    double tps = 0.0;
    double ttft = 0.0;
    size_t memory_usage_gb = 0;
    double quality_metric = 0.0; // e.g., perplexity or accuracy
    bool build_success = false;
};
