// benchmark_aperture_64gb.cpp
// Comprehensive benchmark for DDR5-to-GPU aperture bypass on 64GB system
// Validates performance before 192GB upgrade

#include "rawr_sovereign_bridge.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <string>

using namespace rawr;
using namespace std::chrono;

enum class PatchMode {
    SUGGEST_ONLY,
    AUTO_APPLY_LOW_RISK,
    AUTO_APPLY_ALL
};

struct TelemetryFrame {
    double memory_throughput_gbps = 0.0;
    double activation_us = 0.0;
    double tokens_per_sec = 0.0;

    int prefetch_depth = 1;
    float utilization = 0.0f;
    int tier = 0; // 0=normal, 1=warning, 2=critical

    bool allocation_fallback = false;
    bool large_pages_enabled = false;

    double cache_efficiency_score = 0.0;
};

enum class RuntimeSignalType {
    OVER_PREFETCH,
    DISPATCH_BOUND,
    CACHE_THRASH,
    MEMORY_CONSTRAINT,
    STABLE
};

struct RuntimeSignal {
    RuntimeSignalType type = RuntimeSignalType::STABLE;
    float severity = 0.0f;
    std::string context;
};

struct PatchSuggestion {
    std::string component;
    std::string change;
    std::string rationale;
    bool low_risk = true;
};

class RuntimeSignalInterpreter {
public:
    RuntimeSignal Interpret(const TelemetryFrame& frame) {
        if (frame.allocation_fallback && !frame.large_pages_enabled) {
            return {RuntimeSignalType::MEMORY_CONSTRAINT, 0.90f,
                    "allocator in fallback mode under OS privilege constraints"};
        }

        if (frame.tier == 1) {
            last_warning_throughput_ = frame.memory_throughput_gbps;
        }

        if (frame.tier == 2 && last_warning_throughput_ > 0.0 &&
            frame.memory_throughput_gbps > last_warning_throughput_ * 1.05) {
            const double ratio = frame.memory_throughput_gbps / last_warning_throughput_;
            const float severity = static_cast<float>(std::min(1.0, 0.60 + (ratio - 1.0)));
            return {RuntimeSignalType::OVER_PREFETCH, severity,
                    "critical tier outperforms warning tier, likely aggressive prefetch overshoot"};
        }

        if (seen_prev_ && frame.tokens_per_sec > 0.0 && prev_.tokens_per_sec > 0.0) {
            const double tps_drop = (prev_.tokens_per_sec - frame.tokens_per_sec) /
                                    std::max(1.0, prev_.tokens_per_sec);
            const double bw_delta = std::abs(frame.memory_throughput_gbps - prev_.memory_throughput_gbps) /
                                    std::max(1.0, prev_.memory_throughput_gbps);
            if (tps_drop > 0.08 && bw_delta < 0.05) {
                const float severity = static_cast<float>(std::min(1.0, 0.65 + tps_drop));
                prev_ = frame;
                return {RuntimeSignalType::DISPATCH_BOUND, severity,
                        "tokens/sec dropped while memory throughput stayed stable"};
            }
        }

        if (frame.activation_us > 2500.0 && frame.cache_efficiency_score < 0.65) {
            const double activation_penalty = std::min(1.0, (frame.activation_us - 2500.0) / 3000.0);
            const float severity = static_cast<float>(std::min(1.0, 0.60 + activation_penalty));
            prev_ = frame;
            return {RuntimeSignalType::CACHE_THRASH, severity,
                    "activation latency spike with weak cache-efficiency score"};
        }

        seen_prev_ = true;
        prev_ = frame;
        return {RuntimeSignalType::STABLE, 0.10f, "runtime policy stable"};
    }

private:
    bool seen_prev_ = false;
    TelemetryFrame prev_{};
    double last_warning_throughput_ = 0.0;
};

class PatchSuggestionEngine {
public:
    PatchSuggestion GeneratePatch(const RuntimeSignal& signal) const {
        switch (signal.type) {
            case RuntimeSignalType::OVER_PREFETCH:
                return {
                    "RawrSetPrefetchDepth",
                    "Reduce CRITICAL tier prefetch depth from 4 to 3",
                    "Prevent cache eviction collapse at high utilization",
                    true
                };
            case RuntimeSignalType::DISPATCH_BOUND:
                return {
                    "MoE_Scheduler",
                    "Increase expert reuse window from 1 to 3 tokens",
                    "Reduce per-token routing overhead while bandwidth is healthy",
                    true
                };
            case RuntimeSignalType::CACHE_THRASH:
                return {
                    "RAWR_Aggressive_Stream",
                    "Clamp lookahead prefetch depth to 1 for 1GB+ tensors",
                    "Reduce cache thrash and DRAM row churn under large activations",
                    false
                };
            case RuntimeSignalType::MEMORY_CONSTRAINT:
                return {
                    "SovereignBridge",
                    "Bias allocator toward smaller aperture chunks in fallback mode",
                    "Stabilize commit pressure when large-page privilege is unavailable",
                    true
                };
            case RuntimeSignalType::STABLE:
            default:
                return {
                    "none",
                    "No patch needed",
                    "Signal interpreter reports stable behavior",
                    true
                };
        }
    }

    void Emit(const RuntimeSignal& signal,
              const PatchSuggestion& patch,
              PatchMode mode,
              const char* phase,
              const TelemetryFrame& frame) const {
        std::cout << "[RAWR IDE AUTOPATCH SUGGESTION]" << std::endl;
        std::cout << "Phase: " << phase << std::endl;
        std::cout << "Issue: " << SignalToText(signal.type)
                  << " (severity " << std::fixed << std::setprecision(2) << signal.severity << ")"
                  << std::endl;
        std::cout << "Evidence:" << std::endl;
        std::cout << "- " << signal.context << std::endl;
        std::cout << "- throughput=" << std::setprecision(2) << frame.memory_throughput_gbps
                  << " GB/s, activation=" << frame.activation_us
                  << " us, tps=" << frame.tokens_per_sec << std::endl;
        std::cout << "Suggested Patch:" << std::endl;
        std::cout << "- Component: " << patch.component << std::endl;
        std::cout << "- Change: " << patch.change << std::endl;
        std::cout << "Expected Impact:" << std::endl;
        std::cout << "- " << patch.rationale << std::endl;
        std::cout << "Patch Mode: " << ModeToText(mode) << std::endl;

        if (mode == PatchMode::AUTO_APPLY_ALL ||
            (mode == PatchMode::AUTO_APPLY_LOW_RISK && patch.low_risk)) {
            std::cout << "Auto-Apply Decision: ENABLED for this suggestion" << std::endl;
        } else {
            std::cout << "Auto-Apply Decision: SUGGESTION ONLY" << std::endl;
        }
        std::cout << std::endl;
    }

private:
    static const char* SignalToText(RuntimeSignalType type) {
        switch (type) {
            case RuntimeSignalType::OVER_PREFETCH: return "OVER_PREFETCH";
            case RuntimeSignalType::DISPATCH_BOUND: return "DISPATCH_BOUND";
            case RuntimeSignalType::CACHE_THRASH: return "CACHE_THRASH";
            case RuntimeSignalType::MEMORY_CONSTRAINT: return "MEMORY_CONSTRAINT";
            case RuntimeSignalType::STABLE:
            default:
                return "STABLE";
        }
    }

    static const char* ModeToText(PatchMode mode) {
        switch (mode) {
            case PatchMode::AUTO_APPLY_LOW_RISK: return "AUTO_APPLY_LOW_RISK";
            case PatchMode::AUTO_APPLY_ALL: return "AUTO_APPLY_ALL";
            case PatchMode::SUGGEST_ONLY:
            default:
                return "SUGGEST_ONLY";
        }
    }
};

// Benchmark configuration for 64GB system
struct BenchmarkConfig {
    size_t system_ram_gb = 64;
    size_t vram_gb = 16;
    size_t aperture_gb = 48;  // Leave 16GB for OS/apps
    
    // MoE simulation parameters
    size_t expert_size_mb = 2048;      // 2GB per expert (typical for large MoE)
    size_t num_experts = 64;           // Simulated expert pool
    size_t active_experts = 2;         // Top-k experts active
    size_t lookahead_depth = 2;        // Reduced from 4 for 64GB
    
    // Test iterations
    int warmup_iterations = 10;
    int benchmark_iterations = 100;

    PatchMode patch_mode = PatchMode::SUGGEST_ONLY;
};

// Performance metrics
struct BenchmarkResults {
    double allocation_time_ms = 0;
    double pin_time_ms = 0;
    double prefetch_time_ms = 0;
    double activation_time_ms = 0;
    double throughput_gbps = 0;
    double avg_latency_us = 0;
    size_t total_bytes_transferred = 0;
};

class ApertureBenchmark {
private:
    BenchmarkConfig config_;
    std::vector<void*> allocated_blocks_;
    std::mt19937 rng_{42}; // Fixed seed for reproducibility
    RuntimeSignalInterpreter interpreter_;
    PatchSuggestionEngine patch_engine_;
    bool allocation_fallback_ = false;
    double latest_throughput_gbps_ = 0.0;
    double latest_activation_us_ = 0.0;
    
public:
    ApertureBenchmark(const BenchmarkConfig& config) : config_(config) {}
    
    void RunAllBenchmarks() {
        std::cout << "=== RawrXD Aperture Bypass Benchmark (64GB System) ===" << std::endl;
        std::cout << "System RAM: " << config_.system_ram_gb << " GB" << std::endl;
        std::cout << "VRAM: " << config_.vram_gb << " GB" << std::endl;
        std::cout << "Aperture Pool: " << config_.aperture_gb << " GB" << std::endl;
        std::cout << std::endl;
        
        // Initialize bridge
        if (!InitializeSovereignBridge(config_.aperture_gb)) {
            std::cerr << "FAILED: Could not initialize sovereign bridge" << std::endl;
            return;
        }
        
        auto& bridge = GetSovereignBridge();
        std::cout << "Bridge initialized:" << std::endl;
        std::cout << "  Large pages: " << (bridge.LargePagesActive() ? "YES" : "NO (fallback)") << std::endl;
        std::cout << "  NUMA optimized: " << (bridge.NUMAOptimized() ? "YES" : "NO") << std::endl;
        std::cout << std::endl;

        allocation_fallback_ = !bridge.LargePagesActive();
        
        // Run benchmarks
        BenchmarkAllocation();
        BenchmarkPinning();
        BenchmarkPrefetch();
        BenchmarkActivation();
        BenchmarkMoEPattern();
        BenchmarkTieredOverflow();
        
        // Cleanup
        ShutdownSovereignBridge();
        
        std::cout << "\n=== Benchmark Complete ===" << std::endl;
    }
    
private:
    void BenchmarkAllocation() {
        std::cout << "[Benchmark] Memory Allocation" << std::endl;
        
        auto& bridge = GetSovereignBridge();
        size_t block_size = 1024 * 1024 * 1024; // 1GB blocks
        int num_blocks = 10;
        
        // Warmup
        for (int i = 0; i < config_.warmup_iterations; i++) {
            void* ptr = bridge.AllocateApertureSpace(64 * 1024 * 1024);
            (void)ptr;
        }
        
        // Benchmark
        auto start = high_resolution_clock::now();
        for (int i = 0; i < num_blocks; i++) {
            void* ptr = bridge.AllocateApertureSpace(block_size);
            allocated_blocks_.push_back(ptr);
        }
        auto end = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(end - start);
        double avg_time = duration.count() / (double)num_blocks;
        
        std::cout << "  Allocated " << num_blocks << " x 1GB blocks" << std::endl;
        std::cout << "  Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " us" << std::endl;
        std::cout << "  Throughput: " << std::setprecision(2) 
                  << (1000000.0 / avg_time) * block_size / (1024.0 * 1024 * 1024) 
                  << " GB/s" << std::endl;
        std::cout << std::endl;

        latest_throughput_gbps_ = (1000000.0 / avg_time) * block_size / (1024.0 * 1024 * 1024);
        TelemetryFrame frame{};
        frame.memory_throughput_gbps = latest_throughput_gbps_;
        frame.activation_us = 0.0;
        frame.tokens_per_sec = 0.0;
        frame.prefetch_depth = 1;
        frame.utilization = static_cast<float>(std::min(1.0, latest_throughput_gbps_ / 31.5));
        frame.tier = 0;
        frame.allocation_fallback = allocation_fallback_;
        frame.large_pages_enabled = !allocation_fallback_;
        frame.cache_efficiency_score = frame.utilization;
        EmitPatchIfNeeded("allocation", frame);
    }
    
    void BenchmarkPinning() {
        std::cout << "[Benchmark] Memory Pinning" << std::endl;
        
        size_t test_size = 512 * 1024 * 1024; // 512MB
        
        // Warmup
        for (int i = 0; i < config_.warmup_iterations; i++) {
            RawrPinMemory(allocated_blocks_[0], test_size);
            RawrUnpinMemory(allocated_blocks_[0], test_size);
        }
        
        // Benchmark
        auto start = high_resolution_clock::now();
        for (int i = 0; i < config_.benchmark_iterations; i++) {
            RawrPinMemory(allocated_blocks_[i % allocated_blocks_.size()], test_size);
        }
        auto end = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(end - start);
        double avg_time = duration.count() / (double)config_.benchmark_iterations;
        
        std::cout << "  Test size: " << test_size / (1024 * 1024) << " MB" << std::endl;
        std::cout << "  Average pin time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " us" << std::endl;
        std::cout << "  Pin rate: " << std::setprecision(2) 
                  << (test_size / (1024.0 * 1024)) / (avg_time / 1000.0) 
                  << " MB/ms" << std::endl;
        std::cout << std::endl;
    }
    
    void BenchmarkPrefetch() {
        std::cout << "[Benchmark] Non-Temporal Prefetch" << std::endl;
        
        // Test different prefetch sizes
        std::vector<size_t> sizes = {64 * 1024 * 1024, 256 * 1024 * 1024, 1024 * 1024 * 1024};
        
        for (size_t size : sizes) {
            // Warmup
            for (int i = 0; i < 5; i++) {
                RawrPrefetchMemory(allocated_blocks_[0], size);
            }
            
            // Benchmark
            auto start = high_resolution_clock::now();
            for (int i = 0; i < 20; i++) {
                RawrPrefetchMemory(allocated_blocks_[i % allocated_blocks_.size()], size);
            }
            auto end = high_resolution_clock::now();
            
            auto duration = duration_cast<microseconds>(end - start);
            double avg_time = duration.count() / 20.0;
            double throughput = (size / (1024.0 * 1024 * 1024)) / (avg_time / 1000000.0);
            
            std::cout << "  Size: " << size / (1024 * 1024) << " MB" << std::endl;
            std::cout << "    Avg time: " << std::fixed << std::setprecision(2) 
                      << avg_time << " us" << std::endl;
            std::cout << "    Throughput: " << std::setprecision(2) 
                      << throughput << " GB/s" << std::endl;

            latest_throughput_gbps_ = throughput;
            TelemetryFrame frame{};
            frame.memory_throughput_gbps = throughput;
            frame.prefetch_depth = 1;
            frame.utilization = static_cast<float>(std::min(1.0, throughput / 31.5));
            frame.tier = 0;
            frame.allocation_fallback = allocation_fallback_;
            frame.large_pages_enabled = !allocation_fallback_;
            frame.cache_efficiency_score = frame.utilization;
            EmitPatchIfNeeded("prefetch", frame);
        }
        std::cout << std::endl;
    }
    
    void BenchmarkActivation() {
        std::cout << "[Benchmark] Full Activation (Prefetch + Flush + Barrier)" << std::endl;
        
        auto& bridge = GetSovereignBridge();
        size_t tensor_size = 1024 * 1024 * 1024; // 1GB tensor
        
        // Warmup
        for (int i = 0; i < config_.warmup_iterations; i++) {
            bridge.ActivateAperture(allocated_blocks_[0], tensor_size);
        }
        
        // Benchmark
        auto start = high_resolution_clock::now();
        for (int i = 0; i < config_.benchmark_iterations; i++) {
            bridge.ActivateAperture(allocated_blocks_[i % allocated_blocks_.size()], tensor_size);
        }
        auto end = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(end - start);
        double avg_time = duration.count() / (double)config_.benchmark_iterations;
        double throughput = (tensor_size / (1024.0 * 1024 * 1024)) / (avg_time / 1000000.0);
        
        std::cout << "  Tensor size: 1 GB" << std::endl;
        std::cout << "  Average activation: " << std::fixed << std::setprecision(2) 
                  << avg_time << " us" << std::endl;
        std::cout << "  Effective throughput: " << std::setprecision(2) 
                  << throughput << " GB/s" << std::endl;
        std::cout << "  PCIe 4.0 utilization: " << std::setprecision(1) 
                  << (throughput / 31.5) * 100.0 << "%" << std::endl;
        std::cout << std::endl;

        latest_throughput_gbps_ = throughput;
        latest_activation_us_ = avg_time;
        TelemetryFrame frame{};
        frame.memory_throughput_gbps = throughput;
        frame.activation_us = avg_time;
        frame.prefetch_depth = 2;
        frame.utilization = static_cast<float>(std::min(1.0, throughput / 31.5));
        frame.tier = 1;
        frame.allocation_fallback = allocation_fallback_;
        frame.large_pages_enabled = !allocation_fallback_;
        frame.cache_efficiency_score = std::max(0.0, std::min(1.0, throughput / 31.5));
        EmitPatchIfNeeded("activation", frame);
    }
    
    void BenchmarkMoEPattern() {
        std::cout << "[Benchmark] MoE Expert Loading Pattern" << std::endl;
        
        auto& bridge = GetSovereignBridge();
        auto& worker = GetPrefetchWorker();
        
        size_t expert_size = config_.expert_size_mb * 1024 * 1024;
        
        std::cout << "  Simulating " << config_.num_experts << " experts (" 
                  << config_.expert_size_mb << " MB each)" << std::endl;
        std::cout << "  Active experts per token: " << config_.active_experts << std::endl;
        std::cout << "  Lookahead depth: " << config_.lookahead_depth << std::endl;
        
        // Simulate token generation
        int num_tokens = 20;
        double total_time = 0;
        
        for (int token = 0; token < num_tokens; token++) {
            // Simulate router selecting top-k experts
            std::vector<int> selected_experts;
            for (int i = 0; i < config_.active_experts; i++) {
                selected_experts.push_back(rng_() % config_.num_experts);
            }
            
            auto start = high_resolution_clock::now();
            
            // Stage selected experts
            for (int expert_idx : selected_experts) {
                void* expert_ptr = allocated_blocks_[expert_idx % allocated_blocks_.size()];
                bridge.ActivateAperture(expert_ptr, expert_size);
            }
            
            // Lookahead prefetch
            for (int i = 0; i < config_.lookahead_depth; i++) {
                int next_expert = rng_() % config_.num_experts;
                void* next_ptr = allocated_blocks_[next_expert % allocated_blocks_.size()];
                worker.QueuePrefetch(next_ptr, expert_size);
            }
            
            auto end = high_resolution_clock::now();
            total_time += duration_cast<microseconds>(end - start).count();
        }
        
        double avg_token_time = total_time / num_tokens;
        double tokens_per_sec = 1000000.0 / avg_token_time;
        
        std::cout << "  Average token time: " << std::fixed << std::setprecision(2) 
                  << avg_token_time << " us" << std::endl;
        std::cout << "  Estimated tokens/sec: " << std::setprecision(2) 
                  << tokens_per_sec << std::endl;
        std::cout << std::endl;

        TelemetryFrame frame{};
        frame.memory_throughput_gbps = latest_throughput_gbps_;
        frame.activation_us = latest_activation_us_;
        frame.tokens_per_sec = tokens_per_sec;
        frame.prefetch_depth = static_cast<int>(config_.lookahead_depth);
        frame.utilization = static_cast<float>(std::min(1.0, latest_throughput_gbps_ / 31.5));
        frame.tier = frame.utilization > 0.85f ? 2 : (frame.utilization > 0.70f ? 1 : 0);
        frame.allocation_fallback = allocation_fallback_;
        frame.large_pages_enabled = !allocation_fallback_;
        frame.cache_efficiency_score = std::max(0.0, std::min(1.0, frame.tokens_per_sec / 120.0));
        EmitPatchIfNeeded("moe", frame);
    }
    
    void BenchmarkTieredOverflow() {
        std::cout << "[Benchmark] Tiered Overflow Management" << std::endl;
        
        auto& bridge = GetSovereignBridge();
        
        // Simulate memory pressure levels
        struct TierTest {
            const char* name;
            float utilization;
            size_t prefetch_depth;
        };
        
        std::vector<TierTest> tiers = {
            {"NORMAL", 0.60f, 1},
            {"WARNING", 0.75f, 2},
            {"CRITICAL", 0.90f, 4},
        };
        
        size_t test_size = 256 * 1024 * 1024;
        
        for (const auto& tier : tiers) {
            std::cout << "  Tier: " << tier.name << " (" << (int)(tier.utilization * 100) 
                      << "% utilization)" << std::endl;
            
            auto start = high_resolution_clock::now();
            
            // Simulate prefetch at this tier's depth
            for (size_t i = 0; i < tier.prefetch_depth; i++) {
                RawrPrefetchMemory(allocated_blocks_[i % allocated_blocks_.size()], 
                                test_size / tier.prefetch_depth);
            }
            
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            const double tier_throughput = (test_size / (1024.0 * 1024 * 1024)) / (duration.count() / 1000000.0);
            
            std::cout << "    Prefetch depth: " << tier.prefetch_depth << std::endl;
            std::cout << "    Time: " << duration.count() << " us" << std::endl;

            TelemetryFrame frame{};
            frame.memory_throughput_gbps = tier_throughput;
            frame.activation_us = static_cast<double>(duration.count());
            frame.tokens_per_sec = 0.0;
            frame.prefetch_depth = static_cast<int>(tier.prefetch_depth);
            frame.utilization = tier.utilization;
            frame.tier = (std::string(tier.name) == "CRITICAL") ? 2 : (std::string(tier.name) == "WARNING" ? 1 : 0);
            frame.allocation_fallback = allocation_fallback_;
            frame.large_pages_enabled = !allocation_fallback_;
            frame.cache_efficiency_score = std::max(0.0, std::min(1.0, tier_throughput / 31.5));
            EmitPatchIfNeeded("tiered-overflow", frame);
        }
        
        std::cout << std::endl;
        std::cout << "  64GB System Recommendations:" << std::endl;
        std::cout << "    - NORMAL (<44.8GB): Standard operation" << std::endl;
        std::cout << "    - WARNING (44.8-54.4GB): Enable 2-page prefetch" << std::endl;
        std::cout << "    - CRITICAL (54.4-60.8GB): Streaming prefetch depth=4" << std::endl;
        std::cout << "    - PANIC (>60.8GB): Enable compression + NVMe swap" << std::endl;
        std::cout << std::endl;
    }

    void EmitPatchIfNeeded(const char* phase, const TelemetryFrame& frame) {
        RuntimeSignal signal = interpreter_.Interpret(frame);
        if (signal.severity <= 0.60f || signal.type == RuntimeSignalType::STABLE) {
            return;
        }
        PatchSuggestion suggestion = patch_engine_.GeneratePatch(signal);
        patch_engine_.Emit(signal, suggestion, config_.patch_mode, phase, frame);
    }
};

int main(int argc, char** argv) {
    BenchmarkConfig config;
    
    // Parse command line args
    if (argc > 1) {
        config.aperture_gb = std::atoi(argv[1]);
    }

    if (argc > 2) {
        const std::string mode_arg = argv[2];
        if (mode_arg == "--auto-low-risk") {
            config.patch_mode = PatchMode::AUTO_APPLY_LOW_RISK;
        } else if (mode_arg == "--auto-apply-all") {
            config.patch_mode = PatchMode::AUTO_APPLY_ALL;
        }
    }
    
    std::cout << "RawrXD Aperture Bypass Benchmark" << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Target: 64GB DDR5 + 16GB VRAM system" << std::endl;
    std::cout << "Aperture pool: " << config.aperture_gb << " GB" << std::endl;
    std::cout << "Autopatch mode: "
              << (config.patch_mode == PatchMode::AUTO_APPLY_ALL ? "AUTO_APPLY_ALL" :
                  (config.patch_mode == PatchMode::AUTO_APPLY_LOW_RISK ? "AUTO_APPLY_LOW_RISK" : "SUGGEST_ONLY"))
              << std::endl;
    std::cout << std::endl;
    
    ApertureBenchmark benchmark(config);
    benchmark.RunAllBenchmarks();
    
    return 0;
}
