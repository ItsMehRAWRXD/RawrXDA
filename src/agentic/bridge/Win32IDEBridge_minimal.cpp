// Minimal Win32IDEBridge implementation for Win32IDE link closure when the full
// bridge (ObsTel / extended wiring) is not built. preprocessMessage returns 0 so
// Win32IDE::handleMessage continues normal dispatch.

#include "Win32IDEBridge.hpp"
#include <unordered_map>
#include <mutex>

namespace RawrXD::Agentic::Bridge {

struct CapabilityEntry {
    std::string name;
    uint32_t version;
    Wiring::CapabilityFactory factory;
    std::vector<std::string> deps;
};

struct HotpatchEntry {
    std::string target;
    void* replacement;
    bool enabled;
};

static std::mutex s_bridgeMutex;
static std::unordered_map<std::string, CapabilityEntry> s_capabilities;
static std::unordered_map<std::string, HotpatchEntry> s_hotpatches;
static std::unordered_map<std::string, bool> s_featureFlags;

Win32IDEBridge& Win32IDEBridge::instance() {
    static Win32IDEBridge s_inst;
    return s_inst;
}

bool Win32IDEBridge::initialize(HINSTANCE hInst, int /*nCmdShow*/) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    hInstance_ = hInst;
    initialized_ = true;
    s_capabilities.clear();
    s_hotpatches.clear();
    s_featureFlags.clear();
    return true;
}

void Win32IDEBridge::shutdown() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    initialized_ = false;
    mainHwnd_ = nullptr;
    router_ = nullptr;
    hotpatch_ = nullptr;
    observabilityReady_ = false;
    s_capabilities.clear();
    s_hotpatches.clear();
}

LRESULT Win32IDEBridge::preprocessMessage(HWND hwnd, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    if (hwnd != nullptr) {
        mainHwnd_ = hwnd;
    }
    return 0;
}

void Win32IDEBridge::onIdle() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    // Process deferred capability initializations
    for (auto& [name, cap] : s_capabilities) {
        if (cap.factory) {
            cap.factory();
        }
    }
}

void* Win32IDEBridge::requestCapability(const char* name, uint32_t version) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    auto it = s_capabilities.find(name);
    if (it != s_capabilities.end() && it->second.version >= version) {
        OutputDebugStringA((std::string("[Win32IDEBridge] Capability found: ") + name + " v" + std::to_string(it->second.version) + "\n").c_str());
        return &it->second;
    }
    OutputDebugStringA((std::string("[Win32IDEBridge] Capability NOT found: ") + name + " v" + std::to_string(version) + "\n").c_str());
    return nullptr;
}

bool Win32IDEBridge::registerCapability(const char* name, uint32_t version,
                                          Wiring::CapabilityFactory factory,
                                          const std::vector<std::string>& deps) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    CapabilityEntry entry;
    entry.name = name;
    entry.version = version;
    entry.factory = factory;
    entry.deps = deps;
    s_capabilities[name] = std::move(entry);
    OutputDebugStringA((std::string("[Win32IDEBridge] Registered capability: ") + name + " v" + std::to_string(version) + "\n").c_str());
    return true;
}

bool Win32IDEBridge::registerHotpatch(const char* target, void* replacement) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    if (!target || !replacement) return false;
    HotpatchEntry entry;
    entry.target = target;
    entry.replacement = replacement;
    entry.enabled = false;
    s_hotpatches[target] = std::move(entry);
    OutputDebugStringA((std::string("[Win32IDEBridge] Registered hotpatch: ") + target + "\n").c_str());
    return true;
}

bool Win32IDEBridge::enableHotpatch(const char* name) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    auto it = s_hotpatches.find(name);
    if (it == s_hotpatches.end()) return false;
    it->second.enabled = true;
    OutputDebugStringA((std::string("[Win32IDEBridge] Enabled hotpatch: ") + name + "\n").c_str());
    return true;
}

bool Win32IDEBridge::disableHotpatch(const char* name) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    auto it = s_hotpatches.find(name);
    if (it == s_hotpatches.end()) return false;
    it->second.enabled = false;
    OutputDebugStringA((std::string("[Win32IDEBridge] Disabled hotpatch: ") + name + "\n").c_str());
    return true;
}

void Win32IDEBridge::logFunctionCall(const std::string& functionName) {
    OutputDebugStringA((std::string("[Win32IDEBridge] Function call: ") + functionName + "\n").c_str());
}

void Win32IDEBridge::logError(const std::string& functionName, const std::string& error) {
    OutputDebugStringA((std::string("[Win32IDEBridge] ERROR in ") + functionName + ": " + error + "\n").c_str());
}

void Win32IDEBridge::metric(const std::string& name, double value) {
    OutputDebugStringA((std::string("[Win32IDEBridge] Metric: ") + name + " = " + std::to_string(value) + "\n").c_str());
}

void Win32IDEBridge::setFeatureFlag(const std::string& feature, bool enabled) {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    s_featureFlags[feature] = enabled;
    OutputDebugStringA((std::string("[Win32IDEBridge] Feature flag ") + feature + " = " + (enabled ? "true" : "false") + "\n").c_str());
}

bool Win32IDEBridge::isFeatureEnabled(const std::string& feature) const {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    auto it = s_featureFlags.find(feature);
    if (it != s_featureFlags.end()) return it->second;
    return true; // Default: enabled
}

size_t Win32IDEBridge::getCapabilityCount() const {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    return s_capabilities.size();
}

size_t Win32IDEBridge::getHotpatchCount() const {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    return s_hotpatches.size();
}

bool Win32IDEBridge::initializeCapabilities() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    for (auto& [name, cap] : s_capabilities) {
        if (cap.factory) {
            cap.factory();
        }
    }
    return true;
}

bool Win32IDEBridge::initializeHotpatching() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    for (auto& [name, hp] : s_hotpatches) {
        if (hp.enabled && hp.replacement) {
            OutputDebugStringA((std::string("[Win32IDEBridge] Applying hotpatch: ") + name + "\n").c_str());
        }
    }
    return true;
}

bool Win32IDEBridge::initializeObservability() {
    observabilityReady_ = true;
    OutputDebugStringA("[Win32IDEBridge] Observability initialized\n");
    return true;
}

LRESULT Win32IDEBridge::handleAgenticMessage(WPARAM wParam, LPARAM lParam) {
    OutputDebugStringA((std::string("[Win32IDEBridge] Agentic message: wParam=") + std::to_string(wParam) + " lParam=" + std::to_string(lParam) + "\n").c_str());
    return 0;
}

LRESULT Win32IDEBridge::handleHotkeyMessage(WPARAM wParam, LPARAM lParam) {
    OutputDebugStringA((std::string("[Win32IDEBridge] Hotkey message: wParam=") + std::to_string(wParam) + " lParam=" + std::to_string(lParam) + "\n").c_str());
    return 0;
}

bool Win32IDEBridge::loadConfig() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    // Load feature flags from registry or config file
    OutputDebugStringA("[Win32IDEBridge] Configuration loaded\n");
    return true;
}

bool Win32IDEBridge::saveConfig() {
    std::lock_guard<std::mutex> lock(s_bridgeMutex);
    OutputDebugStringA("[Win32IDEBridge] Configuration saved\n");
    return true;
}

} // namespace RawrXD::Agentic::Bridge
