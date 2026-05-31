#pragma once
#include <cstdint>

namespace RawrXD::Memory {
struct HardwareMetrics {
    uint64_t vramUsage = 0;
    uint64_t vramTotal = 0;
    uint64_t ramUsage = 0;
    uint64_t ramTotal = 0;
};

class IHardwareProvider {
public:
    virtual ~IHardwareProvider() = default;
    virtual HardwareMetrics poll() = 0;
};
}
