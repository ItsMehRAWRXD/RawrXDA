#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <winsock2.h>

namespace RawrXD::Networking {

struct DiscoveryPacket {
    char header[16]; // "RAWRXD_DISCOVERY"
    uint64_t node_id;
    uint8_t pubkey_hash[32];
    uint32_t protocol_version;
    uint64_t timestamp;
};

struct HandshakePacket {
    uint64_t node_id;
    uint8_t pubkey[64]; // Ed25519 placeholder or PQC fragment
    uint64_t nonce;
    uint64_t timestamp;
    uint8_t signature[64];
};

enum class NodeTrustLevel {
    None,
    Observer,
    Participant,
    Validator,
    Sovereign
};

class SovereignMeshProvider {
public:
    static SovereignMeshProvider& Instance();

    bool InitializeDiscovery(uint16_t port = 9005);
    void ProcessDiscoveryLoop();
    
    bool InitiateHandshake(const std::string& targetIp, uint16_t port);
    bool HandleIncomingHandshake(SOCKET s);

    // Bootstrap verification
    bool VerifyMeshRoot(const uint8_t* incomingRootHash);
    
private:
    SovereignMeshProvider() = default;
    SOCKET m_discoverySocket = INVALID_SOCKET;
    uint64_t m_localNodeId = 0;
    uint8_t m_expectedMeshRoot[32] = {0}; // Hash of Genesis Kernel Set
};

} // namespace RawrXD::Networking
