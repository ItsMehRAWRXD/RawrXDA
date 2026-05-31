// ab_harness_measurement.cpp
// Dual-mode A/B harness for measuring aperture behavior:
// fallback mode vs locked-page mode
// Logs: init convergence, tier transitions, page-fault stats, TPS

#include "rawr_sovereign_bridge.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace rawr;
using namespace std::chrono;

struct ABMeasurement {
    std::string mode_name;                // "fallback" or "locked_pages"
    
    // Initialization metrics
    int init_retries = 0;
    int init_total_attempts = 0;
    size_t final_aperture_size_gb = 0;
    double init_time_ms = 0.0;
    bool init_success = false;
    
    // Runtime metrics (over 1k token window)
    int tier_transitions_per_1k_tokens = 0;
    int tier_0_count = 0;  // normal
    int tier_1_count = 0;  // warning
    int tier_2_count = 0;  // critical
    int tier_3_count = 0;  // panic
    
    // Memory pressure
    double avg_utilization_percent = 0.0;
    double max_utilization_percent = 0.0;
    
    // Page fault metrics (if available)
    unsigned long page_faults_minor = 0;
    unsigned long page_faults_major = 0;
    
    // Throughput and latency
    double tokens_per_sec = 0.0;
    double avg_latency_us = 0.0;
    double std_dev_latency_us = 0.0;
    
    // Variance/stability
    double throughput_variance = 0.0;
    
    // Notes
    std::string notes;
};

class ABHarness {
private:
    std::vector<ABMeasurement> measurements_;
    std::ofstream csv_log_;
    
    struct TokenFrame {
        int tier = 0;
        double utilization = 0.0;
        double latency_us = 0.0;
    };
    
public:
    ABHarness(const std::string& csv_path) {
        csv_log_.open(csv_path, std::ios::app);
        if (csv_log_.is_open()) {
            // Write CSV header if file is empty
            csv_log_ << "Mode,InitRetries,FinalApertureGB,InitTimeMS,"
                     << "TierTransitionsPerK,Tier0Count,Tier1Count,Tier2Count,Tier3Count,"
                     << "AvgUtilPercent,MaxUtilPercent,"
                     << "PageFaultsMinor,PageFaultsMajor,"
                     << "TokensPerSec,AvgLatencyUS,StdDevLatencyUS,"
                     << "ThroughputVariance,Notes\n";
        }
    }
    
    ~ABHarness() {
        if (csv_log_.is_open()) csv_log_.close();
    }
    
    void RunDualModeTest(size_t target_aperture_gb = 48) {
        std::cout << "\n========== A/B HARNESS: DUAL-MODE MEASUREMENT ==========\n" << std::endl;
        
        // Mode 1: Fallback (pageable memory, adaptive sizing)
        ABMeasurement fallback_result = RunModeTest(
            "fallback",
            true,  // force_fallback
            target_aperture_gb
        );
        
        // Mode 2: Locked Pages (if privilege available)
        ABMeasurement locked_result = RunModeTest(
            "locked_pages",
            false,  // allow_privilege
            target_aperture_gb
        );
        
        measurements_.push_back(fallback_result);
        measurements_.push_back(locked_result);
        
        // Print side-by-side comparison
        PrintComparison(fallback_result, locked_result);
        
        // Write to CSV
        WriteCSV(fallback_result);
        WriteCSV(locked_result);
    }
    
private:
    ABMeasurement RunModeTest(const std::string& mode_name, bool force_fallback, size_t target_aperture_gb) {
        ABMeasurement result;
        result.mode_name = mode_name;
        
        std::cout << "\n--- MODE: " << mode_name << " ---" << std::endl;
        
        // === INITIALIZATION PHASE ===
        auto init_start = high_resolution_clock::now();
        
        int init_attempts = 0;
        size_t current_aperture = target_aperture_gb;
        bool init_success = false;
        
        while (init_attempts < 5 && current_aperture >= 16) {
            init_attempts++;
            std::cout << "  Init attempt " << init_attempts 
                      << ": trying aperture size " << current_aperture << " GB... ";
            
            if (InitializeSovereignBridgeWithSize(current_aperture, force_fallback)) {
                init_success = true;
                std::cout << "SUCCESS" << std::endl;
                break;
            } else {
                std::cout << "RETRY (downsizing)" << std::endl;
                current_aperture -= 8;  // Downshift by 8GB
            }
        }
        
        auto init_end = high_resolution_clock::now();
        double init_duration_ms = duration<double, std::milli>(init_end - init_start).count();
        
        result.init_success = init_success;
        result.init_retries = init_attempts - 1;
        result.init_total_attempts = init_attempts;
        result.final_aperture_size_gb = current_aperture;
        result.init_time_ms = init_duration_ms;
        
        if (!init_success) {
            result.notes = "Init failed after " + std::to_string(init_attempts) + " attempts";
            return result;
        }
        
        auto& bridge = GetSovereignBridge();
        bool large_pages_active = bridge.LargePagesActive();
        
        std::cout << "  Large pages active: " << (large_pages_active ? "YES" : "NO") << std::endl;
        std::cout << "  Init time: " << std::fixed << std::setprecision(2) << init_duration_ms << " ms" << std::endl;
        
        // === RUNTIME PHASE (synthetic 1k-token window) ===
        std::cout << "\n  Running 1k-token simulation..." << std::endl;
        
        std::vector<TokenFrame> token_frames;
        int tier_transitions = 0;
        double total_utilization = 0.0;
        double max_utilization = 0.0;
        std::vector<double> latencies;
        
        for (int token = 0; token < 1000; token++) {
            // Query tier and utilization
            float util = bridge.GetMemoryUtilization();
            int tier = bridge.GetCurrentTier();
            
            // Measure simulated latency (activation time)
            auto token_start = high_resolution_clock::now();
            
            // Simulate token processing: allocate temp buffer, prefetch, process
            void* temp_buf = bridge.AllocateApertureSpace(1024 * 1024);  // 1MB temp
            if (temp_buf) {
                // Simulate prefetch
                bridge.SetPrefetchDepth(tier == 2 ? 2 : tier == 1 ? 3 : 4);
                
                // Simulate activation compute (spin)
                volatile int x = 0;
                for (int i = 0; i < 10000; i++) {
                    x += i;
                }
                
                bridge.FreeApertureSpace(temp_buf);
            }
            
            auto token_end = high_resolution_clock::now();
            double latency_us = duration<double, std::micro>(token_end - token_start).count();
            latencies.push_back(latency_us);
            
            // Track tier transitions
            if (token_frames.size() > 0 && token_frames.back().tier != tier) {
                tier_transitions++;
            }
            
            total_utilization += util;
            max_utilization = std::max(max_utilization, (double)util);
            
            TokenFrame frame{tier, (double)util, latency_us};
            token_frames.push_back(frame);
            
            // Track tier counts
            if (tier == 0) result.tier_0_count++;
            else if (tier == 1) result.tier_1_count++;
            else if (tier == 2) result.tier_2_count++;
            else if (tier == 3) result.tier_3_count++;
        }
        
        // === AGGREGATE METRICS ===
        result.tier_transitions_per_1k_tokens = tier_transitions;
        result.avg_utilization_percent = (total_utilization / 1000.0) * 100.0;
        result.max_utilization_percent = max_utilization * 100.0;
        
        if (!latencies.empty()) {
            double sum_latency = 0.0;
            for (double lat : latencies) sum_latency += lat;
            result.avg_latency_us = sum_latency / latencies.size();
            
            // Standard deviation
            double variance = 0.0;
            for (double lat : latencies) {
                variance += (lat - result.avg_latency_us) * (lat - result.avg_latency_us);
            }
            variance /= latencies.size();
            result.std_dev_latency_us = std::sqrt(variance);
            
            // Estimated TPS (assuming ~100us per token average in this synthetic test)
            if (result.avg_latency_us > 0) {
                result.tokens_per_sec = 1000000.0 / result.avg_latency_us;
            }
            
            // Throughput variance as coefficient of variation
            result.throughput_variance = result.std_dev_latency_us / result.avg_latency_us;
        }
        
        // Read page fault stats if available
        result.page_faults_minor = 0;
        result.page_faults_major = 0;
        
        #ifdef _WIN32
            // On Windows, this would require querying process performance counters
            // For now, we'll note that this is not available
            result.notes = "Page fault stats not available on Windows (requires WMI/PerfCounter)";
        #else
            // On Linux/Unix, read from /proc/self/stat
            // Placeholder for now
        #endif
        
        ShutdownSovereignBridge();
        
        return result;
    }
    
    void PrintComparison(const ABMeasurement& fallback, const ABMeasurement& locked) {
        std::cout << "\n========== A/B COMPARISON ==========\n" << std::endl;
        
        std::cout << std::setw(30) << "Metric" 
                  << std::setw(20) << "Fallback" 
                  << std::setw(20) << "Locked Pages" 
                  << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        // Init metrics
        std::cout << std::setw(30) << "Init Retries" 
                  << std::setw(20) << fallback.init_retries 
                  << std::setw(20) << locked.init_retries 
                  << std::endl;
        
        std::cout << std::setw(30) << "Final Aperture (GB)" 
                  << std::setw(20) << fallback.final_aperture_size_gb 
                  << std::setw(20) << locked.final_aperture_size_gb 
                  << std::endl;
        
        std::cout << std::setw(30) << "Init Time (ms)" 
                  << std::setw(20) << std::fixed << std::setprecision(2) << fallback.init_time_ms
                  << std::setw(20) << std::fixed << std::setprecision(2) << locked.init_time_ms 
                  << std::endl;
        
        // Tier metrics
        std::cout << std::setw(30) << "Tier Transitions/1K" 
                  << std::setw(20) << fallback.tier_transitions_per_1k_tokens 
                  << std::setw(20) << locked.tier_transitions_per_1k_tokens 
                  << std::endl;
        
        std::cout << std::setw(30) << "Tier 0 Count" 
                  << std::setw(20) << fallback.tier_0_count 
                  << std::setw(20) << locked.tier_0_count 
                  << std::endl;
        
        std::cout << std::setw(30) << "Tier 1 Count" 
                  << std::setw(20) << fallback.tier_1_count 
                  << std::setw(20) << locked.tier_1_count 
                  << std::endl;
        
        std::cout << std::setw(30) << "Tier 2 Count" 
                  << std::setw(20) << fallback.tier_2_count 
                  << std::setw(20) << locked.tier_2_count 
                  << std::endl;
        
        // Utilization
        std::cout << std::setw(30) << "Avg Utilization (%)" 
                  << std::setw(20) << std::fixed << std::setprecision(1) << fallback.avg_utilization_percent 
                  << std::setw(20) << std::fixed << std::setprecision(1) << locked.avg_utilization_percent 
                  << std::endl;
        
        std::cout << std::setw(30) << "Max Utilization (%)" 
                  << std::setw(20) << std::fixed << std::setprecision(1) << fallback.max_utilization_percent 
                  << std::setw(20) << std::fixed << std::setprecision(1) << locked.max_utilization_percent 
                  << std::endl;
        
        // Throughput and latency
        std::cout << std::setw(30) << "Tokens/Sec (est.)" 
                  << std::setw(20) << std::fixed << std::setprecision(0) << fallback.tokens_per_sec 
                  << std::setw(20) << std::fixed << std::setprecision(0) << locked.tokens_per_sec 
                  << std::endl;
        
        std::cout << std::setw(30) << "Avg Latency (us)" 
                  << std::setw(20) << std::fixed << std::setprecision(2) << fallback.avg_latency_us 
                  << std::setw(20) << std::fixed << std::setprecision(2) << locked.avg_latency_us 
                  << std::endl;
        
        std::cout << std::setw(30) << "Latency Std Dev (us)" 
                  << std::setw(20) << std::fixed << std::setprecision(2) << fallback.std_dev_latency_us 
                  << std::setw(20) << std::fixed << std::setprecision(2) << locked.std_dev_latency_us 
                  << std::endl;
        
        std::cout << std::setw(30) << "Throughput Variance" 
                  << std::setw(20) << std::fixed << std::setprecision(3) << fallback.throughput_variance 
                  << std::setw(20) << std::fixed << std::setprecision(3) << locked.throughput_variance 
                  << std::endl;
        
        std::cout << "\n========== KEY INSIGHTS ==========\n" << std::endl;
        
        // Interpret results
        if (fallback.init_retries == 0 && locked.init_retries == 0) {
            std::cout << "✓ Both modes initialize deterministicly (zero retries)" << std::endl;
        } else if (locked.init_retries < fallback.init_retries) {
            std::cout << "✓ Locked pages init determinism improved by " 
                      << (fallback.init_retries - locked.init_retries) << " retries" << std::endl;
        }
        
        if (locked.tier_transitions_per_1k_tokens < fallback.tier_transitions_per_1k_tokens) {
            std::cout << "✓ Locked pages show fewer tier transitions (" 
                      << locked.tier_transitions_per_1k_tokens << " vs " 
                      << fallback.tier_transitions_per_1k_tokens << ")" << std::endl;
        }
        
        if (locked.std_dev_latency_us < fallback.std_dev_latency_us) {
            double variance_improvement = (fallback.std_dev_latency_us - locked.std_dev_latency_us) / 
                                         fallback.std_dev_latency_us * 100.0;
            std::cout << "✓ Locked pages reduce latency variance by " 
                      << std::fixed << std::setprecision(1) << variance_improvement << "%" << std::endl;
        }
        
        if (locked.tokens_per_sec > fallback.tokens_per_sec * 1.02) {
            double tps_gain = (locked.tokens_per_sec - fallback.tokens_per_sec) / 
                             fallback.tokens_per_sec * 100.0;
            std::cout << "✓ Locked pages show throughput gain of " 
                      << std::fixed << std::setprecision(1) << tps_gain << "%" << std::endl;
        }
        
        std::cout << "\n" << std::endl;
    }
    
    void WriteCSV(const ABMeasurement& m) {
        if (!csv_log_.is_open()) return;
        
        csv_log_ << m.mode_name << ","
                 << m.init_retries << ","
                 << m.final_aperture_size_gb << ","
                 << std::fixed << std::setprecision(2) << m.init_time_ms << ","
                 << m.tier_transitions_per_1k_tokens << ","
                 << m.tier_0_count << ","
                 << m.tier_1_count << ","
                 << m.tier_2_count << ","
                 << m.tier_3_count << ","
                 << std::fixed << std::setprecision(1) << m.avg_utilization_percent << ","
                 << std::fixed << std::setprecision(1) << m.max_utilization_percent << ","
                 << m.page_faults_minor << ","
                 << m.page_faults_major << ","
                 << std::fixed << std::setprecision(0) << m.tokens_per_sec << ","
                 << std::fixed << std::setprecision(2) << m.avg_latency_us << ","
                 << std::fixed << std::setprecision(2) << m.std_dev_latency_us << ","
                 << std::fixed << std::setprecision(3) << m.throughput_variance << ","
                 << m.notes << "\n";
        
        csv_log_.flush();
    }
    
    bool InitializeSovereignBridgeWithSize(size_t aperture_gb, bool force_fallback) {
        // Placeholder: this would call actual sovereign bridge init with size parameter
        // For now, return success to show structure
        return true;
    }
};

int main() {
    ABHarness harness("d:\\ab_measurement_results.csv");
    harness.RunDualModeTest(48);  // 48GB aperture target on 64GB system
    return 0;
}
