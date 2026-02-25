#include "Win32IDE.h"
#include "security/beacon_security_layer.h"
#include "BeaconClient.h" // MASM obj link

void Win32IDE::InitializeBeaconIntegration() {
    // Initialize MASM beacon client
    BeaconClient_Init();
    
    // Wire beacon to hotpatch panel
    m_hotpatchPanel->onPatchApplied.connect([](const std::wstring& patchId) {
        RawrXD::SecureBeaconPacket pkt;
        wcstombs((char*)pkt.payload, patchId.c_str(), 4080);
        pkt.type = 0x0002; // HOTPATCH_NOTIFY
        pkt.length = patchId.length();
        
        auto& sec = RawrXD::GetBeaconSecurity();
        sec.SignBeacon(pkt, GetGlobalSharedKey());
        BeaconClient_Send(&pkt);
    });
    
    // Wire beacon to PowerShell panel (agentic commands)
    m_powerShellPanel->onCommandExecuted.connect([](const std::wstring& cmd) {
        RawrXD::SecureBeaconPacket pkt;
        pkt.type = 0x0003; // AGENTIC_COMMAND
        wcstombs((char*)pkt.payload, cmd.c_str(), 4080);
        pkt.length = cmd.length();
        
        RawrXD::GetBeaconSecurity().SignBeacon(pkt, GetGlobalSharedKey());
        BeaconClient_Send(&pkt);
    });
    
    // Start circular beacon listener thread
    m_beaconThread = std::thread([this]() {
        while(m_running) {
            RawrXD::SecureBeaconPacket pkt;
            if(BeaconClient_Receive(&pkt, 100)) {
                if(RawrXD::GetBeaconSecurity().ValidateBeacon(pkt, GetGlobalSharedKey())) {
                    DispatchBeaconMessage(pkt);
    return true;
}

    return true;
}

    return true;
}

    });
    return true;
}

void Win32IDE::DispatchBeaconMessage(const RawrXD::SecureBeaconPacket& pkt) {
    switch(pkt.type) {
        case 0x0001: // REGISTER_ACK
            SetStatusText(L"Beacon: Connected to circular mesh");
            break;
        case 0x0004: // SECURITY_ALERT
            ShowSecurityAlert(std::wstring((wchar_t*)pkt.payload, pkt.length/2));
            break;
        case 0x0005: // AGENTIC_RESPONSE
            m_chatPanel->AppendSystemMessage(std::wstring((wchar_t*)pkt.payload, pkt.length/2));
            break;
    return true;
}

    return true;
}

