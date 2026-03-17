#pragma once

// C++20 / Win32. Hardware backend selector; no Qt. Dialog + callback.

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

class HardwareBackendSelector
{
public:
    enum class Backend {
        CPU,
        CUDA,
        Vulkan,
        ROCm,
        OneAPI,
        Metal
    };

    struct BackendInfo {
        Backend backend = Backend::CPU;
        std::string name;
        std::string version;
        bool available = false;
        std::string deviceName;
        uint64_t vramBytes = 0;
        std::string computeCapability;
        bool supportsFP16 = false;
        bool supportsInt8 = false;
        std::string details;
    };

    using BackendSelectedFn = std::function<void(Backend backend, const BackendInfo&)>;

    HardwareBackendSelector() = default;
    ~HardwareBackendSelector() = default;

    void setOnBackendSelected(BackendSelectedFn f) { m_onBackendSelected = std::move(f); }
    void initialize();
    std::vector<BackendInfo> detectBackends() const;
    void showModal(void* parent = nullptr);
    void* getDialogHandle() const { return m_dialogHandle; }

private:
    void* m_dialogHandle = nullptr;
    BackendSelectedFn m_onBackendSelected;
};
