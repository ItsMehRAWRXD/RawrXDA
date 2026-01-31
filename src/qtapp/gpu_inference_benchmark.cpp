#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <thread>
#include <filesystem>
#include <algorithm>

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


}

void printSystemInfo() {


    GPUBackend& gpu = GPUBackend::instance();
    bool gpu_init = gpu.initialize();


    if (gpu_init && gpu.isAvailable()) {


    } else {
        
    }


}

BenchmarkResult benchmarkRealModel(const std::string& model_path, int num_tokens = 128) {
    BenchmarkResult result;
    result.model_path = model_path;
    result.model_name = fs::path(model_path).stem().string();
    result.file_size_gb = fs::file_size(model_path) / (1024ULL * 1024 * 1024);
    result.total_tokens = num_tokens;
    result.success = false;


    try {
        InferenceEngine engine(std::string::fromStdString(model_path));


        auto load_start = std::chrono::high_resolution_clock::now();
        
        bool loaded = engine.loadModel(std::string::fromStdString(model_path));
        
        auto load_end = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        if (!loaded) {
            result.error = "Failed to load model";
            
            return result;
        }


        // Prepare input
        std::string prompt = "Write a short story about artificial intelligence:";
        std::vector<int32_t> tokens = engine.tokenize(prompt);


        // Run inference
        auto gen_start = std::chrono::high_resolution_clock::now();
        
        std::vector<int32_t> output = engine.generate(tokens, num_tokens);
        
        auto gen_end = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
        
        // Calculate metrics
        result.tokens_per_sec = (num_tokens * 1000.0) / result.total_time_ms;
        result.avg_latency_ms = result.total_time_ms / num_tokens;
        result.success = true;


        engine.unloadModel();
        
    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
        
    }
    
    return result;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    printHeader();
    printSystemInfo();
    
    // Configuration
    std::string arg1 = argc > 1 ? argv[1] : std::string("D:\\OllamaModels");
    int tokens_per_model = argc > 2 ? std::atoi(argv[2]) : 128;


    // Build list of models: if arg1 is a .gguf file, benchmark that file directly
    std::vector<std::string> model_paths;
    try {
        fs::path p(arg1);
        if (fs::is_regular_file(p) && p.extension() == ".gguf") {
            model_paths.push_back(p.string());
        } else {
            // Treat as directory
            for (const auto& entry : fs::directory_iterator(p)) {
                if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                    model_paths.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        
        return 1;
    }
    
    // Sort by size (descending)
    std::sort(model_paths.begin(), model_paths.end(), [](const std::string& a, const std::string& b) {
        return fs::file_size(a) > fs::file_size(b);
    });


    if (model_paths.empty()) {
        
        return 1;
    }
    
    // Benchmark each model
    std::vector<BenchmarkResult> results;
    
    for (size_t i = 0; i < model_paths.size(); i++) {
        
        BenchmarkResult result = benchmarkRealModel(model_paths[i], tokens_per_model);
        results.push_back(result);
        
        // Brief pause between models
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Print summary


    for (const auto& r : results) {
        
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
        
    }


    return 0;
}

