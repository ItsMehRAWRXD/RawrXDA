/**
 * SecurityManager unit tests — C++20 only (no Qt).
 * Uses SecurityManager from include/security_manager.h (std::string, std::vector<uint8_t>).
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

// Prefer project include path; build may add include/
#if __has_include("security_manager.h")
#include "security_manager.h"
#else
#include "../../include/security_manager.h"
#endif

#define TEST_VERIFY(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", #cond); ++g_fail; } } while(0)
#define TEST_VERIFY2(cond, msg) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", (msg)); ++g_fail; } } while(0)
#define TEST_COMPARE(a, b) do { if ((a) != (b)) { std::fprintf(stderr, "FAIL: %s != %s\n", #a, #b); ++g_fail; } } while(0)

static int g_fail = 0;

static void testSingletonInstance() {
    SecurityManager* instance1 = SecurityManager::getInstance();
    SecurityManager* instance2 = SecurityManager::getInstance();
    TEST_VERIFY(instance1 != nullptr);
    TEST_VERIFY(instance2 != nullptr);
    TEST_COMPARE(instance1, instance2);
}

static void testInitialization() {
    SecurityManager* security = SecurityManager::getInstance();
    bool result = security->initialize("TestMasterPassword123!");
    TEST_VERIFY2(result, "SecurityManager initialization should succeed");
    TEST_VERIFY(security->validateSetup());
}

static void testShutdownCleanup() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("ShutdownTestPassword!");
    TEST_VERIFY(security->validateSetup());
    security->shutdown();
    TEST_VERIFY(!security->validateSetup());
}

static void testBasicEncryptionDecryption() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("EncTestPassword!");
    std::string plainStr = "This is a secret message!";
    std::vector<uint8_t> plaintext(plainStr.begin(), plainStr.end());
    std::string ciphertext = security->encryptData(plaintext);
    TEST_VERIFY(!ciphertext.empty());
    std::vector<uint8_t> decrypted = security->decryptData(ciphertext);
    TEST_VERIFY(decrypted.size() == plaintext.size());
    TEST_VERIFY(std::memcmp(decrypted.data(), plaintext.data(), plaintext.size()) == 0);
}

static void testHMACGeneration() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("HMACTestPassword!");
    std::vector<uint8_t> data = { 'M', 'e', 's', 's', 'a', 'g', 'e' };
    std::string hmac = security->generateHMAC(data);
    TEST_VERIFY(!hmac.empty());
    bool valid = security->verifyHMAC(data, hmac);
    TEST_VERIFY(valid);
}

static void testStoreAndRetrieveCredential() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("CredTestPassword!");
    bool stored = security->storeCredential("user@example.com", "access_token_12345", "bearer", 0, "");
    TEST_VERIFY(stored);
    auto cred = security->getCredential("user@example.com");
    TEST_VERIFY(cred.username == "user@example.com");
    TEST_VERIFY(cred.token == "access_token_12345");
    bool removed = security->removeCredential("user@example.com");
    TEST_VERIFY(removed);
}

static void testAccessControl() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("ACLTestPassword!");
    bool set = security->setAccessControl("alice", "/resource1", SecurityManager::AccessLevel::Read);
    TEST_VERIFY(set);
    bool hasAccess = security->checkAccess("alice", "/resource1", SecurityManager::AccessLevel::Read);
    TEST_VERIFY(hasAccess);
    auto acl = security->getResourceACL("/resource1");
    TEST_VERIFY(acl.size() >= 1);
}

static void testAuditLogging() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("AuditTestPassword!");
    security->logSecurityEvent("login", "user1", "/api", true, "OK");
    auto events = security->getAuditLog(10);
    TEST_VERIFY(events.size() >= 1);
}

static void testGetConfiguration() {
    SecurityManager* security = SecurityManager::getInstance();
    security->initialize("ConfigTestPassword!");
    std::string config = security->getConfiguration();
    TEST_VERIFY(!config.empty());
}

#define RUN(test) do { test(); } while(0)

int main() {
    std::fprintf(stdout, "SecurityManager unit tests (C++20, no Qt)\n");
    RUN(testSingletonInstance);
    RUN(testInitialization);
    RUN(testShutdownCleanup);
    RUN(testBasicEncryptionDecryption);
    RUN(testHMACGeneration);
    RUN(testStoreAndRetrieveCredential);
    RUN(testAccessControl);
    RUN(testAuditLogging);
    RUN(testGetConfiguration);
    SecurityManager::getInstance()->shutdown();
    std::fprintf(stdout, "Done: %d failures\n", g_fail);
    return g_fail ? 1 : 0;
}
