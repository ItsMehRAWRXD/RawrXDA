#include "hardware_provider_interface.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

#pragma comment(lib, "pdh.lib")

namespace RawrXD::Memory {

class Win32HardwareProvider : public IHardwareProvider {
public:
    Win32HardwareProvider() : m_query(nullptr), m_cpuCounter(nullptr) {}

    ~Win32HardwareProvider() {
        if (m_query) PdhCloseQuery(m_query);
    }

    bool initialize() override {
        if (PdhOpenQuery(NULL, NULL, &m_query) != ERROR_SUCCESS) return false;

        // Total CPU Load
        if (PdhAddCounter(m_query, L"\\Processor(_Total)\\% Processor Time", NULL, &m_cpuCounter) != ERROR_SUCCESS) {
            return false;
        }

        PdhCollectQueryData(m_query);
        return true;
    }

    HardwareMetrics sample() override {
        HardwareMetrics m{};

        // CPU Load
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(m_query);
        if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS) {
            m.cpuLoadPercent = static_cast<float>(counterVal.doubleValue);
        }

        // System RAM (GlobalMemoryStatusEx)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            m.ramTotalBytes = memInfo.ullTotalPhys;
            m.ramUsedBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        }

        return m;
    }

    std::string getProviderName() const override { return "Win32/PDH"; }

private:
    PDH_HQUERY   m_query;
    PDH_HCOUNTER m_cpuCounter;
};

} // namespace RawrXD::Memory
