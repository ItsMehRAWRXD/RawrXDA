// ============================================================================
// inference_standalone_main.cpp — RawrXD-InferenceEngine Standalone Entry
// ============================================================================
//
// Self-hosting inference engine: no Qt, no GUI, no IDE dependencies.
// This is the Phase 6 "Inference-Standalone" endpoint.
//
// Usage:
//   RawrXD-InferenceEngine.exe --model <path.gguf> [--prompt "..."] [--bench]
//
// The T4 Autonomous Recovery Orchestrator can invoke this binary directly
// for reasoning tasks during self-repair operations.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <chrono>
#include <vector>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ultra_fast_inference.h"

// ============================================================================
// Forward declarations — these live in the inference core sources
// ============================================================================
namespace rawrxd { namespace inference {
    class UltraFastInferenceEngine;
} }

// ============================================================================
// Telemetry integration (MASM kernel)
// ============================================================================
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
extern "C" {
    void UTC_LogEvent(const char* msg);
    void UTC_IncrementCounter(void* counter);
    uint64_t UTC_ReadCounter(void* counter);
}
#else
static inline void UTC_LogEvent(const char*) {}
#endif

// ============================================================================
// CLI argument parser (minimal, no dependencies)
// ============================================================================
struct InferenceCLI {
    std::string modelPath;
    std::string prompt;
    int maxTokens       = 256;
    float temperature   = 0.7f;
    bool benchmark      = false;
    bool interactive    = false;
    bool verbose        = false;

    static InferenceCLI parse(int argc, char* argv[]) {
        InferenceCLI cli;
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
                cli.modelPath = argv[++i];
            } else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) {
                cli.prompt = argv[++i];
            } else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) {
                cli.maxTokens = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--temperature") == 0 && i + 1 < argc) {
                cli.temperature = static_cast<float>(atof(argv[++i]));
            } else if (strcmp(argv[i], "--bench") == 0) {
                cli.benchmark = true;
            } else if (strcmp(argv[i], "--interactive") == 0) {
                cli.interactive = true;
            } else if (strcmp(argv[i], "--verbose") == 0) {
                cli.verbose = true;
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                printf("RawrXD-InferenceEngine v1.0.0 — Standalone Inference\n");
                printf("  --model <path>       Path to GGUF model file\n");
                printf("  --prompt <text>      Prompt text for inference\n");
                printf("  --max-tokens <n>     Maximum tokens to generate (default: 256)\n");
                printf("  --temperature <f>    Sampling temperature (default: 0.7)\n");
                printf("  --bench              Run TPS benchmark\n");
                printf("  --interactive        Interactive REPL mode\n");
                printf("  --verbose            Verbose telemetry output\n");
                exit(0);
            }
        }
        return cli;
    }
};

// ============================================================================
// TPS Benchmark
// ============================================================================
struct BenchmarkResult {
    double tokensPerSecond;
    double timeToFirstTokenMs;
    int totalTokens;
    double totalTimeMs;
};

static BenchmarkResult runBenchmark(const InferenceCLI& cli) {
    BenchmarkResult result = {};

    printf("[Benchmark] Model: %s\n", cli.modelPath.c_str());
    printf("[Benchmark] Max tokens: %d, Temperature: %.2f\n",
           cli.maxTokens, cli.temperature);

    auto start = std::chrono::high_resolution_clock::now();

    // Load model via polymorphic_loader → ultra_fast_inference
    rawrxd::inference::AutonomousInferenceEngine::InferenceConfig config;
    config.max_memory_mb = 0; // Auto-detect
    config.quality_target = 0.8f;
    config.enable_streaming_pruning = true;
    config.enable_hotpatching = true;
    config.enable_gpu = true;

    rawrxd::inference::AutonomousInferenceEngine engine(config);

    if (!engine.loadModelAutomatic(cli.modelPath)) {
        printf("[Benchmark] ERROR: Failed to load model: %s\n", cli.modelPath.c_str());
        result.tokensPerSecond = 0;
        return result;
    }

    // Tokenize prompt (simplified: one token per word)
    std::vector<int32_t> prompt_tokens;
    std::istringstream iss(cli.prompt.empty() ? "Hello world" : cli.prompt);
    std::string word;
    while (iss >> word) {
        // Simple hash-based token ID
        uint32_t h = 0;
        for (char c : word) h = h * 31 + static_cast<uint32_t>(c);
        prompt_tokens.push_back(static_cast<int32_t>(h % 32000));
    }

    // Run inference with token callback
    int tokens = 0;
    auto firstTokenTime = start;
    bool firstToken = false;

    engine.infer(prompt_tokens, [&](const std::string& token) {
        if (!firstToken) {
            firstTokenTime = std::chrono::high_resolution_clock::now();
            firstToken = true;
        }
        tokens++;
        if (cli.interactive) {
            printf("%s", token.c_str());
            fflush(stdout);
        }
    }, cli.maxTokens);

    auto end = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    double ttftMs  = std::chrono::duration<double, std::milli>(firstTokenTime - start).count();

    result.totalTokens = tokens;
    result.totalTimeMs = totalMs;
    result.timeToFirstTokenMs = ttftMs;
    result.tokensPerSecond = (totalMs > 0) ? (tokens * 1000.0 / totalMs) : 0;

    printf("[Benchmark] Results:\n");
    printf("  Tokens generated: %d\n", result.totalTokens);
    printf("  Total time:       %.2f ms\n", result.totalTimeMs);
    printf("  Time to first:    %.2f ms\n", result.timeToFirstTokenMs);
    printf("  Throughput:       %.2f tok/s\n", result.tokensPerSecond);

    return result;
}

// ============================================================================
// Interactive REPL
// ============================================================================
static void runInteractive(const InferenceCLI& cli) {
    printf("RawrXD-InferenceEngine Interactive Mode\n");
    printf("Model: %s\n", cli.modelPath.c_str());
    printf("Type 'exit' or 'quit' to stop.\n\n");

    // Load the inference engine once for the session
    rawrxd::inference::AutonomousInferenceEngine::InferenceConfig config;
    config.max_memory_mb = 0; // auto-detect
    config.quality_target = 0.8f;
    config.enable_streaming_pruning = true;
    config.enable_hotpatching = true;
    config.enable_gpu = true;

    rawrxd::inference::AutonomousInferenceEngine engine(config);
    bool modelLoaded = false;

    if (!cli.modelPath.empty()) {
        printf("[Interactive] Loading model: %s\n", cli.modelPath.c_str());
        modelLoaded = engine.loadModelAutomatic(cli.modelPath);
        if (modelLoaded) {
            printf("[Interactive] Model loaded successfully.\n\n");
        } else {
            printf("[Interactive] WARNING: Failed to load model. Inference unavailable.\n\n");
        }
    }

    char lineBuf[4096];
    while (true) {
        printf(">>> ");
        fflush(stdout);

        if (!fgets(lineBuf, sizeof(lineBuf), stdin)) break;

        // Trim trailing newline
        size_t len = strlen(lineBuf);
        while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r'))
            lineBuf[--len] = '\0';

        if (strcmp(lineBuf, "exit") == 0 || strcmp(lineBuf, "quit") == 0)
            break;

        if (len == 0) continue;

        if (!modelLoaded) {
            printf("[Inference] ERROR: No model loaded. Start with --model <path>\n\n");
            continue;
        }

        // Tokenize prompt
        std::vector<int32_t> promptTokens;
        std::istringstream iss(lineBuf);
        std::string word;
        while (iss >> word) {
            uint32_t h = 0;
            for (char c : word) h = h * 31 + (uint32_t)c;
            promptTokens.push_back((int32_t)(h % 32000));
        }

        // Run inference, streaming tokens to stdout
        auto start = std::chrono::high_resolution_clock::now();
        int tokenCount = 0;

        engine.infer(promptTokens, [&](const std::string& token) {
            printf("%s", token.c_str());
            fflush(stdout);
            tokenCount++;
        }, cli.maxTokens);

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double tps = (ms > 0) ? (tokenCount * 1000.0 / ms) : 0;

        printf("\n[%d tokens, %.1f ms, %.1f tok/s]\n\n", tokenCount, ms, tps);
    }
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    InferenceCLI cli = InferenceCLI::parse(argc, argv);

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
    UTC_LogEvent("[InferenceEngine] Standalone entry point initialized");
#endif

    if (cli.modelPath.empty() && !cli.benchmark) {
        printf("RawrXD-InferenceEngine v1.0.0\n");
        printf("Error: --model <path> is required (or use --bench for benchmarking)\n");
        printf("Run with --help for usage.\n");
        return 1;
    }

    if (cli.verbose) {
        printf("[Config] Model:       %s\n", cli.modelPath.c_str());
        printf("[Config] MaxTokens:   %d\n", cli.maxTokens);
        printf("[Config] Temperature: %.2f\n", cli.temperature);
        printf("[Config] Benchmark:   %s\n", cli.benchmark ? "yes" : "no");
        printf("[Config] Interactive: %s\n", cli.interactive ? "yes" : "no");
    }

    if (cli.benchmark) {
        BenchmarkResult bench = runBenchmark(cli);
        (void)bench;
        return 0;
    }

    if (cli.interactive) {
        runInteractive(cli);
        return 0;
    }

    // Single-shot inference
    if (!cli.prompt.empty()) {
        printf("[Inference] Prompt: %s\n", cli.prompt.c_str());
        printf("[Inference] Generating up to %d tokens...\n", cli.maxTokens);

        rawrxd::inference::AutonomousInferenceEngine::InferenceConfig config;
        config.max_memory_mb = 0;
        config.quality_target = 0.8f;
        config.enable_streaming_pruning = true;
        config.enable_hotpatching = true;
        config.enable_gpu = true;

        rawrxd::inference::AutonomousInferenceEngine engine(config);
        if (engine.loadModelAutomatic(cli.modelPath)) {
            std::vector<int32_t> promptTokens;
            std::istringstream piss(cli.prompt);
            std::string w;
            while (piss >> w) {
                uint32_t h = 0;
                for (char c : w) h = h * 31 + (uint32_t)c;
                promptTokens.push_back((int32_t)(h % 32000));
            }

            int tokenCount = 0;
            engine.infer(promptTokens, [&](const std::string& token) {
                printf("%s", token.c_str());
                fflush(stdout);
                tokenCount++;
            }, cli.maxTokens);
            printf("\n[%d tokens generated]\n", tokenCount);
        } else {
            printf("[Inference] ERROR: Failed to load model: %s\n", cli.modelPath.c_str());
        }
    }

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
    UTC_LogEvent("[InferenceEngine] Standalone exit");
#endif

    return 0;
}
