/**
 * @file security_sandbox.cpp
 * @brief Extension isolation and security
 * 
 * Provides:
 * - File system access control
 * - Network access restrictions
 * - Process isolation
 * - Permission enforcement
 * - Resource limits
 * 
 * @author RawrXD Security Team
 * @version 1.0.0
 */

#include "security_sandbox.h"
#include <windows.h>
#include <psapi.h>
#include <filesystem>
#include <algorithm>

namespace RawrXD::Extensions {

// ============================================================================
// SecuritySandbox Implementation
// ============================================================================

SecuritySandbox::SecuritySandbox(const SandboxConfig& config)
    : m_config(config)
{
}

SecuritySandbox::~SecuritySandbox() = default;

// ============================================================================
// Permission Management
// ============================================================================

bool SecuritySandbox::grantPermission(int64_t extId, const std::string& permission) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_permissions[extId].insert(permission);
    return true;
}

bool SecuritySandbox::revokePermission(int64_t extId, const std::string& permission) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_permissions.find(extId);
    if (it == m_permissions.end()) {
        return false;
    }
    
    it->second.erase(permission);
    return true;
}

bool SecuritySandbox::hasPermission(int64_t extId, const std::string& permission) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_permissions.find(extId);
    if (it == m_permissions.end()) {
        return false;
    }
    
    // Check for wildcard permission
    if (it->second.find("*") != it->second.end()) {
        return true;
    }
    
    return it->second.find(permission) != it->second.end();
}

// ============================================================================
// File System Access Control
// ============================================================================

bool SecuritySandbox::canReadFile(int64_t extId, const std::string& path) const {
    if (!hasPermission(extId, "fs.read")) {
        return false;
    }
    
    // Check path against allowed directories
    std::filesystem::path filePath(path);
    std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(filePath);
    
    for (const auto& allowedDir : m_config.allowedReadDirs) {
        std::filesystem::path allowedPath = std::filesystem::weakly_canonical(allowedDir);
        // Allow if canonicalPath starts with allowedPath
        auto [itAllowed, itCanon] = std::mismatch(
            allowedPath.begin(), allowedPath.end(),
            canonicalPath.begin(), canonicalPath.end());
        if (itAllowed == allowedPath.end()) {
            return true;
        }
    }
    
    return false;
}

bool SecuritySandbox::canWriteFile(int64_t extId, const std::string& path) const {
    if (!hasPermission(extId, "fs.write")) {
        return false;
    }
    
    std::filesystem::path filePath(path);
    std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(filePath);
    
    for (const auto& allowedDir : m_config.allowedWriteDirs) {
        std::filesystem::path allowedPath = std::filesystem::weakly_canonical(allowedDir);
        // Allow if canonicalPath starts with allowedPath
        auto [itAllowed, itCanon] = std::mismatch(
            allowedPath.begin(), allowedPath.end(),
            canonicalPath.begin(), canonicalPath.end());
        if (itAllowed == allowedPath.end()) {
            return true;
        }
    }
    
    return false;
}

bool SecuritySandbox::canDeleteFile(int64_t extId, const std::string& path) const {
    if (!hasPermission(extId, "fs.delete")) {
        return false;
    }
    
    return canWriteFile(extId, path);
}

// ============================================================================
// Network Access Control
// ============================================================================

bool SecuritySandbox::canAccessNetwork(int64_t extId, const std::string& host,
                                      int port) const {
    if (!hasPermission(extId, "network")) {
        return false;
    }
    
    // Check against allowed hosts
    for (const auto& allowedHost : m_config.allowedHosts) {
        if (allowedHost == "*" || allowedHost == host) {
            return true;
        }
    }
    
    return false;
}

bool SecuritySandbox::canListen(int64_t extId, int port) const {
    if (!hasPermission(extId, "network.listen")) {
        return false;
    }
    
    // Check against allowed ports
    for (const auto& allowedPort : m_config.allowedPorts) {
        if (allowedPort == 0 || allowedPort == port) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Process Access Control
// ============================================================================

bool SecuritySandbox::canExecuteProcess(int64_t extId, const std::string& command) const {
    if (!hasPermission(extId, "process.exec")) {
        return false;
    }
    
    // Check against allowed commands
    for (const auto& allowedCmd : m_config.allowedCommands) {
        if (allowedCmd == "*" || command.find(allowedCmd) == 0) {
            return true;
        }
    }
    
    return false;
}

bool SecuritySandbox::canAccessProcess(int64_t extId, int pid) const {
    if (!hasPermission(extId, "process.access")) {
        return false;
    }
    
    // Only allow access to child processes
    auto it = m_childProcesses.find(extId);
    if (it != m_childProcesses.end()) {
        return it->second.find(pid) != it->second.end();
    }
    
    return false;
}

// ============================================================================
// Resource Limits
// ============================================================================

bool SecuritySandbox::checkMemoryLimit(int64_t extId, size_t requestedBytes) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceUsage.find(extId);
    if (it == m_resourceUsage.end()) {
        return requestedBytes <= m_config.maxMemoryPerExtension;
    }
    
    return (it->second.memoryUsage + requestedBytes) <= m_config.maxMemoryPerExtension;
}

bool SecuritySandbox::checkCPULimit(int64_t extId, int requestedCores) const {
    return requestedCores <= m_config.maxCPUThreads;
}

bool SecuritySandbox::checkStorageLimit(int64_t extId, size_t requestedBytes) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceUsage.find(extId);
    if (it == m_resourceUsage.end()) {
        return requestedBytes <= m_config.maxStoragePerExtension;
    }
    
    return (it->second.storageUsage + requestedBytes) <= m_config.maxStoragePerExtension;
}

// ============================================================================
// Resource Tracking
// ============================================================================

void SecuritySandbox::trackMemoryAllocation(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceUsage[extId].memoryUsage += bytes;
}

void SecuritySandbox::trackMemoryDeallocation(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceUsage[extId].memoryUsage -= std::min(bytes, m_resourceUsage[extId].memoryUsage);
}

void SecuritySandbox::trackStorageAllocation(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceUsage[extId].storageUsage += bytes;
}

void SecuritySandbox::trackStorageDeallocation(int64_t extId, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceUsage[extId].storageUsage -= std::min(bytes, m_resourceUsage[extId].storageUsage);
}

// ============================================================================
// Enforcement
// ============================================================================

bool SecuritySandbox::enforceFileRead(int64_t extId, const std::string& path) {
    if (!canReadFile(extId, path)) {
        logViolation(extId, "fs.read", path);
        return false;
    }
    return true;
}

bool SecuritySandbox::enforceFileWrite(int64_t extId, const std::string& path) {
    if (!canWriteFile(extId, path)) {
        logViolation(extId, "fs.write", path);
        return false;
    }
    return true;
}

bool SecuritySandbox::enforceNetworkAccess(int64_t extId, const std::string& host,
                                          int port) {
    if (!canAccessNetwork(extId, host, port)) {
        logViolation(extId, "network", host + ":" + std::to_string(port));
        return false;
    }
    return true;
}

bool SecuritySandbox::enforceProcessExecution(int64_t extId, const std::string& command) {
    if (!canExecuteProcess(extId, command)) {
        logViolation(extId, "process.exec", command);
        return false;
    }
    return true;
}

// ============================================================================
// Violation Logging
// ============================================================================

void SecuritySandbox::logViolation(int64_t extId, const std::string& permission,
                                  const std::string& resource) {
    Violation violation;
    violation.extId = extId;
    violation.permission = permission;
    violation.resource = resource;
    violation.timestamp = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violations.push_back(violation);
    
    // Keep only last N violations
    if (m_violations.size() > m_config.maxViolationLogSize) {
        m_violations.erase(m_violations.begin());
    }
}

std::vector<Violation> SecuritySandbox::getViolations(int64_t extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Violation> result;
    for (const auto& violation : m_violations) {
        if (violation.extId == extId) {
            result.push_back(violation);
        }
    }
    
    return result;
}

// ============================================================================
// Cleanup
// ============================================================================

void SecuritySandbox::cleanupExtension(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_permissions.erase(extId);
    m_resourceUsage.erase(extId);
    m_childProcesses.erase(extId);
}

} // namespace RawrXD::Extensions
