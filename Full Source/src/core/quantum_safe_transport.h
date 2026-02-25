// ============================================================================
// quantum_safe_transport.h — Phase 22A: Quantum-Safe Encryption for Shard
//                            Transmission (Post-Quantum TLS)
// ============================================================================
// Implements post-quantum cryptographic primitives for securing shard data
// transmitted between swarm nodes. Replaces the plaintext ShardTransfer
// (SwarmOpcode::ShardTransfer) path with CRYSTALS-Kyber KEM + AES-256-GCM
// hybrid encryption, ensuring forward secrecy against quantum adversaries.
//
// Architecture:
//   1. PQKeyEncapsulation — CRYSTALS-Kyber-1024 key encapsulation mechanism
//   2. HybridCipher — AES-256-GCM authenticated encryption with PQ KEM
//   3. QuantumSafeSession — Per-node ephemeral session with PQ key exchange
//   4. SecureShardTransport — Drop-in replacement for plaintext shard xfer
//   5. KeyRotationManager — Automatic PQ key rotation on configurable cadence
//
// Integrations:
//   - SwarmCoordinator (src/core/swarm_coordinator.h) — secure node sessions
//   - SwarmProtocol (src/core/swarm_protocol.h) — encrypted ShardTransfer
//   - SwarmDecisionBridge (src/core/swarm_decision_bridge.h) — secure tasks
//   - UnifiedHotpatchManager — encrypted hotpatch broadcast
//
// Crypto Primitives:
//   - CRYSTALS-Kyber-1024 (NIST PQC winner, ML-KEM)
//   - AES-256-GCM (symmetric authenticated encryption)
//   - BLAKE2b-256 (key derivation / session binding)
//   - X25519 (classical ECDH hybrid, defense-in-depth)
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded, per-node session state.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <array>
#include <unordered_map>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <bcrypt.h>

// Forward declarations
class SwarmCoordinator;

// ============================================================================
// Post-Quantum Algorithm Selection
// ============================================================================
enum class PQAlgorithm : uint8_t {
    Kyber512            = 0,    // NIST Level 1 (128-bit classical security)
    Kyber768            = 1,    // NIST Level 3 (192-bit classical security)
    Kyber1024           = 2,    // NIST Level 5 (256-bit classical security) [DEFAULT]
    HybridX25519Kyber   = 3,    // X25519 + Kyber768 double encapsulation
};

// ============================================================================
// Symmetric Cipher Mode (post-KEM session encryption)
// ============================================================================
enum class SymmetricMode : uint8_t {
    AES256_GCM          = 0,    // AES-256-GCM (default, hardware-accelerated via AES-NI)
    ChaCha20_Poly1305   = 1,    // ChaCha20-Poly1305 (software fallback)
};

// ============================================================================
// Key Rotation Policy
// ============================================================================
enum class KeyRotationPolicy : uint8_t {
    Never               = 0,    // No automatic rotation (manual only)
    PerSession          = 1,    // New keys every session
    Hourly              = 2,    // Rotate every hour
    PerGigabyte         = 3,    // Rotate after 1GB of encrypted data
    Adaptive            = 4,    // Based on threat model / traffic volume [DEFAULT]
};

// ============================================================================
// Transport Security Result (structured, no exceptions)
// ============================================================================
struct TransportSecurityResult {
    bool        success;
    int32_t     errorCode;
    const char* detail;

    static TransportSecurityResult ok(const char* msg = "OK") {
        return { true, 0, msg };
    }
    static TransportSecurityResult error(int32_t code, const char* msg) {
        return { false, code, msg };
    }
};

// ============================================================================
// CRYSTALS-Kyber Key Sizes (FIPS 203 / ML-KEM)
// ============================================================================
// Kyber-1024 (ML-KEM-1024):
//   Public key:   1568 bytes
//   Secret key:   3168 bytes
//   Ciphertext:   1568 bytes
//   Shared secret: 32 bytes

namespace PQCrypto {

    constexpr uint32_t KYBER512_PK_BYTES       = 800;
    constexpr uint32_t KYBER512_SK_BYTES       = 1632;
    constexpr uint32_t KYBER512_CT_BYTES       = 768;
    constexpr uint32_t KYBER512_SS_BYTES       = 32;

    constexpr uint32_t KYBER768_PK_BYTES       = 1184;
    constexpr uint32_t KYBER768_SK_BYTES       = 2400;
    constexpr uint32_t KYBER768_CT_BYTES       = 1088;
    constexpr uint32_t KYBER768_SS_BYTES       = 32;

    constexpr uint32_t KYBER1024_PK_BYTES      = 1568;
    constexpr uint32_t KYBER1024_SK_BYTES      = 3168;
    constexpr uint32_t KYBER1024_CT_BYTES      = 1568;
    constexpr uint32_t KYBER1024_SS_BYTES      = 32;

    constexpr uint32_t X25519_PK_BYTES         = 32;
    constexpr uint32_t X25519_SK_BYTES         = 32;
    constexpr uint32_t X25519_SS_BYTES         = 32;

    constexpr uint32_t AES256_KEY_BYTES        = 32;
    constexpr uint32_t AES256_GCM_NONCE_BYTES  = 12;
    constexpr uint32_t AES256_GCM_TAG_BYTES    = 16;

    constexpr uint32_t BLAKE2B_256_BYTES       = 32;

    // Max ciphertext overhead for Kyber-1024 + GCM tag + nonce
    constexpr uint32_t MAX_PQ_OVERHEAD         = KYBER1024_CT_BYTES + AES256_GCM_TAG_BYTES
                                                  + AES256_GCM_NONCE_BYTES + 64; // padding

    // Session ID length
    constexpr uint32_t SESSION_ID_BYTES        = 16;

} // namespace PQCrypto

// ============================================================================
// Kyber Key Pair
// ============================================================================
struct KyberKeyPair {
    std::vector<uint8_t> publicKey;
    std::vector<uint8_t> secretKey;
    PQAlgorithm          algorithm;
    uint64_t             generatedAtMs;
    uint64_t             expiresAtMs;       // 0 = no expiration
    bool                 valid;

    KyberKeyPair()
        : algorithm(PQAlgorithm::Kyber1024)
        , generatedAtMs(0), expiresAtMs(0), valid(false)
    {}
};

// ============================================================================
// X25519 Key Pair (classical hybrid component)
// ============================================================================
struct X25519KeyPair {
    std::array<uint8_t, 32> publicKey;
    std::array<uint8_t, 32> secretKey;
    uint64_t                generatedAtMs;
    bool                    valid;

    X25519KeyPair() : publicKey{}, secretKey{}, generatedAtMs(0), valid(false) {}
};

// ============================================================================
// Session Key Material (derived from KEM + optional X25519)
// ============================================================================
struct SessionKeyMaterial {
    std::array<uint8_t, 32> encryptionKey;      // AES-256 key
    std::array<uint8_t, 32> authKey;            // Authentication sub-key
    std::array<uint8_t, 16> sessionId;          // Unique session identifier
    uint64_t                nonceCounter;       // Monotonic nonce counter (GCM)
    uint64_t                bytesEncrypted;     // Total bytes encrypted with this key
    uint64_t                bytesDecrypted;     // Total bytes decrypted
    uint64_t                establishedAtMs;    // When session was established
    uint64_t                lastRotatedAtMs;    // When keys were last rotated
    bool                    valid;

    SessionKeyMaterial() : encryptionKey{}, authKey{}, sessionId{}
        , nonceCounter(0), bytesEncrypted(0), bytesDecrypted(0)
        , establishedAtMs(0), lastRotatedAtMs(0), valid(false) {}
};

// ============================================================================
// Quantum-Safe Session — Per-node encrypted communication channel
// ============================================================================
struct QuantumSafeSession {
    uint32_t            nodeSlot;               // Remote node slot index
    uint8_t             remoteNodeId[16];       // Remote node's 128-bit ID
    KyberKeyPair        localKyberKeys;         // Our Kyber key pair
    X25519KeyPair       localX25519Keys;        // Our X25519 key pair (hybrid)
    std::vector<uint8_t> remoteKyberPK;         // Remote node's Kyber public key
    std::array<uint8_t, 32> remoteX25519PK;     // Remote node's X25519 public key
    SessionKeyMaterial  sessionKeys;            // Derived session material
    PQAlgorithm         algorithm;
    SymmetricMode       symmetricMode;
    KeyRotationPolicy   rotationPolicy;
    uint64_t            rotationThresholdBytes; // Auto-rotate after N bytes
    uint64_t            rotationIntervalMs;     // Auto-rotate after N ms
    uint32_t            keyRotationCount;       // Number of rotations in session
    bool                established;
    bool                peerVerified;

    QuantumSafeSession()
        : nodeSlot(0xFFFFFFFF), remoteNodeId{}, remoteX25519PK{}
        , algorithm(PQAlgorithm::Kyber1024)
        , symmetricMode(SymmetricMode::AES256_GCM)
        , rotationPolicy(KeyRotationPolicy::Adaptive)
        , rotationThresholdBytes(1ULL << 30)    // 1 GB
        , rotationIntervalMs(3600000)           // 1 hour
        , keyRotationCount(0)
        , established(false), peerVerified(false)
    {}
};

// ============================================================================
// Encrypted Shard Envelope — wraps encrypted shard data for transmission
// ============================================================================
#pragma pack(push, 1)
struct EncryptedShardHeader {
    uint32_t    magic;              // 0x51534543 ('QSEC')
    uint8_t     version;            // Protocol version (1)
    uint8_t     algorithm;          // PQAlgorithm enum
    uint8_t     symmetricMode;      // SymmetricMode enum
    uint8_t     flags;              // Bit 0: has KEM ciphertext, Bit 1: hybrid mode
    uint8_t     sessionId[16];      // Session identifier
    uint8_t     nonce[12];          // AES-256-GCM nonce
    uint32_t    plaintextLen;       // Original plaintext length
    uint32_t    ciphertextLen;      // Encrypted data length (includes GCM tag)
    uint16_t    kemCiphertextLen;   // Kyber ciphertext length (0 = reuse session key)
    uint16_t    pad0;
    uint64_t    sequenceNum;        // Monotonic sequence for replay protection
    uint8_t     tag[16];            // AES-256-GCM authentication tag
    // Followed by: [kemCiphertext] + [ciphertext]
};
#pragma pack(pop)

static_assert(sizeof(EncryptedShardHeader) == 72, "EncryptedShardHeader must be 72 bytes");

constexpr uint32_t QSEC_MAGIC = 0x51534543; // 'QSEC'

// ============================================================================
// Transport Statistics
// ============================================================================
struct TransportSecurityStats {
    std::atomic<uint64_t> sessionsEstablished{0};
    std::atomic<uint64_t> sessionsFailed{0};
    std::atomic<uint64_t> keyRotations{0};
    std::atomic<uint64_t> shardsEncrypted{0};
    std::atomic<uint64_t> shardsDecrypted{0};
    std::atomic<uint64_t> bytesEncryptedTotal{0};
    std::atomic<uint64_t> bytesDecryptedTotal{0};
    std::atomic<uint64_t> authFailures{0};
    std::atomic<uint64_t> replayAttacksBlocked{0};
    std::atomic<uint64_t> kemOperations{0};
    std::atomic<uint64_t> hybridHandshakes{0};
    std::atomic<uint64_t> avgEncryptLatencyUs{0};
    std::atomic<uint64_t> avgDecryptLatencyUs{0};
};

// ============================================================================
// Callbacks (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*PQSessionCallback)(uint32_t nodeSlot, bool established, void* userData);
typedef void (*PQKeyRotationCallback)(uint32_t nodeSlot, uint32_t rotationCount, void* userData);

// ============================================================================
// QuantumSafeTransport — Main Class
// ============================================================================
class QuantumSafeTransport {
public:
    static QuantumSafeTransport& instance();

    // ---- Lifecycle ----
    TransportSecurityResult initialize(PQAlgorithm algo = PQAlgorithm::Kyber1024,
                                        SymmetricMode sym = SymmetricMode::AES256_GCM);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_relaxed); }

    // ---- Key Generation ----

    // Generate a fresh Kyber key pair for this node.
    TransportSecurityResult generateKyberKeyPair(KyberKeyPair& outKP,
                                                  PQAlgorithm algo = PQAlgorithm::Kyber1024);

    // Generate a fresh X25519 key pair for hybrid mode.
    TransportSecurityResult generateX25519KeyPair(X25519KeyPair& outKP);

    // Get this node's current Kyber public key (for advertisement to peers).
    const std::vector<uint8_t>& getLocalPublicKey() const;

    // ---- Session Establishment (Handshake) ----

    // Initiator side: create encrypted session with a remote node.
    // Sends KEM encapsulation using the remote node's public key.
    TransportSecurityResult initiateSession(uint32_t nodeSlot,
                                             const uint8_t* remoteKyberPK,
                                             uint32_t remotePKLen,
                                             const uint8_t* remoteX25519PK = nullptr);

    // Responder side: accept session from remote initiator.
    // Decapsulates the KEM ciphertext to derive the shared secret.
    TransportSecurityResult acceptSession(uint32_t nodeSlot,
                                           const uint8_t* kemCiphertext,
                                           uint32_t kemCTLen,
                                           const uint8_t* x25519Ephemeral = nullptr);

    // Check if a session is established with a node.
    bool hasSession(uint32_t nodeSlot) const;

    // Destroy a session (on node disconnect or key compromise).
    void destroySession(uint32_t nodeSlot);

    // ---- Encrypt / Decrypt Shard Data ----

    // Encrypt a plaintext shard for transmission to nodeSlot.
    // Returns: encrypted envelope (header + ciphertext) in outBuffer.
    TransportSecurityResult encryptShard(uint32_t nodeSlot,
                                          const void* plaintext, uint32_t plaintextLen,
                                          std::vector<uint8_t>& outBuffer);

    // Decrypt an incoming encrypted shard from nodeSlot.
    // Returns: decrypted plaintext in outPlaintext.
    TransportSecurityResult decryptShard(uint32_t nodeSlot,
                                          const void* encryptedEnvelope, uint32_t envelopeLen,
                                          std::vector<uint8_t>& outPlaintext);

    // ---- Key Rotation ----

    // Manually trigger key rotation for a specific session.
    TransportSecurityResult rotateSessionKeys(uint32_t nodeSlot);

    // Rotate all sessions simultaneously (e.g., on security event).
    TransportSecurityResult rotateAllSessions();

    // Set rotation policy for new sessions.
    void setRotationPolicy(KeyRotationPolicy policy);
    KeyRotationPolicy getRotationPolicy() const { return m_defaultRotationPolicy; }

    // ---- Configuration ----
    void setDefaultAlgorithm(PQAlgorithm algo) { m_defaultAlgorithm = algo; }
    PQAlgorithm getDefaultAlgorithm() const { return m_defaultAlgorithm; }
    void setDefaultSymmetricMode(SymmetricMode mode) { m_defaultSymmetric = mode; }
    void setSwarmCoordinator(SwarmCoordinator* coordinator);

    // ---- Statistics ----
    const TransportSecurityStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- Callbacks ----
    void setSessionCallback(PQSessionCallback cb, void* userData);
    void setKeyRotationCallback(PQKeyRotationCallback cb, void* userData);

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string sessionsToJson() const;
    std::string statsToJson() const;

private:
    QuantumSafeTransport();
    ~QuantumSafeTransport();
    QuantumSafeTransport(const QuantumSafeTransport&) = delete;
    QuantumSafeTransport& operator=(const QuantumSafeTransport&) = delete;

    // Internal: Kyber KEM operations (software reference implementation)
    TransportSecurityResult kyberKeyGen(std::vector<uint8_t>& pk, std::vector<uint8_t>& sk,
                                         PQAlgorithm algo);
    TransportSecurityResult kyberEncaps(const std::vector<uint8_t>& pk,
                                         std::vector<uint8_t>& ciphertext,
                                         std::array<uint8_t, 32>& sharedSecret,
                                         PQAlgorithm algo);
    TransportSecurityResult kyberDecaps(const std::vector<uint8_t>& sk,
                                         const std::vector<uint8_t>& ciphertext,
                                         std::array<uint8_t, 32>& sharedSecret,
                                         PQAlgorithm algo);

    // Internal: X25519 ECDH (via Windows BCrypt)
    TransportSecurityResult x25519KeyGen(X25519KeyPair& kp);
    TransportSecurityResult x25519Derive(const std::array<uint8_t, 32>& ourSK,
                                          const std::array<uint8_t, 32>& theirPK,
                                          std::array<uint8_t, 32>& sharedSecret);

    // Internal: AES-256-GCM authenticated encryption
    TransportSecurityResult aesGcmEncrypt(const std::array<uint8_t, 32>& key,
                                           const uint8_t* nonce, uint32_t nonceLen,
                                           const void* plaintext, uint32_t plaintextLen,
                                           const void* aad, uint32_t aadLen,
                                           void* ciphertext, uint8_t* tag);
    TransportSecurityResult aesGcmDecrypt(const std::array<uint8_t, 32>& key,
                                           const uint8_t* nonce, uint32_t nonceLen,
                                           const void* ciphertext, uint32_t ciphertextLen,
                                           const void* aad, uint32_t aadLen,
                                           const uint8_t* tag,
                                           void* plaintext);

    // Internal: Key derivation (BLAKE2b-based HKDF-like)
    TransportSecurityResult deriveSessionKeys(const std::array<uint8_t, 32>& kyberSS,
                                               const std::array<uint8_t, 32>* x25519SS,
                                               const uint8_t* sessionContext,
                                               uint32_t contextLen,
                                               SessionKeyMaterial& outKeys);

    // Internal: Generate nonce from counter + session ID
    void generateNonce(const QuantumSafeSession& session, uint8_t* nonce12);

    // Internal: CSPRNG (via BCryptGenRandom)
    TransportSecurityResult secureRandom(void* buffer, uint32_t len);

    // Internal: Check if rotation is needed for a session
    bool needsKeyRotation(const QuantumSafeSession& session) const;

    // Internal: Key rotation monitor thread
    static DWORD WINAPI keyRotationThread(LPVOID param);
    void monitorKeyRotation();

    // Internal: BLAKE2b-256 hash
    TransportSecurityResult blake2b256(const void* data, uint32_t dataLen,
                                        std::array<uint8_t, 32>& outHash);

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    mutable std::mutex                                  m_mutex;
    std::atomic<bool>                                   m_initialized;
    std::atomic<bool>                                   m_shutdownRequested;

    // This node's key pairs
    KyberKeyPair                                        m_localKyberKeys;
    X25519KeyPair                                       m_localX25519Keys;

    // Per-node sessions
    std::unordered_map<uint32_t, QuantumSafeSession>    m_sessions;

    // Replay protection: per-session highest seen sequence number
    std::unordered_map<uint32_t, uint64_t>              m_replayWindows;

    // Configuration
    PQAlgorithm                                         m_defaultAlgorithm;
    SymmetricMode                                       m_defaultSymmetric;
    KeyRotationPolicy                                   m_defaultRotationPolicy;

    // Engine pointers
    SwarmCoordinator*                                   m_coordinator;

    // BCrypt handles (Windows CNG)
    BCRYPT_ALG_HANDLE                                   m_hAesGcm;
    BCRYPT_ALG_HANDLE                                   m_hRng;

    // Statistics
    TransportSecurityStats                              m_stats;

    // Callbacks
    PQSessionCallback                                   m_sessionCb;
    void*                                               m_sessionUserData;
    PQKeyRotationCallback                               m_rotationCb;
    void*                                               m_rotationUserData;

    // Key rotation monitor
    HANDLE                                              m_hRotationThread;
};
