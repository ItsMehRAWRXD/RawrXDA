#include "Win32IDEBridge.hpp"
#include "../manifestor/SelfManifestor.hpp"
#include "../OrchestratorBridge.h"
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
        
        return false;
    }
    
    // Initialize hotpatching
    if (config_.enableHotpatching && !initializeHotpatching()) {
        
        return false;
    }
    
    initialized_ = true;

    // Initialize the Agent Orchestrator Bridge
    // This wires ToolRegistry, OllamaClient, FIMPromptBuilder into BoundedAgentLoop
    {
        // Use build directory parent as working dir, or fallback to CWD
        std::string workingDir = ".";
        if (!config_.buildDirectory.empty()) {
            // buildDirectory is relative; use its parent
            workingDir = config_.buildDirectory + "/..";
        }
        auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
        if (!orch.Initialize(workingDir)) {
            // Non-fatal: agent features unavailable but IDE still works
        }
    }

    return true;
}

void Win32IDEBridge::shutdown() {
    if (!initialized_) {
        return;
    }


    // Shutdown subsystems in reverse order
    if (router_) {
        router_->shutdownAll();
    }
    
    if (hotpatch_) {
        // Remove all hooks
        // hotpatch_->removeModuleHooks(GetModuleHandle(NULL));
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
            if (hotpatch_ && hotpatch_->isHotkey(static_cast<UINT>(wParam))) {
                return hotpatch_->execute(static_cast<UINT>(wParam));
            }
            break;
            
        case WM_TIMER:
            // Periodic telemetry updates
            if (telemetry_ && wParam == 1001) {
                telemetry_->recordEvent("timer_tick", {{"interval_ms", 1000}});
                // Export buffered metrics to disk periodically
                auto now = std::chrono::system_clock::now();
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                telemetry_->recordEvent("metrics_flush", {{"timestamp_ms", nowMs}});
            }
            break;
    }
    
    return 0; // Pass through to original WndProc
}

void Win32IDEBridge::onIdle() {
    if (!initialized_) {
        return;
    }

    // Export metrics periodically via telemetry subsystem
    if (telemetry_) {
        TelemetrySnapshot snap;
        if (telemetry::Poll(snap)) {
            telemetry_->recordEvent("idle_poll", {
                {"cpu_usage", snap.cpuUsagePercent},
                {"gpu_usage", snap.gpuUsagePercent},
                {"cpu_temp", snap.cpuTempC},
                {"capabilities", getCapabilityCount()},
                {"hotpatches", getHotpatchCount()}
            });
        }
    }
}

void* Win32IDEBridge::requestCapability(const char* name, uint32_t version) {
    if (!router_) {
        return nullptr;
    }
    
    auto capability = router_->getCapability(name);
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
    
    return router_->registerCapability(name, version, factory, deps);
}

bool Win32IDEBridge::registerHotpatch(const char* target, void* replacement) {
    if (!hotpatch_) {
        return false;
    }
    
    // Convert string to address
    void* targetAddr = nullptr;
    // Implementation would resolve symbol name to address
    
    return hotpatch_->installHook("dynamic_hook", Hotpatch::HookType::DETOUR, 
                                         targetAddr, replacement);
}

bool Win32IDEBridge::enableHotpatch(const char* name) {
    if (!hotpatch_) {
        return false;
    }
    
    return hotpatch_->enableHook(name);
}

bool Win32IDEBridge::disableHotpatch(const char* name) {
    if (!hotpatch_) {
        return false;
    }
    
    return hotpatch_->disableHook(name);
}

void Win32IDEBridge::logFunctionCall(const std::string& functionName) {
    if (telemetry_) {
        telemetry_->recordEvent("function_call", {{"function", functionName}});
    }
}

void Win32IDEBridge::logError(const std::string& functionName, const std::string& error) {
    if (telemetry_) {
        telemetry_->recordEvent("error", {
            {"function", functionName},
            {"error", error}
        });
    }
}

void Win32IDEBridge::metric(const std::string& name, double value) {
    if (telemetry_) {
        telemetry_->recordEvent("metric", {
            {"name", name},
            {"value", value}
        });
    }
}

void Win32IDEBridge::setFeatureFlag(const std::string& feature, bool enabled) {
    if (router_) {
        router_->setFeatureFlag(feature, enabled);
    }
}

bool Win32IDEBridge::isFeatureEnabled(const std::string& feature) const {
    if (router_) {
        return router_->isFeatureEnabled(feature);
    }
    return false;
}

size_t Win32IDEBridge::getCapabilityCount() const {
    return router_ ? router_->getRegisteredCount() : 0;
}

size_t Win32IDEBridge::getHotpatchCount() const {
    return hotpatch_ ? hotpatch_->getHookCount() : 0;
}

bool Win32IDEBridge::initializeCapabilities() {
    router_ = &Wiring::CapabilityRouter::instance();
    
    // Self-manifest: detect capabilities from build artifacts
    Manifestor::SelfManifestor manifestor;
    auto capabilities = manifestor.scanBuildDirectory(config_.buildDirectory);
    
    // Register discovered capabilities
    for (auto& cap : capabilities) {
        // Create factory from discovered build artifact's factory function pointer
        Wiring::CapabilityFactory factory = [cap]() -> std::unique_ptr<Wiring::ICapability> {
            if (cap.factory) {
                // Cast the discovered factory pointer and invoke it
                using FactoryFn = Wiring::ICapability* (*)();
                auto fn = reinterpret_cast<FactoryFn>(cap.factory);
                return std::unique_ptr<Wiring::ICapability>(fn());
            }
            return nullptr;
        };
        
        router_->registerCapability(cap.name, cap.version, factory, cap.dependencies);
        
        });
        }
    }
    
    // Initialize all capabilities
    if (!router_->initializeAll()) {
        return false;
    }
    
    // Generate wiring diagram
    manifestor.generateWiringDiagram("wiring_diagram.json");
    manifestor.generateReverseEngineeringPlan("reverse_engineering_plan.json");
    
    return true;
}

bool Win32IDEBridge::initializeHotpatching() {
    hotpatch_ = &Hotpatch::Engine::instance();
    
    // Install module hooks
    if (!hotpatch_->installModuleHooks(GetModuleHandle(NULL))) {
        return false;
    }
    
    // Register hotkeys
    hotpatch_->registerHotkey(VK_F12, []() {
        // Toggle agentic mode
        MessageBoxA(nullptr, "Agentic hotkey triggered", "Hotpatch", MB_OK);
    });
    
    return true;
}

bool Win32IDEBridge::initializeObservability() {
    // Initialize the global telemetry subsystem (two-phase: hardware polling deferred)
    if (!telemetry::Initialize()) {
        return false;
    }
    // Allocate instance-local Telemetry for structured event logging
    telemetryOwned_ = std::make_unique<Telemetry>();
    telemetry_ = telemetryOwned_.get();
    // Start a periodic timer (1 sec) for metric flushing via WM_TIMER
    if (mainHwnd_) {
        SetTimer(mainHwnd_, 1001, 1000, nullptr);
    }
    return true;
}

LRESULT Win32IDEBridge::handleAgenticMessage(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case 1: { // Capability request — lParam is const char* name
            const char* capName = reinterpret_cast<const char*>(lParam);
            if (capName && router_) {
                void* iface = requestCapability(capName, 1);
                logFunctionCall(std::string("capability_request:") + capName);
                return iface ? 1 : 0;
            }
            return 0;
        }
        case 2: { // Hotpatch request — lParam is const char* hook name
            const char* hookName = reinterpret_cast<const char*>(lParam);
            if (hookName && hotpatch_) {
                bool ok = enableHotpatch(hookName);
                logFunctionCall(std::string("hotpatch_request:") + hookName);
                return ok ? 1 : 0;
            }
            return 0;
        }
        case 3: { // Telemetry update — force a metric flush
            if (telemetry_) {
                TelemetrySnapshot snap;
                if (telemetry::Poll(snap)) {
                    telemetry_->recordEvent("telemetry_update", {
                        {"cpu_usage", snap.cpuUsagePercent},
                        {"gpu_usage", snap.gpuUsagePercent}
                    });
                }
            }
            return 1;
        }
    }
    return 0;
}

LRESULT Win32IDEBridge::handleHotkeyMessage(WPARAM wParam, LPARAM lParam) {
    if (hotpatch_) {
        UINT vk = static_cast<UINT>(wParam);
        if (hotpatch_->isHotkey(vk)) {
            logFunctionCall(std::string("hotkey:") + std::to_string(vk));
            return hotpatch_->execute(vk);
        }
    }
    return 0;
}

bool Win32IDEBridge::loadConfig() {
    std::ifstream file(config_.configPath);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        json config = json::parse(content);
        
        if (config.contains("enable_capabilities")) {
            config_.enableCapabilities = config["enable_capabilities"].get<bool>();
        }
        if (config.contains("enable_hotpatching")) {
            config_.enableHotpatching = config["enable_hotpatching"].get<bool>();
        }
        if (config.contains("enable_observability")) {
            config_.enableObservability = config["enable_observability"].get<bool>();
        }
        if (config.contains("build_directory")) {
            config_.buildDirectory = config["build_directory"].get<std::string>();
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
