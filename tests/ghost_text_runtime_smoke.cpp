// ============================================================================
// ghost_text_runtime_smoke.cpp
// Headless ghost-text runtime smoke target (no HWND / no rendering dependency)
//
// Stresses:
//   - high-frequency completion dispatch
//   - newest-request-wins cancellation
//   - fallback behavior when provider unavailable
//   - lock contention / recursive lock depth telemetry
//
// Emits summary + machine-readable line:
//   RAWRXD_GHOST_SOAK_JSON={...}
// ============================================================================

#include "../src/agentic/NativeStreamProvider.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Args {
    int iterations = 1200;
    int maxInflight = 32;
    int timeoutMs = 1500;
    int simulateWorkUs = 0;
    std::string baseUrl = "http://localhost:11435";
    bool verbose = false;
};

struct Metrics {
    std::atomic<uint64_t> ghostRequests{0};
    std::atomic<uint64_t> ghostCancels{0};
    std::atomic<uint64_t> ghostTimeouts{0};
    std::atomic<uint64_t> ghostFallbacks{0};
    std::atomic<uint64_t> ghostStaleDrops{0};
    std::atomic<uint64_t> nativeHits{0};
    std::atomic<uint64_t> snippetHits{0};
    std::atomic<uint64_t> queueDepth{0};
    std::atomic<uint64_t> maxQueueDepth{0};
    std::atomic<uint64_t> lockAcquireCount{0};
    std::atomic<uint64_t> lockWaitTotalUs{0};
    std::atomic<uint64_t> maxRecursionDepth{0};
    std::atomic<uint64_t> latencyTotalUs{0};
    std::atomic<uint64_t> latencySamples{0};
    std::atomic<uint64_t> latencyMaxUs{0};
};

struct SharedState {
    std::recursive_mutex lock;
    std::atomic<uint64_t> latestGeneration{0};
    bool providerAvailable = false;
};

thread_local uint64_t t_recursionDepth = 0;

class ScopedRecursiveTelemetryLock {
  public:
    ScopedRecursiveTelemetryLock(std::recursive_mutex& m, Metrics& metrics)
        : m_(m), metrics_(metrics), start_(std::chrono::steady_clock::now()) {
        m_.lock();
        const auto end = std::chrono::steady_clock::now();
        const auto waitUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        metrics_.lockAcquireCount.fetch_add(1, std::memory_order_relaxed);
        metrics_.lockWaitTotalUs.fetch_add(static_cast<uint64_t>(std::max<int64_t>(0, waitUs)),
                                           std::memory_order_relaxed);

        ++t_recursionDepth;
        uint64_t prev = metrics_.maxRecursionDepth.load(std::memory_order_relaxed);
        while (t_recursionDepth > prev &&
               !metrics_.maxRecursionDepth.compare_exchange_weak(prev, t_recursionDepth,
                                                                 std::memory_order_relaxed,
                                                                 std::memory_order_relaxed)) {
        }
    }

    ~ScopedRecursiveTelemetryLock() {
        if (t_recursionDepth > 0) {
            --t_recursionDepth;
        }
        m_.unlock();
    }

  private:
    std::recursive_mutex& m_;
    Metrics& metrics_;
    std::chrono::steady_clock::time_point start_;
};

void updateMax(std::atomic<uint64_t>& dst, uint64_t value) {
    uint64_t prev = dst.load(std::memory_order_relaxed);
    while (value > prev && !dst.compare_exchange_weak(prev, value, std::memory_order_relaxed,
                                                      std::memory_order_relaxed)) {
    }
}

std::string snippetFallback(const std::string& prefix) {
    if (prefix.find("std::") != std::string::npos) {
        return "vector<int> values;";
    }
    if (prefix.find("for (") != std::string::npos) {
        return "int i = 0; i < n; ++i) {\n    \n}";
    }
    if (prefix.find("if (") != std::string::npos) {
        return "condition) {\n    \n}";
    }
    return "// completion";
}

Args parseArgs(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto readInt = [&](int& out) {
            if (i + 1 < argc) {
                try {
                    out = std::stoi(argv[++i]);
                } catch (...) {
                }
            }
        };
        auto readString = [&](std::string& out) {
            if (i + 1 < argc) {
                out = argv[++i];
            }
        };

        if (a == "--iterations") {
            readInt(args.iterations);
        } else if (a == "--max-inflight") {
            readInt(args.maxInflight);
        } else if (a == "--timeout-ms") {
            readInt(args.timeoutMs);
        } else if (a == "--simulate-work-us") {
            readInt(args.simulateWorkUs);
        } else if (a == "--base-url") {
            readString(args.baseUrl);
        } else if (a == "--verbose") {
            args.verbose = true;
        }
    }

    args.iterations = std::max(1, args.iterations);
    args.maxInflight = std::max(1, args.maxInflight);
    args.timeoutMs = std::max(100, args.timeoutMs);
    args.simulateWorkUs = std::max(0, args.simulateWorkUs);
    return args;
}

std::string runSingleRequest(uint64_t generation, const std::string& prefix,
                             RawrXD::Prediction::NativeStreamProvider& provider,
                             const Args& args,
                             Metrics& metrics,
                             SharedState& state) {
    const auto started = std::chrono::steady_clock::now();
    metrics.ghostRequests.fetch_add(1, std::memory_order_relaxed);

    const uint64_t q = metrics.queueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
    updateMax(metrics.maxQueueDepth, q);

    std::string completion;

    auto finalize = [&]() {
        const auto ended = std::chrono::steady_clock::now();
        const uint64_t elapsedUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(ended - started).count());
        metrics.latencyTotalUs.fetch_add(elapsedUs, std::memory_order_relaxed);
        metrics.latencySamples.fetch_add(1, std::memory_order_relaxed);
        updateMax(metrics.latencyMaxUs, elapsedUs);

        if (elapsedUs > static_cast<uint64_t>(args.timeoutMs) * 1000ULL) {
            metrics.ghostTimeouts.fetch_add(1, std::memory_order_relaxed);
        }

        metrics.queueDepth.fetch_sub(1, std::memory_order_relaxed);
    };

    if (args.simulateWorkUs > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(args.simulateWorkUs));
    }

    RawrXD::Prediction::PredictionContext ctx;
    ctx.prefix = prefix;
    ctx.suffix = "\n";
    ctx.filePath = "ghost_text_runtime_smoke.cpp";
    ctx.language = "cpp";
    ctx.cursorLine = 1;
    ctx.cursorColumn = static_cast<int>(prefix.size());

    bool providerAvailable = false;
    {
        ScopedRecursiveTelemetryLock lk(state.lock, metrics);
        providerAvailable = state.providerAvailable;
    }

    if (providerAvailable) {
        std::atomic<bool> cancelled{false};
        provider.PredictStreaming(
            ctx,
            [&](const std::string& token, bool) -> bool {
                const uint64_t latest = state.latestGeneration.load(std::memory_order_acquire);
                if (generation != latest) {
                    cancelled.store(true, std::memory_order_release);
                    metrics.ghostCancels.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                if (!token.empty()) {
                    completion += token;
                }
                return true;
            });

        if (!cancelled.load(std::memory_order_acquire)) {
            const uint64_t latest = state.latestGeneration.load(std::memory_order_acquire);
            if (generation != latest) {
                metrics.ghostStaleDrops.fetch_add(1, std::memory_order_relaxed);
                completion.clear();
            }
        }

        if (!completion.empty()) {
            metrics.nativeHits.fetch_add(1, std::memory_order_relaxed);
            finalize();
            return completion;
        }
    }

    metrics.ghostFallbacks.fetch_add(1, std::memory_order_relaxed);
    {
        const uint64_t latest = state.latestGeneration.load(std::memory_order_acquire);
        if (generation != latest) {
            metrics.ghostStaleDrops.fetch_add(1, std::memory_order_relaxed);
            metrics.ghostCancels.fetch_add(1, std::memory_order_relaxed);
            finalize();
            return "";
        }
    }
    metrics.snippetHits.fetch_add(1, std::memory_order_relaxed);
    completion = snippetFallback(prefix);
    finalize();
    return completion;
}

std::string toJson(const Metrics& m, const Args& a, bool pass) {
    const uint64_t samples = m.latencySamples.load(std::memory_order_relaxed);
    const uint64_t avgUs = samples == 0 ? 0 : m.latencyTotalUs.load(std::memory_order_relaxed) / samples;

    std::ostringstream oss;
    oss << "{";
    oss << "\"pass\":" << (pass ? "true" : "false") << ",";
    oss << "\"iterations\":" << a.iterations << ",";
    oss << "\"max_inflight\":" << a.maxInflight << ",";
    oss << "\"timeout_ms\":" << a.timeoutMs << ",";
    oss << "\"simulate_work_us\":" << a.simulateWorkUs << ",";
    oss << "\"requests\":" << m.ghostRequests.load(std::memory_order_relaxed) << ",";
    oss << "\"cancels\":" << m.ghostCancels.load(std::memory_order_relaxed) << ",";
    oss << "\"timeouts\":" << m.ghostTimeouts.load(std::memory_order_relaxed) << ",";
    oss << "\"fallbacks\":" << m.ghostFallbacks.load(std::memory_order_relaxed) << ",";
    oss << "\"stale_drops\":" << m.ghostStaleDrops.load(std::memory_order_relaxed) << ",";
    oss << "\"native_hits\":" << m.nativeHits.load(std::memory_order_relaxed) << ",";
    oss << "\"snippet_hits\":" << m.snippetHits.load(std::memory_order_relaxed) << ",";
    oss << "\"max_queue_depth\":" << m.maxQueueDepth.load(std::memory_order_relaxed) << ",";
    oss << "\"avg_latency_us\":" << avgUs << ",";
    oss << "\"max_latency_us\":" << m.latencyMaxUs.load(std::memory_order_relaxed) << ",";
    oss << "\"lock_acquires\":" << m.lockAcquireCount.load(std::memory_order_relaxed) << ",";
    oss << "\"lock_wait_total_us\":" << m.lockWaitTotalUs.load(std::memory_order_relaxed) << ",";
    oss << "\"max_recursion_depth\":" << m.maxRecursionDepth.load(std::memory_order_relaxed);
    oss << "}";
    return oss.str();
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    Metrics metrics;
    SharedState state;

    RawrXD::Prediction::NativeStreamProvider provider(args.baseUrl);
    RawrXD::Prediction::PredictionConfig cfg;
    cfg.model = "qwen2.5-coder:14b";
    cfg.temperature = 0.2f;
    cfg.maxTokens = 96;
    cfg.maxLines = 8;
    cfg.useFIM = true;
    provider.Configure(cfg);

    {
        ScopedRecursiveTelemetryLock lk(state.lock, metrics);
        state.providerAvailable = provider.IsAvailable();
    }

    std::vector<std::string> prefixes = {
        "std::", "for (", "if (", "class Worker { public:", "auto result =",
        "template <typename T>", "void process() {", "switch (state)", "while (running)",
        "try {"};

    std::mt19937 rng(1337u);
    std::uniform_int_distribution<int> prefixDist(0, static_cast<int>(prefixes.size() - 1));
    std::uniform_int_distribution<int> jitterDist(0, 3);

    std::vector<std::future<std::string>> inFlight;
    inFlight.reserve(static_cast<size_t>(args.maxInflight * 2));

    for (int i = 0; i < args.iterations; ++i) {
        const uint64_t generation = static_cast<uint64_t>(i + 1);
        state.latestGeneration.store(generation, std::memory_order_release);

        std::string prefix = prefixes[static_cast<size_t>(prefixDist(rng))] + " // req=" + std::to_string(i);

        inFlight.emplace_back(std::async(std::launch::async, [&provider, &args, &metrics, &state, generation, prefix]() {
            return runSingleRequest(generation, prefix, provider, args, metrics, state);
        }));

        if (static_cast<int>(inFlight.size()) >= args.maxInflight) {
            (void)inFlight.front().get();
            inFlight.erase(inFlight.begin());
        }

        const int jitter = jitterDist(rng);
        if (jitter > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(jitter));
        }
    }

    for (auto& f : inFlight) {
        (void)f.get();
    }

    const uint64_t requests = metrics.ghostRequests.load(std::memory_order_relaxed);
    const uint64_t samples = metrics.latencySamples.load(std::memory_order_relaxed);
    const uint64_t avgLatencyUs = samples == 0 ? 0 : metrics.latencyTotalUs.load(std::memory_order_relaxed) / samples;

    const bool pass = (requests == static_cast<uint64_t>(args.iterations)) &&
                      (metrics.maxRecursionDepth.load(std::memory_order_relaxed) <= 8) &&
                      (avgLatencyUs <= static_cast<uint64_t>(args.timeoutMs) * 1000ULL * 2ULL);

    std::cout << "[GhostSoak] provider_available="
              << (state.providerAvailable ? "yes" : "no")
              << " requests=" << requests
              << " cancels=" << metrics.ghostCancels.load(std::memory_order_relaxed)
              << " stale_drops=" << metrics.ghostStaleDrops.load(std::memory_order_relaxed)
              << " fallbacks=" << metrics.ghostFallbacks.load(std::memory_order_relaxed)
              << " avg_latency_us=" << avgLatencyUs
              << " max_latency_us=" << metrics.latencyMaxUs.load(std::memory_order_relaxed)
              << " max_queue_depth=" << metrics.maxQueueDepth.load(std::memory_order_relaxed)
              << " max_recursion_depth=" << metrics.maxRecursionDepth.load(std::memory_order_relaxed)
              << " => " << (pass ? "PASS" : "FAIL")
              << "\n";

    std::cout << "RAWRXD_GHOST_SOAK_JSON=" << toJson(metrics, args, pass) << "\n";
    return pass ? 0 : 1;
}
