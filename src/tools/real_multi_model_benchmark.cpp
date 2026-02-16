#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include "../inference/InferenceEngine.hpp"

#include "logging/logger.h"
static Logger s_logger("real_multi_model_benchmark");

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
    s_logger.info("\n");
    s_logger.info("╔═══════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║   REAL MULTI-MODEL GPU BENCHMARK - ACTUAL INFERENCE TEST     ║\n");
    s_logger.info("║         Testing All GGUF Models with Real Loading            ║\n");
    s_logger.info("╚═══════════════════════════════════════════════════════════════╝\n\n");
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
        s_logger.error( "Error scanning directory: " << e.what() << "\n";
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
    
    s_logger.info("\n╔═══════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║ Model: ");
    s_logger.info("║ Size:  ");
    s_logger.info("╚═══════════════════════════════════════════════════════════════╝\n");
    
    try {
        // Initialize InferenceEngine
        InferenceEngine engine;
        
        s_logger.info("Loading model...");
        auto load_start = std::chrono::high_resolution_clock::now();
        
        bool loaded = engine.loadModel(model_path);
        
        auto load_end = std::chrono::high_resolution_clock::now();
        double load_time_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();
        
        if (!loaded) {
            result.error = "Failed to load model";
            s_logger.info(" FAILED\n");
            s_logger.info("Error: ");
            return result;
        }
        
        s_logger.info(" OK (");
        
        // Prepare prompt
        std::string prompt = "Write a short story about artificial intelligence:";
        s_logger.info("Generating ");
        
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
        s_logger.info("\n✓ RESULTS:\n");
        s_logger.info("  Total Time:      ");
        s_logger.info("  Tokens/Sec:      ");
        s_logger.info("  Avg Latency:     ");
        s_logger.info("  Output Length:   ");
        
        // Unload model
        engine.unloadModel();
        
    } catch (const std::exception& e) {
        result.error = std::string("Exception: ") + e.what();
        s_logger.info("\n✗ ERROR: ");
    }
    
    return result;
}

void printSummary(const std::vector<ModelBenchmarkResult>& results) {
    s_logger.info("\n╔═══════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║                  BENCHMARK SUMMARY                            ║\n");
    s_logger.info("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    s_logger.info( std::left << std::setw(40) << "Model"
              << std::setw(10) << "Size (GB)"
              << std::setw(12) << "TPS"
              << std::setw(15) << "Latency (ms)"
              << std::setw(10) << "Status" << "\n";
    s_logger.info( std::string(90, '─') << "\n";
    
    for (const auto& result : results) {
        s_logger.info( std::left << std::setw(40) << result.model_name.substr(0, 38)
                  << std::setw(10) << result.file_size_gb
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << (result.success ? result.tokens_per_sec : 0.0)
                  << std::setw(15) << std::fixed << std::setprecision(2) 
                  << (result.success ? result.avg_latency_ms : 0.0)
                  << std::setw(10) << (result.success ? "✓" : "✗") << "\n";
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
        
        s_logger.info("\n");
        s_logger.info("Successful Models: ");
        s_logger.info("Average TPS:       ");
        s_logger.info("Max TPS:           ");
        s_logger.info("Min TPS:           ");
    }
    
    s_logger.info("\n");
}

void exportCSV(const std::vector<ModelBenchmarkResult>& results, const std::string& filename) {
    std::ofstream csv(filename);
    
    if (!csv.is_open()) {
        s_logger.error( "Failed to open CSV file: " << filename << "\n";
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
    s_logger.info("✓ Results exported to: ");
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
    
    s_logger.info("Models Directory: ");
    s_logger.info("Tokens Per Test:  ");
    s_logger.info("\n");
    
    // Discover models
    s_logger.info("Discovering GGUF models...\n");
    std::vector<std::string> model_paths = discoverGGUFModels(models_dir);
    
    if (model_paths.empty()) {
        s_logger.error( "No GGUF models found in " << models_dir << "\n";
        return 1;
    }
    
    s_logger.info("Found ");
    
    // Benchmark each model
    std::vector<ModelBenchmarkResult> results;
    
    for (size_t i = 0; i < model_paths.size(); i++) {
        s_logger.info("\n[");
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
    
    s_logger.info("\n✓ ALL BENCHMARKS COMPLETE\n\n");
    
    return 0;
}
