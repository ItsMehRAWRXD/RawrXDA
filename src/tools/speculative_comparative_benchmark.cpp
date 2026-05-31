#include "gpu/speculative_decoder_v2.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct ToyModel {
    std::string modelId;
    int vocabSize = 32000;
    int divergenceStride = 7;
    bool draft = false;
    int workIterations = 0;
};

void burnCycles(int workIterations, uint32_t seed) {
    volatile uint32_t accumulator = seed | 1u;
    for (int i = 0; i < workIterations; ++i) {
        accumulator = accumulator * 1664525u + 1013904223u;
        accumulator ^= (accumulator >> 13);
    }
    (void)accumulator;
}

uint32_t hashContext(const std::vector<int>& context) {
    uint32_t hash = 2166136261u;
    for (int token : context) {
        hash ^= static_cast<uint32_t>(token);
        hash *= 16777619u;
    }
    return hash;
}

std::vector<std::pair<int, float>> toyLogprobs(const std::vector<int>& context, int topK, void* userData) {
    auto* model = static_cast<ToyModel*>(userData);
    std::vector<std::pair<int, float>> result;
    if (!model || topK <= 0) return result;

    const uint32_t hash = hashContext(context);
    burnCycles(model->workIterations, hash);
    int token = static_cast<int>(hash % static_cast<uint32_t>(model->vocabSize));
    
    if (model->draft && model->divergenceStride > 0 && !context.empty() &&
        (context.size() % static_cast<size_t>(model->divergenceStride) == 0)) {
        token = (token + 1) % model->vocabSize;
    }

    result.reserve(static_cast<size_t>(topK));
    result.push_back({token, 0.0f});
    for (int rank = 1; rank < topK; ++rank) {
        result.push_back({(token + rank) % model->vocabSize, -1.386f * static_cast<float>(rank)});
    }
    return result;
}

std::string toyDecode(int tokenId, void*) {
    const char c = static_cast<char>(32 + (tokenId % 95));
    return std::string(1, c);
}

std::vector<int> toyEncode(const std::string& text, void*) {
    std::vector<int> tokens;
    tokens.reserve(text.size());
    for (unsigned char c : text) {
        tokens.push_back(static_cast<int>(c));
    }
    return tokens;
}

RawrXD::Speculative::ModelInference makeInference(ToyModel& model) {
    RawrXD::Speculative::ModelInference inference;
    inference.modelId = model.modelId;
    inference.logprobs = &toyLogprobs;
    inference.decode = &toyDecode;
    inference.encode = &toyEncode;
    inference.userData = &model;
    return inference;
}

struct SummaryStats {
    double p50Ms = 0.0, p95Ms = 0.0, p99Ms = 0.0, avgMs = 0.0, sigmaMs = 0.0;
};

SummaryStats summarize(std::vector<double> samples) {
    SummaryStats out;
    if (samples.empty()) return out;
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    out.p50Ms = samples[n / 2];
    out.p95Ms = samples[static_cast<size_t>(n * 0.95)];
    out.p99Ms = samples[static_cast<size_t>(n * 0.99)];
    double sum = 0.0;
    for (double s : samples) sum += s;
    out.avgMs = sum / static_cast<double>(n);
    double variance = 0.0;
    for (double s : samples) { double diff = s - out.avgMs; variance += diff * diff; }
    out.sigmaMs = std::sqrt(variance / static_cast<double>(n));
    return out;
}

struct BenchConfig {
    int maxNewTokens = 256;
    int runs = 12;
    int divergenceStride = 7;
    std::vector<int> concurrencyLevels{1, 4, 8};
    std::string jsonOutputPath;
};

struct ModeBatchResult {
    std::vector<double> requestLatenciesMs;
    double avgBatchMs = 0.0, throughputTokensPerSec = 0.0, acceptanceRate = 0.0, decoderSpeedupRatio = 1.0;
};

struct ConcurrencyResult {
    int concurrency = 1;
    SummaryStats baseline, speculative;
    double baselineTps = 0.0, speculativeTps = 0.0, realizedSpeedup = 1.0, acceptanceRate = 0.0, decoderSpeedupRatio = 1.0;
};

std::vector<int> parseCsvInts(const std::string& csv) {
    std::vector<int> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) out.push_back(std::max(1, std::stoi(token)));
    }
    if (out.empty()) out.push_back(1);
    return out;
}

ModeBatchResult runConcurrentMode(const std::vector<int>& promptTokens, int maxNewTokens, int runs,
                                   int divergenceStride, int concurrency, bool speculative) {
    ModeBatchResult aggregate;
    double totalBatchMs = 0.0;
    int successfulRequests = 0;
    double acceptanceAccumulator = 0.0, decoderSpeedupAccumulator = 0.0;

    for (int run = 0; run < runs; ++run) {
        auto batchStart = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> workers;
        std::mutex resultMutex;
        std::vector<double> runLatencies;
        double runAcceptance = 0.0, runSpeedup = 0.0;
        int runSuccess = 0;

        for (int t = 0; t < concurrency; ++t) {
            workers.emplace_back([&, t]() {
                ToyModel draftModel{"draft-" + std::to_string(t), 32000, divergenceStride, true, 750};
                ToyModel targetModel{"target-" + std::to_string(t), 32000, divergenceStride, false, 120000};
                auto draft = makeInference(draftModel);
                auto target = makeInference(targetModel);
                
                RawrXD::Speculative::SpeculativeDecoderV2 decoder;
                RawrXD::Speculative::SpeculationConfig config;
                config.maxDraftTokens = speculative ? 4 : 1;
                config.adaptiveDraftLen = speculative;
                decoder.setConfig(config);
                
                if (speculative) { decoder.setDraftModel(draft); decoder.setTargetModel(target); }
                else { decoder.setDraftModel(target); decoder.setTargetModel(target); }
                
                auto t0 = std::chrono::high_resolution_clock::now();
                auto result = decoder.generate(promptTokens, maxNewTokens);
                auto t1 = std::chrono::high_resolution_clock::now();
                
                if (result.success && !result.tokens.empty()) {
                    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                    std::lock_guard<std::mutex> lk(resultMutex);
                    runLatencies.push_back(ms);
                    runAcceptance += result.stats.acceptanceRate;
                    runSpeedup += result.stats.speedupRatio;
                    runSuccess++;
                }
            });
        }
        for (auto& w : workers) w.join();
        for (double ms : runLatencies) aggregate.requestLatenciesMs.push_back(ms);
        successfulRequests += runSuccess;
        acceptanceAccumulator += runAcceptance;
        decoderSpeedupAccumulator += runSpeedup;
        auto batchEnd = std::chrono::high_resolution_clock::now();
        totalBatchMs += std::chrono::duration<double, std::milli>(batchEnd - batchStart).count();
    }

    if (runs > 0) aggregate.avgBatchMs = totalBatchMs / static_cast<double>(runs);
    if (aggregate.avgBatchMs > 0.0 && successfulRequests > 0) {
        aggregate.throughputTokensPerSec = (static_cast<double>(successfulRequests) * static_cast<double>(maxNewTokens)) / (totalBatchMs / 1000.0);
        aggregate.acceptanceRate = acceptanceAccumulator / static_cast<double>(successfulRequests);
        aggregate.decoderSpeedupRatio = decoderSpeedupAccumulator / static_cast<double>(successfulRequests);
    }
    return aggregate;
}

std::string toFixed(double value, int precision = 4) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string buildJsonReport(const BenchConfig& bench, const std::vector<ConcurrencyResult>& results) {
    std::ostringstream out;
    out << "{\n  \"benchmark\": \"RawrXD Speculative Comparative Benchmark\",\n";
    out << "  \"sequence_tokens\": " << bench.maxNewTokens << ",\n  \"runs\": " << bench.runs << ",\n";
    out << "  \"draft_divergence_stride\": " << bench.divergenceStride << ",\n  \"concurrency_results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\"concurrency\": " << r.concurrency << ", \"baseline_tps\": " << toFixed(r.baselineTps, 2);
        out << ", \"speculative_tps\": " << toFixed(r.speculativeTps, 2) << ", \"realized_speedup\": " << toFixed(r.realizedSpeedup, 4);
        out << ", \"acceptance_rate\": " << toFixed(r.acceptanceRate, 4) << ", \"decoder_speedup_ratio\": " << toFixed(r.decoderSpeedupRatio, 4);
        out << ", \"baseline\": {\"avg_ms\": " << toFixed(r.baseline.avgMs, 4) << ", \"p50_ms\": " << toFixed(r.baseline.p50Ms, 4);
        out << ", \"p95_ms\": " << toFixed(r.baseline.p95Ms, 4) << ", \"p99_ms\": " << toFixed(r.baseline.p99Ms, 4) << "}";
        out << ", \"speculative\": {\"avg_ms\": " << toFixed(r.speculative.avgMs, 4) << ", \"p50_ms\": " << toFixed(r.speculative.p50Ms, 4);
        out << ", \"p95_ms\": " << toFixed(r.speculative.p95Ms, 4) << ", \"p99_ms\": " << toFixed(r.speculative.p99Ms, 4) << "}}";
        out << (i + 1 == results.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace

int main(int argc, char* argv[]) {
    BenchConfig bench;
    if (argc > 1) bench.maxNewTokens = std::max(1, std::stoi(argv[1]));
    if (argc > 2) bench.runs = std::max(1, std::stoi(argv[2]));
    if (argc > 3) bench.divergenceStride = std::max(2, std::stoi(argv[3]));
    if (argc > 4) {
        std::string arg = argv[4];
        if (arg.find('x') != std::string::npos) {
            bench.concurrencyLevels = {std::stoi(arg.substr(0, arg.find('x')))};
        } else {
            bench.concurrencyLevels = parseCsvInts(arg);
        }
    }
    if (argc > 5) bench.jsonOutputPath = argv[5];

    const std::string prompt = "Compressed sprint benchmark prompt: quantify speculative execution gains.";
    ToyModel promptModel{"prompt-encoder", 32000, bench.divergenceStride, false, 0};
    const auto promptTokens = toyEncode(prompt, &promptModel);

    std::vector<ConcurrencyResult> results;
    results.reserve(bench.concurrencyLevels.size());

    std::cout << "RawrXD Speculative Comparative Benchmark\n======================================\n";
    std::cout << "Prompt tokens: " << promptTokens.size() << "\nGenerated tokens: " << bench.maxNewTokens;
    std::cout << "\nRuns: " << bench.runs << "\nDraft divergence stride: " << bench.divergenceStride;
    std::cout << "\nConcurrency levels: ";
    for (size_t i = 0; i < bench.concurrencyLevels.size(); ++i)
        std::cout << bench.concurrencyLevels[i] << (i + 1 < bench.concurrencyLevels.size() ? "," : "\n");
    std::cout << "\n";

    for (int concurrency : bench.concurrencyLevels) {
        std::cout << "Running concurrency " << concurrency << "x baseline...\n" << std::flush;
        auto baselineBatch = runConcurrentMode(promptTokens, bench.maxNewTokens, bench.runs, bench.divergenceStride, concurrency, false);
        std::cout << "Running concurrency " << concurrency << "x speculative...\n" << std::flush;
        auto speculativeBatch = runConcurrentMode(promptTokens, bench.maxNewTokens, bench.runs, bench.divergenceStride, concurrency, true);

        if (baselineBatch.requestLatenciesMs.empty() || speculativeBatch.requestLatenciesMs.empty()) {
            std::cerr << "Benchmark failed for concurrency=" << concurrency << std::endl;
            return 3;
        }

        ConcurrencyResult result;
        result.concurrency = concurrency;
        result.baseline = summarize(baselineBatch.requestLatenciesMs);
        result.speculative = summarize(speculativeBatch.requestLatenciesMs);
        result.baselineTps = baselineBatch.throughputTokensPerSec;
        result.speculativeTps = speculativeBatch.throughputTokensPerSec;
        result.realizedSpeedup = baselineBatch.throughputTokensPerSec > 0.0 ? speculativeBatch.throughputTokensPerSec / baselineBatch.throughputTokensPerSec : 1.0;
        result.acceptanceRate = speculativeBatch.acceptanceRate;
        result.decoderSpeedupRatio = speculativeBatch.decoderSpeedupRatio;
        results.push_back(result);
    }

    for (const auto& result : results) {
        std::cout << "\nConcurrency " << result.concurrency << "x\n";
        std::cout << "  Baseline TPS:        " << result.baselineTps << "\n";
        std::cout << "  Baseline p50/p99:    " << result.baseline.p50Ms << " / " << result.baseline.p99Ms << " ms\n";
        std::cout << "  Speculative TPS:     " << result.speculativeTps << "\n";
        std::cout << "  Speculative p50/p99: " << result.speculative.p50Ms << " / " << result.speculative.p99Ms << " ms\n";
        std::cout << "  Acceptance:          " << (result.acceptanceRate * 100.0) << "%\n";
        std::cout << "  Realized speedup:    " << result.realizedSpeedup << "x\n";
        std::cout << "  Decoder ratio:       " << result.decoderSpeedupRatio << "x\n";
        std::cout << "  Sigma (baseline/spec): " << result.baseline.sigmaMs << " / " << result.speculative.sigmaMs << " ms\n";
    }

    if (!bench.jsonOutputPath.empty()) {
        std::ofstream jsonOut(bench.jsonOutputPath, std::ios::binary);
        if (jsonOut.is_open()) {
            jsonOut << buildJsonReport(bench, results);
            std::cout << "\nJSON report: " << bench.jsonOutputPath << "\n";
        }
    }

    return 0;
}#include "gpu/speculative_decoder_v2.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Toy model for benchmarking without real inference
struct ToyModel {
    std::string modelId;
    int vocabSize = 32000;
    int divergenceStride = 7;
    bool draft = false;
    int workIterations = 0;
};

void burnCycles(int workIterations, uint32_t seed) {
    volatile uint32_t accumulator = seed | 1u;
    for (int i = 0; i < workIterations; ++i) {
        accumulator = accumulator * 1664525u + 1013904223u;
        accumulator ^= (accumulator >> 13);
    }
    (void)accumulator;
}

uint32_t hashContext(const std::vector<int>& context) {
    uint32_t hash = 2166136261u;
    for (int token : context) {
        hash ^= static_cast<uint32_t>(token);
        hash *= 16777619u;
    }
    return hash;
}

std::vector<std::pair<int, float>> toyLogprobs(const std::vector<int>& context,
                                               int topK,
                                               void* userData) {
    auto* model = static_cast<ToyModel*>(userData);
    std::vector<std::pair<int, float>> result;
    if (!model || topK <= 0) return result;

    const uint32_t hash = hashContext(context);
    burnCycles(model->workIterations, hash);
    int token = static_cast<int>(hash % static_cast<uint32_t>(model->vocabSize));
    
    // Draft model diverges at stride intervals
    if (model->draft && model->divergenceStride > 0 && !context.empty() &&
        (context.size() % static_cast<size_t>(model->divergenceStride) == 0)) {
        token = (token + 1) % model->vocabSize;
    }

    result.reserve(static_cast<size_t>(topK));
    result.push_back({token, 0.0f});
    for (int rank = 1; rank < topK; ++rank) {
        result.push_back({(token + rank) % model->vocabSize,
                          -1.386f * static_cast<float>(rank)});
    }
    return result;
}

std::string toyDecode(int tokenId, void* /*userData*/) {
    const char c = static_cast<char>(32 + (tokenId % 95));
    return std::string(1, c);
}

std::vector<int> toyEncode(const std::string& text, void* /*userData*/) {
    std::vector<int> tokens;
    tokens.reserve(text.size());
    for (unsigned char c : text) {
        tokens.push_back(static_cast<int>(c));
    }
    return tokens;
}

RawrXD::Speculative::ModelInference makeInference(ToyModel& model) {
    RawrXD::Speculative::ModelInference inference;
    inference.modelId = model.modelId;
    inference.logprobs = &toyLogprobs;
    inference.decode = &toyDecode;
    inference.encode = &toyEncode;
    inference.userData = &model;
    return inference;
}

// Statistics helpers
struct SummaryStats {
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double avgMs = 0.0;
    double sigmaMs = 0.0;
};

SummaryStats summarize(std::vector<double> samples) {
    SummaryStats out;
    if (samples.empty()) return out;

    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    
    out.p50Ms = samples[n / 2];
    out.p95Ms = samples[static_cast<size_t>(n * 0.95)];
    out.p99Ms = samples[static_cast<size_t>(n * 0.99)];
    
    double sum = 0.0;
    for (double s : samples) sum += s;
    out.avgMs = sum / static_cast<double>(n);
    
    double variance = 0.0;
    for (double s : samples) {
        double diff = s - out.avgMs;
        variance += diff * diff;
    }
    out.sigmaMs = std::sqrt(variance / static_cast<double>(n));
    
    return out;
}

// Benchmark configuration
struct BenchConfig {
    int maxNewTokens = 256;
    int runs = 12;
    int divergenceStride = 7;
    std::vector<int> concurrencyLevels{1, 4, 8};
    std::string jsonOutputPath;
};

struct ModeBatchResult {
    std::vector<double> requestLatenciesMs;
    double avgBatchMs = 0.0;
    double throughputTokensPerSec = 0.0;
    double acceptanceRate = 0.0;
    double decoderSpeedupRatio = 1.0;
};

struct ConcurrencyResult {
    int concurrency = 1;
    SummaryStats baseline;
    SummaryStats speculative;
    double baselineTps = 0.0;
    double speculativeTps = 0.0;
    double realizedSpeedup = 1.0;
    double acceptanceRate = 0.0;
    double decoderSpeedupRatio = 1.0;
};

std::vector<int> parseCsvInts(const std::string& csv) {
    std::vector<int> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            out.push_back(std::max(1, std::stoi(token)));
        }
    }
    if (out.empty()) out.push_back(1);
    return out;
}

// Run concurrent benchmark for a single mode
ModeBatchResult runConcurrentMode(const std::vector<int>& promptTokens,
                                   int maxNewTokens,
                                   int runs,
                                   int divergenceStride,
                                   int concurrency,
                                   bool speculative) {
    ModeBatchResult aggregate;
    double totalBatchMs = 0.0;
    int successfulRequests = 0;
    double acceptanceAccumulator = 0.0;
    double decoderSpeedupAccumulator = 0.0;

    for (int run = 0; run < runs; ++run) {
        auto batchStart = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> workers;
        std::mutex resultMutex;
        std::vector<double> runLatencies;
        double runAcceptance = 0.0;
        double runSpeedup = 0.0;
        int runSuccess = 0;

        for (int t = 0; t < concurrency; ++t) {
            workers.emplace_back([&, t]() {
                ToyModel draftModel{"draft-" + std::to_string(t), 32000, divergenceStride, true, 750};
                ToyModel targetModel{"target-" + std::to_string(t), 32000, divergenceStride, false, 120000};
                
                auto draft = makeInference(draftModel);
                auto target = makeInference(targetModel);
                
                RawrXD::Speculative::SpeculativeDecoderV2 decoder;
                RawrXD::Speculative::SpeculationConfig config;
                config.maxDraftTokens = speculative ? 4 : 1;
                config.adaptiveDraftLen = speculative;
                decoder.setConfig(config);
                
                if (speculative) {
                    decoder.setDraftModel(draft);
                    decoder.setTargetModel(target);
                } else {
                    decoder.setDraftModel(target);
                    decoder.setTargetModel(target);
                }
                
                auto t0 = std::chrono::high_resolution_clock::now();
                auto result = decoder.generate(promptTokens, maxNewTokens);
                auto t1 = std::chrono::high_resolution_clock::now();
                
                if (result.success && !result.tokens.empty()) {
                    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                    std::lock_guard<std::mutex> lk(resultMutex);
                    runLatencies.push_back(ms);
                    runAcceptance += result.stats.acceptanceRate;
                    runSpeedup += result.stats.speedupRatio;
                    runSuccess++;
                }
            });
        }
        
        for (auto& w : workers) {
            w.join();
        }
        
        for (double ms : runLatencies) {
            aggregate.requestLatenciesMs.push_back(ms);
        }
        successfulRequests += runSuccess;
        acceptanceAccumulator += runAcceptance;
        decoderSpeedupAccumulator += runSpeedup;
        
        auto batchEnd = std::chrono::high_resolution_clock::now();
        totalBatchMs += std::chrono::duration<double, std::milli>(batchEnd - batchStart).count();
    }

    if (runs > 0) {
        aggregate.avgBatchMs = totalBatchMs / static_cast<double>(runs);
    }
    if (aggregate.avgBatchMs > 0.0 && successfulRequests > 0) {
        aggregate.throughputTokensPerSec = 
            (static_cast<double>(successfulRequests) * static_cast<double>(maxNewTokens)) /
            (totalBatchMs / 1000.0);
        aggregate.acceptanceRate = acceptanceAccumulator / static_cast<double>(successfulRequests);
        aggregate.decoderSpeedupRatio = decoderSpeedupAccumulator / static_cast<double>(successfulRequests);
    }
    
    return aggregate;
}

std::string toFixed(double value, int precision = 4) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string buildJsonReport(const BenchConfig& bench,
                            const std::vector<ConcurrencyResult>& results) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"benchmark\": \"RawrXD Speculative Comparative Benchmark\",\n";
    out << "  \"sequence_tokens\": " << bench.maxNewTokens << ",\n";
    out << "  \"runs\": " << bench.runs << ",\n";
    out << "  \"draft_divergence_stride\": " << bench.divergenceStride << ",\n";
    out << "  \"concurrency_results\": [\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"concurrency\": " << r.concurrency << ",\n";
        out << "      \"baseline_tps\": " << toFixed(r.baselineTps, 2) << ",\n";
        out << "      \"speculative_tps\": " << toFixed(r.speculativeTps, 2) << ",\n";
        out << "      \"realized_speedup\": " << toFixed(r.realizedSpeedup, 4) << ",\n";
        out << "      \"acceptance_rate\": " << toFixed(r.acceptanceRate, 4) << ",\n";
        out << "      \"decoder_speedup_ratio\": " << toFixed(r.decoderSpeedupRatio, 4) << ",\n";
        out << "      \"baseline\": {\n";
        out << "        \"avg_ms\": " << toFixed(r.baseline.avgMs, 4) << ",\n";
        out << "        \"p50_ms\": " << toFixed(r.baseline.p50Ms, 4) << ",\n";
        out << "        \"p95_ms\": " << toFixed(r.baseline.p95Ms, 4) << ",\n";
        out << "        \"p99_ms\": " << toFixed(r.baseline.p99Ms, 4) << "\n";
        out << "      },\n";
        out << "      \"speculative\": {\n";
        out << "        \"avg_ms\": " << toFixed(r.speculative.avgMs, 4) << ",\n";
        out << "        \"p50_ms\": " << toFixed(r.speculative.p50Ms, 4) << ",\n";
        out << "        \"p95_ms\": " << toFixed(r.speculative.p95Ms, 4) << ",\n";
        out << "        \"p99_ms\": " << toFixed(r.speculative.p99Ms, 4) << "\n";
        out << "      }\n";
        out << "    }" << (i + 1 == results.size() ? "\n" : ",\n");
    }
    
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace

int main(int argc, char* argv[]) {
    BenchConfig bench;

    // Parse positional args
    if (argc > 1) bench.maxNewTokens = std::max(1, std::stoi(argv[1]));
    if (argc > 2) bench.runs = std::max(1, std::stoi(argv[2]));
    if (argc > 3) bench.divergenceStride = std::max(2, std::stoi(argv[3]));
    if (argc > 4) {
        std::string arg = argv[4];
        if (arg.find('x') != std::string::npos) {
            int level = std::stoi(arg.substr(0, arg.find('x')));
            bench.concurrencyLevels = {level};
        } else {
            bench.concurrencyLevels = parseCsvInts(arg);
        }
    }
    if (argc > 5) bench.jsonOutputPath = argv[5];

    const std::string prompt = "Compressed sprint benchmark prompt: quantify speculative execution gains.";
    ToyModel promptModel{"prompt-encoder", 32000, bench.divergenceStride, false, 0};
    const auto promptTokens = toyEncode(prompt, &promptModel);

    std::vector<ConcurrencyResult> results;
    results.reserve(bench.concurrencyLevels.size());

    std::cout << "RawrXD Speculative Comparative Benchmark\n";
    std::cout << "======================================\n";
    std::cout << "Prompt tokens: " << promptTokens.size() << "\n";
    std::cout << "Generated tokens: " << bench.maxNewTokens << "\n";
    std::cout << "Runs: " << bench.runs << "\n";
    std::cout << "Draft divergence stride: " << bench.divergenceStride << "\n";
    std::cout << "Concurrency levels: ";
    for (size_t i = 0; i < bench.concurrencyLevels.size(); ++i) {
        std::cout << bench.concurrencyLevels[i] << (i + 1 < bench.concurrencyLevels.size() ? "," : "\n");
    }
    std::cout << "\n";

    for (int concurrency : bench.concurrencyLevels) {
        std::cout << "Running concurrency " << concurrency << "x baseline...\n" << std::flush;
        const auto baselineBatch = runConcurrentMode(promptTokens,
            bench.maxNewTokens, bench.runs, bench.divergenceStride, concurrency, false);
        
        std::cout << "Running concurrency " << concurrency << "x speculative...\n" << std::flush;
        const auto speculativeBatch = runConcurrentMode(promptTokens,
            bench.maxNewTokens, bench.runs, bench.divergenceStride, concurrency, true);

        if (baselineBatch.requestLatenciesMs.empty() || speculativeBatch.requestLatenciesMs.empty()) {
            std::cerr << "Benchmark failed to collect latency samples for concurrency=" << concurrency << std::endl;
            return 3;
        }

        ConcurrencyResult result;
        result.concurrency = concurrency;
        result.baseline = summarize(baselineBatch.requestLatenciesMs);
        result.speculative = summarize(speculativeBatch.requestLatenciesMs);
        result.baselineTps = baselineBatch.throughputTokensPerSec;
        result.speculativeTps = speculativeBatch.throughputTokensPerSec;
        result.realizedSpeedup = (baselineBatch.throughputTokensPerSec > 0.0)
            ? speculativeBatch.throughputTokensPerSec / baselineBatch.throughputTokensPerSec
            : 1.0;
        result.acceptanceRate = speculativeBatch.acceptanceRate;
        result.decoderSpeedupRatio = speculativeBatch.decoderSpeedupRatio;
        results.push_back(result);
    }

    for (const auto& result : results) {
        std::cout << "\nConcurrency " << result.concurrency << "x\n";
        std::cout << "  Baseline TPS:        " << result.baselineTps << "\n";
        std::cout << "  Baseline p50/p99:    " << result.baseline.p50Ms << " / "
                  << result.baseline.p99Ms << " ms\n";
        std::cout << "  Speculative TPS:     " << result.speculativeTps << "\n";
        std::cout << "  Speculative p50/p99: " << result.speculative.p50Ms << " / "
                  << result.speculative.p99Ms << " ms\n";
        std::cout << "  Acceptance:          " << (result.acceptanceRate * 100.0) << "%\n";
        std::cout << "  Realized speedup:    " << result.realizedSpeedup << "x\n";
        std::cout << "  Decoder ratio:       " << result.decoderSpeedupRatio << "x\n";
        std::cout << "  Sigma (baseline/spec): " << result.baseline.sigmaMs << " / "
                  << result.speculative.sigmaMs << " ms\n";
    }

    if (!bench.jsonOutputPath.empty()) {
        std::ofstream jsonOut(bench.jsonOutputPath, std::ios::binary);
        if (jsonOut.is_open()) {
            jsonOut << buildJsonReport(bench, results);
            std::cout << "\nJSON report: " << bench.jsonOutputPath << "\n";
        }
    }

    return 0;
}
    for (int token : context) {
        hash ^= static_cast<uint32_t>(token);
        hash *= 16777619u;
    }
    return hash;
}

std::vector<std::pair<int, float>> toyLogprobs(const std::vector<int>& context,
                                               int topK,
                                               void* userData) {
    auto* model = static_cast<ToyModel*>(userData);
    std::vector<std::pair<int, float>> result;
    if (!model || topK <= 0) {
        return result;
    }

    const uint32_t hash = hashContext(context);
    burnCycles(model->workIterations, hash);
    int token = static_cast<int>(hash % static_cast<uint32_t>(model->vocabSize));
    if (model->draft && model->divergenceStride > 0 && !context.empty() &&
        (context.size() % static_cast<size_t>(model->divergenceStride) == 0)) {
        token = (token + 1) % model->vocabSize;
    }

    result.reserve(static_cast<size_t>(topK));
    result.push_back({token, 0.0f});
    for (int rank = 1; rank < topK; ++rank) {
        result.push_back({(token + rank) % model->vocabSize,
                          -1.386f * static_cast<float>(rank)});
    }
    return result;
}

std::string toyDecode(int tokenId, void* /*userData*/) {
    const char c = static_cast<char>(32 + (tokenId % 95));
    return std::string(1, c);
}

std::vector<int> toyEncode(const std::string& text, void* /*userData*/) {
    std::vector<int> tokens;
    tokens.reserve(text.size());
    for (unsigned char c : text) {
        tokens.push_back(static_cast<int>(c));
    }
    return tokens;
}

RawrXD::Speculative::ModelInference makeInference(ToyModel& model) {
    RawrXD::Speculative::ModelInference inference;
    inference.modelId = model.modelId;
    inference.logprobs = &toyLogprobs;
    inference.decode = &toyDecode;
    inference.encode = &toyEncode;
    inference.userData = &model;
    return inference;
}

struct SummaryStats {
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double avgMs = 0.0;
    double sigmaMs = 0.0;
};

struct RunConfig {
    int maxNewTokens = 256;
    int runsPerThread = 12;
    int divergenceStride = 7;
    std::vector<int> concurrencyLevels{1, 4, 8};
    std::string jsonOut;
};

struct ModeResult {
    SummaryStats latency{};
    double throughputTps = 0.0;
    double acceptanceRate = 0.0;
    double speedupRatio = 0.0;
    double effectiveTokensPerStep = 1.0;
    int64_t totalGeneratedTokens = 0;
    std::vector<double> samplesMs;
};

struct LoadResult {
    int concurrency = 1;
    ModeResult baseline;
    ModeResult speculative;
};

std::vector<int> parseCsvInts(const std::string& csv) {
    std::vector<int> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            continue;
        }
        out.push_back(std::max(1, std::stoi(token)));
    }
    if (out.empty()) {
        out.push_back(1);
    }
    return out;
}

ModeResult runModeConcurrent(const RunConfig& cfg, int concurrency, bool speculative) {
    struct Shared {
        std::mutex mu;
        std::vector<double> latencies;
        double acceptanceSum = 0.0;
        double speedupSum = 0.0;
        double effTokensStepSum = 0.0;
        int successfulRuns = 0;
        int64_t generatedTokens = 0;
    } shared;

    auto worker = [&](int workerId) {
        ToyModel draftModel{"toy-draft-" + std::to_string(workerId), 32000,
                           cfg.divergenceStride, true, 750};
        ToyModel targetModel{"toy-target-" + std::to_string(workerId), 32000,
                            cfg.divergenceStride, false, 120000};

        auto draft = makeInference(draftModel);
        auto target = makeInference(targetModel);

        RawrXD::Speculative::SpeculativeDecoderV2 decoder;
        RawrXD::Speculative::SpeculationConfig config;
        config.maxDraftTokens = speculative ? 4 : 1;
        config.minDraftTokens = 1;
        config.acceptanceThreshold = 0.3f;
        config.adaptiveDraftLen = speculative;
        decoder.setConfig(config);

        if (speculative) {
            if (!decoder.setDraftModel(draft).success || !decoder.setTargetModel(target).success) {
                return;
            }
        } else {
            if (!decoder.setDraftModel(target).success || !decoder.setTargetModel(target).success) {
                return;
            }
        }

        const std::string prompt = "Compressed sprint benchmark prompt: quantify speculative execution gains.";
        const auto promptTokens = target.encode(prompt, target.userData);

        (void)decoder.generate(promptTokens, 16);

        for (int i = 0; i < cfg.runsPerThread; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            auto result = decoder.generate(promptTokens, cfg.maxNewTokens);
            auto t1 = std::chrono::high_resolution_clock::now();

            if (!result.success || result.tokens.empty()) {
                continue;
            }

            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double accepted = result.stats.acceptanceRate;
            const double speedup = result.stats.speedupRatio;
            const double effTokensStep = speculative
                ? (1.0 + accepted * static_cast<double>(config.maxDraftTokens))
                : 1.0;

            std::lock_guard<std::mutex> lk(shared.mu);
            shared.latencies.push_back(ms);
            shared.acceptanceSum += accepted;
            shared.speedupSum += speedup;
            shared.effTokensStepSum += effTokensStep;
            shared.generatedTokens += static_cast<int64_t>(result.tokens.size());
            shared.successfulRuns++;
        }
    };

    auto modeStart = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(concurrency));
    for (int i = 0; i < concurrency; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    auto modeEnd = std::chrono::high_resolution_clock::now();

    ModeResult out;
    out.samplesMs = shared.latencies;
    out.latency = summarize(shared.latencies);
    const double wallSec = std::chrono::duration<double>(modeEnd - modeStart).count();
    out.totalGeneratedTokens = shared.generatedTokens;
    out.throughputTps = (wallSec > 0.0) ? (static_cast<double>(shared.generatedTokens) / wallSec) : 0.0;

    if (shared.successfulRuns > 0) {
        out.acceptanceRate = shared.acceptanceSum / static_cast<double>(shared.successfulRuns);
        out.speedupRatio = shared.speedupSum / static_cast<double>(shared.successfulRuns);
        out.effectiveTokensPerStep = shared.effTokensStepSum / static_cast<double>(shared.successfulRuns);
    }

    return out;
}

void maybeWriteJson(const RunConfig& cfg, const std::vector<LoadResult>& allResults) {
    if (cfg.jsonOut.empty()) {
        return;
    }

    std::ofstream ofs(cfg.jsonOut, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open JSON output: " << cfg.jsonOut << std::endl;
        return;
    }

    ofs << "{\n";
    ofs << "  \"generated_at_utc\": \"" << "2026-04-11T00:00:00Z" << "\",\n";
    ofs << "  \"sequence_length\": " << cfg.maxNewTokens << ",\n";
    ofs << "  \"runs_per_thread\": " << cfg.runsPerThread << ",\n";
    ofs << "  \"divergence_stride\": " << cfg.divergenceStride << ",\n";
    ofs << "  \"loads\": [\n";

    for (size_t i = 0; i < allResults.size(); ++i) {
        const auto& r = allResults[i];
        ofs << "    {\n";
        ofs << "      \"concurrency\": " << r.concurrency << ",\n";
        ofs << "      \"baseline\": {\n";
        ofs << "        \"throughput_tps\": " << r.baseline.throughputTps << ",\n";
        ofs << "        \"avg_latency_ms\": " << r.baseline.latency.avgMs << ",\n";
        ofs << "        \"p50_latency_ms\": " << r.baseline.latency.p50Ms << ",\n";
        ofs << "        \"p95_latency_ms\": " << r.baseline.latency.p95Ms << ",\n";
        ofs << "        \"p99_latency_ms\": " << r.baseline.latency.p99Ms << "\n";
        ofs << "      },\n";
        ofs << "      \"speculative\": {\n";
        ofs << "        \"throughput_tps\": " << r.speculative.throughputTps << ",\n";
        ofs << "        \"avg_latency_ms\": " << r.speculative.latency.avgMs << ",\n";
        ofs << "        \"p50_latency_ms\": " << r.speculative.latency.p50Ms << ",\n";
        ofs << "        \"p95_latency_ms\": " << r.speculative.latency.p95Ms << ",\n";
        ofs << "        \"p99_latency_ms\": " << r.speculative.latency.p99Ms << ",\n";
        ofs << "        \"acceptance_rate\": " << r.speculative.acceptanceRate << ",\n";
        ofs << "        \"effective_tokens_per_step\": " << r.speculative.effectiveTokensPerStep << "\n";
        ofs << "      }\n";
        ofs << "    }" << (i + 1 < allResults.size() ? "," : "") << "\n";
    }

    ofs << "  ]\n";
    ofs << "}\n";
}

struct LaneOutcome {
    double latencyMs = 0.0;
    double acceptanceRate = 0.0;
    double decoderSpeedupRatio = 1.0;
    bool success = false;
};

struct ModeBatchResult {
    std::vector<double> requestLatenciesMs;
    double avgBatchMs = 0.0;
    double throughputTokensPerSec = 0.0;
    double acceptanceRate = 0.0;
    double decoderSpeedupRatio = 1.0;
};

struct ConcurrencyResult {
    int concurrency = 1;
    SummaryStats baseline;
    SummaryStats speculative;
    double baselineTps = 0.0;
    double speculativeTps = 0.0;
    double realizedSpeedup = 1.0;
    double acceptanceRate = 0.0;
    double decoderSpeedupRatio = 1.0;
};

struct BenchConfig {
    int maxNewTokens = 256;
    int runs = 12;
    int divergenceStride = 7;
    std::vector<int> concurrencyLevels{1, 4, 8};
    std::string jsonOutputPath;
};

} // namespace

int main(int argc, char* argv[]) {
    BenchConfig bench;

    // Backward-compatible positional args.
    if (argc > 1) {
        bench.maxNewTokens = std::max(1, std::stoi(argv[1]));
    }
    if (argc > 2) {
        bench.runs = std::max(1, std::stoi(argv[2]));
    }
    if (argc > 3) {
        bench.divergenceStride = std::max(2, std::stoi(argv[3]));
    }
    if (argc > 4) {
        // Parse 66x format or comma-separated concurrency levels
        std::string arg = argv[4];
        if (arg.find('x') != std::string::npos) {
            // Parse "66x" format
            int level = std::stoi(arg.substr(0, arg.find('x')));
            bench.concurrencyLevels = {level};
        } else {
            bench.concurrencyLevels = parseCsvInts(arg);
        }
    }
    if (argc > 5) {
        bench.jsonOutputPath = argv[5];
    }

    const std::string prompt = "Compressed sprint benchmark prompt: quantify speculative execution gains.";
    ToyModel promptModel{"prompt-encoder", 32000, bench.divergenceStride, false, 0};
    const auto promptTokens = toyEncode(prompt, &promptModel);

    std::vector<ConcurrencyResult> results;
    results.reserve(bench.concurrencyLevels.size());

    for (int concurrency : bench.concurrencyLevels) {
        const auto baselineBatch = runConcurrentMode(promptTokens,
            bench.maxNewTokens,
            bench.runs,
            bench.divergenceStride,
            concurrency,
            false);
        const auto speculativeBatch = runConcurrentMode(promptTokens,
            bench.maxNewTokens,
            bench.runs,
            bench.divergenceStride,
            concurrency,
            true);

        if (baselineBatch.requestLatenciesMs.empty() || speculativeBatch.requestLatenciesMs.empty()) {
            std::cerr << "Benchmark failed to collect latency samples for concurrency="
                      << concurrency << std::endl;
            return 3;
        }

        ConcurrencyResult result;
        result.concurrency = concurrency;
        result.baseline = summarize(baselineBatch.requestLatenciesMs);
        result.speculative = summarize(speculativeBatch.requestLatenciesMs);
        result.baselineTps = baselineBatch.throughputTokensPerSec;
        result.speculativeTps = speculativeBatch.throughputTokensPerSec;
        result.realizedSpeedup = speculativeBatch.throughputTokensPerSec / baselineBatch.throughputTokensPerSec;
        result.acceptanceRate = speculativeBatch.acceptanceRate;
        result.decoderSpeedupRatio = speculativeBatch.decoderSpeedupRatio;
        results.push_back(result);
    }

    std::cout << "RawrXD Speculative Comparative Benchmark\n";
    std::cout << "======================================\n";
    std::cout << "Prompt tokens: " << promptTokens.size() << "\n";
    std::cout << "Generated tokens: " << bench.maxNewTokens << "\n";
    std::cout << "Runs: " << bench.runs << "\n";
    std::cout << "Draft divergence stride: " << bench.divergenceStride << "\n";
    std::cout << "Concurrency levels: ";
    for (size_t i = 0; i < bench.concurrencyLevels.size(); ++i) {
        std::cout << bench.concurrencyLevels[i] << (i + 1 < bench.concurrencyLevels.size() ? "," : "\n");
    }
    std::cout << "\n";

    for (const auto& result : results) {
        std::cout << "Concurrency " << result.concurrency << "x\n";
        std::cout << "  Baseline TPS:        " << result.baselineTps << "\n";
        std::cout << "  Baseline p50/p99:    " << result.baseline.p50Ms << " / "
                  << result.baseline.p99Ms << " ms\n";
        std::cout << "  Speculative TPS:     " << result.speculativeTps << "\n";
        std::cout << "  Speculative p50/p99: " << result.speculative.p50Ms << " / "
                  << result.speculative.p99Ms << " ms\n";
        std::cout << "  Acceptance:          " << (result.acceptanceRate * 100.0) << "%\n";
        std::cout << "  Realized speedup:    " << result.realizedSpeedup << "x\n";
        std::cout << "  Decoder ratio:       " << result.decoderSpeedupRatio << "x\n";
        std::cout << "  Sigma (baseline/spec): " << result.baseline.sigmaMs << " / "
                  << result.speculative.sigmaMs << " ms\n\n";
    }

    if (!bench.jsonOutputPath.empty()) {
        std::ofstream jsonOut(bench.jsonOutputPath, std::ios::binary);
        if (jsonOut.is_open()) {
            jsonOut << buildJsonReport(bench, results);
            std::cout << "JSON report: " << bench.jsonOutputPath << "\n";
        }
    }

    return 0;
}
