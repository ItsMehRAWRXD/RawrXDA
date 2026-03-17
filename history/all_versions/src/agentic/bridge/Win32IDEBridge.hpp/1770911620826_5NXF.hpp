#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include "../wiring/CapabilityRouter.hpp"
#include "../hotpatch/Engine.hpp"
#include "../../telemetry.h"

namespace RawrXD::Agentic::Bridge {

// Message IDs for agentic integration
constexpr UINT WM_AGENTIC_BASE = WM_USER + 1000;
constexpr UINT WM_AGENTIC_CAPABILITY_REQUEST = WM_AGENTIC_BASE + 1;
constexpr UINT WM_AGENTIC_HOTPATCH_REQUEST = WM_AGENTIC_BASE + 2;
constexpr UINT WM_AGENTIC_TELEMETRY_UPDATE = WM_AGENTIC_BASE + 3;

// Bridge configuration
struct BridgeConfig {
    bool enableCapabilities = true;
    bool enableHotpatching = true;
    bool enableObservability = true;
    std::string configPath = "rawrxd.config.toml";
    std::string buildDirectory = "build/";
};

// Minimal interface surface for Win32IDE.cpp
class Win32IDEBridge {
public:
    static Win32IDEBridge& instance();
    
    // Initialization and lifecycle
    bool initialize(HINSTANCE hInst, int nCmdShow);
    void shutdown();
    
    // Message processing hooks
    LRESULT preprocessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onIdle();  // Called during message loop idle
    
    // Capability access
    void* requestCapability(const char* name, uint32_t version);
    bool registerCapability(const char* name, uint32_t version, 
                           Wiring::CapabilityFactory factory, 
                           const std::vector<std::string>& deps = {});
    
    // Hotpatch management
    bool registerHotpatch(const char* target, void* replacement);
    bool enableHotpatch(const char* name);
    bool disableHotpatch(const char* name);
    
    // Observability
    void logFunctionCall(const std::string& functionName);
    void logError(const std::string& functionName, const std::string& error);
    void metric(const std::string& name, double value = 1.0);
    
    // Feature flags
    void setFeatureFlag(const std::string& feature, bool enabled);
    bool isFeatureEnabled(const std::string& feature) const;
    
    // Status
    bool isInitialized() const { return initialized_; }
    size_t getCapabilityCount() const;
    size_t getHotpatchCount() const;
    
private:
    Win32IDEBridge() = default;
    
    bool initialized_ = false;
    BridgeConfig config_;
    HWND mainHwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    
    // Subsystems
    Wiring::CapabilityRouter* router_ = nullptr;
    Hotpatch::Engine* hotpatch_ = nullptr;
    Observability::
    
    // Initialization methods
    bool initializeCapabilities();
    bool initializeHotpatching();
    bool initializeObservability();
    
    // Message handlers
    LRESULT handleAgenticMessage(WPARAM wParam, LPARAM lParam);
    LRESULT handleHotkeyMessage(WPARAM wParam, LPARAM lParam);
    
    // Configuration
    bool loadConfig();
    bool saveConfig();
};

} // namespace RawrXD::Agentic::Bridge

