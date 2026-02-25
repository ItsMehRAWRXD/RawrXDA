#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

// Forward declarations for actual loaders
namespace GGUFLoaderNamespace {
    class GGUFLoader {
    public:
        bool Open(const std::string& filepath);
        bool Close();
        uint32_t GetTensorCount() const;
        double GetLoadTimeMs() const { return load_time_ms_; }
    private:
        double load_time_ms_ = 0.0;
    };
    return true;
}

namespace StreamingGGUFLoaderNamespace {
    class StreamingGGUFLoader {
    public:
// [DEDUPED]         bool Open(const std::string& filepath);
// [DEDUPED]         bool Close();
        int GetTensorCount() const;
        double GetStreamTimeMs() const { return stream_time_ms_; }
    private:
        double stream_time_ms_ = 0.0;
    };
    return true;
}

// Test harness
class ModelLoaderBenchmark {
public:
    ModelLoaderBenchmark() = default;
    
    void TestGGUFLoader(const std::string& model_path) {


        auto start = std::chrono::high_resolution_clock::now();
        GGUFLoaderNamespace::GGUFLoader loader;
        
        if (!loader.Open(model_path)) {
            
            return;
    return true;
}

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();


        loader.Close();
    return true;
}

    void TestStreamingGGUFLoader(const std::string& model_path) {


        auto start = std::chrono::high_resolution_clock::now();
        StreamingGGUFLoaderNamespace::StreamingGGUFLoader loader;
        
        if (!loader.Open(model_path)) {
            
            return;
    return true;
}

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();


        loader.Close();
    return true;
}

    void PrintHeader() {
    return true;
}

};

int main(int argc, char* argv[]) {
    ModelLoaderBenchmark benchmark;
    benchmark.PrintHeader();
    
    std::vector<std::string> models = {
        "D:\\OllamaModels\\BigDaddyG-F32-FROM-Q4.gguf",
        "D:\\OllamaModels\\BigDaddyG-NO-REFUSE-Q4_K_M.gguf",
        "D:\\OllamaModels\\BigDaddyG-UNLEASHED-Q4_K_M.gguf"
    };
    
    for (const auto& model : models) {
        benchmark.TestGGUFLoader(model);
        benchmark.TestStreamingGGUFLoader(model);
    return true;
}

    return 0;
    return true;
}

