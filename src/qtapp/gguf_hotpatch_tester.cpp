/**
 * GGUF Hotpatch Tester - Command-line tool for REAL hotpatch testing
 * NO SIMULATIONS - Actually loads models and runs GPU inference
 * 
 * Usage: gguf_hotpatch_tester.exe --model <path> --tokens <num> [--prompt <text>]
 * Output: JSON format for PowerShell parsing
 */

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <QCoreApplication>
#include <QString>
#include "inference_engine.hpp"
#include "gpu_backend.hpp"

struct TestResult {
    bool success = false;
    double tokens_per_sec = 0.0;
    double total_time_ms = 0.0;
    double load_time_ms = 0.0;
    int tokens_generated = 0;
    int output_length = 0;
    std::string error;
    std::string gpu_backend;
    bool gpu_enabled = false;
};

void printUsage(const char* prog_name) {
    std::cout << "GGUF Hotpatch Tester - REAL Model Inference (NO SIMULATIONS)\n\n";
    std::cout << "Usage: " << prog_name << " --model <path> --tokens <num> [--prompt <text>]\n\n";
    std::cout << "Required:\n";
    std::cout << "  --model <path>   Path to GGUF model file\n";
    std::cout << "  --tokens <num>   Number of tokens to generate\n\n";
    std::cout << "Optional:\n";
    std::cout << "  --prompt <text>  Prompt text (default: 'Test')\n\n";
    std::cout << "Output: JSON to stdout, logs to stderr\n";
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
    
    config.valid = !config.model_path.empty() && config.num_tokens > 0;
    return config;
}

TestResult runRealInference(const TestConfig& config) {
    TestResult result;
    
    try {
        std::cerr << "[REAL TEST] Starting ACTUAL GPU inference (no simulation)\n";
        
        // Initialize GPU backend
        std::cerr << "[REAL TEST] Initializing GPU backend...\n";
        GPUBackend& gpu = GPUBackend::instance();
        bool gpu_init = gpu.initialize();
        
        result.gpu_enabled = gpu_init && gpu.isAvailable();
        result.gpu_backend = gpu.backendName().toStdString();
        std::cerr << "[REAL TEST] GPU: " << (result.gpu_enabled ? "ENABLED" : "DISABLED") 
                  << " (" << result.gpu_backend << ")\n";
        
        // Create inference engine (like minimal_qt_test does)
        std::cerr << "[REAL TEST] Creating InferenceEngine...\n";
        InferenceEngine engine;
        
        // Load model - THIS IS REAL
        std::cerr << "[REAL TEST] Loading GGUF model: " << config.model_path << "\n";
        auto load_start = std::chrono::high_resolution_clock::now();
        
        bool loaded = engine.loadModel(QString::fromStdString(config.model_path));
        
        auto load_end = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        std::cerr << "[REAL TEST] Model loaded: " << (loaded ? "SUCCESS" : "FAILED") 
                  << " (took " << result.load_time_ms << " ms)\n";
        
        if (!loaded) {
            result.error = "Failed to load GGUF model";
            return result;
        }
        
        if (!engine.isModelLoaded()) {
            result.error = "Model reports not loaded after load";
            return result;
        }
        
        // Tokenize prompt - THIS IS REAL
        std::cerr << "[REAL TEST] Tokenizing prompt: \"" << config.prompt << "\"\n";
        std::vector<int32_t> input_tokens = engine.tokenize(QString::fromStdString(config.prompt));
        std::cerr << "[REAL TEST] Prompt tokenized to " << input_tokens.size() << " tokens\n";
        
        // Generate tokens - THIS IS REAL GPU INFERENCE
        std::cerr << "[REAL TEST] Running REAL inference for " << config.num_tokens << " tokens...\n";
        auto gen_start = std::chrono::high_resolution_clock::now();
        
        std::vector<int32_t> output_tokens = engine.generate(input_tokens, config.num_tokens);
        
        auto gen_end = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
        result.tokens_generated = output_tokens.size();
        
        std::cerr << "[REAL TEST] Generated " << result.tokens_generated << " tokens in " 
                  << result.total_time_ms << " ms\n";
        
        // Detokenize to verify output is real
        QString output_text = engine.detokenize(output_tokens);
        result.output_length = output_text.length();
        std::cerr << "[REAL TEST] Output text: " << result.output_length << " characters\n";
        std::cerr << "[REAL TEST] First 50 chars: " << output_text.left(50).toStdString() << "...\n";
        
        // Calculate metrics from REAL inference
        if (result.total_time_ms > 0) {
            result.tokens_per_sec = (result.tokens_generated * 1000.0) / result.total_time_ms;
        }
        
        result.success = true;
        std::cerr << "[REAL TEST] ✓ REAL inference completed successfully!\n";
        std::cerr << "[REAL TEST] ✓ TPS: " << result.tokens_per_sec << "\n";
        
    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
        std::cerr << "[ERROR] Exception during REAL inference: " << e.what() << "\n";
    } catch (...) {
        result.error = "Unknown exception during REAL inference";
        std::cerr << "[ERROR] Unknown exception during REAL inference\n";
    }
    
    return result;
}

void printJSON(const TestResult& result) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "{\n";
    std::cout << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    std::cout << "  \"tokens_per_sec\": " << result.tokens_per_sec << ",\n";
    std::cout << "  \"total_time_ms\": " << result.total_time_ms << ",\n";
    std::cout << "  \"load_time_ms\": " << result.load_time_ms << ",\n";
    std::cout << "  \"tokens_generated\": " << result.tokens_generated << ",\n";
    std::cout << "  \"output_length\": " << result.output_length << ",\n";
    std::cout << "  \"gpu_enabled\": " << (result.gpu_enabled ? "true" : "false") << ",\n";
    std::cout << "  \"gpu_backend\": \"" << result.gpu_backend << "\"";
    
    if (!result.error.empty()) {
        std::cout << ",\n  \"error\": \"" << result.error << "\"";
    }
    
    std::cout << "\n}\n";
}

int main(int argc, char* argv[]) {
    std::cerr << "[STARTUP] GGUF Hotpatch Tester - REAL INFERENCE MODE\n";
    
    QCoreApplication app(argc, argv);
    
    TestConfig config = parseArgs(argc, argv);
    
    if (!config.valid) {
        printUsage(argv[0]);
        if (argc > 1) {
            std::cerr << "\nError: Missing or invalid arguments\n";
        }
        return 1;
    }
    
    std::cerr << "[CONFIG] Model: " << config.model_path << "\n";
    std::cerr << "[CONFIG] Tokens: " << config.num_tokens << "\n";
    std::cerr << "[CONFIG] Prompt: \"" << config.prompt << "\"\n\n";
    
    // Run REAL inference (no simulation!)
    TestResult result = runRealInference(config);
    
    // Output JSON to stdout
    printJSON(result);
    
    return result.success ? 0 : 1;
}
