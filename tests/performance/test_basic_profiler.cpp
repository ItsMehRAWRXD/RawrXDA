#include "profiler.h"

#include <iostream>

int main() {
    std::cout << "=== RawrXD Basic Profiler Test ===\n";

    Profiler profiler;
    profiler.startProfiling();

    if (!profiler.isProfiling()) {
        std::cerr << "Profiler failed to enter profiling state\n";
        return 1;
    }

    profiler.recordMemoryAllocation(1024 * 1024);
    profiler.recordBatchCompleted(10, 100);

    const auto snapshot = profiler.getCurrentSnapshot();
    std::cout << "Current metrics - CPU: " << snapshot.cpuUsagePercent
              << "%, Memory: " << snapshot.memoryUsageMB << "MB\n";

    profiler.stopProfiling();

    const auto report = profiler.getProfilingReport();
    std::cout << "Report length: " << report.length() << " characters\n";
    std::cout << "Basic profiler test completed successfully\n";
    return 0;
}
