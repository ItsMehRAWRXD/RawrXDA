#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <memory>

namespace RawrXD::Security {

enum class AttestationLevel : uint8_t {
    Developer = 1,      // Build signature only
    Runtime = 2,        // + Loaded module hashes
    Hardware = 3,       // + VRAM/TPM measurements
    Full = 4            // + Continuous integrity monitoring
};

struct AuditBlock {
    uint64_t timestampUnixMs;
    uint64_t sequence;
    std::array<uint8_t, 32> eventHash;      // SHA-256
    std::array<uint8_t, 32> prevHash;       // Chained
    std::array<uint8_t, 32> stateRoot;      // Merkle Root
    std::string eventType;                  
    std::string payload;                    // JSON
};

struct RuntimeProof {
    uint64_t issuedAt;
    AttestationLevel level;
    std::string deviceId;
    std::string sbom;                       // CycloneDX Format
    std::vector<AuditBlock> chain;
    std::array<uint8_t, 64> ed25519Sig;     // Ephemeral/HSM Signature
    std::array<uint8_t, 32> vramHash;       // Model integrity
};

class SovereignAttestation {
public:
    static SovereignAttestation& Instance();
    
    bool Initialize(const std::string& hsmKeyPath = {});
    void RecordEvent(const std::string& type, const std::string& jsonPayload);
    void UpdateVramIntegrity(const std::string& path, const void* base, size_t size);
    
    RuntimeProof GenerateProof(AttestationLevel lvl = AttestationLevel::Runtime) const;
    std::string ExportToJson(const RuntimeProof& proof) const;
    bool VerifyChain() const;
    std::string GenerateComplianceReport() const; // NIST 800-218 formatted

private:
    SovereignAttestation();
    ~SovereignAttestation();
    
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace RawrXD::Security
