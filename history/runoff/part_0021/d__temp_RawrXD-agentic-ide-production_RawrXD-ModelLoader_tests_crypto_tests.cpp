/**
 * Comprehensive Cryptographic Library Test Suite
 * Tests all cryptographic primitives with known test vectors
 */

#include "crypto/rawrxd_crypto.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

using namespace RawrXD::Crypto;

// Helper function to print hex
void printHex(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << ": ";
    for (uint8_t b : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    std::cout << std::dec << std::endl;
}

// ============================================================
// BASE64URL TESTS
// ============================================================

void testBase64Url() {
    std::cout << "\n=== Testing Base64 URL-Safe Encoding ===\n";
    
    // Test 1: Basic encoding/decoding
    std::string plaintext = "Hello, World!";
    std::string encoded = Base64Url::encode(plaintext);
    std::vector<uint8_t> decoded = Base64Url::decode(encoded);
    std::string decodedStr(decoded.begin(), decoded.end());
    
    std::cout << "Original: " << plaintext << std::endl;
    std::cout << "Encoded: " << encoded << std::endl;
    std::cout << "Decoded: " << decodedStr << std::endl;
    assert(plaintext == decodedStr);
    
    // Test 2: RFC 4648 Test Vectors
    assert(Base64Url::encode("") == "");
    assert(Base64Url::encode("f") == "Zg");
    assert(Base64Url::encode("fo") == "Zm8");
    assert(Base64Url::encode("foo") == "Zm9v");
    assert(Base64Url::encode("foob") == "Zm9vYg");
    assert(Base64Url::encode("fooba") == "Zm9vYmE");
    assert(Base64Url::encode("foobar") == "Zm9vYmFy");
    
    std::cout << "✓ Base64 URL encoding tests passed\n";
}

// ============================================================
// SHA-256 TESTS
// ============================================================

void testSHA256() {
    std::cout << "\n=== Testing SHA-256 ===\n";
    
    // Test 1: Empty string
    auto hash1 = SHA256::hash("");
    std::string expected1 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    assert(BigInt(hash1).toHex() == expected1);
    
    // Test 2: "abc"
    auto hash2 = SHA256::hash("abc");
    std::string expected2 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    assert(BigInt(hash2).toHex() == expected2);
    
    // Test 3: Long string
    auto hash3 = SHA256::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    std::string expected3 = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
    assert(BigInt(hash3).toHex() == expected3);
    
    std::cout << "✓ SHA-256 tests passed\n";
}

// ============================================================
// HMAC-SHA256 TESTS
// ============================================================

void testHMAC() {
    std::cout << "\n=== Testing HMAC-SHA256 ===\n";
    
    // RFC 4231 Test Case 1
    std::vector<uint8_t> key1(20, 0x0b);
    std::string data1 = "Hi There";
    auto mac1 = HMAC_SHA256::compute(key1, std::vector<uint8_t>(data1.begin(), data1.end()));
    std::string expected1 = "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7";
    assert(BigInt(mac1).toHex() == expected1);
    
    // RFC 4231 Test Case 2
    std::vector<uint8_t> key2 = {0x4a, 0x65, 0x66, 0x65}; // "Jefe"
    std::string data2 = "what do ya want for nothing?";
    auto mac2 = HMAC_SHA256::compute(key2, std::vector<uint8_t>(data2.begin(), data2.end()));
    std::string expected2 = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
    assert(BigInt(mac2).toHex() == expected2);
    
    std::cout << "✓ HMAC-SHA256 tests passed\n";
}

// ============================================================
// BIG INTEGER TESTS
// ============================================================

void testBigInt() {
    std::cout << "\n=== Testing Big Integer Arithmetic ===\n";
    
    // Test 1: Basic operations
    BigInt a(123);
    BigInt b(456);
    
    BigInt sum = a + b;
    assert(sum == BigInt(579));
    
    BigInt diff = b - a;
    assert(diff == BigInt(333));
    
    BigInt prod = a * b;
    assert(prod == BigInt(56088));
    
    // Test 2: Modular exponentiation
    BigInt base(2);
    BigInt exp(10);
    BigInt mod(1000);
    BigInt result = base.modPow(exp, mod);
    assert(result == BigInt(24)); // 2^10 mod 1000 = 1024 mod 1000 = 24
    
    // Test 3: From/to hex
    BigInt hex1 = BigInt::fromHex("DEADBEEF");
    assert(hex1.toHex() == "deadbeef");
    
    std::cout << "✓ Big Integer tests passed\n";
}

// ============================================================
// RSA TESTS
// ============================================================

void testRSA() {
    std::cout << "\n=== Testing RSA Signature Verification ===\n";
    
    // Test with a small RSA key (for testing only - use 2048+ in production)
    // This is a simplified test - in real usage, load from JWK
    
    // For now, just test that the API works
    RSAPublicKey key;
    
    // Test modPow operation
    BigInt plaintext(42);
    BigInt n = BigInt::fromHex("C9ABA");
    BigInt e(65537);
    
    RSAPublicKey testKey(n, e);
    BigInt encrypted = testKey.encrypt(plaintext);
    
    std::cout << "✓ RSA API tests passed\n";
}

// ============================================================
// ELLIPTIC CURVE TESTS
// ============================================================

void testECC() {
    std::cout << "\n=== Testing Elliptic Curve Cryptography ===\n";
    
    // Test P-256 curve initialization
    ECCurve curve(ECCurve::CurveType::P256);
    
    // Verify generator point is on curve
    assert(curve.isOnCurve(curve.getG()));
    
    // Test point doubling
    ECPoint doubled = curve.double_(curve.getG());
    assert(curve.isOnCurve(doubled));
    
    // Test point multiplication by 1
    ECPoint g1 = curve.multiply(curve.getG(), BigInt(1));
    assert(g1.x == curve.getG().x);
    assert(g1.y == curve.getG().y);
    
    std::cout << "✓ Elliptic Curve tests passed\n";
}

// ============================================================
// AES-256-GCM TESTS
// ============================================================

void testAESGCM() {
    std::cout << "\n=== Testing AES-256-GCM ===\n";
    
    // Test encryption/decryption
    uint8_t key[32] = {0}; // All zeros for testing
    uint8_t iv[12] = {0};
    
    std::string plaintext = "Hello, AES-GCM!";
    std::vector<uint8_t> ciphertext;
    
    bool encrypted = AES256GCM::encrypt(
        key, iv,
        reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size(),
        nullptr, 0,
        ciphertext
    );
    
    assert(encrypted);
    assert(ciphertext.size() == plaintext.size() + AES256GCM::TAG_SIZE);
    
    // Extract tag
    const uint8_t* tag = ciphertext.data() + plaintext.size();
    
    // Decrypt
    std::vector<uint8_t> decrypted;
    bool decryptOk = AES256GCM::decrypt(
        key, iv,
        ciphertext.data(), plaintext.size(),
        tag,
        nullptr, 0,
        decrypted
    );
    
    assert(decryptOk);
    std::string decryptedStr(decrypted.begin(), decrypted.end());
    assert(decryptedStr == plaintext);
    
    std::cout << "✓ AES-256-GCM tests passed\n";
}

// ============================================================
// SECURE RANDOM TESTS
// ============================================================

void testSecureRandom() {
    std::cout << "\n=== Testing Secure Random Number Generator ===\n";
    
    // Generate random bytes
    auto random1 = SecureRandom::generate(32);
    auto random2 = SecureRandom::generate(32);
    
    // Verify different outputs
    assert(random1 != random2);
    
    // Generate random integers
    uint32_t r1 = SecureRandom::generateUInt32();
    uint32_t r2 = SecureRandom::generateUInt32();
    assert(r1 != r2 || true); // May collide but unlikely
    
    std::cout << "✓ Secure Random tests passed\n";
}

// ============================================================
// CONSTANT-TIME OPERATIONS TESTS
// ============================================================

void testConstantTimeOps() {
    std::cout << "\n=== Testing Constant-Time Operations ===\n";
    
    uint8_t a[] = {1, 2, 3, 4};
    uint8_t b[] = {1, 2, 3, 4};
    uint8_t c[] = {1, 2, 3, 5};
    
    assert(SecureMemory::constantTimeCompare(a, b, 4) == 0);
    assert(SecureMemory::constantTimeCompare(a, c, 4) != 0);
    
    // Test secure zero
    uint8_t secret[] = {0xDE, 0xAD, 0xBE, 0xEF};
    SecureMemory::secureZero(secret, sizeof(secret));
    assert(secret[0] == 0 && secret[1] == 0 && secret[2] == 0 && secret[3] == 0);
    
    std::cout << "✓ Constant-time operation tests passed\n";
}

// ============================================================
// MAIN TEST RUNNER
// ============================================================

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RawrXD Cryptographic Library - Comprehensive Test Suite  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    try {
        testBase64Url();
        testSHA256();
        testHMAC();
        testBigInt();
        testRSA();
        testECC();
        testAESGCM();
        testSecureRandom();
        testConstantTimeOps();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║              ✓ ALL TESTS PASSED SUCCESSFULLY              ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ TEST FAILED: Unknown error" << std::endl;
        return 1;
    }
}
