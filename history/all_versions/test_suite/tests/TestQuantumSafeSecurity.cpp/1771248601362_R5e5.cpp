#include <cassert>
#include <iostream>
#include <random>
#include <nlohmann/json.hpp>
#include <map>
#include <cmath>
#include <string>
#include "enterprise/QuantumSafeSecurity.hpp"

class TestQuantumSafeSecurity {
    
private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Quantum cryptography tests
    void testKyberKeyGeneration();
    void testKyberEncapsulationDecapsulation();
    void testDilithiumSignatureGeneration();
    void testDilithiumSignatureVerification();
    void testQuantumThreatDetection();
    void testPostQuantumIntegrity();
    void testQuantumSafeSessionManagement();
    void testQuantumRandomnessQuality();
    void testQuantumKeyRotation();
    
    // Security benchmarks
    void benchmarkQuantumKeyGeneration();
    void benchmarkQuantumEncapsulation();
    void benchmarkQuantumSignatureOperations();
    
private:
    QuantumSafeSecurity* m_quantumSecurity;
};

void TestQuantumSafeSecurity::initTestCase() {
    m_quantumSecurity = QuantumSafeSecurity::instance();
    QVERIFY(m_quantumSecurity != nullptr);
}

void TestQuantumSafeSecurity::cleanupTestCase() {
    // No global teardown required for now
}

void TestQuantumSafeSecurity::testKyberKeyGeneration() {
    // Test post-quantum Kyber key generation
    QJsonObject keyPair = m_quantumSecurity->generateKyberKeyPair();
    
    QVERIFY(!keyPair.isEmpty());
    QVERIFY(keyPair.contains("privateMatrix"));
    QVERIFY(keyPair.contains("publicMatrix"));
    QCOMPARE(keyPair["algorithm"].toString(), QString("KYBER_768_ENTERPRISE"));
    QCOMPARE(keyPair["securityLevel"].toInt(), 3);
    
    QJsonArray privateMatrix = keyPair["privateMatrix"].toArray();
    QJsonArray publicMatrix = keyPair["publicMatrix"].toArray();
    
    QVERIFY(!privateMatrix.isEmpty());
    QVERIFY(!publicMatrix.isEmpty());
    QVERIFY(privateMatrix.size() > 0);
    QVERIFY(publicMatrix.size() > 0);
    
    // Verify key sizes are reasonable for enterprise
    int privateKeySize = keyPair["privateKeySize"].toInt();
    int publicKeySize = keyPair["publicKeySize"].toInt();
    
    QVERIFY(privateKeySize > 100); // Enterprise keys are large
    QVERIFY(publicKeySize > 50);
    
    qDebug() << "Kyber key pair generated - Private:" << privateKeySize << "bytes, Public:" << publicKeySize << "bytes";
}

void TestQuantumSafeSecurity::testKyberEncapsulationDecapsulation() {
    // Test complete Kyber encapsulation/decapsulation cycle
    QJsonObject keyPair = m_quantumSecurity->generateKyberKeyPair();
    QJsonObject publicKey = keyPair; // In our implementation, public key is part of keyPair
    
    QByteArray sharedSecretOriginal = "ENTERPRISE_QUANTUM_SHARED_SECRET_256_BITS_LONG";
    
    // Encapsulate
    QByteArray ciphertext = m_quantumSecurity->kyberEncapsulate(publicKey, sharedSecretOriginal);
    
    QVERIFY(!ciphertext.isEmpty());
    QVERIFY(ciphertext.size() > 0);
    
    // Decapsulate
    QJsonObject privateKey = keyPair; // Private key is part of keyPair
    QByteArray sharedSecretRecovered = m_quantumSecurity->kyberDecapsulate(privateKey, ciphertext);
    
    QVERIFY(!sharedSecretRecovered.isEmpty());
    QCOMPARE(sharedSecretOriginal.size(), sharedSecretRecovered.size());
    
    // Verify the shared secret matches
    QCOMPARE(sharedSecretOriginal, sharedSecretRecovered);
    
    qDebug() << "Kyber encapsulation/decapsulation successful - Secret size:" << sharedSecretOriginal.size() << "bytes";
}

void TestQuantumSafeSecurity::testDilithiumSignatureGeneration() {
    // Test post-quantum Dilithium signature generation
    QJsonObject keyPair = m_quantumSecurity->generateDilithiumKeyPair();
    
    QVERIFY(!keyPair.isEmpty());
    QVERIFY(keyPair.contains("signingKey"));
    QVERIFY(keyPair.contains("verificationKey"));
    QCOMPARE(keyPair["algorithm"].toString(), QString("DILITHIUM_3_ENTERPRISE"));
    QCOMPARE(keyPair["securityLevel"].toInt(), 3);
    
    QByteArray message = "Enterprise quantum-safe message for signature testing";
    
    // Generate signature
    QJsonObject signingKey = keyPair; // Signing key is part of keyPair
    QByteArray signature = m_quantumSecurity->dilithiumSign(signingKey, message);
    
    QVERIFY(!signature.isEmpty());
    
    // Parse signature to verify structure
    QJsonDocument sigDoc = QJsonDocument::fromJson(signature);
    QVERIFY(sigDoc.isObject());
    
    QJsonObject signatureObj = sigDoc.object();
    QVERIFY(signatureObj.contains("signature"));
    QVERIFY(signatureObj.contains("messageHash"));
    QVERIFY(signatureObj.contains("timestamp"));
    QVERIFY(signatureObj.contains("algorithm"));
    
    QCOMPARE(signatureObj["algorithm"].toString(), QString("DILITHIUM_3_ENTERPRISE"));
    
    qDebug() << "Dilithium signature generated - Size:" << signature.size() << "bytes";
}

void TestQuantumSafeSecurity::testDilithiumSignatureVerification() {
    // Test Dilithium signature verification
    QJsonObject keyPair = m_quantumSecurity->generateDilithiumKeyPair();
    QByteArray message = "Test message for signature verification";
    
    // Generate signature
    QJsonObject signingKey = keyPair;
    QByteArray signature = m_quantumSecurity->dilithiumSign(signingKey, message);
    
    // Verify signature
    QJsonObject verificationKey = keyPair; // Verification key is part of keyPair
    bool verified = m_quantumSecurity->dilithiumVerify(verificationKey, message, signature);
    
    QVERIFY(verified);
    
    // Test with tampered message (should fail verification)
    QByteArray tamperedMessage = "TAMPERED Test message for signature verification";
    bool tamperedVerified = m_quantumSecurity->dilithiumVerify(verificationKey, tamperedMessage, signature);
    
    QCOMPARE(tamperedVerified, false);
    
    qDebug() << "Dilithium signature verification successful - Tamper detection working";
}

void TestQuantumSafeSecurity::testPostQuantumIntegrity() {
    // Test post-quantum cryptographic integrity
    
    // Generate multiple key pairs and ensure they're different
    auto keyPair1 = m_quantumSecurity->generateKyberKeyPair();
    auto keyPair2 = m_quantumSecurity->generateKyberKeyPair();
    
    // Keys should be different (high entropy)
    std::string keyData1 = keyPair1.dump();
    std::string keyData2 = keyPair2.dump();
    
    assert(keyData1 != keyData2);
    
    // Test cryptographic operations integrity
    std::string testMessage = "POST_QUANTUM_INTEGRITY_TEST_MESSAGE";
    
    // Kyber integrity test
    auto kyberKeys = m_quantumSecurity->generateKyberKeyPair();
    auto ciphertext = m_quantumSecurity->kyberEncapsulate(kyberKeys, testMessage);
    auto decrypted = m_quantumSecurity->kyberDecapsulate(kyberKeys, ciphertext);
    
    assert(decrypted == testMessage);
    
    // Dilithium integrity test
    auto dilithiumKeys = m_quantumSecurity->generateDilithiumKeyPair();
    auto signature = m_quantumSecurity->dilithiumSign(dilithiumKeys, testMessage);
    bool verified = m_quantumSecurity->dilithiumVerify(dilithiumKeys, testMessage, signature);
    
    assert(verified);
    
    std::cout << "Post-quantum cryptographic integrity verified" << std::endl;
}

void TestQuantumSafeSecurity::testQuantumRandomnessQuality() {
    // Test quality of quantum-safe random number generation
    
    uint64_t r1 = m_rng();
    uint64_t r2 = m_rng();
    std::string randomData1(reinterpret_cast<const char*>(&r1), sizeof(r1));
    std::string randomData2(reinterpret_cast<const char*>(&r2), sizeof(r2));
    
    // Random data should be different (high entropy)
    assert(randomData1 != randomData2);
    
    // Test randomness distribution (simplified test)
    std::map<int, int> byteFrequency;
    std::string largeRandomSample;
    
    for (int i = 0; i < 1000; ++i) {
        uint64_t rv = m_rng();
        std::string randomBytes(reinterpret_cast<const char*>(&rv), sizeof(rv));
        largeRandomSample.append(randomBytes);

        for (unsigned char byte : randomBytes) {
            byteFrequency[byte]++;
        }
    }
    
    // Check distribution - expect roughly uniform (within 50% tolerance due to random variation)
    double expectedFrequency = largeRandomSample.size() / 256.0;
    double tolerance = expectedFrequency * 0.5; // 50% tolerance for randomness
    
    int validFrequencies = 0;
    for (auto& pair : byteFrequency) {
        if (std::abs(pair.second - expectedFrequency) < tolerance) {
            validFrequencies++;
        }
    }
    
    // At least 60% of byte values should fall within tolerance
    assert(validFrequencies >= (256 * 0.6));
    
    std::cout << "Quantum randomness quality verified - Sample size:" << largeRandomSample.size() << "bytes" << std::endl;
}

void TestQuantumSafeSecurity::benchmarkQuantumKeyGeneration() {
    // Simple benchmark for key generation
    auto keyPair = m_quantumSecurity->generateKyberKeyPair();
    assert(!keyPair.empty());
}

void TestQuantumSafeSecurity::benchmarkQuantumEncapsulation() {
    auto keyPair = m_quantumSecurity->generateKyberKeyPair();
    std::string sharedSecret = "QUANTUM_BENCHMARK_SHARED_SECRET";
    auto ciphertext = m_quantumSecurity->kyberEncapsulate(keyPair, sharedSecret);
    assert(!ciphertext.empty());
}

void TestQuantumSafeSecurity::benchmarkQuantumSignatureOperations() {
    QJsonObject keyPair = m_quantumSecurity->generateDilithiumKeyPair();
    QByteArray message = "QUANTUM_SIGNATURE_BENCHMARK_MESSAGE";
    
    QBENCHMARK {
        QByteArray signature = m_quantumSecurity->dilithiumSign(keyPair, message);
        QVERIFY(!signature.isEmpty());
        
        bool verified = m_quantumSecurity->dilithiumVerify(keyPair, message, signature);
        QVERIFY(verified);
    }
}

QTEST_MAIN(TestQuantumSafeSecurity)
#include "TestQuantumSafeSecurity.moc"

// Stub implementations for not-yet-implemented tests to satisfy linker
void TestQuantumSafeSecurity::testQuantumThreatDetection() { QSKIP("Not implemented yet"); }
void TestQuantumSafeSecurity::testQuantumSafeSessionManagement() { QSKIP("Not implemented yet"); }
void TestQuantumSafeSecurity::testQuantumKeyRotation() { QSKIP("Not implemented yet"); }
