#pragma once
#include <windows.h>
#include <string>
#include <chrono>
#include <atomic>
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
    uint32_t gpu_temp_c;
    uint32_t cpu_temp_c;
    uint64_t requests_total;
    uint64_t requests_active;
    double avg_latency_ms;
};

class MetricsCollector {
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuCounter;
    std::atomic<uint64_t> requestCount{0};
    std::atomic<uint64_t> activeRequests{0};
    std::atomic<uint64_t> totalLatencyUs{0};
    
public:
    bool Initialize() {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
        PdhCollectQueryData(cpuQuery);
        return true;
    }
    
    void RecordRequestStart() { activeRequests++; requestCount++; }
    void RecordRequestEnd(uint64_t latencyUs) { 
        activeRequests--; 
        totalLatencyUs += latencyUs;
    }
    
    SystemMetrics Collect() {
        SystemMetrics m{};
        
        PDH_FMT_COUNTERVALUE cpuVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, nullptr, &cpuVal);
        m.cpu_percent = cpuVal.doubleValue;
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        m.memory_used_mb = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0);
        m.memory_total_mb = memStatus.ullTotalPhys / (1024.0 * 1024.0);
        
        m.requests_total = requestCount.load();
        m.requests_active = activeRequests.load();
        m.avg_latency_ms = requestCount > 0 ? (totalLatencyUs.load() / requestCount.load() / 1000.0) : 0.0;
        
        m.cpu_temp_c = ReadThermalZone("\\\\.\\Thermal-ZONE0");
        m.gpu_temp_c = ReadGPUTemp();
        
        return m;
    }
    
    std::string ExportPrometheus() {
        auto m = Collect();
        std::stringstream ss;
        ss << "# HELP rawrxd_cpu_percent CPU usage percent\n";
        ss << "# TYPE rawrxd_cpu_percent gauge\n";
        ss << "rawrxd_cpu_percent " << m.cpu_percent << "\n";
        ss << "# HELP rawrxd_memory_used_mb Memory used in MB\n";
        ss << "rawrxd_memory_used_mb " << m.memory_used_mb << "\n";
        ss << "# HELP rawrxd_requests_total Total requests\n";
        ss << "rawrxd_requests_total " << m.requests_total << "\n";
        ss << "# HELP rawrxd_requests_active Active requests\n";
        ss << "rawrxd_requests_active " << m.requests_active << "\n";
        ss << "# HELP rawrxd_avg_latency_ms Average latency\n";
        ss << "rawrxd_avg_latency_ms " << m.avg_latency_ms << "\n";
        return ss.str();
    }
    
private:
    uint32_t ReadThermalZone(const char* path) {
        HANDLE hDevice = CreateFileA(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice == INVALID_HANDLE_VALUE) return 0;
        DWORD temp = 0, read = 0;
        DeviceIoControl(hDevice, IOCTL_THERMAL_READ_TEMPERATURE, nullptr, 0, &temp, sizeof(temp), &read, nullptr);
        CloseHandle(hDevice);
        return temp / 10;
    }
    
    uint32_t ReadGPUTemp() {
        return 0;
    }
};

} // namespace Metrics
} // namespace RawrXD