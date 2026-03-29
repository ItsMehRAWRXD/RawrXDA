#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace RawrXD
{
namespace Core
{
struct FallbackRouteCounter
{
    int backendSuccess = 0;
    int backendFailure = 0;
    int guiFallback = 0;
};

inline std::mutex& FallbackRouteMetricsMutex()
{
    static std::mutex mtx;
    return mtx;
}

inline std::unordered_map<std::string, FallbackRouteCounter>& FallbackRouteMetricsMap()
{
    static std::unordered_map<std::string, FallbackRouteCounter> counters;
    return counters;
}

inline std::string BuildFallbackKey(const std::string& surface, const std::string& handler, const std::string& tool)
{
    return surface + "|" + handler + "|" + tool;
}

inline void RecordFallbackRoute(const std::string& surface, const std::string& handler, const std::string& tool,
                                bool backendSuccess, bool guiFallback)
{
    std::lock_guard<std::mutex> lock(FallbackRouteMetricsMutex());
    auto& entry = FallbackRouteMetricsMap()[BuildFallbackKey(surface, handler, tool)];
    if (backendSuccess)
        ++entry.backendSuccess;
    else
        ++entry.backendFailure;
    if (guiFallback)
        ++entry.guiFallback;
}

inline nlohmann::json SnapshotFallbackRouteMetrics()
{
    std::lock_guard<std::mutex> lock(FallbackRouteMetricsMutex());
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& kv : FallbackRouteMetricsMap())
    {
        const std::string& key = kv.first;
        const auto& counter = kv.second;
        const size_t s1 = key.find('|');
        const size_t s2 = (s1 == std::string::npos) ? std::string::npos : key.find('|', s1 + 1);
        const std::string surface = (s1 == std::string::npos) ? key : key.substr(0, s1);
        const std::string handler =
            (s1 == std::string::npos || s2 == std::string::npos) ? "" : key.substr(s1 + 1, s2 - (s1 + 1));
        const std::string tool = (s2 == std::string::npos) ? "" : key.substr(s2 + 1);
        rows.push_back({{"surface", surface},
                        {"handler", handler},
                        {"tool", tool},
                        {"backendSuccess", counter.backendSuccess},
                        {"backendFailure", counter.backendFailure},
                        {"guiFallback", counter.guiFallback}});
    }
    return rows;
}

inline nlohmann::json SnapshotFallbackRouteMetricsBySurface(const std::string& surfaceFilter)
{
    if (surfaceFilter.empty())
        return SnapshotFallbackRouteMetrics();
    std::lock_guard<std::mutex> lock(FallbackRouteMetricsMutex());
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& kv : FallbackRouteMetricsMap())
    {
        const std::string& key = kv.first;
        const auto& counter = kv.second;
        const size_t s1 = key.find('|');
        const size_t s2 = (s1 == std::string::npos) ? std::string::npos : key.find('|', s1 + 1);
        const std::string surface = (s1 == std::string::npos) ? key : key.substr(0, s1);
        if (surface != surfaceFilter)
            continue;
        const std::string handler =
            (s1 == std::string::npos || s2 == std::string::npos) ? "" : key.substr(s1 + 1, s2 - (s1 + 1));
        const std::string tool = (s2 == std::string::npos) ? "" : key.substr(s2 + 1);
        rows.push_back({{"surface", surface},
                        {"handler", handler},
                        {"tool", tool},
                        {"backendSuccess", counter.backendSuccess},
                        {"backendFailure", counter.backendFailure},
                        {"guiFallback", counter.guiFallback}});
    }
    return rows;
}

inline nlohmann::json SnapshotFallbackRouteMetricsTotals(const std::string& surfaceFilter = std::string())
{
    std::lock_guard<std::mutex> lock(FallbackRouteMetricsMutex());
    int backendSuccess = 0;
    int backendFailure = 0;
    int guiFallback = 0;
    std::unordered_map<std::string, FallbackRouteCounter> perSurface;

    for (const auto& kv : FallbackRouteMetricsMap())
    {
        const std::string& key = kv.first;
        const auto& counter = kv.second;
        const size_t s1 = key.find('|');
        const std::string surface = (s1 == std::string::npos) ? key : key.substr(0, s1);
        if (!surfaceFilter.empty() && surface != surfaceFilter)
            continue;

        backendSuccess += counter.backendSuccess;
        backendFailure += counter.backendFailure;
        guiFallback += counter.guiFallback;
        auto& agg = perSurface[surface];
        agg.backendSuccess += counter.backendSuccess;
        agg.backendFailure += counter.backendFailure;
        agg.guiFallback += counter.guiFallback;
    }

    nlohmann::json perSurfaceJson = nlohmann::json::array();
    for (const auto& kv : perSurface)
    {
        perSurfaceJson.push_back({{"surface", kv.first},
                                  {"backendSuccess", kv.second.backendSuccess},
                                  {"backendFailure", kv.second.backendFailure},
                                  {"guiFallback", kv.second.guiFallback}});
    }

    return {{"backendSuccess", backendSuccess},
            {"backendFailure", backendFailure},
            {"guiFallback", guiFallback},
            {"perSurface", perSurfaceJson}};
}

inline nlohmann::json FallbackMetricSurfacesCatalog()
{
    return nlohmann::json::array(
        {nlohmann::json{{"surface", "autoFeatureStub"},
                        {"description", "Auto-feature macro stubs with backend-first routing + GUI fallback"}},
         nlohmann::json{{"surface", "autoMissing"},
                        {"description", "SSOT auto-missing handlers routed through shared fallback router"}},
         nlohmann::json{{"surface", "missingProvider"},
                        {"description", "SSOT missing-provider handlers routed through shared fallback router"}},
         nlohmann::json{{"surface", "linkerGap"},
                        {"description", "SSOT linker-gap handlers with backend-first route resolution"}}});
}

inline void ResetFallbackRouteMetrics()
{
    std::lock_guard<std::mutex> lock(FallbackRouteMetricsMutex());
    FallbackRouteMetricsMap().clear();
}
}  // namespace Core
}  // namespace RawrXD
