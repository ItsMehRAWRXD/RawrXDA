#include "performance_profiler.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {

static PerformanceProfiler g_global_profiler;

PerformanceProfiler& GetGlobalProfiler()
{
    return g_global_profiler;
}

void PerformanceProfiler::Record(const std::string& name, std::chrono::duration<double, std::milli> duration)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_measurements.find(name);
    if (it != m_measurements.end()) {
        Measurement& m = it->second;
        m.call_count++;
        m.total_duration += duration;
        m.duration = duration;
        
        if (duration < m.min_duration) {
            m.min_duration = duration;
        }
        if (duration > m.max_duration) {
            m.max_duration = duration;
        }
    } else {
        Measurement m;
        m.name = name;
        m.duration = duration;
        m.call_count = 1;
        m.total_duration = duration;
        m.min_duration = duration;
        m.max_duration = duration;
        m_measurements[name] = m;
    }
}

const std::unordered_map<std::string, PerformanceProfiler::Measurement>&
PerformanceProfiler::GetMeasurements() const
{
    return m_measurements;
}

const PerformanceProfiler::Measurement*
PerformanceProfiler::GetMeasurement(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_measurements.find(name);
    if (it != m_measurements.end()) {
        return &it->second;
    }
    return nullptr;
}

void PerformanceProfiler::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_measurements.clear();
}

std::string PerformanceProfiler::GenerateReport() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream oss;
    oss << "=== Performance Profile Report ===\n";
    oss << std::setw(40) << std::left << "Scope"
        << std::setw(12) << "Count"
        << std::setw(12) << "Total (ms)"
        << std::setw(12) << "Avg (ms)"
        << std::setw(12) << "Min (ms)"
        << std::setw(12) << "Max (ms)\n";
    oss << std::string(100, '-') << "\n";

    for (const auto& [name, measurement] : m_measurements) {
        double avg = measurement.call_count > 0 
            ? measurement.total_duration.count() / measurement.call_count 
            : 0.0;

        oss << std::setw(40) << std::left << name
            << std::setw(12) << measurement.call_count
            << std::setw(12) << std::fixed << std::setprecision(3) << measurement.total_duration.count()
            << std::setw(12) << std::fixed << std::setprecision(3) << avg
            << std::setw(12) << std::fixed << std::setprecision(3) << measurement.min_duration.count()
            << std::setw(12) << std::fixed << std::setprecision(3) << measurement.max_duration.count()
            << "\n";
    }
    
    oss << std::string(100, '-') << "\n";
    
    return oss.str();
}

std::vector<PerformanceProfiler::Measurement>
PerformanceProfiler::GetMeasurementsAboveThreshold(
    std::chrono::duration<double, std::milli> threshold) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Measurement> hot_paths;
    for (const auto& [name, measurement] : m_measurements) {
        double avg = measurement.call_count > 0 
            ? measurement.total_duration.count() / measurement.call_count 
            : 0.0;
        
        if (std::chrono::duration<double, std::milli>(avg) >= threshold) {
            hot_paths.push_back(measurement);
        }
    }
    
    // Sort by total time descending
    std::sort(hot_paths.begin(), hot_paths.end(),
        [](const Measurement& a, const Measurement& b) {
            return a.total_duration > b.total_duration;
        });
    
    return hot_paths;
}

} // namespace RawrXD

