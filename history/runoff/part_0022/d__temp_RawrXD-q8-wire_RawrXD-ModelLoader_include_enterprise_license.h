// enterprise_license.h - RawrXD Enterprise License System
// Manages license creation, validation, tier enforcement, and feature gating
// Thread-safe singleton with HMAC-SHA256 key validation
// No exceptions. No STL allocators in hot path. PatchResult-style returns.

#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>
#include <chrono>

// ============================================================================
// License Tiers
// ============================================================================
enum class LicenseTier : uint8_t {
    Community    = 0,   // Free — basic model loading, single engine
    Professional = 1,   // Pro — hotpatching, multi-model, telemetry
    Enterprise   = 2,   // Enterprise — all features, dual-engine, 800B, agentic
    Sovereign    = 3    // Sovereign — air-gapped, custom signing, unlimited seats
};

inline const char* LicenseTierToString(LicenseTier tier) {
    switch (tier) {
        case LicenseTier::Community:    return "Community";
        case LicenseTier::Professional: return "Professional";
        case LicenseTier::Enterprise:   return "Enterprise";
        case LicenseTier::Sovereign:    return "Sovereign";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Enterprise Feature IDs — every gatable feature in the system
// ============================================================================
enum class EnterpriseFeature : uint32_t {
    // --- Core Engine (Community) ---
    BasicModelLoading       = 0x0001,
    SingleEngineInference   = 0x0002,
    BasicChat               = 0x0003,
    FileExplorer            = 0x0004,
    TerminalAccess          = 0x0005,
    BasicEditor             = 0x0006,

    // --- Professional Tier ---
    MemoryHotpatching       = 0x0100,
    ByteLevelHotpatching    = 0x0101,
    ServerHotpatching       = 0x0102,
    UnifiedHotpatchManager  = 0x0103,
    ProxyHotpatcher         = 0x0104,
    OllamaHotpatchProxy     = 0x0105,
    MultiModelQueue         = 0x0106,
    ModelMonitor            = 0x0107,
    TelemetryCollector      = 0x0108,
    MetricsCollector        = 0x0109,
    FlashAttention          = 0x010A,
    KVCacheOptimization     = 0x010B,
    QuantUtils              = 0x010C,
    StreamingInference      = 0x010D,
    GPUBackendSelector      = 0x010E,
    VulkanCompute           = 0x010F,
    CUDABackend             = 0x0110,
    HIPBackend              = 0x0111,
    CommandPalette          = 0x0112,
    SyntaxHighlighting      = 0x0113,
    GitIntegration          = 0x0114,

    // --- Enterprise Tier ---
    DualEngineInference     = 0x0200,   // 800B Dual-Engine
    Engine800B              = 0x0201,   // 800B model support
    AgenticKernel           = 0x0202,
    AgenticFailureDetector  = 0x0203,
    AgenticPuppeteer        = 0x0204,
    AgenticSelfCorrector    = 0x0205,
    AgentOrchestrator       = 0x0206,
    MCPServerHost           = 0x0207,
    IDEAgentBridge          = 0x0208,
    ToolExecutionEngine     = 0x0209,
    WorkspaceIndexer        = 0x020A,
    PredictiveCommandKernel = 0x020B,
    SelfManifestor          = 0x020C,
    MMFProducer             = 0x020D,
    HotpatchEngine          = 0x020E,
    AutoBootstrap           = 0x020F,
    HotReload               = 0x0210,
    SelfTestGate            = 0x0211,
    SelfPatch               = 0x0212,
    SelfCode                = 0x0213,
    ZeroTouch               = 0x0214,
    DistributedTraining     = 0x0215,
    SecurityManager         = 0x0216,
    ComplianceLogger        = 0x0217,
    SLAManager              = 0x0218,
    BackupManager           = 0x0219,
    AICodeAssistant         = 0x021A,
    AISwitcher              = 0x021B,
    AgentModeHandler        = 0x021C,
    PlanModeHandler         = 0x021D,
    AskModeHandler          = 0x021E,
    SemanticDiffAnalyzer    = 0x021F,
    AIMergeResolver         = 0x0220,
    MultiFileBatchEdit      = 0x0221,
    SchematicStudio         = 0x0222,
    WiringOracle            = 0x0223,
    ObservabilityDashboard  = 0x0224,
    OverclockGovernor       = 0x0225,
    CICDIntegration         = 0x0226,
    CodeSigner              = 0x0227,
    ReleaseAgent            = 0x0228,
    Rollback                = 0x0229,

    // --- Sovereign Tier ---
    AirGappedMode           = 0x0300,
    CustomSigningKeys       = 0x0301,
    UnlimitedSeats          = 0x0302,
    OnPremTelemetry         = 0x0303,
    SovereignAuditLog       = 0x0304,
    HardwareSecurityModule  = 0x0305,
    FIPSCompliance          = 0x0306,
    CustomModelTraining     = 0x0307,

    _COUNT
};

inline const char* EnterpriseFeatureToString(EnterpriseFeature f) {
    switch (f) {
        case EnterpriseFeature::BasicModelLoading:       return "Basic Model Loading";
        case EnterpriseFeature::SingleEngineInference:   return "Single Engine Inference";
        case EnterpriseFeature::BasicChat:               return "Basic Chat";
        case EnterpriseFeature::FileExplorer:            return "File Explorer";
        case EnterpriseFeature::TerminalAccess:          return "Terminal Access";
        case EnterpriseFeature::BasicEditor:             return "Basic Editor";
        case EnterpriseFeature::MemoryHotpatching:       return "Memory Hotpatching";
        case EnterpriseFeature::ByteLevelHotpatching:    return "Byte-Level Hotpatching";
        case EnterpriseFeature::ServerHotpatching:       return "Server Hotpatching";
        case EnterpriseFeature::UnifiedHotpatchManager:  return "Unified Hotpatch Manager";
        case EnterpriseFeature::ProxyHotpatcher:         return "Proxy Hotpatcher";
        case EnterpriseFeature::OllamaHotpatchProxy:     return "Ollama Hotpatch Proxy";
        case EnterpriseFeature::MultiModelQueue:         return "Multi-Model Queue";
        case EnterpriseFeature::ModelMonitor:            return "Model Monitor";
        case EnterpriseFeature::TelemetryCollector:      return "Telemetry Collector";
        case EnterpriseFeature::MetricsCollector:        return "Metrics Collector";
        case EnterpriseFeature::FlashAttention:          return "Flash Attention";
        case EnterpriseFeature::KVCacheOptimization:     return "KV Cache Optimization";
        case EnterpriseFeature::QuantUtils:              return "Quantization Utilities";
        case EnterpriseFeature::StreamingInference:      return "Streaming Inference";
        case EnterpriseFeature::GPUBackendSelector:      return "GPU Backend Selector";
        case EnterpriseFeature::VulkanCompute:           return "Vulkan Compute";
        case EnterpriseFeature::CUDABackend:             return "CUDA Backend";
        case EnterpriseFeature::HIPBackend:              return "HIP/ROCm Backend";
        case EnterpriseFeature::CommandPalette:          return "Command Palette";
        case EnterpriseFeature::SyntaxHighlighting:      return "Syntax Highlighting";
        case EnterpriseFeature::GitIntegration:          return "Git Integration";
        case EnterpriseFeature::DualEngineInference:     return "800B Dual-Engine Inference";
        case EnterpriseFeature::Engine800B:              return "800B Model Support";
        case EnterpriseFeature::AgenticKernel:           return "Agentic Kernel";
        case EnterpriseFeature::AgenticFailureDetector:  return "Agentic Failure Detector";
        case EnterpriseFeature::AgenticPuppeteer:        return "Agentic Puppeteer";
        case EnterpriseFeature::AgenticSelfCorrector:    return "Agentic Self-Corrector";
        case EnterpriseFeature::AgentOrchestrator:       return "Agent Orchestrator";
        case EnterpriseFeature::MCPServerHost:           return "MCP Server Host";
        case EnterpriseFeature::IDEAgentBridge:          return "IDE Agent Bridge";
        case EnterpriseFeature::ToolExecutionEngine:     return "Tool Execution Engine";
        case EnterpriseFeature::WorkspaceIndexer:        return "Workspace Indexer";
        case EnterpriseFeature::PredictiveCommandKernel: return "Predictive Command Kernel";
        case EnterpriseFeature::SelfManifestor:          return "Self-Manifestor";
        case EnterpriseFeature::MMFProducer:             return "MMF Producer";
        case EnterpriseFeature::HotpatchEngine:          return "Hotpatch Engine (ASM)";
        case EnterpriseFeature::AutoBootstrap:           return "Auto-Bootstrap";
        case EnterpriseFeature::HotReload:               return "Hot Reload";
        case EnterpriseFeature::SelfTestGate:            return "Self-Test Gate";
        case EnterpriseFeature::SelfPatch:               return "Self-Patch";
        case EnterpriseFeature::SelfCode:                return "Self-Code";
        case EnterpriseFeature::ZeroTouch:               return "Zero-Touch Deployment";
        case EnterpriseFeature::DistributedTraining:     return "Distributed Training";
        case EnterpriseFeature::SecurityManager:         return "Security Manager";
        case EnterpriseFeature::ComplianceLogger:        return "Compliance Logger";
        case EnterpriseFeature::SLAManager:              return "SLA Manager";
        case EnterpriseFeature::BackupManager:           return "Backup Manager";
        case EnterpriseFeature::AICodeAssistant:         return "AI Code Assistant";
        case EnterpriseFeature::AISwitcher:              return "AI Switcher";
        case EnterpriseFeature::AgentModeHandler:        return "Agent Mode Handler";
        case EnterpriseFeature::PlanModeHandler:         return "Plan Mode Handler";
        case EnterpriseFeature::AskModeHandler:          return "Ask Mode Handler";
        case EnterpriseFeature::SemanticDiffAnalyzer:    return "Semantic Diff Analyzer";
        case EnterpriseFeature::AIMergeResolver:         return "AI Merge Resolver";
        case EnterpriseFeature::MultiFileBatchEdit:      return "Multi-File Batch Edit";
        case EnterpriseFeature::SchematicStudio:         return "Schematic Studio";
        case EnterpriseFeature::WiringOracle:            return "Wiring Oracle";
        case EnterpriseFeature::ObservabilityDashboard:  return "Observability Dashboard";
        case EnterpriseFeature::OverclockGovernor:       return "Overclock Governor";
        case EnterpriseFeature::CICDIntegration:         return "CI/CD Integration";
        case EnterpriseFeature::CodeSigner:              return "Code Signer";
        case EnterpriseFeature::ReleaseAgent:            return "Release Agent";
        case EnterpriseFeature::Rollback:                return "Rollback";
        case EnterpriseFeature::AirGappedMode:           return "Air-Gapped Mode";
        case EnterpriseFeature::CustomSigningKeys:       return "Custom Signing Keys";
        case EnterpriseFeature::UnlimitedSeats:          return "Unlimited Seats";
        case EnterpriseFeature::OnPremTelemetry:         return "On-Premises Telemetry";
        case EnterpriseFeature::SovereignAuditLog:       return "Sovereign Audit Log";
        case EnterpriseFeature::HardwareSecurityModule:  return "Hardware Security Module";
        case EnterpriseFeature::FIPSCompliance:          return "FIPS Compliance";
        case EnterpriseFeature::CustomModelTraining:     return "Custom Model Training";
        default:                                         return "Unknown Feature";
    }
}

// ============================================================================
// License Result (No Exceptions)
// ============================================================================
struct LicenseResult {
    bool success = false;
    const char* detail = "";
    int errorCode = 0;

    static LicenseResult ok(const char* msg = "OK") {
        LicenseResult r; r.success = true; r.detail = msg; return r;
    }
    static LicenseResult error(const char* msg, int code = -1) {
        LicenseResult r; r.success = false; r.detail = msg; r.errorCode = code; return r;
    }
};

// ============================================================================
// License Key Structure
// ============================================================================
struct LicenseKey {
    std::string keyString;          // The serialized license key
    std::string licensee;           // Organization / user name
    std::string email;              // Contact email
    LicenseTier tier = LicenseTier::Community;
    uint32_t seatCount = 1;
    uint64_t issuedTimestamp = 0;   // Unix epoch seconds
    uint64_t expiryTimestamp = 0;   // 0 = perpetual
    std::string machineFingerprint; // Hardware-bound (optional)
    std::string signature;          // HMAC signature
    uint32_t featureOverrides = 0;  // Bitmask for individual feature grants

    bool isExpired() const;
    bool isPerpetual() const { return expiryTimestamp == 0; }
    std::string expiryDateString() const;
    std::string issuedDateString() const;
};

// ============================================================================
// Feature Manifest Entry — describes one feature for audit/display
// ============================================================================
struct FeatureManifestEntry {
    EnterpriseFeature id;
    const char* name;
    const char* description;
    LicenseTier requiredTier;
    bool implemented;               // Does the code exist?
    bool wired;                     // Is it connected to the build?
    bool tested;                    // Has it passed self-test?
    const char* sourceFile;         // Primary source file
    const char* headerFile;         // Primary header file
};

// ============================================================================
// Enterprise License Manager — Thread-safe Singleton
// ============================================================================
class EnterpriseLicenseManager {
public:
    static EnterpriseLicenseManager& getInstance();

    // --- License Key Operations ---
    LicenseResult initialize();
    LicenseResult loadLicense(const std::string& filePath);
    LicenseResult saveLicense(const std::string& filePath) const;
    LicenseResult activateLicense(const std::string& keyString);
    LicenseResult deactivateLicense();
    LicenseResult validateLicense() const;

    // --- License Key Creator ---
    static LicenseKey createLicenseKey(
        const std::string& licensee,
        const std::string& email,
        LicenseTier tier,
        uint32_t seatCount = 1,
        uint64_t validDays = 365,
        const std::string& machineFingerprint = ""
    );
    static std::string serializeLicenseKey(const LicenseKey& key);
    static LicenseKey deserializeLicenseKey(const std::string& serialized);
    static std::string signLicenseKey(const LicenseKey& key);
    static bool verifySignature(const LicenseKey& key);

    // --- Feature Gating ---
    bool isFeatureUnlocked(EnterpriseFeature feature) const;
    bool isFeatureLocked(EnterpriseFeature feature) const;
    LicenseTier getRequiredTier(EnterpriseFeature feature) const;
    LicenseTier getCurrentTier() const;
    const LicenseKey& getCurrentLicense() const;

    // --- Feature Manifest / Audit ---
    std::vector<FeatureManifestEntry> getFullManifest() const;
    std::vector<FeatureManifestEntry> getUnlockedFeatures() const;
    std::vector<FeatureManifestEntry> getLockedFeatures() const;
    std::vector<FeatureManifestEntry> getMissingFeatures() const;
    std::vector<FeatureManifestEntry> getImplementedFeatures() const;
    std::string generateAuditReport() const;
    std::string generateFeatureMatrix() const;

    // --- Machine Fingerprint ---
    static std::string generateMachineFingerprint();

    // --- Callbacks ---
    using LicenseChangeCallback = std::function<void(LicenseTier oldTier, LicenseTier newTier)>;
    void registerLicenseChangeCallback(LicenseChangeCallback cb);

    // --- Utility ---
    uint64_t daysUntilExpiry() const;
    bool isTrialExpired() const;
    std::string getLicenseSummary() const;

    ~EnterpriseLicenseManager();
    EnterpriseLicenseManager(const EnterpriseLicenseManager&) = delete;
    EnterpriseLicenseManager& operator=(const EnterpriseLicenseManager&) = delete;

private:
    EnterpriseLicenseManager();
    void buildFeatureManifest();
    void buildTierMap();

    LicenseKey m_currentLicense;
    std::map<EnterpriseFeature, FeatureManifestEntry> m_manifest;
    std::map<EnterpriseFeature, LicenseTier> m_tierMap;
    std::vector<LicenseChangeCallback> m_callbacks;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_licensed{false};
};

// ============================================================================
// Convenience Macros for Feature Gating
// ============================================================================
#define RAWRXD_FEATURE_CHECK(feature) \
    EnterpriseLicenseManager::getInstance().isFeatureUnlocked(EnterpriseFeature::feature)

#define RAWRXD_FEATURE_GUARD(feature, action_if_locked) \
    do { \
        if (!RAWRXD_FEATURE_CHECK(feature)) { \
            action_if_locked; \
        } \
    } while(0)

#define RAWRXD_REQUIRE_ENTERPRISE(feature) \
    RAWRXD_FEATURE_GUARD(feature, return LicenseResult::error("Feature requires Enterprise license: " #feature))

#define RAWRXD_REQUIRE_TIER(feature, retval) \
    RAWRXD_FEATURE_GUARD(feature, return retval)
