#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include "../inference/InferenceEngine.hpp"

namespace fs = std::filesystem;

struct ModelBenchmarkResult {
    std::string model_path;
    std::string model_name;
    size_t file_size_gb;
    int tokens_generated;
    double total_time_ms;
    double tokens_per_sec;
    double avg_latency_ms;
    bool success;
    std::string error;
};

void printHeader() {


}

std::vector<std::string> discoverGGUFModels(const std::string& models_dir) {
    std::vector<std::string> models;
    
    try {
        for (const auto& entry : fs::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                models.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
        
    }
    
    // Sort by file size (descending)
    std::sort(models.begin(), models.end(), [](const std::string& a, const std::string& b) {
        return fs::file_size(a) > fs::file_size(b);
    });
    
    return models;
}

ModelBenchmarkResult benchmarkModel(const std::string& model_path, int num_tokens = 128) {
    ModelBenchmarkResult result;
    result.model_path = model_path;
    result.model_name = fs::path(model_path).stem().string();
    result.file_size_gb = fs::file_size(model_path) / (1024ULL * 1024 * 1024);
    result.tokens_generated = num_tokens;
    result.success = false;


    try {
        // Initialize InferenceEngine
        InferenceEngine engine;


        auto load_start = std::chrono::high_resolution_clock::now();
        
        bool loaded = engine.loadModel(model_path);
        
        auto load_end = std::chrono::high_resolution_clock::now();
        double load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        if (!loaded) {
            result.error = "Failed to load model";


            return result;
        }


        // Prepare prompt
        std::string prompt = "Write a short story about artificial intelligence:";


        // Run inference and measure time
        auto gen_start = std::chrono::high_resolution_clock::now();
        
        std::string output = engine.generate(prompt, num_tokens);
        
        auto gen_end = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
        
        // Calculate metrics
        result.tokens_per_sec = (num_tokens * 1000.0) / result.total_time_ms;
        result.avg_latency_ms = result.total_time_ms / num_tokens;
        result.success = true;
        
        // Print results


        // Unload model
        engine.unloadModel();
        
    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
        
    }
    
    return result;
}

void printSummary(const std::vector<ModelBenchmarkResult>& results) {


    for (const auto& result : results) {
        
    }
    
    // Calculate statistics
    std::vector<double> tps_values;
    for (const auto& r : results) {
        if (r.success) tps_values.push_back(r.tokens_per_sec);
    }
    
    if (!tps_values.empty()) {
        double avg_tps = 0.0;
        for (double tps : tps_values) avg_tps += tps;
        avg_tps /= tps_values.size();
        
        double max_tps = *std::max_element(tps_values.begin(), tps_values.end());
        double min_tps = *std::min_element(tps_values.begin(), tps_values.end());


    }


}

void exportCSV(const std::vector<ModelBenchmarkResult>& results, const std::string& filename) {
    std::ofstream csv(filename);
    
    if (!csv.is_open()) {
        
        return;
    }
    
    // Header
    csv << "model,file_size_gb,tokens_generated,total_time_ms,tokens_per_sec,avg_latency_ms,success,error\n";
    
    // Data
    for (const auto& r : results) {
        csv << r.model_name << ","
            << r.file_size_gb << ","
            << r.tokens_generated << ","
            << std::fixed << std::setprecision(3) << r.total_time_ms << ","
            << std::fixed << std::setprecision(3) << r.tokens_per_sec << ","
            << std::fixed << std::setprecision(3) << r.avg_latency_ms << ","
            << (r.success ? "true" : "false") << ","
            << r.error << "\n";
    }
    
    csv.close();
    
}

int main(int argc, char* argv[]) {
    printHeader();
    
    // Configuration
    std::string models_dir = "D:\\OllamaModels";
    int tokens_per_model = 128;  // Generate 128 tokens per model
    
    // Parse command line args
    if (argc > 1) {
        models_dir = argv[1];
    }
    if (argc > 2) {
        tokens_per_model = std::atoi(argv[2]);
    }


    // Discover models
    
    std::vector<std::string> model_paths = discoverGGUFModels(models_dir);
    
    if (model_paths.empty()) {
        
        return 1;
    }


    // Benchmark each model
    std::vector<ModelBenchmarkResult> results;
    
    for (size_t i = 0; i < model_paths.size(); i++) {
        
        ModelBenchmarkResult result = benchmarkModel(model_paths[i], tokens_per_model);
        results.push_back(result);
        
        // Brief pause between models
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Print summary
    printSummary(results);
    
    // Export to CSV
    std::string csv_path = "D:\\temp\\RawrXD-q8-wire\\test_results\\REAL_GPU_BENCHMARK_RESULTS.csv";
    exportCSV(results, csv_path);


    return 0;
}
