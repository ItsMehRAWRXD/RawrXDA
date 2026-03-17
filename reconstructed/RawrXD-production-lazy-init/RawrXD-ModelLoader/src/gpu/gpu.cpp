// RawrXD Custom GPU Abstraction Layer Implementation
// This file provides a stub implementation for the agentic GPU interface.
// Copyright (c) 2026 RawrXD Project
#include "gpu.h"
#include <cstring>

namespace rawrxd {
namespace gpu {

namespace {
class CustomDevice : public Device {
public:
    DeviceInfo getInfo() const override {
        static const char* name = "RawrXD Agentic GPU (Stub)";
        return DeviceInfo{name, 1, 256 * 1024 * 1024}; // 1 CU, 256MB VRAM (stub)
    }
    bool isAvailable() const override {
        return true;
    }
};
} // anonymous namespace

Device* createDevice(BackendType type) {
    // Only Custom backend is supported for now
    (void)type;
    return new CustomDevice();
}

} // namespace gpu
} // namespace rawrxd
