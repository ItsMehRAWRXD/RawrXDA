#include "core/accelerator_router.h"
#include "core/flash_attention.h"
#include "core/enterprise_license.h"
#include "core/unified_memory_executor.h"
#include "enterprise_license.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Standalone benchmark tool does not dispatch flash-attention kernels directly.
// Provide no-op fallbacks for ASM exports so accelerator_router + flash_attention
// can link without pulling MASM objects into this utility target.
extern "C" {
    __declspec(selectany) uint64_t g_FlashAttnCalls = 0;
    __declspec(selectany) uint64_t g_FlashAttnTiles = 0;

    int32_t FlashAttention_CheckAVX512() { return 0; }
    int32_t FlashAttention_Init() { return 0; }
    int32_t FlashAttention_Forward(RawrXD::FlashAttentionConfig*) { return -1; }
    int32_t FlashAttention_GetTileConfig(RawrXD::FlashAttentionTileConfig* out) {
        if (out) {
            out->tileM = 64;
            out->tileN = 64;
            out->headDim = 128;
            out->scratchBytes = 0;
        }
        return 1;
    }
}

namespace RawrXD::License {
EnterpriseLicenseV2& EnterpriseLicenseV2::Instance() {
    static EnterpriseLicenseV2 instance;
    return instance;
}

bool EnterpriseLicenseV2::gate(FeatureID, const char*) {
    return true;
}
} // namespace RawrXD::License

namespace RawrXD {
EnterpriseLicense& EnterpriseLicense::Instance() {
    static EnterpriseLicense instance;
    return instance;
}

bool EnterpriseLicense::HasFeature(EnterpriseFeature) const {
    return true;
}

const char* EnterpriseLicense::GetEditionName() const {
    return "Benchmark";
}
} // namespace RawrXD

namespace RawrXD::UnifiedMemory {
UnifiedMemoryExecutor& UnifiedMemoryExecutor::instance() {
    static UnifiedMemoryExecutor instance;
    return instance;
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::initialize() {
    return {};
}

void UnifiedMemoryExecutor::shutdown() {
}
} // namespace RawrXD::UnifiedMemory

namespace {

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<uint32_t> parsePrompt(const std::string& csv) {
    std::vector<uint32_t> out;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = trim(item);
        if (t.empty()) continue;
        out.push_back(static_cast<uint32_t>(std::stoul(t)));
    }
    return out;
}

bool parseArg(int argc, char** argv, const char* name, std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::string(argv[i]) == name) {
            if (i + 1 < argc && argv[i + 1]) {
                value = argv[i + 1];
                return true;
            }
            return false;
        }
    }
    return false;
}

void writeJson(const std::string& path,
               const std::vector<uint32_t>& prompt,
               int runs,
               const double gen[18],
               const double longCtx[72]) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Failed to open JSON output: " << path << "\n";
        return;
    }

    out << std::fixed << std::setprecision(6);
    out << "{\n";
    out << "  \"runs\": " << runs << ",\n";
    out << "  \"prompt_tokens\": [";
    for (size_t i = 0; i < prompt.size(); ++i) {
        if (i) out << ", ";
        out << prompt[i];
    }
    out << "],\n";

    out << "  \"benchmark_generation\": {\n";
    out << "    \"greedy_sync_ms\": " << gen[0] << ",\n";
    out << "    \"greedy_pipe_ms\": " << gen[1] << ",\n";
    out << "    \"greedy_speedup\": " << gen[2] << ",\n";
    out << "    \"greedy_sync_tps\": " << gen[3] << ",\n";
    out << "    \"greedy_pipe_tps\": " << gen[4] << ",\n";
    out << "    \"stoch_sync_ms\": " << gen[5] << ",\n";
    out << "    \"stoch_pipe_ms\": " << gen[6] << ",\n";
    out << "    \"stoch_speedup\": " << gen[7] << ",\n";
    out << "    \"stoch_sync_tps\": " << gen[8] << ",\n";
    out << "    \"stoch_pipe_tps\": " << gen[9] << ",\n";
    out << "    \"greedy_sync_ci95_ms\": " << gen[10] << ",\n";
    out << "    \"greedy_pipe_ci95_ms\": " << gen[11] << ",\n";
    out << "    \"stoch_sync_ci95_ms\": " << gen[12] << ",\n";
    out << "    \"stoch_pipe_ci95_ms\": " << gen[13] << ",\n";
    out << "    \"greedy_sync_std_ms\": " << gen[14] << ",\n";
    out << "    \"greedy_pipe_std_ms\": " << gen[15] << ",\n";
    out << "    \"stoch_sync_std_ms\": " << gen[16] << ",\n";
    out << "    \"stoch_pipe_std_ms\": " << gen[17] << "\n";
    out << "  },\n";

    out << "  \"benchmark_long_context\": [\n";
    for (int s = 0; s < 6; ++s) {
        int o = s * 12;
        out << "    {\n";
        out << "      \"context_length\": " << longCtx[o + 0] << ",\n";
        out << "      \"sync_ms\": " << longCtx[o + 1] << ",\n";
        out << "      \"pipe_ms\": " << longCtx[o + 2] << ",\n";
        out << "      \"sync_tps\": " << longCtx[o + 3] << ",\n";
        out << "      \"pipe_tps\": " << longCtx[o + 4] << ",\n";
        out << "      \"speedup\": " << longCtx[o + 5] << ",\n";
        out << "      \"sync_std_ms\": " << longCtx[o + 6] << ",\n";
        out << "      \"pipe_std_ms\": " << longCtx[o + 7] << ",\n";
        out << "      \"sync_ci95_ms\": " << longCtx[o + 8] << ",\n";
        out << "      \"pipe_ci95_ms\": " << longCtx[o + 9] << ",\n";
        out << "      \"sync_std_tps\": " << longCtx[o + 10] << ",\n";
        out << "      \"pipe_std_tps\": " << longCtx[o + 11] << "\n";
        out << "    }";
        if (s < 5) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void writeCsv(const std::string& path, const double gen[18], const double longCtx[72]) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Failed to open CSV output: " << path << "\n";
        return;
    }

    out << std::fixed << std::setprecision(6);
    out << "suite,mode,context_length,sync_ms,pipe_ms,speedup,sync_tps,pipe_tps,sync_ci95_ms,pipe_ci95_ms,sync_std_ms,pipe_std_ms,sync_std_tps,pipe_std_tps\n";

    out << "generation,greedy,0," << gen[0] << "," << gen[1] << "," << gen[2] << ","
        << gen[3] << "," << gen[4] << "," << gen[10] << "," << gen[11] << ","
        << gen[14] << "," << gen[15] << ",0,0\n";

    out << "generation,stochastic,0," << gen[5] << "," << gen[6] << "," << gen[7] << ","
        << gen[8] << "," << gen[9] << "," << gen[12] << "," << gen[13] << ","
        << gen[16] << "," << gen[17] << ",0,0\n";

    for (int s = 0; s < 6; ++s) {
        int o = s * 12;
        out << "long_context,stochastic," << longCtx[o + 0] << ","
            << longCtx[o + 1] << "," << longCtx[o + 2] << "," << longCtx[o + 5] << ","
            << longCtx[o + 3] << "," << longCtx[o + 4] << ","
            << longCtx[o + 8] << "," << longCtx[o + 9] << ","
            << longCtx[o + 6] << "," << longCtx[o + 7] << ","
            << longCtx[o + 10] << "," << longCtx[o + 11] << "\n";
    }
}

void printUsage(const char* exe) {
    std::cerr
        << "Usage: " << exe << " --prompt <csv_tokens> [--runs <n>] [--json <file>] [--csv <file>]\n"
        << "Example: " << exe << " --prompt \"1,15043\" --runs 5 --json router_bench.json --csv router_bench.csv\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string promptCsv;
    if (!parseArg(argc, argv, "--prompt", promptCsv) || promptCsv.empty()) {
        printUsage(argv[0]);
        return 2;
    }

    std::string runsStr;
    int runs = 5;
    if (parseArg(argc, argv, "--runs", runsStr) && !runsStr.empty()) {
        runs = std::max(1, std::min(10, std::atoi(runsStr.c_str())));
    }

    std::string jsonPath = "router_benchmark_results.json";
    std::string csvPath = "router_benchmark_results.csv";
    std::string temp;
    if (parseArg(argc, argv, "--json", temp) && !temp.empty()) jsonPath = temp;
    if (parseArg(argc, argv, "--csv", temp) && !temp.empty()) csvPath = temp;

    std::vector<uint32_t> prompt = parsePrompt(promptCsv);
    if (prompt.empty()) {
        std::cerr << "Prompt token list is empty after parsing\n";
        return 2;
    }

    AcceleratorRouter& router = AcceleratorRouter::instance();
    RouterResult initRes = router.initialize();
    if (!initRes.success && !router.isInitialized()) {
        std::cerr << "Router init failed: " << (initRes.detail ? initRes.detail : "unknown") << "\n";
        return 1;
    }

    RouterBackendType selectedBackend = RouterBackendType::NVIDIA_CUDA;
    RouterResult forceRes = router.forceBackend(RouterBackendType::NVIDIA_CUDA);
    if (!forceRes.success) {
        selectedBackend = router.getActiveBackend();
        std::cerr << "Warning: forceBackend(NVIDIA_CUDA) failed, using active backend "
                  << static_cast<int>(selectedBackend) << ": "
                  << (forceRes.detail ? forceRes.detail : "unknown") << "\n";
    }
    if (selectedBackend == RouterBackendType::None) {
        std::cerr << "No backend available for benchmark execution\n";
        router.shutdown();
        return 1;
    }

    double genOut[18] = {};
    RouterInferenceTask genTask{};
    genTask.inputData = prompt.data();
    genTask.inputSizeBytes = static_cast<uint64_t>(prompt.size() * sizeof(uint32_t));
    genTask.outputData = genOut;
    genTask.outputSizeBytes = sizeof(genOut);
    genTask.kernelName = "benchmark_generation";
    genTask.priority = DispatchPriority::Batch;
    genTask.scope = DispatchScope::Inference;
    genTask.preferredBackend = selectedBackend;
    genTask.timeoutMs = 0;
    genTask.batchSize = static_cast<uint32_t>(runs);
    genTask.quantType = 0;

    RouterResult genRes = router.submitTo(selectedBackend, genTask);
    if (!genRes.success) {
        std::cerr << "benchmark_generation failed: "
                  << (genRes.detail ? genRes.detail : "unknown")
                  << " (code=" << genRes.errorCode << ")\n";
        return 1;
    }

    double longOut[72] = {};
    RouterInferenceTask longTask{};
    longTask.inputData = prompt.data();
    longTask.inputSizeBytes = static_cast<uint64_t>(prompt.size() * sizeof(uint32_t));
    longTask.outputData = longOut;
    longTask.outputSizeBytes = sizeof(longOut);
    longTask.kernelName = "benchmark_long_context";
    longTask.priority = DispatchPriority::Batch;
    longTask.scope = DispatchScope::Inference;
    longTask.preferredBackend = selectedBackend;
    longTask.timeoutMs = 0;
    longTask.batchSize = static_cast<uint32_t>(runs);
    longTask.quantType = 0;

    RouterResult longRes = router.submitTo(selectedBackend, longTask);
    if (!longRes.success) {
        std::cerr << "benchmark_long_context failed: "
                  << (longRes.detail ? longRes.detail : "unknown")
                  << " (code=" << longRes.errorCode << ")\n";
        return 1;
    }

    writeJson(jsonPath, prompt, runs, genOut, longOut);
    writeCsv(csvPath, genOut, longOut);

    std::cout << "Benchmark run complete\n";
    std::cout << "  JSON: " << jsonPath << "\n";
    std::cout << "  CSV:  " << csvPath << "\n";
    std::cout << "  Greedy speedup:    " << genOut[2] << "x\n";
    std::cout << "  Stochastic speedup:" << genOut[7] << "x\n";

    router.shutdown();
    return 0;
}
