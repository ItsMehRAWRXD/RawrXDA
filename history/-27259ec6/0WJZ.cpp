// RawrXD Unit Tests - SecurityManager
// Version: 2.0
// Target: 35+ comprehensive unit tests for security components

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include "../src/security_manager.h"

class TestSecurityManager : public QObject
{
    Q_OBJECT

private slots:
    // Setup/Teardown
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Initialization Tests
    void testSingletonInstance();
    void testInitialization();
    void testInitializationWithMasterPassword();
    void testDuplicateInitializationPrevention();
    void testShutdownCleanup();

    // Encryption/Decryption Tests
    void testBasicEncryption();
    void testBasicDecryption();
    void testEncryptionWithEmptyData();
    void testEncryptionWithLargeData();
    void testDecryptionWithWrongKey();
    void testDecryptionWithCorruptedData();
    void testDecryptionWithWrongAAD();

    // HMAC Tests
    void testHMACGeneration();
    void testHMACVerification();
    void testHMACVerificationFailure();
    void testHMACWithEmptyData();

    // Credential Management Tests
    void testStoreCredential();
    void testRetrieveCredential();
    void testUpdateCredential();
    void testDeleteCredential();
    void testListCredentials();
    void testCredentialExpiry();
    void testRefreshableCredential();

    // Access Control Tests
    void testSetAccessControl();
    void testCheckAccess();
    void testAccessDenied();
    void testAccessLevelHierarchy();
    void testGetResourceACL();
    void testRemoveAccessControl();

    // Audit Logging Tests
    void testSecurityEventLogging();
    void testAuditLogRetrieval();
    void testAuditLogFiltering();
    void testSensitiveDataRedaction();

    // Certificate Pinning Tests
    void testAddCertificatePin();
    void testVerifyCertificatePinSuccess();
    void testVerifyCertificatePinFailure();
    void testRemoveCertificatePin();

    // Key Management Tests
    void testKeyRotation();
    void testCurrentKeyID();
    void testKeyDerivation();

    // Configuration Tests
    void testGetConfiguration();
    void testValidateSetup();

private:
    SecurityManager* security;
    QTemporaryDir* tempDir;
    QString testMasterPassword;
};

void TestSecurityManager::initTestCase()
{
    qInfo() << "Starting SecurityManager test suite";
    testMasterPassword = "TestMasterPassword123!";
}

void TestSecurityManager::cleanupTestCase()
{
    qInfo() << "SecurityManager tests completed";
}

void TestSecurityManager::init()
{
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());

    security = SecurityManager::getInstance();
    QVERIFY(security != nullptr);
}

void TestSecurityManager::cleanup()
{
    security->shutdown();
    delete tempDir;
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

void TestSecurityManager::testSingletonInstance()
{
    SecurityManager* instance1 = SecurityManager::getInstance();
    SecurityManager* instance2 = SecurityManager::getInstance();

    QVERIFY(instance1 != nullptr);
    QVERIFY(instance2 != nullptr);
    QCOMPARE(instance1, instance2);
}

void TestSecurityManager::testInitialization()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.encryptionAlgorithm = "AES-256-GCM";
    config.keyDerivationIterations = 100000;

    bool result = security->initialize(config);
    QVERIFY2(result, "SecurityManager initialization should succeed");

    QVERIFY(security->validateSetup());
}

void TestSecurityManager::testInitializationWithMasterPassword()
{
    SecurityManager::Config config;
    config.masterPassword = "StrongPassword456!";
    config.encryptionAlgorithm = "AES-256-GCM";

    bool result = security->initialize(config);
    QVERIFY(result);

    auto configData = security->getConfiguration();
    QCOMPARE(configData["encryption_algorithm"].toString(), QString("AES-256-GCM"));
}

void TestSecurityManager::testDuplicateInitializationPrevention()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;

    bool result1 = security->initialize(config);
    QVERIFY(result1);

    // Second initialization should fail
    bool result2 = security->initialize(config);
    QVERIFY2(!result2, "Duplicate initialization should be prevented");
}

void TestSecurityManager::testShutdownCleanup()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;

    security->initialize(config);
    QVERIFY(security->validateSetup());

    security->shutdown();

    // After shutdown, setup should be invalid
    QVERIFY(!security->validateSetup());
}

// ============================================================================
// ENCRYPTION/DECRYPTION TESTS
// ============================================================================

void TestSecurityManager::testBasicEncryption()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray plaintext = "This is a secret message!";
    QByteArray aad = "context_info";

    QByteArray ciphertext = security->encryptData(plaintext, aad);

    QVERIFY(!ciphertext.isEmpty());
    QVERIFY(ciphertext != plaintext);
}

void TestSecurityManager::testBasicDecryption()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray plaintext = "This is a secret message!";
    QByteArray aad = "context_info";

    QByteArray ciphertext = security->encryptData(plaintext, aad);
    QByteArray decrypted = security->decryptData(ciphertext, aad);

    QCOMPARE(decrypted, plaintext);
}

void TestSecurityManager::testEncryptionWithEmptyData()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray plaintext;
    QByteArray aad = "context";

    QByteArray ciphertext = security->encryptData(plaintext, aad);

    // Empty data should still encrypt (produces IV + tag)
    QVERIFY(!ciphertext.isEmpty());
}

void TestSecurityManager::testEncryptionWithLargeData()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    // 1 MB of data
    QByteArray plaintext(1024 * 1024, 'A');
    QByteArray aad = "large_data";

    QByteArray ciphertext = security->encryptData(plaintext, aad);
    QByteArray decrypted = security->decryptData(ciphertext, aad);

    QCOMPARE(decrypted.size(), plaintext.size());
    QCOMPARE(decrypted, plaintext);
}

void TestSecurityManager::testDecryptionWithWrongKey()
{
    // Initialize with first password
    SecurityManager::Config config1;
    config1.masterPassword = "Password1";
    security->initialize(config1);

    QByteArray plaintext = "Secret data";
    QByteArray ciphertext = security->encryptData(plaintext, "aad");

    // Shutdown and reinitialize with different password
    security->shutdown();

    SecurityManager::Config config2;
    config2.masterPassword = "Password2";
    security->initialize(config2);

    // Decryption should fail
    QByteArray decrypted = security->decryptData(ciphertext, "aad");
    QVERIFY(decrypted.isEmpty());
}

void TestSecurityManager::testDecryptionWithCorruptedData()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray plaintext = "Secret data";
    QByteArray ciphertext = security->encryptData(plaintext, "aad");

    // Corrupt the ciphertext
    ciphertext[10] ^= 0xFF;

    QByteArray decrypted = security->decryptData(ciphertext, "aad");
    QVERIFY(decrypted.isEmpty());
}

void TestSecurityManager::testDecryptionWithWrongAAD()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray plaintext = "Secret data";
    QByteArray ciphertext = security->encryptData(plaintext, "correct_aad");

    // Try to decrypt with wrong AAD
    QByteArray decrypted = security->decryptData(ciphertext, "wrong_aad");
    QVERIFY(decrypted.isEmpty());
}

// ============================================================================
// HMAC TESTS
// ============================================================================

void TestSecurityManager::testHMACGeneration()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray data = "Message to authenticate";
    QByteArray hmac = security->generateHMAC(data);

    QVERIFY(!hmac.isEmpty());
    QCOMPARE(hmac.size(), 32);  // SHA-256 produces 32 bytes
}

void TestSecurityManager::testHMACVerification()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray data = "Message to authenticate";
    QByteArray hmac = security->generateHMAC(data);

    bool valid = security->verifyHMAC(data, hmac);
    QVERIFY(valid);
}

void TestSecurityManager::testHMACVerificationFailure()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray data = "Message to authenticate";
    QByteArray hmac = security->generateHMAC(data);

    // Modify data
    QByteArray tamperedData = "Tampered message";

    bool valid = security->verifyHMAC(tamperedData, hmac);
    QVERIFY(!valid);
}

void TestSecurityManager::testHMACWithEmptyData()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QByteArray data;
    QByteArray hmac = security->generateHMAC(data);

    QVERIFY(!hmac.isEmpty());

    bool valid = security->verifyHMAC(data, hmac);
    QVERIFY(valid);
}

// ============================================================================
// CREDENTIAL MANAGEMENT TESTS
// ============================================================================

void TestSecurityManager::testStoreCredential()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString token = "access_token_12345";
    QString tokenType = "bearer";
    QDateTime expiresAt = QDateTime::currentDateTime().addSecs(3600);

    bool result = security->storeCredential(userId, token, tokenType, expiresAt);
    QVERIFY(result);
}

void TestSecurityManager::testRetrieveCredential()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString token = "access_token_12345";
    QString tokenType = "bearer";
    QDateTime expiresAt = QDateTime::currentDateTime().addSecs(3600);

    security->storeCredential(userId, token, tokenType, expiresAt);

    SecurityManager::CredentialInfo cred = security->getCredential(userId);

    QCOMPARE(cred.userId, userId);
    QCOMPARE(cred.token, token);
    QCOMPARE(cred.tokenType, tokenType);
}

void TestSecurityManager::testUpdateCredential()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    
    // Store initial credential
    security->storeCredential(userId, "token1", "bearer", QDateTime::currentDateTime().addSecs(3600));

    // Update credential
    QString newToken = "token2";
    security->storeCredential(userId, newToken, "bearer", QDateTime::currentDateTime().addSecs(7200));

    SecurityManager::CredentialInfo cred = security->getCredential(userId);
    QCOMPARE(cred.token, newToken);
}

void TestSecurityManager::testDeleteCredential()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    security->storeCredential(userId, "token", "bearer", QDateTime::currentDateTime().addSecs(3600));

    bool deleteResult = security->deleteCredential(userId);
    QVERIFY(deleteResult);

    SecurityManager::CredentialInfo cred = security->getCredential(userId);
    QVERIFY(cred.userId.isEmpty());
}

void TestSecurityManager::testListCredentials()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    security->storeCredential("user1@example.com", "token1", "bearer", QDateTime::currentDateTime().addSecs(3600));
    security->storeCredential("user2@example.com", "token2", "bearer", QDateTime::currentDateTime().addSecs(3600));
    security->storeCredential("user3@example.com", "token3", "bearer", QDateTime::currentDateTime().addSecs(3600));

    QStringList userIds = security->listStoredCredentials();

    QCOMPARE(userIds.size(), 3);
    QVERIFY(userIds.contains("user1@example.com"));
    QVERIFY(userIds.contains("user2@example.com"));
    QVERIFY(userIds.contains("user3@example.com"));
}

void TestSecurityManager::testCredentialExpiry()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QDateTime expiresAt = QDateTime::currentDateTime().addSecs(-10);  // Already expired

    security->storeCredential(userId, "token", "bearer", expiresAt);

    bool isExpired = security->isTokenExpired(userId);
    QVERIFY(isExpired);
}

void TestSecurityManager::testRefreshableCredential()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString accessToken = "access_token";
    QString refreshToken = "refresh_token";

    security->storeCredential(userId, accessToken, "bearer", 
                             QDateTime::currentDateTime().addSecs(3600),
                             refreshToken);

    SecurityManager::CredentialInfo cred = security->getCredential(userId);
    QCOMPARE(cred.refreshToken, refreshToken);
    QVERIFY(cred.isRefreshable);
}

// ============================================================================
// ACCESS CONTROL TESTS
// ============================================================================

void TestSecurityManager::testSetAccessControl()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString resource = "models/training";
    SecurityManager::AccessLevel level = SecurityManager::AccessLevel::Write;

    bool result = security->setAccessControl(userId, resource, level);
    QVERIFY(result);
}

void TestSecurityManager::testCheckAccess()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString resource = "models/training";
    SecurityManager::AccessLevel level = SecurityManager::AccessLevel::Write;

    security->setAccessControl(userId, resource, level);

    bool hasAccess = security->checkAccess(userId, resource, level);
    QVERIFY(hasAccess);
}

void TestSecurityManager::testAccessDenied()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString resource = "models/training";

    // User has no access set
    bool hasAccess = security->checkAccess(userId, resource, SecurityManager::AccessLevel::Read);
    QVERIFY(!hasAccess);
}

void TestSecurityManager::testAccessLevelHierarchy()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString resource = "models/training";

    // Grant Write access
    security->setAccessControl(userId, resource, SecurityManager::AccessLevel::Write);

    // Write access should also grant Read access
    bool canRead = security->checkAccess(userId, resource, SecurityManager::AccessLevel::Read);
    QVERIFY(canRead);

    // But not Admin access
    bool canAdmin = security->checkAccess(userId, resource, SecurityManager::AccessLevel::Admin);
    QVERIFY(!canAdmin);
}

void TestSecurityManager::testGetResourceACL()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString resource = "models/training";

    security->setAccessControl("user1@example.com", resource, SecurityManager::AccessLevel::Read);
    security->setAccessControl("user2@example.com", resource, SecurityManager::AccessLevel::Write);
    security->setAccessControl("user3@example.com", resource, SecurityManager::AccessLevel::Admin);

    QMap<QString, SecurityManager::AccessLevel> acl = security->getResourceACL(resource);

    QCOMPARE(acl.size(), 3);
    QCOMPARE(acl["user1@example.com"], SecurityManager::AccessLevel::Read);
    QCOMPARE(acl["user2@example.com"], SecurityManager::AccessLevel::Write);
    QCOMPARE(acl["user3@example.com"], SecurityManager::AccessLevel::Admin);
}

void TestSecurityManager::testRemoveAccessControl()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString userId = "user@example.com";
    QString resource = "models/training";

    security->setAccessControl(userId, resource, SecurityManager::AccessLevel::Write);
    QVERIFY(security->checkAccess(userId, resource, SecurityManager::AccessLevel::Write));

    security->removeAccessControl(userId, resource);
    QVERIFY(!security->checkAccess(userId, resource, SecurityManager::AccessLevel::Write));
}

// ============================================================================
// AUDIT LOGGING TESTS
// ============================================================================

void TestSecurityManager::testSecurityEventLogging()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.auditLogging.enabled = true;
    security->initialize(config);

    security->logSecurityEvent(
        "login_success",
        "user@example.com",
        "User logged in successfully",
        SecurityManager::SecurityEventSeverity::Info
    );

    auto events = security->getAuditLog(QDateTime::currentDateTime().addSecs(-60), 
                                        QDateTime::currentDateTime().addSecs(60));
    QVERIFY(events.size() > 0);
}

void TestSecurityManager::testAuditLogRetrieval()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.auditLogging.enabled = true;
    security->initialize(config);

    // Log multiple events
    security->logSecurityEvent("event1", "user1", "Event 1", SecurityManager::SecurityEventSeverity::Info);
    security->logSecurityEvent("event2", "user2", "Event 2", SecurityManager::SecurityEventSeverity::Warning);
    security->logSecurityEvent("event3", "user3", "Event 3", SecurityManager::SecurityEventSeverity::Critical);

    QDateTime start = QDateTime::currentDateTime().addSecs(-60);
    QDateTime end = QDateTime::currentDateTime().addSecs(60);

    auto events = security->getAuditLog(start, end);
    QVERIFY(events.size() >= 3);
}

void TestSecurityManager::testAuditLogFiltering()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.auditLogging.enabled = true;
    security->initialize(config);

    security->logSecurityEvent("login", "user1", "Login", SecurityManager::SecurityEventSeverity::Info);
    security->logSecurityEvent("access_denied", "user2", "Access denied", SecurityManager::SecurityEventSeverity::Warning);

    // Filter by event type
    auto events = security->getAuditLog(QDateTime::currentDateTime().addSecs(-60),
                                        QDateTime::currentDateTime().addSecs(60),
                                        "login");

    bool foundLogin = false;
    bool foundAccessDenied = false;

    for (const auto& event : events) {
        if (event.eventType == "login") foundLogin = true;
        if (event.eventType == "access_denied") foundAccessDenied = true;
    }

    QVERIFY(foundLogin);
    QVERIFY(!foundAccessDenied);  // Should be filtered out
}

void TestSecurityManager::testSensitiveDataRedaction()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.auditLogging.enabled = true;
    config.auditLogging.redactSensitiveData = true;
    security->initialize(config);

    QString sensitiveDetails = "Password: MySecret123!";
    security->logSecurityEvent("login_attempt", "user@example.com", sensitiveDetails, 
                               SecurityManager::SecurityEventSeverity::Info);

    auto events = security->getAuditLog(QDateTime::currentDateTime().addSecs(-60),
                                        QDateTime::currentDateTime().addSecs(60));

    // Verify sensitive data is redacted
    for (const auto& event : events) {
        if (event.eventType == "login_attempt") {
            QVERIFY(!event.details.contains("MySecret123!"));
        }
    }
}

// ============================================================================
// CERTIFICATE PINNING TESTS
// ============================================================================

void TestSecurityManager::testAddCertificatePin()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.certificatePinning.enabled = true;
    security->initialize(config);

    QString hostname = "api.example.com";
    QString certHash = "sha256:ABCDEF1234567890";

    bool result = security->addCertificatePin(hostname, certHash);
    QVERIFY(result);
}

void TestSecurityManager::testVerifyCertificatePinSuccess()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.certificatePinning.enabled = true;
    security->initialize(config);

    QString hostname = "api.example.com";
    QString certHash = "sha256:ABCDEF1234567890";

    security->addCertificatePin(hostname, certHash);

    bool valid = security->verifyCertificatePin(hostname, certHash);
    QVERIFY(valid);
}

void TestSecurityManager::testVerifyCertificatePinFailure()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.certificatePinning.enabled = true;
    security->initialize(config);

    QString hostname = "api.example.com";
    QString correctHash = "sha256:ABCDEF1234567890";
    QString wrongHash = "sha256:0987654321FEDCBA";

    security->addCertificatePin(hostname, correctHash);

    bool valid = security->verifyCertificatePin(hostname, wrongHash);
    QVERIFY(!valid);
}

void TestSecurityManager::testRemoveCertificatePin()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.certificatePinning.enabled = true;
    security->initialize(config);

    QString hostname = "api.example.com";
    QString certHash = "sha256:ABCDEF1234567890";

    security->addCertificatePin(hostname, certHash);
    QVERIFY(security->verifyCertificatePin(hostname, certHash));

    security->removeCertificatePin(hostname);
    QVERIFY(!security->verifyCertificatePin(hostname, certHash));
}

// ============================================================================
// KEY MANAGEMENT TESTS
// ============================================================================

void TestSecurityManager::testKeyRotation()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString oldKeyId = security->getCurrentKeyID();

    bool result = security->rotateEncryptionKey();
    QVERIFY(result);

    QString newKeyId = security->getCurrentKeyID();
    QVERIFY(oldKeyId != newKeyId);
}

void TestSecurityManager::testCurrentKeyID()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QString keyId = security->getCurrentKeyID();
    QVERIFY(!keyId.isEmpty());
}

void TestSecurityManager::testKeyDerivation()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.keyDerivationIterations = 100000;
    security->initialize(config);

    auto configData = security->getConfiguration();
    QCOMPARE(configData["pbkdf2_iterations"].toInt(), 100000);
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

void TestSecurityManager::testGetConfiguration()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    config.encryptionAlgorithm = "AES-256-GCM";
    config.keyDerivationIterations = 100000;
    security->initialize(config);

    QJsonObject configData = security->getConfiguration();

    QCOMPARE(configData["encryption_algorithm"].toString(), QString("AES-256-GCM"));
    QCOMPARE(configData["pbkdf2_iterations"].toInt(), 100000);
    QVERIFY(configData.contains("current_key_id"));
}

void TestSecurityManager::testValidateSetup()
{
    SecurityManager::Config config;
    config.masterPassword = testMasterPassword;
    security->initialize(config);

    QVERIFY(security->validateSetup());

    security->shutdown();

    QVERIFY(!security->validateSetup());
}

QTEST_MAIN(TestSecurityManager)
#include "test_security_manager.moc"
