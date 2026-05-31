#include "PerformanceProfiler.h"
#include <psapi.h>
#include <iostream>
#include <iomanip>

namespace RawrXD::Performance {

PerformanceProfiler& PerformanceProfiler::GetInstance() {
    static PerformanceProfiler instance;
    return instance;
}

size_t PerformanceProfiler::GetCurrentProcessMemory() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.PrivateUsage;
    }
    return 0;
}

void PerformanceProfiler::StartEvent(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProfileEvent ev;
    ev.name = name;
    ev.start = std::chrono::high_resolution_clock::now();
    ev.memoryDeltaBytes = GetCurrentProcessMemory();
    m_activeEvents[name] = ev;
}

void PerformanceProfiler::EndEvent(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activeEvents.find(name);
    if (it != m_activeEvents.end()) {
        it->second.end = std::chrono::high_resolution_clock::now();
        it->second.memoryDeltaBytes = GetCurrentProcessMemory() - it->second.memoryDeltaBytes;
        m_completedEvents.push_back(it->second);
        m_activeEvents.erase(it);
    }
}

void PerformanceProfiler::ReportResults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "\n--- RawrXD Performance Profile Report ---\n";
    std::cout << std::left << std::setw(25) << "Event Name" << std::setw(15) << "Duration (ms)" << "Memory Delta (KB)\n";
    std::cout << std::string(55, '-') << "\n";

    for (const auto& ev : m_completedEvents) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(ev.end - ev.start).count();
        std::cout << std::left << std::setw(25) << ev.name 
                  << std::setw(15) << duration 
                  << (ev.memoryDeltaBytes / 1024) << " KB\n";
    }
}

}
