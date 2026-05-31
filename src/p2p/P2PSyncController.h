// ============================================================================
// P2PSyncController.h — Phase 2: Sovereign Swarm Node Controller
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "AssetExchangeManager_Internal.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace RawrXD {
namespace P2P {

struct PeerNode {
    std::string id;
    std::string endpoint; // IP:Port
    uint32_t capabilities; // AVX2, AVX512, VRAM_SIZE
    int64_t lastSeen;
};

class P2PSyncController {
public:
    P2PSyncController();
    ~P2PSyncController();

    // Start discovery and sync listening
    bool Start(uint16_t port = 4488);
    void Stop();

    // Discovery
    void BroadcastIdentity();
    std::vector<PeerNode> GetActivePeers();
    void SetLocalCapabilities(uint32_t capabilities);
    uint32_t GetLocalCapabilities() const;

    // Zero-Knowledge Sync
    bool ProposeKernel(const std::string& kernelName, const std::vector<uint8_t>& binary);
    bool VerifyAndAcceptKernel(const std::string& peerId, const std::string& kernelName);

private:
    void DiscoveryLoop();
    void ListenLoop();
    void UpsertPeer(const std::string& ip, const IdentityAnnouncement& announcement);

    std::atomic<bool> m_running{false};
    uint16_t m_port{4488};
    SOCKET m_udpSocket{INVALID_SOCKET};
    SOCKET m_tcpListenSocket{INVALID_SOCKET};
    
    std::thread m_discoveryThread;
    std::thread m_listenThread;
    
    std::string m_localNodeId;
    std::atomic<uint32_t> m_localCapabilities{0};
    mutable std::mutex m_peerMutex;
    std::vector<PeerNode> m_peers;
    mutable std::mutex m_kernelMutex;
    std::unordered_map<std::string, std::vector<uint8_t>> m_localKernelStore;
    std::unordered_map<std::string, uint32_t> m_peerCapabilities;
};

} // namespace P2P
} // namespace RawrXD
