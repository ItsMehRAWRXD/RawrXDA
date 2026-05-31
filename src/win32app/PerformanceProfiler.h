#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <mutex>

namespace RawrXD::Performance {

struct ProfileEvent {
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;
    size_t memoryDeltaBytes;
};

class PerformanceProfiler {
public:
    static PerformanceProfiler& GetInstance();

    void StartEvent(const std::string& name);
    void EndEvent(const std::string& name);
    
    void ReportResults();

private:
    PerformanceProfiler() = default;
    
    std::map<std::string, ProfileEvent> m_activeEvents;
    std::vector<ProfileEvent> m_completedEvents;
    std::mutex m_mutex;

    size_t GetCurrentProcessMemory();
};

}
