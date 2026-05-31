// ============================================================================
// enterprise_feature_gate_test_simple.cpp — Enterprise Feature Gate Self-Test
// Simplified version without telemetry dependencies
// ============================================================================

#include "../../include/enterprise_license.h"
#include <cstdio>

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
    std::fprintf(stdout, "[FeatureGateTest] Testing 15 Phase 3 wired features...\n\n");

    auto& lic = EnterpriseLicenseV2::Instance();
    LicenseResult init = lic.initialize();
    if (!init.success) {
        std::fprintf(stderr, "[FeatureGateTest] INIT FAILED: %s (%d)\n",
            init.detail ? init.detail : "unknown", init.errorCode);
        return 1;
    }
    std::fprintf(stdout, "[FeatureGateTest] License system initialized.\n");

    const char* secret = "RawrXD-FeatureGate-Test-Secret";
    LicenseKeyV2 key{};
    LicenseResult keyRes = lic.createKey(LicenseTierV2::Sovereign, 1, secret, &key);
    if (!keyRes.success) {
        std::fprintf(stderr, "[FeatureGateTest] CREATE KEY FAILED: %s (%d)\n",
            keyRes.detail ? keyRes.detail : "unknown", keyRes.errorCode);
        return 2;
    }
    std::fprintf(stdout, "[FeatureGateTest] License key created (Sovereign tier, 1 day).\n");

    LicenseResult loadRes = lic.loadKeyFromMemory(&key, sizeof(key));
    if (!loadRes.success) {
        std::fprintf(stderr, "[FeatureGateTest] LOAD KEY FAILED: %s (%d)\n",
            loadRes.detail ? loadRes.detail : "unknown", loadRes.errorCode);
        return 3;
    }
    std::fprintf(stdout, "[FeatureGateTest] License key loaded.\n\n");
    std::fprintf(stdout, "[FeatureGateTest] ═══════════════════════════════════════════════\n");
    std::fprintf(stdout, "[FeatureGateTest] Testing 15 wired features (Phase 3):\n");
    std::fprintf(stdout, "[FeatureGateTest] ═══════════════════════════════════════════════\n\n");

    int passed = 0;
    int total = 15;

    // Phase 3 wired features (Professional tier: 8, Enterprise tier: 7)
    if (checkFeature(lic, FeatureID::ModelComparison, "ModelComparison")) passed++;
    if (checkFeature(lic, FeatureID::BatchProcessing, "BatchProcessing")) passed++;
    if (checkFeature(lic, FeatureID::CustomStopSequences, "CustomStopSequences")) passed++;
    if (checkFeature(lic, FeatureID::GrammarConstrainedGen, "GrammarConstrainedGen")) passed++;
    if (checkFeature(lic, FeatureID::LoRAAdapterSupport, "LoRAAdapterSupport")) passed++;
    if (checkFeature(lic, FeatureID::ResponseCaching, "ResponseCaching")) passed++;
    if (checkFeature(lic, FeatureID::PromptLibrary, "PromptLibrary")) passed++;
    if (checkFeature(lic, FeatureID::ExportImportSessions, "ExportImportSessions")) passed++;
    if (checkFeature(lic, FeatureID::ModelSharding, "ModelSharding")) passed++;
    if (checkFeature(lic, FeatureID::TensorParallel, "TensorParallel")) passed++;
    if (checkFeature(lic, FeatureID::PipelineParallel, "PipelineParallel")) passed++;
    if (checkFeature(lic, FeatureID::CustomQuantSchemes, "CustomQuantSchemes")) passed++;
    if (checkFeature(lic, FeatureID::MultiGPULoadBalance, "MultiGPULoadBalance")) passed++;
    if (checkFeature(lic, FeatureID::DynamicBatchSizing, "DynamicBatchSizing")) passed++;
    if (checkFeature(lic, FeatureID::APIKeyManagement, "APIKeyManagement")) passed++;

    std::fprintf(stdout, "\n[FeatureGateTest] ═══════════════════════════════════════════════\n");
    std::fprintf(stdout, "[FeatureGateTest] Result: %d/%d features passed\n", passed, total);
    std::fprintf(stdout, "[FeatureGateTest] ═══════════════════════════════════════════════\n\n");

    if (passed == total) {
        std::fprintf(stdout, "[FeatureGateTest] ✓ ALL TESTS PASSED\n");
        return 0;
    } else {
        std::fprintf(stderr, "[FeatureGateTest] ✗ SOME TESTS FAILED (%d/%d passed)\n", passed, total);
        return 4;
    }
}
