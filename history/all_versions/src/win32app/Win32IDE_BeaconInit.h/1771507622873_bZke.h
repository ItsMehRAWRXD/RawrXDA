// Win32IDE_BeaconInit.h — Beacon System Initialization for Win32IDE
// Wires WinHTTPBeaconClient + GUIPaneBeaconWiring into the IDE lifecycle.
// Include this header from Win32IDE.h and call InitializeBeaconSystem() from onCreate().
#pragma once

#include "../beacon/BeaconClient.h"
#include "../beacon/gui_pane_beacon_wiring.h"
#include "../../include/circular_beacon_system.h"

// ============================================================================
// Beacon system members — add to Win32IDE private section:
//   std::unique_ptr<RawrXD::WinHTTPBeaconClient>  m_httpBeacon;
//   std::unique_ptr<RawrXD::GUIPaneBeaconWiring>   m_beaconWiring;
// ============================================================================

namespace Win32IDE_BeaconInit {

/// Full beacon-stack initialisation.
/// Call from Win32IDE::onCreate() or deferredHeavyInit() after all GUI panes
/// and the CircularBeaconManager are ready.
///
/// @param ide         pointer to the Win32IDE (for member access)
/// @param httpBeacon  out — receives the WinHTTP beacon client
/// @param wiring      out — receives the GUI pane wiring object
/// @param beaconMgr   existing CircularBeaconManager (m_circularBeaconManager)
/// @param beaconClient existing BeaconClient           (m_beaconClient)
/// @param securePatcher existing SecureHotpatchOrchestrator (m_securePatcher)
/// @param eventBus    global EventBus reference
///
inline bool InitializeBeaconSystem(
        std::unique_ptr<RawrXD::WinHTTPBeaconClient>& httpBeacon,
        std::unique_ptr<RawrXD::GUIPaneBeaconWiring>& wiring,
        CircularBeaconManager*  beaconMgr,
        BeaconClient*           beaconClient,
        SecureHotpatchOrchestrator* securePatcher,
        RawrXD::EventBus&       eventBus)
{
    // ── 1. Start the WinHTTP beacon transport ───────────────────────────
    httpBeacon = std::make_unique<RawrXD::WinHTTPBeaconClient>();

    // Register inbound handlers that forward into the native BeaconHub ring
    httpBeacon->RegisterHandler("agentic.response", [](const char* verb, const char* payload) {
        RawrXD::BeaconHub::instance().broadcast(
            RawrXD::BeaconKind::AgenticEngine, verb);
    });

    httpBeacon->RegisterHandler("hotpatch.status", [&eventBus](const char* verb, const char* payload) {
        eventBus.HotpatchApplied.emit(std::string(payload ? payload : ""));
    });

    httpBeacon->RegisterHandler("security.alert", [&eventBus](const char* verb, const char* payload) {
        eventBus.SecurityViolation.emit(std::string(payload ? payload : ""));
    });

    // Start the polling transport (connects to beacon-manager on localhost)
    if (!httpBeacon->Start("127.0.0.1", 8099)) {
        // Non-fatal — IDE works without the HTTP beacon layer
        httpBeacon.reset();
    }

    // ── 2. Wire GUI panes into BeaconHub ────────────────────────────────
    wiring = std::make_unique<RawrXD::GUIPaneBeaconWiring>();

    // If the HTTP beacon is alive, wire inbound hotpatch/agentic handlers
    if (httpBeacon) {
        wiring->WireInboundHotpatchHandler(*httpBeacon, securePatcher);
    }

    // ── 3. Bridge existing BeaconClient → EventBus ──────────────────────
    if (beaconClient) {
        beaconClient->setMessageCallback([&eventBus](const std::string& msg) {
            eventBus.AgentMessage.emit(msg);
        });
    }

    // ── 4. Register core subsystems with BeaconHub if not already done ──
    auto& hub = RawrXD::BeaconHub::instance();

    // IDE subsystem beacon
    hub.registerBeacon(
        RawrXD::BeaconKind::IDE, "Win32IDE", nullptr,
        [&eventBus](const RawrXD::BeaconMessage& bm) {
            // IDE-targeted messages get reflected onto the EventBus
            if (bm.verb && std::string(bm.verb) == "file.open") {
                eventBus.FileOpened.emit(std::string(bm.payload ? bm.payload : ""));
            }
        });

    // Security subsystem beacon
    if (securePatcher) {
        hub.registerBeacon(
            RawrXD::BeaconKind::SecurityMonitor, "SecurePatcher", securePatcher,
            [securePatcher](const RawrXD::BeaconMessage& bm) {
                if (bm.verb && std::string(bm.verb) == "hotpatch.apply") {
                    // Route through RBAC-guarded patcher
                    securePatcher->RequestPatch(
                        "beacon-session",                       // session token
                        std::string(bm.payload ? bm.payload : "beacon-patch"),  // patch name
                        nullptr,                                // target (resolved by patcher)
                        {});                                    // opcodes (from payload)
                }
            });
    }

    // ── 5. Walk the ring once to verify connectivity ────────────────────
    hub.walkRing();

    return true;
}

/// Shutdown — call from Win32IDE::onDestroy()
inline void ShutdownBeaconSystem(
        std::unique_ptr<RawrXD::WinHTTPBeaconClient>& httpBeacon,
        std::unique_ptr<RawrXD::GUIPaneBeaconWiring>& wiring)
{
    if (httpBeacon) {
        httpBeacon->Stop();
        httpBeacon.reset();
    }
    wiring.reset();
}

} // namespace Win32IDE_BeaconInit
