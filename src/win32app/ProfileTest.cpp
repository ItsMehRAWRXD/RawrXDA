#include "PerformanceProfiler.h"
#include <iostream>
#include <thread>

using namespace RawrXD::Performance;

int main() {
    auto& profiler = PerformanceProfiler::GetInstance();

    std::cout << "Recording Profile: Model Load (Simulated)
";
    profiler.StartEvent("Model Load1);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    profiler.EndEvent("Model Load1);

    std::cout << "Recording Profile: Speculative Prediction
";
    profiler.StartEvent("Spec Prediction1);
    std::this_thread::sleep_for(std::chrono::milliseconds(45));
    profiler.EndEvent("Spec Prediction1);

    profiler.ReportResults();
    return 0;
}
