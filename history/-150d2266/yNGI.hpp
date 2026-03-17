// ============================================================================
// enterprise_feature_manager.hpp — Unified Enterprise Feature Manager (Non-Qt)
// ============================================================================
// Central hub for all enterprise feature gating, display, status tracking,
// and license integration. Replaces the old Qt-based version with a pure
// C++20 implementation backed by EnterpriseLicense + FeatureRegistry.
//
// Architecture:
//   EnterpriseFeatureManager::Instance()
//    ├─ EnterpriseLicense (MASM/C++ license backend)
//    ├─ FeatureRegistry (Phase 31 audit system)
//    └─ Feature gate checks (composable with LicenseGuard)
//
// 8 Enterprise Features (bitmask 0x01–0x80):
//   0x01  800B Dual-Engine         — Multi-shard 800B model inference
//   0x02  AVX-512 Premium          — Hardware-accelerated quant kernels
//   0x04  Distributed Swarm        — Multi-node swarm inference
//   0x08  GPU Quant 4-bit          — GPU-accelerated Q4 quantization
//   0x10  Enterprise Support       — Priority support tier
//   0x20  Unlimited Context        — 200K token context window
//   0x40  Flash Attention          — AVX-512 flash attention kernels
//   0x80  Multi-GPU                — Multi-GPU inference distribution
//
// PATTERN:   No exceptions. Returns PatchResult/bool status codes.
// THREADING: Singleton with std::mutex. Thread-safe.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>

// Forward declarations — actual includes in .cpp
namespace RawrXD {
    class EnterpriseLicense;
    enum class LicenseState : uint32_t;
    enum class EnterpriseFeature : uint64_t;
}

// ============================================================================
// Feature Definition — describes one gated enterprise feature
// ============================================================================
struct EnterpriseFeatureDef {
    uint64_t    mask;               // Bitmask (0x01–0x80)
    const char* name;               // Human-readable name
    const char* description;        // Brief description
    const char* gateLocation;       // Source file(s) where feature is gated
    const char* uiLocation;         // Where feature appears in the UI
    const char* phase;              // Phase identifier
    bool        implemented;        // Has real implementation behind the gate
    bool        wiredToUI;          // Connected to menu/dialog/REPL
    bool        wiredToBackend;     // Connected to engine/runtime gating
    float       completionPct;      // 0.0–1.0 completion estimate
};

// ============================================================================
// Feature Status — runtime state of one enterprise feature
// ============================================================================
struct EnterpriseFeatureStatus {
    uint64_t    mask;
    const char* name;
    bool        licensed;           // Current license grants this feature
    bool        available;          // Feature code is compiled in
    bool        active;             // Feature is currently running/enabled
    const char* statusText;         // Human-readable status string
};

// ============================================================================
// License Tier — edition classification
// ============================================================================
#ifndef RAWRXD_LICENSE_TIER_DEFINED
#define RAWRXD_LICENSE_TIER_DEFINED
enum class LicenseTier {
    Community      = 0,    // Free — 70B models, 32K context
    Trial          = 1,    // 30-day — 180B models, 128K context
    Pro            = 2,    // Paid — 800B + AVX-512 + Flash Attention
    Professional   = 2,    // Alias for Pro (telemetry compat)
    Enterprise     = 3,    // Full — all features, unlimited
    OEM            = 4,    // Partner — customizable
};
#endif

// ============================================================================
// Audit Entry — per-feature audit result
// ============================================================================
struct FeatureAuditEntry {
    uint64_t    mask;
    const char* name;
    bool        hasHeader;          // Header file exists with declarations
    bool        hasCppImpl;         // .cpp implementation exists
    bool        hasAsmImpl;         // .asm kernel exists
    bool        hasLicenseGate;     // License check guards the feature
    bool        hasMenuWiring;      // Connected to Win32 menu
    bool        hasREPLCommand;     // Connected to CLI/REPL
    bool        hasAPIEndpoint;     // Connected to HTTP API
    bool        registeredInFeatureRegistry; // Registered with Phase 31 audit
    float       completionPct;      // Overall completion estimate
    const char* missingItems;       // What's still needed
};

// ============================================================================
// Enterprise Feature Manager — Singleton
// ============================================================================
class EnterpriseFeatureManager {
public:
    // Singleton access
    static EnterpriseFeatureManager& Instance();

    // Non-copyable
    EnterpriseFeatureManager(const EnterpriseFeatureManager&) = delete;
    EnterpriseFeatureManager& operator=(const EnterpriseFeatureManager&) = delete;

    // ---- Initialization ----

    /// Initialize the feature manager. Call after EnterpriseLicense::Initialize().
    /// Registers all 8 features, wires license callbacks, registers with FeatureRegistry.
    bool Initialize();

    /// Shutdown and cleanup.
    void Shutdown();

    /// Returns true if Initialize() has been called.
    bool IsInitialized() const { return m_initialized; }

    // ---- Feature Queries ----

    /// Check if a specific feature is licensed AND available.
    bool IsFeatureEnabled(uint64_t featureMask) const;

    /// Check if a feature is licensed (ignoring compilation availability).
    bool IsFeatureLicensed(uint64_t featureMask) const;

    /// Check if a feature's implementation is compiled in.
    bool IsFeatureAvailable(uint64_t featureMask) const;

    /// Get all 8 feature definitions.
    const std::vector<EnterpriseFeatureDef>& GetFeatureDefinitions() const;

    /// Get runtime status of all features.
    std::vector<EnterpriseFeatureStatus> GetFeatureStatuses() const;

    /// Get current license tier.
    LicenseTier GetCurrentTier() const;

    /// Get tier name string.
    static const char* GetTierName(LicenseTier tier);

    // ---- Audit ----

    /// Run full audit of all enterprise features — checks headers, impls,
    /// license gates, menu wiring, REPL commands, API endpoints, and
    /// FeatureRegistry integration.
    std::vector<FeatureAuditEntry> RunFullAudit() const;

    /// Generate a formatted text report of the audit.
    std::string GenerateAuditReport() const;

    /// Generate a dashboard-style summary (for REPL /license command).
    std::string GenerateDashboard() const;

    // ---- Feature Gating Helpers ----

    /// Gate a code path: returns true if feature is enabled, logs denial otherwise.
    bool Gate(uint64_t featureMask, const char* callerContext = nullptr) const;

    /// Get maximum model size in GB for current license.
    uint64_t GetMaxModelSizeGB() const;

    /// Get maximum context length for current license.
    uint64_t GetMaxContextLength() const;

    /// Get allocation budget limit in bytes.
    uint64_t GetAllocationBudget() const;

    // ---- License Operations (delegates to EnterpriseLicense) ----

    /// Get hardware ID as hex string.
    std::string GetHWIDString() const;

    /// Get current edition name.
    const char* GetEditionName() const;

    /// Get feature mask as hex string.
    std::string GetFeatureMaskString() const;

    /// Install license from file path.
    bool InstallLicenseFromFile(const std::string& path);

    /// Attempt dev unlock (requires RAWRXD_ENTERPRISE_DEV=1).
    bool DevUnlock();

    // ---- Callbacks ----

    using FeatureChangeCallback = void(*)(uint64_t oldMask, uint64_t newMask);

    /// Register a callback for when licensed features change.
    void OnFeatureChange(FeatureChangeCallback callback);

private:
    EnterpriseFeatureManager() = default;
    ~EnterpriseFeatureManager() = default;

    void buildFeatureDefinitions();
    void registerWithFeatureRegistry();
    void updateFeatureStatuses();

    bool m_initialized = false;
    mutable std::mutex m_mutex;
    std::vector<EnterpriseFeatureDef> m_definitions;
    std::vector<FeatureChangeCallback> m_callbacks;
    uint64_t m_lastFeatureMask = 0;
};

// ============================================================================
// Convenience macros for feature gating in source files
// ============================================================================
#define ENTERPRISE_GATE(mask) \
    if (!EnterpriseFeatureManager::Instance().Gate(mask, __FUNCTION__)) return

#define ENTERPRISE_GATE_BOOL(mask) \
    if (!EnterpriseFeatureManager::Instance().Gate(mask, __FUNCTION__)) return false

#define ENTERPRISE_GATE_NULL(mask) \
    if (!EnterpriseFeatureManager::Instance().Gate(mask, __FUNCTION__)) return nullptr

#define ENTERPRISE_GATE_STR(mask) \
    if (!EnterpriseFeatureManager::Instance().Gate(mask, __FUNCTION__)) return std::string("[Enterprise] Feature requires upgrade")
