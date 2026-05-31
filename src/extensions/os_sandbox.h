/**
 * @file os_sandbox.h
 * @brief OS-Level Security Sandbox for Extension Isolation
 *
 * Provides:
 * - Windows job objects for process isolation
 * - Restricted tokens for filesystem/network access
 * - ACL-based path allowlists
 * - Network firewall rules per extension
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>

namespace RawrXD::Extensions {

// ============================================================================
// Sandbox Configuration
// ============================================================================

struct OSSandboxConfig {
    std::vector<std::string> allowedReadPaths;
    std::vector<std::string> allowedWritePaths;
    std::vector<std::string> allowedHosts;
    std::vector<int> allowedPorts;
    size_t maxMemoryBytes = 256 * 1024 * 1024;
    uint32_t maxCpuPercent = 25;
    bool blockNetwork = false;
    bool blockRegistry = true;
    bool blockProcessCreation = true;
};

// ============================================================================
// OS Sandbox
// ============================================================================

class OSSandbox {
public:
    explicit OSSandbox(const OSSandboxConfig& config);
    ~OSSandbox();

    // Apply sandbox to an existing process
    bool applyToProcess(int64_t extId, HANDLE hProcess);

    // Remove sandbox from process
    bool removeFromProcess(int64_t extId);

    // Check if operation is allowed
    bool canReadFile(int64_t extId, const std::string& path) const;
    bool canWriteFile(int64_t extId, const std::string& path) const;
    bool canAccessNetwork(int64_t extId, const std::string& host, int port) const;
    bool canAccessRegistry(int64_t extId) const;
    bool canCreateProcess(int64_t extId) const;

    // Enforcement helpers
    bool enforceFileRead(int64_t extId, const std::string& path);
    bool enforceFileWrite(int64_t extId, const std::string& path);
    bool enforceNetworkAccess(int64_t extId, const std::string& host, int port);

    // Resource limits
    bool setMemoryLimit(int64_t extId, size_t bytes);
    bool setCpuLimit(int64_t extId, uint32_t percent);

    // Audit
    struct Violation {
        int64_t extId = -1;
        std::string operation;
        std::string resource;
        uint64_t timestamp = 0;
    };
    std::vector<Violation> getViolations(int64_t extId) const;
    void clearViolations(int64_t extId);

private:
    bool createRestrictedToken(HANDLE hProcess, HANDLE& hTokenOut);
    bool buildPathAcl(const std::vector<std::string>& paths,
                      ACCESS_MASK access, PACL& pAcl);
    bool isPathAllowed(const std::string& path,
                       const std::vector<std::string>& allowed) const;
    void logViolation(int64_t extId, const std::string& op,
                      const std::string& resource);

    OSSandboxConfig m_config;
    mutable std::mutex m_mutex;
    std::map<int64_t, HANDLE> m_jobs;       // extId -> job handle
    std::map<int64_t, HANDLE> m_tokens;    // extId -> restricted token
    std::vector<Violation> m_violations;
    size_t m_maxViolationLog = 1000;
};

} // namespace RawrXD::Extensions
