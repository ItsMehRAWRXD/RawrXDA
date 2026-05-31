// ============================================================================
// extension_permissions.cpp — Extension Permissions Manager Implementation
// ============================================================================
// Architecture: C++20 | Win32 | Deny-by-default security | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "extension_permissions.h"

#include <windows.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Global Manager Instance
// ============================================================================

static ExtensionPermissionsManager* g_permissionsManager = nullptr;

ExtensionPermissionsManager& GetPermissionsManager() {
    if (!g_permissionsManager) {
        g_permissionsManager = new ExtensionPermissionsManager();
    }
    return *g_permissionsManager;
}

// ============================================================================
// Helper Functions
// ============================================================================

static std::string PermissionScopeToString(PermissionScope scope) {
    switch (scope) {
        case PermissionScope::FileSystemRead: return "fs.read";
        case PermissionScope::FileSystemWrite: return "fs.write";
        case PermissionScope::FileSystemDelete: return "fs.delete";
        case PermissionScope::FileSystemExecute: return "fs.execute";
        case PermissionScope::NetworkHttp: return "net.http";
        case PermissionScope::NetworkWebsocket: return "net.websocket";
        case PermissionScope::NetworkTcp: return "net.tcp";
        case PermissionScope::NetworkUdp: return "net.udp";
        case PermissionScope::WorkspaceRead: return "workspace.read";
        case PermissionScope::WorkspaceWrite: return "workspace.write";
        case PermissionScope::WorkspaceCommand: return "workspace.command";
        case PermissionScope::WorkspaceTrust: return "workspace.trust";
        case PermissionScope::SystemTerminal: return "system.terminal";
        case PermissionScope::SystemClipboard: return "system.clipboard";
        case PermissionScope::SystemProcessSpawn: return "system.spawn";
        case PermissionScope::SystemEnvironment: return "system.env";
        case PermissionScope::VscodeSecrets: return "vscode.secrets";
        case PermissionScope::VscodeLanguageServer: return "vscode.lsp";
        case PermissionScope::VscodeDebugger: return "vscode.debug";
        case PermissionScope::DevTools: return "devtools";
        default: return "unknown";
    }
}

static PermissionScope StringToPermissionScope(const std::string& str) {
    if (str == "fs.read") return PermissionScope::FileSystemRead;
    if (str == "fs.write") return PermissionScope::FileSystemWrite;
    if (str == "fs.delete") return PermissionScope::FileSystemDelete;
    if (str == "fs.execute") return PermissionScope::FileSystemExecute;
    if (str == "net.http") return PermissionScope::NetworkHttp;
    if (str == "net.websocket") return PermissionScope::NetworkWebsocket;
    if (str == "net.tcp") return PermissionScope::NetworkTcp;
    if (str == "net.udp") return PermissionScope::NetworkUdp;
    if (str == "workspace.read") return PermissionScope::WorkspaceRead;
    if (str == "workspace.write") return PermissionScope::WorkspaceWrite;
    if (str == "workspace.command") return PermissionScope::WorkspaceCommand;
    if (str == "workspace.trust") return PermissionScope::WorkspaceTrust;
    if (str == "system.terminal") return PermissionScope::SystemTerminal;
    if (str == "system.clipboard") return PermissionScope::SystemClipboard;
    if (str == "system.spawn") return PermissionScope::SystemProcessSpawn;
    if (str == "system.env") return PermissionScope::SystemEnvironment;
    if (str == "vscode.secrets") return PermissionScope::VscodeSecrets;
    if (str == "vscode.lsp") return PermissionScope::VscodeLanguageServer;
    if (str == "vscode.debug") return PermissionScope::VscodeDebugger;
    if (str == "devtools") return PermissionScope::DevTools;
    return static_cast<PermissionScope>(0);
}

// ============================================================================
// ExtensionPermissionsManager Implementation
// ============================================================================

ExtensionPermissionsManager::ExtensionPermissionsManager()
    : m_defaultPolicy(PermissionScope::DefaultSafe)
    , m_workspaceTrusted(false)
{
}

ExtensionPermissionsManager::~ExtensionPermissionsManager() {
}

bool ExtensionPermissionsManager::RegisterExtension(
    const std::string& extensionId,
    PermissionScope requestedScopes,
    std::function<void()> onNeedsApproval
) {
    if (extensionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    auto& record = m_permissions[extensionId];
    record.requestedScopes = requestedScopes;

    // Auto-grant safe defaults
    record.grantedScopes = m_defaultPolicy;

    // Check if additional approval might be needed
    if ((requestedScopes & ~m_defaultPolicy) != static_cast<PermissionScope>(0)) {
        record.lastDecision = PermissionDecision::PromptRequired;
        if (onNeedsApproval) {
            m_approvalCallbacks[extensionId] = onNeedsApproval;
        }
    } else {
        record.lastDecision = PermissionDecision::Approved;
    }

    return true;
}

PermissionCheckResult ExtensionPermissionsManager::CheckPermission(
    const std::string& extensionId,
    PermissionScope scope,
    const std::string& detail
) {
    if (extensionId.empty()) {
        return PermissionCheckResult::Deny("Extension ID is empty");
    }

    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(extensionId);
    if (it == m_permissions.end()) {
        return PermissionCheckResult::Deny("Extension not registered");
    }

    auto& record = it->second;

    // Check if extension is blacklisted
    if (record.isBlacklisted) {
        return PermissionCheckResult::Deny("Extension is blacklisted");
    }

    // Check if workspace trust is required
    if ((scope & PermissionScope::WorkspaceTrust) && !m_workspaceTrusted) {
        return PermissionCheckResult::Deny("Workspace not trusted for this operation");
    }

    // Check granted scopes
    if (ScopeContains(record.grantedScopes, scope)) {
        return PermissionCheckResult::Allow(record.grantedScopes);
    }

    // Check if permission was explicitly denied
    if (record.lastDecision == PermissionDecision::Denied) {
        return PermissionCheckResult::Deny("Permission previously denied by user");
    }

    // Permission required but not yet decided
    return PermissionCheckResult::Deny("Permission approval required");
}

bool ExtensionPermissionsManager::CheckAllPermissions(
    const std::string& extensionId,
    PermissionScope scope
) {
    auto result = CheckPermission(extensionId, scope);
    return result.allowed;
}

void ExtensionPermissionsManager::RequestUserApproval(
    const PermissionRequest& request,
    std::function<void(const PermissionApproval&)> callback
) {
    if (!callback) {
        return;
    }

    PermissionApproval approval;
    approval.extensionId = request.extensionId;
    approval.scope = request.scope;

    // Build permission description
    std::string scopeDesc = PermissionScopeToString(request.scope);
    std::string msg = "Extension '" + request.extensionId + "' requests permission:\n\n" +
                      "Scope: " + scopeDesc + "\n";
    if (!request.detail.empty()) {
        msg += "Detail: " + request.detail + "\n";
    }
    msg += "\nAllow this permission?";

    std::wstring wMsg(msg.begin(), msg.end());
    std::wstring wTitle = L"RawrXD — Extension Permission";

    int result = MessageBoxW(nullptr, wMsg.c_str(), wTitle.c_str(),
                             MB_YESNOCANCEL | MB_ICONQUESTION | MB_TOPMOST);

    if (result == IDYES) {
        approval.decision = PermissionDecision::Approved;
        RecordApproval(approval);
    } else if (result == IDNO) {
        approval.decision = PermissionDecision::Denied;
        RecordApproval(approval);
    } else {
        approval.decision = PermissionDecision::PromptRequired;
    }

    callback(approval);
}

bool ExtensionPermissionsManager::RecordApproval(const PermissionApproval& approval) {
    if (approval.extensionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(approval.extensionId);
    if (it == m_permissions.end()) {
        return false;
    }

    auto& record = it->second;

    if (approval.decision == PermissionDecision::Approved ||
        approval.decision == PermissionDecision::TemporaryApproval) {
        record.grantedScopes = approval.scope;
        record.lastDecision = approval.decision;
        record.lastPromptTimeMs = ::GetTickCount64();
    } else if (approval.decision == PermissionDecision::Denied) {
        record.lastDecision = PermissionDecision::Denied;
    }

    return true;
}

bool ExtensionPermissionsManager::RevokePermission(
    const std::string& extensionId,
    PermissionScope scope
) {
    if (extensionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(extensionId);
    if (it == m_permissions.end()) {
        return false;
    }

    auto& record = it->second;

    // Remove specific scope from granted
    uint32_t current = static_cast<uint32_t>(record.grantedScopes);
    uint32_t toRevoke = static_cast<uint32_t>(scope);
    current &= ~toRevoke;
    record.grantedScopes = static_cast<PermissionScope>(current);

    return true;
}

PermissionScope ExtensionPermissionsManager::GetGrantedScopes(
    const std::string& extensionId
) const {
    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(extensionId);
    if (it == m_permissions.end()) {
        return static_cast<PermissionScope>(0);
    }

    return it->second.grantedScopes;
}

PermissionScope ExtensionPermissionsManager::GetRequestedScopes(
    const std::string& extensionId
) const {
    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(extensionId);
    if (it == m_permissions.end()) {
        return static_cast<PermissionScope>(0);
    }

    return it->second.requestedScopes;
}

std::vector<std::string> ExtensionPermissionsManager::GetApprovedExtensions() const {
    std::lock_guard<std::mutex> lock(m_lock);

    std::vector<std::string> approved;
    for (const auto& [id, record] : m_permissions) {
        if (record.lastDecision == PermissionDecision::Approved) {
            approved.push_back(id);
        }
    }
    return approved;
}

std::vector<std::string> ExtensionPermissionsManager::GetDeniedExtensions() const {
    std::lock_guard<std::mutex> lock(m_lock);

    std::vector<std::string> denied;
    for (const auto& [id, record] : m_permissions) {
        if (record.lastDecision == PermissionDecision::Denied) {
            denied.push_back(id);
        }
    }
    return denied;
}

std::vector<std::string> ExtensionPermissionsManager::GetPendingApprovals() const {
    std::lock_guard<std::mutex> lock(m_lock);

    std::vector<std::string> pending;
    for (const auto& [id, record] : m_permissions) {
        if (record.lastDecision == PermissionDecision::PromptRequired) {
            pending.push_back(id);
        }
    }
    return pending;
}

void ExtensionPermissionsManager::SetDefaultPolicy(PermissionScope defaultScopes) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_defaultPolicy = defaultScopes;
}

PermissionScope ExtensionPermissionsManager::GetDefaultPolicy() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_defaultPolicy;
}

void ExtensionPermissionsManager::SetExtensionPolicy(
    const std::string& extensionId,
    PermissionScope scopes
) {
    if (extensionId.empty()) return;

    std::lock_guard<std::mutex> lock(m_lock);

    auto it = m_permissions.find(extensionId);
    if (it != m_permissions.end()) {
        it->second.grantedScopes = scopes;
    }
}

void ExtensionPermissionsManager::SetWorkspaceTrusted(bool trusted) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_workspaceTrusted = trusted;
}

bool ExtensionPermissionsManager::IsWorkspaceTrusted() const {
    std::lock_guard<std::mutex> lock(m_lock);
    return m_workspaceTrusted;
}

bool ExtensionPermissionsManager::LoadPermissions(const std::string& storagePath) {
    if (storagePath.empty()) {
        return false;
    }

    try {
        std::ifstream file(storagePath);
        if (!file.is_open()) {
            return false;  // File doesn't exist yet, not an error
        }

        json data;
        file >> data;
        file.close();

        std::lock_guard<std::mutex> lock(m_lock);

        if (data.contains("permissions") && data["permissions"].is_object()) {
            for (auto& [extId, perms] : data["permissions"].items()) {
                if (!perms.is_object()) continue;
                auto& record = m_permissions[extId];
                if (perms.contains("requestedScopes") && perms["requestedScopes"].is_number()) {
                    record.requestedScopes = static_cast<PermissionScope>(perms["requestedScopes"].get<uint32_t>());
                }
                if (perms.contains("grantedScopes") && perms["grantedScopes"].is_number()) {
                    record.grantedScopes = static_cast<PermissionScope>(perms["grantedScopes"].get<uint32_t>());
                }
                if (perms.contains("lastDecision") && perms["lastDecision"].is_string()) {
                    std::string d = perms["lastDecision"].get<std::string>();
                    if (d == "approved") record.lastDecision = PermissionDecision::Approved;
                    else if (d == "denied") record.lastDecision = PermissionDecision::Denied;
                    else if (d == "temporary") record.lastDecision = PermissionDecision::TemporaryApproval;
                    else record.lastDecision = PermissionDecision::PromptRequired;
                }
                if (perms.contains("lastPromptTimeMs") && perms["lastPromptTimeMs"].is_number()) {
                    record.lastPromptTimeMs = perms["lastPromptTimeMs"].get<uint64_t>();
                }
                if (perms.contains("isBlacklisted") && perms["isBlacklisted"].is_boolean()) {
                    record.isBlacklisted = perms["isBlacklisted"].get<bool>();
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ExtensionPermissionsManager::SavePermissions(const std::string& storagePath) {
    if (storagePath.empty()) {
        return false;
    }

    try {
        json data;
        data["permissions"] = json::object();

        {
            std::lock_guard<std::mutex> lock(m_lock);
            for (const auto& [extId, record] : m_permissions) {
                json item;
                item["requestedScopes"] = static_cast<uint32_t>(record.requestedScopes);
                item["grantedScopes"] = static_cast<uint32_t>(record.grantedScopes);
                switch (record.lastDecision) {
                    case PermissionDecision::Approved: item["lastDecision"] = "approved"; break;
                    case PermissionDecision::Denied: item["lastDecision"] = "denied"; break;
                    case PermissionDecision::TemporaryApproval: item["lastDecision"] = "temporary"; break;
                    default: item["lastDecision"] = "prompt"; break;
                }
                item["lastPromptTimeMs"] = record.lastPromptTimeMs;
                item["isBlacklisted"] = record.isBlacklisted;
                data["permissions"][extId] = item;
            }
        }

        std::ofstream file(storagePath);
        if (!file.is_open()) {
            return false;
        }

        file << data.dump(2);
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

bool ExtensionPermissionsManager::ScopeContains(
    PermissionScope granted,
    PermissionScope requested
) const {
    uint32_t g = static_cast<uint32_t>(granted);
    uint32_t r = static_cast<uint32_t>(requested);
    return (g & r) == r;
}

std::string ExtensionPermissionsManager::ScopeToString(PermissionScope scope) const {
    return PermissionScopeToString(scope);
}

// ============================================================================
// Global Helper Functions
// ============================================================================

PermissionCheckResult CheckExtensionPermission(const std::string& extensionId,
                                               PermissionScope scope) {
    return GetPermissionsManager().CheckPermission(extensionId, scope);
}

bool RequestExtensionPermission(const std::string& extensionId,
                                PermissionScope scope,
                                const std::string& reason) {
    PermissionRequest req(extensionId, scope, reason);
    GetPermissionsManager().RequestUserApproval(req,
        [](const PermissionApproval& approval) {
            fprintf(stderr, "[ExtensionPermissions] User response: %s\n", approval.granted ? "granted" : "denied");
        }
    );
    return false;  // Would be true after user approves
}

}  // namespace Extensions
}  // namespace RawrXD

// ============================================================================
// End of extension_permissions.cpp
// ============================================================================
