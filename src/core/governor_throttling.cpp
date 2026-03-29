#include <windows.h>
#include <pdh.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#pragma comment(lib, "pdh.lib")

namespace RawrXD {

class GovernorThrottling {
public:
    GovernorThrottling();
    ~GovernorThrottling();

    bool initialize();
    void shutdown();

    // Throttling control
    void setCpuThreshold(float threshold); // 0.0-1.0
    void setGpuThreshold(float threshold); // 0.0-1.0
    void setMemoryThreshold(float threshold); // 0.0-1.0

    // Check if operation should be throttled
    bool shouldThrottle() const;

    // Get current system metrics
    float getCpuUsage() const;
    float getGpuUsage() const;
    float getMemoryUsage() const;

    // Throttling actions
    void throttleInference(int delayMs);
    void throttleBackgroundTasks();

private:
    void monitoringThread();

    PDH_HQUERY m_cpuQuery;
    PDH_HCOUNTER m_cpuCounter;
    std::atomic<float> m_cpuUsage;
    std::atomic<float> m_gpuUsage;
    std::atomic<float> m_memoryUsage;

    float m_cpuThreshold;
    float m_gpuThreshold;
    float m_memoryThreshold;

    std::thread m_monitorThread;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;
};

GovernorThrottling::GovernorThrottling()
    : m_cpuUsage(0.0f), m_gpuUsage(0.0f), m_memoryUsage(0.0f),
      m_cpuThreshold(0.8f), m_gpuThreshold(0.8f), m_memoryThreshold(0.9f),
      m_running(false) {
}

GovernorThrottling::~GovernorThrottling() {
    shutdown();
}

bool GovernorThrottling::initialize() {
    // Initialize PDH for CPU monitoring
    if (PdhOpenQueryA(nullptr, 0, &m_cpuQuery) != ERROR_SUCCESS) {
        std::cerr << "Failed to open PDH query" << std::endl;
        return false;
    }

    if (PdhAddCounterA(m_cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter) != ERROR_SUCCESS) {
        std::cerr << "Failed to add CPU counter" << std::endl;
        PdhCloseQuery(m_cpuQuery);
        return false;
    }

    m_running = true;
    m_monitorThread = std::thread([this]() { this->monitoringThread(); });

    std::cout << "Governor Throttling initialized" << std::endl;
    return true;
}

void GovernorThrottling::shutdown() {
    m_running = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
    PdhCloseQuery(m_cpuQuery);
}

void GovernorThrottling::setCpuThreshold(float threshold) {
    m_cpuThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void GovernorThrottling::setGpuThreshold(float threshold) {
    m_gpuThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void GovernorThrottling::setMemoryThreshold(float threshold) {
    m_memoryThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

bool GovernorThrottling::shouldThrottle() const {
    return m_cpuUsage > m_cpuThreshold ||
           m_gpuUsage > m_gpuThreshold ||
           m_memoryUsage > m_memoryThreshold;
}

float GovernorThrottling::getCpuUsage() const {
    return m_cpuUsage;
}

float GovernorThrottling::getGpuUsage() const {
    return m_gpuUsage;
}

float GovernorThrottling::getMemoryUsage() const {
    return m_memoryUsage;
}

void GovernorThrottling::throttleInference(int delayMs) {
    if (shouldThrottle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

void GovernorThrottling::throttleBackgroundTasks() {
    if (shouldThrottle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void GovernorThrottling::monitoringThread() {
    while (m_running) {
        // Update CPU usage
        PdhCollectQueryData(m_cpuQuery);
        PDH_FMT_COUNTERVALUE value;
        if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
            m_cpuUsage = static_cast<float>(value.doubleValue) / 100.0f;
        }

        // Update memory usage
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            m_memoryUsage = 1.0f - static_cast<float>(memInfo.ullAvailPhys) / static_cast<float>(memInfo.ullTotalPhys);
        }

        // Update GPU usage via DXGI adapter query
        {
            float gpuUsage = 0.0f;
            HMODULE hDXGI = LoadLibraryA("dxgi.dll");
            if (hDXGI) {
                using PFN_CreateDXGIFactory1 = HRESULT (WINAPI*)(REFIID, void**);
                auto fnCreate = reinterpret_cast<PFN_CreateDXGIFactory1>(
                    GetProcAddress(hDXGI, "CreateDXGIFactory1"));
                if (fnCreate) {
                    // IID_IDXGIFactory4
                    static const GUID iidFactory = { 0x1bc6ea02, 0xef36, 0x464f,
                        { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a } };
                    void* pFactory = nullptr;
                    if (SUCCEEDED(fnCreate(iidFactory, &pFactory)) && pFactory) {
                        // IDXGIFactory4::EnumAdapters is vtable[7]
                        auto vtbl = *reinterpret_cast<void***>(pFactory);
                        using PFN_EnumAdapters = HRESULT (STDMETHODCALLTYPE*)(void*, UINT, void**);
                        auto fnEnum = reinterpret_cast<PFN_EnumAdapters>(vtbl[7]);
                        void* pAdapter = nullptr;
                        if (SUCCEEDED(fnEnum(pFactory, 0, &pAdapter)) && pAdapter) {
                            // IDXGIAdapter3::QueryVideoMemoryInfo is vtable[14]
                            auto adapterVtbl = *reinterpret_cast<void***>(pAdapter);
                            struct DXGI_QUERY_VIDEO_MEMORY_INFO {
                                UINT64 Budget;
                                UINT64 CurrentUsage;
                                UINT64 AvailableForReservation;
                                UINT64 CurrentReservation;
                            };
                            using PFN_QueryVidMem = HRESULT (STDMETHODCALLTYPE*)(void*, UINT, int, DXGI_QUERY_VIDEO_MEMORY_INFO*);
                            auto fnQuery = reinterpret_cast<PFN_QueryVidMem>(adapterVtbl[14]);
                            DXGI_QUERY_VIDEO_MEMORY_INFO vidMemInfo{};
                            if (SUCCEEDED(fnQuery(pAdapter, 0, 0, &vidMemInfo)) && vidMemInfo.Budget > 0) {
                                gpuUsage = static_cast<float>(vidMemInfo.CurrentUsage) /
                                           static_cast<float>(vidMemInfo.Budget);
                            }
                            // Release adapter
                            using PFN_Release = ULONG (STDMETHODCALLTYPE*)(void*);
                            reinterpret_cast<PFN_Release>(adapterVtbl[2])(pAdapter);
                        }
                        // Release factory
                        using PFN_Release = ULONG (STDMETHODCALLTYPE*)(void*);
                        auto factoryVtbl = *reinterpret_cast<void***>(pFactory);
                        reinterpret_cast<PFN_Release>(factoryVtbl[2])(pFactory);
                    }
                }
                FreeLibrary(hDXGI);
            }
            m_gpuUsage = gpuUsage;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace RawrXD