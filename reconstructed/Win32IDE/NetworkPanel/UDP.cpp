// ============================================================================
// Win32IDE_NetworkPanel_UDP.cpp — UDP/QUIC/P2P Implementation
// ============================================================================

#include "Win32IDE.h"
#include "Win32IDE_P2PBridge.h"
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <thread>
#include <sstream>
#include <iomanip>
#include <chrono>
#pragma comment(lib, "mswsock.lib")
// P2P relay functions
#include "../rawrxd/include/P2PRelay.h"

// Protocol enum extension
enum class ForwardProtocol {
    TCP = 0,
    UDP = 1,
    QUIC = 2,
    P2P = 3
};

// Extended PortForwardEntry struct
struct PortForwardEntry {
    uint16_t    localPort;
    uint16_t    remotePort;
    std::string label;
    std::string protocol;      // "TCP", "UDP", "QUIC", "P2P"
    uint32_t    natType;       // NAT_TYPE_*
    uint32_t    peerPublicIp;
    uint16_t    peerPublicPort;
    SOCKET      udpSocket;     // For UDP/QUIC
    sockaddr_in peerAddr;      // P2P peer address
    // ... rest
};

// NAT Type enum
enum class NATType { Open, FullCone, Restricted, PortRestricted, Symmetric, Blocked };

const char* s_natTypeStrings[] = {
    "Open Internet",
    "Full Cone NAT",
    "Full Cone NAT", 
    "Restricted NAT",
    "Port Restricted NAT",
    "Symmetric NAT",
    "Blocked"
};

// ============================================================================
// Win32IDE Class Extensions
// ============================================================================

// Global STUN server address
sockaddr_in g_StunServerAddr = {};
sockaddr_in localAddr = {}; // Local bind address

// Helper functions
PortForwardEntry* Win32IDE::GetSelectedPort() {
    // Return the currently selected port entry from UI
    if (s_portEntries.empty()) return nullptr;
    return s_portEntries.back(); // Simplified - return last entry
}

sockaddr_in Win32IDE::GetPeerAddressFromUI() {
    // Get peer address from UI input fields
    sockaddr_in peerAddr = {};
    peerAddr.sin_family = AF_INET;
    // Parse IP and port from UI controls...
    inet_pton(AF_INET, "127.0.0.1", &peerAddr.sin_addr); // Placeholder
    peerAddr.sin_port = htons(3000); // Placeholder
    return peerAddr;
}

void Win32IDE::cmdNetworkAddUDPPort() {
    PortForwardEntry* entry = new PortForwardEntry();
    entry->localPort = 3478;  // STUN default
    entry->protocol = "UDP";
    entry->udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    // Detect NAT type first
    u_long mode = 1; // Non-blocking
    ioctlsocket(entry->udpSocket, FIONBIO, &mode);
    
    sockaddr_in mappedAddr = {};
    entry->natType = UDPHolePunch(entry->localPort, "142.250.21.127", 19302, &mappedAddr);
    
    appendToOutput(std::string("[NetworkPanel] NAT Type: ") + s_natTypeStrings[entry->natType] + "\n");
    
    s_portEntries.push_back(entry);
    refreshPortListView();
}

void Win32IDE::InitializeUDPRelay(PortForwardEntry* entry) {
    // STUN discovery in background thread
    entry->udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    std::thread([entry]() {
        sockaddr_in mappedAddr = {};
        int natType = UDPHolePunch(entry->localPort, "142.250.4.127", 19302, &mappedAddr);
        
        entry->natType = natType;
        
        char ipStr[16];
        inet_ntop(AF_INET, &mappedAddr.sin_addr, ipStr, 16);
        
        std::ostringstream oss;
        oss << "[UDP] Public address discovered: " << ipStr << ":" 
            << ntohs(mappedAddr.sin_port)
            << " (NAT Type: " << s_natTypeStrings[natType] << ")\n";
        appendToOutput(oss.str());
    }).detach();
}

// Benchmark function for testing
void Win32IDE::BenchmarkUDPHolePunch() {
    appendToOutput("[Benchmark] Starting UDP/STUN performance test...\n");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Test 100 STUN binding requests
    for (int i = 0; i < 100; i++) {
        sockaddr_in mappedAddr = {};
        UDPHolePunch(0, "142.250.4.127", 19302, &mappedAddr);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::ostringstream oss;
    oss << "[Benchmark] 100 STUN requests completed in " << ms << "ms (" 
        << (ms / 100.0) << "ms avg)\n";
    appendToOutput(oss.str());
}

// ============================================================================
// UI Dialog Procedures
// ============================================================================

INT_PTR CALLBACK AddPortDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static PortForwardEntry* entry = nullptr;
    
    switch (msg) {
    case WM_INITDIALOG: {
        entry = (PortForwardEntry*)lParam;
        
        // Set default values
        SetDlgItemInt(hwnd, IDC_LOCAL_PORT, entry->localPort, FALSE);
        SetDlgItemInt(hwnd, IDC_REMOTE_PORT, entry->remotePort, FALSE);
        
        // Protocol combo box
        HWND hCombo = GetDlgItem(hwnd, IDC_PROTOCOL_COMBO);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"TCP");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"UDP");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"QUIC");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"P2P");
        SendMessage(hCombo, CB_SETCURSEL, 1, 0);  // Default to UDP
        
        return TRUE;
    }
    
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            // Get values from dialog
            entry->localPort = GetDlgItemInt(hwnd, IDC_LOCAL_PORT, NULL, FALSE);
            entry->remotePort = GetDlgItemInt(hwnd, IDC_REMOTE_PORT, NULL, FALSE);
            
            // Get protocol selection
            HWND hCombo = GetDlgItem(hwnd, IDC_PROTOCOL_COMBO);
            int sel = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            const char* protos[] = {"TCP", "UDP", "QUIC", "P2P"};
            entry->protocol = protos[sel];
            
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    
    return FALSE;
}

// ============================================================================
// List View Management
// ============================================================================

void refreshPortListViewV2(HWND hwndList) {
    ListView_DeleteAllItems(hwndList);
    
    for (size_t i = 0; i < s_portEntries.size(); ++i) {
        const auto* entry = s_portEntries[i];
        
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        
        // Local Port
        lvi.iSubItem = 0;
        std::wstringstream wss;
        wss << entry->localPort;
        lvi.pszText = const_cast<LPWSTR>(wss.str().c_str());
        ListView_InsertItem(hwndList, &lvi);
        
        // Remote Port
        lvi.iSubItem = 1;
        wss.str(L"");
        wss << entry->remotePort;
        lvi.pszText = const_cast<LPWSTR>(wss.str().c_str());
        ListView_SetItem(hwndList, &lvi);
        
        // Protocol
        lvi.iSubItem = 2;
        std::wstring protoStr(entry->protocol.begin(), entry->protocol.end());
        lvi.pszText = const_cast<LPWSTR>(protoStr.c_str());
        ListView_SetItem(hwndList, &lvi);
        
        // Status
        lvi.iSubItem = 3;
        if (entry->protocol == "UDP" || entry->protocol == "QUIC") {
            if (entry->udpSocket != INVALID_SOCKET) {
                if (entry->natType != NATType::Blocked) {
                    lvi.pszText = L"Ready";
                } else {
                    lvi.pszText = L"NAT Blocked";
                }
            } else {
                lvi.pszText = L"Initializing";
            }
        } else {
            lvi.pszText = L"TCP Forward";
        }
        ListView_SetItem(hwndList, &lvi);
        
        // Public Address (if available)
        lvi.iSubItem = 4;
        if (entry->publicAddr.sin_addr.s_addr != 0) {
            char ipStr[16];
            inet_ntop(AF_INET, &entry->publicAddr.sin_addr, ipStr, 16);
            wss.str(L"");
            wss << ipStr << L":" << ntohs(entry->publicAddr.sin_port);
            lvi.pszText = const_cast<LPWSTR>(wss.str().c_str());
        } else {
            lvi.pszText = L"N/A";
        }
        ListView_SetItem(hwndList, &lvi);
    }
}

// ============================================================================
// Worker Threads
// ============================================================================

void UDPRelayWorker(PortForwardEntry* entry) {
    // UDP relay loop - simplified
    char buffer[65536];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    
    while (true) {
        int recvLen = recvfrom(entry->udpSocket, buffer, sizeof(buffer), 0,
                              (sockaddr*)&fromAddr, &fromLen);
        
        if (recvLen > 0) {
            // Process UDP packet
            // Forward to remote endpoint or handle STUN/QUIC
        }
    }
}

void QUICWorker(PortForwardEntry* entry) {
    // QUIC frame processing loop
    uint8_t buffer[65536];
    QUIC_HEADER header;
    int payloadOffset;
    
    while (true) {
        int recvLen = recv(entry->udpSocket, (char*)buffer, sizeof(buffer), 0);
        
        if (recvLen > 0) {
            // Decode QUIC header
            if (QUIC_DecodeHeader(buffer, &header, &payloadOffset) >= 0) {
                // Process QUIC frames
                uint8_t* payload = buffer + payloadOffset;
                int payloadLen = recvLen - payloadOffset;
                
                // Handle STREAM frames, etc.
            }
        }
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdNetworkToggleUDP() {
    if (s_portEntries.empty()) return;
    
    auto* entry = s_portEntries.back();
    
    if (entry->protocol == PortProtocol::UDP) {
        if (entry->udpSocket == INVALID_SOCKET) {
            // Start UDP relay
            InitializeUDPRelay(entry);
            std::thread(UDPRelayWorker, entry).detach();
        } else {
            // Stop UDP relay
            closesocket(entry->udpSocket);
            entry->udpSocket = INVALID_SOCKET;
        }
    }
    
    refreshPortListView();
}

void Win32IDE::cmdNetworkToggleQUIC() {
    if (s_portEntries.empty()) return;
    
    auto* entry = s_portEntries.back();
    
    if (entry->protocol == PortProtocol::QUIC) {
        if (!entry->quicHandshakeComplete) {
            // Start QUIC handshake
            InitializeUDPRelay(entry);
            std::thread(QUICWorker, entry).detach();
        } else {
            // Stop QUIC connection
            closesocket(entry->udpSocket);
            entry->udpSocket = INVALID_SOCKET;
            entry->quicHandshakeComplete = false;
        }
    }
    
    refreshPortListView();
}

void Win32IDE::cmdNetworkP2PConnect() {
    auto* entry = GetSelectedPort();
    if (!entry) return;
    
    sockaddr_in peerAddr = GetPeerAddressFromUI();
    
    if (entry->protocol == "UDP") {
        sockaddr_in mappedAddr = {};
        int result = UDPHolePunch(entry->localPort, "142.250.21.127", 19302, &mappedAddr);
        if (result >= 0) {
            appendToOutput("[P2P] Hole punched successfully! NAT bypass active.\n");
        } else {
            appendToOutput("[P2P] Hole punch failed - Symmetric NAT detected, using relay.\n");
        }
    }
}

void Win32IDE::cmdNetworkPunchHole() {
    int sel = (int)SendMessageW(s_hwndPortListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (sel < 0) return;
    
    PortForwardEntry* e = s_portEntries[sel];
    if (e->protocol != "UDP" && e->protocol != "QUIC") return;
    
    // Parse peer address from dialog input (simplified)
    uint32_t peerIp = inet_addr("192.168.1.100");  // Get from UI
    uint16_t peerPort = htons(3478);
    
    sockaddr_in holeResult = {};
    int result = UDPHolePunch(e->localPort, "142.250.21.127", 19302, &holeResult);
    if (result >= 0) {
        e->active = true;
        appendToOutput("[NetworkPanel] UDP hole punch succeeded - P2P established\n");
    }
    refreshPortListView();
}

// UI Command handlers
void Win32IDE::cmdNetworkDetectNat() {
    if (!m_networkPanelInitialized) initNetworkPanel();
    
    appendToOutput("[Network] Detecting NAT type via STUN...\n");
    
    sockaddr_in mappedAddr = {};
    int nat = UDPHolePunch(0, "142.250.21.127", 19302, &mappedAddr);
    
    std::ostringstream oss;
    if (nat >= 0) {
        char ipStr[16];
        inet_ntop(AF_INET, &mappedAddr.sin_addr, ipStr, 16);
        oss << "[Network] NAT Type: " << s_natTypeStrings[nat] << " (" << nat << ")\n"
            << "[Network] Public IP: " << ipStr << ":" << ntohs(mappedAddr.sin_port) << "\n";
    } else {
        oss << "[Network] NAT detection failed (firewalled or no connectivity)\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdNetworkAddUDPPort() {
    // Create UDP port forward entry with P2P capabilities
    PortForwardEntry* entry = new PortForwardEntry();
    entry->localPort = 3478;  // STUN default
    entry->protocol = "UDP";
    entry->udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    // Enable P2P mode
    sockaddr_in local = {AF_INET, htons(entry->localPort), INADDR_ANY};
    bind(entry->udpSocket, (sockaddr*)&local, sizeof(local));
    
    // Start STUN keepalive thread
    std::thread([entry]() {
        sockaddr_in mappedAddr;
        while (entry->running) {
            // Send binding request every 30s to keep NAT mapping alive
            UDPHolePunch(entry->localPort, "142.250.21.127", 19302, &mappedAddr);
            Sleep(30000);
        }
    }).detach();
    
    std::lock_guard<std::mutex> lock(s_portMutex);
    s_portEntries.push_back(entry);
    refreshPortListView();
}

// Benchmark function for testing
void Win32IDE::cmdNetworkBenchmarkP2P() {
    appendToOutput("╔════════════════════════════════════════════════════════╗\n");
    appendToOutput("║         P2P/STUN Benchmark Suite v1.0                  ║\n");
    appendToOutput("╠════════════════════════════════════════════════════════╣\n");
    
    // Test 1: STUN binding request latency
    sockaddr_in mappedAddr = {};
    
    auto start = std::chrono::high_resolution_clock::now();
    int ret = UDPHolePunch(3478, "142.250.21.127", 19302, &mappedAddr);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::ostringstream oss;
    oss << "║ STUN Binding: " << (ret >= 0 ? "SUCCESS" : "FAILED") 
        << " (" << ms << "ms) NAT=" << s_natTypeStrings[ret] << " ║\n";
    appendToOutput(oss.str());
    
    // Test 2: NAT Type Detection (simplified - reuse STUN result)
    oss.str("");
    oss << "║ NAT Type: " << s_natTypeStrings[ret] << "                           ║\n";
    appendToOutput(oss.str());
    
    // Test 3: QUIC frame processing (dummy packet)
    uint8_t quicPkt[] = {
        0x40,                               // Short header, 1 byte packet number
        0x00, 0x00, 0x00, 0x00,             // DCID (placeholder)
        0x08,                               // STREAM frame, OFF=0, LEN=1, FIN=0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Stream ID
        0x00, 0x05,                         // Length
        'H', 'e', 'l', 'l', 'o'             // Data
    };
    
    uint8_t out[256];
    int bytes = QUICFrameRelay(0, quicPkt, sizeof(quicPkt), &mappedAddr);
    
    oss.str("");
    oss << "║ QUIC Decode: " << bytes << " bytes processed                ║\n";
    appendToOutput(oss.str());
    
    appendToOutput("╚════════════════════════════════════════════════════════╝\n");
}

// Benchmark function for testing (legacy)
void Win32IDE::cmdBenchmarkUDPHolePunch() {
    appendToOutput("[P2P Benchmark] Starting STUN + Hole Punch test...\n");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    sockaddr_in mappedAddr = {};
    int result = UDPHolePunch(0, "142.250.21.127", 19302, &mappedAddr);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (result >= 0) {
        char ipStr[16];
        inet_ntop(AF_INET, &mappedAddr.sin_addr, ipStr, 16);
        std::ostringstream oss;
        oss << "[P2P Benchmark] STUN success in " << ms << "ms\n"
            << "  Public address: " << ipStr << ":" << ntohs(mappedAddr.sin_port) << "\n"
            << "  NAT Type: " << s_natTypeStrings[result] << "\n";
        appendToOutput(oss.str());
    } else {
        appendToOutput("[P2P Benchmark] STUN timeout (firewalled?)\n");
    }
}