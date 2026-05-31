// ============================================================================
// ExtensionPermissions.cpp — Extension Security & Permission System Implementation
// ============================================================================

#include "ExtensionPermissions.hpp"
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

// ============================================================================
// Global Instance Management
// ============================================================================

static std::unique_ptr<ExtensionPermissionManager> g_permissionManager;
static std::mutex g_instanceMutex;

ExtensionPermissionManager& GetExtensionPermissionManager() {
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    if (!g_permissionManager) {
        g_permissionManager = std::make_unique<ExtensionPermissionManager>();
    }
    return *g_permissionManager;
}

// ============================================================================
// Extension Permission Manifest Implementation
// ============================================================================

ExtensionPermissionManifest ExtensionPermissionManifest::fromVSCodeManifest(const std::string& manifestJson) {
    ExtensionPermissionManifest manifest;
    
    try {
        json j = json::parse(manifestJson);
        
        // Parse enabledApiProposals (standard VS Code field)
        if (j.contains("enabledApiProposals") && j["enabledApiProposals"].is_array()) {
            for (const auto& proposal : j["enabledApiProposals"]) {
                if (proposal.is_string()) {
                    manifest.enabledApiProposals.insert(proposal);
                }
            }
        }
        
        // Parse custom rawrXDPermissions field
        if (j.contains("rawrXDPermissions") && j["rawrXDPermissions"].is_object()) {
            const auto& perms = j["rawrXDPermissions"];
            
            if (perms.contains("permissions") && perms["permissions"].is_array()) {
                for (const auto& perm : perms["permissions"]) {
                    if (perm.is_string()) {
                        manifest.requestedPermissions.push_back(perm);
                    }
                }
            }
            
            if (perms.contains("fileSystemPaths") && perms["fileSystemPaths"].is_array()) {
                for (const auto& path : perms["fileSystemPaths"]) {
                    if (path.is_string()) {
                        manifest.fileSystemPaths.push_back(path);
                    }
                }
            }
            
            if (perms.contains("networkHosts") && perms["networkHosts"].is_array()) {
                for (const auto& host : perms["networkHosts"]) {
                    if (host.is_string()) {
                        manifest.networkHosts.push_back(host);
                    }
                }
            }
            
            if (perms.contains("commandPatterns") && perms["commandPatterns"].is_array()) {
                for (const auto& cmd : perms["commandPatterns"]) {
                    if (cmd.is_string()) {
                        manifest.commandPatterns.push_back(cmd);
                    }
                }
            }
            
            if (perms.contains("requireTrustedWorkspace")) {
                manifest.requireTrustedWorkspace = perms["requireTrustedWorkspace"];
            }
            
            if (perms.contains("requireDeveloperMode")) {
                manifest.requireDeveloperMode = perms["requireDeveloperMode"];
            }
        }
        
        // Apply default resource limits
        manifest.resourceLimits = ExtensionResourceLimits::getDefault();
        
        // Language servers get enhanced permissions by default
        if (j.contains("contributes") && j["contributes"].contains("languages")) {
            manifest.requestedPermissions.push_back("languageServer");
            manifest.resourceLimits = ExtensionResourceLimits::getTrusted();
        }
        
        // Debuggers get enhanced permissions
        if (j.contains("contributes") && j["contributes"].contains("debuggers")) {
            manifest.requestedPermissions.push_back("debugger");
            manifest.resourceLimits = ExtensionResourceLimits::getTrusted();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ExtensionPermissions] Failed to parse manifest: " << e.what() << std::endl;
    }
    
    return manifest;
}

bool ExtensionPermissionManifest::validate(std::string& errorMessage) const {
    // Check for dangerous permission combinations
    bool hasNetworkAccess = std::find(requestedPermissions.begin(), requestedPermissions.end(), "network") != requestedPermissions.end();
    bool hasFileSystemWrite = std::find(requestedPermissions.begin(), requestedPermissions.end(), "fileSystemWrite") != requestedPermissions.end();
    bool hasProcessSpawn = std::find(requestedPermissions.begin(), requestedPermissions.end(), "processSpawn") != requestedPermissions.end();
    
    if (hasNetworkAccess && hasFileSystemWrite && hasProcessSpawn && !requireTrustedWorkspace) {
        errorMessage = "Extensions with network, file write, and process spawn permissions must require trusted workspace";
        return false;
    }
    
    // Validate network host patterns
    for (const std::string& host : networkHosts) {
        if (host == "*" && !requireTrustedWorkspace) {
            errorMessage = "Wildcard network access requires trusted workspace";
            return false;
        }
    }
    
    // Validate command patterns
    for (const std::string& pattern : commandPatterns) {
        if (pattern.find("rm ") != std::string::npos || pattern.find("del ") != std::string::npos) {
            if (!requireTrustedWorkspace) {
                errorMessage = "File deletion commands require trusted workspace";
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// Extension Permission Manager Implementation
// ============================================================================

ExtensionPermissionManager::ExtensionPermissionManager() 
    : m_workspaceTrusted(false), m_developerMode(false) {
    // Initialize with default secure state
}

ExtensionPermissionManager::~ExtensionPermissionManager() {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    m_extensions.clear();
}

bool ExtensionPermissionManager::registerExtension(
    const std::string& extensionId,
    const ExtensionPermissionManifest& manifest,
    bool isTrustedWorkspace,
    bool isDeveloperMode,
    std::string& errorMessage
) {
    // Validate manifest
    if (!validateManifest(manifest, errorMessage)) {
        return false;
    }
    
    // Check workspace trust requirement
    if (manifest.requireTrustedWorkspace && !isTrustedWorkspace) {
        errorMessage = "Extension requires trusted workspace";
        return false;
    }
    
    // Check developer mode requirement
    if (manifest.requireDeveloperMode && !isDeveloperMode) {
        errorMessage = "Extension requires developer mode";
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    // Create runtime context
    ExtensionRuntimeContext context;
    context.extensionId = extensionId;
    context.grantedPermissions = parsePermissions(manifest.requestedPermissions);
    context.resourceLimits = manifest.resourceLimits;
    context.isTrustedWorkspace = isTrustedWorkspace;
    context.isDeveloperMode = isDeveloperMode;
    context.currentMemoryUsage = 0;
    context.currentOpenFiles = 0;
    context.currentProcesses = 0;
    context.currentNetworkConnections = 0;
    context.apiCallCount = 0;
    context.lastAPICall = std::chrono::steady_clock::now();
    
    // Apply enhanced permissions for trusted workspace
    if (isTrustedWorkspace) {
        context.grantedPermissions |= static_cast<uint32_t>(ExtensionPermission::TrustedWorkspaceOnly);
        context.resourceLimits = ExtensionResourceLimits::getTrusted();
    }
    
    // Apply developer mode permissions
    if (isDeveloperMode) {
        context.grantedPermissions |= static_cast<uint32_t>(ExtensionPermission::DeveloperMode);
    }
    
    m_extensions[extensionId] = std::move(context);
    
    logSecurityEvent(extensionId, "Extension Registered", 
        "Permissions: " + std::to_string(context.grantedPermissions));
    
    return true;
}

bool ExtensionPermissionManager::unregisterExtension(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return false;
    }
    
    logSecurityEvent(extensionId, "Extension Unregistered", "");
    m_extensions.erase(it);
    return true;
}

bool ExtensionPermissionManager::hasPermission(const std::string& extensionId, ExtensionPermission permission) const {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return false; // Extension not registered = no permissions
    }
    
    const uint32_t permissionFlag = static_cast<uint32_t>(permission);
    return (it->second.grantedPermissions & permissionFlag) != 0;
}

bool ExtensionPermissionManager::canAccessFile(const std::string& extensionId, const std::string& filePath, bool write) const {
    // Check basic file system permissions
    ExtensionPermission requiredPerm = write ? ExtensionPermission::FileSystemWrite : ExtensionPermission::FileSystemRead;
    if (!hasPermission(extensionId, requiredPerm)) {
        return false;
    }
    
    // Check if file is in workspace (safer)
    if (isPathInWorkspace(filePath)) {
        return true;
    }
    
    // Global file system access requires special permission
    return hasPermission(extensionId, ExtensionPermission::FileSystemGlobal);
}

bool ExtensionPermissionManager::canConnectTo(const std::string& extensionId, const std::string& host, uint16_t port) const {
    // Check network permission
    if (!hasPermission(extensionId, ExtensionPermission::NetworkHTTP)) {
        return false;
    }
    
    // Localhost connections require special permission
    if (host == "localhost" || host == "127.0.0.1" || host == "::1") {
        return hasPermission(extensionId, ExtensionPermission::NetworkLocalhost);
    }
    
    // Check if host is in allowed list
    // Note: This would check against manifest.networkHosts in real implementation
    return true;
}

bool ExtensionPermissionManager::canSpawnProcess(const std::string& extensionId, const std::string& command) const {
    if (!hasPermission(extensionId, ExtensionPermission::ProcessSpawn)) {
        return false;
    }
    
    // Check command against allowed patterns
    // Note: This would check against manifest.commandPatterns in real implementation
    return true;
}

bool ExtensionPermissionManager::canUseAPIProposal(const std::string& extensionId, const std::string& proposalName) const {
    if (!hasPermission(extensionId, ExtensionPermission::APIProposed)) {
        return false;
    }
    
    // Note: This would check against manifest.enabledApiProposals in real implementation
    return true;
}

bool ExtensionPermissionManager::requestResource(const std::string& extensionId, const std::string& resourceType, size_t amount) {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return false;
    }
    
    ExtensionRuntimeContext& context = it->second;
    
    if (resourceType == "memory") {
        size_t newUsage = context.currentMemoryUsage.fetch_add(amount) + amount;
        if (newUsage > context.resourceLimits.maxMemoryBytes) {
            context.currentMemoryUsage.fetch_sub(amount);
            return false;
        }
    } else if (resourceType == "file") {
        uint32_t newFiles = context.currentOpenFiles.fetch_add(static_cast<uint32_t>(amount)) + static_cast<uint32_t>(amount);
        if (newFiles > context.resourceLimits.maxOpenFiles) {
            context.currentOpenFiles.fetch_sub(static_cast<uint32_t>(amount));
            return false;
        }
    } else if (resourceType == "process") {
        uint32_t newProcesses = context.currentProcesses.fetch_add(static_cast<uint32_t>(amount)) + static_cast<uint32_t>(amount);
        if (newProcesses > context.resourceLimits.maxProcesses) {
            context.currentProcesses.fetch_sub(static_cast<uint32_t>(amount));
            return false;
        }
    } else if (resourceType == "network") {
        uint32_t newConnections = context.currentNetworkConnections.fetch_add(static_cast<uint32_t>(amount)) + static_cast<uint32_t>(amount);
        if (newConnections > context.resourceLimits.maxNetworkConnections) {
            context.currentNetworkConnections.fetch_sub(static_cast<uint32_t>(amount));
            return false;
        }
    }
    
    return true;
}

void ExtensionPermissionManager::releaseResource(const std::string& extensionId, const std::string& resourceType, size_t amount) {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return;
    }
    
    ExtensionRuntimeContext& context = it->second;
    
    if (resourceType == "memory") {
        context.currentMemoryUsage.fetch_sub(amount);
    } else if (resourceType == "file") {
        context.currentOpenFiles.fetch_sub(static_cast<uint32_t>(amount));
    } else if (resourceType == "process") {
        context.currentProcesses.fetch_sub(static_cast<uint32_t>(amount));
    } else if (resourceType == "network") {
        context.currentNetworkConnections.fetch_sub(static_cast<uint32_t>(amount));
    }
}

bool ExtensionPermissionManager::checkResourceLimits(const std::string& extensionId, std::string& limitReached) const {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        limitReached = "Extension not registered";
        return false;
    }
    
    const ExtensionRuntimeContext& context = it->second;
    
    if (context.currentMemoryUsage.load() > context.resourceLimits.maxMemoryBytes) {
        limitReached = "Memory limit exceeded";
        return false;
    }
    
    if (context.currentOpenFiles.load() > context.resourceLimits.maxOpenFiles) {
        limitReached = "File handle limit exceeded";
        return false;
    }
    
    if (context.currentProcesses.load() > context.resourceLimits.maxProcesses) {
        limitReached = "Process limit exceeded";
        return false;
    }
    
    if (context.currentNetworkConnections.load() > context.resourceLimits.maxNetworkConnections) {
        limitReached = "Network connection limit exceeded";
        return false;
    }
    
    return true;
}

bool ExtensionPermissionManager::checkAPIRateLimit(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return false;
    }
    
    ExtensionRuntimeContext& context = it->second;
    auto now = std::chrono::steady_clock::now();
    
    // Reset counter every second
    if (now - context.lastAPICall >= std::chrono::seconds(1)) {
        context.apiCallCount = 0;
        context.lastAPICall = now;
    }
    
    uint32_t currentCalls = context.apiCallCount.fetch_add(1) + 1;
    return currentCalls <= context.resourceLimits.maxAPICallsPerSecond;
}

void ExtensionPermissionManager::logSecurityEvent(const std::string& extensionId, const std::string& event, const std::string& details) {
    std::lock_guard<std::mutex> lock(m_auditMutex);
    
    SecurityEvent secEvent;
    secEvent.timestamp = std::chrono::steady_clock::now();
    secEvent.extensionId = extensionId;
    secEvent.event = event;
    secEvent.details = details;
    
    m_auditLog.push_back(secEvent);
    
    // Keep audit log bounded (last 10000 events)
    if (m_auditLog.size() > 10000) {
        m_auditLog.erase(m_auditLog.begin(), m_auditLog.begin() + 1000);
    }
    
    // Debug output
    std::cout << "[ExtensionSecurity] " << extensionId << ": " << event;
    if (!details.empty()) {
        std::cout << " - " << details;
    }
    std::cout << std::endl;
}

std::vector<std::string> ExtensionPermissionManager::getSecurityLogs(const std::string& extensionId) const {
    std::lock_guard<std::mutex> lock(m_auditMutex);
    
    std::vector<std::string> logs;
    
    for (const auto& event : m_auditLog) {
        if (extensionId.empty() || event.extensionId == extensionId) {
            std::ostringstream oss;
            oss << "[" << event.extensionId << "] " << event.event << " - " << event.details;
            logs.push_back(oss.str());
        }
    }
    
    return logs;
}

void ExtensionPermissionManager::setWorkspaceTrusted(bool trusted) {
    m_workspaceTrusted = trusted;
    logSecurityEvent("SYSTEM", "Workspace Trust Changed", trusted ? "TRUSTED" : "UNTRUSTED");
}

void ExtensionPermissionManager::setDeveloperMode(bool enabled) {
    m_developerMode = enabled;
    logSecurityEvent("SYSTEM", "Developer Mode Changed", enabled ? "ENABLED" : "DISABLED");
}

void ExtensionPermissionManager::grantBuiltinPermissions(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    
    auto it = m_extensions.find(extensionId);
    if (it != m_extensions.end()) {
        it->second.grantedPermissions = PERMISSION_FULL_TRUST;
        it->second.resourceLimits = ExtensionResourceLimits::getTrusted();
        logSecurityEvent(extensionId, "Granted Builtin Permissions", "FULL_TRUST");
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

bool ExtensionPermissionManager::validateManifest(const ExtensionPermissionManifest& manifest, std::string& error) const {
    return manifest.validate(error);
}

uint32_t ExtensionPermissionManager::parsePermissions(const std::vector<std::string>& permissions) const {
    uint32_t result = 0;
    
    for (const std::string& perm : permissions) {
        if (perm == "fileSystemRead") {
            result |= static_cast<uint32_t>(ExtensionPermission::FileSystemRead);
        } else if (perm == "fileSystemWrite") {
            result |= static_cast<uint32_t>(ExtensionPermission::FileSystemWrite);
        } else if (perm == "fileSystemGlobal") {
            result |= static_cast<uint32_t>(ExtensionPermission::FileSystemGlobal);
        } else if (perm == "network") {
            result |= static_cast<uint32_t>(ExtensionPermission::NetworkHTTP);
        } else if (perm == "networkLocalhost") {
            result |= static_cast<uint32_t>(ExtensionPermission::NetworkLocalhost);
        } else if (perm == "processSpawn") {
            result |= static_cast<uint32_t>(ExtensionPermission::ProcessSpawn);
        } else if (perm == "debugger") {
            result |= static_cast<uint32_t>(ExtensionPermission::ProcessDebugger);
            result |= static_cast<uint32_t>(ExtensionPermission::APIProposed);
        } else if (perm == "languageServer") {
            result |= PERMISSION_LANGUAGE_SERVER;
        } else if (perm == "apiStable") {
            result |= static_cast<uint32_t>(ExtensionPermission::APIStable);
        } else if (perm == "apiProposed") {
            result |= static_cast<uint32_t>(ExtensionPermission::APIProposed);
        }
        // Add more permission mappings as needed
    }
    
    // Always grant stable API access by default
    result |= static_cast<uint32_t>(ExtensionPermission::APIStable);
    
    return result;
}

std::string ExtensionPermissionManager::getWorkspaceRoot() const {
    // Get actual workspace root from IDE or environment
    // Try IDE first, then environment variable, then current path
    if (g_ideInstance && g_ideInstance->GetWorkspaceRoot()) {
        return g_ideInstance->GetWorkspaceRoot();
    }
    const char* workspace = std::getenv("RAWRXD_WORKSPACE");
    if (workspace) {
        return workspace;
    }
    return std::filesystem::current_path().string();
}

bool ExtensionPermissionManager::isPathInWorkspace(const std::string& path) const {
    std::string workspaceRoot = getWorkspaceRoot();
    std::filesystem::path absPath = std::filesystem::absolute(path);
    std::filesystem::path absWorkspace = std::filesystem::absolute(workspaceRoot);
    
    // Check if path is under workspace
    auto workspaceString = absWorkspace.string();
    auto pathString = absPath.string();
    
    return pathString.length() >= workspaceString.length() &&
           pathString.substr(0, workspaceString.length()) == workspaceString;
}

bool ExtensionPermissionManager::isNetworkHostAllowed(const std::string& host, const std::vector<std::string>& allowedHosts) const {
    for (const std::string& allowedHost : allowedHosts) {
        if (allowedHost == "*" || allowedHost == host) {
            return true;
        }
        
        // Simple wildcard matching for subdomains
        if (allowedHost.front() == '*' && allowedHost.length() > 1) {
            std::string domain = allowedHost.substr(1); // Remove '*'
            if (host.length() >= domain.length() && 
                host.substr(host.length() - domain.length()) == domain) {
                return true;
            }
        }
    }
    
    return false;
}

bool ExtensionPermissionManager::isCommandAllowed(const std::string& command, const std::vector<std::string>& patterns) const {
    for (const std::string& pattern : patterns) {
        try {
            std::regex regex(pattern);
            if (std::regex_match(command, regex)) {
                return true;
            }
        } catch (const std::regex_error&) {
            // Fallback to simple substring matching
            if (command.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

// ============================================================================
// Integration Helper Functions
// ============================================================================

static std::string g_currentExtensionId;
static std::mutex g_currentExtensionMutex;

std::string GetCurrentExtensionId() {
    std::lock_guard<std::mutex> lock(g_currentExtensionMutex);
    return g_currentExtensionId;
}

void MarkExtensionAsSystemBuiltin(const std::string& extensionId) {
    GetExtensionPermissionManager().grantBuiltinPermissions(extensionId);
}

bool RegisterExtensionFromVSIX(
    const std::string& extensionId,
    const std::string& manifestJson,
    bool isTrustedWorkspace,
    std::string& errorMessage
) {
    ExtensionPermissionManifest manifest = ExtensionPermissionManifest::fromVSCodeManifest(manifestJson);
    
    // Check if this is a system extension (e.g., rawrxd.*)
    bool isDeveloperMode = extensionId.find("rawrxd.") == 0 || 
                          extensionId.find("system.") == 0;
    
    return GetExtensionPermissionManager().registerExtension(
        extensionId, manifest, isTrustedWorkspace, isDeveloperMode, errorMessage
    );
}
