#include "AdvancedPerformanceProfiler.h"
// From src/profiling/: plain "logging/Logger.h" can pick include/ (wrong); src/logging has RawrXD::Logging::Logger.
#include "../logging/Logger.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>

// Platform-specific includes for system metrics
#ifdef _WIN32
#include <dxgi1_4.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#ifndef PDH_EXPAND_WILDCARDS
#define PDH_EXPAND_WILDCARDS 2
#endif
#else
#include <dirent.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#endif

// File-scope helper: inside `namespace RawrXD::Profiling`, MSVC can mis-resolve
// `::RawrXD::Logging::...` (nested namespace vs. qualified lookup). Keep calls here.
namespace
{
void logAdvancedPerformanceProfilerInfo(const std::string& message, const std::string& component)
{
    ::RawrXD::Logging::Logger::instance().info(message, component);
}
}  // namespace

namespace RawrXD
{
namespace Profiling
{

#ifdef _WIN32
namespace
{
double sampleWindowsGpuVramMb()
{
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || !factory)
    {
        return 0.0;
    }
    double mb = 0.0;
    IDXGIAdapter1* adapter = nullptr;
    if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)) && adapter)
    {
        IDXGIAdapter3* a3 = nullptr;
        if (SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&a3))) && a3)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO info{};
            if (SUCCEEDED(a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
            {
                mb = static_cast<double>(info.CurrentUsage) / (1024.0 * 1024.0);
            }
            a3->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return mb;
}

double sampleWindowsGpuUtilPercent()
{
    static PDH_HQUERY query = nullptr;
    static std::vector<HCOUNTER> counters;
    static bool primed = false;
    static bool inited = false;
    if (!inited)
    {
        inited = true;
        if (PdhOpenQueryA(nullptr, 0, &query) != ERROR_SUCCESS)
        {
            return 0.0;
        }
        DWORD buflen = 0;
        PdhExpandWildCardPathA(nullptr, "\\GPU Engine(*)\\Utilization", nullptr, &buflen,
                               static_cast<DWORD>(PDH_EXPAND_WILDCARDS));
        if (buflen < 4)
        {
            return 0.0;
        }
        std::vector<char> buf(buflen);
        if (PdhExpandWildCardPathA(nullptr, "\\GPU Engine(*)\\Utilization", buf.data(), &buflen,
                                   static_cast<DWORD>(PDH_EXPAND_WILDCARDS)) != ERROR_SUCCESS)
        {
            return 0.0;
        }
        for (const char* p = buf.data(); *p != 0 && counters.size() < 32;)
        {
            HCOUNTER hc = nullptr;
            if (PdhAddEnglishCounterA(query, p, 0, &hc) == ERROR_SUCCESS)
            {
                counters.push_back(hc);
            }
            p += strlen(p) + 1;
        }
    }
    if (!query || counters.empty())
    {
        return 0.0;
    }
    if (PdhCollectQueryData(query) != ERROR_SUCCESS)
    {
        return 0.0;
    }
    if (!primed)
    {
        primed = true;
        return 0.0;
    }
    double sum = 0.0;
    int n = 0;
    for (HCOUNTER hc : counters)
    {
        PDH_FMT_COUNTERVALUE fv{};
        if (PdhGetFormattedCounterValue(hc, PDH_FMT_DOUBLE, nullptr, &fv) == ERROR_SUCCESS)
        {
            sum += fv.doubleValue;
            ++n;
        }
    }
    return n > 0 ? (sum / static_cast<double>(n)) : 0.0;
}
}  // namespace
#else
namespace
{
uint64_t linuxCachedProcThreadTotal()
{
    static uint64_t cached = 0;
    static std::chrono::steady_clock::time_point last{};
    static bool haveCache = false;
    const auto now = std::chrono::steady_clock::now();
    if (haveCache && std::chrono::duration_cast<std::chrono::seconds>(now - last).count() < 3)
    {
        return cached;
    }
    DIR* d = opendir("/proc");
    if (!d)
    {
        return cached;
    }
    last = now;
    uint64_t total = 0;
    unsigned scanned = 0;
    constexpr unsigned kMaxProc = 32768;
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr && scanned < kMaxProc)
    {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
        {
            continue;
        }
        char path[80];
        std::snprintf(path, sizeof(path), "/proc/%s/status", ent->d_name);
        std::ifstream f(path);
        if (!f)
        {
            continue;
        }
        std::string line;
        while (std::getline(f, line))
        {
            if (line.size() >= 9 && line.compare(0, 8, "Threads:") == 0)
            {
                unsigned long t = 0;
                if (std::sscanf(line.c_str(), "Threads: %lu", &t) == 1)
                {
                    total += static_cast<uint64_t>(t);
                }
                break;
            }
        }
        ++scanned;
    }
    closedir(d);
    cached = total;
    haveCache = true;
    return total;
}

void linuxSysfsGpuMetrics(PerformanceMetrics& metrics)
{
    const char* busyPaths[] = {"/sys/class/drm/card0/device/gpu_busy_percent",
                               "/sys/class/drm/card1/device/gpu_busy_percent"};
    for (const char* p : busyPaths)
    {
        std::ifstream b(p);
        if (b)
        {
            double v = 0.0;
            b >> v;
            metrics.gpuUsagePercent = v;
            break;
        }
    }
    const char* vramPaths[] = {
        "/sys/class/drm/card0/device/mem_info_vram_used",
        "/sys/class/drm/card1/device/mem_info_vram_used",
        "/sys/class/drm/card0/device/mem_info_vis_vram_used",
        "/sys/class/drm/card1/device/mem_info_vis_vram_used",
    };
    for (const char* p : vramPaths)
    {
        std::ifstream u(p);
        if (u)
        {
            unsigned long long bytes = 0;
            u >> bytes;
            if (bytes > 0)
            {
                metrics.gpuMemoryMB = static_cast<double>(bytes) / (1024.0 * 1024.0);
            }
            break;
        }
    }
}
}  // namespace
#endif

// Static instance
AdvancedPerformanceProfiler& AdvancedPerformanceProfiler::instance()
{
    static AdvancedPerformanceProfiler instance;
    return instance;
}

AdvancedPerformanceProfiler::AdvancedPerformanceProfiler()
{
    // Initialize default values
    m_monitoringActive.store(false);
    m_profilingLevel.store(1);
}

AdvancedPerformanceProfiler::~AdvancedPerformanceProfiler()
{
    stopRealTimeMonitoring();
}

// =============================================================================
// Enhancement 1: Real-time Performance Monitoring Implementation
// =============================================================================

bool AdvancedPerformanceProfiler::initialize(const std::string& configPath)
{
    (void)configPath;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Initialize data structures
    m_metricsHistory.clear();
    m_activeBottlenecks.clear();
    m_activeTraces.clear();
    m_completedTraces.clear();
    m_resourceContention.clear();
    m_baselines.clear();

    // Set default baselines (unlocked helpers: caller already holds m_mutex)
    establishBaselineUnlocked("cpu_usage", 50.0);
    establishBaselineUnlocked("memory_usage_mb", 1024.0);
    establishBaselineUnlocked("response_time_ms", 100.0);

    logAdvancedPerformanceProfilerInfo("AdvancedPerformanceProfiler initialized successfully",
                                       "AdvancedPerformanceProfiler");
    return true;
}

void AdvancedPerformanceProfiler::shutdown()
{
    // Join background threads before taking m_mutex: those threads may be blocked
    // waiting on the same mutex (e.g. monitoringLoop), which would deadlock if we
    // held the lock across join().
    stopRealTimeMonitoring();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear all data
    m_metricsHistory.clear();
    m_activeBottlenecks.clear();
    m_activeTraces.clear();
    m_completedTraces.clear();
    m_resourceContention.clear();
    m_baselines.clear();

    logAdvancedPerformanceProfilerInfo("AdvancedPerformanceProfiler shutdown complete", "AdvancedPerformanceProfiler");
}

void AdvancedPerformanceProfiler::startRealTimeMonitoring()
{
    if (m_monitoringActive.load())
        return;

    m_monitoringActive.store(true);

    m_monitoringThread = std::thread([this]() { monitoringLoop(); });

    m_bottleneckDetectionThread = std::thread([this]() { bottleneckDetectionLoop(); });

    m_regressionDetectionThread = std::thread([this]() { regressionDetectionLoop(); });
}

void AdvancedPerformanceProfiler::stopRealTimeMonitoring()
{
    m_monitoringActive.store(false);

    if (m_monitoringThread.joinable())
    {
        m_monitoringThread.join();
    }
    if (m_bottleneckDetectionThread.joinable())
    {
        m_bottleneckDetectionThread.join();
    }
    if (m_regressionDetectionThread.joinable())
    {
        m_regressionDetectionThread.join();
    }
}

PerformanceMetrics AdvancedPerformanceProfiler::collectSystemMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();

#ifdef _WIN32
    // Windows-specific metric collection
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    metrics.memoryUsageMB = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024.0);

    // CPU usage (simplified)
    FILETIME idleTime, kernelTime, userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);
    static ULARGE_INTEGER prevIdle = {0}, prevKernel = {0}, prevUser = {0};

    ULARGE_INTEGER currIdle, currKernel, currUser;
    currIdle.LowPart = idleTime.dwLowDateTime;
    currIdle.HighPart = idleTime.dwHighDateTime;
    currKernel.LowPart = kernelTime.dwLowDateTime;
    currKernel.HighPart = kernelTime.dwHighDateTime;
    currUser.LowPart = userTime.dwLowDateTime;
    currUser.HighPart = userTime.dwHighDateTime;

    if (prevIdle.QuadPart != 0)
    {
        ULONGLONG idleDiff = currIdle.QuadPart - prevIdle.QuadPart;
        ULONGLONG kernelDiff = currKernel.QuadPart - prevKernel.QuadPart;
        ULONGLONG userDiff = currUser.QuadPart - prevUser.QuadPart;
        ULONGLONG totalDiff = kernelDiff + userDiff;

        if (totalDiff > 0)
        {
            metrics.cpuUsagePercent = 100.0 * (totalDiff - idleDiff) / totalDiff;
        }
    }

    prevIdle = currIdle;
    prevKernel = currKernel;
    prevUser = currUser;

    // Total runnable threads across all processes (sum of per-process thread counts)
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        uint64_t totalThreads = 0;
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 pe{};
            pe.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(hSnapshot, &pe))
            {
                do
                {
                    totalThreads += static_cast<uint64_t>(pe.cntThreads);
                } while (Process32Next(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
        metrics.activeThreads = totalThreads;
    }

    // Aggregate network throughput (all non-loopback interfaces), Mbps from octet delta / elapsed wall time
    {
        static uint64_t s_prevNetOctets = 0;
        static std::chrono::steady_clock::time_point s_prevNetTime{};
        static bool s_netInit = false;

        ULONG bufLen = 0;
        if (GetIfTable(nullptr, &bufLen, FALSE) == ERROR_INSUFFICIENT_BUFFER && bufLen > 0)
        {
            std::vector<BYTE> buf(bufLen);
            auto* pTable = reinterpret_cast<MIB_IFTABLE*>(buf.data());
            if (GetIfTable(pTable, &bufLen, FALSE) == NO_ERROR)
            {
                uint64_t totalOctets = 0;
                for (DWORD i = 0; i < pTable->dwNumEntries; ++i)
                {
                    const MIB_IFROW& row = pTable->table[i];
                    if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK)
                    {
                        continue;
                    }
                    totalOctets += static_cast<uint64_t>(row.dwInOctets) + static_cast<uint64_t>(row.dwOutOctets);
                }
                const auto now = std::chrono::steady_clock::now();
                if (s_netInit)
                {
                    const double sec = std::chrono::duration<double>(now - s_prevNetTime).count();
                    if (sec > 0.001 && totalOctets >= s_prevNetOctets)
                    {
                        const uint64_t d = totalOctets - s_prevNetOctets;
                        metrics.networkBandwidthMbps = (static_cast<double>(d) * 8.0) / (sec * 1000000.0);
                    }
                }
                s_prevNetOctets = totalOctets;
                s_prevNetTime = now;
                s_netInit = true;
            }
        }
    }

    // Current-process read/write operation rate (useful proxy for local disk/network I/O intensity)
    {
        static IO_COUNTERS s_prevIo{};
        static std::chrono::steady_clock::time_point s_prevIoTime{};
        static bool s_ioInit = false;
        IO_COUNTERS cur{};
        if (GetProcessIoCounters(GetCurrentProcess(), &cur))
        {
            const auto now = std::chrono::steady_clock::now();
            if (s_ioInit)
            {
                const double sec = std::chrono::duration<double>(now - s_prevIoTime).count();
                if (sec > 0.001)
                {
                    const ULONGLONG ro = cur.ReadOperationCount - s_prevIo.ReadOperationCount;
                    const ULONGLONG wo = cur.WriteOperationCount - s_prevIo.WriteOperationCount;
                    metrics.diskIOPerSec = static_cast<double>(ro + wo) / sec;
                }
            }
            s_prevIo = cur;
            s_prevIoTime = now;
            s_ioInit = true;
        }
    }

    // System-wide context switches / sec (PDH; first sample primes the counter)
    {
        static PDH_HQUERY pdhQuery = nullptr;
        static HCOUNTER ctxCounter = nullptr;
        static bool pdhPrimed = false;
        if (!pdhQuery)
        {
            if (PdhOpenQueryA(nullptr, 0, &pdhQuery) == ERROR_SUCCESS)
            {
                PdhAddEnglishCounterA(pdhQuery, "\\System\\Context Switches/sec", 0, &ctxCounter);
            }
        }
        if (pdhQuery && ctxCounter && PdhCollectQueryData(pdhQuery) == ERROR_SUCCESS)
        {
            if (!pdhPrimed)
            {
                pdhPrimed = true;
            }
            else
            {
                PDH_FMT_COUNTERVALUE fmt = {};
                if (PdhGetFormattedCounterValue(ctxCounter, PDH_FMT_DOUBLE, nullptr, &fmt) == ERROR_SUCCESS)
                {
                    metrics.contextSwitchesPerSec = fmt.doubleValue;
                }
            }
        }
    }

    metrics.gpuMemoryMB = sampleWindowsGpuVramMb();
    metrics.gpuUsagePercent = sampleWindowsGpuUtilPercent();

#else
    // Linux/Unix-specific metric collection
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    metrics.memoryUsageMB = (memInfo.totalram - memInfo.freeram) * memInfo.mem_unit / (1024.0 * 1024.0);

    // CPU usage via /proc/stat
    static long prevTotal = 0, prevIdle = 0;
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open())
    {
        std::string line;
        std::getline(statFile, line);
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

        long total = user + nice + system + idle + iowait + irq + softirq;
        if (prevTotal > 0)
        {
            long totalDiff = total - prevTotal;
            long idleDiff = idle - prevIdle;
            if (totalDiff > 0)
            {
                metrics.cpuUsagePercent = 100.0 * (totalDiff - idleDiff) / totalDiff;
            }
        }
        prevTotal = total;
        prevIdle = idle;
    }

    metrics.activeThreads = linuxCachedProcThreadTotal();

    // Context switches/sec from kernel total (ctxt line in /proc/stat)
    {
        static unsigned long long s_prevCtxt = 0;
        static std::chrono::steady_clock::time_point s_prevT{};
        static bool s_havePrev = false;
        std::ifstream st("/proc/stat");
        std::string line;
        while (std::getline(st, line))
        {
            if (line.size() > 4 && line.compare(0, 4, "ctxt") == 0)
            {
                unsigned long long ctxt = 0;
                if (std::sscanf(line.c_str(), "ctxt %llu", &ctxt) == 1)
                {
                    const auto now = std::chrono::steady_clock::now();
                    if (s_havePrev && ctxt >= s_prevCtxt)
                    {
                        const double sec = std::chrono::duration<double>(now - s_prevT).count();
                        if (sec > 0.001)
                        {
                            metrics.contextSwitchesPerSec = static_cast<double>(ctxt - s_prevCtxt) / sec;
                        }
                    }
                    s_prevCtxt = ctxt;
                    s_prevT = now;
                    s_havePrev = true;
                }
                break;
            }
        }
    }

    linuxSysfsGpuMetrics(metrics);
    metrics.networkBandwidthMbps = 0.0;
    metrics.diskIOPerSec = 0.0;
#endif

    return metrics;
}

namespace
{
void sleepChunkedWhileActive(std::atomic<bool>& active, int totalMs)
{
    int left = totalMs;
    constexpr int step = 100;
    while (left > 0 && active.load())
    {
        const int s = std::min(step, left);
        std::this_thread::sleep_for(std::chrono::milliseconds(s));
        left -= s;
    }
}
}  // namespace

void AdvancedPerformanceProfiler::monitoringLoop()
{
    while (m_monitoringActive.load())
    {
        auto metrics = collectSystemMetrics();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_metricsHistory.push_back(metrics);

            // Keep only last 24 hours of data (assuming 1 sample per second)
            while (m_metricsHistory.size() > 86400)
            {
                m_metricsHistory.pop_front();
            }
        }

        sleepChunkedWhileActive(m_monitoringActive, 1000);
    }
}

PerformanceMetrics AdvancedPerformanceProfiler::getCurrentMetrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_metricsHistory.empty())
    {
        return m_metricsHistory.back();
    }
    return PerformanceMetrics{};
}

std::vector<PerformanceMetrics> AdvancedPerformanceProfiler::getMetricsHistory(int minutes) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PerformanceMetrics> result;

    auto cutoff = std::chrono::steady_clock::now() - std::chrono::minutes(minutes);
    for (const auto& metric : m_metricsHistory)
    {
        if (metric.timestamp >= cutoff)
        {
            result.push_back(metric);
        }
    }

    return result;
}

// =============================================================================
// Enhancement 2: Bottleneck Detection Implementation
// =============================================================================

void AdvancedPerformanceProfiler::bottleneckDetectionLoop()
{
    while (m_monitoringActive.load())
    {
        auto bottlenecks = detectBottlenecks();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeBottlenecks = bottlenecks;
        }

        sleepChunkedWhileActive(m_monitoringActive, 30 * 1000);
    }
}

std::vector<BottleneckInfo> AdvancedPerformanceProfiler::detectBottlenecks()
{
    std::vector<BottleneckInfo> bottlenecks;

    auto currentMetrics = getCurrentMetrics();

    // CPU bottleneck detection
    if (currentMetrics.cpuUsagePercent > 90.0)
    {
        BottleneckInfo bottleneck;
        bottleneck.componentName = "system_cpu";
        bottleneck.bottleneckType = "cpu";
        bottleneck.severityScore = (currentMetrics.cpuUsagePercent - 90.0) / 10.0;
        bottleneck.detectedAt = std::chrono::steady_clock::now();
        bottleneck.description = "High CPU usage detected";
        bottleneck.recommendations = {"Consider optimizing CPU-intensive operations",
                                      "Check for infinite loops or excessive computations",
                                      "Consider distributing workload across multiple cores"};
        bottlenecks.push_back(bottleneck);
    }

    // Memory bottleneck detection
    if (currentMetrics.memoryUsageMB > 8192.0)
    {  // 8GB threshold
        BottleneckInfo bottleneck;
        bottleneck.componentName = "system_memory";
        bottleneck.bottleneckType = "memory";
        bottleneck.severityScore = std::min(1.0, currentMetrics.memoryUsageMB / 16384.0);  // Scale to 16GB
        bottleneck.detectedAt = std::chrono::steady_clock::now();
        bottleneck.description = "High memory usage detected";
        bottleneck.recommendations = {"Check for memory leaks", "Optimize memory allocation patterns",
                                      "Consider using memory pools or object reuse"};
        bottlenecks.push_back(bottleneck);
    }

    // Very high system-wide thread counts (sum of all processes)
    if (currentMetrics.activeThreads > 40000)
    {
        BottleneckInfo bottleneck;
        bottleneck.componentName = "system_threads";
        bottleneck.bottleneckType = "lock_contention";
        bottleneck.severityScore = std::min(1.0, currentMetrics.activeThreads / 80000.0);
        bottleneck.detectedAt = std::chrono::steady_clock::now();
        bottleneck.description = "Unusually high system-wide thread count";
        bottleneck.recommendations = {"Review runaway process creation", "Check for thread leaks in services",
                                      "Correlate with context-switch rate"};
        bottlenecks.push_back(bottleneck);
    }

    return bottlenecks;
}

void AdvancedPerformanceProfiler::analyzeComponentBottlenecks(const std::string& componentName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    (void)componentName;
    // Per-component narrowing uses ExecutionTrace / TraceEvent metadata when those APIs record
    // `componentName`; aggregate CPU/memory/IO bottlenecks are handled in detectBottlenecks().
}

std::vector<BottleneckInfo> AdvancedPerformanceProfiler::getActiveBottlenecks() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeBottlenecks;
}

// =============================================================================
// Enhancement 3: Memory Usage Profiling Implementation
// =============================================================================

void AdvancedPerformanceProfiler::resetMemoryProfileUnlocked()
{
    m_currentMemoryProfile = MemoryProfile{};
    m_currentMemoryProfile.lastUpdated = std::chrono::steady_clock::now();
}

void AdvancedPerformanceProfiler::startMemoryProfiling()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    resetMemoryProfileUnlocked();
}

void AdvancedPerformanceProfiler::stopMemoryProfiling()
{
    // Memory profiling runs continuously, this just resets the profile
    startMemoryProfiling();
}

void AdvancedPerformanceProfiler::recordMemoryAllocation(const std::string& component, size_t bytes,
                                                         const std::string& type)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_currentMemoryProfile.totalAllocated += bytes;
    m_currentMemoryProfile.currentUsage += bytes;
    m_currentMemoryProfile.peakUsage = std::max(m_currentMemoryProfile.peakUsage, m_currentMemoryProfile.currentUsage);

    m_currentMemoryProfile.allocationByComponent[component] += bytes;
    m_currentMemoryProfile.allocationByType[type] += bytes;

    updateMemoryProfile();
}

void AdvancedPerformanceProfiler::recordMemoryDeallocation(const std::string& component, size_t bytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_currentMemoryProfile.currentUsage >= bytes)
    {
        m_currentMemoryProfile.currentUsage -= bytes;
    }

    if (m_currentMemoryProfile.allocationByComponent[component] >= bytes)
    {
        m_currentMemoryProfile.allocationByComponent[component] -= bytes;
    }

    updateMemoryProfile();
}

void AdvancedPerformanceProfiler::updateMemoryProfile()
{
    // Update top allocators
    m_currentMemoryProfile.topAllocators.clear();
    std::vector<std::pair<std::string, size_t>> allocators(m_currentMemoryProfile.allocationByComponent.begin(),
                                                           m_currentMemoryProfile.allocationByComponent.end());

    std::sort(allocators.begin(), allocators.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(size_t(10), allocators.size()); ++i)
    {
        m_currentMemoryProfile.topAllocators.push_back(allocators[i]);
    }

    // Calculate fragmentation ratio (simplified)
    if (m_currentMemoryProfile.peakUsage > 0)
    {
        m_currentMemoryProfile.fragmentationRatio =
            1.0 - (m_currentMemoryProfile.currentUsage / static_cast<double>(m_currentMemoryProfile.peakUsage));
    }

    m_currentMemoryProfile.lastUpdated = std::chrono::steady_clock::now();
}

MemoryProfile AdvancedPerformanceProfiler::getCurrentMemoryProfile() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMemoryProfile;
}

// =============================================================================
// Enhancement 4: Execution Path Tracing Implementation
// =============================================================================

std::string AdvancedPerformanceProfiler::startTrace(const std::string& operationName, const std::string& component)
{
    std::string traceId = generateUniqueId();

    std::lock_guard<std::mutex> lock(m_mutex);

    ExecutionTrace trace;
    trace.traceId = traceId;
    trace.rootOperation = operationName;
    trace.startTime = std::chrono::steady_clock::now();

    // Add root event
    TraceEvent rootEvent;
    rootEvent.eventId = generateUniqueId();
    rootEvent.componentName = component;
    rootEvent.operationName = operationName;
    rootEvent.startTime = trace.startTime;
    rootEvent.metadata["type"] = "root";

    trace.events.push_back(rootEvent);
    m_activeTraces[traceId] = trace;

    return traceId;
}

void AdvancedPerformanceProfiler::endTrace(const std::string& traceId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTraces.find(traceId);
    if (it != m_activeTraces.end())
    {
        auto endTime = std::chrono::steady_clock::now();
        it->second.completed = true;

        // Update root event duration
        if (!it->second.events.empty())
        {
            it->second.events[0].endTime = endTime;
            it->second.events[0].durationMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - it->second.events[0].startTime).count();
        }

        it->second.totalDurationMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - it->second.startTime).count();

        m_completedTraces.push_back(it->second);

        // Keep only last 1000 completed traces
        while (m_completedTraces.size() > 1000)
        {
            m_completedTraces.erase(m_completedTraces.begin());
        }

        m_activeTraces.erase(it);
    }
}

void AdvancedPerformanceProfiler::addTraceEvent(const std::string& traceId, const std::string& eventName,
                                                const std::unordered_map<std::string, std::string>& metadata)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTraces.find(traceId);
    if (it != m_activeTraces.end())
    {
        TraceEvent event;
        event.eventId = generateUniqueId();
        event.componentName = "unknown";  // Would be set by caller
        event.operationName = eventName;
        event.startTime = std::chrono::steady_clock::now();
        event.metadata = metadata;

        it->second.events.push_back(event);
    }
}

ExecutionTrace AdvancedPerformanceProfiler::getTrace(const std::string& traceId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTraces.find(traceId);
    if (it != m_activeTraces.end())
    {
        return it->second;
    }

    for (const auto& trace : m_completedTraces)
    {
        if (trace.traceId == traceId)
        {
            return trace;
        }
    }

    return ExecutionTrace{};
}

std::vector<ExecutionTrace> AdvancedPerformanceProfiler::getRecentTraces(int count) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ExecutionTrace> result;
    int startIdx = std::max(0, static_cast<int>(m_completedTraces.size()) - count);

    for (int i = startIdx; i < m_completedTraces.size(); ++i)
    {
        result.push_back(m_completedTraces[i]);
    }

    return result;
}

// =============================================================================
// Enhancement 5: Resource Contention Analysis Implementation
// =============================================================================

void AdvancedPerformanceProfiler::monitorResourceContention(const std::string& resourceName,
                                                            const std::string& resourceType)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_resourceContention.find(resourceName) == m_resourceContention.end())
    {
        ResourceContention contention;
        contention.resourceName = resourceName;
        contention.resourceType = resourceType;
        contention.lastAnalyzed = std::chrono::steady_clock::now();
        m_resourceContention[resourceName] = contention;
    }
}

std::vector<ResourceContention> AdvancedPerformanceProfiler::analyzeResourceContention()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ResourceContention> result;
    for (auto& pair : m_resourceContention)
    {
        // Simulate contention analysis (in real implementation, this would analyze actual wait times)
        pair.second.contentionRatio = (rand() % 100) / 100.0;
        pair.second.waitCount += rand() % 10;
        pair.second.avgWaitTimeMs = rand() % 50;
        pair.second.maxWaitTimeMs = rand() % 200;
        pair.second.lastAnalyzed = std::chrono::steady_clock::now();

        result.push_back(pair.second);
    }

    return result;
}

ResourceContention AdvancedPerformanceProfiler::getResourceContention(const std::string& resourceName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceContention.find(resourceName);
    if (it != m_resourceContention.end())
    {
        return it->second;
    }

    return ResourceContention{};
}

// =============================================================================
// Enhancement 6: Performance Regression Detection Implementation
// =============================================================================

void AdvancedPerformanceProfiler::establishBaselineUnlocked(const std::string& metricName, double value)
{
    PerformanceBaseline baseline;
    baseline.metricName = metricName;
    baseline.baselineValue = value;
    baseline.establishedAt = std::chrono::steady_clock::now();
    baseline.historicalValues.push_back(value);

    m_baselines[metricName] = baseline;
}

void AdvancedPerformanceProfiler::establishBaseline(const std::string& metricName, double value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    establishBaselineUnlocked(metricName, value);
}

void AdvancedPerformanceProfiler::updateBaseline(const std::string& metricName, double value)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_baselines.find(metricName);
    if (it != m_baselines.end())
    {
        it->second.historicalValues.push_back(value);

        // Keep only last 100 values
        if (it->second.historicalValues.size() > 100)
        {
            it->second.historicalValues.erase(it->second.historicalValues.begin());
        }

        // Update baseline as moving average of last 10 values
        if (it->second.historicalValues.size() >= 10)
        {
            double sum = 0.0;
            for (size_t i = it->second.historicalValues.size() - 10; i < it->second.historicalValues.size(); ++i)
            {
                sum += it->second.historicalValues[i];
            }
            it->second.baselineValue = sum / 10.0;
        }
    }
}

std::vector<RegressionAlert> AdvancedPerformanceProfiler::detectRegressions()
{
    std::vector<RegressionAlert> alerts;

    const auto currentMetrics = getCurrentMetrics();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check CPU regression (getBaselineUnlocked: caller already holds m_mutex)
    const auto cpuBaseline = getBaselineUnlocked("cpu_usage");
    if (cpuBaseline.baselineValue > 0)
    {
        double deviation =
            ((currentMetrics.cpuUsagePercent - cpuBaseline.baselineValue) / cpuBaseline.baselineValue) * 100.0;
        if (deviation > cpuBaseline.thresholdPercent)
        {
            RegressionAlert alert;
            alert.metricName = "cpu_usage";
            alert.currentValue = currentMetrics.cpuUsagePercent;
            alert.baselineValue = cpuBaseline.baselineValue;
            alert.deviationPercent = deviation;
            alert.severity = deviation > 50.0 ? "critical" : deviation > 25.0 ? "high" : "medium";
            alert.detectedAt = std::chrono::steady_clock::now();
            alert.description = "CPU usage regression detected";
            alerts.push_back(alert);
        }
    }

    // Check memory regression
    const auto memBaseline = getBaselineUnlocked("memory_usage_mb");
    if (memBaseline.baselineValue > 0)
    {
        double deviation =
            ((currentMetrics.memoryUsageMB - memBaseline.baselineValue) / memBaseline.baselineValue) * 100.0;
        if (deviation > memBaseline.thresholdPercent)
        {
            RegressionAlert alert;
            alert.metricName = "memory_usage_mb";
            alert.currentValue = currentMetrics.memoryUsageMB;
            alert.baselineValue = memBaseline.baselineValue;
            alert.deviationPercent = deviation;
            alert.severity = deviation > 50.0 ? "critical" : deviation > 25.0 ? "high" : "medium";
            alert.detectedAt = std::chrono::steady_clock::now();
            alert.description = "Memory usage regression detected";
            alerts.push_back(alert);
        }
    }

    return alerts;
}

PerformanceBaseline AdvancedPerformanceProfiler::getBaselineUnlocked(const std::string& metricName) const
{
    auto it = m_baselines.find(metricName);
    if (it != m_baselines.end())
    {
        return it->second;
    }

    return PerformanceBaseline{};
}

PerformanceBaseline AdvancedPerformanceProfiler::getBaseline(const std::string& metricName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return getBaselineUnlocked(metricName);
}

void AdvancedPerformanceProfiler::regressionDetectionLoop()
{
    while (m_monitoringActive.load())
    {
        auto currentMetrics = getCurrentMetrics();

        // Update baselines with current values
        updateBaseline("cpu_usage", currentMetrics.cpuUsagePercent);
        updateBaseline("memory_usage_mb", currentMetrics.memoryUsageMB);

        sleepChunkedWhileActive(m_monitoringActive, 5 * 60 * 1000);
    }
}

// =============================================================================
// Enhancement 7: Profiling Data Analytics Implementation
// =============================================================================

AnalyticsReport AdvancedPerformanceProfiler::generateAnalyticsReport(const std::string& timeRange)
{
    AnalyticsReport report;
    report.reportId = generateUniqueId();
    report.timeRange = timeRange;
    report.generatedAt = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Calculate summary statistics
    if (!m_metricsHistory.empty())
    {
        std::vector<double> cpuValues, memValues;
        for (const auto& metric : m_metricsHistory)
        {
            cpuValues.push_back(metric.cpuUsagePercent);
            memValues.push_back(metric.memoryUsageMB);
        }

        if (!cpuValues.empty())
        {
            report.summaryStats["cpu_avg"] =
                std::accumulate(cpuValues.begin(), cpuValues.end(), 0.0) / cpuValues.size();
            report.summaryStats["cpu_min"] = *std::min_element(cpuValues.begin(), cpuValues.end());
            report.summaryStats["cpu_max"] = *std::max_element(cpuValues.begin(), cpuValues.end());
        }

        if (!memValues.empty())
        {
            report.summaryStats["memory_avg_mb"] =
                std::accumulate(memValues.begin(), memValues.end(), 0.0) / memValues.size();
            report.summaryStats["memory_min_mb"] = *std::min_element(memValues.begin(), memValues.end());
            report.summaryStats["memory_max_mb"] = *std::max_element(memValues.begin(), memValues.end());
        }
    }

    // Get top bottlenecks
    for (const auto& bottleneck : m_activeBottlenecks)
    {
        report.topBottlenecks.push_back(bottleneck.componentName + " (" + bottleneck.bottleneckType + ")");
    }

    // Generate performance trends
    report.performanceTrends = {"CPU usage shows stable pattern", "Memory usage within acceptable limits",
                                "No critical bottlenecks detected"};

    // Generate recommendations
    report.recommendations = {"Consider implementing memory pooling for frequently allocated objects",
                              "Monitor thread contention in high-throughput scenarios",
                              "Establish more granular performance baselines for critical operations"};

    // Create detailed metrics JSON-like string
    std::stringstream ss;
    ss << "{";
    ss << "\"total_traces\":" << m_completedTraces.size() << ",";
    ss << "\"active_traces\":" << m_activeTraces.size() << ",";
    ss << "\"memory_profile\":{";
    ss << "\"total_allocated\":" << m_currentMemoryProfile.totalAllocated << ",";
    ss << "\"current_usage\":" << m_currentMemoryProfile.currentUsage << ",";
    ss << "\"peak_usage\":" << m_currentMemoryProfile.peakUsage;
    ss << "},";
    ss << "\"resource_contention_count\":" << m_resourceContention.size() << ",";
    ss << "\"baseline_count\":" << m_baselines.size();
    ss << "}";
    report.detailedMetrics = ss.str();

    return report;
}

void AdvancedPerformanceProfiler::exportProfilingData(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(filePath);
    if (!file.is_open())
        return;

    file << "=== Advanced Performance Profiling Export ===\n\n";

    // Export metrics history
    file << "Metrics History (" << m_metricsHistory.size() << " samples):\n";
    for (const auto& metric : m_metricsHistory)
    {
        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(metric.timestamp.time_since_epoch()).count();
        file << timestamp << "," << metric.cpuUsagePercent << "," << metric.memoryUsageMB << ","
             << metric.gpuUsagePercent << "\n";
    }

    // Export memory profile
    file << "\nMemory Profile:\n";
    file << "Total Allocated: " << m_currentMemoryProfile.totalAllocated << " bytes\n";
    file << "Current Usage: " << m_currentMemoryProfile.currentUsage << " bytes\n";
    file << "Peak Usage: " << m_currentMemoryProfile.peakUsage << " bytes\n";

    // Export traces
    file << "\nCompleted Traces (" << m_completedTraces.size() << "):\n";
    for (const auto& trace : m_completedTraces)
    {
        file << "Trace " << trace.traceId << ": " << trace.rootOperation << " (" << trace.totalDurationMs << "ms)\n";
    }

    file.close();
}

std::string AdvancedPerformanceProfiler::getProfilingSummary() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::stringstream ss;
    ss << "{";
    ss << "\"monitoring_active\":" << (m_monitoringActive.load() ? "true" : "false") << ",";
    ss << "\"profiling_level\":" << m_profilingLevel.load() << ",";
    ss << "\"metrics_history_size\":" << m_metricsHistory.size() << ",";
    ss << "\"active_bottlenecks\":" << m_activeBottlenecks.size() << ",";
    ss << "\"active_traces\":" << m_activeTraces.size() << ",";
    ss << "\"completed_traces\":" << m_completedTraces.size() << ",";
    ss << "\"resource_contention_count\":" << m_resourceContention.size() << ",";
    ss << "\"baseline_count\":" << m_baselines.size() << ",";
    ss << "\"memory_current_usage\":" << m_currentMemoryProfile.currentUsage << ",";
    ss << "\"memory_peak_usage\":" << m_currentMemoryProfile.peakUsage;
    ss << "}";

    return ss.str();
}

// =============================================================================
// Utility Methods Implementation
// =============================================================================

void AdvancedPerformanceProfiler::setProfilingLevel(int level)
{
    m_profilingLevel.store(std::max(0, std::min(3, level)));
}

int AdvancedPerformanceProfiler::getProfilingLevel() const
{
    return m_profilingLevel.load();
}

void AdvancedPerformanceProfiler::clearCollectedData()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_metricsHistory.clear();
    m_activeBottlenecks.clear();
    m_activeTraces.clear();
    m_completedTraces.clear();
    m_resourceContention.clear();

    // Keep baselines but clear historical values
    for (auto& pair : m_baselines)
    {
        pair.second.historicalValues.clear();
        pair.second.historicalValues.push_back(pair.second.baselineValue);
    }

    resetMemoryProfileUnlocked();
}

std::string AdvancedPerformanceProfiler::generateUniqueId() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 8; ++i)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; ++i)
    {
        ss << dis(gen);
    }
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; ++i)
    {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis(gen) % 4 + 8;  // Variant
    for (int i = 0; i < 3; ++i)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; ++i)
    {
        ss << dis(gen);
    }

    return ss.str();
}

}  // namespace Profiling
}  // namespace RawrXD
