#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <QCoreApplication>
#include "gpu_backend.hpp"
#include "inference_engine.hpp"

namespace fs = std::filesystem;

struct BenchmarkResult {
    std::string model_path;
    std::string model_name;
    size_t file_size_gb;
    int total_tokens;
    double load_time_ms;
    double total_time_ms;
    double tokens_per_sec;
    double avg_latency_ms;
    bool success;
    std::string error;
};

void printHeader() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║   REAL GPU BENCHMARK - ACTUAL MODEL LOADING & INFERENCE║\n";
    std::cout << "║         AMD Radeon RX 7800 XT - Vulkan Backend         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
}

void printSystemInfo() {
    std::cout << "=== SYSTEM INFORMATION ===\n";
    
    GPUBackend& gpu = GPUBackend::instance();
    bool gpu_init = gpu.initialize();
    
    std::cout << "GPU Backend Initialized: " << (gpu_init ? "YES" : "NO") << "\n";
    
    if (gpu_init && gpu.isAvailable()) {
        std::cout << "GPU Device: " << gpu.deviceName().toStdString() << "\n";
        std::cout << "GPU Memory: " << (gpu.totalMemory() / (1024*1024)) << " MB\n";
        std::cout << "GPU Backend: " << gpu.backendName().toStdString() << "\n";
    } else {
        std::cout << "GPU Backend: CPU FALLBACK (no GPU acceleration)\n";
    }
    
    std::cout << "\n";
}

BenchmarkResult benchmarkRealModel(const std::string& model_path, int num_tokens = 128) {
    BenchmarkResult result;
    result.model_path = model_path;
    result.model_name = fs::path(model_path).stem().string();
    result.file_size_gb = fs::file_size(model_path) / (1024ULL * 1024 * 1024);
    result.total_tokens = num_tokens;
    result.success = false;
    
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Model: " << result.model_name << "\n";
    std::cout << "║ Size:  " << result.file_size_gb << " GB\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        InferenceEngine engine(QString::fromStdString(model_path));
        
        std::cout << "Loading model..." << std::flush;
        auto load_start = std::chrono::high_resolution_clock::now();
        
        bool loaded = engine.loadModel(QString::fromStdString(model_path));
        
        auto load_end = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        if (!loaded) {
            result.error = "Failed to load model";
            std::cout << " FAILED\n";
            return result;
        }
        
        std::cout << " OK (" << (result.load_time_ms / 1000.0) << " sec)\n";
        
        // Prepare input
        QString prompt = "Write a short story about artificial intelligence:";
        std::vector<int32_t> tokens = engine.tokenize(prompt);
        
        std::cout << "Generating " << num_tokens << " tokens..." << std::flush;
        
        // Run inference
        auto gen_start = std::chrono::high_resolution_clock::now();
        
        std::vector<int32_t> output = engine.generate(tokens, num_tokens);
        
        auto gen_end = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
        
        // Calculate metrics
        result.tokens_per_sec = (num_tokens * 1000.0) / result.total_time_ms;
        result.avg_latency_ms = result.total_time_ms / num_tokens;
        result.success = true;
        
        std::cout << " OK\n";
        std::cout << "\n✓ RESULTS:\n";
        std::cout << "  Load Time:       " << std::fixed << std::setprecision(2) << (result.load_time_ms / 1000.0) << " sec\n";
        std::cout << "  Generation Time: " << std::fixed << std::setprecision(2) << result.total_time_ms << " ms\n";
        std::cout << "  Tokens/Sec:      " << std::fixed << std::setprecision(2) << result.tokens_per_sec << " TPS\n";
        std::cout << "  Avg Latency:     " << std::fixed << std::setprecision(2) << result.avg_latency_ms << " ms/token\n";
        std::cout << "  Output Tokens:   " << output.size() << "\n";
        
        engine.unloadModel();
        
    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
        std::cout << "\n✗ ERROR: " << result.error << "\n";
    }
    
    return result;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    printHeader();
    printSystemInfo();
    
    // Configuration
    std::string models_dir = "D:\\OllamaModels";
    int tokens_per_model = 128;
    
    // Parse command line
    if (argc > 1) {
        models_dir = argv[1];
    }
    if (argc > 2) {
        tokens_per_model = std::atoi(argv[2]);
    }
    
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     REAL GPU BENCHMARK - ACTUAL MODEL LOADING          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Models Directory: " << models_dir << "\n";
    std::cout << "Tokens Per Test:  " << tokens_per_model << "\n\n";
    
    // Discover GGUF models
    std::vector<std::string> model_paths;
    try {
        for (const auto& entry : fs::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                model_paths.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << "\n";
        return 1;
    }
    
    // Sort by size (descending)
    std::sort(model_paths.begin(), model_paths.end(), [](const std::string& a, const std::string& b) {
        return fs::file_size(a) > fs::file_size(b);
    });
    
    std::cout << "Found " << model_paths.size() << " GGUF models\n";
    
    if (model_paths.empty()) {
        std::cerr << "No GGUF models found!\n";
        return 1;
    }
    
    // Benchmark each model
    std::vector<BenchmarkResult> results;
    
    for (size_t i = 0; i < model_paths.size(); i++) {
        std::cout << "\n[" << (i+1) << "/" << model_paths.size() << "] ";
        BenchmarkResult result = benchmarkRealModel(model_paths[i], tokens_per_model);
        results.push_back(result);
        
        // Brief pause between models
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Print summary
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║                  BENCHMARK SUMMARY                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << std::left << std::setw(35) << "Model"
              << std::setw(10) << "Size (GB)"
              << std::setw(12) << "TPS"
              << std::setw(12) << "Latency"
              << std::setw(10) << "Status" << "\n";
    std::cout << std::string(80, '─') << "\n";
    
    for (const auto& r : results) {
        std::cout << std::left << std::setw(35) << r.model_name.substr(0, 33)
                  << std::setw(10) << r.file_size_gb
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << (r.success ? r.tokens_per_sec : 0.0)
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << (r.success ? r.avg_latency_ms : 0.0)
                  << std::setw(10) << (r.success ? "✓" : "✗") << "\n";
    }
    
    // Export CSV
    std::string csv_path = "D:\\temp\\RawrXD-q8-wire\\test_results\\REAL_GPU_BENCHMARK_RESULTS.csv";
    std::ofstream csv(csv_path);
    
    if (csv.is_open()) {
        csv << "model,file_size_gb,tokens,load_time_ms,gen_time_ms,tps,latency_ms,success,error\n";
        for (const auto& r : results) {
            csv << r.model_name << "," << r.file_size_gb << "," << r.total_tokens << ","
                << std::fixed << std::setprecision(3) << r.load_time_ms << ","
                << r.total_time_ms << "," << r.tokens_per_sec << "," << r.avg_latency_ms << ","
                << (r.success ? "true" : "false") << "," << r.error << "\n";
        }
        csv.close();
        std::cout << "\n✓ Results exported to: " << csv_path << "\n";
    }
    
    std::cout << "\n✓ ALL BENCHMARKS COMPLETE\n\n";
    
    return 0;
}
