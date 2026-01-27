#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fstream>
#include <QCoreApplication>
#include <QString>
#include "inference_engine.hpp"

// Forward declaration to test if linking works
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "Starting...\n" << std::flush;
    
    // Initialize Qt application
    QCoreApplication app(argc, argv);
    
    std::cout << "Qt initialized\n" << std::flush;
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║   REAL GPU INFERENCE TEST - Loading Actual Models      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    std::string models_dir = "D:\\OllamaModels";
    int num_tokens = 64;
    
    if (argc > 1) {
        models_dir = argv[1];
    }
    if (argc > 2) {
        num_tokens = std::atoi(argv[2]);
    }
    
    std::cout << "Models Directory: " << models_dir << "\n";
    std::cout << "Tokens Per Model: " << num_tokens << "\n\n";
    
    try {
        std::vector<std::string> models;
        for (const auto& entry : fs::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                models.push_back(entry.path().string());
            }
        }
        
        std::sort(models.begin(), models.end(), [](const std::string& a, const std::string& b) {
            return fs::file_size(a) > fs::file_size(b);
        });
        
        std::cout << "Found " << models.size() << " GGUF models\n\n";
        
        // Open CSV for results
        std::ofstream csv("D:\\temp\\RawrXD-q8-wire\\test_results\\REAL_GPU_BENCHMARK_RESULTS.csv");
        csv << "model,file_size_gb,tokens,load_time_sec,gen_time_ms,tps,latency_ms,success\n";
        
        // Test each model with REAL inference
        for (size_t i = 0; i < models.size(); i++) {
            auto path = fs::path(models[i]);
            double size_gb = fs::file_size(models[i]) / (1024.0 * 1024 * 1024);
            
            std::cout << "\n[" << (i+1) << "/" << models.size() << "] " << path.filename().string() << " (" << size_gb << " GB)\n";
            std::cout << "────────────────────────────────────────\n";
            
            try {
                // Create inference engine
                InferenceEngine engine;
                
                std::cout << "  Loading model..." << std::flush;
                auto load_start = std::chrono::high_resolution_clock::now();
                
                bool loaded = engine.loadModel(QString::fromStdString(models[i]));
                
                auto load_end = std::chrono::high_resolution_clock::now();
                double load_time = std::chrono::duration<double>(load_end - load_start).count();
                
                if (!loaded) {
                    std::cout << " FAILED\n";
                    csv << path.stem().string() << "," << size_gb << "," << num_tokens << ",0,0,0,0,false\n";
                    continue;
                }
                
                std::cout << " OK (" << load_time << "s)\n";
                
                // Generate tokens
                QString prompt = "Write a short story about AI:";
                std::vector<int32_t> tokens = engine.tokenize(prompt);
                
                std::cout << "  Generating " << num_tokens << " tokens..." << std::flush;
                
                auto gen_start = std::chrono::high_resolution_clock::now();
                std::vector<int32_t> output = engine.generate(tokens, num_tokens);
                auto gen_end = std::chrono::high_resolution_clock::now();
                
                double gen_time_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
                double tps = (num_tokens * 1000.0) / gen_time_ms;
                double latency_ms = gen_time_ms / num_tokens;
                
                std::cout << " OK\n";
                std::cout << "  ✓ TPS: " << tps << "\n";
                std::cout << "  ✓ Latency: " << latency_ms << " ms/token\n";
                std::cout << "  ✓ Total Time: " << gen_time_ms << " ms\n";
                std::cout << "  ✓ Output Tokens: " << output.size() << "\n";
                
                // Write to CSV
                csv << path.stem().string() << "," << size_gb << "," << num_tokens << ","
                    << load_time << "," << gen_time_ms << "," << tps << "," << latency_ms << ",true\n";
                csv.flush();
                
                engine.unloadModel();
                
                // Brief pause
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
            } catch (const std::exception& e) {
                std::cout << "\n  ✗ ERROR: " << e.what() << "\n";
                csv << path.stem().string() << "," << size_gb << "," << num_tokens << ",0,0,0,0,false\n";
            }
        }
        
        csv.close();
        std::cout << "\n\n✓ ALL BENCHMARKS COMPLETE\n";
        std::cout << "✓ Results saved to: D:\\temp\\RawrXD-q8-wire\\test_results\\REAL_GPU_BENCHMARK_RESULTS.csv\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
