// Minimal Win32IDEBridge implementation for Win32IDE link closure when the full
// bridge (ObsTel / extended wiring) is not built. preprocessMessage returns 0 so
// Win32IDE::handleMessage continues normal dispatch.

#include "Win32IDEBridge.hpp"

namespace RawrXD::Agentic::Bridge {

Win32IDEBridge& Win32IDEBridge::instance() {
    static Win32IDEBridge s_inst;
    return s_inst;
}

bool Win32IDEBridge::initialize(HINSTANCE hInst, int /*nCmdShow*/) {
    hInstance_ = hInst;
    initialized_ = true;
    return true;
}

void Win32IDEBridge::shutdown() {
    initialized_ = false;
    mainHwnd_ = nullptr;
    router_ = nullptr;
    hotpatch_ = nullptr;
    observabilityReady_ = false;
}

LRESULT Win32IDEBridge::preprocessMessage(HWND hwnd, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    if (hwnd != nullptr) {
        mainHwnd_ = hwnd;
    }
    return 0;
}

void Win32IDEBridge::onIdle() {}

void* Win32IDEBridge::requestCapability(const char* /*name*/, uint32_t /*version*/) {
    return nullptr;
}

bool Win32IDEBridge::registerCapability(const char* /*name*/, uint32_t /*version*/,
                                          Wiring::CapabilityFactory /*factory*/,
                                          const std::vector<std::string>& /*deps*/) {
    return false;
}

bool Win32IDEBridge::registerHotpatch(const char* /*target*/, void* /*replacement*/) {
    return false;
}

bool Win32IDEBridge::enableHotpatch(const char* /*name*/) {
    return false;
}

bool Win32IDEBridge::disableHotpatch(const char* /*name*/) {
    return false;
}

void Win32IDEBridge::logFunctionCall(const std::string& /*functionName*/) {}

void Win32IDEBridge::logError(const std::string& /*functionName*/, const std::string& /*error*/) {}

void Win32IDEBridge::metric(const std::string& /*name*/, double /*value*/) {}

void Win32IDEBridge::setFeatureFlag(const std::string& /*feature*/, bool /*enabled*/) {}

bool Win32IDEBridge::isFeatureEnabled(const std::string& /*feature*/) const {
    return false;
}

size_t Win32IDEBridge::getCapabilityCount() const {
    return 0;
}

size_t Win32IDEBridge::getHotpatchCount() const {
    return 0;
}

bool Win32IDEBridge::initializeCapabilities() {
    return true;
}

bool Win32IDEBridge::initializeHotpatching() {
    return true;
}

bool Win32IDEBridge::initializeObservability() {
    observabilityReady_ = false;
    return true;
}

LRESULT Win32IDEBridge::handleAgenticMessage(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    return 0;
}

LRESULT Win32IDEBridge::handleHotkeyMessage(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    return 0;
}

bool Win32IDEBridge::loadConfig() {
    return true;
}

bool Win32IDEBridge::saveConfig() {
    return true;
}

} // namespace RawrXD::Agentic::Bridge
