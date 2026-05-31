#include "model_idea_generator.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <future>
#include <iomanip>

void print_config(const ModelParams::ModelConfiguration& config) {
    for (const auto& pair : config) {
        std::cout << "  " << std::setw(25) << std::left << pair.first << ": ";
        std::visit([](const auto& value) {
            std::cout << value;
        }, pair.second);
        std::cout << std::endl;
    }
}

void print_result(const ModelEvaluationResult& result) {
    std::cout << "  TPS: " << result.tps << ", TTFT: " << result.ttft
              << ", Mem (GB): " << result.memory_usage_gb
              << ", Quality: " << result.quality_metric
              << ", Build OK: " << (result.build_success ? "Yes" : "No") << std::endl;
}

int main() {
    ModelIdeaGenerator generator;
    std::vector<std::pair<ModelParams::ModelConfiguration, ModelEvaluationResult>> results;
    const int num_ideas_to_generate = 100;
    const int num_concurrent_evaluations = std::thread::hardware_concurrency();

    std::cout << "Starting model idea generation and evaluation..." << std::endl;
    std::cout << "Generating " << num_ideas_to_generate << " ideas with " << num_concurrent_evaluations << " concurrent evaluations." << std::endl;

    for (int i = 0; i < num_ideas_to_generate; ++i) {
        auto idea = generator.generate_idea();
        auto result = generator.evaluate_idea(idea);
        results.emplace_back(idea, result);

        std::cout << "\n--- Idea " << i + 1 << " ---" << std::endl;
        print_config(idea);
        std::cout << "--- Result ---" << std::endl;
        print_result(result);
    }

    // Sort results by quality metric
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.second.quality_metric > b.second.quality_metric;
    });

    std::cout << "\n\n--- Top 5 Model Ideas by Quality ---" << std::endl;
    for (int i = 0; i < std::min(5, (int)results.size()); ++i) {
        if (results[i].second.build_success) {
            std::cout << "\n--- Rank " << i + 1 << " ---" << std::endl;
            print_config(results[i].first);
            print_result(results[i].second);
        }
    }

    return 0;
}
