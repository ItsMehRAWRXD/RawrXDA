#pragma once
#include "hardware_provider_interface.h"
#include <windows.h>
#include <pdh.h>

#pragma comment(lib, "pdh.lib")

namespace RawrXD::Memory {
class Win32HardwareProvider : public IHardwareProvider {
public:
    Win32HardwareProvider() {
        if (PdhOpenQueryW(NULL, 0, &m_query) == ERROR_SUCCESS) {
            PdhAddCounterW(m_query, L"\\Memory\\Available MBytes", 0, &m_counter);
        }
    }
    ~Win32HardwareProvider() { if (m_query) PdhCloseQuery(m_query); }
    HardwareMetrics poll() override {
        HardwareMetrics m;
        PDH_FMT_COUNTERVALUE val;
        PdhCollectQueryData(m_query);
        if (PdhGetFormattedCounterValue(m_counter, PDH_FMT_LONG, NULL, &val) == ERROR_SUCCESS) {
            MEMORYSTATUSEX status; status.dwLength = sizeof(status);
            GlobalMemoryStatusEx(&status);
            m.ramUsage = status.ullTotalPhys - (val.longValue * 1024ULL * 1024ULL);
            m.ramTotal = status.ullTotalPhys;
        }
        return m;
    }
private:
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_counter = nullptr;
};
}
