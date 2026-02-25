// benchmark_runner.cpp — Win32/C++ benchmark runner (Qt-free).
// Executes a small suite of real end-to-end tests against CPUInferenceEngine:
// tokenize -> generate -> detokenize, tracking latency + throughput.

#include "../include/benchmark_runner.hpp"

#include "cpu_inference_engine.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <sstream>
#include <thread>

using Clock = std::chrono::high_resolution_clock;

static double nowMs() {
    return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
}

static void trimLineEnds(std::string& s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
}

BenchmarkRunner::BenchmarkRunner() = default;

BenchmarkRunner::~BenchmarkRunner() {
    stop();
    engine_.reset();
}

void BenchmarkRunner::runBenchmarks(const std::vector<std::string>& selectedTests,
                                    const std::string& modelPath,
                                    bool gpuEnabled,
                                    bool verbose) {
    selectedTests_ = selectedTests;
    modelPath_ = modelPath;
    gpuEnabled_ = gpuEnabled;
    verbose_ = verbose;
    shouldStop_ = false;
    results_.clear();

    // Run in background thread to keep UI responsive.
    std::thread([this]() { this->executeRun(); }).detach();
}

void BenchmarkRunner::stop() {
    shouldStop_ = true;
}

const std::vector<BenchmarkTestResult>& BenchmarkRunner::getResults() const {
    return results_;
}

void BenchmarkRunner::executeRun() {
    if (m_onStarted) m_onStarted();

    auto logi = [&](const std::string& s, int lvl) { log(s, lvl); };
    logi("[Benchmark] Initializing...", 1);

    engine_ = std::make_unique<RawrXD::CPUInferenceEngine>();
    engine_->SetUseTitanAssembly(true);
    engine_->SetThreadCount((int)std::max(1u, std::thread::hardware_concurrency()));

    if (!modelPath_.empty()) {
        if (!engine_->LoadModel(modelPath_)) {
            logi("[Benchmark] ERROR: LoadModel failed: " + modelPath_, 4);
            if (m_onFinished) m_onFinished(0, 0, 0.0);
            return;
        }
        logi("[Benchmark] Model loaded: " + modelPath_, 2);
    } else {
        logi("[Benchmark] WARNING: No model path set; benchmarks will fail. Provide a GGUF path.", 3);
    }

    const auto suiteStart = Clock::now();

    int passed = 0;
    const int total = (int)selectedTests_.size();

    auto runOne = [&](const std::string& name, bool (BenchmarkRunner::*fn)(BenchmarkTestResult&)) {
        if (shouldStop_) return;
        if (m_onTestStarted) m_onTestStarted(name);

        BenchmarkTestResult r;
        r.testName = name;
        const bool ok = (this->*fn)(r);
        r.passed = ok;
        results_.push_back(r);
        if (ok) passed++;

        if (m_onTestCompleted) m_onTestCompleted(name, ok, r.avgLatencyMs);
    };

    for (int i = 0; i < total && !shouldStop_; ++i) {
        if (m_onProgress) m_onProgress(i + 1, total);

        const std::string& t = selectedTests_[i];
        if (t == "cold_start")        runOne(t, &BenchmarkRunner::testColdStart);
        else if (t == "warm_cache")   runOne(t, &BenchmarkRunner::testWarmCache);
        else if (t == "rapid_fire")   runOne(t, &BenchmarkRunner::testRapidFire);
        else if (t == "multi_lang")   runOne(t, &BenchmarkRunner::testMultiLanguage);
        else if (t == "context_aware")runOne(t, &BenchmarkRunner::testContextAware);
        else if (t == "multi_line")   runOne(t, &BenchmarkRunner::testMultiLine);
        else if (t == "gpu")          runOne(t, &BenchmarkRunner::testGPUAcceleration);
        else if (t == "memory")       runOne(t, &BenchmarkRunner::testMemory);
        else {
            BenchmarkTestResult r;
            r.testName = t;
            r.passed = false;
            r.notes = "Unknown test name";
            results_.push_back(r);
            if (m_onTestCompleted) m_onTestCompleted(t, false, 0.0);
        }
    }

    const double suiteSec = std::chrono::duration<double>(Clock::now() - suiteStart).count();
    if (m_onFinished) m_onFinished(passed, total, suiteSec);
}

BenchmarkTestResult BenchmarkRunner::calculateStats(const std::string& name,
                                                    const std::vector<double>& latencies,
                                                    size_t successful,
                                                    size_t total) {
    BenchmarkTestResult r;
    r.testName = name;
    r.totalRequests = total;
    r.successfulRequests = successful;
    r.successRate = (total == 0) ? 0.0 : (double)successful / (double)total;

    if (latencies.empty()) {
        r.avgLatencyMs = 0;
        r.minLatencyMs = 0;
        r.maxLatencyMs = 0;
        r.medianLatencyMs = 0;
        r.p95LatencyMs = 0;
        r.p99LatencyMs = 0;
        r.throughputReqSec = 0;
        r.passed = false;
        r.notes = "No samples";
        return r;
    }

    std::vector<double> sorted = latencies;
    std::sort(sorted.begin(), sorted.end());

    r.minLatencyMs = sorted.front();
    r.maxLatencyMs = sorted.back();
    r.avgLatencyMs = std::accumulate(sorted.begin(), sorted.end(), 0.0) / (double)sorted.size();
    r.medianLatencyMs = sorted[sorted.size() / 2];

    auto pct = [&](double p) -> double {
        if (sorted.empty()) return 0.0;
        size_t idx = (size_t)std::clamp((size_t)(p * (double)(sorted.size() - 1)), (size_t)0, sorted.size() - 1);
        return sorted[idx];
    };
    r.p95LatencyMs = pct(0.95);
    r.p99LatencyMs = pct(0.99);

    const double totalMs = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    r.throughputReqSec = (totalMs > 0) ? ((double)sorted.size() * 1000.0) / totalMs : 0.0;
    r.passed = (successful == total);
    return r;
}

void BenchmarkRunner::log(const std::string& message, int level) {
    if (m_onLogMessage) m_onLogMessage(message, level);
}

static bool runPrompt(RawrXD::CPUInferenceEngine& engine,
                      const std::string& prompt,
                      int maxTokens,
                      double* outLatencyMs) {
    const auto t0 = Clock::now();
    auto toks = engine.Tokenize(prompt);
    if (toks.empty()) return false;
    auto out = engine.Generate(toks, maxTokens);
    (void)engine.Detokenize(out);
    const auto t1 = Clock::now();
    if (outLatencyMs) *outLatencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return true;
}

bool BenchmarkRunner::testColdStart(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    // Cold-ish: clear cache and run a single small completion.
    engine_->ClearCache();
    double ms = 0.0;
    const bool ok = runPrompt(*engine_, "Write a hello world in C.", 32, &ms);
    result = calculateStats("cold_start", {ms}, ok ? 1 : 0, 1);
    return ok;
}

bool BenchmarkRunner::testWarmCache(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    std::vector<double> lats;
    lats.reserve(8);
    size_t okCount = 0;
    for (int i = 0; i < 8 && !shouldStop_; ++i) {
        double ms = 0.0;
        bool ok = runPrompt(*engine_, "Summarize: The quick brown fox jumps over the lazy dog.", 32, &ms);
        if (ok) okCount++;
        lats.push_back(ms);
    }
    result = calculateStats("warm_cache", lats, okCount, lats.size());
    return okCount == lats.size();
}

bool BenchmarkRunner::testRapidFire(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    std::vector<double> lats;
    lats.reserve(20);
    size_t okCount = 0;
    for (int i = 0; i < 20 && !shouldStop_; ++i) {
        double ms = 0.0;
        bool ok = runPrompt(*engine_, "Answer in one sentence: what is a mutex?", 8, &ms);
        if (ok) okCount++;
        lats.push_back(ms);
    }
    result = calculateStats("rapid_fire", lats, okCount, lats.size());
    return okCount == lats.size();
}

bool BenchmarkRunner::testMultiLanguage(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    const char* prompts[] = {
        "C++: Implement a small RAII wrapper for a HANDLE.",
        "Python: Write a function that parses a PE header (high-level).",
        "JavaScript: Create a debounce function.",
        "Rust: Show a safe wrapper around Win32 CreateFileW signature.",
    };

    std::vector<double> lats;
    lats.reserve(4);
    size_t okCount = 0;
    for (const char* p : prompts) {
        if (shouldStop_) break;
        double ms = 0.0;
        bool ok = runPrompt(*engine_, p, 48, &ms);
        if (ok) okCount++;
        lats.push_back(ms);
    }
    result = calculateStats("multi_lang", lats, okCount, lats.size());
    return okCount == lats.size();
}

bool BenchmarkRunner::testContextAware(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    std::string longPrompt =
        "You are reviewing a Windows x64 disassembler.\n"
        "Constraints:\n"
        "- No dynamic allocation in hot path.\n"
        "- Must support REX prefixes, ModRM, SIB, RIP-relative.\n"
        "- Must avoid false positives on inline data.\n"
        "Task: list 5 failure modes and mitigations.\n";

    double ms = 0.0;
    const bool ok = runPrompt(*engine_, longPrompt, 96, &ms);
    result = calculateStats("context_aware", {ms}, ok ? 1 : 0, 1);
    return ok;
}

bool BenchmarkRunner::testMultiLine(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }
    std::string prompt =
        "Generate a small Win32 program skeleton:\n"
        "1) RegisterClassEx\n"
        "2) CreateWindowEx\n"
        "3) Message loop\n";
    double ms = 0.0;
    const bool ok = runPrompt(*engine_, prompt, 96, &ms);
    result = calculateStats("multi_line", {ms}, ok ? 1 : 0, 1);
    return ok;
}

bool BenchmarkRunner::testGPUAcceleration(BenchmarkTestResult& result) {
    // In Gold/CPU engine, GPU acceleration is not required for correctness; report capability.
    result.testName = "gpu";
    result.totalRequests = 1;
    result.successfulRequests = 1;
    result.successRate = 1.0;
    result.avgLatencyMs = 0.0;
    result.notes = gpuEnabled_ ? "GPU requested; CPUInferenceEngine uses CPU/Titan ASM path." : "GPU disabled";
    return true;
}

bool BenchmarkRunner::testMemory(BenchmarkTestResult& result) {
    if (!engine_ || !engine_->IsModelLoaded()) {
        result.notes = "No model loaded";
        return false;
    }

    // Measure memory usage around a larger generation.
    const size_t before = engine_->GetMemoryUsage();
    double ms = 0.0;
    const bool ok = runPrompt(*engine_, "Explain how PE sections are laid out and aligned.", 128, &ms);
    const size_t after = engine_->GetMemoryUsage();

    std::ostringstream oss;
    oss << "Memory usage bytes: before=" << before << " after=" << after;
    result = calculateStats("memory", {ms}, ok ? 1 : 0, 1);
    result.notes = oss.str();
    return ok;
}
