#pragma once
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <windows.h>

namespace RawrXD::Extensions {

enum class PermissionType {
    FileRead,
    FileWrite,
    NetworkAccess,
    ExecuteProcess
};

class ExtensionSandboxManager {
public:
    static ExtensionSandboxManager& GetInstance();

    bool IsPathAllowed(const std::string& extensionId, const std::string& path, PermissionType accessType);
    void GrantPath(const std::string& extensionId, const std::string& path, PermissionType accessType);
    
    // Internal validation logic
    bool ValidateRequest(const std::string& extensionId, PermissionType type, const std::string& target);

private:
    ExtensionSandboxManager() = default;
    
    struct DomainPermissions {
        std::set<std::string> allowedReadPaths;
        std::set<std::string> allowedWritePaths;
        bool hasNetworkAccess = false;
    };

    std::map<std::string, DomainPermissions> m_permissions;
    std::mutex m_mutex;

    bool IsSubPath(const std::string& base, const std::string& target);
};

}
