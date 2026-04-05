#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include "qtapp/inference_engine.hpp"
#include "qtapp/gguf.h"

// Minimal Qt setup to use InferenceEngine outside GUI
#include <QCoreApplication>
#include <QString>
#include <QDebug>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model.gguf> <prompt> [max_tokens]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string prompt_text = argv[2];
    int max_tokens = (argc > 3) ? std::stoi(argv[3]) : 48;

    std::cout << "=== InferenceEngine Forensic Probe ===" << std::endl;
    std::cout << "Model: " << model_path << std::endl;
    std::cout << "Prompt: " << prompt_text << std::endl;
    std::cout << "Max Tokens: " << max_tokens << std::endl;
    std::cout << std::endl;

    // Create InferenceEngine and load model
    InferenceEngine engine(QString::fromStdString(model_path));
    
    std::cout << "[INFO] Loading model..." << std::endl;
    auto load_start = std::chrono::high_resolution_clock::now();
    
    int load_status = engine.loadModel();
    
    auto load_end = std::chrono::high_resolution_clock::now();
    auto load_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
    
    std::cout << "[INFO] Model loaded in " << std::fixed << std::setprecision(2) << load_ms << " ms" << std::endl;
    
    if (load_status != 0) {
        std::cerr << "[ERROR] Failed to load model" << std::endl;
        return 1;
    }

    // Tokenize prompt manually (simplified)
    std::vector<int32_t> prompt_tokens;
    // For now, just use ASCII codes as placeholder
    for (char c : prompt_text) {
        prompt_tokens.push_back(static_cast<int32_t>(c));
    }

    std::cout << "[INFO] Prompt tokens: " << prompt_tokens.size() << std::endl;

    // Run inference with forensic logging (high-resolution logging is in inference_engine.cpp now)
    std::cout << "[INFO] Starting inference with forensic timing..." << std::endl;
    std::cout << "(Check debug output for [FORENSIC] timing logs)" << std::endl;
    std::cout << std::endl;

    auto inference_start = std::chrono::high_resolution_clock::now();
    
    std::vector<int32_t> result = engine.generate(prompt_tokens, max_tokens);
    
    auto inference_end = std::chrono::high_resolution_clock::now();
    auto inference_ms = std::chrono::duration<double, std::milli>(inference_end - inference_start).count();

    std::cout << std::endl;
    std::cout << "[RESULT] Generated " << (result.size() - prompt_tokens.size()) << " tokens" << std::endl;
    std::cout << "[RESULT] Total inference time: " << std::fixed << std::setprecision(2) << inference_ms << " ms" << std::endl;
    
    if ((result.size() - prompt_tokens.size()) > 0) {
        double tps = (result.size() - prompt_tokens.size()) / (inference_ms / 1000.0);
        std::cout << "[RESULT] Throughput: " << std::fixed << std::setprecision(4) << tps << " tokens/sec" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "(Detailed forensic logs are in debug output above)" << std::endl;

    return 0;
}
