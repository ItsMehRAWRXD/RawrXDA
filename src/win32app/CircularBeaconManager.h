// ============================================================================
// CircularBeaconManager.h — Orchestrates all panel beacons via BeaconHub
// ============================================================================
// Replaces the old CircularBeaconManager that used OpenSSL/Vulkan/JNI.
// This version: C++20, Win32 only, zero external deps.
// Manages PanelBeaconBridge instances for ALL 40+ panels.
// ============================================================================

#pragma once

#include "../../include/circular_beacon_system.h"
#include <memory>
#include <array>
#include <string>
#include <memory>
#include <map>
#include <windows.h>

// Forward declare Win32IDE — the manager holds a pointer but doesn't include the header
class Win32IDE;

namespace RawrXD {

// ============================================================================
// CircularBeaconManager — Owns all PanelBeaconBridge instances
// ============================================================================
class CircularBeaconManager {
public:
    CircularBeaconManager() = default;
    ~CircularBeaconManager() { shutdown(); }

    // ── Lifecycle ──
    void initializeFullCircularSystem(HWND parentHwnd, Win32IDE* ide = nullptr);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── System-wide operations ──
    void performSystemWideHotReload();
    void executeAgenticWorkflow(const std::string& workflow);
    std::string getCircularSystemStatus();
    void performEmergencyCircularReset(HWND hwnd = nullptr);

    // ── Bridge access (for cross-panel lookups) ──
    PanelBeaconBridge* getBridge(BeaconKind kind);

private:
    bool m_initialized{false};
    Win32IDE* m_ide{nullptr};
    HWND m_parentHwnd{nullptr};

    // One PanelBeaconBridge per panel/subsystem slot.
    // Only the ones that have real HWND/instances are initialized.
    std::unordered_map<uint32_t, std::unique_ptr<PanelBeaconBridge>> m_bridges;

    // Helpers
    void registerPanelBridge(BeaconKind kind, const char* name, void* instance,
                             BeaconHandler handler = nullptr);
};

} // namespace RawrXD
