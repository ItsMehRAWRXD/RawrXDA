// ============================================================================
// EncryptedTransferSession.h — Secure P2P TCP framing and session state
// ============================================================================
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <cstdint>
#include "CryptoHelpers.h"

namespace RawrXD {
namespace P2P {

enum class MessageType : uint32_t {
    SOVEREIGN_HELLO = 1,      // Handshake initiation
    ASSET_ADVERTISEMENT = 2,  // Advertise a new kernel/shard
    ASSET_REQUEST = 3,        // Request an advertised asset
    ASSET_RESPONSE = 4,       // The encrypted binary blob
    ASSET_VALIDATION = 5      // Acceptance/rejection after ZK proof
};

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic = 0x5258444E; // 'RXDN' (RawrXD Node)
    MessageType type;           // Message type
    uint32_t payloadLength;      // Unencrypted length
    uint32_t encryptedLength;    // Length of AES-GCM payload
    uint8_t iv[12];              // Standard GCM 96-bit IV
    uint8_t tag[16];             // GCM 128-bit authentication tag
};
#pragma pack(pop)

class EncryptedTransferSession {
public:
    EncryptedTransferSession(SOCKET s, const std::string& peerId);
    ~EncryptedTransferSession();

    bool PerformHandshake(); // Exchange X25519 keys
    bool SendSovereignMessage(MessageType type, const std::vector<uint8_t>& payload);
    bool ReceiveSovereignMessage(MessageType& outType, std::vector<uint8_t>& outPayload);

    // ECDH + AES-GCM State
    std::vector<uint8_t> m_localPublicKey;
    std::vector<uint8_t> m_peerPublicKey;
    BCRYPT_KEY_HANDLE m_hLocalPrivKey = nullptr;

private:
    SOCKET m_socket;
    std::string m_peerId;
    std::vector<uint8_t> m_sessionKey;
    std::atomic<uint64_t> m_sequenceNumber{0};
    std::mutex m_sendMutex;
    
    // Hardware fingerprint used for payload-layer encryption (Asset binding)
    std::vector<uint8_t> m_localHardwareKey;
};

// ============================================================================
// PeerManager — Manages active secure TCP connections
// ============================================================================
class PeerManager {
public:
    static PeerManager& Instance();
    void AcceptConnection(SOCKET s);
    void ConnectToPeer(const std::string& ip, uint16_t port);
    
private:
    std::map<std::string, std::shared_ptr<EncryptedTransferSession>> m_activeSessions;
    std::mutex m_sessionsMutex;
};

} // namespace P2P
} // namespace RawrXD
