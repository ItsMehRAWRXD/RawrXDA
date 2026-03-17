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
//   --threads N           CPU threads (default: auto)
//   --interactive         Interactive REPL mode
//   --benchmark           Run throughput benchmark
//   --quiet               Suppress info logging (tokens only)
//
// Architecture: C++20, no exceptions, PatchResult pattern.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "cpu_inference_engine.h"

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
// Generate from prompt string using CPUInferenceEngine's streaming API
// ============================================================================
static int generateFromPrompt(RawrXD::CPUInferenceEngine& engine,
                               const std::string& prompt,
                               int maxTokens,
                               bool quiet)
{
    // Tokenize the prompt
    std::vector<int32_t> tokens = engine.Tokenize(prompt);
    if (tokens.empty()) {
        fprintf(stderr, "[ERROR] Tokenization produced empty sequence\n");
        return 0;
    }

    if (!quiet) {
        fprintf(stderr, "[RawrXD] Prompt tokenized: %zu tokens\n", tokens.size());
    }

    std::atomic<int> tokenCount{0};

    // Use streaming generation with token callback
    engine.GenerateStreaming(
        tokens,
        maxTokens,
        // token_callback: receives decoded text
        [&](const std::string& tokenText) {
            fputs(tokenText.c_str(), stdout);
            fflush(stdout);
            tokenCount.fetch_add(1);
        },
        // complete_callback
        []() {}
    );

    return tokenCount.load();
}

// ============================================================================
// Benchmark Mode
// ============================================================================
static void runBenchmark(RawrXD::CPUInferenceEngine& engine, const InferenceConfig& cfg) {
    const char* benchPrompt = "The quick brown fox jumps over the lazy dog. "
                               "In a world where artificial intelligence has become ";

    fprintf(stderr, "\n=== RawrXD Inference Engine Benchmark ===\n");
    fprintf(stderr, "Model: %s\n", cfg.modelPath);
    fprintf(stderr, "Tokens: %d\n", cfg.maxTokens);
    fprintf(stderr, "Threads: %d\n", cfg.threads > 0 ? cfg.threads : engine.GetThreadCount());
    fprintf(stderr, "\nGenerating...\n\n");

    auto startTime = std::chrono::high_resolution_clock::now();

    int tokenCount = generateFromPrompt(engine, benchPrompt, cfg.maxTokens, cfg.quiet);

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    double tokPerSec = (tokenCount > 0 && totalMs > 0)
        ? (tokenCount / (totalMs / 1000.0))
        : 0.0;

    fprintf(stderr, "\n\n=== Benchmark Results ===\n");
    fprintf(stderr, "Tokens generated:     %d\n", tokenCount);
    fprintf(stderr, "Total time:           %.2f ms\n", totalMs);
    fprintf(stderr, "Throughput:           %.2f tokens/sec\n", tokPerSec);
    fprintf(stderr, "=========================\n");
}

// ============================================================================
// Interactive REPL Mode
// ============================================================================
static void runInteractive(RawrXD::CPUInferenceEngine& engine, const InferenceConfig& cfg) {
    fprintf(stderr, "RawrXD Inference Engine — Interactive Mode\n");
    fprintf(stderr, "Model: %s\n", cfg.modelPath);
    fprintf(stderr, "Vocab: %d | Dim: %d | Layers: %d | Heads: %d\n",
            engine.GetVocabSize(), engine.GetEmbeddingDim(),
            engine.GetNumLayers(), engine.GetNumHeads());
    fprintf(stderr, "Type your prompt, press Enter to generate. Ctrl+C to exit.\n\n");

    std::string line;
    while (true) {
        fprintf(stdout, "> ");
        fflush(stdout);

        if (!std::getline(std::cin, line) || line.empty()) {
            if (std::cin.eof()) break;
            continue;
        }

        generateFromPrompt(engine, line, cfg.maxTokens, false);
        fprintf(stdout, "\n\n");
    }
}

// ============================================================================
// Single-Shot Generation
// ============================================================================
static void runSingleShot(RawrXD::CPUInferenceEngine& engine, const InferenceConfig& cfg) {
    if (cfg.prompt.empty()) {
        fprintf(stderr, "Error: --prompt required for non-interactive mode\n");
        return;
    }

    generateFromPrompt(engine, cfg.prompt, cfg.maxTokens, cfg.quiet);
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

    // ---- Load model via CPUInferenceEngine ----
    RawrXD::CPUInferenceEngine engine;

    if (cfg.threads > 0) {
        engine.SetThreadCount(cfg.threads);
    }

    auto loadStart = std::chrono::high_resolution_clock::now();

    if (!engine.LoadModel(cfg.modelPath)) {
        fprintf(stderr, "[ERROR] Failed to load model: %s\n", cfg.modelPath);
        return 2;
    }

    auto loadEnd = std::chrono::high_resolution_clock::now();
    double loadMs = std::chrono::duration<double, std::milli>(loadEnd - loadStart).count();

    if (!cfg.quiet) {
        fprintf(stderr, "[RawrXD] Model loaded in %.1f ms\n", loadMs);
        fprintf(stderr, "[RawrXD] Vocab: %d | Embed: %d | Layers: %d | Heads: %d\n",
                engine.GetVocabSize(), engine.GetEmbeddingDim(),
                engine.GetNumLayers(), engine.GetNumHeads());
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
