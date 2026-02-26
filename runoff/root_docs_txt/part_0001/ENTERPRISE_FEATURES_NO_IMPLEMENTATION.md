# Enterprise-Licensed Features: Implementation Status

All features listed below have `implemented: false` in `g_FeatureManifest`
(defined in `src/core/ide_linker_bridge.cpp`, 65 entries).

**Runtime behavior:** `EnterpriseLicenseV2::gate(FeatureID id, caller)` checks
`isFeatureImplemented(id)` *before* the license bitmask. If the feature is not
implemented, `gate()` calls `recordAudit(id, false, caller, "Feature not
implemented")` and returns `false`. No unimplemented feature can be activated
regardless of license tier.

Reference: `src/core/enterprise_licensev2_impl.cpp`, `include/enterprise_license.h`

---

## Professional Tier (implemented: false)

| # | FeatureID | Name | Source (planned) | Phase | Notes |
|---|-----------|------|------------------|-------|-------|
| 1 | `CUDABackend` | CUDA Backend | `src/cuda_backend.cpp` | 7 | `src/gpu/cuda_inference_engine.cpp` exists as 100% MOCK when `RAWR_HAS_CUDA` is not defined. `initializeCUDA()` logs: *"MOCK: no GPU calls. Build with -DRAWR_HAS_CUDA and link cuda.lib for real GPU."* No `cudaMalloc`, `cudaSetDevice`, or GPU kernels run without the define. |
| 2 | `LoRAAdapterSupport` | LoRA Adapter Support | `src/lora.cpp` | 13 | No LoRA adapter load/merge logic. |
| 3 | `HIPBackend` | HIP Backend | `src/hip_backend.cpp` | 7 | AMD GPU — no HIP runtime linked. |

## Enterprise Tier (implemented: false)

| # | FeatureID | Name | Source (planned) | Phase | Notes |
|---|-----------|------|------------------|-------|-------|
| 4 | `SchematicStudioIDE` | Schematic Studio IDE | `src/schematic.cpp` | 16 | Visual wiring design tool — no UI. |
| 5 | `WiringOracleDebug` | Wiring Oracle Debug | `src/wiring_oracle.cpp` | 16 | Debug component wiring — no implementation. |
| 6 | `SpeculativeDecoding` | Speculative Decoding | `src/speculative.cpp` | 18 | Multi-draft speculative decoding — no engine. |
| 7 | `TensorParallel` | Tensor Parallel | `src/tensor_parallel.cpp` | 19 | Tensor parallelism — no implementation. |
| 8 | `ContinuousBatching` | Continuous Batching | `src/continuous_batch.cpp` | 20 | Dynamic batch scheduling — no scheduler. |
| 9 | `GPTQQuantization` | GPTQ Quantization | `src/gptq.cpp` | 20 | GPTQ-quality quantization — no implementation. |
| 10 | `AWQQuantization` | AWQ Quantization | `src/awq.cpp` | 20 | AWQ activation-aware quantization — no implementation. |
| 11 | `CustomQuantSchemes` | Custom Quant Schemes | `src/custom_quant.cpp` | 20 | User-defined quantization — no implementation. |
| 12 | `DynamicBatchSizing` | Dynamic Batch Sizing | `src/dynamic_batch.cpp` | 20 | Runtime batch size — no implementation. |
| 13 | `PriorityQueuing` | Priority Queuing | `src/priority_queue.cpp` | 22 | Request priority management — no implementation. |
| 14 | `RBAC` | RBAC | `src/rbac.cpp` | 23 | Role-based access control — no implementation. |
| 15 | `RawrTunerIDE` | RawrTuner IDE | `src/tuner.cpp` | 26 | Fine-tuning IDE — no implementation. |

## Sovereign Tier (implemented: false)

| # | FeatureID | Name | Source (planned) | Phase | Notes |
|---|-----------|------|------------------|-------|-------|
| 16 | `AirGappedDeploy` | Air-Gapped Deploy | `src/airgap.cpp` | 27 | Offline deployment — no implementation. |
| 17 | `HSMIntegration` | HSM Integration | `src/hsm.cpp` | 27 | `src/security/hsm_integration.cpp` exists with full PKCS#11 code behind `RAWR_HAS_PKCS11`. Default build uses CNG session key. Manifest lists `implemented: false` because no production PKCS#11 link. Warning: *"Stub mode: PKCS#11 not linked."* |
| 18 | `FIPS140_2Compliance` | FIPS 140-2 | `src/fips.cpp` | 27 | `src/security/fips_compliance.cpp` exists with OpenSSL FIPS code behind `RAWR_HAS_FIPS`. Default build uses CNG AES/SHA. Manifest lists `implemented: false`. One-time warning: *"Default build: FIPS mode not available. Use -DRAWR_HAS_FIPS and link OpenSSL FIPS module for production compliance."* |
| 19 | `CustomSecurityPolicies` | Custom Security Policies | `src/security_policy.cpp` | 27 | User-defined security rules — no implementation. |
| 20 | `SovereignKeyMgmt` | Sovereign Key Mgmt | `src/sovereign_keys.cpp` | 27 | `src/security/sovereign_keymgmt.cpp` exists with CNG AES-256-CBC/GCM. Manifest lists `implemented: false` because default build uses `std::mt19937` for master KEK. One-time warning: *"Default build uses random master KEK. Use setMasterKeyFromExternal() with HSM-derived key."* |
| 21 | `ClassifiedNetwork` | Classified Network | `src/classified.cpp` | 27 | Classified network operation — no implementation. |
| 22 | `ImmutableAuditLogs` | Immutable Audit Logs | `src/immutable_logs.cpp` | 27 | Tamper-evident audit trail — no implementation. |
| 23 | `KubernetesSupport` | Kubernetes Support | `src/k8s.cpp` | 27 | K8s deployment and scaling — no implementation. |
| 24 | `SecureBootChain` | Secure Boot Chain | `src/secure_boot.cpp` | 27 | Verified boot chain — no implementation. |

---

## Security Stub Summary

| Area | Default Build Behavior | Production Build |
|------|----------------------|-----------------|
| **FIPS 140-2** | CNG AES-256-CBC & SHA-256 (NIST algorithms via bcrypt.lib). One-time `std::call_once` warning. `fipsMode` stays `false`. | `-DRAWR_HAS_FIPS` + OpenSSL FIPS module. `FIPS_mode_set(1)` enables validated mode. |
| **HSM / PKCS#11** | CNG AES-256-CBC with `BCryptGenRandom` session key. Warning logged in `connect()`. No HSM hardware. | `-DRAWR_HAS_PKCS11` + link `pkcs11.lib`. Real `C_Initialize`, `C_OpenSession`, `C_EncryptInit`. |
| **Sovereign Key Mgmt** | CNG AES-256-CBC/GCM for key-at-rest. Master KEK from `std::mt19937`. One-time `std::call_once` warning. | `setMasterKeyFromExternal()` with HSM-derived 256-bit key. Optionally `-DRAWR_HAS_PKCS11`. |
| **CUDA Inference** | 100% MOCK. `initializeCUDA()` logs *"MOCK: no GPU calls."* Host `malloc` replaces `cudaMalloc`. `forward()` does `memcpy`. | `-DRAWR_HAS_CUDA` + `cuda.lib` + `cuda_kernels.cu` (nvcc). Real GPU kernel launches. |

---

## How to verify at runtime

```cpp
auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();

// Returns false + audit entry for any feature with implemented==false
bool ok = lic.gate(RawrXD::License::FeatureID::CUDABackend, __FUNCTION__);

// Query manifest directly
const auto& def = lic.getFeatureDef(RawrXD::License::FeatureID::HSMIntegration);
printf("HSM implemented: %s\n", def.implemented ? "yes" : "no");

// Counts
printf("Implemented: %u / %u\n", lic.countImplemented(), TOTAL_FEATURES);
```

---

## References

- `src/core/ide_linker_bridge.cpp` — `g_FeatureManifest[TOTAL_FEATURES]`
- `src/core/enterprise_licensev2_impl.cpp` — `gate()`, `isFeatureImplemented()`
- `include/enterprise_license.h` — `FeatureDefV2`, `FeatureID`, `TOTAL_FEATURES`
- `src/security/fips_compliance.cpp` — FIPS stub + CNG + OpenSSL paths
- `src/security/hsm_integration.cpp` — HSM stub + CNG + PKCS#11 paths
- `src/security/sovereign_keymgmt.cpp` — Sovereign key mgmt + warning
- `src/gpu/cuda_inference_engine.cpp` — CUDA mock + real paths
