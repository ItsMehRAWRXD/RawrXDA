// Link shims for the rawrxd CLI target to avoid unresolved symbols when
// running without the full Win32 IDE / Titan stack.
#include <string>
#include "../ui/webview2_bridge.hpp"
#include "../agent/agent_self_healing_orchestrator.hpp"
#include "../IDELogger.h"

namespace rawrxd::ui {

bool WebView2Bridge::initialize(HWND) { return false; }
void WebView2Bridge::shutdown() {}
void WebView2Bridge::sendBinaryMessage(ipc::MessageType, const void*, size_t) {}
void WebView2Bridge::postMessage(const std::string&) {}
void WebView2Bridge::onMessageFromUI(std::function<void(const std::string&)>) {}
void WebView2Bridge::onBinaryMessageFromUI(const uint8_t*, size_t) {}
void WebView2Bridge::snapshotModules() {}
void WebView2Bridge::initGdiFallback(HWND) {}

} // namespace rawrxd::ui

AgentSelfHealingOrchestrator& AgentSelfHealingOrchestrator::instance() {
    static AgentSelfHealingOrchestrator inst;
    return inst;
}

SelfHealReport AgentSelfHealingOrchestrator::runHealingCycle() {
    return {};
}

AgentSelfHealingOrchestrator::AgentSelfHealingOrchestrator() = default;
AgentSelfHealingOrchestrator::~AgentSelfHealingOrchestrator() = default;

extern "C" void RawrXD_Disasm_HandleReq() {}
extern "C" void RawrXD_Symbol_HandleReq() {}
extern "C" void RawrXD_Module_HandleReq() {}
