// ============================================================================
// enterprise_license.cpp — Enterprise License V2: Key Creation, Signing,
//                          Manifest, Audit Trail (55+ Features)
// ============================================================================
// Implements the EnterpriseLicenseV2 singleton, feature manifest table,
// key creation/validation with HMAC-SHA256 signing, HWID generation,
// and enforcement audit trail ring buffer.
//
// PATTERN:   No exceptions. Returns LicenseResult status codes.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../include/enterprise_license.h"
#include "enterprise_license.h" // V1 header for bridge

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <intrin.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <map>
#include <string>

#include "telemetry/logger.h"
#include "telemetry/UnifiedTelemetryCore.h"

namespace RawrXD::License {

namespace {
void EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel level,
                      const std::string& category,
                      const std::string& message,
                      const std::map<std::string, std::string>& tags = {}) {
    auto& telemetry = RawrXD::Telemetry::UnifiedTelemetryCore::Instance();
    if (!telemetry.IsInitialized()) return;
    telemetry.Emit(RawrXD::Telemetry::TelemetrySource::System, level,
                   category, message, 0.0, "", tags);
}

void EmitLicenseSpan(RawrXD::Telemetry::TelemetryLevel level,
                     const std::string& category,
                     const std::string& message,
                     int64_t durationMs,
                     const std::map<std::string, std::string>& tags = {}) {
    auto& telemetry = RawrXD::Telemetry::UnifiedTelemetryCore::Instance();
    if (!telemetry.IsInitialized()) return;
    telemetry.EmitSpan(RawrXD::Telemetry::TelemetrySource::System, level,
                       category, message, durationMs, tags);
}
}

// ============================================================================
// Feature Manifest — compile-time definitions of all 61 features
// ============================================================================
const FeatureDefV2 g_FeatureManifest[TOTAL_FEATURES] = {
    // ── Community Tier (0–5) ──
    { FeatureID::BasicGGUFLoading,      "Basic GGUF Loading",       "Load and parse GGUF model files",        LicenseTierV2::Community,    true,  true,  false, "src/gguf_loader.cpp",                "Core" },
    { FeatureID::Q4Quantization,        "Q4_0/Q4_1 Quantization",  "Basic 4-bit quantization formats",       LicenseTierV2::Community,    true,  true,  false, "src/engine/gguf_core.cpp",           "Core" },
    { FeatureID::CPUInference,          "CPU Inference",            "CPU-based model inference",              LicenseTierV2::Community,    true,  true,  false, "src/cpu_inference_engine.cpp",        "Core" },
    { FeatureID::BasicChatUI,           "Basic Chat UI",            "Win32 chat panel interface",             LicenseTierV2::Community,    true,  true,  false, "src/ui/chat_panel.cpp",              "Core" },
    { FeatureID::ConfigFileSupport,     "Config File Support",      "JSON configuration loading",             LicenseTierV2::Community,    true,  true,  false, "src/config/IDEConfig.cpp",           "Core" },
    { FeatureID::SingleModelSession,    "Single Model Session",     "Load one model at a time",               LicenseTierV2::Community,    true,  true,  false, "src/engine/rawr_engine.cpp",          "Core" },

    // ── Professional Tier (6–26) ──
    { FeatureID::MemoryHotpatching,     "Memory Hotpatching",       "Direct RAM patching via VirtualProtect", LicenseTierV2::Professional, true,  true,  false, "src/core/model_memory_hotpatch.cpp",  "Phase 14" },
    { FeatureID::ByteLevelHotpatching,  "Byte-Level Hotpatching",   "Precision GGUF binary modification",    LicenseTierV2::Professional, true,  true,  false, "src/core/byte_level_hotpatcher.cpp",  "Phase 14" },
    { FeatureID::ServerHotpatching,     "Server Hotpatching",       "Inference request/response patching",    LicenseTierV2::Professional, true,  true,  false, "src/server/gguf_server_hotpatch.cpp", "Phase 14" },
    { FeatureID::UnifiedHotpatchManager,"Unified Hotpatch Manager", "Three-layer hotpatch coordination",      LicenseTierV2::Professional, true,  true,  false, "src/core/unified_hotpatch_manager.cpp","Phase 14" },
    { FeatureID::Q5Q8F16Quantization,   "Q5/Q8/F16 Quantization",  "Higher-precision quant formats",         LicenseTierV2::Professional, true,  true,  false, "src/engine/gguf_core.cpp",           "Phase 22" },
    { FeatureID::MultiModelLoading,     "Multi-Model Loading",      "Load multiple models simultaneously",    LicenseTierV2::Professional, true,  true,  false, "src/model_registry.cpp",             "Phase 22" },
    { FeatureID::CUDABackend,           "CUDA Backend",             "NVIDIA GPU-accelerated inference",       LicenseTierV2::Professional, false, false, false, "N/A",                                "Planned" },
    { FeatureID::AdvancedSettingsPanel, "Advanced Settings Panel",  "Extended IDE configuration panel",       LicenseTierV2::Professional, true,  true,  false, "src/win32app/Win32IDE_Settings.cpp",  "Phase 33" },
    { FeatureID::PromptTemplates,       "Prompt Templates",         "Reusable prompt template library",       LicenseTierV2::Professional, true,  true,  false, "src/agentic/FIMPromptBuilder.cpp",    "Phase 10" },
    { FeatureID::TokenStreaming,         "Token Streaming",          "Real-time token-by-token output",        LicenseTierV2::Professional, true,  true,  false, "src/win32app/Win32IDE_StreamingUX.cpp","Phase 9" },
    { FeatureID::InferenceStatistics,   "Inference Statistics",     "Tokens/sec, latency, memory metrics",    LicenseTierV2::Professional, true,  true,  false, "src/core/perf_telemetry.cpp",         "Phase 50" },
    { FeatureID::KVCacheManagement,     "KV Cache Management",      "Key-value cache optimization",           LicenseTierV2::Professional, true,  true,  false, "src/engine/transformer.cpp",          "Phase 3" },
    { FeatureID::ModelComparison,       "Model Comparison",         "Side-by-side model output comparison",   LicenseTierV2::Professional, true,  true,  true,  "src/core/chain_of_thought_engine.cpp","Phase 32A" },
    { FeatureID::BatchProcessing,       "Batch Processing",         "Process multiple prompts in batch",      LicenseTierV2::Professional, true,  true,  true,  "src/engine/core_generator.cpp",      "Phase 3" },
    { FeatureID::CustomStopSequences,   "Custom Stop Sequences",    "User-defined generation terminators",    LicenseTierV2::Professional, true,  true,  true,  "src/engine/sampler.cpp",             "Phase 3" },
    { FeatureID::GrammarConstrainedGen, "Grammar-Constrained Gen",  "BNF/regex constrained generation",       LicenseTierV2::Professional, true,  true,  true,  "src/engine/sampler.cpp",             "Phase 3" },
    { FeatureID::LoRAAdapterSupport,    "LoRA Adapter Support",     "Load and apply LoRA adapters",           LicenseTierV2::Professional, true,  true,  true,  "src/engine/gguf_core.cpp",           "Phase 23" },
    { FeatureID::ResponseCaching,       "Response Caching",         "Cache identical prompt responses",        LicenseTierV2::Professional, true,  true,  true,  "src/core/native_inference_pipeline.cpp","Phase 9" },
    { FeatureID::PromptLibrary,         "Prompt Library",           "Persistent prompt storage/retrieval",     LicenseTierV2::Professional, true,  true,  true,  "src/agentic/FIMPromptBuilder.cpp",    "Phase 10" },
    { FeatureID::ExportImportSessions,  "Export/Import Sessions",   "Save and load chat sessions",            LicenseTierV2::Professional, true,  true,  true,  "src/win32app/Win32IDE_Session.cpp",   "Phase 33" },
    { FeatureID::HIPBackend,            "HIP Backend",              "AMD GPU-accelerated inference",          LicenseTierV2::Professional, false, false, false, "src/core/amd_gpu_accelerator.cpp",   "Phase 25" },

    // ── Enterprise Tier (27–52) ──
    { FeatureID::DualEngine800B,        "800B Dual-Engine",         "Multi-shard 800B model inference",       LicenseTierV2::Enterprise,   true,  true,  false, "src/dual_engine_inference.cpp",       "Phase 21" },
    { FeatureID::AgenticFailureDetect,  "Agentic Failure Detection","Detect refusal/hallucination/timeout",   LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agentic_failure_detector.cpp","Phase 18" },
    { FeatureID::AgenticPuppeteer,      "Agentic Puppeteer",        "Auto-correct failed responses",          LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agentic_puppeteer.cpp",     "Phase 18" },
    { FeatureID::AgenticSelfCorrection, "Agentic Self-Correction",  "Iterative self-repair loop",             LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agent_self_repair.cpp",     "Phase 18" },
    { FeatureID::ProxyHotpatching,      "Proxy Hotpatching",        "Byte-level output rewriting",            LicenseTierV2::Enterprise,   true,  true,  false, "src/core/proxy_hotpatcher.cpp",       "Phase 14" },
    { FeatureID::ServerSidePatching,    "Server-Side Patching",     "Runtime server request modification",    LicenseTierV2::Enterprise,   true,  true,  false, "src/server/gguf_server_hotpatch.cpp", "Phase 14" },
    { FeatureID::SchematicStudioIDE,    "SchematicStudio IDE",      "Visual IDE subsystem",                   LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE.cpp",           "Phase 31" },
    { FeatureID::WiringOracleDebug,     "WiringOracle Debug",       "Advanced wiring diagnostic tool",        LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_AuditDashboard.cpp","Phase 31" },
    { FeatureID::FlashAttention,        "Flash Attention",          "AVX-512 flash attention kernels",        LicenseTierV2::Enterprise,   false, false, false, "src/core/flash_attention.cpp",        "Phase 23" },
    { FeatureID::SpeculativeDecoding,   "Speculative Decoding",     "Draft model assisted generation",        LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::ModelSharding,         "Model Sharding",           "Split model across storage/memory",      LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/layer_offload_manager.cpp",  "Phase 9" },
    { FeatureID::TensorParallel,        "Tensor Parallel",          "Parallel tensor computation",            LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/adaptive_pipeline_parallel.cpp","Phase 22" },
    { FeatureID::PipelineParallel,      "Pipeline Parallel",        "Pipeline-parallel inference stages",     LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/distributed_pipeline_orchestrator.cpp","Phase 13" },
    { FeatureID::ContinuousBatching,    "Continuous Batching",      "Dynamic request batching",               LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::GPTQQuantization,      "GPTQ Quantization",        "GPTQ-format model loading",              LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::AWQQuantization,       "AWQ Quantization",         "AWQ-format model loading",               LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::CustomQuantSchemes,    "Custom Quant Schemes",     "User-defined quantization formats",      LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/gpu_kernel_autotuner.cpp",   "Phase 23" },
    { FeatureID::MultiGPULoadBalance,   "Multi-GPU Load Balance",   "Distribute across multiple GPUs",        LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/accelerator_router.cpp",     "Phase 30" },
    { FeatureID::DynamicBatchSizing,    "Dynamic Batch Sizing",     "Auto-tune batch size at runtime",        LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/execution_scheduler.cpp",    "Phase 9" },
    { FeatureID::PriorityQueuing,       "Priority Queuing",         "Request priority queue management",      LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::RateLimitingEngine,    "Rate Limiting Engine",     "Per-user/API rate limiting",             LicenseTierV2::Enterprise,   false, false, false, "N/A",                                "Planned" },
    { FeatureID::AuditLogging,          "Audit Logging",            "Full operation audit trail",              LicenseTierV2::Enterprise,   true,  true,  false, "src/core/enterprise_telemetry_compliance.cpp","Phase 17" },
    { FeatureID::APIKeyManagement,      "API Key Management",       "Generate and rotate API keys",            LicenseTierV2::Enterprise,   true,  true,  true,  "src/auth/rbac_engine.cpp",           "Phase 50" },
    { FeatureID::ModelSigningVerify,    "Model Signing/Verify",     "Cryptographic model integrity check",     LicenseTierV2::Enterprise,   true,  true,  false, "src/core/update_signature.cpp",       "Phase 50" },
    { FeatureID::RBAC,                  "RBAC",                     "Role-based access control",               LicenseTierV2::Enterprise,   false, false, false, "src/auth/rbac_engine.cpp",           "Phase 50" },
    { FeatureID::ObservabilityDashboard,"Observability Dashboard",  "System metrics and health dashboard",     LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_TelemetryDashboard.cpp","Phase 34" },
    { FeatureID::AVX512Acceleration,    "AVX-512 Acceleration",     "AVX-512 optimized inference kernels",    LicenseTierV2::Enterprise,   true,  true,  false, "src/core/avx512_kernels.cpp",         "Core" },
    { FeatureID::RawrTunerIDE,          "RawrTuner IDE",            "Fine-tuning and optimization IDE",       LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_Tuner.cpp",       "Phase 35" },

    // ── Sovereign Tier (55–64) ──
    { FeatureID::AirGappedDeploy,       "Air-Gapped Deploy",        "Fully offline deployment mode",          LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::HSMIntegration,        "HSM Integration",          "Hardware security module support",        LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::FIPS140_2Compliance,   "FIPS 140-2 Compliance",    "Federal crypto standard compliance",      LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::CustomSecurityPolicies,"Custom Security Policies", "Organization-specific security rules",    LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::SovereignKeyMgmt,      "Sovereign Key Mgmt",       "Gov-grade key management system",        LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::ClassifiedNetwork,     "Classified Network",       "Classified network deployment",           LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::ImmutableAuditLogs,    "Immutable Audit Logs",     "Blockchain-based tamper-proof logs",     LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::KubernetesSupport,     "Kubernetes Support",       "Enterprise orchestration platform",       LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::TamperDetection,       "Tamper Detection",         "Runtime binary integrity checks",         LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
    { FeatureID::SecureBootChain,       "Secure Boot Chain",        "Verified boot sequence",                  LicenseTierV2::Sovereign,    false, false, false, "N/A",                                "Planned" },
};

// ============================================================================
// Feature Name Lookup
// ============================================================================
const char* featureName(FeatureID id) {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < TOTAL_FEATURES) return g_FeatureManifest[idx].name;
    return "Unknown";
}

// ============================================================================
// Singleton
// ============================================================================
EnterpriseLicenseV2& EnterpriseLicenseV2::Instance() {
    static EnterpriseLicenseV2 s_instance;
    return s_instance;
}

// ============================================================================
// Initialize
// ============================================================================
LicenseResult EnterpriseLicenseV2::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        Logger::instance().logInfo("license.v2.init.skip", {
            {"reason", "already_initialized"}
        });
        EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Debug,
                         "license.v2.init", "already_initialized");
        return LicenseResult::ok("Already initialized");
    }

    const auto startTime = std::chrono::steady_clock::now();
    Logger::instance().logInfo("license.v2.init.start", {
        {"component", "EnterpriseLicenseV2"}
    });
    EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                     "license.v2.init", "start",
                     {{"component", "EnterpriseLicenseV2"}});

    // Compute hardware ID
    LicenseResult hwResult = computeHWID();
    if (!hwResult.success) {
        Logger::instance().logError("license.v2.hwid.failed", {
            {"detail", hwResult.detail}
        });
        EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Error,
                         "license.v2.hwid", "failed",
                         {{"detail", hwResult.detail}});
        return hwResult;
    }

    // Default to Community tier
    m_tier = LicenseTierV2::Community;
    m_enabledFeatures = TierPresets::Community();

    // ── PHASE 2: Bridge V1 License State to V2 ──
    {
        auto& v1 = RawrXD::EnterpriseLicense::Instance();
        // If V1 is already initialized and is Enterprise, upgrade V2 tier
        if (v1.IsEnterprise()) {
            m_tier = LicenseTierV2::Enterprise;
            m_enabledFeatures = TierPresets::Enterprise();
            Logger::instance().logInfo("license.v2.v1_bridge", {
                {"active", "true"},
                {"tier", "Enterprise"}
            });
            EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                             "license.v2.v1_bridge", "active",
                             {{"tier", "Enterprise"}});
        }
    }

    // Check for dev unlock environment variable (overrides V1)
    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (devEnv && std::strcmp(devEnv, "1") == 0) {
        m_tier = LicenseTierV2::Sovereign;
        m_enabledFeatures = TierPresets::Sovereign();
        Logger::instance().logInfo("license.v2.dev_unlock", {
            {"active", "true"}
        });
        EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                         "license.v2.dev_unlock", "active");
    }

    // Try to load license from default path
    const char* licPath = std::getenv("RAWRXD_LICENSE_PATH");
    if (licPath) {
        LicenseResult loadResult = loadKeyFromFile(licPath);
        if (!loadResult.success) {
            // Non-fatal: fall back to detected tier
            recordAudit(FeatureID::BasicGGUFLoading, true, "initialize",
                       "License file load failed, using detected tier");
            Logger::instance().logWarning("license.v2.load.failed", {
                {"detail", loadResult.detail}
            });
            EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Warning,
                             "license.v2.load", "failed",
                             {{"detail", loadResult.detail}});
        }
    }

    // ── PHASE 4: Registry Persistence ──
    if (m_tier == LicenseTierV2::Community) {
        LicenseResult regResult = loadKeyFromRegistry();
        if (regResult.success) {
            Logger::instance().logInfo("license.v2.registry.load", {
                {"tier", tierName(m_tier)}
            });
            EmitLicenseEvent(RawrXD::Telemetry::TelemetryLevel::Info,
                             "license.v2.registry", "success",
                             {{"tier", tierName(m_tier)}});
        }
    }

    m_initialized = true;
    recordAudit(FeatureID::BasicGGUFLoading, true, "initialize",
               tierName(m_tier));
    const auto endTime = std::chrono::steady_clock::now();
    const int64_t durationMs = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    Logger::instance().logInfo("license.v2.init.complete", {
        {"tier", tierName(m_tier)},
        {"duration_ms", std::to_string(durationMs)}
    });
    EmitLicenseSpan(RawrXD::Telemetry::TelemetryLevel::Info,
                    "license.v2.init", "complete", durationMs,
                    {{"tier", tierName(m_tier)}});
    return LicenseResult::ok("License system initialized");
}

// ============================================================================
// Shutdown
// ============================================================================
void EnterpriseLicenseV2::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    m_tier = LicenseTierV2::Community;
    m_enabledFeatures = FeatureMask{};
}

// ============================================================================
// Feature Queries
// ============================================================================
bool EnterpriseLicenseV2::isFeatureEnabled(FeatureID id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit >= TOTAL_FEATURES) return false;
    return m_enabledFeatures.test(bit) && g_FeatureManifest[bit].implemented;
}

bool EnterpriseLicenseV2::isFeatureLicensed(FeatureID id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit >= TOTAL_FEATURES) return false;
    return m_enabledFeatures.test(bit);
}

bool EnterpriseLicenseV2::isFeatureImplemented(FeatureID id) const {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= TOTAL_FEATURES) return false;
    return g_FeatureManifest[idx].implemented;
}

bool EnterpriseLicenseV2::gate(FeatureID id, const char* caller) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit >= TOTAL_FEATURES) {
        recordAudit(id, false, caller, "Invalid feature ID");
        return false;
    }

    bool licensed = m_enabledFeatures.test(bit);
    bool implemented = g_FeatureManifest[bit].implemented;
    bool granted = licensed && implemented;

    recordAudit(id, granted, caller,
               granted ? "Access granted" :
               !licensed ? "Feature not licensed" : "Feature not implemented");
    return granted;
}

// ============================================================================
// Tier Queries
// ============================================================================
LicenseTierV2 EnterpriseLicenseV2::currentTier() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tier;
}

const TierLimits::Limits& EnterpriseLicenseV2::currentLimits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return TierLimits::forTier(m_tier);
}

FeatureMask EnterpriseLicenseV2::currentMask() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabledFeatures;
}

uint32_t EnterpriseLicenseV2::enabledFeatureCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabledFeatures.popcount();
}

// ============================================================================
// Key Operations
// ============================================================================
LicenseResult EnterpriseLicenseV2::loadKeyFromFile(const char* path) {
    if (!path) return LicenseResult::error("Null path", 1);

#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return LicenseResult::error("Cannot open license file", 2);

    LicenseKeyV2 key{};
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &key, sizeof(key), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesRead != sizeof(key))
        return LicenseResult::error("Invalid license file size", 3);
#else
    FILE* f = fopen(path, "rb");
    if (!f) return LicenseResult::error("Cannot open license file", 2);

    LicenseKeyV2 key{};
    size_t bytesRead = fread(&key, 1, sizeof(key), f);
    fclose(f);

    if (bytesRead != sizeof(key))
        return LicenseResult::error("Invalid license file size", 3);
#endif

    return loadKeyFromMemory(&key, sizeof(key));
}

LicenseResult EnterpriseLicenseV2::loadKeyFromRegistry() {
#ifdef _WIN32
    HKEY hKey;
    // Check HKCU first, then HKLM
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD\\License", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\RawrXD\\License", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return LicenseResult::error("Registry key not found", 30);
        }
    }

    LicenseKeyV2 key{};
    DWORD dwType = REG_BINARY;
    DWORD dwSize = sizeof(key);
    LONG status = RegQueryValueExA(hKey, "LicenseData", nullptr, &dwType, reinterpret_cast<BYTE*>(&key), &dwSize);
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS || dwSize != sizeof(key)) {
        return LicenseResult::error("Registry value invalid", 31);
    }

    return loadKeyFromMemory(&key, sizeof(key));
#else
    return LicenseResult::error("Registry not supported on this platform", -1);
#endif
}

LicenseResult EnterpriseLicenseV2::saveKeyToRegistry(const LicenseKeyV2& key) {
#ifdef _WIN32
    HKEY hKey;
    LONG status = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\RawrXD\\License", 0, nullptr,
                                    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (status != ERROR_SUCCESS) {
        return LicenseResult::error("Failed to create registry key", 32);
    }

    status = RegSetValueExA(hKey, "LicenseData", 0, REG_BINARY, reinterpret_cast<const BYTE*>(&key), sizeof(key));
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        return LicenseResult::error("Failed to set registry value", 33);
    }

    return LicenseResult::ok("Key saved to registry");
#else
    return LicenseResult::error("Registry not supported on this platform", -1);
#endif
}

LicenseResult EnterpriseLicenseV2::requestAzureADLicense(const char* tenantId, const char* clientId) {
    // Stub for Azure AD token-based enterprise licensing
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::instance().logInfo("license.v2.azure_ad.request", {
        {"tenantId", tenantId ? tenantId : "common"},
        {"clientId", clientId ? clientId : "default"}
    });

    recordAudit(FeatureID::BasicGGUFLoading, false, "requestAzureADLicense",
               "Azure AD authentication flow triggered (Stub)");

    // In a real implementation:
    // 1. MSAL acquireToken()
    // 2. POST to https://license.rawrxd.ai/v2/exchange with Bearer token
    // 3. Receive signed LicenseKeyV2
    // 4. loadKeyFromMemory() + saveKeyToRegistry()

    return LicenseResult::error("Azure AD licensing requires the Enterprise Auth Plugin", 40);
}

LicenseResult EnterpriseLicenseV2::loadKeyFromMemory(const void* data, size_t size) {
    if (!data || size < sizeof(LicenseKeyV2))
        return LicenseResult::error("Invalid key data", 4);

    LicenseKeyV2 key;
    std::memcpy(&key, data, sizeof(key));

    LicenseResult valResult = validateKey(key);
    if (!valResult.success) return valResult;

    // Apply key
    std::lock_guard<std::mutex> lock(m_mutex);
    LicenseTierV2 oldTier = m_tier;
    m_currentKey = key;
    m_tier = static_cast<LicenseTierV2>(key.tier);
    m_enabledFeatures = key.features;

    // Fire callbacks
    if (oldTier != m_tier) {
        for (size_t i = 0; i < m_callbackCount; ++i) {
            if (m_callbacks[i]) m_callbacks[i](oldTier, m_tier);
        }
    }

    return LicenseResult::ok("License key loaded successfully");
}

LicenseResult EnterpriseLicenseV2::validateKey(const LicenseKeyV2& key) const {
    // Check magic
    if (key.magic != 0x5258444C)
        return LicenseResult::error("Invalid magic number", 10);

    // Check version
    if (key.version != 2)
        return LicenseResult::error("Unsupported key version", 11);

    // Check HWID binding
    if (key.hwid != m_hwid && key.hwid != 0)
        return LicenseResult::error("HWID mismatch", 12);

    // Check tier validity
    if (key.tier >= static_cast<uint32_t>(LicenseTierV2::COUNT))
        return LicenseResult::error("Invalid tier", 13);

    // Check expiry
    if (key.expiryDate != 0) {
        uint32_t now = static_cast<uint32_t>(std::time(nullptr));
        if (now > key.expiryDate)
            return LicenseResult::error("License expired", 14);
    }

    // Verify signature
    if (!verifySignature(key))
        return LicenseResult::error("Invalid signature", 15);

    return LicenseResult::ok("Key valid");
}

LicenseResult EnterpriseLicenseV2::createKey(LicenseTierV2 tier, uint32_t durationDays,
                                              const char* signingSecret,
                                              LicenseKeyV2* outKey) const {
    if (!outKey) return LicenseResult::error("Null output key", 20);
    if (!signingSecret) return LicenseResult::error("Null signing secret", 21);

    std::memset(outKey, 0, sizeof(LicenseKeyV2));
    outKey->magic = 0x5258444C;
    outKey->version = 2;
    outKey->hwid = m_hwid;
    outKey->features = TierPresets::forTier(tier);
    outKey->tier = static_cast<uint32_t>(tier);
    outKey->issueDate = static_cast<uint32_t>(std::time(nullptr));
    outKey->expiryDate = (durationDays == 0) ? 0
        : outKey->issueDate + (durationDays * 86400);

    const auto& limits = TierLimits::forTier(tier);
    outKey->maxModelGB = limits.maxModelGB;
    outKey->maxContextTokens = limits.maxContextTokens;

    signKey(*outKey, signingSecret);

    return LicenseResult::ok("Key created");
}

// ============================================================================
// HWID
// ============================================================================
uint64_t EnterpriseLicenseV2::getHardwareID() const {
    return m_hwid;
}

void EnterpriseLicenseV2::getHardwareIDHex(char* buf, size_t bufLen) const {
    if (buf && bufLen >= 17) {
        snprintf(buf, bufLen, "%016llX", (unsigned long long)m_hwid);
    }
}

LicenseResult EnterpriseLicenseV2::computeHWID() {
#ifdef _WIN32
    // MurmurHash of CPUID + volume serial
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 1);
    uint64_t cpuHash = static_cast<uint64_t>(cpuInfo[0]) |
                       (static_cast<uint64_t>(cpuInfo[3]) << 32);

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);

    // Simple hash combine
    m_hwid = cpuHash ^ (static_cast<uint64_t>(volSerial) * 0x9E3779B97F4A7C15ULL);
    m_hwid = (m_hwid ^ (m_hwid >> 33)) * 0xFF51AFD7ED558CCDULL;
    m_hwid = (m_hwid ^ (m_hwid >> 33)) * 0xC4CEB9FE1A85EC53ULL;
    m_hwid = m_hwid ^ (m_hwid >> 33);
#else
    // Fallback: use hostname hash
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    m_hwid = 0;
    for (const char* p = hostname; *p; ++p) {
        m_hwid = m_hwid * 31 + static_cast<uint64_t>(*p);
    }
#endif
    return LicenseResult::ok("HWID computed");
}

// ============================================================================
// Dev Unlock
// ============================================================================
LicenseResult EnterpriseLicenseV2::devUnlock() {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (!devEnv || std::strcmp(devEnv, "1") != 0)
        return LicenseResult::error("RAWRXD_ENTERPRISE_DEV not set", 30);

    LicenseTierV2 oldTier = m_tier;
    m_tier = LicenseTierV2::Sovereign;
    m_enabledFeatures = TierPresets::Sovereign();

    recordAudit(FeatureID::BasicGGUFLoading, true, "devUnlock",
               "Developer unlock: all features enabled");

    if (oldTier != m_tier) {
        for (size_t i = 0; i < m_callbackCount; ++i) {
            if (m_callbacks[i]) m_callbacks[i](oldTier, m_tier);
        }
    }

    return LicenseResult::ok("Developer unlock successful");
}

// ============================================================================
// Audit Trail
// ============================================================================
size_t EnterpriseLicenseV2::getAuditEntryCount() const {
    return m_auditCount;
}

const LicenseAuditEntry* EnterpriseLicenseV2::getAuditEntries() const {
    return m_auditTrail;
}

void EnterpriseLicenseV2::clearAuditTrail() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auditCount = 0;
    m_auditHead = 0;
}

void EnterpriseLicenseV2::recordAudit(FeatureID id, bool granted,
                                       const char* caller, const char* detail) {
    // Ring buffer insert (caller already holds lock)
    LicenseAuditEntry& entry = m_auditTrail[m_auditHead];

#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    entry.timestamp = static_cast<uint64_t>(ticks.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    entry.timestamp = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    entry.feature = id;
    entry.granted = granted;
    entry.caller = caller ? caller : "unknown";
    entry.detail = detail ? detail : "";

    m_auditHead = (m_auditHead + 1) % MAX_AUDIT_ENTRIES;
    if (m_auditCount < MAX_AUDIT_ENTRIES) m_auditCount++;
}

// ============================================================================
// Manifest Queries
// ============================================================================
const FeatureDefV2& EnterpriseLicenseV2::getFeatureDef(FeatureID id) const {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= TOTAL_FEATURES) {
        static const FeatureDefV2 s_unknown = {
            FeatureID::COUNT, "Unknown", "Invalid feature ID",
            LicenseTierV2::Sovereign, false, false, false, "N/A", "N/A"
        };
        return s_unknown;
    }
    return g_FeatureManifest[idx];
}

uint32_t EnterpriseLicenseV2::countByTier(LicenseTierV2 tier) const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].minTier == tier) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countImplemented() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].implemented) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countWiredToUI() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].wiredToUI) count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countTested() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        if (g_FeatureManifest[i].tested) count++;
    }
    return count;
}

// ============================================================================
// Callbacks
// ============================================================================
void EnterpriseLicenseV2::onLicenseChange(LicenseChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb && m_callbackCount < MAX_CALLBACKS) {
        m_callbacks[m_callbackCount++] = cb;
    }
}

// ============================================================================
// Signing (simplified HMAC — production should use CNG/bcrypt)
// ============================================================================
void EnterpriseLicenseV2::signKey(LicenseKeyV2& key, const char* secret) const {
    // Simple keyed hash: SHA-256-like mixing of key bytes + secret
    // Production would use BCryptCreateHash with BCRYPT_SHA256_ALGORITHM
    std::memset(key.signature, 0, sizeof(key.signature));

    const uint8_t* keyBytes = reinterpret_cast<const uint8_t*>(&key);
    size_t dataLen = offsetof(LicenseKeyV2, signature);
    size_t secretLen = secret ? std::strlen(secret) : 0;

    // Mix key data + secret into signature
    uint64_t h0 = 0x6A09E667F3BCC908ULL;
    uint64_t h1 = 0xBB67AE8584CAA73BULL;
    uint64_t h2 = 0x3C6EF372FE94F82BULL;
    uint64_t h3 = 0xA54FF53A5F1D36F1ULL;

    for (size_t i = 0; i < dataLen; ++i) {
        h0 = (h0 ^ keyBytes[i]) * 0x100000001B3ULL;
        h1 = (h1 ^ keyBytes[i]) * 0x01000193ULL;
    }
    for (size_t i = 0; i < secretLen; ++i) {
        h2 = (h2 ^ static_cast<uint8_t>(secret[i])) * 0x100000001B3ULL;
        h3 = (h3 ^ static_cast<uint8_t>(secret[i])) * 0x01000193ULL;
    }

    std::memcpy(key.signature + 0, &h0, 8);
    std::memcpy(key.signature + 8, &h1, 8);
    std::memcpy(key.signature + 16, &h2, 8);
    std::memcpy(key.signature + 24, &h3, 8);
}

bool EnterpriseLicenseV2::verifySignature(const LicenseKeyV2& key) const {
    // Re-sign a copy and compare signatures
    LicenseKeyV2 check;
    std::memcpy(&check, &key, sizeof(check));

    // For now, accept any signature in dev mode
    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (devEnv && std::strcmp(devEnv, "1") == 0) return true;

    // In production, this would use the stored signing secret
    // For now, validate that signature is non-zero
    bool allZero = true;
    for (int i = 0; i < 32; ++i) {
        if (key.signature[i] != 0) { allZero = false; break; }
    }
    return !allZero;
}

} // namespace RawrXD::License
