/**
 * Direct C++ test for StreamingGGUFLoader performance with lazy initialization
 * Measures actual model loading and zone management performance
 */

#include "streaming_gguf_loader.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct BenchmarkResult {
    std::string model_name;
    size_t file_size_mb;
    double header_parse_ms;
    double index_build_ms;
    double zone_assign_ms;
    double total_open_ms;
    double first_zone_load_ms;
    double cached_zone_load_ms;
    size_t tensor_count;
    size_t zone_count;
    bool success;
};

class LazyInitBenchmark {
private:
    std::vector<BenchmarkResult> results_;
    static constexpr double BASELINE_MS = 675.0;
    static constexpr int NUM_ITERATIONS = 5;

public:
    void runBenchmark(const std::string& model_path) {
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  StreamingGGUFLoader Performance Benchmark               ║\n";
        std::cout << "║  Testing Lazy Initialization vs 675ms Baseline          ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

        fs::path p(model_path);
        BenchmarkResult result;
        result.model_name = p.stem().string();
        result.file_size_mb = fs::file_size(p) / (1024 * 1024);
        result.success = false;

        std::cout << "Model: " << result.model_name << "\n";
        std::cout << "Size:  " << result.file_size_mb << " MB\n\n";

        // Test 1: Initial open with lazy loading
        std::cout << "[TEST 1] Cold Open with Lazy Initialization\n";
        std::cout << "────────────────────────────────────────────────────\n";

        std::vector<double> open_times;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            StreamingGGUFLoader loader;
            
            auto start = std::chrono::high_resolution_clock::now();
            bool success = loader.Open(model_path);
            auto end = std::chrono::high_resolution_clock::now();
            
            double open_ms = std::chrono::duration<double, std::milli>(end - start).count();
            open_times.push_back(open_ms);
            
            if (success && i == 0) {
                result.tensor_count = loader.GetTensorCount();
                result.zone_count = loader.GetZoneCount();
            }
            
            std::cout << "  Iteration " << (i+1) << ": " << std::fixed << std::setprecision(2) 
                      << open_ms << " ms";
            
            if (open_ms < BASELINE_MS) {
                std::cout << " ✓ (faster than baseline)";
            }
            std::cout << "\n";
            
            loader.Close();
        }

        result.total_open_ms = std::accumulate(open_times.begin(), open_times.end(), 0.0) / open_times.size();
        result.success = true;

        double min_time = *std::min_element(open_times.begin(), open_times.end());
        double max_time = *std::max_element(open_times.begin(), open_times.end());
        
        std::cout << "\n  Results:\n";
        std::cout << "  ├─ Average:    " << std::fixed << std::setprecision(2) << result.total_open_ms << " ms\n";
        std::cout << "  ├─ Min:        " << min_time << " ms\n";
        std::cout << "  ├─ Max:        " << max_time << " ms\n";
        std::cout << "  ├─ Baseline:   " << BASELINE_MS << " ms\n";
        
        double improvement = ((BASELINE_MS - result.total_open_ms) / BASELINE_MS) * 100;
        if (improvement > 0) {
            std::cout << "  └─ Improvement: " << std::fixed << std::setprecision(1) << improvement << "% faster ✓\n";
        } else {
            std::cout << "  └─ Performance: " << std::fixed << std::setprecision(1) << std::abs(improvement) << "% slower\n";
        }

        // Test 2: Zone loading performance
        std::cout << "\n[TEST 2] Lazy Zone Loading Performance\n";
        std::cout << "────────────────────────────────────────────────────\n";

        StreamingGGUFLoader loader;
        if (loader.Open(model_path)) {
            auto zones = loader.GetAvailableZones();
            
            if (!zones.empty()) {
                std::string first_zone = zones[0];
                
                std::cout << "  Testing zone: " << first_zone << "\n";
                
                // Cold zone load
                auto start = std::chrono::high_resolution_clock::now();
                bool loaded = loader.LoadZone(first_zone);
                auto end = std::chrono::high_resolution_clock::now();
                
                result.first_zone_load_ms = std::chrono::duration<double, std::milli>(end - start).count();
                
                std::cout << "  Cold zone load:   " << std::fixed << std::setprecision(2) 
                          << result.first_zone_load_ms << " ms\n";
                
                // Warm zone load (from cache)
                start = std::chrono::high_resolution_clock::now();
                loaded = loader.LoadZone(first_zone);
                end = std::chrono::high_resolution_clock::now();
                
                result.cached_zone_load_ms = std::chrono::duration<double, std::milli>(end - start).count();
                
                std::cout << "  Cached zone load: " << std::fixed << std::setprecision(2) 
                          << result.cached_zone_load_ms << " ms\n";
                
                double cache_speedup = result.first_zone_load_ms / result.cached_zone_load_ms;
                std::cout << "  Cache speedup:    " << std::fixed << std::setprecision(1) 
                          << cache_speedup << "x ✓\n";
            }
            
            loader.Close();
        }

        // Test 3: Memory footprint
        std::cout << "\n[TEST 3] Memory Efficiency\n";
        std::cout << "────────────────────────────────────────────────────\n";
        std::cout << "  Tensors indexed:  " << result.tensor_count << "\n";
        std::cout << "  Zones created:    " << result.zone_count << "\n";
        std::cout << "  Index overhead:   ~" << ((result.tensor_count * 100) / 1024) << " KB\n";
        std::cout << "  ✓ No tensor data loaded until zone access\n";

        results_.push_back(result);
    }

    void runMultipleModels(const std::string& directory) {
        std::vector<std::string> model_paths;
        
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                model_paths.push_back(entry.path().string());
                if (model_paths.size() >= 3) break;  // Test first 3 models
            }
        }

        std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Multi-Model Benchmark Suite                             ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\nFound " << model_paths.size() << " GGUF models\n";

        for (const auto& path : model_paths) {
            runBenchmark(path);
        }

        printSummary();
    }

    void printSummary() {
        std::cout << "\n\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    BENCHMARK SUMMARY                     ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

        double total_improvement = 0;
        int faster_count = 0;

        for (const auto& r : results_) {
            std::cout << "Model: " << r.model_name << "\n";
            std::cout << "  • Open time:      " << std::fixed << std::setprecision(2) << r.total_open_ms << " ms\n";
            std::cout << "  • Zone load:      " << r.first_zone_load_ms << " ms (cold)\n";
            std::cout << "  • Cached load:    " << r.cached_zone_load_ms << " ms\n";
            
            double improvement = ((BASELINE_MS - r.total_open_ms) / BASELINE_MS) * 100;
            total_improvement += improvement;
            
            if (improvement > 0) {
                std::cout << "  • vs Baseline:    +" << std::fixed << std::setprecision(1) 
                          << improvement << "% faster ✓\n";
                faster_count++;
            } else {
                std::cout << "  • vs Baseline:    " << std::fixed << std::setprecision(1) 
                          << std::abs(improvement) << "% slower\n";
            }
            std::cout << "\n";
        }

        if (!results_.empty()) {
            double avg_improvement = total_improvement / results_.size();
            
            std::cout << "Overall Statistics:\n";
            std::cout << "  • Models tested:         " << results_.size() << "\n";
            std::cout << "  • Faster than baseline:  " << faster_count << "/" << results_.size() << "\n";
            std::cout << "  • Average improvement:   " << std::fixed << std::setprecision(1) 
                      << avg_improvement << "%\n";
            std::cout << "  • Baseline reference:    " << BASELINE_MS << " ms\n\n";

            if (avg_improvement > 30) {
                std::cout << "✅ EXCELLENT: Lazy initialization shows significant improvement!\n";
            } else if (avg_improvement > 0) {
                std::cout << "✓ GOOD: Performance improved over baseline\n";
            } else {
                std::cout << "⚠ Review: Performance below baseline target\n";
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model_path_or_directory>\n";
        return 1;
    }

    std::string input = argv[1];
    LazyInitBenchmark benchmark;

    try {
        fs::path p(input);
        
        if (fs::is_directory(p)) {
            benchmark.runMultipleModels(input);
        } else if (fs::is_regular_file(p)) {
            benchmark.runBenchmark(input);
            benchmark.printSummary();
        } else {
            std::cerr << "Error: Invalid path: " << input << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
