#include "runtime/RuntimeProvider.h"

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct StressConfig {
    std::wstring modelPath;
    int maxTokens = 512;
    int runs = 1;
    float minTps = 40.0f;
};

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], len);
    return out;
}

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) {
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), &out[0], len, nullptr, nullptr);
    return out;
}

bool FileExistsW(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring ResolveDefaultModel() {
    const char* envModel = std::getenv("RAWRXD_RUNTIME_MODEL");
    if (envModel && *envModel) {
        std::wstring model = Utf8ToWide(envModel);
        if (FileExistsW(model)) {
            return model;
        }
    }

    const std::vector<std::wstring> candidates = {
        L"D:\\codestral22b.gguf",
        L"D:\\gptoss20b_link.gguf",
        L"D:\\ministral3.gguf",
        L"D:\\phi3mini.gguf"
    };

    for (const auto& candidate : candidates) {
        if (FileExistsW(candidate)) {
            return candidate;
        }
    }

    return {};
}

bool ParseArgs(int argc, char** argv, StressConfig& cfg) {
    cfg.modelPath = ResolveDefaultModel();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            cfg.modelPath = Utf8ToWide(argv[++i]);
            continue;
        }
        if (arg == "--tokens" && i + 1 < argc) {
            cfg.maxTokens = std::max(1, std::atoi(argv[++i]));
            continue;
        }
        if (arg == "--runs" && i + 1 < argc) {
            cfg.runs = std::max(1, std::atoi(argv[++i]));
            continue;
        }
        if (arg == "--min-tps" && i + 1 < argc) {
            cfg.minTps = static_cast<float>(std::atof(argv[++i]));
            if (cfg.minTps < 0.0f) {
                cfg.minTps = 0.0f;
            }
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "RuntimeProvider 512-token stress\n"
                      << "  --model <path>   GGUF model path\n"
                      << "  --tokens <n>     max generated tokens (default 512)\n"
                      << "  --runs <n>       number of repeated runs (default 1)\n"
                      << "  --min-tps <n>    pass threshold (default 40.0)\n";
            return false;
        }
    }

    return true;
}

}  // namespace

int main(int argc, char** argv) {
    StressConfig cfg;
    if (!ParseArgs(argc, argv, cfg)) {
        return 2;
    }

    if (cfg.modelPath.empty() || !FileExistsW(cfg.modelPath)) {
        std::cerr << "[FAIL] No model found. Provide --model <path> or set RAWRXD_RUNTIME_MODEL.\n";
        return 3;
    }

    const std::string prompt =
        "You are RuntimeProvider stress probe. "
        "Generate deterministic coding guidance bullets for Win32 concurrency and Vulkan telemetry. "
        "Keep emitting concise technical tokens without stopping early.";

    RawrXD::Runtime::RuntimeProvider provider;
    if (!provider.LoadModel(cfg.modelPath)) {
        std::cerr << "[FAIL] LoadModel failed: " << provider.GetLastError() << "\n";
        return 4;
    }

    double totalTps = 0.0;
    double totalTtftMs = 0.0;
    int passCount = 0;

    for (int run = 0; run < cfg.runs; ++run) {
        int streamedTokenCount = 0;
        auto runStart = std::chrono::steady_clock::now();

        RawrXD::Runtime::RuntimeProvider::GenParams gp;
        gp.prompt = prompt;
        gp.maxTokens = cfg.maxTokens;
        gp.temperature = 0.1f;
        gp.onToken = [&](const std::string&) {
            ++streamedTokenCount;
        };

        const bool ok = provider.Generate(gp);
        const auto runMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - runStart).count();

        if (!ok) {
            std::cerr << "[FAIL] Generate failed (run " << (run + 1) << "): " << provider.GetLastError() << "\n";
            continue;
        }

        const auto& telem = provider.GetTelemetry();
        const bool runPass = (telem.tokensPerSecond >= cfg.minTps);
        if (runPass) {
            ++passCount;
        }

        totalTps += static_cast<double>(telem.tokensPerSecond);
        totalTtftMs += static_cast<double>(telem.timeToFirstTokenS) * 1000.0;

        std::cout << "[RUN " << (run + 1) << "] "
                  << "tokens(streamed)=" << streamedTokenCount
                  << " ttft_ms=" << std::fixed << std::setprecision(2) << (telem.timeToFirstTokenS * 1000.0f)
                  << " tps=" << std::fixed << std::setprecision(2) << telem.tokensPerSecond
                  << " wall_ms=" << runMs
                  << " backend=\"" << telem.activeBackend << "\""
                  << " vram_usage_mb=" << (telem.vramUsageBytes / (1024ull * 1024ull))
                  << " vram_total_mb=" << (telem.vramTotalBytes / (1024ull * 1024ull))
                  << " result=" << (runPass ? "PASS" : "FAIL")
                  << "\n";
    }

    const double avgTps = cfg.runs > 0 ? (totalTps / static_cast<double>(cfg.runs)) : 0.0;
    const double avgTtftMs = cfg.runs > 0 ? (totalTtftMs / static_cast<double>(cfg.runs)) : 0.0;
    const bool overallPass = (passCount == cfg.runs);

    std::cout << "[SUMMARY] model=\"" << WideToUtf8(cfg.modelPath)
              << "\" runs=" << cfg.runs
              << " avg_ttft_ms=" << std::fixed << std::setprecision(2) << avgTtftMs
              << " avg_tps=" << std::fixed << std::setprecision(2) << avgTps
              << " threshold_tps=" << cfg.minTps
              << " milestoneA=" << (overallPass ? "CERTIFIED" : "NOT_CERTIFIED")
              << "\n";

    provider.UnloadModel();
    return overallPass ? 0 : 5;
}
