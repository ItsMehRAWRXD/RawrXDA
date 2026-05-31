#include "SovereignMeshProvider.h"
#include <chrono>
#include <iostream>
#include <ws2tcpip.h>


namespace RawrXD::Networking
{

SovereignMeshProvider& SovereignMeshProvider::Instance()
{
    static SovereignMeshProvider instance;
    return instance;
}

bool SovereignMeshProvider::InitializeDiscovery(uint16_t port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;

    m_discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_discoverySocket == INVALID_SOCKET)
        return false;

    BOOL broadcast = TRUE;
    setsockopt(m_discoverySocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_discoverySocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        return false;
    }

    // Initialize local identity (NodeID)
    m_localNodeId = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

    // Set Genesis Hash (Hardcoded Root of Trust)
    // In production, this matches the SHA256 of the initial aperture block
    memset(m_expectedMeshRoot, 0xEE, 32);

    std::cout << "[SovereignMesh] Discovery Layer initialized on port " << port << std::endl;
    return true;
}

void SovereignMeshProvider::ProcessDiscoveryLoop()
{
    DiscoveryPacket packet;
    sockaddr_in from;
    int fromLen = sizeof(from);

    while (true)
    {
        int bytes = recvfrom(m_discoverySocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &fromLen);
        if (bytes == sizeof(DiscoveryPacket))
        {
            if (memcmp(packet.header, "RAWRXD_DISCOVERY", 16) == 0)
            {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &from.sin_addr, ipStr, INET_ADDRSTRLEN);
                std::cout << "[SovereignMesh] Peer Discovered: " << ipStr << " NodeID: " << packet.node_id << std::endl;

                // Logic to transition from Discovery to Handshake (TCP)
                // InitiateHandshake(ipStr, 9006);
            }
        }
        Sleep(100);
    }
}

bool SovereignMeshProvider::VerifyMeshRoot(const uint8_t* incomingRootHash)
{
    // Zero-Trust Check: Block poisoning at the gateway
    if (memcmp(incomingRootHash, m_expectedMeshRoot, 32) != 0)
    {
        std::cerr << "[SovereignMesh] CRITICAL: Mesh Poisoning Detected! Root Hash Mismatch." << std::endl;
        return false;
    }
    return true;
}

}  // namespace RawrXD::Networking
