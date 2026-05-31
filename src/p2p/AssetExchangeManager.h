// ============================================================================
// AssetExchangeManager.h — Orchestrator for the Sovereign Swarm Trade Loop
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include "P2PSyncController.h"
#include "ZeroKnowledgeValidator.h"
#include "EncryptedTransferSession.h"

namespace RawrXD {
namespace P2P {

struct EvolutionaryTrait {
    std::string assetId;        // e.g., "tokenizer_avx512_v1"
    uint32_t hardwareReq;       // e.g., AVX512_BIT | VULKAN_BIT
    uint64_t claimedCycles;     // Performance metric
    std::vector<uint8_t> proof; // Initial ZK proof summary
};

/**
 * @brief The AssetExchangeManager unifies Discovery, ZK-Verification, and 
 * Encrypted Transfer into a single "Sovereign Trade" workflow.
 */
class AssetExchangeManager {
public:
    static AssetExchangeManager& Instance();

    // ---- Discovery Support ----
    void OnPeerDiscovered(const PeerNode& peer);
    void OnAdvertisementReceived(const std::string& peerId, const EvolutionaryTrait& trait);

    // ---- The Trade Loop ----
    /**
     * @brief Initiates a "Sovereign Trade" for a specific trait.
     * 1. Issues ZK Challenge (Sovereign Seed)
     * 2. Verifies Proof
     * 3. Establishes Encrypted Session
     * 4. Downloads and Seals Asset to local hardware
     */
    void RequestEvolutionaryTrade(const std::string& peerId, const std::string& assetId);

    // ---- State Retrieval ----
    std::vector<EvolutionaryTrait> GetAvailableTraits(const std::string& peerId);

private:
    AssetExchangeManager() = default;
    
    ZeroKnowledgeValidator m_validator;
    std::map<std::string, std::vector<EvolutionaryTrait>> m_peerTraits;
    std::mutex m_traitsMutex;

    // Internal workflow steps
    bool VerifyPeerCapability(const std::string& peerId, const EvolutionaryTrait& trait);
    void ExecuteSecureDownload(const std::string& peerId, const std::string& assetId);
};

} // namespace P2P
} // namespace RawrXD
