// ============================================================================
// enterprise_feature_gate_test.cpp — Enterprise Feature Gate Self-Test
// ============================================================================
// Small harness to validate license gates for the 15 wired features.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "../../include/enterprise_license.h"
#include <cstdio>
#include <cstring>

using RawrXD::License::EnterpriseLicenseV2;
using RawrXD::License::FeatureID;
using RawrXD::License::LicenseKeyV2;
using RawrXD::License::LicenseResult;
using RawrXD::License::LicenseTierV2;

static bool checkFeature(EnterpriseLicenseV2& lic, FeatureID id, const char* name) {
    if (!lic.isFeatureImplemented(id)) {
        std::fprintf(stderr, "[FeatureGateTest] NOT IMPLEMENTED: %s\n", name);
        return false;
    }
    if (!lic.gate(id, "enterprise_feature_gate_test")) {
        std::fprintf(stderr, "[FeatureGateTest] GATE FAILED: %s\n", name);
        return false;
    }
    std::fprintf(stdout, "[FeatureGateTest] OK: %s\n", name);
    return true;
}

int main() {
    std::fprintf(stdout, "[FeatureGateTest] Starting Enterprise feature gate self-test...\n");

    auto& lic = EnterpriseLicenseV2::Instance();
    LicenseResult init = lic.initialize();
    if (!init.success) {
        std::fprintf(stderr, "[FeatureGateTest] INIT FAILED: %s (%d)\n",
            init.detail ? init.detail : "unknown", init.errorCode);
        return 1;
    }

    const char* secret = "RawrXD-FeatureGate-Test-Secret";
    LicenseKeyV2 key{};
    LicenseResult keyRes = lic.createKey(LicenseTierV2::Sovereign, 1, secret, &key);
    if (!keyRes.success) {
        std::fprintf(stderr, "[FeatureGateTest] CREATE KEY FAILED: %s (%d)\n",
            keyRes.detail ? keyRes.detail : "unknown", keyRes.errorCode);
        return 2;
    }

    LicenseResult loadRes = lic.loadKeyFromMemory(&key, sizeof(key));
    if (!loadRes.success) {
        std::fprintf(stderr, "[FeatureGateTest] LOAD KEY FAILED: %s (%d)\n",
            loadRes.detail ? loadRes.detail : "unknown", loadRes.errorCode);
        return 3;
    }

    bool allOk = true;
    allOk &= checkFeature(lic, FeatureID::ModelComparison, "Model Comparison");
    allOk &= checkFeature(lic, FeatureID::BatchProcessing, "Batch Processing");
    allOk &= checkFeature(lic, FeatureID::CustomStopSequences, "Custom Stop Sequences");
    allOk &= checkFeature(lic, FeatureID::GrammarConstrainedGen, "Grammar-Constrained Gen");
    allOk &= checkFeature(lic, FeatureID::LoRAAdapterSupport, "LoRA Adapter Support");
    allOk &= checkFeature(lic, FeatureID::ResponseCaching, "Response Caching");
    allOk &= checkFeature(lic, FeatureID::PromptLibrary, "Prompt Library");
    allOk &= checkFeature(lic, FeatureID::ExportImportSessions, "Export/Import Sessions");
    allOk &= checkFeature(lic, FeatureID::ModelSharding, "Model Sharding");
    allOk &= checkFeature(lic, FeatureID::TensorParallel, "Tensor Parallel");
    allOk &= checkFeature(lic, FeatureID::PipelineParallel, "Pipeline Parallel");
    allOk &= checkFeature(lic, FeatureID::CustomQuantSchemes, "Custom Quant Schemes");
    allOk &= checkFeature(lic, FeatureID::MultiGPULoadBalance, "Multi-GPU Load Balance");
    allOk &= checkFeature(lic, FeatureID::DynamicBatchSizing, "Dynamic Batch Sizing");
    allOk &= checkFeature(lic, FeatureID::APIKeyManagement, "API Key Management");

    std::fprintf(stdout, "[FeatureGateTest] Result: %s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 4;
}
