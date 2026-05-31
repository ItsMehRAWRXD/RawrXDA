#include "cpu_inference_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

// One-shot guard to prevent orchestration re-entry loops
static std::atomic<bool> g_benchmarkCompleted{false};

namespace {

struct CliOptions {
    std::string model_path;
    std::string prompt = "test";
    std::string csv_path;
    int context_tokens = 2048;
    int generate_tokens = 64;
    int threads = 1;
    int runs = 1;
};

struct RunMetrics {
    bool success = false;
    std::string error;
    int prompt_tokens = 0;
    int generated_tokens = 0;
    double total_ms = 0.0;
    double ttft_ms = 0.0;
    double decode_ms = 0.0;
    double decode_tps = 0.0;
    double avg_decode_latency_ms = 0.0;
    double p50_decode_latency_ms = 0.0;
    double p95_decode_latency_ms = 0.0;
};

void PrintUsage() {
    std::printf("RawrXD-DecodeMicroBench (CPUInferenceEngine direct)\n");
    std::printf("Usage:\n");
    std::printf("  RawrXD-DecodeMicroBench --model <path.gguf> [options]\n\n");
    std::printf("Options:\n");
    std::printf("  --prompt <text>      Prompt text (default: \"test\")\n");
    std::printf("  --ctx <n>            Context limit tokens (default: 2048)\n");
    std::printf("  --n <n>              Tokens to generate (default: 64)\n");
    std::printf("  --threads <n>        Engine thread count (default: 1)\n");
    std::printf("  --runs <n>           Number of repeated runs (default: 1)\n");
    std::printf("  --csv <path>         Append machine-readable metrics to CSV\n");
    std::printf("  --help               Show help\n");
}

bool ParsePositiveInt(const char* text, int* out) {
    if (!text || !out) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (!end || *end != '\0' || value <= 0 || value > 1'000'000) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool ParseCli(int argc, char** argv, CliOptions* opts, std::string* error) {
    if (!opts || !error) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        } else if (arg == "--model" && i + 1 < argc) {
            opts->model_path = argv[++i];
        } else if (arg == "--prompt" && i + 1 < argc) {
            opts->prompt = argv[++i];
        } else if (arg == "--ctx" && i + 1 < argc) {
            if (!ParsePositiveInt(argv[++i], &opts->context_tokens)) {
                *error = "invalid --ctx value";
                return false;
            }
        } else if (arg == "--n" && i + 1 < argc) {
            if (!ParsePositiveInt(argv[++i], &opts->generate_tokens)) {
                *error = "invalid --n value";
                return false;
            }
        } else if (arg == "--threads" && i + 1 < argc) {
            if (!ParsePositiveInt(argv[++i], &opts->threads)) {
                *error = "invalid --threads value";
                return false;
            }
        } else if (arg == "--runs" && i + 1 < argc) {
            if (!ParsePositiveInt(argv[++i], &opts->runs)) {
                *error = "invalid --runs value";
                return false;
            }
        } else if (arg == "--csv" && i + 1 < argc) {
            opts->csv_path = argv[++i];
        } else {
            *error = "unknown or incomplete argument: " + arg;
            return false;
        }
    }

    if (opts->model_path.empty()) {
        *error = "--model is required";
        return false;
    }

    return true;
}

std::string DecodeVerdict(double decode_tps) {
    if (decode_tps >= 12.0) {
        return "GO";
    }
    if (decode_tps >= 8.0) {
        return "BORDERLINE";
    }
    return "NO_GO";
}

double PercentileMs(std::vector<double> values, double pct) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double idx = std::clamp(pct, 0.0, 1.0) * static_cast<double>(values.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) {
        return values[lo];
    }
    const double frac = idx - static_cast<double>(lo);
    return values[lo] + (values[hi] - values[lo]) * frac;
}

RunMetrics ExecuteOneRun(const CliOptions& opts, int run_index) {
    RunMetrics metrics;

    auto engine = RawrXD::CPUInferenceEngine::GetSharedInstance();
    if (!engine) {
        metrics.error = "failed to acquire CPUInferenceEngine shared instance";
        return metrics;
    }

    engine->SetThreadCount(opts.threads);
    engine->SetContextLimit(static_cast<size_t>(opts.context_tokens));

    if (!engine->IsModelLoaded()) {
        std::printf("[run %d] loading model: %s\n", run_index, opts.model_path.c_str());
        if (!engine->LoadModel(opts.model_path)) {
            metrics.error = std::string("LoadModel failed: ") + engine->GetLastLoadErrorMessage();
            return metrics;
        }
    }

    const std::vector<int32_t> prompt_tokens = engine->Tokenize(opts.prompt);
    if (prompt_tokens.empty()) {
        metrics.error = "tokenize produced no tokens";
        return metrics;
    }
    metrics.prompt_tokens = static_cast<int>(prompt_tokens.size());    
    std::printf("[DIAG] Tokenized prompt: %d tokens\n", metrics.prompt_tokens);
    std::printf("[DIAG] Starting generation for %d tokens...\n", opts.generate_tokens);
    std::fflush(stdout);
    std::vector<std::chrono::high_resolution_clock::time_point> token_times;
    token_times.reserve(static_cast<size_t>(opts.generate_tokens));

    using clock = std::chrono::high_resolution_clock;
    const auto generation_start = clock::now();

    // Add max-time guard (5 minutes for large models)
    const double max_generation_ms = 300000.0;  // 5 minutes
    bool timeout_exceeded = false;
    size_t last_token_count = 0;
    auto last_progress_time = clock::now();

    try {
        engine->GenerateStreaming(
            prompt_tokens,
            opts.generate_tokens,
            [](const std::string&) {},
            []() {},
            [&](int32_t token_id) {
                token_times.push_back(clock::now());
                last_token_count = token_times.size();
                last_progress_time = clock::now();
                
                // Progress heartbeat every token
                if (token_times.size() <= 5 || token_times.size() % 10 == 0) {
                    auto elapsed_ms = std::chrono::duration<double, std::milli>(
                        token_times.back() - generation_start).count();
                    std::printf("[Progress] token %zu generated (%.1f ms elapsed)\n", 
                                token_times.size(), elapsed_ms);
                    std::fflush(stdout);
                }
            }
        );
    } catch (const std::exception& ex) {
        metrics.error = std::string("GenerateStreaming exception: ") + ex.what();
        std::printf("[ERROR] Exception during generation: %s\n", ex.what());
        return metrics;
    } catch (...) {
        metrics.error = "GenerateStreaming unknown exception";
        std::printf("[ERROR] Unknown exception during generation\n");
        return metrics;
    }

    const auto generation_end = clock::now();
    const double total_elapsed_ms = std::chrono::duration<double, std::milli>(generation_end - generation_start).count();
    
    // Check for timeout
    if (total_elapsed_ms > max_generation_ms) {
        std::printf("[TIMEOUT] Generation exceeded %.0f ms limit\n", max_generation_ms);
        timeout_exceeded = true;
    }

    metrics.generated_tokens = static_cast<int>(token_times.size());
    metrics.total_ms = total_elapsed_ms;

    if (token_times.empty()) {
        metrics.error = "no output tokens generated";
        std::printf("[STALL] Forward pass produced no tokens after %.1f ms\n", total_elapsed_ms);
        return metrics;
    }

    metrics.ttft_ms = std::chrono::duration<double, std::milli>(token_times.front() - generation_start).count();

    std::vector<double> decode_latencies_ms;
    decode_latencies_ms.reserve(token_times.size() > 1 ? token_times.size() - 1 : 0);
    for (size_t i = 1; i < token_times.size(); ++i) {
        const double dt = std::chrono::duration<double, std::milli>(token_times[i] - token_times[i - 1]).count();
        decode_latencies_ms.push_back(dt);
    }

    if (!decode_latencies_ms.empty()) {
        metrics.decode_ms = std::chrono::duration<double, std::milli>(token_times.back() - token_times.front()).count();
        const double decode_token_count = static_cast<double>(decode_latencies_ms.size());
        metrics.decode_tps = (metrics.decode_ms > 0.0) ? (decode_token_count * 1000.0 / metrics.decode_ms) : 0.0;
        metrics.avg_decode_latency_ms = std::accumulate(decode_latencies_ms.begin(), decode_latencies_ms.end(), 0.0) /
                                        static_cast<double>(decode_latencies_ms.size());
        metrics.p50_decode_latency_ms = PercentileMs(decode_latencies_ms, 0.50);
        metrics.p95_decode_latency_ms = PercentileMs(decode_latencies_ms, 0.95);
    } else {
        metrics.decode_ms = 0.0;
        metrics.decode_tps = 0.0;
        metrics.avg_decode_latency_ms = 0.0;
        metrics.p50_decode_latency_ms = 0.0;
        metrics.p95_decode_latency_ms = 0.0;
    }

    metrics.success = true;
    return metrics;
}

void MaybeAppendCsv(const CliOptions& opts, const RunMetrics& m, int run_index) {
    if (opts.csv_path.empty()) {
        return;
    }

    std::ofstream out(opts.csv_path, std::ios::app);
    if (!out.is_open()) {
        std::fprintf(stderr, "warning: unable to open csv path: %s\n", opts.csv_path.c_str());
        return;
    }

    if (out.tellp() == 0) {
        out << "run,ctx,n,threads,prompt_tokens,generated_tokens,total_ms,ttft_ms,decode_ms,decode_tps,avg_decode_latency_ms,p50_decode_latency_ms,p95_decode_latency_ms,verdict\n";
    }

    out << run_index << ","
        << opts.context_tokens << ","
        << opts.generate_tokens << ","
        << opts.threads << ","
        << m.prompt_tokens << ","
        << m.generated_tokens << ","
        << m.total_ms << ","
        << m.ttft_ms << ","
        << m.decode_ms << ","
        << m.decode_tps << ","
        << m.avg_decode_latency_ms << ","
        << m.p50_decode_latency_ms << ","
        << m.p95_decode_latency_ms << ","
        << DecodeVerdict(m.decode_tps)
        << "\n";
}

void PrintRunSummary(const RunMetrics& m, int run_index) {
    std::printf("\n[run %d] generated_tokens = %d\n", run_index, m.generated_tokens);
    std::printf("ttft time = %.2f ms\n", m.ttft_ms);
    if (m.generated_tokens >= 2) {
        const int decode_tokens = m.generated_tokens - 1;
        std::printf("eval time = %.2f ms / %d tokens (%.2f tokens per second)\n",
                    m.decode_ms,
                    decode_tokens,
                    m.decode_tps);
    } else {
        std::printf("eval time = 0.00 ms / 0 tokens (0.00 tokens per second)\n");
    }
    std::printf("decode latency (avg/p50/p95) = %.2f / %.2f / %.2f ms\n",
                m.avg_decode_latency_ms,
                m.p50_decode_latency_ms,
                m.p95_decode_latency_ms);
    std::printf("verdict = %s\n", DecodeVerdict(m.decode_tps).c_str());
}

}  // namespace

int main(int argc, char** argv) {
    // Prevent orchestration re-entry loops
    if (g_benchmarkCompleted.exchange(true)) {
        std::fprintf(stderr, "[GUARD] Benchmark already completed - preventing re-entry\n");
        return 0;
    }
    
    CliOptions opts;
    std::string parse_error;
    if (!ParseCli(argc, argv, &opts, &parse_error)) {
        std::fprintf(stderr, "error: %s\n\n", parse_error.c_str());
        PrintUsage();
        return 2;
    }

    std::printf("RawrXD Decode MicroBench\n");
    std::printf("model=%s\n", opts.model_path.c_str());
    std::printf("ctx=%d n=%d threads=%d runs=%d\n",
                opts.context_tokens,
                opts.generate_tokens,
                opts.threads,
                opts.runs);

    int failures = 0;
    std::vector<RunMetrics> results;
    results.reserve(static_cast<size_t>(opts.runs));

    for (int run = 1; run <= opts.runs; ++run) {
        RunMetrics m = ExecuteOneRun(opts, run);
        if (!m.success) {
            ++failures;
            std::fprintf(stderr, "[run %d] error: %s\n", run, m.error.c_str());
        } else {
            PrintRunSummary(m, run);
            MaybeAppendCsv(opts, m, run);
        }
        results.push_back(m);
    }

    if (results.empty()) {
        std::fprintf(stderr, "no runs executed\n");
        return 3;
    }

    double sum_decode_tps = 0.0;
    int ok_runs = 0;
    for (const RunMetrics& m : results) {
        if (m.success && m.generated_tokens >= 2) {
            sum_decode_tps += m.decode_tps;
            ++ok_runs;
        }
    }

    if (ok_runs > 0) {
        const double mean_decode_tps = sum_decode_tps / static_cast<double>(ok_runs);
        std::printf("\nmean decode throughput = %.2f tokens per second over %d successful runs\n",
                    mean_decode_tps,
                    ok_runs);
        std::printf("final verdict = %s\n", DecodeVerdict(mean_decode_tps).c_str());
    }

    return (failures == 0) ? 0 : 1;
}
