// ============================================================================
// quickjs_sandbox.cpp — Plugin Trust Boundary (QuickJS Sandbox Hardening)
// ============================================================================
//
// Enforcement layer between QuickJS extensions and the RawrXD core.
// Whitelist-based native function access, memory/CPU limits, FS sandboxing,
// and network access control.
//
// DEPS:     quickjs_sandbox.h
// PATTERN:  PatchResult-compatible, no exceptions
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/quickjs_sandbox.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace RawrXD {
namespace Sandbox {

// ============================================================================
// Singleton
// ============================================================================
PluginSandbox& PluginSandbox::instance() {
    static PluginSandbox s_instance;
    return s_instance;
}

PluginSandbox::PluginSandbox()
    : m_initialized(false)
    , m_extensionCount(0)
    , m_violationCallback(nullptr)
    , m_violationUserData(nullptr)
{
    memset(m_extensions, 0, sizeof(m_extensions));
    memset(&m_stats, 0, sizeof(m_stats));
}

PluginSandbox::~PluginSandbox() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
SandboxResult PluginSandbox::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) return SandboxResult::ok("Already initialized");

    m_extensionCount = 0;
    memset(m_extensions, 0, sizeof(m_extensions));
    memset(&m_stats, 0, sizeof(m_stats));

    m_initialized = true;
    OutputDebugStringA("[PluginSandbox] Initialized\n");
    return SandboxResult::ok("Plugin sandbox initialized");
}

void PluginSandbox::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return;

    // Deactivate all extensions
    for (uint32_t i = 0; i < m_extensionCount; i++) {
        m_extensions[i].active = false;
    }

    m_initialized = false;
    OutputDebugStringA("[PluginSandbox] Shutdown\n");
}

// ============================================================================
// Default Configuration Factory
// ============================================================================
SandboxConfig PluginSandbox::createDefaultConfig(SecurityTier tier) {
    SandboxConfig config;
    memset(&config, 0, sizeof(config));
    config.tier = tier;

    switch (tier) {
        case SecurityTier::Untrusted:
            config.maxMemoryBytes = 16 * 1024 * 1024;   // 16MB
            config.maxStackBytes  = 256 * 1024;           // 256KB
            config.maxCpuMs       = 1000;                 // 1s per call
            config.maxTotalCpuMs  = 30000;                // 30s lifetime
            config.allowedPathCount = 0;
            config.allowedHostCount = 0;
            config.allowedNativeFuncCount = 0;
            config.killOnViolation = true;
            config.logViolations   = true;
            config.maxViolationsBeforeKill = 1;
            break;

        case SecurityTier::Sandboxed:
            config.maxMemoryBytes = 64 * 1024 * 1024;   // 64MB
            config.maxStackBytes  = 1024 * 1024;          // 1MB
            config.maxCpuMs       = 5000;                 // 5s per call
            config.maxTotalCpuMs  = 300000;               // 5min lifetime
            config.killOnViolation = false;
            config.logViolations   = true;
            config.maxViolationsBeforeKill = 5;
            break;

        case SecurityTier::Trusted:
            config.maxMemoryBytes = 256 * 1024 * 1024;  // 256MB
            config.maxStackBytes  = 8 * 1024 * 1024;     // 8MB
            config.maxCpuMs       = 30000;                // 30s per call
            config.maxTotalCpuMs  = 0;                    // No lifetime limit
            config.killOnViolation = false;
            config.logViolations   = true;
            config.maxViolationsBeforeKill = 20;
            break;

        case SecurityTier::Privileged:
            config.maxMemoryBytes = 0;                   // No limit
            config.maxStackBytes  = 0;                    // No limit
            config.maxCpuMs       = 0;                    // No limit
            config.maxTotalCpuMs  = 0;                    // No limit
            config.killOnViolation = false;
            config.logViolations   = true;
            config.maxViolationsBeforeKill = 0;           // Never kill
            break;
    }

    return config;
}

void PluginSandbox::addStandardAPIWhitelist(SandboxConfig& config) {
    // Standard safe RawrXD API functions available to extensions
    const char* safeFuncs[] = {
        "rawrxd.getVersion",
        "rawrxd.getEdition",
        "rawrxd.log",
        "rawrxd.debug",
        "rawrxd.warn",
        "rawrxd.error",
        "rawrxd.getConfig",
        "rawrxd.getTheme",
        "rawrxd.showMessage",
        "rawrxd.showStatusBar",
        "rawrxd.createPanel",
        "rawrxd.registerCommand",
        "rawrxd.getOpenFiles",
        "rawrxd.getActiveFile",
        "rawrxd.getSelectedText",
        "rawrxd.insertText",
        "rawrxd.replaceSelection",
        "rawrxd.getDiagnostics",
        "rawrxd.getWorkspaceFolder",
        "rawrxd.onDidSaveTextDocument",
        "rawrxd.onDidOpenTextDocument",
        "rawrxd.onDidCloseTextDocument",
        "rawrxd.onDidChangeActiveEditor",
    };

    for (size_t i = 0; i < sizeof(safeFuncs) / sizeof(safeFuncs[0]); i++) {
        if (config.allowedNativeFuncCount >= SANDBOX_MAX_NATIVE_FUNCS) break;
        strncpy_s(config.allowedNativeFuncs[config.allowedNativeFuncCount].name,
                   sizeof(config.allowedNativeFuncs[0].name),
                   safeFuncs[i], _TRUNCATE);
        config.allowedNativeFuncCount++;
    }
}

// ============================================================================
// Extension Registration
// ============================================================================
int PluginSandbox::findExtension(const char* extensionId) const {
    if (!extensionId) return -1;
    for (uint32_t i = 0; i < m_extensionCount; i++) {
        if (strcmp(m_extensions[i].id, extensionId) == 0 && m_extensions[i].active) {
            return (int)i;
        }
    }
    return -1;
}

SandboxResult PluginSandbox::registerExtension(const char* extensionId,
                                                 SecurityTier tier)
{
    SandboxConfig config = createDefaultConfig(tier);

    // Add standard API whitelist for Sandboxed and above
    if (tier >= SecurityTier::Sandboxed) {
        addStandardAPIWhitelist(config);
    }

    return registerExtensionWithConfig(extensionId, config);
}

SandboxResult PluginSandbox::registerExtensionWithConfig(const char* extensionId,
                                                           const SandboxConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return SandboxResult::error("Sandbox not initialized");
    if (!extensionId)   return SandboxResult::error("Null extension ID");

    // Check for existing (reactivate if deactivated)
    int idx = findExtension(extensionId);
    if (idx >= 0) {
        m_extensions[idx].config = config;
        m_extensions[idx].memoryUsed = 0;
        m_extensions[idx].cpuTimeMs = 0;
        m_extensions[idx].violationCount = 0;
        return SandboxResult::ok("Extension re-registered");
    }

    // Find free slot
    if (m_extensionCount >= SANDBOX_MAX_EXTENSIONS) {
        return SandboxResult::error("Maximum extensions reached", -1);
    }

    ExtensionSlot& slot = m_extensions[m_extensionCount];
    memset(&slot, 0, sizeof(slot));
    strncpy_s(slot.id, sizeof(slot.id), extensionId, _TRUNCATE);
    slot.config = config;
    slot.active = true;
    slot.memoryUsed = 0;
    slot.cpuTimeMs = 0;
    slot.violationCount = 0;
    slot.violationHead = 0;

    m_extensionCount++;
    m_stats.totalExtensionsLoaded++;

    char dbg[256];
    snprintf(dbg, sizeof(dbg),
             "[PluginSandbox] Registered extension '%s' at tier %u\n",
             extensionId, (unsigned)config.tier);
    OutputDebugStringA(dbg);

    return SandboxResult::ok("Extension registered");
}

void PluginSandbox::unregisterExtension(const char* extensionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx >= 0) {
        m_extensions[idx].active = false;
        char dbg[256];
        snprintf(dbg, sizeof(dbg),
                 "[PluginSandbox] Unregistered extension '%s'\n", extensionId);
        OutputDebugStringA(dbg);
    }
}

const SandboxConfig* PluginSandbox::getConfig(const char* extensionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int idx = findExtension(extensionId);
    if (idx < 0) return nullptr;
    return &m_extensions[idx].config;
}

// ============================================================================
// Access Control
// ============================================================================
bool PluginSandbox::isNativeFuncAllowed(const char* extensionId, const char* funcName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    const SandboxConfig& cfg = m_extensions[idx].config;

    // Privileged tier has no restrictions
    if (cfg.tier == SecurityTier::Privileged) {
        m_stats.nativeCallsAllowed++;
        return true;
    }

    // Untrusted tier blocks all native calls
    if (cfg.tier == SecurityTier::Untrusted) {
        m_stats.nativeCallsBlocked++;
        recordViolation(extensionId, ViolationType::NativeFunctionBlocked, funcName);
        return false;
    }

    // Check whitelist
    for (uint32_t i = 0; i < cfg.allowedNativeFuncCount; i++) {
        if (strcmp(cfg.allowedNativeFuncs[i].name, funcName) == 0) {
            m_stats.nativeCallsAllowed++;
            return true;
        }
    }

    m_stats.nativeCallsBlocked++;
    recordViolation(extensionId, ViolationType::NativeFunctionBlocked, funcName);
    return false;
}

bool PluginSandbox::isFileAccessAllowed(const char* extensionId, const wchar_t* filePath,
                                          bool isWrite)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    const SandboxConfig& cfg = m_extensions[idx].config;

    if (cfg.tier == SecurityTier::Privileged) {
        m_stats.fsAccessAllowed++;
        return true;
    }

    if (cfg.tier == SecurityTier::Untrusted) {
        m_stats.fsAccessBlocked++;
        recordViolation(extensionId, ViolationType::FileAccessDenied, "FS blocked for untrusted");
        return false;
    }

    // Check whitelist
    for (uint32_t i = 0; i < cfg.allowedPathCount; i++) {
        // Prefix match: allowed if filePath starts with the whitelisted path
        size_t pathLen = wcslen(cfg.allowedPaths[i].path);
        if (wcsncmp(filePath, cfg.allowedPaths[i].path, pathLen) == 0) {
            // Check write permission
            if (isWrite && cfg.allowedPaths[i].readOnly) {
                m_stats.fsAccessBlocked++;
                recordViolation(extensionId, ViolationType::FileAccessDenied,
                                "Write to read-only path");
                return false;
            }
            m_stats.fsAccessAllowed++;
            return true;
        }
    }

    m_stats.fsAccessBlocked++;
    recordViolation(extensionId, ViolationType::FileAccessDenied, "Path not whitelisted");
    return false;
}

bool PluginSandbox::isNetworkAccessAllowed(const char* extensionId,
                                             const char* hostname, uint16_t port)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    const SandboxConfig& cfg = m_extensions[idx].config;

    if (cfg.tier == SecurityTier::Privileged) {
        m_stats.netAccessAllowed++;
        return true;
    }

    if (cfg.tier == SecurityTier::Untrusted || cfg.tier == SecurityTier::Sandboxed) {
        m_stats.netAccessBlocked++;
        recordViolation(extensionId, ViolationType::NetworkAccessDenied,
                        hostname ? hostname : "null");
        return false;
    }

    // Trusted tier: check whitelist
    for (uint32_t i = 0; i < cfg.allowedHostCount; i++) {
        if (_stricmp(cfg.allowedHosts[i].hostname, hostname) == 0) {
            if (cfg.allowedHosts[i].port == 0 || cfg.allowedHosts[i].port == port) {
                m_stats.netAccessAllowed++;
                return true;
            }
        }
    }

    m_stats.netAccessBlocked++;
    recordViolation(extensionId, ViolationType::NetworkAccessDenied,
                    hostname ? hostname : "null");
    return false;
}

bool PluginSandbox::isModuleImportAllowed(const char* extensionId, const char* moduleName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    const SandboxConfig& cfg = m_extensions[idx].config;

    if (cfg.tier == SecurityTier::Privileged) return true;

    // Block dangerous modules for non-privileged
    const char* blockedModules[] = {
        "os", "child_process", "fs", "net", "http", "https",
        "dgram", "cluster", "worker_threads", "vm", "v8",
        "process", "native", "ffi",
    };

    for (size_t i = 0; i < sizeof(blockedModules) / sizeof(blockedModules[0]); i++) {
        if (_stricmp(moduleName, blockedModules[i]) == 0) {
            recordViolation(extensionId, ViolationType::ModuleImportBlocked, moduleName);
            return false;
        }
    }

    return true; // Allow non-blocked modules
}

bool PluginSandbox::checkMemoryAllocation(const char* extensionId, uint64_t allocationSize) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    ExtensionSlot& slot = m_extensions[idx];

    if (slot.config.maxMemoryBytes == 0) return true; // No limit

    if (slot.memoryUsed + allocationSize > slot.config.maxMemoryBytes) {
        m_stats.memoryLimitHits++;
        recordViolation(extensionId, ViolationType::MemoryLimitExceeded,
                        "Allocation would exceed memory limit");
        return false;
    }

    slot.memoryUsed += allocationSize;
    return true;
}

bool PluginSandbox::checkCPUTime(const char* extensionId, uint32_t elapsedMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return false;

    ExtensionSlot& slot = m_extensions[idx];

    // Per-call limit
    if (slot.config.maxCpuMs > 0 && elapsedMs > slot.config.maxCpuMs) {
        m_stats.cpuLimitHits++;
        recordViolation(extensionId, ViolationType::CPUTimeLimitExceeded,
                        "Single call exceeded CPU time limit");
        return false;
    }

    // Lifetime limit
    slot.cpuTimeMs += elapsedMs;
    if (slot.config.maxTotalCpuMs > 0 && slot.cpuTimeMs > slot.config.maxTotalCpuMs) {
        m_stats.cpuLimitHits++;
        recordViolation(extensionId, ViolationType::CPUTimeLimitExceeded,
                        "Total CPU time limit exceeded");
        return false;
    }

    return true;
}

// ============================================================================
// Violation Handling
// ============================================================================
void PluginSandbox::setViolationCallback(ViolationCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violationCallback = callback;
    m_violationUserData = userData;
}

void PluginSandbox::recordViolation(const char* extensionId, ViolationType type,
                                      const char* detail)
{
    // NOTE: caller must hold m_mutex

    int idx = findExtension(extensionId);
    if (idx < 0) return;

    ExtensionSlot& slot = m_extensions[idx];

    SandboxViolation v = SandboxViolation::create(type, extensionId, detail);

    // Store in ring buffer
    uint32_t ringIdx = slot.violationHead % 32;
    slot.violations[ringIdx] = v;
    slot.violationHead++;
    slot.violationCount++;
    m_stats.totalViolations++;

    // Log
    if (slot.config.logViolations) {
        char dbg[512];
        snprintf(dbg, sizeof(dbg),
                 "[PluginSandbox] VIOLATION: ext='%s' type=%u detail='%s'\n",
                 extensionId, (unsigned)type, detail ? detail : "");
        OutputDebugStringA(dbg);
    }

    // Notify callback
    if (m_violationCallback) {
        m_violationCallback(&v, m_violationUserData);
    }

    // Kill check
    if (slot.config.killOnViolation) {
        slot.active = false;
        m_stats.extensionsKilled++;
        char dbg[256];
        snprintf(dbg, sizeof(dbg),
                 "[PluginSandbox] Extension '%s' KILLED (killOnViolation)\n",
                 extensionId);
        OutputDebugStringA(dbg);
    } else if (slot.config.maxViolationsBeforeKill > 0 &&
               slot.violationCount >= slot.config.maxViolationsBeforeKill) {
        slot.active = false;
        m_stats.extensionsKilled++;
        char dbg[256];
        snprintf(dbg, sizeof(dbg),
                 "[PluginSandbox] Extension '%s' KILLED (max violations %u reached)\n",
                 extensionId, slot.config.maxViolationsBeforeKill);
        OutputDebugStringA(dbg);
    }
}

uint32_t PluginSandbox::getViolations(const char* extensionId,
                                        SandboxViolation* outViolations,
                                        uint32_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return 0;

    const ExtensionSlot& slot = m_extensions[idx];
    uint32_t count = 0;
    uint32_t total = (slot.violationHead < 32) ? slot.violationHead : 32;

    for (uint32_t i = 0; i < total && count < maxCount; i++) {
        outViolations[count++] = slot.violations[i];
    }
    return count;
}

uint32_t PluginSandbox::getViolationCount(const char* extensionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findExtension(extensionId);
    if (idx < 0) return 0;
    return m_extensions[idx].violationCount;
}

// ============================================================================
// Statistics
// ============================================================================
SandboxStats PluginSandbox::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void PluginSandbox::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

void PluginSandbox::dumpState() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[PluginSandbox] State: %u extensions, %llu violations, %llu killed\n",
             m_extensionCount,
             (unsigned long long)m_stats.totalViolations,
             (unsigned long long)m_stats.extensionsKilled);
    OutputDebugStringA(buf);

    for (uint32_t i = 0; i < m_extensionCount; i++) {
        const ExtensionSlot& s = m_extensions[i];
        if (!s.active) continue;
        snprintf(buf, sizeof(buf),
                 "  Extension '%s': tier=%u mem=%llu/%llu cpu=%u/%u violations=%u\n",
                 s.id,
                 (unsigned)s.config.tier,
                 (unsigned long long)s.memoryUsed,
                 (unsigned long long)s.config.maxMemoryBytes,
                 s.cpuTimeMs, s.config.maxTotalCpuMs,
                 s.violationCount);
        OutputDebugStringA(buf);
    }
}

} // namespace Sandbox
} // namespace RawrXD
