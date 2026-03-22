#include "inference_benchmark.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::cout << "RawrXD Inference Engine Benchmark Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    RawrXD::BenchmarkConfig config;
    if (argc > 1) {
        config.modelPaths.push_back(argv[1]);
    } else {
        config.modelPaths = {
            "F:\\OllamaModels\\Codestral-22B-v0.1-hf.Q4_K_S.gguf",
            "F:\\OllamaModels\\BigDaddyG-Q2_K-PRUNED-16GB.gguf"
        };
    }

    config.testPrompts = {
        "def fibonacci(n):\n    ",
        "class NeuralNetwork:\n    def __init__(self):\n        ",
        "#include <iostream>\nint main() {\n    std::cout << "
    };

    RawrXD::InferenceBenchmark benchmark;

    std::cout << "Running benchmark suite..." << std::endl;
    const auto results = benchmark.runBenchmarkSuite(config);

    std::cout << "\nGenerating comparison report..." << std::endl;
    const std::string report = benchmark.generateComparisonReport(results);
    std::cout << report << std::endl;

    std::ofstream reportFile("comprehensive_benchmark_report.txt");
    if (reportFile.is_open()) {
        reportFile << report;
        std::cout << "Report saved to comprehensive_benchmark_report.txt" << std::endl;
    }

    for (const auto& modelPath : config.modelPaths) {
        const auto recommended = benchmark.getRecommendedBackend(modelPath, results);
        const auto backendInfo = RawrXD::BackendSelector().getBackendInfo(recommended);
        std::cout << "Recommended backend for " << modelPath << ": "
                  << backendInfo.name << std::endl;
    }

    return 0;
}
