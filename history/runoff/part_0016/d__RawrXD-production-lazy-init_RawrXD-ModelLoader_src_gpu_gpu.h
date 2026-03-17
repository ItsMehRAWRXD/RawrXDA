// RawrXD Custom GPU Abstraction Layer
// This header defines the agentic GPU interface, replacing all Vulkan/ROCm dependencies.
// Copyright (c) 2026 RawrXD Project
#pragma once

namespace rawrxd {
namespace gpu {

// Enum for supported GPU backends (future-proof, but only 'Custom' is implemented)
enum class BackendType {
    Custom = 0
};

// Basic device info struct
struct DeviceInfo {
    const char* name;
    int computeUnits;
    size_t vramBytes;
};

// Abstract GPU device interface
class Device {
public:
    virtual ~Device() = default;
    virtual DeviceInfo getInfo() const = 0;
    virtual bool isAvailable() const = 0;
    // Add more agentic compute/graphics methods as needed
};

// Factory for creating devices
Device* createDevice(BackendType type = BackendType::Custom);

} // namespace gpu
} // namespace rawrxd
