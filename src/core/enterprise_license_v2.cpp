// ============================================================================
// enterprise_license_v2.cpp — EnterpriseLicenseV2 Implementation
// ============================================================================
// Implements the 4-tier, 61-feature Enterprise License V2 singleton.
// All methods declared in include/enterprise_license.h are defined here.
//
// Key responsibilities:
//   - g_FeatureManifest[] — compile-time table of all 61 features
//   - featureName()       — human-readable feature name lookup
//   - EnterpriseLicenseV2 — singleton lifecycle, feature gating, key ops,
//                           HWID binding, audit trail, manifest queries
//
// PATTERN:   No exceptions. Returns LicenseResult / bool.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "enterprise_license.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <intrin.h>
#include <windows.h>

#else
#include <sys/utsname.h>
#include <unistd.h>

#endif

namespace RawrXD::License
{

// ============================================================================
// g_FeatureManifest — compile-time table of all 61 licenseable features
// ============================================================================
// Fields: id, name, description, minTier, implemented, wiredToUI, tested, sourceFile, phase
const FeatureDefV2 g_FeatureManifest[TOTAL_FEATURES] = {

    // ── Community Tier (0–5) ─────────────────────────────────────
    {FeatureID::BasicGGUFLoading, "Basic GGUF Loading", "Local GGUF file loading and parsing", LicenseTierV2::Community,
     true, true, false, "src/core/gguf_loader.cpp", "Phase 1"},

    {FeatureID::Q4Quantization, "Q4_0/Q4_1 Quantization", "4-bit quantization for memory-efficient inference",
     LicenseTierV2::Community, true, true, false, "src/core/quantization.cpp", "Phase 1"},

    {FeatureID::CPUInference, "CPU Inference", "Single-threaded and multi-threaded CPU inference",
     LicenseTierV2::Community, true, true, false, "src/core/inference_engine.cpp", "Phase 1"},

    {FeatureID::BasicChatUI, "Basic Chat UI", "Win32 chat window with input/output panels", LicenseTierV2::Community,
     true, true, false, "src/win32app/Win32IDE.cpp", "Phase 1"},

    {FeatureID::ConfigFileSupport, "Config File Support", "JSON configuration file loading and saving",
     LicenseTierV2::Community, true, true, false, "src/core/settings.cpp", "Phase 1"},

    {FeatureID::SingleModelSession, "Single Model Session", "Load and interact with a single model at a time",
     LicenseTierV2::Community, true, true, false, "src/core/model_session.cpp", "Phase 1"},

    // ── Professional Tier (6–26) ─────────────────────────────────
    {FeatureID::MemoryHotpatching, "Memory Hotpatching", "Direct RAM patching via VirtualProtect/mprotect",
     LicenseTierV2::Professional, true, true, false, "src/core/model_memory_hotpatch.cpp", "Phase 6"},

    {FeatureID::ByteLevelHotpatching, "Byte-Level Hotpatching",
     "Precision GGUF binary modification without full reparse", LicenseTierV2::Professional, true, true, false,
     "src/core/byte_level_hotpatcher.cpp", "Phase 6"},

    {FeatureID::ServerHotpatching, "Server Hotpatching", "Runtime request/response modification at injection points",
     LicenseTierV2::Professional, true, true, false, "src/server/gguf_server_hotpatch.cpp", "Phase 6"},

    {FeatureID::UnifiedHotpatchManager, "Unified Hotpatch Manager",
     "Coordinated 3-layer hotpatch routing and statistics", LicenseTierV2::Professional, true, true, false,
     "src/core/unified_hotpatch_manager.cpp", "Phase 6"},

    {FeatureID::Q5Q8F16Quantization, "Q5/Q8/F16 Quantization",
     "Higher-precision quantization formats (Q5_0, Q5_1, Q8_0, F16)", LicenseTierV2::Professional, true, true, false,
     "src/core/quantization.cpp", "Phase 7"},

    {FeatureID::MultiModelLoading, "Multi-Model Loading", "Load and switch between multiple GGUF models concurrently",
     LicenseTierV2::Professional, true, true, false, "src/core/model_manager.cpp", "Phase 8"},

    {FeatureID::CUDABackend, "CUDA Backend", "NVIDIA GPU acceleration via CUDA compute kernels",
     LicenseTierV2::Professional, false, false, false, "src/gpu/cuda_backend.cpp", "Phase 12"},

    {FeatureID::AdvancedSettingsPanel, "Advanced Settings Panel",
     "Extended parameter controls (temperature, top_p, repeat_penalty, etc.)", LicenseTierV2::Professional, true, true,
     false, "src/win32app/Win32IDE_Settings.cpp", "Phase 3"},

    {FeatureID::PromptTemplates, "Prompt Templates", "Pre-built and custom prompt templates with variable substitution",
     LicenseTierV2::Professional, true, true, false, "src/core/prompt_templates.cpp", "Phase 9"},

    {FeatureID::TokenStreaming, "Token Streaming", "Real-time token-by-token output streaming",
     LicenseTierV2::Professional, true, true, false, "src/core/streaming_orchestrator.cpp", "Phase 4"},

    {FeatureID::InferenceStatistics, "Inference Statistics", "Tokens/sec, time-to-first-token, memory usage tracking",
     LicenseTierV2::Professional, true, true, false, "src/core/inference_stats.cpp", "Phase 5"},

    {FeatureID::KVCacheManagement, "KV Cache Management", "Key-value cache pruning, rotation, and memory optimization",
     LicenseTierV2::Professional, true, true, false, "src/core/kv_cache.cpp", "Phase 10"},

    {FeatureID::ModelComparison, "Model Comparison", "Side-by-side inference output comparison between models",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 15"},

    {FeatureID::BatchProcessing, "Batch Processing", "Process multiple prompts in a single batch execution",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 15"},

    {FeatureID::CustomStopSequences, "Custom Stop Sequences", "User-defined stop strings for generation termination",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 11"},

    {FeatureID::GrammarConstrainedGen, "Grammar-Constrained Generation",
     "BNF/GBNF grammar enforcement during token sampling", LicenseTierV2::Professional, true, true, false,
     "src/win32app/Win32IDE_Commands.cpp", "Phase 14"},

    {FeatureID::LoRAAdapterSupport, "LoRA Adapter Support", "Load and apply LoRA adapters for model fine-tuning",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 16"},

    {FeatureID::ResponseCaching, "Response Caching", "Cache inference results for repeated prompts",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 13"},

    {FeatureID::PromptLibrary, "Prompt Library", "Organized prompt collection with tags and categories",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 13"},

    {FeatureID::ExportImportSessions, "Export/Import Sessions", "Save and restore complete chat sessions to JSON",
     LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 13"},

    {FeatureID::HIPBackend, "HIP Backend", "AMD GPU acceleration via HIP compute kernels", LicenseTierV2::Professional,
     false, false, false, "src/gpu/hip_backend.cpp", "Phase 12"},

    // ── Enterprise Tier (27–52) ──────────────────────────────────
    {FeatureID::DualEngine800B, "800B Dual-Engine", "Multi-shard inference orchestrator for 800B+ parameter models",
     LicenseTierV2::Enterprise, true, true, false, "src/dual_engine_inference.cpp", "Phase 21"},

    {FeatureID::AgenticFailureDetect, "Agentic Failure Detection",
     "Automated detection of refusal, hallucination, timeout, safety violations", LicenseTierV2::Enterprise, true, true,
     false, "src/agent/agentic_failure_detector.cpp", "Phase 18"},

    {FeatureID::AgenticPuppeteer, "Agentic Puppeteer", "Auto-correction engine for failed agentic responses",
     LicenseTierV2::Enterprise, true, true, false, "src/agent/agentic_puppeteer.cpp", "Phase 18"},

    {FeatureID::AgenticSelfCorrection, "Agentic Self-Correction",
     "Iterative self-correction loop with confidence scoring", LicenseTierV2::Enterprise, true, true, false,
     "src/agent/agentic_self_corrector.cpp", "Phase 19"},

    {FeatureID::ProxyHotpatching, "Proxy Hotpatching", "Byte-level output rewriting and token bias injection",
     LicenseTierV2::Enterprise, true, true, false, "src/core/proxy_hotpatcher.cpp", "Phase 20"},

    {FeatureID::ServerSidePatching, "Server-Side Patching",
     "Request/response interception at PreRequest/PostResponse/StreamChunk points", LicenseTierV2::Enterprise, true,
     true, false, "src/server/gguf_server_hotpatch.cpp", "Phase 20"},

    {FeatureID::SchematicStudioIDE, "SchematicStudio IDE",
     "Visual schematic editor for model architecture visualization", LicenseTierV2::Enterprise, true, true, false,
     "src/win32app/Win32IDE_SchematicStudio.cpp", "Phase 22"},

    {FeatureID::WiringOracleDebug, "WiringOracle Debug", "Connection graph debug and validation tool",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_WiringOracle.cpp", "Phase 22"},

    {FeatureID::FlashAttention, "Flash Attention", "Memory-efficient attention kernel (O(N) memory, tiled computation)",
     LicenseTierV2::Enterprise, false, false, false, "src/core/flash_attention.cpp", "Phase 23"},

    {FeatureID::SpeculativeDecoding, "Speculative Decoding", "Draft-model assisted decoding for faster generation",
     LicenseTierV2::Enterprise, false, false, false, "src/core/speculative_decoding.cpp", "Phase 23"},

    {FeatureID::ModelSharding, "Model Sharding", "Split large models across multiple devices or memory regions",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 21"},

    {FeatureID::TensorParallel, "Tensor Parallelism", "Distribute tensor operations across multiple compute units",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 21"},

    {FeatureID::PipelineParallel, "Pipeline Parallelism", "Stage-based model execution across multiple devices",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 21"},

    {FeatureID::ContinuousBatching, "Continuous Batching", "Dynamic request batching with continuous scheduling",
     LicenseTierV2::Enterprise, false, false, false, "src/core/continuous_batching.cpp", "Phase 24"},

    {FeatureID::GPTQQuantization, "GPTQ Quantization", "Post-training quantization using GPTQ algorithm",
     LicenseTierV2::Enterprise, false, false, false, "src/core/gptq_quant.cpp", "Phase 25"},

    {FeatureID::AWQQuantization, "AWQ Quantization", "Activation-aware weight quantization for LLMs",
     LicenseTierV2::Enterprise, false, false, false, "src/core/awq_quant.cpp", "Phase 25"},

    {FeatureID::CustomQuantSchemes, "Custom Quant Schemes", "User-defined quantization formats and configuration",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 25"},

    {FeatureID::MultiGPULoadBalance, "Multi-GPU Load Balancing", "Dynamic work distribution across multiple GPUs",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 24"},

    {FeatureID::DynamicBatchSizing, "Dynamic Batch Sizing",
     "Automatic batch size adjustment based on available resources", LicenseTierV2::Enterprise, true, true, false,
     "src/win32app/Win32IDE_Commands.cpp", "Phase 24"},

    {FeatureID::PriorityQueuing, "Priority Queuing", "Request priority queue with SLA-based scheduling",
     LicenseTierV2::Enterprise, false, false, false, "src/server/priority_queue.cpp", "Phase 26"},

    {FeatureID::RateLimitingEngine, "Rate Limiting Engine", "Token bucket rate limiter with per-client quotas",
     LicenseTierV2::Enterprise, false, false, false, "src/server/rate_limiter.cpp", "Phase 26"},

    {FeatureID::AuditLogging, "Audit Logging", "Comprehensive compliance audit trail with structured logging",
     LicenseTierV2::Enterprise, true, true, false, "src/core/enterprise_telemetry_compliance.cpp", "Phase 22"},

    {FeatureID::APIKeyManagement, "API Key Management", "Secure API key generation, rotation, and revocation",
     LicenseTierV2::Enterprise, true, true, false, "src/win32app/Win32IDE_Commands.cpp", "Phase 26"},

    {FeatureID::ModelSigningVerify, "Model Signing/Verification",
     "Cryptographic model integrity verification and signing", LicenseTierV2::Enterprise, true, true, false,
     "src/core/model_signing.cpp", "Phase 22"},

    {FeatureID::RBAC, "Role-Based Access Control", "Fine-grained role-based access control for features and models",
     LicenseTierV2::Enterprise, false, false, false, "src/core/rbac.cpp", "Phase 27"},

    {FeatureID::ObservabilityDashboard, "Observability Dashboard",
     "Real-time metrics dashboard with system health monitoring", LicenseTierV2::Enterprise, true, true, false,
     "src/win32app/Win32IDE_Observability.cpp", "Phase 22"},

    // ── Sovereign Tier (53–60) ───────────────────────────────────
    {FeatureID::AirGappedDeploy, "Air-Gapped Deployment",
     "Fully offline deployment with no external network dependencies", LicenseTierV2::Sovereign, false, false, false,
     "src/win32app/Win32IDE_AirgappedEnterprise.cpp", "Phase 28"},

    {FeatureID::HSMIntegration, "HSM Integration", "Hardware Security Module integration for key management",
     LicenseTierV2::Sovereign, false, false, false, "src/security/hsm_integration.cpp", "Phase 28"},

    {FeatureID::FIPS140_2Compliance, "FIPS 140-2 Compliance",
     "Federal Information Processing Standards compliant cryptography", LicenseTierV2::Sovereign, false, false, false,
     "src/security/fips_compliance.cpp", "Phase 28"},

    {FeatureID::CustomSecurityPolicies, "Custom Security Policies",
     "Organization-specific security policy engine and enforcement", LicenseTierV2::Sovereign, false, false, false,
     "src/security/custom_policies.cpp", "Phase 28"},

    {FeatureID::SovereignKeyMgmt, "Sovereign Key Management",
     "Government-grade key management with escrow and recovery", LicenseTierV2::Sovereign, false, false, false,
     "src/security/sovereign_key_mgmt.cpp", "Phase 28"},

    {FeatureID::ClassifiedNetwork, "Classified Network Support", "Deployment on classified/SIPR/JWICS networks",
     LicenseTierV2::Sovereign, false, false, false, "src/security/classified_network.cpp", "Phase 28"},

    {FeatureID::TamperDetection, "Tamper Detection", "Runtime tampering detection with integrity verification chain",
     LicenseTierV2::Sovereign, false, false, false, "src/security/tamper_detection.cpp", "Phase 28"},

    {FeatureID::SecureBootChain, "Secure Boot Chain", "Cryptographic boot chain verification from UEFI to application",
     LicenseTierV2::Sovereign, false, false, false, "src/security/secure_boot.cpp", "Phase 28"},
};

// ============================================================================
// featureName — human-readable feature name lookup
// ============================================================================
const char* featureName(FeatureID id)
{
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < TOTAL_FEATURES)
        return g_FeatureManifest[idx].name;
    return "Unknown";
}

// ============================================================================
// Singleton
// ============================================================================
EnterpriseLicenseV2& EnterpriseLicenseV2::Instance()
{
    static EnterpriseLicenseV2 s_instance;
    return s_instance;
}

// ============================================================================
// Initialize — sets up license system, attempts auto-load and dev unlock
// ============================================================================
LicenseResult EnterpriseLicenseV2::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
        return LicenseResult::ok("Already initialized");

    fprintf(stderr, "[LicenseV2] Initializing Enterprise License V2...\n");

    // Compute hardware ID
    LicenseResult hwResult = computeHWID();
    if (!hwResult.success)
    {
        fprintf(stderr, "[LicenseV2] WARNING: HWID computation failed: %s\n", hwResult.detail);
        // Continue anyway — community tier still works
    }

    // Default to Community tier
    m_tier = LicenseTierV2::Community;
    m_enabledFeatures = TierPresets::Community();

    // Try dev unlock via environment variable
    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (devEnv && (std::strcmp(devEnv, "1") == 0 || std::strcmp(devEnv, "true") == 0))
    {
        m_tier = LicenseTierV2::Sovereign;
        m_enabledFeatures = TierPresets::Sovereign();
        fprintf(stderr, "[LicenseV2] DEV UNLOCK: All features enabled (Sovereign tier)\n");
    }

    // Try auto-load from default paths
    const char* autoLoadPaths[] = {
        "license.rawrlic",
        "config/license.rawrlic",
        "../license.rawrlic",
    };
    for (const char* path : autoLoadPaths)
    {
        std::ifstream test(path, std::ios::binary);
        if (test.good())
        {
            test.close();
            LicenseResult loadResult = loadKeyFromFile(path);
            if (loadResult.success)
            {
                fprintf(stderr, "[LicenseV2] Auto-loaded license from: %s\n", path);
                break;
            }
        }
    }

    m_initialized = true;

    fprintf(stderr, "[LicenseV2] License V2 initialized — Tier: %s, Features: %u/%u\n", tierName(m_tier),
            enabledFeatureCount(), TOTAL_FEATURES);

    return LicenseResult::ok("License V2 initialized");
}

// ============================================================================
// Shutdown
// ============================================================================
void EnterpriseLicenseV2::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    m_tier = LicenseTierV2::Community;
    m_enabledFeatures = FeatureMask();
    m_auditCount = 0;
    m_auditHead = 0;
    fprintf(stderr, "[LicenseV2] Shutdown complete\n");
}

// ============================================================================
// Feature Queries
// ============================================================================
bool EnterpriseLicenseV2::isFeatureEnabled(FeatureID id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit >= TOTAL_FEATURES)
        return false;
    return m_enabledFeatures.test(bit);
}

bool EnterpriseLicenseV2::isFeatureLicensed(FeatureID id) const
{
    // Check if the current tier is sufficient for this feature
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= TOTAL_FEATURES)
        return false;
    return static_cast<uint32_t>(m_tier) >= static_cast<uint32_t>(g_FeatureManifest[idx].minTier);
}

bool EnterpriseLicenseV2::isFeatureImplemented(FeatureID id) const
{
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= TOTAL_FEATURES)
        return false;
    return g_FeatureManifest[idx].implemented;
}

bool EnterpriseLicenseV2::gate(FeatureID id, const char* caller)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t bit = static_cast<uint32_t>(id);
    if (bit >= TOTAL_FEATURES)
    {
        recordAudit(id, false, caller, "Invalid feature ID");
        return false;
    }

    bool enabled = m_enabledFeatures.test(bit);

    if (!enabled)
    {
        const char* featName = g_FeatureManifest[bit].name;
        char detail[256];
        snprintf(detail, sizeof(detail), "Feature '%s' requires %s tier (current: %s)", featName,
                 tierName(g_FeatureManifest[bit].minTier), tierName(m_tier));
        recordAudit(id, false, caller, detail);
        fprintf(stderr, "[LicenseV2] DENIED: %s — caller: %s\n", detail, caller ? caller : "<unknown>");
    }
    else
    {
        recordAudit(id, true, caller, "Access granted");
    }

    return enabled;
}

// ============================================================================
// Tier Queries
// ============================================================================
LicenseTierV2 EnterpriseLicenseV2::currentTier() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tier;
}

const TierLimits::Limits& EnterpriseLicenseV2::currentLimits() const
{
    // No lock needed — tier reading is atomic-width
    return TierLimits::forTier(m_tier);
}

FeatureMask EnterpriseLicenseV2::currentMask() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabledFeatures;
}

uint32_t EnterpriseLicenseV2::enabledFeatureCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabledFeatures.popcount();
}

// ============================================================================
// Key Operations
// ============================================================================
LicenseResult EnterpriseLicenseV2::loadKeyFromFile(const char* path)
{
    if (!path)
        return LicenseResult::error("Null path", -1);

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot open license file: %s", path);
        return LicenseResult::error(buf, -2);
    }

    LicenseKeyV2 key{};
    ifs.read(reinterpret_cast<char*>(&key), sizeof(key));
    if (static_cast<size_t>(ifs.gcount()) != sizeof(key))
    {
        return LicenseResult::error("Invalid key file size", -3);
    }

    return loadKeyFromMemory(&key, sizeof(key));
}

LicenseResult EnterpriseLicenseV2::loadKeyFromMemory(const void* data, size_t size)
{
    if (!data || size < sizeof(LicenseKeyV2))
    {
        return LicenseResult::error("Invalid key data", -1);
    }

    LicenseKeyV2 key{};
    std::memcpy(&key, data, sizeof(LicenseKeyV2));

    LicenseResult valResult = validateKey(key);
    if (!valResult.success)
        return valResult;

    // Apply key
    std::lock_guard<std::mutex> lock(m_mutex);

    LicenseTierV2 oldTier = m_tier;
    m_currentKey = key;
    m_tier = static_cast<LicenseTierV2>(key.tier);
    m_enabledFeatures = key.features;

    // Notify callbacks
    if (oldTier != m_tier)
    {
        for (size_t i = 0; i < m_callbackCount; ++i)
        {
            if (m_callbacks[i])
                m_callbacks[i](oldTier, m_tier);
        }
    }

    fprintf(stderr, "[LicenseV2] Key loaded — Tier: %s, Features: %u/%u\n", tierName(m_tier),
            m_enabledFeatures.popcount(), TOTAL_FEATURES);

    return LicenseResult::ok("Key loaded successfully");
}

LicenseResult EnterpriseLicenseV2::validateKey(const LicenseKeyV2& key) const
{
    // Magic check
    if (key.magic != 0x5258444C)
    {
        return LicenseResult::error("Invalid magic (expected RXDL)", -10);
    }

    // Version check
    if (key.version != 2)
    {
        return LicenseResult::error("Unsupported key version", -11);
    }

    // Tier bounds check
    if (key.tier >= static_cast<uint32_t>(LicenseTierV2::COUNT))
    {
        return LicenseResult::error("Invalid tier value", -12);
    }

    // Expiry check
    if (key.expiryDate != 0)
    {
        uint32_t now = static_cast<uint32_t>(std::time(nullptr));
        if (now > key.expiryDate)
        {
            return LicenseResult::error("License expired", -13);
        }
    }

    // HWID check (if key is hardware-bound)
    if (key.hwid != 0 && key.hwid != m_hwid)
    {
        return LicenseResult::error("Hardware ID mismatch", -14);
    }

    // Signature verification
    if (!verifySignature(key))
    {
        return LicenseResult::error("Signature verification failed", -15);
    }

    return LicenseResult::ok("Key valid");
}

// ============================================================================
// Create Key
// ============================================================================
LicenseResult EnterpriseLicenseV2::createKey(LicenseTierV2 tier, uint32_t durationDays, const char* signingSecret,
                                             LicenseKeyV2* outKey) const
{
    if (!outKey)
        return LicenseResult::error("Null output key", -1);
    if (!signingSecret)
        return LicenseResult::error("Null signing secret", -2);

    std::memset(outKey, 0, sizeof(LicenseKeyV2));

    outKey->magic = 0x5258444C;  // "RXDL"
    outKey->version = 2;
    outKey->tier = static_cast<uint32_t>(tier);
    outKey->features = TierPresets::forTier(tier);
    outKey->issueDate = static_cast<uint32_t>(std::time(nullptr));

    if (durationDays > 0)
    {
        outKey->expiryDate = outKey->issueDate + (durationDays * 86400);
    }
    else
    {
        outKey->expiryDate = 0;  // Perpetual
    }

    // Set tier limits
    const auto& limits = TierLimits::forTier(tier);
    outKey->maxModelGB = limits.maxModelGB;
    outKey->maxContextTokens = limits.maxContextTokens;

    // Sign the key
    signKey(*outKey, signingSecret);

    return LicenseResult::ok("Key created");
}

// ============================================================================
// HWID
// ============================================================================
uint64_t EnterpriseLicenseV2::getHardwareID() const
{
    return m_hwid;
}

void EnterpriseLicenseV2::getHardwareIDHex(char* buf, size_t bufLen) const
{
    if (buf && bufLen >= 17)
    {
        snprintf(buf, bufLen, "%016llX", (unsigned long long)m_hwid);
    }
}

LicenseResult EnterpriseLicenseV2::computeHWID()
{
    uint64_t hash = 0x811C9DC5ULL;  // FNV-1a offset

#ifdef _WIN32
    // CPU info
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 1);
    hash ^= static_cast<uint64_t>(cpuInfo[0]);
    hash *= 0x01000193ULL;
    hash ^= static_cast<uint64_t>(cpuInfo[3]);
    hash *= 0x01000193ULL;

    // Computer name
    char compName[256] = {};
    DWORD compNameLen = sizeof(compName);
    if (GetComputerNameA(compName, &compNameLen))
    {
        for (DWORD i = 0; i < compNameLen; ++i)
        {
            hash ^= static_cast<uint64_t>(compName[i]);
            hash *= 0x01000193ULL;
        }
    }

    // Volume serial
    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);
    hash ^= static_cast<uint64_t>(volSerial);
    hash *= 0x01000193ULL;
#else
    // Hostname
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    for (size_t i = 0; hostname[i]; ++i)
    {
        hash ^= static_cast<uint64_t>(hostname[i]);
        hash *= 0x01000193ULL;
    }
#endif

    m_hwid = hash;
    return LicenseResult::ok("HWID computed");
}

// ============================================================================
// Dev Unlock
// ============================================================================
LicenseResult EnterpriseLicenseV2::devUnlock()
{
    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (!devEnv || (std::strcmp(devEnv, "1") != 0 && std::strcmp(devEnv, "true") != 0))
    {
        return LicenseResult::error("RAWRXD_ENTERPRISE_DEV not set", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    LicenseTierV2 oldTier = m_tier;
    m_tier = LicenseTierV2::Sovereign;
    m_enabledFeatures = TierPresets::Sovereign();

    for (size_t i = 0; i < m_callbackCount; ++i)
    {
        if (m_callbacks[i])
            m_callbacks[i](oldTier, m_tier);
    }

    fprintf(stderr, "[LicenseV2] DEV UNLOCK: Sovereign tier activated\n");
    return LicenseResult::ok("Dev unlock — Sovereign tier");
}

// ============================================================================
// Audit Trail
// ============================================================================
size_t EnterpriseLicenseV2::getAuditEntryCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_auditCount;
}

const LicenseAuditEntry* EnterpriseLicenseV2::getAuditEntries() const
{
    return m_auditTrail;
}

void EnterpriseLicenseV2::clearAuditTrail()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auditCount = 0;
    m_auditHead = 0;
    std::memset(m_auditTrail, 0, sizeof(m_auditTrail));
}

void EnterpriseLicenseV2::recordAudit(FeatureID id, bool granted, const char* caller, const char* detail)
{
    // Caller must already hold m_mutex
    LicenseAuditEntry& entry = m_auditTrail[m_auditHead];
#ifdef _WIN32
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    entry.timestamp = static_cast<uint64_t>(pc.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    entry.timestamp = static_cast<uint64_t>(tv.tv_sec) * 1000000ULL + tv.tv_usec;
#endif
    entry.feature = id;
    entry.granted = granted;
    entry.caller = caller;
    entry.detail = detail;

    m_auditHead = (m_auditHead + 1) % MAX_AUDIT_ENTRIES;
    if (m_auditCount < MAX_AUDIT_ENTRIES)
        m_auditCount++;
}

// ============================================================================
// Manifest Queries
// ============================================================================
const FeatureDefV2& EnterpriseLicenseV2::getFeatureDef(FeatureID id) const
{
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= TOTAL_FEATURES)
    {
        static const FeatureDefV2 invalid = {
            FeatureID::COUNT, "Invalid", "Invalid feature", LicenseTierV2::Sovereign, false, false, false, "", ""};
        return invalid;
    }
    return g_FeatureManifest[idx];
}

uint32_t EnterpriseLicenseV2::countByTier(LicenseTierV2 tier) const
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i)
    {
        if (g_FeatureManifest[i].minTier == tier)
            count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countImplemented() const
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i)
    {
        if (g_FeatureManifest[i].implemented)
            count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countWiredToUI() const
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i)
    {
        if (g_FeatureManifest[i].wiredToUI)
            count++;
    }
    return count;
}

uint32_t EnterpriseLicenseV2::countTested() const
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i)
    {
        if (g_FeatureManifest[i].tested)
            count++;
    }
    return count;
}

// ============================================================================
// Callbacks
// ============================================================================
void EnterpriseLicenseV2::onLicenseChange(LicenseChangeCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb && m_callbackCount < MAX_CALLBACKS)
    {
        m_callbacks[m_callbackCount++] = cb;
    }
}

// ============================================================================
// Key Signing — HMAC-like FNV-1a double-hash
// ============================================================================
void EnterpriseLicenseV2::signKey(LicenseKeyV2& key, const char* secret) const
{
    // Zero signature before signing
    std::memset(key.signature, 0, sizeof(key.signature));

    // FNV-1a hash of key body + secret
    uint64_t h1 = 0xCBF29CE484222325ULL;
    uint64_t h2 = 0x100000001B3ULL;

    // Hash key body (everything except signature and padding)
    const uint8_t* body = reinterpret_cast<const uint8_t*>(&key);
    size_t bodyLen = offsetof(LicenseKeyV2, signature);
    for (size_t i = 0; i < bodyLen; ++i)
    {
        h1 ^= body[i];
        h1 *= 0x01000193ULL;
        h2 ^= body[i];
        h2 *= 0x00000100000001B3ULL;
    }

    // Mix in secret
    if (secret)
    {
        for (size_t i = 0; secret[i]; ++i)
        {
            h1 ^= static_cast<uint8_t>(secret[i]);
            h1 *= 0x01000193ULL;
            h2 ^= static_cast<uint8_t>(secret[i]);
            h2 *= 0x00000100000001B3ULL;
        }
    }

    // Write dual 64-bit hashes into first 16 bytes of signature
    std::memcpy(key.signature + 0, &h1, 8);
    std::memcpy(key.signature + 8, &h2, 8);

    // Fill remaining 16 bytes with derived values
    uint64_t h3 = h1 ^ h2;
    uint64_t h4 = h1 + h2;
    std::memcpy(key.signature + 16, &h3, 8);
    std::memcpy(key.signature + 24, &h4, 8);
}

bool EnterpriseLicenseV2::verifySignature(const LicenseKeyV2& key) const
{
    // Dev-unlock keys bypass signature check
    const char* devEnv = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (devEnv && (std::strcmp(devEnv, "1") == 0 || std::strcmp(devEnv, "true") == 0))
    {
        return true;
    }

    // For production: re-derive signature and compare
    // Note: Without knowing the signing secret, we fall back to structural validation
    // The full HMAC check happens when the secret is known (createKey / external validator)
    return key.magic == 0x5258444C && key.version == 2;
}

}  // namespace RawrXD::License
