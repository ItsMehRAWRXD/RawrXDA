// ============================================================================
// Win32IDE_NetworkPanel_UDP.cpp — UDP/QUIC/P2P Implementation
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <thread>
#include <sstream>
#include <iomanip>
#include <chrono>
#pragma comment(lib, "mswsock.lib")

// Extern ASM functions
extern "C" {
    uint64_t UDPHolePunch_Init(int localPort);
    int UDPHolePunch_PerformStun(uint64_t socket, sockaddr_in* stunServer, sockaddr_in* outPublicAddr);
    int UDPHolePunch_Perform(uint64_t socket, sockaddr_in* peerAddr, void* context);
    int QUIC_DecodeHeader(uint8_t* packet, void* outHeader, int* outPayloadOffset);
    int QUIC_CreateStreamFrame(uint8_t* outBuffer, uint32_t streamId, void* data, int len, uint64_t offset);
}

// Protocol types enum
enum class ProtocolType { TCP, UDP, QUIC };

struct PortForwardEntry {
    uint16_t localPort, remotePort;
    ProtocolType protocol;
    NATType natType;
    sockaddr_in publicAddr;        // After STUN
    SOCKET udpSocket;
    bool quicHandshakeComplete;
    // ... existing fields
};

// NAT Type enum
enum class NATType { Open, FullCone, Restricted, PortRestricted, Symmetric, Blocked };

// ============================================================================
// Win32IDE Class Extensions
// ============================================================================

void Win32IDE::cmdNetworkAddPort() {
    // Extended dialog with protocol dropdown
    PortForwardEntry* entry = new PortForwardEntry();
    entry->localPort = 3000 + s_portEntries.size();
    entry->protocol = ProtocolType::QUIC;  // Default to QUIC for new entries
    
    if (entry->protocol == ProtocolType::UDP || entry->protocol == ProtocolType::QUIC) {
        InitializeUDPRelay(entry);
    }
    
    s_portEntries.push_back(entry);
}

void Win32IDE::InitializeUDPRelay(PortForwardEntry* entry) {
    // Initialize UDP socket via ASM
    entry->udpSocket = (SOCKET)UDPHolePunch_Init(entry->localPort);
    
    // STUN discovery in background thread
    std::thread([entry]() {
        sockaddr_in stunServer;
        // Parse stun.l.google.com:19302...
        inet_pton(AF_INET, "142.250.4.127", &stunServer.sin_addr);
        stunServer.sin_port = htons(19302);
        stunServer.sin_family = AF_INET;
        
        int natType = UDPHolePunch_PerformStun(
            (uint64_t)entry->udpSocket, 
            &stunServer, 
            &entry->publicAddr
        );
        
        entry->natType = static_cast<NATType>(natType);
        
        char ipStr[16];
        inet_ntop(AF_INET, &entry->publicAddr.sin_addr, ipStr, 16);
        
        std::ostringstream oss;
        oss << "[UDP] Public address discovered: " << ipStr << ":" 
            << ntohs(entry->publicAddr.sin_port)
            << " (NAT Type: " << natType << ")\n";
        appendToOutput(oss.str());
    }).detach();
}

// Benchmark function for testing
void Win32IDE::BenchmarkUDPHolePunch() {
    appendToOutput("[Benchmark] Starting UDP/STUN performance test...\n");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Test 1000 STUN requests
    for (int i = 0; i < 1000; i++) {
        SOCKET sock = (SOCKET)UDPHolePunch_Init(0);  // Random port
        if (sock == INVALID_SOCKET) continue;
        
        sockaddr_in stun;
        inet_pton(AF_INET, "142.250.4.127", &stun.sin_addr);
        stun.sin_port = htons(19302);
        
        sockaddr_in publicAddr;
        UDPHolePunch_PerformStun((uint64_t)sock, &stun, &publicAddr);
        
        closesocket(sock);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::ostringstream oss;
    oss << "[Benchmark] 1000 STUN requests completed in " << ms << "ms (" 
        << (ms / 1000.0) << "ms avg)\n";
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
        SendMessage(hCombo, CB_SETCURSEL, (WPARAM)static_cast<int>(entry->protocol), 0);
        
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
            entry->protocol = static_cast<ProtocolType>(sel);
            
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
        const wchar_t* protoNames[] = { L"TCP", L"UDP", L"QUIC" };
        lvi.pszText = const_cast<LPWSTR>(protoNames[static_cast<int>(entry->protocol)]);
        ListView_SetItem(hwndList, &lvi);
        
        // Status
        lvi.iSubItem = 3;
        if (entry->protocol == ProtocolType::UDP || entry->protocol == ProtocolType::QUIC) {
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
    
    if (entry->protocol == ProtocolType::UDP) {
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
    
    if (entry->protocol == ProtocolType::QUIC) {
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