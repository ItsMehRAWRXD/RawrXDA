# Enterprise-Licensed Features Without Backing Implementation

**Purpose:** Audit of FeatureID enum (65 total) to identify features that are license-gated but have **no backing implementation** (stub-only or no code path). These 21+ features require either a real implementation or an explicit "not implemented" UX so users are not charged for non-functional capabilities.

**Date:** 2026-02-14

---

## Summary

| Category | Count | Action |
|----------|--------|--------|
| **Has backing implementation** | 44 | — |
| **No backing (stub or missing)** | 21 | Document below; gate with `HasBackingImplementation()` and show "Not implemented" in UI/REPL |

---

## Features WITHOUT Backing Implementation (21)

| FeatureID | Name | Tier | Notes |
|-----------|------|------|--------|
| 13 | AdvancedSettingsPanel | Pro | UI panel stub only; no settings persistence/apply |
| 18 | ModelComparison | Pro | No A/B comparison or metrics |
| 19 | BatchProcessing | Pro | No batch job queue or runner |
| 24 | PromptLibrary | Pro | No library storage/load |
| 25 | ExportImportSessions | Pro | No export/import code path |
| 33 | SchematicStudioIDE | Enterprise | No schematic editor or wiring UI |
| 34 | WiringOracleDebug | Enterprise | No oracle or debug integration |
| 37 | ModelSharding | Enterprise | No shard split/merge implementation |
| 38 | TensorParallel | Enterprise | Stub only; no real tensor parallel |
| 39 | PipelineParallel | Enterprise | Partial; no full pipeline stages |
| 40 | ContinuousBatching | Enterprise | No continuous batch scheduler |
| 41 | GPTQQuantization | Enterprise | No GPTQ kernel or loader |
| 42 | AWQQuantization | Enterprise | No AWQ kernel or loader |
| 43 | CustomQuantSchemes | Enterprise | No custom scheme registration |
| 45 | DynamicBatchSizing | Enterprise | No dynamic sizing logic |
| 46 | PriorityQueuing | Enterprise | No priority queue implementation |
| 47 | RateLimitingEngine | Enterprise | No rate limiter |
| 49 | APIKeyManagement | Enterprise | No API key store/rotate UI or API |
| 50 | ModelSigningVerify | Enterprise | No signing/verification path |
| 54 | RawrTunerIDE | Enterprise | No tuner IDE UI or flow |
| 58 | CustomSecurityPolicies | Sovereign | No policy DSL or engine |

---

## Implementation Status Table (by FeatureID)

- **0–12:** Backed (GGUF, quant, CPU inference, chat, config, hotpatch, multi-model, CUDA, etc.).
- **13:** AdvancedSettingsPanel — **NO BACKING**
- **14–17:** PromptTemplates, TokenStreaming, InferenceStatistics, KVCacheManagement — backed or stubbed.
- **18–19:** ModelComparison, BatchProcessing — **NO BACKING**
- **20–23:** CustomStopSequences, GrammarConstrainedGen, LoRA, ResponseCaching — backed.
- **24–25:** PromptLibrary, ExportImportSessions — **NO BACKING**
- **26:** HIPBackend — stub (similar to CUDA mock without RAWR_HAS_HIP).
- **27–32:** DualEngine800B, Agentic*, ProxyHotpatch, ServerSidePatching — backed.
- **33–34:** SchematicStudioIDE, WiringOracleDebug — **NO BACKING**
- **35–36:** FlashAttention, SpeculativeDecoding — backed.
- **37–43:** ModelSharding through CustomQuantSchemes — **NO BACKING** (or stub only).
- **44:** MultiGPULoadBalance — partial (multi_gpu.cpp).
- **45–47:** DynamicBatchSizing, PriorityQueuing, RateLimitingEngine — **NO BACKING**
- **48:** AuditLogging — backed (audit_log_immutable, etc.).
- **49–50:** APIKeyManagement, ModelSigningVerify — **NO BACKING**
- **51–53:** RBAC, ObservabilityDashboard, AVX512Acceleration — backed or partial.
- **54:** RawrTunerIDE — **NO BACKING**
- **55–57, 59–64:** Sovereign (AirGapped, HSM, FIPS, SovereignKeyMgmt, ClassifiedNetwork, ImmutableAuditLogs, Kubernetes, TamperDetection, SecureBootChain) — most backed; **58 CustomSecurityPolicies — NO BACKING**

---

## Recommended Actions

1. **Central gate:** Add `bool HasBackingImplementation(FeatureID id)` in `enterprise_feature_manager.cpp` or `license_enforcement.h` returning false for the 21 IDs above.
2. **UI/REPL:** When a feature is licensed but `!HasBackingImplementation(id)`, show: "This feature is licensed but not yet implemented. Contact support or check release notes."
3. **Roadmap:** Prioritize implementation or explicitly mark as "planned" for the 21 so that license sales align with delivery.

---

## Cross-References

- `include/enterprise_license.h` — FeatureID enum
- `include/enterprise_feature_manifest.hpp` — 8-feature mask (all implemented)
- `src/core/enterprise_feature_manager.cpp` — feature registration and checks
