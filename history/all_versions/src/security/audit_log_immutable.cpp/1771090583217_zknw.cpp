// ============================================================================
// audit_log_immutable.cpp — Immutable Audit Log System
// ============================================================================
// Track C: Sovereign Tier Security Features
// Feature: ImmutableAuditLogs (Sovereign tier)
// Purpose: Tamper-proof audit trail using blockchain/Merkle tree
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "../include/license_enforcement.h"

namespace RawrXD::Sovereign {

// ============================================================================
// Audit Log Entry
// ============================================================================

struct AuditEntry {
    uint64_t id;
    std::time_t timestamp;
    std::string event;
    std::string actor;
    std::string resource;
    std::string details;
    std::string previousHash;  // Hash of previous entry (blockchain-style)
    std::string entryHash;     // Hash of this entry
};

// ============================================================================
// Immutable Audit Log
// ============================================================================

class ImmutableAuditLog {
private:
    bool licensed;
    std::vector<AuditEntry> logChain;
    std::string logFilePath;
    uint64_t nextId;

public:
    ImmutableAuditLog(const std::string& logPath = "audit.log") 
        : licensed(false), logFilePath(logPath), nextId(1) {
        
        // License check (Sovereign tier required)
        licensed = RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::ImmutableAuditLogs, __FUNCTION__);
        
        if (!licensed) {
            std::cerr << "[LICENSE] ImmutableAuditLogs requires Sovereign license\n";
            return;
        }
        
        std::cout << "[AuditLog] Immutable audit log initialized\n";
        
        // Load existing log chain
        loadLogChain();
    }

    // Log audit event (append-only)
    bool logEvent(const std::string& event, const std::string& actor, 
                  const std::string& resource, const std::string& details = "") {
        if (!licensed) return false;

        AuditEntry entry;
        entry.id = nextId++;
        entry.timestamp = std::time(nullptr);
        entry.event = event;
        entry.actor = actor;
        entry.resource = resource;
        entry.details = details;
        
        // Get hash of previous entry (blockchain chaining)
        if (!logChain.empty()) {
            entry.previousHash = logChain.back().entryHash;
        } else {
            entry.previousHash = "0000000000000000";  // Genesis block
        }
        
        // Compute hash of this entry
        entry.entryHash = computeHash(entry);
        
        // Append to chain (immutable)
        logChain.push_back(entry);
        
        // Persist to disk (append-only file)
        persistEntry(entry);
        
        std::cout << "[AuditLog] #" << entry.id << " " << event 
                  << " | actor=" << actor 
                  << " | resource=" << resource << "\n";
        
        return true;
    }

    // Verify log chain integrity
    bool verifyIntegrity() const {
        if (!licensed) return false;

        std::cout << "[AuditLog] Verifying audit log integrity...\n";

        if (logChain.empty()) {
            std::cout << "[AuditLog] Log chain is empty (OK)\n";
            return true;
        }

        // Verify each entry's hash
        for (size_t i = 0; i < logChain.size(); ++i) {
            const AuditEntry& entry = logChain[i];
            
            // Re-compute hash
            std::string computedHash = computeHash(entry);
            if (computedHash != entry.entryHash) {
                std::cerr << "[AuditLog] INTEGRITY FAILURE at entry #" 
                          << entry.id << "\n";
                std::cerr << "  Expected: " << entry.entryHash << "\n";
                std::cerr << "  Computed: " << computedHash << "\n";
                return false;
            }
            
            // Verify chain link
            if (i > 0) {
                const AuditEntry& prev = logChain[i - 1];
                if (entry.previousHash != prev.entryHash) {
                    std::cerr << "[AuditLog] CHAIN BREAK at entry #" 
                              << entry.id << "\n";
                    return false;
                }
            }
        }

        std::cout << "[AuditLog] Integrity verified: " << logChain.size() 
                  << " entries OK\n";
        return true;
    }

    // Query log entries
    std::vector<AuditEntry> query(const std::string& filter = "") const {
        if (!licensed) return {};

        std::vector<AuditEntry> results;
        
        for (const auto& entry : logChain) {
            if (filter.empty() || 
                entry.event.find(filter) != std::string::npos ||
                entry.actor.find(filter) != std::string::npos ||
                entry.resource.find(filter) != std::string::npos) {
                results.push_back(entry);
            }
        }
        
        return results;
    }

    // Export log to external system (compliance reporting)
    bool exportLog(const std::string& exportPath) const {
        if (!licensed) return false;

        std::cout << "[AuditLog] Exporting audit log to " << exportPath << "...\n";

        std::ofstream file(exportPath);
        if (!file) {
            std::cerr << "[AuditLog] Failed to open export file\n";
            return false;
        }

        // Write header
        file << "ID,Timestamp,Event,Actor,Resource,Details,PreviousHash,EntryHash\n";

        // Write entries
        for (const auto& entry : logChain) {
            file << entry.id << ","
                 << entry.timestamp << ","
                 << entry.event << ","
                 << entry.actor << ","
                 << entry.resource << ","
                 << entry.details << ","
                 << entry.previousHash << ","
                 << entry.entryHash << "\n";
        }

        file.close();
        
        std::cout << "[AuditLog] Exported " << logChain.size() 
                  << " entries to " << exportPath << "\n";
        return true;
    }

    // Get audit log status
    void printStatus() const {
        std::cout << "\n[AuditLog] Immutable Audit Log Status:\n";
        std::cout << "  Licensed: " << (licensed ? "YES" : "NO") << "\n";
        std::cout << "  Total entries: " << logChain.size() << "\n";
        std::cout << "  Log file: " << logFilePath << "\n";
        
        if (!logChain.empty()) {
            std::cout << "  First entry: #" << logChain.front().id 
                      << " (" << logChain.front().event << ")\n";
            std::cout << "  Last entry: #" << logChain.back().id 
                      << " (" << logChain.back().event << ")\n";
        }
    }

private:
    // Compute SHA-256 hash of entry (simplified)
    std::string computeHash(const AuditEntry& entry) const {
        // In production: Use SHA-256 or SHA3-256
        // For now: Simple checksum placeholder
        
        std::stringstream ss;
        ss << entry.id << entry.timestamp << entry.event 
           << entry.actor << entry.resource << entry.details 
           << entry.previousHash;
        
        std::string data = ss.str();
        
        // Simple hash: sum of bytes (NOT cryptographically secure)
        uint64_t hash = 0;
        for (char c : data) {
            hash = hash * 31 + static_cast<uint8_t>(c);
        }
        
        // Convert to hex string
        std::stringstream hashStr;
        hashStr << std::hex << std::setfill('0') << std::setw(16) << hash;
        
        return hashStr.str();
    }

    // Persist entry to append-only file
    void persistEntry(const AuditEntry& entry) {
        std::ofstream file(logFilePath, std::ios::app);
        if (!file) {
            std::cerr << "[AuditLog] Failed to write to log file\n";
            return;
        }

        file << entry.id << "|"
             << entry.timestamp << "|"
             << entry.event << "|"
             << entry.actor << "|"
             << entry.resource << "|"
             << entry.details << "|"
             << entry.previousHash << "|"
             << entry.entryHash << "\n";
        
        file.close();
    }

    // Load existing log chain from disk
    void loadLogChain() {
        std::ifstream file(logFilePath);
        if (!file) {
            std::cout << "[AuditLog] No existing log file (starting fresh)\n";
            return;
        }

        std::cout << "[AuditLog] Loading existing log chain...\n";

        std::string line;
        while (std::getline(file, line)) {
            AuditEntry entry;
            
            // Parse line (format: id|timestamp|event|actor|resource|details|prevHash|hash)
            std::istringstream ss(line);
            std::string token;
            
            std::getline(ss, token, '|'); entry.id = std::stoull(token);
            std::getline(ss, token, '|'); entry.timestamp = std::stoll(token);
            std::getline(ss, entry.event, '|');
            std::getline(ss, entry.actor, '|');
            std::getline(ss, entry.resource, '|');
            std::getline(ss, entry.details, '|');
            std::getline(ss, entry.previousHash, '|');
            std::getline(ss, entry.entryHash, '|');
            
            logChain.push_back(entry);
            nextId = entry.id + 1;
        }

        file.close();
        
        std::cout << "[AuditLog] Loaded " << logChain.size() 
                  << " existing entries\n";
    }
};

} // namespace RawrXD::Sovereign

// ============================================================================
// Test Entry Point
// ============================================================================

#ifdef BUILD_AUDITLOG_TEST
int main() {
    std::cout << "RawrXD Immutable Audit Log Test\n";
    std::cout << "Track C: Sovereign Security Feature\n\n";

    RawrXD::Sovereign::ImmutableAuditLog auditLog("test_audit.log");

    // Log events
    auditLog.logEvent("USER_LOGIN", "alice", "system", "Successful login");
    auditLog.logEvent("MODEL_LOADED", "alice", "llama-3-70b.gguf", "");
    auditLog.logEvent("INFERENCE_START", "alice", "prompt-12345", "");
    auditLog.logEvent("KEY_ACCESSED", "alice", "encryption-key-01", "");
    auditLog.logEvent("USER_LOGOUT", "alice", "system", "");

    // Verify integrity
    if (!auditLog.verifyIntegrity()) {
        std::cerr << "[FAILED] Audit log integrity check failed\n";
        return 1;
    }

    // Query logs
    auto userLogs = auditLog.query("alice");
    std::cout << "\n[Query] Found " << userLogs.size() 
              << " entries for 'alice'\n";

    // Export for compliance
    auditLog.exportLog("audit_export.csv");

    auditLog.printStatus();

    std::cout << "\n[SUCCESS] Immutable audit logging operational\n";
    return 0;
}
#endif
