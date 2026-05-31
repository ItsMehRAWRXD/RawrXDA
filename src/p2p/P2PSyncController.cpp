// ============================================================================
// P2PSyncController.cpp — Primary Swarm Node Manager
// ============================================================================
#include "P2PSyncController.h"
#include "AssetExchangeManager.h"
#include "AssetExchangeManager_Internal.h"
#include "EvolutionEventBus.h"
#include "ThermalThrottleMonitor.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace RawrXD {
namespace P2P {

P2PSyncController::P2PSyncController() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    const uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
    const uint32_t tick = static_cast<uint32_t>(GetTickCount());
    m_localNodeId = "LOCAL-" + std::to_string(pid) + "-" + std::to_string(tick);
    m_localCapabilities.store(CAP_AVX2 | CAP_ENCRYPTED);
}

P2PSyncController::~P2PSyncController() {
    Stop();
    WSACleanup();
}

bool P2PSyncController::Start(uint16_t port) {
    if (m_running) return true;
    m_port = port;
    m_running = true;

    // UDP socket for discovery
    m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udpSocket == INVALID_SOCKET) return false;

    // Set broadcast option
    BOOL bBroadcast = TRUE;
    setsockopt(m_udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast, sizeof(bBroadcast));

    // Non-blocking socket for UDP
    u_long mode = 1;
    ioctlsocket(m_udpSocket, FIONBIO, &mode);

    // Bind locally
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(m_udpSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_udpSocket);
        m_udpSocket = INVALID_SOCKET;
        m_running = false;
        return false;
    }

    m_discoveryThread = std::thread(&P2PSyncController::DiscoveryLoop, this);
    m_listenThread = std::thread(&P2PSyncController::ListenLoop, this);

    return true;
}

void P2PSyncController::Stop() {
    m_running = false;
    if (m_discoveryThread.joinable()) m_discoveryThread.join();
    if (m_listenThread.joinable()) m_listenThread.join();
    
    if (m_udpSocket != INVALID_SOCKET) {
        closesocket(m_udpSocket);
        m_udpSocket = INVALID_SOCKET;
    }
    if (m_tcpListenSocket != INVALID_SOCKET) {
        closesocket(m_tcpListenSocket);
        m_tcpListenSocket = INVALID_SOCKET;
    }
}

void P2PSyncController::BroadcastIdentity() {
    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(m_port);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    const uint64_t epochMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    IdentityAnnouncement announcement;
    announcement.nodeId = m_localNodeId;
    announcement.capabilities = m_localCapabilities.load();
    announcement.port = m_port;
    announcement.epochMs = epochMs;
    std::string msg = BuildIdentityMessage(announcement);
    sendto(m_udpSocket, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

    // Handle Thermal Pulse Monitoring
    auto& thermal = ThermalThrottleMonitor::Instance();
    ThrottleState state = thermal.CheckThrottle();
    const size_t peerCount = [&]() -> size_t {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        return m_peers.size();
    }();
    std::string thermalJson = "{\"status\": \"" + std::string(state.is_throttled ? "THROTTLED" : "STABLE") + "\", \"freq_mhz\": " + std::to_string(state.freq_mhz) + ", \"peers_discovered\": " + std::to_string(peerCount) + "}";
    EvolutionEventBus::Instance().Emit("PulseCycleComplete", "LocalNode", thermalJson.c_str());
}

void P2PSyncController::DiscoveryLoop() {
    while (m_running) {
        BroadcastIdentity();
        
        // Listen for peer responses
        char buffer[1024] = {0};
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);
        int bytes = recvfrom(m_udpSocket, buffer, sizeof(buffer)-1, 0, (sockaddr*)&fromAddr, &fromLen);
        
        if (bytes > 0) {
            std::string peerMsg(buffer, bytes);
            IdentityAnnouncement announcement;
            if (ParseIdentityMessage(peerMsg, announcement)) {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &fromAddr.sin_addr, ipStr, sizeof(ipStr));
                UpsertPeer(ipStr, announcement);
            } else {
                KernelProposalAnnouncement proposal;
                if (ParseKernelProposalMessage(peerMsg, proposal)) {
                    VerifyAndAcceptKernel(proposal.nodeId, proposal.kernelName);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void P2PSyncController::ListenLoop() {
    // Basic TCP listener loop placeholder (for file/kernel transfers)
}

std::vector<PeerNode> P2PSyncController::GetActivePeers() {
    std::lock_guard<std::mutex> lock(m_peerMutex);
    return m_peers;
}

void P2PSyncController::SetLocalCapabilities(uint32_t capabilities) {
    m_localCapabilities.store(capabilities);
}

uint32_t P2PSyncController::GetLocalCapabilities() const {
    return m_localCapabilities.load();
}

bool P2PSyncController::ProposeKernel(const std::string& kernelName, const std::vector<uint8_t>& binary) {
    if (!m_running.load()) {
        return false;
    }
    if (kernelName.empty() || binary.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_kernelMutex);
        m_localKernelStore[kernelName] = binary;
    }

    KernelProposalAnnouncement msg;
    msg.nodeId = m_localNodeId;
    msg.kernelName = kernelName;
    msg.payloadSize = static_cast<uint64_t>(binary.size());
    msg.epochMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const std::string wire = BuildKernelProposalMessage(msg);

    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(m_port);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(m_udpSocket, wire.c_str(), static_cast<int>(wire.size()), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

    return true;
}

bool P2PSyncController::VerifyAndAcceptKernel(const std::string& peerId, const std::string& kernelName) {
    if (peerId.empty() || kernelName.empty()) {
        return false;
    }

    uint32_t peerCaps = 0;
    {
        std::lock_guard<std::mutex> lock(m_kernelMutex);
        const auto it = m_peerCapabilities.find(peerId);
        if (it != m_peerCapabilities.end()) {
            peerCaps = it->second;
        }
    }

    if ((peerCaps & CAP_ENCRYPTED) == 0) {
        return false;
    }

    std::string decisionJson = "{\"kernel\":\"" + kernelName +
        "\",\"peer\":\"" + peerId +
        "\",\"decision\":\"accepted_for_zk_stage\"}";
    EvolutionEventBus::Instance().Emit("KernelProposalAccepted", peerId.c_str(), decisionJson.c_str());

    return true;
}

void P2PSyncController::UpsertPeer(const std::string& ip, const IdentityAnnouncement& announcement) {
    if (announcement.nodeId == m_localNodeId) {
        return;
    }

    const int64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::string endpoint = ip + ":" + std::to_string(announcement.port);

    bool isNewPeer = false;
    PeerNode newPeer;
    {
        std::lock_guard<std::mutex> lock(m_peerMutex);
        for (auto& peer : m_peers) {
            if (peer.id == announcement.nodeId) {
                peer.endpoint = endpoint;
                peer.capabilities = announcement.capabilities;
                peer.lastSeen = now;
                std::lock_guard<std::mutex> kernelLock(m_kernelMutex);
                m_peerCapabilities[peer.id] = peer.capabilities;
                return;
            }
        }

        newPeer.id = announcement.nodeId;
        newPeer.endpoint = endpoint;
        newPeer.capabilities = announcement.capabilities;
        newPeer.lastSeen = now;
        m_peers.push_back(newPeer);
        isNewPeer = true;
    }

    {
        std::lock_guard<std::mutex> kernelLock(m_kernelMutex);
        m_peerCapabilities[newPeer.id] = newPeer.capabilities;
    }

    if (isNewPeer) {
        AssetExchangeManager::Instance().OnPeerDiscovered(newPeer);
    }
}

} // namespace P2P
} // namespace RawrXD
