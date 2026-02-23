// ============================================================================
// license_validator_manifest.cpp — Feature manifest for IDE / license_gate_validator
// ============================================================================
// Defines g_FeatureManifest and featureName only. Used when building
// RawrXD-Win32IDE or license_gate_validator without linking src/enterprise_license.cpp
// (which also defines EnterpriseLicenseV2 and would duplicate enterprise_licensev2_impl).
// ============================================================================
#include "../../include/enterprise_license.h"

// SCAFFOLD_187: License validator and manifest


namespace RawrXD::License {

const FeatureDefV2 g_FeatureManifest[TOTAL_FEATURES] = {
    // ── Community Tier (0–5) ──
    { FeatureID::BasicGGUFLoading,      "Basic GGUF Loading",       "Load and parse GGUF model files",        LicenseTierV2::Community,    true,  true,  false, "src/gguf_loader.cpp",                "Core" },
    { FeatureID::Q4Quantization,        "Q4_0/Q4_1 Quantization",  "Basic 4-bit quantization formats",       LicenseTierV2::Community,    true,  true,  false, "src/engine/gguf_core.cpp",           "Core" },
    { FeatureID::CPUInference,          "CPU Inference",            "CPU-based model inference",              LicenseTierV2::Community,    true,  true,  false, "src/cpu_inference_engine.cpp",        "Core" },
    { FeatureID::BasicChatUI,           "Basic Chat UI",            "Win32 chat panel interface",             LicenseTierV2::Community,    true,  true,  false, "src/ui/chat_panel.cpp",              "Core" },
    { FeatureID::ConfigFileSupport,     "Config File Support",      "JSON configuration loading",             LicenseTierV2::Community,    true,  true,  false, "src/config/IDEConfig.cpp",           "Core" },
    { FeatureID::SingleModelSession,    "Single Model Session",     "Load one model at a time",               LicenseTierV2::Community,    true,  true,  false, "src/engine/rawr_engine.cpp",          "Core" },
    // ── Professional (6–26) ──
    { FeatureID::MemoryHotpatching,     "Memory Hotpatching",       "Direct RAM patching via VirtualProtect", LicenseTierV2::Professional, true,  true,  false, "src/core/model_memory_hotpatch.cpp",  "Phase 14" },
    { FeatureID::ByteLevelHotpatching,  "Byte-Level Hotpatching",   "Precision GGUF binary modification",    LicenseTierV2::Professional, true,  true,  false, "src/core/byte_level_hotpatcher.cpp",  "Phase 14" },
    { FeatureID::ServerHotpatching,     "Server Hotpatching",       "Inference request/response patching",    LicenseTierV2::Professional, true,  true,  false, "src/server/gguf_server_hotpatch.cpp", "Phase 14" },
    { FeatureID::UnifiedHotpatchManager,"Unified Hotpatch Manager", "Three-layer hotpatch coordination",      LicenseTierV2::Professional, true,  true,  false, "src/core/unified_hotpatch_manager.cpp","Phase 14" },
    { FeatureID::Q5Q8F16Quantization,   "Q5/Q8/F16 Quantization",  "Higher-precision quant formats",         LicenseTierV2::Professional, true,  true,  false, "src/engine/gguf_core.cpp",           "Phase 22" },
    { FeatureID::MultiModelLoading,     "Multi-Model Loading",      "Load multiple models simultaneously",    LicenseTierV2::Professional, true,  true,  false, "src/model_registry.cpp",             "Phase 22" },
    { FeatureID::CUDABackend,           "CUDA Backend",             "NVIDIA GPU-accelerated inference",       LicenseTierV2::Professional, true,  true,  false, "src/gpu/cuda_inference_engine.cpp",   "Phase 25" },
    { FeatureID::AdvancedSettingsPanel, "Advanced Settings Panel",  "Extended IDE configuration panel",       LicenseTierV2::Professional, true,  true,  false, "src/win32app/Win32IDE_Settings.cpp",  "Phase 33" },
    { FeatureID::PromptTemplates,       "Prompt Templates",         "Reusable prompt template library",       LicenseTierV2::Professional, true,  true,  false, "src/agentic/FIMPromptBuilder.cpp",    "Phase 10" },
    { FeatureID::TokenStreaming,        "Token Streaming",         "Real-time token-by-token output",        LicenseTierV2::Professional, true,  true,  false, "src/win32app/Win32IDE_StreamingUX.cpp","Phase 9" },
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
    // ── Enterprise (27–52) ──
    { FeatureID::DualEngine800B,        "800B Dual-Engine",         "Multi-shard 800B model inference",       LicenseTierV2::Enterprise,   true,  true,  false, "src/dual_engine_inference.cpp",       "Phase 21" },
    { FeatureID::AgenticFailureDetect,  "Agentic Failure Detection","Detect refusal/hallucination/timeout",   LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agentic_failure_detector.cpp","Phase 18" },
    { FeatureID::AgenticPuppeteer,      "Agentic Puppeteer",        "Auto-correct failed responses",          LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agentic_puppeteer.cpp",     "Phase 18" },
    { FeatureID::AgenticSelfCorrection, "Agentic Self-Correction",  "Iterative self-repair loop",             LicenseTierV2::Enterprise,   true,  true,  false, "src/agent/agent_self_repair.cpp",     "Phase 18" },
    { FeatureID::ProxyHotpatching,      "Proxy Hotpatching",        "Byte-level output rewriting",            LicenseTierV2::Enterprise,   true,  true,  false, "src/core/proxy_hotpatcher.cpp",       "Phase 14" },
    { FeatureID::ServerSidePatching,    "Server-Side Patching",     "Runtime server request modification",    LicenseTierV2::Enterprise,   true,  true,  false, "src/server/gguf_server_hotpatch.cpp", "Phase 14" },
    { FeatureID::SchematicStudioIDE,    "SchematicStudio IDE",      "Visual IDE subsystem",                   LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE.cpp",           "Phase 31" },
    { FeatureID::WiringOracleDebug,     "WiringOracle Debug",       "Advanced wiring diagnostic tool",        LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_AuditDashboard.cpp","Phase 31" },
    { FeatureID::FlashAttention,        "Flash Attention",          "AVX-512 flash attention kernels",        LicenseTierV2::Enterprise,   true,  true,  false, "src/core/flash_attention.cpp",        "Phase 23" },
    { FeatureID::SpeculativeDecoding,   "Speculative Decoding",     "Draft model assisted generation",        LicenseTierV2::Enterprise,   true,  true,  false, "src/gpu/speculative_decoder.cpp",    "Phase 23" },
    { FeatureID::ModelSharding,         "Model Sharding",           "Split model across storage/memory",      LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/layer_offload_manager.cpp",  "Phase 9" },
    { FeatureID::TensorParallel,        "Tensor Parallel",          "Parallel tensor computation",            LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/adaptive_pipeline_parallel.cpp","Phase 22" },
    { FeatureID::PipelineParallel,      "Pipeline Parallel",        "Pipeline-parallel inference stages",     LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/distributed_pipeline_orchestrator.cpp","Phase 13" },
    { FeatureID::ContinuousBatching,    "Continuous Batching",      "Dynamic request batching",               LicenseTierV2::Enterprise,   true,  true,  false, "src/core/execution_scheduler.cpp",   "Phase 9" },
    { FeatureID::GPTQQuantization,      "GPTQ Quantization",        "GPTQ-format model loading",              LicenseTierV2::Enterprise,   true,  true,  false, "src/engine/gguf_core.cpp",           "Phase 23" },
    { FeatureID::AWQQuantization,       "AWQ Quantization",         "AWQ-format model loading",               LicenseTierV2::Enterprise,   true,  true,  false, "src/engine/gguf_core.cpp",           "Phase 23" },
    { FeatureID::CustomQuantSchemes,    "Custom Quant Schemes",     "User-defined quantization formats",      LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/gpu_kernel_autotuner.cpp",   "Phase 23" },
    { FeatureID::MultiGPULoadBalance,   "Multi-GPU Load Balance",   "Distribute across multiple GPUs",        LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/accelerator_router.cpp",     "Phase 30" },
    { FeatureID::DynamicBatchSizing,    "Dynamic Batch Sizing",     "Auto-tune batch size at runtime",        LicenseTierV2::Enterprise,   true,  true,  true,  "src/core/execution_scheduler.cpp",    "Phase 9" },
    { FeatureID::PriorityQueuing,       "Priority Queuing",         "Request priority queue management",      LicenseTierV2::Enterprise,   true,  true,  false, "src/core/priority_queuing.cpp",       "Phase 50" },
    { FeatureID::RateLimitingEngine,    "Rate Limiting Engine",     "Per-user/API rate limiting",             LicenseTierV2::Enterprise,   true,  true,  false, "src/core/rate_limiting_engine.cpp",   "Phase 50" },
    { FeatureID::AuditLogging,          "Audit Logging",            "Full operation audit trail",              LicenseTierV2::Enterprise,   true,  true,  false, "src/core/enterprise_telemetry_compliance.cpp","Phase 17" },
    { FeatureID::APIKeyManagement,      "API Key Management",       "Generate and rotate API keys",            LicenseTierV2::Enterprise,   true,  true,  true,  "src/auth/rbac_engine.cpp",           "Phase 50" },
    { FeatureID::ModelSigningVerify,    "Model Signing/Verify",     "Cryptographic model integrity check",     LicenseTierV2::Enterprise,   true,  true,  false, "src/core/update_signature.cpp",       "Phase 50" },
    { FeatureID::RBAC,                  "RBAC",                     "Role-based access control",               LicenseTierV2::Enterprise,   true,  true,  false, "src/auth/rbac_engine.cpp",           "Phase 50" },
    { FeatureID::ObservabilityDashboard,"Observability Dashboard",  "System metrics and health dashboard",     LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_TelemetryDashboard.cpp","Phase 34" },
    { FeatureID::AVX512Acceleration,    "AVX-512 Acceleration",     "AVX-512 optimized inference kernels",    LicenseTierV2::Enterprise,   true,  true,  false, "src/core/avx512_kernels.cpp",         "Core" },
    { FeatureID::RawrTunerIDE,          "RawrTuner IDE",            "Fine-tuning and optimization IDE",       LicenseTierV2::Enterprise,   true,  true,  false, "src/win32app/Win32IDE_Tuner.cpp",       "Phase 35" },
    // ── Sovereign (55–64) ──
    { FeatureID::AirGappedDeploy,       "Air-Gapped Deploy",        "Fully offline deployment mode",          LicenseTierV2::Sovereign,    true,  true,  false, "src/security/airgap_deployer.cpp",    "Phase 50" },
    { FeatureID::HSMIntegration,        "HSM Integration",          "Hardware security module support",        LicenseTierV2::Sovereign,    true,  true,  false, "src/security/hsm_integration.cpp",   "Phase 50" },
    { FeatureID::FIPS140_2Compliance,   "FIPS 140-2 Compliance",    "Federal crypto standard compliance",      LicenseTierV2::Sovereign,    true,  true,  false, "src/security/fips_compliance.cpp",   "Phase 50" },
    { FeatureID::CustomSecurityPolicies,"Custom Security Policies", "Organization-specific security rules",    LicenseTierV2::Sovereign,    true,  true,  false, "src/security/policy_engine.cpp",      "Phase 50" },
    { FeatureID::SovereignKeyMgmt,      "Sovereign Key Mgmt",       "Gov-grade key management system",        LicenseTierV2::Sovereign,    true,  true,  false, "src/security/sovereign_keymgmt.cpp",  "Phase 50" },
    { FeatureID::ClassifiedNetwork,     "Classified Network",       "Classified network deployment",           LicenseTierV2::Sovereign,    true,  true,  false, "src/security/classified_network.cpp","Phase 50" },
    { FeatureID::ImmutableAuditLogs,    "Immutable Audit Logs",     "Blockchain-based tamper-proof logs",     LicenseTierV2::Sovereign,    true,  true,  false, "src/security/audit_log_immutable.cpp","Phase 50" },
    { FeatureID::KubernetesSupport,     "Kubernetes Support",       "Enterprise orchestration platform",       LicenseTierV2::Sovereign,    true,  true,  false, "src/orchestration/kubernetes_adapter.cpp","Phase 50" },
    { FeatureID::TamperDetection,       "Tamper Detection",         "Runtime binary integrity checks",         LicenseTierV2::Sovereign,    true,  true,  false, "src/core/license_anti_tampering.cpp", "Phase 50" },
    { FeatureID::SecureBootChain,       "Secure Boot Chain",        "Verified boot sequence",                  LicenseTierV2::Sovereign,    true,  true,  false, "src/security/tamper_detection.cpp",  "Phase 50" },
};

const char* featureName(FeatureID id) {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < TOTAL_FEATURES) return g_FeatureManifest[idx].name;
    return "Unknown";
}

} // namespace RawrXD::License
