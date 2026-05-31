#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <memory>

namespace RawrXD {

/**
 * PerformanceProfiler - Measures hot path performance for optimization
 *
 * Day 13 profiling targets:
 * - Model loading time (<2s target)
 * - Token generation latency (<100ms target)
 * - IDE startup (<3s target)
 * - Completion response (<200ms target)
 * - Memory peak/steady-state
 */
class PerformanceProfiler {
public:
    struct Measurement {
        std::string name;
        std::chrono::duration<double, std::milli> duration;
        uint64_t call_count = 0;
        std::chrono::duration<double, std::milli> total_duration;
        std::chrono::duration<double, std::milli> min_duration;
        std::chrono::duration<double, std::milli> max_duration;
    };

    struct ScopeGuard {
        PerformanceProfiler& profiler;
        std::string scope_name;
        std::chrono::high_resolution_clock::time_point start_time;

        ScopeGuard(PerformanceProfiler& prof, const std::string& name)
            : profiler(prof), scope_name(name),
              start_time(std::chrono::high_resolution_clock::now()) {}

        ~ScopeGuard()
        {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);
            profiler.Record(scope_name, duration);
        }
    };

    PerformanceProfiler() = default;
    ~PerformanceProfiler() = default;

    /**
     * Record a measurement for a scope.
     * Called automatically by ScopeGuard or manually.
     */
    void Record(const std::string& name, std::chrono::duration<double, std::milli> duration);

    /**
     * Get all measurements.
     */
    const std::unordered_map<std::string, Measurement>& GetMeasurements() const;

    /**
     * Get a specific measurement.
     */
    const Measurement* GetMeasurement(const std::string& name) const;

    /**
     * Reset all measurements.
     */
    void Reset();

    /**
     * Generate a performance report.
     */
    std::string GenerateReport() const;

    /**
     * Get measurements exceeding a threshold (for hot path identification).
     */
    std::vector<Measurement> GetMeasurementsAboveThreshold(
        std::chrono::duration<double, std::milli> threshold) const;

    /**
     * Create a scope guard for automatic timing.
     */
    std::unique_ptr<ScopeGuard> TimeScope(const std::string& name)
    {
        return std::make_unique<ScopeGuard>(*this, name);
    }

private:
    std::unordered_map<std::string, Measurement> m_measurements;
    mutable std::mutex m_mutex;
};

/**
 * Global profiler accessor.
 */
PerformanceProfiler& GetGlobalProfiler();

/**
 * Convenience macro for timing scopes.
 */
#define PROFILE_SCOPE(name) \
    auto _profile_guard = RawrXD::GetGlobalProfiler().TimeScope(name)

} // namespace RawrXD

