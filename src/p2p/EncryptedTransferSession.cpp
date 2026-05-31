// ============================================================================
// EncryptedTransferSession.cpp — Secure Handshake and Message Protocol
// ============================================================================
#include "EncryptedTransferSession.h"
#include <iostream>

namespace RawrXD {
namespace P2P {

EncryptedTransferSession::EncryptedTransferSession(SOCKET s, const std::string& peerId) 
    : m_socket(s), m_peerId(peerId) {
    Crypto::CryptoHelpers::Initialize();
}

EncryptedTransferSession::~EncryptedTransferSession() {
    if (m_hLocalPrivKey) BCryptDestroyKey(m_hLocalPrivKey);
    if (m_socket != INVALID_SOCKET) closesocket(m_socket);
}

bool EncryptedTransferSession::PerformHandshake() {
    // 1. Generate local X25519 KeyPair
    auto kp = Crypto::CryptoHelpers::GenerateX25519KeyPair();
    if (!kp) return false;
    
    m_hLocalPrivKey = kp->hKey;
    kp->hKey = nullptr; // Take ownership
    m_localPublicKey = kp->publicKey;

    // 2. Send Local Public Key (SOVEREIGN_HELLO)
    if (!SendSovereignMessage(MessageType::SOVEREIGN_HELLO, m_localPublicKey)) return false;

    // 3. Receive Peer Public Key
    MessageType type;
    if (!ReceiveSovereignMessage(type, m_peerPublicKey) || type != MessageType::SOVEREIGN_HELLO) return false;

    // 4. Derive Shared Secret
    auto rawSecret = Crypto::CryptoHelpers::DeriveSharedSecret(m_hLocalPrivKey, m_peerPublicKey);
    if (rawSecret.empty()) return false;

    // 5. Hash secret to derive 256-bit session key
    m_sessionKey = Crypto::CryptoHelpers::HashSHA256(rawSecret);
    
    return !m_sessionKey.empty();
}

bool EncryptedTransferSession::SendSovereignMessage(MessageType type, const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    
    MessageHeader header;
    header.type = type;
    header.payloadLength = (uint32_t)payload.size();

    std::vector<uint8_t> encryptedPayload;
    if (!m_sessionKey.empty() && type != MessageType::SOVEREIGN_HELLO) {
        // Encrypt with AES-256-GCM
        auto iv = Crypto::CryptoHelpers::GenerateRandomBytes(12);
        memcpy(header.iv, iv.data(), 12);
        
        std::vector<uint8_t> tag;
        encryptedPayload = Crypto::CryptoHelpers::EncryptAES_GCM(payload, m_sessionKey, iv, tag);
        memcpy(header.tag, tag.data(), 16);
        header.encryptedLength = (uint32_t)encryptedPayload.size();
    } else {
        // Unencrypted (only for Handshake Hello)
        encryptedPayload = payload;
        header.encryptedLength = (uint32_t)payload.size();
        memset(header.iv, 0, 12);
        memset(header.tag, 0, 16);
    }

    // Send Header
    if (send(m_socket, (const char*)&header, sizeof(header), 0) == SOCKET_ERROR) return false;
    
    // Send Payload
    if (send(m_socket, (const char*)encryptedPayload.data(), (int)encryptedPayload.size(), 0) == SOCKET_ERROR) return false;

    m_sequenceNumber++;
    return true;
}

bool EncryptedTransferSession::ReceiveSovereignMessage(MessageType& outType, std::vector<uint8_t>& outPayload) {
    MessageHeader header;
    int received = recv(m_socket, (char*)&header, sizeof(header), MSG_WAITALL);
    if (received != sizeof(header) || header.magic != 0x5258444E) return false;

    outType = header.type;
    std::vector<uint8_t> rawPayload(header.encryptedLength);
    received = recv(m_socket, (char*)rawPayload.data(), (int)rawPayload.size(), MSG_WAITALL);
    if (received != (int)rawPayload.size()) return false;

    if (!m_sessionKey.empty() && outType != MessageType::SOVEREIGN_HELLO) {
        // Decrypt AES-GCM
        std::vector<uint8_t> iv(header.iv, header.iv + 12);
        std::vector<uint8_t> tag(header.tag, header.tag + 16);
        outPayload = Crypto::CryptoHelpers::DecryptAES_GCM(rawPayload, m_sessionKey, iv, tag);
        return !outPayload.empty();
    } else {
        outPayload = rawPayload;
        return true;
    }
}

} // namespace P2P
} // namespace RawrXD
