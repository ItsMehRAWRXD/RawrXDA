/**
 * @file security_sandbox.h
 * @brief Extension isolation and security
 * 
 * @author RawrXD Security Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <mutex>
#include <chrono>
#include <filesystem>

namespace RawrXD::Extensions {

// ============================================================================
// Sandbox Configuration
// ============================================================================

struct SandboxConfig {
    std::vector<std::filesystem::path> allowedReadDirs;
    std::vector<std::filesystem::path> allowedWriteDirs;
    std::vector<std::string> allowedHosts;
    std::vector<int> allowedPorts;
    std::vector<std::string> allowedCommands;
    size_t maxMemoryPerExtension = 256 * 1024 * 1024; // 256MB
    int maxCPUThreads = 4;
    size_t maxStoragePerExtension = 1024 * 1024 * 1024; // 1GB
    size_t maxViolationLogSize = 1000;
};

// ============================================================================
// Resource Usage
// ============================================================================

struct ResourceUsage {
    size_t memoryUsage = 0;
    size_t storageUsage = 0;
    int activeThreads = 0;
};

// ============================================================================
// Violation
// ============================================================================

struct Violation {
    int64_t extId = -1;
    std::string permission;
    std::string resource;
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// Security Sandbox
// ============================================================================

class SecuritySandbox {
public:
    explicit SecuritySandbox(const SandboxConfig& config);
    ~SecuritySandbox();
    
    // Permission management
    bool grantPermission(int64_t extId, const std::string& permission);
    bool revokePermission(int64_t extId, const std::string& permission);
    bool hasPermission(int64_t extId, const std::string& permission) const;
    
    // File system access control
    bool canReadFile(int64_t extId, const std::string& path) const;
    bool canWriteFile(int64_t extId, const std::string& path) const;
    bool canDeleteFile(int64_t extId, const std::string& path) const;
    
    // Network access control
    bool canAccessNetwork(int64_t extId, const std::string& host, int port) const;
    bool canListen(int64_t extId, int port) const;
    
    // Process access control
    bool canExecuteProcess(int64_t extId, const std::string& command) const;
    bool canAccessProcess(int64_t extId, int pid) const;
    
    // Resource limits
    bool checkMemoryLimit(int64_t extId, size_t requestedBytes) const;
    bool checkCPULimit(int64_t extId, int requestedCores) const;
    bool checkStorageLimit(int64_t extId, size_t requestedBytes) const;
    
    // Resource tracking
    void trackMemoryAllocation(int64_t extId, size_t bytes);
    void trackMemoryDeallocation(int64_t extId, size_t bytes);
    void trackStorageAllocation(int64_t extId, size_t bytes);
    void trackStorageDeallocation(int64_t extId, size_t bytes);
    
    // Enforcement
    bool enforceFileRead(int64_t extId, const std::string& path);
    bool enforceFileWrite(int64_t extId, const std::string& path);
    bool enforceNetworkAccess(int64_t extId, const std::string& host, int port);
    bool enforceProcessExecution(int64_t extId, const std::string& command);
    
    // Violation logging
    std::vector<Violation> getViolations(int64_t extId) const;
    
    // Cleanup
    void cleanupExtension(int64_t extId);
    
private:
    void logViolation(int64_t extId, const std::string& permission,
                     const std::string& resource);
    
    SandboxConfig m_config;
    mutable std::mutex m_mutex;
    
    std::map<int64_t, std::set<std::string>> m_permissions;
    std::map<int64_t, ResourceUsage> m_resourceUsage;
    std::map<int64_t, std::set<int>> m_childProcesses;
    std::vector<Violation> m_violations;
};

} // namespace RawrXD::Extensions
