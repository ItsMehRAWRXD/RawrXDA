/**
 * Simple GPU Test Tool - Command-line GGUF Model Tester
 * Designed for hotpatch testing framework
 * 
 * Usage: simple_gpu_test.exe --model <path> --tokens <num> [--prompt <text>]
 */

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <cstring>
#include "../inference/InferenceEngine.hpp"

void printUsage(const char* prog_name) {
    std::cout << "Simple GPU Test - GGUF Model Inference Tool\n\n";
    std::cout << "Usage: " << prog_name << " --model <path> --tokens <num> [--prompt <text>]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --model <path>   Path to GGUF model file (required)\n";
    std::cout << "  --tokens <num>   Number of tokens to generate (required)\n";
    std::cout << "  --prompt <text>  Prompt text (default: 'Test')\n";
    std::cout << "  --help           Show this help message\n\n";
    std::cout << "Output Format (JSON):\n";
    std::cout << "  {\"success\": true, \"tokens_per_sec\": 75.3, \"total_time_ms\": 1698.2, \"tokens\": 128}\n";
}

struct TestConfig {
    std::string model_path;
    int num_tokens = 0;
    std::string prompt = "Test";
    bool valid = false;
};

TestConfig parseArgs(int argc, char* argv[]) {
    TestConfig config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return config;
        }
        else if (arg == "--model" && i + 1 < argc) {
            config.model_path = argv[++i];
        }
        else if (arg == "--tokens" && i + 1 < argc) {
            config.num_tokens = std::atoi(argv[++i]);
        }
        else if (arg == "--prompt" && i + 1 < argc) {
            config.prompt = argv[++i];
        }
    }
    
    // Validate
    config.valid = !config.model_path.empty() && config.num_tokens > 0;
    
    return config;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    TestConfig config = parseArgs(argc, argv);
    
    if (!config.valid) {
        printUsage(argv[0]);
        std::cerr << "\nError: Missing required arguments\n";
        return 1;
    }
    
    try {
        // Initialize engine
        InferenceEngine engine;
        
        // Load model
        auto load_start = std::chrono::high_resolution_clock::now();
        bool loaded = engine.loadModel(config.model_path);
        auto load_end = std::chrono::high_resolution_clock::now();
        
        if (!loaded) {
            std::cout << "{\"success\": false, \"error\": \"Failed to load model\"}\n";
            return 1;
        }
        
        double load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        // Run inference
        auto gen_start = std::chrono::high_resolution_clock::now();
        std::string output = engine.generate(config.prompt, config.num_tokens);
        auto gen_end = std::chrono::high_resolution_clock::now();
        
        double total_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
        double tokens_per_sec = (config.num_tokens * 1000.0) / total_time_ms;
        
        // Output JSON result
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "{"
                  << "\"success\": true, "
                  << "\"tokens_per_sec\": " << tokens_per_sec << ", "
                  << "\"total_time_ms\": " << total_time_ms << ", "
                  << "\"load_time_ms\": " << load_time_ms << ", "
                  << "\"tokens\": " << config.num_tokens << ", "
                  << "\"output_length\": " << output.length()
                  << "}\n";
        
        // Unload
        engine.unloadModel();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "{\"success\": false, \"error\": \"" << e.what() << "\"}\n";
        return 1;
    }
}
