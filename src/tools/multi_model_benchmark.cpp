#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <json/json.h>
#include <cstring>
#include "../qtapp/inference_engine.hpp"

struct BenchmarkResult {
    std::string model_path;
    std::string model_name;
    double tokens_per_sec;
    double avg_latency_ms;
    long load_time_ms;
    long inference_time_ms;
    int tokens_generated;
    bool success;
};

std::string getModelNameFromPath(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    size_t lastDot = filename.find_last_of(".");
    return (lastDot == std::string::npos) ? filename : filename.substr(0, lastDot);
}

BenchmarkResult benchmarkModel(const std::string& model_path, int num_tokens = 256) {
    BenchmarkResult result;
    result.model_path = model_path;
    result.model_name = getModelNameFromPath(model_path);
    result.success = false;
    result.tokens_generated = 0;

    try {
        InferenceEngine engine;
        
        // Measure load time
        auto load_start = std::chrono::high_resolution_clock::now();
        if (!engine.loadModel(QString::fromStdString(model_path))) {
            std::cerr << "Failed to load model: " << model_path << std::endl;
            return result;
        }
        auto load_end = std::chrono::high_resolution_clock::now();
        result.load_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();

        // Simple prompt
        std::vector<int32_t> prompt = engine.tokenize(QString("The meaning of life is"));
        
        // Measure inference time
        auto inference_start = std::chrono::high_resolution_clock::now();
        std::vector<int32_t> generated = engine.generate(prompt, num_tokens);
        auto inference_end = std::chrono::high_resolution_clock::now();
        
        result.inference_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(inference_end - inference_start).count();
        result.tokens_generated = generated.size() - prompt.size();
        
        // Calculate metrics
        if (result.inference_time_ms > 0) {
            result.tokens_per_sec = (result.tokens_generated * 1000.0) / result.inference_time_ms;
            result.avg_latency_ms = (double)result.inference_time_ms / result.tokens_generated;
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception benchmarking model: " << e.what() << std::endl;
    }
    
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: multi_model_benchmark <model_path> [num_tokens]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    int num_tokens = (argc > 2) ? std::atoi(argv[2]) : 256;

    // Run benchmark
    BenchmarkResult result = benchmarkModel(model_path, num_tokens);

    // Output JSON for Python parsing
    Json::Value output;
    output["model_name"] = result.model_name;
    output["model_path"] = result.model_path;
    output["success"] = result.success;
    output["load_time_ms"] = (Json::Value::Int64)result.load_time_ms;
    output["inference_time_ms"] = (Json::Value::Int64)result.inference_time_ms;
    output["tokens_generated"] = result.tokens_generated;
    output["tokens_per_sec"] = result.tokens_per_sec;
    output["avg_latency_ms"] = result.avg_latency_ms;

    Json::StreamWriterBuilder writer;
    std::cout << Json::writeString(writer, output);

    return result.success ? 0 : 1;
}
