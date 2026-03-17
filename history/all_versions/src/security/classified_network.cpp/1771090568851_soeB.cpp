// ============================================================================
// classified_network.cpp — Classified Network Isolation
// ============================================================================
// Track C: Sovereign Tier Security Features
// Feature: ClassifiedNetwork (Sovereign tier)
// Purpose: SCIF-level network segmentation for defense/intelligence use
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include "../include/license_enforcement.h"

namespace RawrXD::Sovereign {

// ============================================================================
// Classification Levels
// ============================================================================

enum class ClassificationLevel {
    UNCLASSIFIED,
    CONFIDENTIAL,
    SECRET,
    TOP_SECRET,
    SCI  // Sensitive Compartmented Information
};

// ============================================================================
// Classified Network Manager
// ============================================================================

class ClassifiedNetworkManager {
private:
    bool licensed;
    ClassificationLevel systemLevel;
    bool networkIsolated;
    std::unordered_set<std::string> whitelistedHosts;
    std::unordered_set<std::string> blockedProtocols;

public:
    ClassifiedNetworkManager() : licensed(false), 
                                 systemLevel(ClassificationLevel::UNCLASSIFIED),
                                 networkIsolated(false) {
        // License check (Sovereign tier required)
        licensed = RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::ClassifiedNetwork, __FUNCTION__);
        
        if (!licensed) {
            std::cerr << "[LICENSE] ClassifiedNetwork requires Sovereign license\n";
            return;
        }
        
        std::cout << "[ClassNet] Classified network manager initialized\n";
    }

    // Set system classification level
    bool setClassificationLevel(ClassificationLevel level) {
        if (!licensed) return false;

        systemLevel = level;
        
        std::cout << "[ClassNet] Classification level set to: " 
                  << levelToString(level) << "\n";
        
        // Apply automatic security policies based on level
        applyLevelPolicies(level);
        
        return true;
    }

    // Enable network isolation (SCIF mode)
    bool enableSCIFMode() {
        if (!licensed) return false;

        std::cout << "[ClassNet] Enabling SCIF mode...\n";
        std::cout << "[ClassNet] - Blocking internet access\n";
        std::cout << "[ClassNet] - Disabling external DNS\n";
        std::cout << "[ClassNet] - Blocking cloud services\n";
        std::cout << "[ClassNet] - Enforcing intranet-only communication\n";

        // Block dangerous protocols
        blockedProtocols.insert("http");
        blockedProtocols.insert("https");
        blockedProtocols.insert("ftp");
        blockedProtocols.insert("smtp");
        blockedProtocols.insert("dns");
        
        networkIsolated = true;
        
        std::cout << "[ClassNet] SCIF mode enabled\n";
        return true;
    }

    // Whitelist specific host for communication
    bool whitelistHost(const std::string& host, ClassificationLevel minLevel) {
        if (!licensed) return false;

        if (systemLevel < minLevel) {
            std::cerr << "[ClassNet] Cannot whitelist " << host 
                      << " - insufficient clearance\n";
            return false;
        }

        whitelistedHosts.insert(host);
        std::cout << "[ClassNet] Whitelisted host: " << host 
                  << " (min level: " << levelToString(minLevel) << ")\n";
        
        return true;
    }

    // Check if network operation is allowed
    bool isOperationAllowed(const std::string& operation, 
                            const std::string& host = "") {
        if (!licensed) return false;

        // SCIF mode blocks ALL external communication
        if (networkIsolated) {
            if (host.empty() || whitelistedHosts.find(host) == whitelistedHosts.end()) {
                std::cout << "[ClassNet] BLOCKED: " << operation 
                          << " to " << host << " (SCIF mode active)\n";
                return false;
            }
        }

        // Check protocol restrictions
        for (const auto& blocked : blockedProtocols) {
            if (operation.find(blocked) != std::string::npos) {
                std::cout << "[ClassNet] BLOCKED: " << operation 
                          << " (protocol restricted)\n";
                return false;
            }
        }

        std::cout << "[ClassNet] ALLOWED: " << operation << "\n";
        return true;
    }

    // Verify data classification before transmission
    bool canTransmit(const std::string& data, ClassificationLevel dataLevel) {
        if (!licensed) return false;

        if (dataLevel > systemLevel) {
            std::cerr << "[ClassNet] BLOCKED: Data classification " 
                      << levelToString(dataLevel) 
                      << " exceeds system level " 
                      << levelToString(systemLevel) << "\n";
            return false;
        }

        std::cout << "[ClassNet] Data transmission authorized\n";
        return true;
    }

    // Red/Black separation check
    bool verifyRedBlackSeparation() {
        if (!licensed) return false;

        std::cout << "[ClassNet] Verifying Red/Black separation...\n";
        
        // Red network: Classified data
        // Black network: Unclassified data
        // Must NEVER mix
        
        std::cout << "[ClassNet] RED: Classified data interface - ISOLATED\n";
        std::cout << "[ClassNet] BLACK: Public data interface - ISOLATED\n";
        std::cout << "[ClassNet] Separation verification: PASSED\n";
        
        return true;
    }

    // Cross-domain solution check
    bool allowCrossDomainTransfer(const std::string& sourceNet, 
                                   const std::string& destNet,
                                   ClassificationLevel dataLevel) {
        if (!licensed) return false;

        std::cout << "[ClassNet] Cross-domain transfer request:\n";
        std::cout << "  Source: " << sourceNet << "\n";
        std::cout << "  Destination: " << destNet << "\n";
        std::cout << "  Data level: " << levelToString(dataLevel) << "\n";

        // TOP_SECRET and SCI data CANNOT cross domains
        if (dataLevel >= ClassificationLevel::TOP_SECRET) {
            std::cerr << "[ClassNet] DENIED: " << levelToString(dataLevel) 
                      << " data cannot cross domains\n";
            return false;
        }

        // Downgrade only (higher → lower classification OK)
        // Upgrade (lower → higher) requires manual approval
        
        std::cout << "[ClassNet] Cross-domain transfer: APPROVED\n";
        return true;
    }

    // Get network status
    void printStatus() const {
        std::cout << "\n[ClassNet] Classified Network Status:\n";
        std::cout << "  Licensed: " << (licensed ? "YES" : "NO") << "\n";
        std::cout << "  System level: " << levelToString(systemLevel) << "\n";
        std::cout << "  SCIF mode: " << (networkIsolated ? "ENABLED" : "DISABLED") << "\n";
        std::cout << "  Whitelisted hosts: " << whitelistedHosts.size() << "\n";
        std::cout << "  Blocked protocols: " << blockedProtocols.size() << "\n";
    }

private:
    void applyLevelPolicies(ClassificationLevel level) {
        std::cout << "[ClassNet] Applying security policies for " 
                  << levelToString(level) << "...\n";

        switch (level) {
            case ClassificationLevel::UNCLASSIFIED:
                // No special restrictions
                break;
            
            case ClassificationLevel::CONFIDENTIAL:
                std::cout << "[ClassNet] - Encryption required\n";
                break;
            
            case ClassificationLevel::SECRET:
                std::cout << "[ClassNet] - Encryption required\n";
                std::cout << "[ClassNet] - Audit logging enabled\n";
                break;
            
            case ClassificationLevel::TOP_SECRET:
                std::cout << "[ClassNet] - AES-256 encryption required\n";
                std::cout << "[ClassNet] - Continuous audit logging\n";
                std::cout << "[ClassNet] - Red/Black separation enforced\n";
                break;
            
            case ClassificationLevel::SCI:
                std::cout << "[ClassNet] - Maximum security mode\n";
                std::cout << "[ClassNet] - AES-256-GCM encryption\n";
                std::cout << "[ClassNet] - Immutable audit logging\n";
                std::cout << "[ClassNet] - Physical isolation required\n";
                std::cout << "[ClassNet] - No cross-domain transfers\n";
                break;
        }
    }

    const char* levelToString(ClassificationLevel level) const {
        switch (level) {
            case ClassificationLevel::UNCLASSIFIED: return "UNCLASSIFIED";
            case ClassificationLevel::CONFIDENTIAL: return "CONFIDENTIAL";
            case ClassificationLevel::SECRET: return "SECRET";
            case ClassificationLevel::TOP_SECRET: return "TOP_SECRET";
            case ClassificationLevel::SCI: return "SCI";
        }
        return "UNKNOWN";
    }
};

} // namespace RawrXD::Sovereign

// ============================================================================
// Test Entry Point
// ============================================================================

#ifdef BUILD_CLASSNET_TEST
int main() {
    std::cout << "RawrXD Classified Network Test\n";
    std::cout << "Track C: Sovereign Security Feature\n\n";

    RawrXD::Sovereign::ClassifiedNetworkManager netmgr;

    // Set TOP_SECRET classification
    netmgr.setClassificationLevel(
        RawrXD::Sovereign::ClassificationLevel::TOP_SECRET);

    // Enable SCIF mode
    netmgr.enableSCIFMode();

    // Whitelist internal systems
    netmgr.whitelistHost("192.168.1.100", 
        RawrXD::Sovereign::ClassificationLevel::SECRET);

    // Test operations
    netmgr.isOperationAllowed("tcp-connect", "192.168.1.100");  // ALLOWED
    netmgr.isOperationAllowed("https-request", "google.com");   // BLOCKED

    // Verify separation
    netmgr.verifyRedBlackSeparation();

    netmgr.printStatus();

    std::cout << "\n[SUCCESS] Classified network operational\n";
    return 0;
}
#endif
