#include "Win32IDEBridge.hpp"
#include "../manifestor/SelfManifestor.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD::Agentic::Bridge {

Win32IDEBridge& Win32IDEBridge::instance() {
    static Win32IDEBridge instance;
    return instance;
}

bool Win32IDEBridge::initialize(HINSTANCE hInst, int nCmdShow) {
    if (initialized_) {
        return true;
    }
    
    hInstance_ = hInst;
    
    // Load configuration first
    if (!loadConfig()) {
        // Create default config
        saveConfig();
    }
    
    // Initialize observability first (per AITK)
    if (config_.enableObservability && !initializeObservability()) {
        return false;
    }
    
    // Initialize capabilities
    if (config_.enableCapabilities && !initializeCapabilities()) {
        telemetry_.get()->logError("Win32IDEBridge", "Failed to initialize capabilities");
        return false;
    }
    
    // Initialize hotpatching
    if (config_.enableHotpatching && !initializeHotpatching()) {
        telemetry_.get()->logError("Win32IDEBridge", "Failed to initialize hotpatching");
        return false;
    }
    
    initialized_ = true;
    
    if (telemetry_) {
        telemetry_.get()->logInfo("Win32IDEBridge", "Agentic bridge initialized successfully");
        telemetry_.get()->metric("bridge.initialized");
    }
    
    return true;
}

void Win32IDEBridge::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (telemetry_) {
        telemetry_.get()->logInfo("Win32IDEBridge", "Agentic bridge shutting down");
    }
    
    // Shutdown subsystems in reverse order
    if (router_) {
        router_.get()->shutdownAll();
    }
    
    if (hotpatch_) {
        // Remove all hooks
        // hotpatch_.get()->removeModuleHooks(GetModuleHandle(NULL));
    }
    
    if (telemetry_) {
        telemetry_.get()->shutdown();
    }
    
    initialized_ = false;
}

LRESULT Win32IDEBridge::preprocessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!initialized_) {
        return 0; // Pass through
    }
    
    mainHwnd_ = hwnd;
    
    // Intercept for agentic features
    switch (msg) {
        case WM_AGENTIC_CAPABILITY_REQUEST:
        case WM_AGENTIC_HOTPATCH_REQUEST:
        case WM_AGENTIC_TELEMETRY_UPDATE:
            return handleAgenticMessage(wParam, lParam);
            
        case WM_KEYDOWN:
            if (hotpatch_ && hotpatch_.get()->isHotkey(static_cast<UINT>(wParam))) {
                return hotpatch_.get()->execute(static_cast<UINT>(wParam));
            }
            break;
            
        case WM_TIMER:
            // Periodic telemetry updates
            if (telemetry_ && wParam == 1001) {
                telemetry_.get()->metric("bridge.active_time");
            }
            break;
    }
    
    return 0; // Pass through to original WndProc
}

void Win32IDEBridge::onIdle() {
    if (!initialized_) {
        return;
    }
    
    // Export metrics periodically
    if (telemetry_) {
        static auto lastExport = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastExport).count() > 30) {
            // telemetry_.get()->exportMetrics(); // Implement this in Telemetry
            lastExport = now;
        }
    }
}

void* Win32IDEBridge::requestCapability(const char* name, uint32_t version) {
    if (!router_) {
        return nullptr;
    }
    
    auto capability = router_.get()->getCapability(name);
    if (!capability || capability->getVersion() < version) {
        return nullptr;
    }
    
    return capability->getInterface("");
}

bool Win32IDEBridge::registerCapability(const char* name, uint32_t version,
                                        Wiring::CapabilityFactory factory,
                                        const std::vector<std::string>& deps) {
    if (!router_) {
        return false;
    }
    
    return router_.get()->registerCapability(name, version, factory, deps);
}

bool Win32IDEBridge::registerHotpatch(const char* target, void* replacement) {
    if (!hotpatch_) {
        return false;
    }
    
    // Convert string to address
    void* targetAddr = nullptr;
    // Implementation would resolve symbol name to address
    
    return hotpatch_.get()->installHook("dynamic_hook", Hotpatch::HookType::DETOUR, 
                                         targetAddr, replacement);
}

bool Win32IDEBridge::enableHotpatch(const char* name) {
    if (!hotpatch_) {
        return false;
    }
    
    return hotpatch_.get()->enableHook(name);
}

bool Win32IDEBridge::disableHotpatch(const char* name) {
    if (!hotpatch_) {
        return false;
    }
    
    return hotpatch_.get()->disableHook(name);
}

void Win32IDEBridge::logFunctionCall(const std::string& functionName) {
    if (telemetry_) {
        telemetry_.get()->logFunctionCall(functionName);
    }
}

void Win32IDEBridge::logError(const std::string& functionName, const std::string& error) {
    if (telemetry_) {
        telemetry_.get()->logError(functionName, error);
    }
}

void Win32IDEBridge::metric(const std::string& name, double value) {
    if (telemetry_) {
        telemetry_.get()->metric(name, value);
    }
}

void Win32IDEBridge::setFeatureFlag(const std::string& feature, bool enabled) {
    if (router_) {
        router_.get()->setFeatureFlag(feature, enabled);
    }
}

bool Win32IDEBridge::isFeatureEnabled(const std::string& feature) const {
    if (router_) {
        return router_.get()->isFeatureEnabled(feature);
    }
    return false;
}

size_t Win32IDEBridge::getCapabilityCount() const {
    return router_ ? router_.get()->getRegisteredCount() : 0;
}

size_t Win32IDEBridge::getHotpatchCount() const {
    return hotpatch_ ? hotpatch_.get()->getHookCount() : 0;
}

bool Win32IDEBridge::initializeCapabilities() {
    router_ = std::make_unique<Wiring::CapabilityRouter>();
    
    // Self-manifest: detect capabilities from build artifacts
    Manifestor::SelfManifestor manifestor;
    auto capabilities = manifestor.scanBuildDirectory(config_.buildDirectory);
    
    // Register discovered capabilities
    for (auto& cap : capabilities) {
        // Create factory from discovered capability
        Wiring::CapabilityFactory factory = [cap]() {
            // This would create the actual capability instance
            // For now, return a placeholder
            return std::unique_ptr<Wiring::ICapability>(nullptr);
        };
        
        router_.get()->registerCapability(cap.name, cap.version, factory, cap.dependencies);
        
        if (telemetry_) {
            telemetry_.get()->metric("capability.registered", 1.0, {{"name", cap.name}});
        }
    }
    
    // Initialize all capabilities
    if (!router_.get()->initializeAll()) {
        return false;
    }
    
    // Generate wiring diagram
    manifestor.generateWiringDiagram("wiring_diagram.json");
    manifestor.generateReverseEngineeringPlan("reverse_engineering_plan.json");
    
    return true;
}

bool Win32IDEBridge::initializeHotpatching() {
    hotpatch_ = std::make_unique<Hotpatch::Engine>();
    
    // Install module hooks
    if (!hotpatch_.get()->installModuleHooks(GetModuleHandle(NULL))) {
        return false;
    }
    
    // Register hotkeys
    hotpatch_.get()->registerHotkey(VK_F12, []() {
        // Toggle agentic mode
        MessageBoxA(nullptr, "Agentic hotkey triggered", "Hotpatch", MB_OK);
    });
    
    return true;
}

bool Win32IDEBridge::initializeObservability() {
    telemetry_ = std::make_unique<Observability::Telemetry>();
    
    Observability::ObservabilityConfig obsConfig;
    obsConfig.enableLogging = true;
    obsConfig.enableMetrics = true;
    obsConfig.enableTracing = true;
    obsConfig.serviceName = "RawrXD-Win32IDE";
    
    return telemetry_.get()->initialize(obsConfig);
}

LRESULT Win32IDEBridge::handleAgenticMessage(WPARAM wParam, LPARAM lParam) {
    // Handle agentic-specific messages
    switch (wParam) {
        case 1: // Capability request
            // Process capability request
            break;
        case 2: // Hotpatch request
            // Process hotpatch request
            break;
        case 3: // Telemetry update
            // Process telemetry update
            break;
    }
    
    return 0;
}

LRESULT Win32IDEBridge::handleHotkeyMessage(WPARAM wParam, LPARAM lParam) {
    // Handle hotkey messages
    return 0;
}

bool Win32IDEBridge::loadConfig() {
    std::ifstream file(config_.configPath);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        json config = json::parse(file);
        
        if (config.contains("enable_capabilities")) {
            config_.enableCapabilities = config["enable_capabilities"];
        }
        if (config.contains("enable_hotpatching")) {
            config_.enableHotpatching = config["enable_hotpatching"];
        }
        if (config.contains("enable_observability")) {
            config_.enableObservability = config["enable_observability"];
        }
        if (config.contains("build_directory")) {
            config_.buildDirectory = config["build_directory"];
        }
        
    } catch (const std::exception& e) {
        return false;
    }
    
    return true;
}

bool Win32IDEBridge::saveConfig() {
    json config;
    config["enable_capabilities"] = config_.enableCapabilities;
    config["enable_hotpatching"] = config_.enableHotpatching;
    config["enable_observability"] = config_.enableObservability;
    config["build_directory"] = config_.buildDirectory;
    
    std::ofstream file(config_.configPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << config.dump(4);
    return true;
}

} // namespace RawrXD::Agentic::Bridge