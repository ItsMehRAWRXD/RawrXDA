#pragma once
#include <windows.h>
#include <string>
#include <sstream>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

namespace RawrXD {
namespace Metrics {

struct SystemMetrics {
    double cpu_percent;
    double memory_used_mb;
    double memory_total_mb;
    uint64_t requests_total;
    uint64_t requests_active;
    double avg_latency_ms;
};

class MetricsCollector {
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuCounter;
    uint64_t requestCount;
    uint64_t activeRequests;
    uint64_t totalLatencyUs;
    
public:
    MetricsCollector() : requestCount(0), activeRequests(0), totalLatencyUs(0) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
        PdhCollectQueryData(cpuQuery);
    }
    
    void RecordRequestStart() { InterlockedIncrement64((LONG64*)&activeRequests); InterlockedIncrement64((LONG64*)&requestCount); }
    void RecordRequestEnd(uint64_t latencyUs) { 
        InterlockedDecrement64((LONG64*)&activeRequests); 
        InterlockedAdd64((LONG64*)&totalLatencyUs, latencyUs);
    }
    
    SystemMetrics Collect() {
        SystemMetrics m = {};
        PDH_FMT_COUNTERVALUE cpuVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, nullptr, &cpuVal);
        m.cpu_percent = cpuVal.doubleValue;
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        m.memory_used_mb = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0);
        m.memory_total_mb = memStatus.ullTotalPhys / (1024.0 * 1024.0);
        
        m.requests_total = requestCount;
        m.requests_active = activeRequests;
        m.avg_latency_ms = requestCount > 0 ? (totalLatencyUs / requestCount / 1000.0) : 0.0;
        return m;
    }
    
    std::string ExportPrometheus() {
        auto m = Collect();
        std::stringstream ss;
        ss << "# HELP rawrxd_cpu_percent CPU usage\n# TYPE rawrxd_cpu_percent gauge\n";
        ss << "rawrxd_cpu_percent " << m.cpu_percent << "\n";
        ss << "# HELP rawrxd_memory_used_mb Memory used\n# TYPE rawrxd_memory_used_mb gauge\n";
        ss << "rawrxd_memory_used_mb " << m.memory_used_mb << "\n";
        ss << "# HELP rawrxd_requests_total Total requests\n# TYPE rawrxd_requests_total counter\n";
        ss << "rawrxd_requests_total " << m.requests_total << "\n";
        ss << "# HELP rawrxd_requests_active Active requests\n# TYPE rawrxd_requests_active gauge\n";
        ss << "rawrxd_requests_active " << m.requests_active << "\n";
        ss << "# HELP rawrxd_avg_latency_ms Average latency\n# TYPE rawrxd_avg_latency_ms gauge\n";
        ss << "rawrxd_avg_latency_ms " << m.avg_latency_ms << "\n";
        return ss.str();
    }
};

} // namespace Metrics
} // namespace RawrXD
