// ================================================================
// RawrXD-ExecAI Benchmark Header
// Performance measurement utilities
// ================================================================
#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>

// ================================================================
// Timer Utility
// ================================================================
class BenchmarkTimer {
public:
    BenchmarkTimer() : start_time(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_time).count();
    }
    
    double elapsed_sec() const {
        return elapsed_ms() / 1000.0;
    }
    
    void reset() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_time;
};

// ================================================================
// Statistics Utilities
// ================================================================
template<typename T>
struct Statistics {
    T min;
    T max;
    T mean;
    T median;
    T p95;
    T p99;
    T stddev;
    
    static Statistics<T> compute(std::vector<T> data) {
        if (data.empty()) {
            return {T{}, T{}, T{}, T{}, T{}, T{}, T{}};
        }
        
        std::sort(data.begin(), data.end());
        
        Statistics<T> stats;
        stats.min = data.front();
        stats.max = data.back();
        stats.median = data[data.size() / 2];
        stats.p95 = data[(data.size() * 95) / 100];
        stats.p99 = data[(data.size() * 99) / 100];
        
        // Mean
        T sum = std::accumulate(data.begin(), data.end(), T{});
        stats.mean = sum / data.size();
        
        // Standard deviation
        T sq_sum = 0;
        for (const auto& val : data) {
            T diff = val - stats.mean;
            sq_sum += diff * diff;
        }
        stats.stddev = std::sqrt(sq_sum / data.size());
        
        return stats;
    }
};

// ================================================================
// Benchmark Result Formatting
// ================================================================
inline void print_statistics(const char* name, const Statistics<double>& stats, const char* unit = "ms") {
    printf("  %s:\n", name);
    printf("    Min:    %.2f %s\n", stats.min, unit);
    printf("    Mean:   %.2f %s\n", stats.mean, unit);
    printf("    Median: %.2f %s\n", stats.median, unit);
    printf("    P95:    %.2f %s\n", stats.p95, unit);
    printf("    P99:    %.2f %s\n", stats.p99, unit);
    printf("    Max:    %.2f %s\n", stats.max, unit);
    printf("    StdDev: %.2f %s\n", stats.stddev, unit);
}

// ================================================================
// Throughput Measurement
// ================================================================
class ThroughputMeter {
public:
    ThroughputMeter() 
        : items_processed(0)
        , start_time(std::chrono::high_resolution_clock::now()) 
    {}
    
    void add_items(uint64_t count) {
        items_processed += count;
    }
    
    double items_per_second() const {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        return (double)items_processed / elapsed;
    }
    
    uint64_t total_items() const {
        return items_processed;
    }
    
private:
    uint64_t items_processed;
    std::chrono::high_resolution_clock::time_point start_time;
};

#endif // BENCHMARK_H
