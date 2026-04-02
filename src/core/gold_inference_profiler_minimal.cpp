// Minimal InferenceProfiler definitions for RawrXD_Gold + BackendOrchestrator only.
#include "InferenceProfiler.h"

namespace RawrXD
{

InferenceProfiler& InferenceProfiler::Instance()
{
    static InferenceProfiler inst;
    return inst;
}

void InferenceProfiler::BeginOp(const std::string&, int) {}

void InferenceProfiler::EndOp(const std::string&, int, double, double) {}

std::string InferenceProfiler::GetPrometheusText() const
{
    return "# RawrXD_Gold: inference profiler stub (no samples)\n";
}

}  // namespace RawrXD
