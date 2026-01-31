// GPU Backend Quick Selector - Compact widget for toolbar/status bar
// Production-ready with live backend switching
#pragma once


namespace RawrXD {

enum class ComputeBackend {
    CPU,
    CUDA,
    Vulkan,
    Metal,       // For future macOS support
    DirectML,    // For Windows ML acceleration
    Auto         // Automatic selection
};

struct BackendInfo {
    ComputeBackend backend;
    std::string displayName;
    std::string icon;
    bool available{false};
    std::string deviceName;
    int vramMB{0};
};

class GPUBackendSelector : public void {

public:
    explicit GPUBackendSelector(void* parent = nullptr);
    ~GPUBackendSelector() override = default;

    // Get currently selected backend
    ComputeBackend selectedBackend() const;
    
    // Set backend programmatically
    void setBackend(ComputeBackend backend);
    
    // Query backend availability
    bool isBackendAvailable(ComputeBackend backend) const;
    
    // Refresh available backends
    void refreshBackends();


    void backendChanged(ComputeBackend backend);
    void backendSwitchFailed(const std::string& error);

private:
    void onBackendChanged(int index);

private:
    void setupUI();
    void detectBackends();
    std::string backendToString(ComputeBackend backend) const;
    std::string backendToIcon(ComputeBackend backend) const;
    
    // UI Components
    void* m_iconLabel{nullptr};
    void* m_backendCombo{nullptr};
    void* m_statusLabel{nullptr};
    
    // State
    std::vector<BackendInfo> m_availableBackends;
    ComputeBackend m_currentBackend{ComputeBackend::Auto};
};

} // namespace RawrXD

