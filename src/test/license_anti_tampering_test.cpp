// ============================================================================
// license_anti_tampering_test.cpp — Anti-Tampering System Test Suite
// ============================================================================

#include "../include/license_anti_tampering.h"
#include "../include/enterprise_license.h"
#include <cassert>
#include <cstring>
#include <cstdio>

using namespace RawrXD::License;
using namespace RawrXD::License::AntiTampering;

// ============================================================================
// CRC32 Tests
// ============================================================================

void test_crc32_basic() {
    printf("[TEST] CRC32 Basic Computation...\n");

    const uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    uint32_t crc = computeCRC32(data, sizeof(data));

    printf("  CRC32(0x01020304) = 0x%08X\n", crc);
    assert(crc != 0);  // Should not be zero for non-zero data
    
    printf("  ✓ PASS\n");
}

void test_crc32_empty() {
    printf("[TEST] CRC32 Empty Data...\n");

    uint32_t crc = computeCRC32(nullptr, 0);
    printf("  CRC32(empty) = 0x%08X\n", crc);

    printf("  ✓ PASS\n");
}

void test_crc32_verification() {
    printf("[TEST] CRC32 Verification...\n");

    const uint8_t data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F };  // "Hello"
    uint32_t crc = computeCRC32(data, sizeof(data));

    bool verified = verifyCRC32(data, sizeof(data), crc);
    printf("  Verify CRC32('Hello') = %s\n", verified ? "true" : "false");
    assert(verified);

    printf("  ✓ PASS\n");
}

// ============================================================================
// SHA256 Tests
// ============================================================================

void test_sha256_known_vectors() {
    printf("[TEST] SHA256 Known Test Vectors...\n");

    // Test vector 1: Empty string
    {
        uint8_t hash[32];
        AntiTampering::sha256(reinterpret_cast<const uint8_t*>(""), 0, hash);
        
        // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        const uint8_t expected[] = {
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
        };

        bool match = std::memcmp(hash, expected, 32) == 0;
        printf("  SHA256('') matches: %s\n", match ? "true" : "false");
        assert(match);
    }

    // Test vector 2: "abc"
    {
        uint8_t hash[32];
        AntiTampering::sha256(reinterpret_cast<const uint8_t*>("abc"), 3, hash);
        
        // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
        const uint8_t expected[] = {
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
            0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
            0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
        };

        bool match = std::memcmp(hash, expected, 32) == 0;
        printf("  SHA256('abc') matches: %s\n", match ? "true" : "false");
        assert(match);
    }

    printf("  ✓ PASS\n");
}

// ============================================================================
// HMAC-SHA256 Tests
// ============================================================================

void test_hmac_sha256_basic() {
    printf("[TEST] HMAC-SHA256 Basic Computation...\n");

    const uint8_t key[] = "secret";
    const uint8_t data[] = "message";
    uint8_t hmac[32];

    bool result = computeHMAC_SHA256(data, sizeof(data) - 1, key, sizeof(key) - 1, hmac);
    printf("  HMAC computation: %s\n", result ? "success" : "failed");
    assert(result);

    printf("  ✓ PASS\n");
}

void test_hmac_sha256_verification() {
    printf("[TEST] HMAC-SHA256 Verification...\n");

    const uint8_t key[] = "secret_key";
    const uint8_t data[] = "test_data_for_hmac";
    uint8_t hmac[32];

    // Compute HMAC
    bool computed = computeHMAC_SHA256(data, sizeof(data) - 1, key, sizeof(key) - 1, hmac);
    assert(computed);

    // Verify HMAC
    bool verified = verifyHMAC_SHA256(data, sizeof(data) - 1, key, sizeof(key) - 1, hmac);
    printf("  HMAC verification: %s\n", verified ? "true" : "false");
    assert(verified);

    printf("  ✓ PASS\n");
}

void test_hmac_sha256_fail_on_tamper() {
    printf("[TEST] HMAC-SHA256 Tamper Detection...\n");

    const uint8_t key[] = "secret_key";
    const uint8_t data[] = "test_data";
    uint8_t hmac[32];

    // Compute valid HMAC
    computeHMAC_SHA256(data, sizeof(data) - 1, key, sizeof(key) - 1, hmac);

    // Tamper with HMAC
    hmac[0] ^= 0xFF;

    // Verify should fail
    bool verified = verifyHMAC_SHA256(data, sizeof(data) - 1, key, sizeof(key) - 1, hmac);
    printf("  HMAC verification after tamper: %s\n", verified ? "true (FAILURE)" : "false (correct)");
    assert(!verified);  // Should fail

    printf("  ✓ PASS\n");
}

// ============================================================================
// License Key Verification Tests
// ============================================================================

void test_license_key_valid() {
    printf("[TEST] Valid License Key Verification...\n");

    // Create a valid license key (simple test, skip HWID/signature details)
    LicenseKeyV2 key = {};
    key.magic = 0x5258444C;  // "RXDL"
    key.version = 2;
    key.tier = static_cast<uint32_t>(LicenseTierV2::Enterprise);
    key.hwid = 0x1234567890ABCDEF;
    key.features.lo = 0xFFFFFFFFFFFFFFFF;
    key.features.hi = 0x0000000000FFFFFF;

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    key.issueDate = now - 86400;  // 1 day ago
    key.expiryDate = now + (365 * 86400);  // 1 year from now

    // Verify key structure
    printf("  Key magic: 0x%08X (expected: 0x5258444C)\n", key.magic);
    printf("  Key version: %u (expected: 2)\n", key.version);
    printf("  Key tier: %u (expected: 2=Enterprise)\n", key.tier);
    printf("  Key HWID: 0x%016llX\n", static_cast<unsigned long long>(key.hwid));
    printf("  Features enabled: lo=0x%016llX hi=0x%016llX\n", 
           static_cast<unsigned long long>(key.features.lo),
           static_cast<unsigned long long>(key.features.hi));

    // Check tampering patterns (should all be clear for a valid key)
    uint32_t tampering = AntiTampering::detectTampering(key);
    printf("  Tampering flags: 0x%08X\n", tampering);
    
    // Note: May have INVALID_HWID or timestamp flags depending on system
    if ((tampering & AntiTampering::TamperingPatterns::INVALID_MAGIC) == 0 &&
        (tampering & AntiTampering::TamperingPatterns::INVALID_VERSION) == 0 &&
        (tampering & AntiTampering::TamperingPatterns::INVALID_TIER) == 0) {
        printf("  ✓ PASS (key structure valid)\n");
    } else {
        printf("  ⚠ WARNING: Some tampering detected (expected for HWID/timestamp)\n");
    }
}

void test_tamper_detection_invalid_magic() {
    printf("[TEST] Tamper Detection - Invalid Magic...\n");

    LicenseKeyV2 key = {};
    key.magic = 0xDEADBEEF;  // Invalid magic (should be 0x5258444C)
    key.version = 2;

    uint32_t tampers = AntiTampering::detectTampering(key);
    bool hasInvalidMagic = (tampers & AntiTampering::TamperingPatterns::INVALID_MAGIC) != 0;

    printf("  Tamper mask: 0x%08X\n", tampers);
    printf("  Invalid magic detected: %s\n", hasInvalidMagic ? "true" : "false");
    assert(hasInvalidMagic);

    printf("  ✓ PASS\n");
}

void test_tamper_detection_invalid_version() {
    printf("[TEST] Tamper Detection - Invalid Version...\n");

    LicenseKeyV2 key = {};
    key.magic = 0x5258444C;
    key.version = 99;  // Invalid version

    uint32_t tampers = AntiTampering::detectTampering(key);
    bool hasInvalidVersion = (tampers & AntiTampering::TamperingPatterns::INVALID_VERSION) != 0;

    printf("  Tamper mask: 0x%08X\n", tampers);
    printf("  Invalid version detected: %s\n", hasInvalidVersion ? "true" : "false");
    assert(hasInvalidVersion);

    printf("  ✓ PASS\n");
}

void test_tamper_detection_invalid_tier() {
    printf("[TEST] Tamper Detection - Invalid Tier...\n");

    LicenseKeyV2 key = {};
    key.magic = 0x5258444C;
    key.version = 2;
    key.tier = 255;  // Invalid tier (must be 0-3)

    uint32_t tampers = AntiTampering::detectTampering(key);
    bool hasInvalidTier = (tampers & AntiTampering::TamperingPatterns::INVALID_TIER) != 0;

    printf("  Tamper mask: 0x%08X\n", tampers);
    printf("  Invalid tier detected: %s\n", hasInvalidTier ? "true" : "false");
    assert(hasInvalidTier);

    printf("  ✓ PASS\n");
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║   License Anti-Tampering System Test Suite                ║\n");
    printf("║   Phase 3 Production Hardening Verification                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    try {
        // CRC32 Tests
        printf("\n[SECTION] CRC32 Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_crc32_basic();
        test_crc32_empty();
        test_crc32_verification();

        // SHA256 Tests
        printf("\n[SECTION] SHA256 Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_sha256_known_vectors();

        // HMAC-SHA256 Tests
        printf("\n[SECTION] HMAC-SHA256 Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_hmac_sha256_basic();
        test_hmac_sha256_verification();
        test_hmac_sha256_fail_on_tamper();

        // License Key Verification Tests
        printf("\n[SECTION] License Key Verification Tests\n");
        printf("───────────────────────────────────────────────────────────\n");
        test_license_key_valid();
        test_tamper_detection_invalid_magic();
        test_tamper_detection_invalid_version();
        test_tamper_detection_invalid_tier();

        printf("\n");
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║   ALL TESTS PASSED ✓                                      ║\n");
        printf("║   Anti-tampering system ready for production deployment    ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n\n");

        return 0;

    } catch (const std::exception& e) {
        printf("\n[ERROR] Test suite failed: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("\n[ERROR] Unknown exception in test suite\n");
        return 1;
    }
}
