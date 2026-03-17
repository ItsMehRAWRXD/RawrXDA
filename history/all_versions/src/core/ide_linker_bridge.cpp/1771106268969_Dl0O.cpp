// ============================================================================
// ide_linker_bridge.cpp — Final Linker Closure for RawrXD-Win32IDE
// ============================================================================
// Only provides symbols that are NOT defined elsewhere in the link.
// All symbols that already have definitions have been removed.
// ============================================================================
#include "enterprise_license.h"
#include "enterprise/multi_gpu.h"
#include "telemetry/UnifiedTelemetryCore.h"
#include "telemetry/logger.h"
#include "rawrxd_telemetry_exports.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace RawrXD {

    // 1. MultiGPUManager bridge (RawrXD::MultiGPUManager → RawrXD::Enterprise::MultiGPUManager)
    class MultiGPUManager {
    public:
        static MultiGPUManager& Instance();
    };

    MultiGPUManager& MultiGPUManager::Instance() {
        return reinterpret_cast<MultiGPUManager&>(RawrXD::Enterprise::MultiGPUManager::Instance());
    }

    namespace Telemetry {
        // 2. Telemetry Logger bridge (RawrXD::Telemetry::Logger::Log → UTC_LogEvent)
        class Logger {
        public:
            static void Log(TelemetryLevel level, const char* fmt, ...);
        };

        void Logger::Log(TelemetryLevel level, const char* fmt, ...) {
            char buffer[2048];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            
            // Forward to the global singleton logger
            ::Logger::instance().log(::Logger::Info, buffer);

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
            UTC_LogEvent(buffer);
#endif
        }
    }
}

// 3. g_FeatureManifest — full 65-entry feature manifest table
namespace RawrXD::License {

const FeatureDefV2 g_FeatureManifest[TOTAL_FEATURES] = {
    // Community (0-5)
    { FeatureID::BasicGGUFLoading,        "Basic GGUF Loading",         "Load and parse GGUF model files",          LicenseTierV2::Community,    true, true, true, "src/gguf_loader.cpp", "Phase 1" },
    { FeatureID::Q4Quantization,          "Q4 Quantization",            "Q4_0 / Q4_1 quantization support",         LicenseTierV2::Community,    true, true, true, "src/quant.cpp", "Phase 1" },
    { FeatureID::CPUInference,            "CPU Inference",              "CPU-only inference engine",                 LicenseTierV2::Community,    true, true, true, "src/cpu_inference_engine.cpp", "Phase 1" },
    { FeatureID::BasicChatUI,             "Basic Chat UI",              "Simple chat interface",                     LicenseTierV2::Community,    true, true, true, "src/win32app/Win32IDE.cpp", "Phase 2" },
    { FeatureID::ConfigFileSupport,       "Config File Support",        "JSON-based configuration files",            LicenseTierV2::Community,    true, true, true, "src/config/config.cpp", "Phase 2" },
    { FeatureID::SingleModelSession,      "Single Model Session",       "Load one model at a time",                  LicenseTierV2::Community,    true, true, true, "src/model_session.cpp", "Phase 1" },
    // Professional (6-26)
    { FeatureID::MemoryHotpatching,       "Memory Hotpatching",         "Direct RAM patching of loaded tensors",     LicenseTierV2::Professional, true, true, false, "src/core/model_memory_hotpatch.cpp", "Phase 4" },
    { FeatureID::ByteLevelHotpatching,    "Byte-Level Hotpatching",     "Precision GGUF binary modification",        LicenseTierV2::Professional, true, true, false, "src/core/byte_level_hotpatcher.cpp", "Phase 4" },
    { FeatureID::ServerHotpatching,       "Server Hotpatching",         "Runtime request/response modification",     LicenseTierV2::Professional, true, true, false, "src/server/gguf_server_hotpatch.cpp", "Phase 4" },
    { FeatureID::UnifiedHotpatchManager,  "Unified Hotpatch Manager",   "Coordinated 3-layer hotpatching",           LicenseTierV2::Professional, true, true, false, "src/core/unified_hotpatch_manager.cpp", "Phase 4" },
    { FeatureID::Q5Q8F16Quantization,     "Q5/Q8/F16 Quantization",     "Advanced quantization formats",             LicenseTierV2::Professional, true, true, false, "src/quant.cpp", "Phase 5" },
    { FeatureID::MultiModelLoading,       "Multi-Model Loading",        "Load multiple models simultaneously",       LicenseTierV2::Professional, true, true, false, "src/model_session.cpp", "Phase 6" },
    { FeatureID::CUDABackend,             "CUDA Backend",               "NVIDIA GPU acceleration",                   LicenseTierV2::Professional, false, false, false, "src/cuda_backend.cpp", "Phase 7" },
    { FeatureID::AdvancedSettingsPanel,    "Advanced Settings Panel",    "Extended configuration UI",                  LicenseTierV2::Professional, true, true, false, "src/win32app/Win32IDE_Settings.cpp", "Phase 8" },
    { FeatureID::PromptTemplates,         "Prompt Templates",           "Reusable prompt templates",                 LicenseTierV2::Professional, true, true, false, "src/prompt_engine.cpp", "Phase 8" },
    { FeatureID::TokenStreaming,           "Token Streaming",            "Real-time token output streaming",          LicenseTierV2::Professional, true, true, false, "src/streaming.cpp", "Phase 3" },
    { FeatureID::InferenceStatistics,     "Inference Statistics",       "Performance metrics and profiling",         LicenseTierV2::Professional, true, true, false, "src/telemetry/logger.cpp", "Phase 9" },
    { FeatureID::KVCacheManagement,       "KV Cache Management",        "Key-value cache optimization",              LicenseTierV2::Professional, true, false, false, "src/kv_cache.cpp", "Phase 10" },
    { FeatureID::ModelComparison,         "Model Comparison",           "Side-by-side model comparison",             LicenseTierV2::Professional, true, true, false, "src/multi_response_engine.cpp", "Phase 11" },
    { FeatureID::BatchProcessing,         "Batch Processing",           "Process multiple prompts at once",          LicenseTierV2::Professional, true, false, false, "src/batch.cpp", "Phase 12" },
    { FeatureID::CustomStopSequences,     "Custom Stop Sequences",      "Configurable stop tokens",                  LicenseTierV2::Professional, true, true, false, "src/sampler.cpp", "Phase 3" },
    { FeatureID::GrammarConstrainedGen,   "Grammar-Constrained Gen",    "GBNF grammar-constrained generation",       LicenseTierV2::Professional, true, false, false, "src/grammar.cpp", "Phase 12" },
    { FeatureID::LoRAAdapterSupport,      "LoRA Adapter Support",       "Load and merge LoRA adapters",              LicenseTierV2::Professional, false, false, false, "src/lora.cpp", "Phase 13" },
    { FeatureID::ResponseCaching,         "Response Caching",           "Cache inference results",                   LicenseTierV2::Professional, true, false, false, "src/cache.cpp", "Phase 12" },
    { FeatureID::PromptLibrary,           "Prompt Library",             "Organized prompt collection",               LicenseTierV2::Professional, true, true, false, "src/prompt_library.cpp", "Phase 8" },
    { FeatureID::ExportImportSessions,    "Export/Import Sessions",     "Session backup and restore",                LicenseTierV2::Professional, true, true, false, "src/session.cpp", "Phase 8" },
    { FeatureID::HIPBackend,              "HIP Backend",                "AMD GPU acceleration",                      LicenseTierV2::Professional, false, false, false, "src/hip_backend.cpp", "Phase 7" },
    // Enterprise (27-54)
    { FeatureID::DualEngine800B,          "800B Dual-Engine",           "Multi-shard 800B model inference",          LicenseTierV2::Enterprise,   true, true, false, "src/dual_engine_inference.cpp", "Phase 14" },
    { FeatureID::AgenticFailureDetect,    "Agentic Failure Detection",  "Detect refusal, hallucination, timeout",    LicenseTierV2::Enterprise,   true, true, false, "src/agent/agentic_failure_detector.cpp", "Phase 15" },
    { FeatureID::AgenticPuppeteer,        "Agentic Puppeteer",          "Auto-correct failed responses",             LicenseTierV2::Enterprise,   true, true, false, "src/agent/agentic_puppeteer.cpp", "Phase 15" },
    { FeatureID::AgenticSelfCorrection,   "Agentic Self-Correction",    "Self-healing agent loop",                   LicenseTierV2::Enterprise,   true, true, false, "src/agent/agent_self_repair.cpp", "Phase 15" },
    { FeatureID::ProxyHotpatching,        "Proxy Hotpatching",          "Byte-level output rewriting",               LicenseTierV2::Enterprise,   true, true, false, "src/core/proxy_hotpatcher.cpp", "Phase 4" },
    { FeatureID::ServerSidePatching,      "Server-Side Patching",       "Request/response injection",                LicenseTierV2::Enterprise,   true, true, false, "src/server/gguf_server_hotpatch.cpp", "Phase 4" },
    { FeatureID::SchematicStudioIDE,      "Schematic Studio IDE",       "Visual wiring design tool",                 LicenseTierV2::Enterprise,   false, false, false, "src/schematic.cpp", "Phase 16" },
    { FeatureID::WiringOracleDebug,       "Wiring Oracle Debug",        "Debug component wiring",                    LicenseTierV2::Enterprise,   false, false, false, "src/wiring_oracle.cpp", "Phase 16" },
    { FeatureID::FlashAttention,          "Flash Attention",            "AVX-512 flash attention kernels",           LicenseTierV2::Enterprise,   true, true, false, "src/core/flash_attention.cpp", "Phase 17" },
    { FeatureID::SpeculativeDecoding,     "Speculative Decoding",       "Multi-draft speculative decoding",          LicenseTierV2::Enterprise,   false, false, false, "src/speculative.cpp", "Phase 18" },
    { FeatureID::ModelSharding,           "Model Sharding",             "Cross-GPU model sharding",                  LicenseTierV2::Enterprise,   true, false, false, "src/sharding.cpp", "Phase 19" },
    { FeatureID::TensorParallel,          "Tensor Parallel",            "Tensor parallelism inference",              LicenseTierV2::Enterprise,   false, false, false, "src/tensor_parallel.cpp", "Phase 19" },
    { FeatureID::PipelineParallel,        "Pipeline Parallel",          "Pipeline parallelism inference",            LicenseTierV2::Enterprise,   true, false, false, "src/core/adaptive_pipeline_parallel.cpp", "Phase 19" },
    { FeatureID::ContinuousBatching,      "Continuous Batching",        "Dynamic batch scheduling",                  LicenseTierV2::Enterprise,   false, false, false, "src/continuous_batch.cpp", "Phase 20" },
    { FeatureID::GPTQQuantization,        "GPTQ Quantization",          "GPTQ-quality quantization",                 LicenseTierV2::Enterprise,   false, false, false, "src/gptq.cpp", "Phase 20" },
    { FeatureID::AWQQuantization,         "AWQ Quantization",           "AWQ activation-aware quantization",         LicenseTierV2::Enterprise,   false, false, false, "src/awq.cpp", "Phase 20" },
    { FeatureID::CustomQuantSchemes,      "Custom Quant Schemes",       "User-defined quantization",                 LicenseTierV2::Enterprise,   false, false, false, "src/custom_quant.cpp", "Phase 20" },
    { FeatureID::MultiGPULoadBalance,     "Multi-GPU Load Balance",     "Intelligent GPU workload distribution",     LicenseTierV2::Enterprise,   true, true, false, "src/core/multi_gpu.cpp", "Phase 21" },
    { FeatureID::DynamicBatchSizing,      "Dynamic Batch Sizing",       "Runtime batch size adjustment",             LicenseTierV2::Enterprise,   false, false, false, "src/dynamic_batch.cpp", "Phase 20" },
    { FeatureID::PriorityQueuing,         "Priority Queuing",           "Request priority management",               LicenseTierV2::Enterprise,   false, false, false, "src/priority_queue.cpp", "Phase 22" },
    { FeatureID::RateLimitingEngine,      "Rate Limiting Engine",       "Request rate control",                      LicenseTierV2::Enterprise,   true, false, false, "src/rate_limit.cpp", "Phase 22" },
    { FeatureID::AuditLogging,            "Audit Logging",              "Enterprise audit trail logging",             LicenseTierV2::Enterprise,   true, true, false, "src/core/enterprise_telemetry_compliance.cpp", "Phase 17" },
    { FeatureID::APIKeyManagement,        "API Key Management",         "Multi-key API management",                  LicenseTierV2::Enterprise,   true, true, false, "src/api_keys.cpp", "Phase 22" },
    { FeatureID::ModelSigningVerify,      "Model Signing & Verify",     "Cryptographic model verification",          LicenseTierV2::Enterprise,   true, false, false, "src/model_signing.cpp", "Phase 23" },
    { FeatureID::RBAC,                    "RBAC",                       "Role-based access control",                 LicenseTierV2::Enterprise,   false, false, false, "src/rbac.cpp", "Phase 23" },
    { FeatureID::ObservabilityDashboard,  "Observability Dashboard",    "Real-time monitoring dashboard",            LicenseTierV2::Enterprise,   true, true, false, "src/win32app/Win32IDE_Telemetry.cpp", "Phase 24" },
    { FeatureID::AVX512Acceleration,      "AVX-512 Acceleration",       "AVX-512 premium compute kernels",           LicenseTierV2::Enterprise,   true, true, false, "src/core/native_speed_layer.cpp", "Phase 25" },
    { FeatureID::RawrTunerIDE,            "RawrTuner IDE",              "Integrated tuner/fine-tuning IDE",          LicenseTierV2::Enterprise,   false, false, false, "src/tuner.cpp", "Phase 26" },
    // Sovereign (55-64)
    { FeatureID::AirGappedDeploy,         "Air-Gapped Deploy",          "Offline deployment capabilities",           LicenseTierV2::Sovereign,    false, false, false, "src/airgap.cpp", "Phase 27" },
    { FeatureID::HSMIntegration,          "HSM Integration",            "Hardware security module support",          LicenseTierV2::Sovereign,    false, false, false, "src/hsm.cpp", "Phase 27" },
    { FeatureID::FIPS140_2Compliance,     "FIPS 140-2 Compliance",      "Federal crypto compliance",                 LicenseTierV2::Sovereign,    false, false, false, "src/fips.cpp", "Phase 27" },
    { FeatureID::CustomSecurityPolicies,  "Custom Security Policies",   "User-defined security rules",               LicenseTierV2::Sovereign,    false, false, false, "src/security_policy.cpp", "Phase 27" },
    { FeatureID::SovereignKeyMgmt,        "Sovereign Key Management",   "National key management",                   LicenseTierV2::Sovereign,    false, false, false, "src/sovereign_keys.cpp", "Phase 27" },
    { FeatureID::ClassifiedNetwork,       "Classified Network",         "Classified network operation",              LicenseTierV2::Sovereign,    false, false, false, "src/classified.cpp", "Phase 27" },
    { FeatureID::ImmutableAuditLogs,      "Immutable Audit Logs",       "Tamper-evident audit trail",                LicenseTierV2::Sovereign,    false, false, false, "src/immutable_logs.cpp", "Phase 27" },
    { FeatureID::KubernetesSupport,       "Kubernetes Support",         "K8s deployment and scaling",                LicenseTierV2::Sovereign,    false, false, false, "src/k8s.cpp", "Phase 27" },
    { FeatureID::TamperDetection,         "Tamper Detection",           "Runtime tamper detection",                  LicenseTierV2::Sovereign,    true, false, false, "src/tamper_detect.cpp", "Phase 27" },
    { FeatureID::SecureBootChain,         "Secure Boot Chain",          "Verified boot chain",                       LicenseTierV2::Sovereign,    false, false, false, "src/secure_boot.cpp", "Phase 27" },
};

const FeatureDefV2* const g_FeatureManifest = s_featureManifest;

const char* featureName(FeatureID id) {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < static_cast<uint32_t>(FeatureID::COUNT)) {
        return s_featureManifest[idx].name;
    }
    return "Unknown";
}

} // namespace RawrXD::License


