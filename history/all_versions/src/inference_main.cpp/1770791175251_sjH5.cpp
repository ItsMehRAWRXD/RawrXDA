// ============================================================================
// inference_main.cpp — RawrXD-InferenceEngine Standalone Entrypoint
// ============================================================================
//
// Phase 6: Production Compilation — Standalone inference executable.
// Loads a GGUF model and emits tokens without the IDE wrapper.
//
// Usage:
//   RawrXD-InferenceEngine.exe <model.gguf> [options]
//
// Options:
//   --prompt "text"       Initial prompt text (default: interactive)
//   --tokens N            Max tokens to generate (default: 256)
//   --temp F              Temperature (default: 0.8)
//   --top-k N             Top-K sampling (default: 40)
//   --top-p F             Top-P sampling (default: 0.9)
//   --threads N           CPU threads (default: auto)
//   --interactive         Interactive REPL mode
//   --benchmark           Run throughput benchmark
//   --quiet               Suppress info logging (tokens only)
//
// Architecture: C++20, no exceptions, PatchResult pattern.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "cpu_inference_engine.h"
#include "gguf_loader.h"
#include "engine/rawr_engine.h"
#include "engine/gguf_core.h"
#include "engine/bpe_tokenizer.h"
#include "engine/sampler.h"
#include "engine/inference_kernels.h"
#include "compression_interface.h"
#include "streaming_gguf_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================================
// CLI Option Parsing
// ============================================================================
struct InferenceConfig {
    const char* modelPath       = nullptr;
    std::string prompt;
    int         maxTokens       = 256;
    float       temperature     = 0.8f;
    int         topK            = 40;
    float       topP            = 0.9f;
    int         threads         = 0;    // 0 = auto
    bool        interactive     = false;
    bool        benchmark       = false;
    bool        quiet           = false;
};

static void printUsage(const char* exe) {
    fprintf(stderr,
        "RawrXD Inference Engine — Standalone GGUF Token Generator\n"
        "Usage: %s <model.gguf> [options]\n\n"
        "Options:\n"
        "  --prompt \"text\"   Prompt text (default: interactive)\n"
        "  --tokens N        Max tokens (default: 256)\n"
        "  --temp F          Temperature (default: 0.8)\n"
        "  --top-k N         Top-K (default: 40)\n"
        "  --top-p F         Top-P (default: 0.9)\n"
        "  --threads N       CPU threads (default: auto)\n"
        "  --interactive     Interactive REPL\n"
        "  --benchmark       Throughput benchmark\n"
        "  --quiet           Suppress info, tokens only\n"
        "\n", exe);
}

static bool parseArgs(int argc, char** argv, InferenceConfig& cfg) {
    if (argc < 2) return false;

    cfg.modelPath = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) {
            cfg.prompt = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0 && i + 1 < argc) {
            cfg.maxTokens = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--temp") == 0 && i + 1 < argc) {
            cfg.temperature = static_cast<float>(atof(argv[++i]));
        } else if (strcmp(argv[i], "--top-k") == 0 && i + 1 < argc) {
            cfg.topK = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--top-p") == 0 && i + 1 < argc) {
            cfg.topP = static_cast<float>(atof(argv[++i]));
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            cfg.threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--interactive") == 0) {
            cfg.interactive = true;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            cfg.benchmark = true;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            cfg.quiet = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return false;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Benchmark Mode
// ============================================================================
struct BenchmarkResult {
    double tokensPerSecond;
    double timeToFirstTokenMs;
    double totalTimeMs;
    int    tokensGenerated;
};

static void runBenchmark(RawrXD::CpuInferenceEngine& engine, const InferenceConfig& cfg) {
    const char* benchPrompt = "The quick brown fox jumps over the lazy dog. "
                               "In a world where artificial intelligence has become ";

    fprintf(stderr, "\n=== RawrXD Inference Engine Benchmark ===\n");
    fprintf(stderr, "Model: %s\n", cfg.modelPath);
    fprintf(stderr, "Tokens: %d\n", cfg.maxTokens);
    fprintf(stderr, "Temperature: %.2f\n", cfg.temperature);
    fprintf(stderr, "Threads: %d\n", cfg.threads > 0 ? cfg.threads : -1);
    fprintf(stderr, "\nGenerating...\n\n");

    auto startTime = std::chrono::high_resolution_clock::now();
    bool firstToken = true;
    double ttftMs = 0.0;
    int tokenCount = 0;

    auto callback = [&](const char* token) {
        if (firstToken) {
            auto now = std::chrono::high_resolution_clock::now();
            ttftMs = std::chrono::duration<double, std::milli>(now - startTime).count();
            firstToken = false;
        }
        tokenCount++;
        if (!cfg.quiet) {
            fputs(token, stdout);
            fflush(stdout);
        }
    };

    // Generate using the engine
    std::string result = engine.generate(benchPrompt, cfg.maxTokens, callback);

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    double tokPerSec = (tokenCount > 0 && totalMs > 0)
        ? (tokenCount / (totalMs / 1000.0))
        : 0.0;

    fprintf(stderr, "\n\n=== Benchmark Results ===\n");
    fprintf(stderr, "Tokens generated:     %d\n", tokenCount);
    fprintf(stderr, "Time to first token:  %.2f ms\n", ttftMs);
    fprintf(stderr, "Total time:           %.2f ms\n", totalMs);
    fprintf(stderr, "Throughput:           %.2f tokens/sec\n", tokPerSec);
    fprintf(stderr, "=========================\n");
}

// ============================================================================
// Interactive REPL Mode
// ============================================================================
static void runInteractive(RawrXD::CpuInferenceEngine& engine, const InferenceConfig& cfg) {
    fprintf(stderr, "RawrXD Inference Engine — Interactive Mode\n");
    fprintf(stderr, "Model: %s\n", cfg.modelPath);
    fprintf(stderr, "Type your prompt, press Enter to generate. Ctrl+C to exit.\n\n");

    std::string line;
    while (true) {
        fprintf(stdout, "> ");
        fflush(stdout);

        if (!std::getline(std::cin, line) || line.empty()) {
            if (std::cin.eof()) break;
            continue;
        }

        auto callback = [](const char* token) {
            fputs(token, stdout);
            fflush(stdout);
        };

        std::string result = engine.generate(line, cfg.maxTokens, callback);
        fprintf(stdout, "\n\n");
    }
}

// ============================================================================
// Single-Shot Generation
// ============================================================================
static void runSingleShot(RawrXD::CpuInferenceEngine& engine, const InferenceConfig& cfg) {
    if (cfg.prompt.empty()) {
        fprintf(stderr, "Error: --prompt required for non-interactive mode\n");
        return;
    }

    auto callback = [](const char* token) {
        fputs(token, stdout);
        fflush(stdout);
    };

    std::string result = engine.generate(cfg.prompt, cfg.maxTokens, callback);
    fprintf(stdout, "\n");
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
#ifdef _WIN32
    // Enable UTF-8 console output
    SetConsoleOutputCP(CP_UTF8);
    // Enable virtual terminal processing for ANSI colors
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
    }
#endif

    InferenceConfig cfg;
    if (!parseArgs(argc, argv, cfg)) {
        printUsage(argv[0]);
        return 1;
    }

    if (!cfg.quiet) {
        fprintf(stderr, "[RawrXD] Loading model: %s\n", cfg.modelPath);
    }

    // ---- Load model via CpuInferenceEngine ----
    RawrXD::CpuInferenceEngine engine;

    auto loadStart = std::chrono::high_resolution_clock::now();

    if (!engine.loadModel(cfg.modelPath)) {
        fprintf(stderr, "[ERROR] Failed to load model: %s\n", cfg.modelPath);
        return 2;
    }

    auto loadEnd = std::chrono::high_resolution_clock::now();
    double loadMs = std::chrono::duration<double, std::milli>(loadEnd - loadStart).count();

    if (!cfg.quiet) {
        fprintf(stderr, "[RawrXD] Model loaded in %.1f ms\n", loadMs);
        fprintf(stderr, "[RawrXD] Architecture: %s\n", engine.getArchitecture().c_str());
        fprintf(stderr, "[RawrXD] Parameters: %s\n", engine.getParameterCount().c_str());
        fprintf(stderr, "[RawrXD] Quantization: %s\n", engine.getQuantization().c_str());
    }

    // ---- Configure sampling ----
    engine.setTemperature(cfg.temperature);
    engine.setTopK(cfg.topK);
    engine.setTopP(cfg.topP);
    if (cfg.threads > 0) {
        engine.setThreadCount(cfg.threads);
    }

    // ---- Run mode ----
    if (cfg.benchmark) {
        runBenchmark(engine, cfg);
    } else if (cfg.interactive || cfg.prompt.empty()) {
        runInteractive(engine, cfg);
    } else {
        runSingleShot(engine, cfg);
    }

    return 0;
}
