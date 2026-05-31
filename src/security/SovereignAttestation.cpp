#include "SovereignAttestation.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <windows.h>
#include <iphlpapi.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#pragma comment(lib, "iphlpapi.lib")

namespace RawrXD::Security {

class SovereignAttestation::Impl {
public:
    std::array<uint8_t, 32> prevHash{};
    uint64_t seq = 0;
    std::vector<AuditBlock> blocks;
    std::array<uint8_t, 32> vramHash{};
    EVP_PKEY* signKey = nullptr;
    std::string deviceId;

    Impl() { std::fill(prevHash.begin(), prevHash.end(), 0); }
    ~Impl() { if (signKey) EVP_PKEY_free(signKey); }

    void GenDeviceId() {
        char name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD sz = sizeof(name);
        GetComputerNameA(name, &sz);
        
        IP_ADAPTER_INFO adapter[16];
        DWORD len = sizeof(adapter);
        GetAdaptersInfo(adapter, &len);
        
        std::string combined = std::string(name) + std::string((char*)adapter[0].Address, 6);
        unsigned char h[32];
        SHA256((const unsigned char*)combined.data(), combined.size(), h);
        
        std::stringstream ss;
        for (int i = 0; i < 8; i++) ss << std::hex << std::setw(2) << std::setfill('0') << (int)h[i];
        deviceId = ss.str();
    }

    bool GenEphemeralKey() {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
        if (!ctx) return false;
        if (EVP_PKEY_keygen_init(ctx) <= 0) { EVP_PKEY_CTX_free(ctx); return false; }
        if (EVP_PKEY_keygen(ctx, &signKey) <= 0) { EVP_PKEY_CTX_free(ctx); return false; }
        EVP_PKEY_CTX_free(ctx);
        return true;
    }
};

SovereignAttestation::SovereignAttestation() : pImpl(std::make_unique<Impl>()) {}
SovereignAttestation::~SovereignAttestation() = default;

SovereignAttestation& SovereignAttestation::Instance() {
    static SovereignAttestation inst;
    return inst;
}

bool SovereignAttestation::Initialize(const std::string& hsmKeyPath) {
    pImpl->GenDeviceId();
    return pImpl->GenEphemeralKey();
}

void SovereignAttestation::RecordEvent(const std::string& type, const std::string& jsonPayload) {
    AuditBlock blk;
    blk.timestampUnixMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    blk.sequence = pImpl->seq++;
    blk.eventType = type;
    blk.payload = jsonPayload;
    blk.prevHash = pImpl->prevHash;
    
    std::string data = std::to_string(blk.timestampUnixMs) + type + jsonPayload;
    SHA256((const unsigned char*)data.data(), data.size(), blk.eventHash.data());
    
    pImpl->blocks.push_back(blk);
    if (pImpl->blocks.size() > 1000) pImpl->blocks.erase(pImpl->blocks.begin());
    
    // Merkle Root calculation (Simplified for Audit Chain)
    std::array<uint8_t, 32> currentRoot = blk.eventHash;
    for (int i = (int)pImpl->blocks.size() - 2; i >= 0; i--) {
        unsigned char combined[64];
        std::copy(pImpl->blocks[i].eventHash.begin(), pImpl->blocks[i].eventHash.end(), combined);
        std::copy(currentRoot.begin(), currentRoot.end(), combined + 32);
        SHA256(combined, 64, currentRoot.data());
    }
    blk.stateRoot = currentRoot;
    pImpl->prevHash = blk.eventHash;
}

void SovereignAttestation::UpdateVramIntegrity(const std::string& path, const void* base, size_t size) {
    if (!base || size == 0) return;
    
    // Sample-based hashing (1MB head/mid/tail)
    size_t sampleSize = 1024 * 1024;
    std::string composite;
    if (size <= sampleSize * 3) {
        composite = std::string((const char*)base, size);
    } else {
        composite.append((const char*)base, sampleSize);
        composite.append((const char*)base + (size / 2), sampleSize);
        composite.append((const char*)base + size - sampleSize, sampleSize);
    }
    composite += path;
    SHA256((const unsigned char*)composite.data(), composite.size(), pImpl->vramHash.data());
}

RuntimeProof SovereignAttestation::GenerateProof(AttestationLevel lvl) const {
    RuntimeProof p;
    p.issuedAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    p.level = lvl;
    p.deviceId = pImpl->deviceId;
    p.chain = pImpl->blocks;
    p.vramHash = pImpl->vramHash;
    
    // Simple JSON SBOM (CycloneDX-lite)
    nlohmann::json sbom;
    sbom["bomFormat"] = "CycloneDX";
    sbom["specVersion"] = "1.5";
    sbom["serialNumber"] = "urn:uuid:" + pImpl->deviceId + "-" + std::to_string(p.issuedAt);
    sbom["components"] = nlohmann::json::array({
        {{"type", "machine-learning-model"}, {"name", "InferenceWeights"}, 
         {"hashes", nlohmann::json::array({{{"alg", "SHA-256"}, {"content", 
             [](const auto& h) {
                std::stringstream ss;
                for (auto b : h) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                return ss.str();
             }(pImpl->vramHash)
         }})}}
        }
    });
    p.sbom = sbom.dump(2);
    
    // Ed25519 signature
    std::string dataToSign = p.deviceId + std::to_string(p.issuedAt) + p.sbom;
    size_t sigLen = 64;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pImpl->signKey);
    EVP_DigestSign(ctx, p.ed25519Sig.data(), &sigLen, (const unsigned char*)dataToSign.data(), dataToSign.size());
    EVP_MD_CTX_free(ctx);
    
    return p;
}

std::string SovereignAttestation::ExportToJson(const RuntimeProof& proof) const {
    nlohmann::json j;
    j["issuedAt"] = proof.issuedAt;
    j["deviceId"] = proof.deviceId;
    j["level"] = static_cast<int>(proof.level);
    j["sbom"] = nlohmann::json::parse(proof.sbom);
    j["auditChainLength"] = proof.chain.size();
    j["verified"] = VerifyChain();
    
    std::stringstream sigHex;
    for (auto b : proof.ed25519Sig) sigHex << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    j["signature"] = sigHex.str();
    
    return j.dump(2);
}

bool SovereignAttestation::VerifyChain() const {
    if (pImpl->blocks.empty()) return true;
    std::array<uint8_t, 32> expectedPrev{};
    std::fill(expectedPrev.begin(), expectedPrev.end(), 0);
    
    for (const auto& blk : pImpl->blocks) {
        if (blk.prevHash != expectedPrev) return false;
        expectedPrev = blk.eventHash;
    }
    return true;
}

std::string SovereignAttestation::GenerateComplianceReport() const {
    std::stringstream rpt;
    rpt << "RAWRXD SOVEREIGN ATTESTATION REPORT (NIST 800-218)\n";
    rpt << "================================================\n";
    rpt << "Device ID: " << pImpl->deviceId << "\n";
    rpt << "Chain Integrity: " << (VerifyChain() ? "VALID" : "TAMPERED") << "\n";
    rpt << "Last VRAM Hash: ";
    for (auto b : pImpl->vramHash) rpt << std::hex << (int)b;
    rpt << "\n\nEVENTS LOGGED (" << pImpl->blocks.size() << ")\n";
    rpt << "---------------------------\n";
    for (const auto& b : pImpl->blocks) {
        rpt << "[" << b.sequence << "] " << b.eventType << " " << b.payload.substr(0, 50) << "...\n";
    }
    return rpt.str();
}

} // namespace RawrXD::Security
