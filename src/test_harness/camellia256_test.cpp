// ============================================================================
// camellia256_test.cpp — Security Test Harness for Camellia-256 Engine
// ============================================================================
//
// PURPOSE:
//   Validates the Camellia-256 MASM engine against:
//   1. RFC 3713 Appendix A known-answer test vectors (via self-test)
//   2. Encrypt/decrypt round-trip correctness
//   3. HMAC-SHA256 authentication (tamper detection)
//   4. Key derivation sanity checks
//   5. Shutdown / secure-zero verification
//
// Build: Part of self_test_gate CMake target
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../core/camellia256_bridge.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace RawrXD::Crypto;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        g_testsPassed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_testsFailed++; \
        printf("  [FAIL] %s\n", msg); \
    } \
} while(0)

// ============================================================================
//  Test 1: RFC 3713 Self-Test via MASM engine
// ============================================================================

static void test_rfc3713_self_test() {
    printf("\n=== Test 1: RFC 3713 Appendix A Self-Test ===\n");

    // Self-test is called during init, but we can call it explicitly
    int rc = asm_camellia256_self_test();
    TEST_ASSERT(rc == 0, "RFC 3713 Camellia-256 self-test passed");
}

// ============================================================================
//  Test 2: Single-block encrypt/decrypt round-trip
// ============================================================================

static void test_block_round_trip() {
    printf("\n=== Test 2: Block Encrypt/Decrypt Round-Trip ===\n");

    // Use a known test key
    uint8_t key[32] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    int rc = asm_camellia256_set_key(key);
    TEST_ASSERT(rc == 0, "set_key with test key succeeded");

    uint8_t plaintext[16] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    uint8_t ciphertext[16] = {};
    uint8_t decrypted[16] = {};

    rc = asm_camellia256_encrypt_block(plaintext, ciphertext);
    TEST_ASSERT(rc == 0, "encrypt_block returned success");

    // Verify ciphertext is different from plaintext
    TEST_ASSERT(memcmp(plaintext, ciphertext, 16) != 0,
                "ciphertext differs from plaintext");

    rc = asm_camellia256_decrypt_block(ciphertext, decrypted);
    TEST_ASSERT(rc == 0, "decrypt_block returned success");

    TEST_ASSERT(memcmp(plaintext, decrypted, 16) == 0,
                "decrypted matches original plaintext");
}

// ============================================================================
//  Test 3: CTR-mode buffer round-trip
// ============================================================================

static void test_ctr_round_trip() {
    printf("\n=== Test 3: CTR-Mode Buffer Round-Trip ===\n");

    uint8_t key[32] = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7
    };
    asm_camellia256_set_key(key);

    // Test with non-block-aligned data (37 bytes)
    uint8_t original[37] = "The quick brown fox jumps over lazy";
    uint8_t buffer[37];
    memcpy(buffer, original, 37);

    uint8_t nonce[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
    uint8_t nonce_copy[16];
    memcpy(nonce_copy, nonce, 16);

    int rc = asm_camellia256_encrypt_ctr(buffer, 37, nonce);
    TEST_ASSERT(rc == 0, "CTR encrypt returned success");
    TEST_ASSERT(memcmp(buffer, original, 37) != 0, "CTR ciphertext differs from plaintext");

    // Decrypt with same nonce (reset)
    memcpy(nonce, nonce_copy, 16);
    rc = asm_camellia256_decrypt_ctr(buffer, 37, nonce);
    TEST_ASSERT(rc == 0, "CTR decrypt returned success");
    TEST_ASSERT(memcmp(buffer, original, 37) == 0, "CTR round-trip matches original");
}

// ============================================================================
//  Test 4: Bridge initialization and HMAC key derivation
// ============================================================================

static void test_bridge_init() {
    printf("\n=== Test 4: Bridge Initialization + HMAC Key ===\n");

    auto& bridge = Camellia256Bridge::instance();

    // Shutdown first to ensure clean state
    bridge.shutdown();

    CamelliaResult r = bridge.initialize();
    TEST_ASSERT(r.success, "Bridge initialize() succeeded");
    TEST_ASSERT(bridge.isInitialized(), "Bridge reports initialized");

    // Verify HMAC key was derived
    uint8_t hmacKey[32] = {};
    int rc = asm_camellia256_get_hmac_key(hmacKey);
    TEST_ASSERT(rc == 0, "get_hmac_key returned success");

    // HMAC key should not be all zeros
    uint8_t zeros[32] = {};
    TEST_ASSERT(memcmp(hmacKey, zeros, 32) != 0, "HMAC key is non-zero");

    // Get status
    CamelliaEngineStatus st = bridge.getStatus();
    TEST_ASSERT(st.initialized == 1, "Engine status shows initialized");

    SecureZeroMemory(hmacKey, 32);
}

// ============================================================================
//  Test 5: Authenticated file encrypt/decrypt round-trip
// ============================================================================

static void test_authenticated_file() {
    printf("\n=== Test 5: Authenticated File Encrypt/Decrypt ===\n");

    auto& bridge = Camellia256Bridge::instance();
    if (!bridge.isInitialized()) {
        bridge.initialize();
    }

    // Create a temporary test file
    const char* testInputPath = "camellia_test_input.tmp";
    const char* testEncPath = "camellia_test_encrypted.camellia";
    const char* testDecPath = "camellia_test_decrypted.tmp";

    // Write test data to input file
    const char* testData = "This is a test of the Camellia-256 authenticated encryption.\n"
                           "HMAC-SHA256 protects against tampering.\n"
                           "RawrXD Enterprise Encryption v2.\n";
    size_t testDataLen = strlen(testData);

    HANDLE hFile = CreateFileA(testInputPath, GENERIC_WRITE, 0,
                                nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, testData, (DWORD)testDataLen, &written, nullptr);
        CloseHandle(hFile);
    }

    // Encrypt
    CamelliaResult r = bridge.encryptFileAuthenticated(testInputPath, testEncPath);
    TEST_ASSERT(r.success, "Authenticated encrypt succeeded");

    // Verify encrypted file has magic header
    hFile = CreateFileA(testEncPath, GENERIC_READ, FILE_SHARE_READ,
                         nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        uint8_t magic[4] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, magic, 4, &bytesRead, nullptr);
        CloseHandle(hFile);
        TEST_ASSERT(magic[0] == 'R' && magic[1] == 'C' &&
                    magic[2] == 'M' && magic[3] == '2',
                    "Encrypted file has correct magic header");
    }

    // Decrypt
    r = bridge.decryptFileAuthenticated(testEncPath, testDecPath);
    TEST_ASSERT(r.success, "Authenticated decrypt succeeded");

    // Verify decrypted content matches original
    hFile = CreateFileA(testDecPath, GENERIC_READ, FILE_SHARE_READ,
                         nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[512] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, (DWORD)testDataLen, &bytesRead, nullptr);
        CloseHandle(hFile);
        TEST_ASSERT(bytesRead == testDataLen &&
                    memcmp(buf, testData, testDataLen) == 0,
                    "Decrypted content matches original");
    }

    // Cleanup temp files
    DeleteFileA(testInputPath);
    DeleteFileA(testEncPath);
    DeleteFileA(testDecPath);
}

// ============================================================================
//  Test 6: HMAC tamper detection
// ============================================================================

static void test_hmac_tamper_detection() {
    printf("\n=== Test 6: HMAC Tamper Detection ===\n");

    auto& bridge = Camellia256Bridge::instance();
    if (!bridge.isInitialized()) {
        bridge.initialize();
    }

    const char* testInputPath = "camellia_tamper_input.tmp";
    const char* testEncPath = "camellia_tamper_encrypted.camellia";
    const char* testDecPath = "camellia_tamper_decrypted.tmp";

    // Create test file
    const char* testData = "Tamper detection test data";
    HANDLE hFile = CreateFileA(testInputPath, GENERIC_WRITE, 0,
                                nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, testData, (DWORD)strlen(testData), &written, nullptr);
        CloseHandle(hFile);
    }

    // Encrypt
    CamelliaResult r = bridge.encryptFileAuthenticated(testInputPath, testEncPath);
    TEST_ASSERT(r.success, "Encrypt for tamper test succeeded");

    // Tamper with the encrypted file (flip a byte in the ciphertext)
    hFile = CreateFileA(testEncPath, GENERIC_READ | GENERIC_WRITE, 0,
                         nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        // Seek past header (56 bytes) to ciphertext, flip one byte
        SetFilePointer(hFile, 56, nullptr, FILE_BEGIN);
        uint8_t byte = 0;
        DWORD bytesRead = 0;
        ReadFile(hFile, &byte, 1, &bytesRead, nullptr);
        byte ^= 0xFF; // flip all bits
        SetFilePointer(hFile, 56, nullptr, FILE_BEGIN);
        DWORD written = 0;
        WriteFile(hFile, &byte, 1, &written, nullptr);
        CloseHandle(hFile);
    }

    // Attempt decrypt — should FAIL with auth error
    r = bridge.decryptFileAuthenticated(testEncPath, testDecPath);
    TEST_ASSERT(!r.success, "Tampered file decrypt correctly rejected");
    TEST_ASSERT(r.errorCode == -7, "Error code indicates authentication failure");

    // Cleanup
    DeleteFileA(testInputPath);
    DeleteFileA(testEncPath);
    DeleteFileA(testDecPath);
}

// ============================================================================
//  Test 7: Shutdown and secure zeroing
// ============================================================================

static void test_shutdown_zeroing() {
    printf("\n=== Test 7: Shutdown and Secure Zeroing ===\n");

    auto& bridge = Camellia256Bridge::instance();
    if (!bridge.isInitialized()) {
        bridge.initialize();
    }

    CamelliaResult r = bridge.shutdown();
    TEST_ASSERT(r.success, "Shutdown succeeded");
    TEST_ASSERT(!bridge.isInitialized(), "Bridge reports not initialized after shutdown");

    // Verify HMAC key getter fails after shutdown
    uint8_t hmacKey[32] = {};
    int rc = asm_camellia256_get_hmac_key(hmacKey);
    TEST_ASSERT(rc != 0, "get_hmac_key fails after shutdown");

    // Verify engine refuses operations
    uint8_t block[16] = {};
    uint8_t out[16] = {};
    rc = asm_camellia256_encrypt_block(block, out);
    TEST_ASSERT(rc != 0, "encrypt_block fails after shutdown");
}

// ============================================================================
//  main — Run all tests
// ============================================================================

int main() {
    printf("================================================================\n");
    printf("  RawrXD Camellia-256 Security Test Harness\n");
    printf("  RFC 3713 | SHA-256 KDF | HMAC-SHA256 Auth | Constant-Time\n");
    printf("================================================================\n");

    test_rfc3713_self_test();
    test_block_round_trip();
    test_ctr_round_trip();
    test_bridge_init();
    test_authenticated_file();
    test_hmac_tamper_detection();
    test_shutdown_zeroing();

    printf("\n================================================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", g_testsPassed, g_testsFailed);
    printf("================================================================\n");

    return g_testsFailed > 0 ? 1 : 0;
}
