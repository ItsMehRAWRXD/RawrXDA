#include "ExtensionSandboxManager.h"
#include <algorithm>
#include <filesystem>

namespace RawrXD::Extensions {

namespace fs = std::filesystem;

ExtensionSandboxManager& ExtensionSandboxManager::GetInstance() {
    static ExtensionSandboxManager instance;
    return instance;
}

bool ExtensionSandboxManager::IsSubPath(const std::string& base, const std::string& target) {
    try {
        fs::path basePath = fs::absolute(base);
        fs::path targetPath = fs::absolute(target);
        
        auto rel = fs::relative(targetPath, basePath);
        return !rel.empty() && rel.native()[0] != L'.'; // Doesn't start with .. or .
    } catch (...) {
        return false;
    }
}

bool ExtensionSandboxManager::IsPathAllowed(const std::string& extensionId, const std::string& path, PermissionType accessType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_permissions.find(extensionId) == m_permissions.end()) return false;
    
    const auto& domain = m_permissions[extensionId];
    const auto& allowedPaths = (accessType == PermissionType::FileWrite) ? domain.allowedWritePaths : domain.allowedReadPaths;
    
    for (const auto& allowed : allowedPaths) {
        if (IsSubPath(allowed, path)) return true;
    }
    
    return false;
}

void ExtensionSandboxManager::GrantPath(const std::string& extensionId, const std::string& path, PermissionType accessType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (accessType == PermissionType::FileWrite) {
        m_permissions[extensionId].allowedWritePaths.insert(path);
    } else {
        m_permissions[extensionId].allowedReadPaths.insert(path);
    }
}

bool ExtensionSandboxManager::ValidateRequest(const std::string& extensionId, PermissionType type, const std::string& target) {
    // Fail-closed by default
    if (type == PermissionType::FileRead || type == PermissionType::FileWrite) {
        return IsPathAllowed(extensionId, target, type);
    }
    
    if (type == PermissionType::NetworkAccess) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_permissions[extensionId].hasNetworkAccess;
    }
    
    return false;
}

}
