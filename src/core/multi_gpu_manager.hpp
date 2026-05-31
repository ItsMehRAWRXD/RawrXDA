#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace RawrXD {
namespace Enterprise {

struct LayerAssignment;
struct MultiGPUResult;
enum class DispatchStrategy;
class MultiGPUManager {
public:
    static MultiGPUManager& Instance();
    void UpdateFabricTelemetry(double latencyMs);
};

} // namespace Enterprise
} // namespace RawrXD
