#include <iostream>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <string>
#include <ctime>
#include "gguf_loader.h"
#include "vulkan_compute.h"
#include "telemetry.h"
#include "settings.h"
#include "gui.h" // for AppState

// Simple benchmark harness for GGUF parsing + optional Vulkan init
// Usage: model_loader_bench <path-to-model.gguf> [--no-gpu] [--iter N] [--matmul-size S] [--vec-size V]
// Outputs JSON line to stdout and writes bench_results.json in working directory.

struct BenchResult {
    std::string model_path;
    uint64_t file_size{};
    bool header_ok{};
    bool metadata_ok{};
    size_t tensor_count{};
    double parse_ms{};
    bool gpu_enabled{};
    bool gpu_init_ok{};
    double gpu_init_ms{};
    std::string timestamp_utc;
    // Kernel microbench
    bool matmul_ran{};
    double matmul_avg_ms{};
    int matmul_iterations{};
    bool rmsnorm_ran{};
    double rmsnorm_avg_ms{};
    int rmsnorm_iterations{};
    bool silu_ran{};
    double silu_avg_ms{};
    int silu_iterations{};
    bool attention_ran{};
    double attention_avg_ms{};
    int attention_iterations{};
    // Overclock / thermal context
    bool overclock_governor_enabled{};
    int cpu_temp_c{};
    int gpu_hotspot_c{};
    int cpu_temp_headroom_c{};
    int gpu_temp_headroom_c{};
    int applied_core_offset_mhz{};
};

static void EnsureBenchDir() {
    std::filesystem::path benchDir{"bench"};
    if (!std::filesystem::exists(benchDir)) {
        std::error_code ec; std::filesystem::create_directories(benchDir, ec);
    }
}

static void WriteResultJSON(const BenchResult& r) {
    EnsureBenchDir();
    std::filesystem::path outPath{"bench/bench_results.json"};
    bool newFile = !std::filesystem::exists(outPath);
    std::ofstream ofs(outPath, std::ios::app);
    if (newFile) {
        ofs << "[\n"; // start array
    }
    ofs << "  {"
        << "\"timestamp_utc\":\"" << r.timestamp_utc << "\"," 
        << "\"model_path\":\"" << r.model_path << "\"," 
        << "\"file_size\":" << r.file_size << ","
        << "\"header_ok\":" << (r.header_ok?"true":"false") << ","
        << "\"metadata_ok\":" << (r.metadata_ok?"true":"false") << ","
        << "\"tensor_count\":" << r.tensor_count << ","
        << "\"parse_ms\":" << r.parse_ms << ","
        << "\"gpu_enabled\":" << (r.gpu_enabled?"true":"false") << ","
        << "\"gpu_init_ok\":" << (r.gpu_init_ok?"true":"false") << ","
        << "\"gpu_init_ms\":" << r.gpu_init_ms << ","
        << "\"matmul_ran\":" << (r.matmul_ran?"true":"false") << ","
        << "\"matmul_iterations\":" << r.matmul_iterations << ","
        << "\"matmul_avg_ms\":" << r.matmul_avg_ms << ","
        << "\"rmsnorm_ran\":" << (r.rmsnorm_ran?"true":"false") << ","
        << "\"rmsnorm_iterations\":" << r.rmsnorm_iterations << ","
        << "\"rmsnorm_avg_ms\":" << r.rmsnorm_avg_ms << ","
        << "\"silu_ran\":" << (r.silu_ran?"true":"false") << ","
        << "\"silu_iterations\":" << r.silu_iterations << ","
        << "\"silu_avg_ms\":" << r.silu_avg_ms << ","
        << "\"attention_ran\":" << (r.attention_ran?"true":"false") << ","
        << "\"attention_iterations\":" << r.attention_iterations << ","
        << "\"attention_avg_ms\":" << r.attention_avg_ms << "},\n"; // trailing comma (simple append format)
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: model_loader_bench <model.gguf> [--no-gpu]\n";
        return 1;
    }

    std::string modelPath = argv[1];
    bool requestNoGPU = false;
    int iterations = 5;
    uint32_t matmulSize = 128; // M=K=N
    uint32_t vecSize = 65536;  // for RMSNorm / SiLU
    uint32_t attSeqLen = 64;
    uint32_t attHeadDim = 64;
    for (int i=2; i<argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-gpu") requestNoGPU = true;
        else if (arg == "--iter" && i+1 < argc) { iterations = std::stoi(argv[++i]); }
        else if (arg == "--matmul-size" && i+1 < argc) { matmulSize = static_cast<uint32_t>(std::stoul(argv[++i])); }
        else if (arg == "--vec-size" && i+1 < argc) { vecSize = static_cast<uint32_t>(std::stoul(argv[++i])); }
        else if (arg == "--att-seq" && i+1 < argc) { attSeqLen = static_cast<uint32_t>(std::stoul(argv[++i])); }
        else if (arg == "--att-head" && i+1 < argc) { attHeadDim = static_cast<uint32_t>(std::stoul(argv[++i])); }
    }
    if (iterations < 1) iterations = 1;

    BenchResult result; 
    result.model_path = modelPath;
    result.gpu_enabled = !requestNoGPU;

    // Load settings to populate overclock context
    AppState st;
    Settings::LoadOverclock(st);
    result.overclock_governor_enabled = st.enable_overclock_governor;
    telemetry::Initialize();
    telemetry::TelemetrySnapshot snap; telemetry::Poll(snap);
    if (snap.cpuTempValid) {
        result.cpu_temp_c = (int)std::lround(snap.cpuTempC);
        result.cpu_temp_headroom_c = (int)st.max_cpu_temp_c - result.cpu_temp_c;
    }
    if (snap.gpuTempValid) {
        result.gpu_hotspot_c = (int)std::lround(snap.gpuTempC);
        result.gpu_temp_headroom_c = (int)st.max_gpu_hotspot_c - result.gpu_hotspot_c;
    }
    result.applied_core_offset_mhz = st.applied_core_offset_mhz; // persisted if governor updated before

    if (!std::filesystem::exists(modelPath)) {
        std::cerr << "Model file not found: " << modelPath << "\n";
        return 2;
    }

    // Timestamp
    {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm gmt{};
    #ifdef _WIN32
        gmtime_s(&gmt, &tt);
    #else
        gmt = *std::gmtime(&tt);
    #endif
        char buf[64]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gmt);
        result.timestamp_utc = buf;
    }

    GGUFLoader loader;

    auto t0 = std::chrono::high_resolution_clock::now();
    if (!loader.Open(modelPath)) {
        std::cerr << "Failed to open model file." << std::endl;
        return 3;
    }

    result.file_size = loader.GetFileSize();
    result.header_ok = loader.ParseHeader();
    if (result.header_ok) {
        result.metadata_ok = loader.ParseMetadata();
    }
    if (result.metadata_ok) {
        result.tensor_count = loader.GetTensorInfo().size();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    result.parse_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto gpuStart = std::chrono::high_resolution_clock::now();
    if (result.gpu_enabled) {
        VulkanCompute compute;
        result.gpu_init_ok = compute.Initialize();
        auto gpuEnd = std::chrono::high_resolution_clock::now();
        result.gpu_init_ms = std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count();

        // MatMul microbench (only if initialized OK)
        if (result.gpu_init_ok) {
            // MatMul microbench
            const uint32_t M = matmulSize, K = matmulSize, N = matmulSize;
            std::vector<float> A(M*K, 0.5f);
            std::vector<float> B(K*N, 0.25f);
            std::vector<float> OUT(M*N, 0.0f);
            std::vector<double> times;
            for (int i=0;i<iterations;i++) {
                auto kt0 = std::chrono::high_resolution_clock::now();
                bool ok = compute.ExecuteMatMul(A.data(), B.data(), OUT.data(), M, K, N);
                auto kt1 = std::chrono::high_resolution_clock::now();
                if (!ok) { break; }
                double ms = std::chrono::duration<double, std::milli>(kt1 - kt0).count();
                times.push_back(ms);
            }
            if (!times.empty()) {
                result.matmul_ran = true;
                result.matmul_iterations = static_cast<int>(times.size());
                double sum=0; for (double v: times) sum+=v;
                result.matmul_avg_ms = sum / times.size();
            }

            // RMSNorm microbench
            std::vector<float> rmsData(vecSize, 0.1f);
            times.clear();
            for (int i=0;i<iterations;i++) {
                auto kt0 = std::chrono::high_resolution_clock::now();
                bool ok = compute.ExecuteRMSNorm(rmsData.data(), vecSize, 1e-5f);
                auto kt1 = std::chrono::high_resolution_clock::now();
                if (!ok) { break; }
                double ms = std::chrono::duration<double, std::milli>(kt1 - kt0).count();
                times.push_back(ms);
            }
            if (!times.empty()) {
                result.rmsnorm_ran = true;
                result.rmsnorm_iterations = static_cast<int>(times.size());
                double sum=0; for (double v: times) sum+=v;
                result.rmsnorm_avg_ms = sum / times.size();
            }

            // SiLU microbench
            std::vector<float> siluData(vecSize, 0.2f);
            times.clear();
            for (int i=0;i<iterations;i++) {
                auto kt0 = std::chrono::high_resolution_clock::now();
                bool ok = compute.ExecuteSiLU(siluData.data(), vecSize);
                auto kt1 = std::chrono::high_resolution_clock::now();
                if (!ok) { break; }
                double ms = std::chrono::duration<double, std::milli>(kt1 - kt0).count();
                times.push_back(ms);
            }
            if (!times.empty()) {
                result.silu_ran = true;
                result.silu_iterations = static_cast<int>(times.size());
                double sum=0; for (double v: times) sum+=v;
                result.silu_avg_ms = sum / times.size();
            }

            // Attention microbench (scaled dot-product single head)
            std::vector<float> Q(attSeqLen * attHeadDim, 0.01f);
            std::vector<float> K(attSeqLen * attHeadDim, 0.02f);
            std::vector<float> V(attSeqLen * attHeadDim, 0.03f);
            std::vector<float> O(attSeqLen * attHeadDim, 0.0f);
            times.clear();
            for (int i=0;i<iterations;i++) {
                auto kt0 = std::chrono::high_resolution_clock::now();
                bool ok = compute.ExecuteAttention(Q.data(), K.data(), V.data(), O.data(), attSeqLen, attHeadDim);
                auto kt1 = std::chrono::high_resolution_clock::now();
                if (!ok) { break; }
                double ms = std::chrono::duration<double, std::milli>(kt1 - kt0).count();
                times.push_back(ms);
            }
            if (!times.empty()) {
                result.attention_ran = true;
                result.attention_iterations = static_cast<int>(times.size());
                double sum=0; for (double v: times) sum+=v;
                result.attention_avg_ms = sum / times.size();
            }
        }
    }

    // Emit JSON line to stdout (single object)
    std::cout << "{"
              << "\"timestamp_utc\":\"" << result.timestamp_utc << "\"," 
              << "\"model_path\":\"" << result.model_path << "\"," 
              << "\"file_size\":" << result.file_size << ","
              << "\"header_ok\":" << (result.header_ok?"true":"false") << ","
              << "\"metadata_ok\":" << (result.metadata_ok?"true":"false") << ","
              << "\"tensor_count\":" << result.tensor_count << ","
              << "\"parse_ms\":" << result.parse_ms << ","
              << "\"gpu_enabled\":" << (result.gpu_enabled?"true":"false") << ","
              << "\"gpu_init_ok\":" << (result.gpu_init_ok?"true":"false") << ","
              << "\"gpu_init_ms\":" << result.gpu_init_ms << ","\
              << "\"matmul_ran\":" << (result.matmul_ran?"true":"false") << ","\
              << "\"matmul_iterations\":" << result.matmul_iterations << ","\
              << "\"matmul_avg_ms\":" << result.matmul_avg_ms << ","\
              << "\"rmsnorm_ran\":" << (result.rmsnorm_ran?"true":"false") << ","\
              << "\"rmsnorm_iterations\":" << result.rmsnorm_iterations << ","\
              << "\"rmsnorm_avg_ms\":" << result.rmsnorm_avg_ms << ","\
              << "\"silu_ran\":" << (result.silu_ran?"true":"false") << ","\
              << "\"silu_iterations\":" << result.silu_iterations << ","\
              << "\"silu_avg_ms\":" << result.silu_avg_ms << ","\
              << "\"attention_ran\":" << (result.attention_ran?"true":"false") << ","\
              << "\"attention_iterations\":" << result.attention_iterations << ","\
              << "\"attention_avg_ms\":" << result.attention_avg_ms << ","\
              << "\"overclock_governor_enabled\":" << (result.overclock_governor_enabled?"true":"false") << ","\
              << "\"cpu_temp_c\":" << result.cpu_temp_c << ","\
              << "\"gpu_hotspot_c\":" << result.gpu_hotspot_c << ","\
              << "\"cpu_temp_headroom_c\":" << result.cpu_temp_headroom_c << ","\
              << "\"gpu_temp_headroom_c\":" << result.gpu_temp_headroom_c << ","\
              << "\"applied_core_offset_mhz\":" << result.applied_core_offset_mhz << "}" << std::endl;

    WriteResultJSON(result);
    telemetry::Shutdown();

    loader.Close();
    return 0;
}
