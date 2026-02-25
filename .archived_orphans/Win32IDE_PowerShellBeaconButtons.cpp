// ============================================================================
// Win32IDE_PowerShellBeaconButtons.cpp — Beacon control buttons for PS panel
// ============================================================================
// Adds Agentic/Hotpatch/Status buttons to the PowerShell panel that dispatch
// through the circular beacon system. Integrates with existing Win32IDE.
//
// No Qt. No exceptions. C++20 + Win32 only.
// ============================================================================

#include "Win32IDE.h"
#include "../beacon/BeaconClient.h"
#include "../../include/circular_beacon_system.h"
#include "../EventBus.h"

// Button command IDs for PowerShell beacon integration
#define IDM_BEACON_AGENTIC   7001
#define IDM_BEACON_HOTPATCH  7002
#define IDM_BEACON_STATUS    7003

void Win32IDE::InitializePowerShellBeaconControls() {
    // Find the existing PowerShell terminal panel handle
    // (GetPowerShellPanelHandle is expected to exist from Win32IDE infrastructure)
    HWND hPSPanel = m_hWnd; // Fallback to main window if panel not found

    // Agentic mode trigger button
    CreateWindowExW(0, L"BUTTON", L"Agentic",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 10, 100, 28,
        hPSPanel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDM_BEACON_AGENTIC)),
        GetModuleHandle(nullptr), nullptr);

    // Hotpatch trigger button
    CreateWindowExW(0, L"BUTTON", L"Hotpatch",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 10, 100, 28,
        hPSPanel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDM_BEACON_HOTPATCH)),
        GetModuleHandle(nullptr), nullptr);

    // Beacon status output (read-only edit control)
    HWND hBeaconOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        10, 45, 320, 90,
        hPSPanel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDM_BEACON_STATUS)),
        GetModuleHandle(nullptr), nullptr);

    // Wire EventBus agent messages to the beacon output control
    RawrXD::EventBus::Get().AgentMessage.connect([hBeaconOutput](const std::string& msg) {
        if (!IsWindow(hBeaconOutput)) return;
        std::string line = "[Agent] " + msg + "\r\n";
        // Append text by selecting end and replacing
        int len = GetWindowTextLengthA(hBeaconOutput);
        SendMessageA(hBeaconOutput, EM_SETSEL, len, len);
        SendMessageA(hBeaconOutput, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(line.c_str()));
    });

    // Wire hotpatch events to the beacon output
    RawrXD::EventBus::Get().HotpatchApplied.connect([hBeaconOutput]() {
        if (!IsWindow(hBeaconOutput)) return;
        int len = GetWindowTextLengthA(hBeaconOutput);
        SendMessageA(hBeaconOutput, EM_SETSEL, len, len);
        SendMessageA(hBeaconOutput, EM_REPLACESEL, 0,
                     reinterpret_cast<LPARAM>("[Hotpatch] Patch applied\r\n"));
    });

    // Wire WinHTTP beacon inbound messages if the client exists
    if (m_beaconClient) {
        // The existing m_beaconClient uses the old API (BeaconClient::sendMessage).
        // We just log inbound activity to the output control for now.
        m_beaconClient->setMessageCallback(
            [hBeaconOutput](const BeaconClient::BeaconMessage& msg) {
                if (!IsWindow(hBeaconOutput)) return;
                std::string line = "[Beacon] " + msg.payload + "\r\n";
                int len = GetWindowTextLengthA(hBeaconOutput);
                SendMessageA(hBeaconOutput, EM_SETSEL, len, len);
                SendMessageA(hBeaconOutput, EM_REPLACESEL, 0,
                             reinterpret_cast<LPARAM>(line.c_str()));
            });
    return true;
}

    return true;
}

LRESULT Win32IDE::OnBeaconButtonClick(WPARAM wp) {
    auto& hub = RawrXD::BeaconHub::instance();

    if (LOWORD(wp) == IDM_BEACON_AGENTIC) {
        // Dispatch agentic command through the circular beacon ring
        hub.send(RawrXD::BeaconKind::IDECore,
                 RawrXD::BeaconKind::AgenticEngine,
                 BEACON_CMD_AGENTIC_REQUEST,
                 "{\"mode\":\"autonomous\"}", 22);

        SetStatusText(L"Beacon: Agentic command dispatched");
        RawrXD::EventBus::Get().AgentMessage.emit("Agentic mode requested via beacon");
    return true;
}

    else if (LOWORD(wp) == IDM_BEACON_HOTPATCH) {
        // Dispatch hotpatch request through circular beacon
        hub.send(RawrXD::BeaconKind::IDECore,
                 RawrXD::BeaconKind::HotpatchManager,
                 BEACON_CMD_HOTRELOAD);

        SetStatusText(L"Beacon: Hotpatch requested");
        RawrXD::EventBus::Get().HotpatchApplied.emit();
    return true;
}

    return 0;
    return true;
}

