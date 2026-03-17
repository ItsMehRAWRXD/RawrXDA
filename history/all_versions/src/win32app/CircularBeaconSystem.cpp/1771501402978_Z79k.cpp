// ============================================================================
// CircularBeaconSystem.cpp — CircularBeaconManager implementation
// ============================================================================
// Replaces old CircularBeaconSystem.cpp. No OpenSSL, no Vulkan, no JNI.
// Uses BeaconHub singleton from circular_beacon_system.h.
// ============================================================================

#include "CircularBeaconSystem.h"
#include "CircularBeaconManager.h"
#include <sstream>

namespace RawrXD {

// ── CircularBeaconManager implementation ──

void CircularBeaconManager::initializeFullCircularSystem(HWND parentHwnd, Win32IDE* ide) {
    if (m_initialized) return;

    m_parentHwnd = parentHwnd;
    m_ide = ide;

    // Register IDECore as the anchor beacon
    BeaconHub::instance().registerBeacon(
        BeaconKind::IDECore, "IDECore", ide,
        [](const BeaconMessage& msg) -> BeaconResponse {
            OutputDebugStringA("[IDECore] Beacon message received\n");
            return { true, 0, "ok", BeaconKind::IDECore };
        });

    m_initialized = true;
    OutputDebugStringA("[CircularBeaconManager] Full circular system initialized\n");
}

void CircularBeaconManager::shutdown() {
    if (!m_initialized) return;

    // Broadcast shutdown to all beacons
    BeaconHub::instance().broadcast(BeaconKind::IDECore, "system.shutdown");

    // Destroy all bridge instances
    m_bridges.clear();

    // Unregister IDECore anchor
    BeaconHub::instance().unregisterBeacon(BeaconKind::IDECore);

    m_initialized = false;
    m_ide = nullptr;
    OutputDebugStringA("[CircularBeaconManager] Shutdown complete\n");
}

void CircularBeaconManager::performSystemWideHotReload() {
    if (!m_initialized) return;
    BeaconHub::instance().broadcast(BeaconKind::IDECore, BEACON_CMD_HOTRELOAD);
    OutputDebugStringA("[CircularBeaconManager] System-wide hot reload broadcast\n");
}

void CircularBeaconManager::executeAgenticWorkflow(const std::string& workflow) {
    if (!m_initialized) return;
    BeaconHub::instance().send(BeaconKind::IDECore, BeaconKind::AgenticEngine,
                               "agent.execute", workflow.c_str(), workflow.size());
}

std::string CircularBeaconManager::getCircularSystemStatus() {
    if (!m_initialized) return "CircularBeaconManager: NOT INITIALIZED";

    auto stats = BeaconHub::instance().getStats();
    std::ostringstream ss;
    ss << "=== RawrXD Circular Beacon System Status ===\n"
       << "Total Beacons:    " << stats.totalBeacons << "\n"
       << "Active Beacons:   " << stats.activeBeacons << "\n"
       << "Messages Sent:    " << stats.totalMessagesSent << "\n"
       << "Messages Recv:    " << stats.totalMessagesReceived << "\n"
       << "Broadcasts:       " << stats.totalBroadcasts << "\n"
       << "Bridges Owned:    " << m_bridges.size() << "\n"
       << "===========================================\n";
    return ss.str();
}

void CircularBeaconManager::performEmergencyCircularReset(HWND hwnd) {
    OutputDebugStringA("[CircularBeaconManager] EMERGENCY RESET\n");
    shutdown();
    Sleep(50);
    initializeFullCircularSystem(hwnd ? hwnd : m_parentHwnd, m_ide);
}

PanelBeaconBridge* CircularBeaconManager::getBridge(BeaconKind kind) {
    auto it = m_bridges.find(static_cast<uint32_t>(kind));
    if (it != m_bridges.end()) return it->second.get();
    return nullptr;
}

void CircularBeaconManager::registerPanelBridge(BeaconKind kind, const char* name,
                                                 void* instance, BeaconHandler handler) {
    auto bridge = std::make_unique<PanelBeaconBridge>();
    bridge->init(kind, name, instance, handler);
    m_bridges[static_cast<uint32_t>(kind)] = std::move(bridge);
}

} // namespace RawrXD