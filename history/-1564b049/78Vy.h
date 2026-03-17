// ============================================================================
// enterprise_license.h — Enterprise License System v2: 4-Tier, 55+ Features
// ============================================================================
// Supersedes the Phase 22 manifest (8 features) with a comprehensive
// enterprise licensing system covering all IDE subsystems.
//
// Architecture:
//   EnterpriseLicenseV2::Instance()
//    ├─ LicenseTierV2        (Community / Professional / Enterprise / Sovereign)
//    ├─ FeatureID             (55+ enumerated features)
//    ├─ LicenseKeyV2          (signed key with HWID binding)
//    ├─ FeatureManifestV2     (compile-time feature definitions)
//    └─ AuditTrailV2          (enforcement audit logging)
//
// 4 License Tiers:
//   Community    — Free: 6 features (basic GGUF loading, CPU inference)
//   Professional — Paid: 21 features (hotpatching, multi-model, quant)
//   Enterprise   — Full: 42 features (800B, agentic, sharding, telemetry)
//   Sovereign    — Gov:  55+ features (air-gap, HSM, FIPS, tamper detection)
//
// PATTERN:   No exceptions. Returns bool/status codes.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>

namespace RawrXD::License {

// ============================================================================
// License Tier — 4-tier classification
// ============================================================================
enum class LicenseTierV2 : uint32_t {
    Community    = 0,   // Free — basic features only
    Professional = 1,   // Paid — hotpatching, multi-model, quant tiers
    Enterprise   = 2,   // Full — 800B, agentic, sharding, observability
    Sovereign    = 3,   // Gov  — air-gap, HSM, FIPS, classified network
    COUNT        = 4
};

// ============================================================================
// Feature ID — enumerated features (55+ total)
// ============================================================================
// Organized by tier. Each tier includes all features from lower tiers.
// Use uint64_t pairs (lo/hi) for bitmask storage (supports up to 128 features).
enum class FeatureID : uint32_t {
    // ── Community Tier (0–5) ─────────────────────────────────────
    BasicGGUFLoading        = 0,
    Q4Quantization          = 1,    // Q4_0 / Q4_1
    CPUInference            = 2,
    BasicChatUI             = 3,
    ConfigFileSupport       = 4,
    SingleModelSession      = 5,

    // ── Professional Tier (6–26) ─────────────────────────────────
    MemoryHotpatching       = 6,
    ByteLevelHotpatching    = 7,
    ServerHotpatching       = 8,
    UnifiedHotpatchManager  = 9,
    Q5Q8F16Quantization     = 10,
    MultiModelLoading       = 11,
    CUDABackend             = 12,
    AdvancedSettingsPanel   = 13,
    PromptTemplates         = 14,
    TokenStreaming           = 15,
    InferenceStatistics     = 16,
    KVCacheManagement       = 17,
    ModelComparison         = 18,
    BatchProcessing         = 19,
    CustomStopSequences     = 20,
    GrammarConstrainedGen   = 21,
    LoRAAdapterSupport      = 22,
    ResponseCaching         = 23,
    PromptLibrary           = 24,
    ExportImportSessions    = 25,
    HIPBackend              = 26,

    // ── Enterprise Tier (27–52) ──────────────────────────────────
    DualEngine800B          = 27,
    AgenticFailureDetect    = 28,
    AgenticPuppeteer        = 29,
    AgenticSelfCorrection   = 30,
    ProxyHotpatching        = 31,
    ServerSidePatching      = 32,
    SchematicStudioIDE      = 33,
    WiringOracleDebug       = 34,
    FlashAttention          = 35,
    SpeculativeDecoding     = 36,
    ModelSharding           = 37,
    TensorParallel          = 38,
    PipelineParallel        = 39,
    ContinuousBatching      = 40,
    GPTQQuantization        = 41,
    AWQQuantization         = 42,
    CustomQuantSchemes      = 43,
    MultiGPULoadBalance     = 44,
    DynamicBatchSizing      = 45,
    PriorityQueuing         = 46,
    RateLimitingEngine      = 47,
    AuditLogging            = 48,
    APIKeyManagement        = 49,
    ModelSigningVerify      = 50,
    RBAC                    = 51,
    ObservabilityDashboard  = 52,
    AVX512Acceleration      = 53,
    RawrTunerIDE            = 54,

    // ── Sovereign Tier (55–64) ───────────────────────────────────
    AirGappedDeploy         = 55,
    HSMIntegration          = 56,
    FIPS140_2Compliance     = 57,
    CustomSecurityPolicies  = 58,
    SovereignKeyMgmt        = 59,
    ClassifiedNetwork       = 60,
    ImmutableAuditLogs      = 61,
    KubernetesSupport       = 62,
    TamperDetection         = 63,
    SecureBootChain         = 64,

    // Sentinel
    COUNT                   = 65
};

// ============================================================================
// Feature Bitmask — dual uint64_t for 128-bit capacity
// ============================================================================
struct FeatureMask {
    uint64_t lo;    // Features 0–63
    uint64_t hi;    // Features 64–127

    constexpr FeatureMask() : lo(0), hi(0) {}
    constexpr FeatureMask(uint64_t l, uint64_t h) : lo(l), hi(h) {}

    constexpr void set(uint32_t bit) {
        if (bit < 64) lo |= (1ULL << bit);
        else          hi |= (1ULL << (bit - 64));
    }

    constexpr bool test(uint32_t bit) const {
        if (bit < 64) return (lo & (1ULL << bit)) != 0;
        else          return (hi & (1ULL << (bit - 64))) != 0;
    }

    constexpr void clear(uint32_t bit) {
        if (bit < 64) lo &= ~(1ULL << bit);
        else          hi &= ~(1ULL << (bit - 64));
    }

    constexpr FeatureMask operator|(const FeatureMask& o) const {
        return { lo | o.lo, hi | o.hi };
    }

    constexpr FeatureMask operator&(const FeatureMask& o) const {
        return { lo & o.lo, hi & o.hi };
    }

    constexpr bool operator==(const FeatureMask& o) const {
        return lo == o.lo && hi == o.hi;
    }

    constexpr bool operator!=(const FeatureMask& o) const {
        return lo != o.lo || hi != o.hi;
    }

    constexpr bool empty() const { return lo == 0 && hi == 0; }

    uint32_t popcount() const {
        uint32_t count = 0;
        uint64_t v = lo;
        while (v) { count++; v &= v - 1; }
        v = hi;
        while (v) { count++; v &= v - 1; }
        return count;
    }
};

// ============================================================================
// Tier Mask Presets — which features each tier unlocks
// ============================================================================
namespace TierPresets {

    // Community: bits 0–5
    constexpr FeatureMask Community() {
        FeatureMask m;
        for (uint32_t i = 0; i <= 5; ++i) m.set(i);
        return m;
    }

    // Professional: bits 0–26
    constexpr FeatureMask Professional() {
        FeatureMask m;
        for (uint32_t i = 0; i <= 26; ++i) m.set(i);
        return m;
    }

    // Enterprise: bits 0–54
    constexpr FeatureMask Enterprise() {
        FeatureMask m;
        for (uint32_t i = 0; i <= 54; ++i) m.set(i);
        return m;
    }

    // Sovereign: bits 0–64
    constexpr FeatureMask Sovereign() {
        FeatureMask m;
        for (uint32_t i = 0; i <= 64; ++i) m.set(i);
        return m;
    }

    // Get preset for tier
    inline FeatureMask forTier(LicenseTierV2 tier) {
        switch (tier) {
            case LicenseTierV2::Community:    return Community();
            case LicenseTierV2::Professional: return Professional();
            case LicenseTierV2::Enterprise:   return Enterprise();
            case LicenseTierV2::Sovereign:    return Sovereign();
            default:                          return Community();
        }
    }
}

// ============================================================================
// Feature Definition — describes one licenseable feature
// ============================================================================
struct FeatureDefV2 {
    FeatureID       id;
    const char*     name;
    const char*     description;
    LicenseTierV2   minTier;        // Minimum tier required
    bool            implemented;    // Has real code behind the gate
    bool            wiredToUI;      // Connected to Win32 menu/panel
    bool            tested;         // Has test coverage
    const char*     sourceFile;     // Primary implementation file
    const char*     phase;          // Phase identifier
};

// ============================================================================
// License Key V2 — signed binary key format
// ============================================================================
#pragma pack(push, 1)
struct LicenseKeyV2 {
    uint32_t    magic;              // 0x5258444C ("RXDL")
    uint16_t    version;            // Key format version (2)
    uint16_t    reserved;
    uint64_t    hwid;               // Hardware ID (MurmurHash3)
    FeatureMask features;           // Enabled feature bitmask
    uint32_t    tier;               // LicenseTierV2
    uint32_t    issueDate;          // Unix timestamp
    uint32_t    expiryDate;         // Unix timestamp (0 = perpetual)
    uint32_t    maxModelGB;         // Max model size in GB
    uint32_t    maxContextTokens;   // Max context window
    uint8_t     signature[32];      // SHA-256 HMAC signature
    uint8_t     padding[12];        // Align to 96 bytes
};
#pragma pack(pop)

static_assert(sizeof(LicenseKeyV2) == 96, "LicenseKeyV2 must be 96 bytes");

// ============================================================================
// Audit Entry — enforcement event record
// ============================================================================
struct LicenseAuditEntry {
    uint64_t    timestamp;          // Performance counter ticks
    FeatureID   feature;            // Which feature was checked
    bool        granted;            // Was access granted?
    const char* caller;             // __FUNCTION__ of the caller
    const char* detail;             // Additional context
};

// ============================================================================
// License Result — non-exception return type
// ============================================================================
struct LicenseResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static LicenseResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static LicenseResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Tier Limits — resource caps per tier
// ============================================================================
namespace TierLimits {
    struct Limits {
        uint32_t maxModelGB;
        uint32_t maxContextTokens;
        uint64_t maxMemoryBudget;
        uint32_t maxConcurrentModels;
    };

    constexpr Limits Community    = {   70,   32000, 4ULL * 1024 * 1024 * 1024,  1 };
    constexpr Limits Professional = {  400,  128000, 32ULL * 1024 * 1024 * 1024, 4 };
    constexpr Limits Enterprise   = {  800,  200000, UINT64_MAX,                 16 };
    constexpr Limits Sovereign    = { 2000,  500000, UINT64_MAX,                 64 };

    inline const Limits& forTier(LicenseTierV2 tier) {
        switch (tier) {
            case LicenseTierV2::Professional: return Professional;
            case LicenseTierV2::Enterprise:   return Enterprise;
            case LicenseTierV2::Sovereign:    return Sovereign;
            default:                          return Community;
        }
    }
}

// ============================================================================
// Convenience Helpers
// ============================================================================

/// Get human-readable tier name
inline const char* tierName(LicenseTierV2 tier) {
    switch (tier) {
        case LicenseTierV2::Community:    return "Community";
        case LicenseTierV2::Professional: return "Professional";
        case LicenseTierV2::Enterprise:   return "Enterprise";
        case LicenseTierV2::Sovereign:    return "Sovereign";
        default:                          return "Unknown";
    }
}

/// Get human-readable feature name
const char* featureName(FeatureID id);

/// Get total feature count
constexpr uint32_t TOTAL_FEATURES = static_cast<uint32_t>(FeatureID::COUNT);

// ============================================================================
// Feature Manifest — compile-time table of all 55+ features
// ============================================================================
extern const FeatureDefV2 g_FeatureManifest[TOTAL_FEATURES];

// ============================================================================
// Enterprise License V2 — Singleton runtime manager
// ============================================================================
class EnterpriseLicenseV2 {
public:
    static EnterpriseLicenseV2& Instance();

    // Non-copyable
    EnterpriseLicenseV2(const EnterpriseLicenseV2&) = delete;
    EnterpriseLicenseV2& operator=(const EnterpriseLicenseV2&) = delete;

    // ── Lifecycle ──
    LicenseResult initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── Feature Queries ──
    bool isFeatureEnabled(FeatureID id) const;
    bool isFeatureLicensed(FeatureID id) const;
    bool isFeatureImplemented(FeatureID id) const;
    bool gate(FeatureID id, const char* caller = nullptr);

    // ── Tier Queries ──
    LicenseTierV2 currentTier() const;
    const TierLimits::Limits& currentLimits() const;
    FeatureMask currentMask() const;
    uint32_t enabledFeatureCount() const;

    // ── Key Operations ──
    LicenseResult loadKeyFromFile(const char* path);
    LicenseResult loadKeyFromMemory(const void* data, size_t size);
    LicenseResult validateKey(const LicenseKeyV2& key) const;

    /// Create a new license key (requires signing secret)
    LicenseResult createKey(LicenseTierV2 tier, uint32_t durationDays,
                            const char* signingSecret, LicenseKeyV2* outKey) const;

    // ── HWID ──
    uint64_t getHardwareID() const;
    void getHardwareIDHex(char* buf, size_t bufLen) const;

    // ── Dev Unlock ──
    LicenseResult devUnlock();

    // ── Audit Trail ──
    static constexpr size_t MAX_AUDIT_ENTRIES = 4096;
    size_t getAuditEntryCount() const;
    const LicenseAuditEntry* getAuditEntries() const;
    void clearAuditTrail();

    // ── Manifest Queries ──
    const FeatureDefV2& getFeatureDef(FeatureID id) const;
    uint32_t countByTier(LicenseTierV2 tier) const;
    uint32_t countImplemented() const;
    uint32_t countWiredToUI() const;
    uint32_t countTested() const;

    // ── Callbacks ──
    using LicenseChangeCallback = void(*)(LicenseTierV2 oldTier, LicenseTierV2 newTier);
    void onLicenseChange(LicenseChangeCallback cb);

private:
    EnterpriseLicenseV2() = default;
    ~EnterpriseLicenseV2() = default;

    void recordAudit(FeatureID id, bool granted, const char* caller, const char* detail);
    LicenseResult computeHWID();
    void signKey(LicenseKeyV2& key, const char* secret) const;
    bool verifySignature(const LicenseKeyV2& key) const;

    mutable std::mutex  m_mutex;
    bool                m_initialized = false;
    LicenseTierV2       m_tier = LicenseTierV2::Community;
    FeatureMask         m_enabledFeatures;
    LicenseKeyV2        m_currentKey{};
    uint64_t            m_hwid = 0;

    // Audit ring buffer
    LicenseAuditEntry   m_auditTrail[MAX_AUDIT_ENTRIES]{};
    size_t              m_auditCount = 0;
    size_t              m_auditHead = 0;

    // Callbacks
    static constexpr size_t MAX_CALLBACKS = 16;
    LicenseChangeCallback m_callbacks[MAX_CALLBACKS]{};
    size_t              m_callbackCount = 0;
};

// ============================================================================
// Convenience Macros — gate code paths by feature
// ============================================================================
#define LICENSE_GATE(feature) \
    if (!RawrXD::License::EnterpriseLicenseV2::Instance().gate( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return

#define LICENSE_GATE_BOOL(feature) \
    if (!RawrXD::License::EnterpriseLicenseV2::Instance().gate( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return false

#define LICENSE_GATE_NULL(feature) \
    if (!RawrXD::License::EnterpriseLicenseV2::Instance().gate( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return nullptr

#define LICENSE_GATE_STR(feature) \
    if (!RawrXD::License::EnterpriseLicenseV2::Instance().gate( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) \
        return std::string("[License] Feature requires upgrade")

} // namespace RawrXD::License
