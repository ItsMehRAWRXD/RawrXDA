// =============================================================================
// Enterprise License Regression Tests
// Spec: tools.instructions.md §4 — Comprehensive Testing
// Covers: license validation paths + feature gating invariants
// =============================================================================

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

#include "enterprise_license.h"
#include "enterprise_feature_manager.hpp"
#include "feature_flags_runtime.h"

static int g_passed = 0;
static int g_failed = 0;
static int g_total  = 0;

#define TEST(name)                                                     \
    do {                                                               \
        g_total++;                                                     \
        const char* _tname = #name;                                    \
        fprintf(stderr, "  [TEST] %-50s ", _tname);                    \
        try {

#define PASS                                                           \
            g_passed++;                                                \
            fprintf(stderr, "PASS\n");                                 \
        } catch (const std::exception& ex) {                           \
            g_failed++;                                                \
            fprintf(stderr, "FAIL (exception: %s)\n", ex.what());      \
        } catch (...) {                                                \
            g_failed++;                                                \
            fprintf(stderr, "FAIL (unknown exception)\n");            \
        }                                                              \
    } while(0)

#define ASSERT_TRUE(cond)                                              \
    if (!(cond)) {                                                     \
        g_failed++;                                                    \
        fprintf(stderr, "FAIL (line %d: %s)\n", __LINE__, #cond);      \
        return;                                                        \
    }

#define ASSERT_EQ(a, b)                                                \
    if ((a) != (b)) {                                                  \
        g_failed++;                                                    \
        fprintf(stderr, "FAIL (line %d: %s != %s)\n", __LINE__, #a, #b);\
        return;                                                        \
    }

static bool IsValidState(RawrXD::LicenseState state) {
    switch (state) {
        case RawrXD::LicenseState::Invalid:
        case RawrXD::LicenseState::ValidTrial:
        case RawrXD::LicenseState::ValidEnterprise:
        case RawrXD::LicenseState::ValidOEM:
        case RawrXD::LicenseState::Expired:
        case RawrXD::LicenseState::HardwareMismatch:
        case RawrXD::LicenseState::Tampered:
        case RawrXD::LicenseState::ValidPro:
            return true;
        default:
            return false;
    }
}

static bool IsValidLicensedState(RawrXD::LicenseState state) {
    return state == RawrXD::LicenseState::ValidEnterprise ||
           state == RawrXD::LicenseState::ValidOEM ||
           state == RawrXD::LicenseState::ValidPro ||
           state == RawrXD::LicenseState::ValidTrial;
}

static void test_license_init_and_state() {
    fprintf(stderr, "\n=== Enterprise License Init ===\n");

    TEST(initialize_license) {
        bool ok = RawrXD::EnterpriseLicense::Instance().Initialize();
        ASSERT_TRUE(ok);
    PASS; }

    TEST(state_is_known_enum) {
        auto state = RawrXD::EnterpriseLicense::Instance().GetState();
        ASSERT_TRUE(IsValidState(state));
    PASS; }
}

static void test_feature_definitions_and_masks() {
    fprintf(stderr, "\n=== Feature Definitions ===\n");

    auto& mgr = EnterpriseFeatureManager::Instance();
    mgr.Initialize();

    TEST(definition_count_is_8) {
        const auto& defs = mgr.GetFeatureDefinitions();
        ASSERT_EQ(defs.size(), static_cast<size_t>(8));
    PASS; }

    TEST(masks_are_unique_and_nonzero) {
        std::set<uint64_t> masks;
        const auto& defs = mgr.GetFeatureDefinitions();
        for (const auto& def : defs) {
            ASSERT_TRUE(def.mask != 0);
            masks.insert(def.mask);
        }
        ASSERT_EQ(masks.size(), defs.size());
    PASS; }
}

static void test_feature_gating_consistency() {
    fprintf(stderr, "\n=== Feature Gating Consistency ===\n");

    auto& lic = RawrXD::EnterpriseLicense::Instance();
    auto& mgr = EnterpriseFeatureManager::Instance();

    TEST(licensed_mask_matches_manager) {
        const auto& defs = mgr.GetFeatureDefinitions();
        for (const auto& def : defs) {
            bool licHas = lic.HasFeatureMask(def.mask);
            bool mgrHas = mgr.IsFeatureLicensed(def.mask);
            ASSERT_EQ(licHas, mgrHas);
        }
    PASS; }

    TEST(gate_returns_enabled_state) {
        const auto& defs = mgr.GetFeatureDefinitions();
        for (const auto& def : defs) {
            bool enabled = mgr.IsFeatureEnabled(def.mask);
            bool gateResult = mgr.Gate(def.mask, "test");
            ASSERT_EQ(gateResult, enabled);
        }
    PASS; }

    TEST(dual_engine_flag_consistency) {
        bool hasFlag = lic.HasFeatureMask(RawrXD::LicenseFeature::DualEngine800B);
        bool isUnlocked = lic.Is800BUnlocked();
        ASSERT_EQ(hasFlag, isUnlocked);
    PASS; }
}

static void test_revalidate_behavior() {
    fprintf(stderr, "\n=== License Revalidate ===\n");

    auto& lic = RawrXD::EnterpriseLicense::Instance();

    TEST(revalidate_does_not_crash) {
        bool ok = lic.Revalidate();
        auto state = lic.GetState();
        if (ok) {
            ASSERT_TRUE(IsValidLicensedState(state));
        }
    PASS; }
}

static void test_v2_key_validation() {
    fprintf(stderr, "\n=== License V2 Key Validation ===\n");

    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
    auto init = lic.initialize();

    TEST(v2_initialize_ok) {
        ASSERT_TRUE(init.success);
    PASS; }

    TEST(v2_create_and_validate_key) {
        RawrXD::License::LicenseKeyV2 key{};
        auto create = lic.createKey(RawrXD::License::LicenseTierV2::Community, 1,
                                    "unit-test-secret", &key);
        ASSERT_TRUE(create.success);

        auto validate = lic.validateKey(key);
        ASSERT_TRUE(validate.success);
    PASS; }

    TEST(v2_signature_tamper_fails) {
        RawrXD::License::LicenseKeyV2 key{};
        auto create = lic.createKey(RawrXD::License::LicenseTierV2::Community, 1,
                                    "unit-test-secret", &key);
        ASSERT_TRUE(create.success);
        std::memset(key.signature, 0, sizeof(key.signature));
        auto validate = lic.validateKey(key);
        ASSERT_TRUE(!validate.success);
    PASS; }
}

static void test_v2_feature_gating() {
    fprintf(stderr, "\n=== License V2 Feature Gating ===\n");

    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();

    TEST(v2_community_gate_denies_dual_engine) {
        RawrXD::License::LicenseKeyV2 key{};
        auto create = lic.createKey(RawrXD::License::LicenseTierV2::Community, 1,
                                    "unit-test-secret", &key);
        ASSERT_TRUE(create.success);
        auto load = lic.loadKeyFromMemory(&key, sizeof(key));
        ASSERT_TRUE(load.success);

        ASSERT_TRUE(!lic.isFeatureLicensed(RawrXD::License::FeatureID::DualEngine800B));
        ASSERT_TRUE(!lic.gate(RawrXD::License::FeatureID::DualEngine800B, "test"));
    PASS; }

    TEST(v2_enterprise_gate_allows_dual_engine) {
        RawrXD::License::LicenseKeyV2 key{};
        auto create = lic.createKey(RawrXD::License::LicenseTierV2::Enterprise, 1,
                                    "unit-test-secret", &key);
        ASSERT_TRUE(create.success);
        auto load = lic.loadKeyFromMemory(&key, sizeof(key));
        ASSERT_TRUE(load.success);

        ASSERT_TRUE(lic.isFeatureLicensed(RawrXD::License::FeatureID::DualEngine800B));
        ASSERT_TRUE(lic.gate(RawrXD::License::FeatureID::DualEngine800B, "test"));
    PASS; }
}

static void test_v2_feature_flags_runtime() {
    fprintf(stderr, "\n=== Feature Flags Runtime ===\n");

    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
    auto& flags = RawrXD::Flags::FeatureFlagsRuntime::Instance();

    TEST(v2_flags_respect_license_mask) {
        RawrXD::License::LicenseKeyV2 key{};
        auto create = lic.createKey(RawrXD::License::LicenseTierV2::Professional, 1,
                                    "unit-test-secret", &key);
        ASSERT_TRUE(create.success);
        auto load = lic.loadKeyFromMemory(&key, sizeof(key));
        ASSERT_TRUE(load.success);

        ASSERT_TRUE(!flags.isEnabled(RawrXD::License::FeatureID::DualEngine800B));
        ASSERT_TRUE(flags.isEnabled(RawrXD::License::FeatureID::BasicGGUFLoading));
    PASS; }
}

int main() {
    test_license_init_and_state();
    test_feature_definitions_and_masks();
    test_feature_gating_consistency();
    test_revalidate_behavior();
    test_v2_key_validation();
    test_v2_feature_gating();
    test_v2_feature_flags_runtime();

    fprintf(stderr, "\n=== Summary ===\n");
    fprintf(stderr, "  Total:  %d\n", g_total);
    fprintf(stderr, "  Passed: %d\n", g_passed);
    fprintf(stderr, "  Failed: %d\n", g_failed);

    return (g_failed == 0) ? 0 : 1;
}
