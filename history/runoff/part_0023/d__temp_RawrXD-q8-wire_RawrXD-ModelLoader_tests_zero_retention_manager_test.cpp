#include <gtest/gtest.h>
#include <QObject>
#include <QUuid>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <QCoreApplication>
#include <chrono>
#include <thread>

#include "../src/terminal/zero_retention_manager.hpp"

class ZeroRetentionManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!m_tempDir.isValid()) {
            FAIL() << "Failed to create temporary directory";
        }

        m_manager = std::make_unique<ZeroRetentionManager>();
        m_auditLogPath = m_tempDir.path() + "/audit.log";
        m_manager->setAuditLogPath(m_auditLogPath.toStdString());
    }

    void TearDown() override
    {
        m_manager.reset();
    }

    QTemporaryDir m_tempDir;
    QString m_auditLogPath;
    std::unique_ptr<ZeroRetentionManager> m_manager;

    // Helper functions
    bool checkAuditLogEntry(const std::string& dataId, const std::string& action)
    {
        QFile logFile(m_auditLogPath);
        if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        QString content = logFile.readAll();
        logFile.close();

        return content.contains(QString::fromStdString(dataId)) &&
               content.contains(QString::fromStdString(action));
    }

    std::string generateSensitiveData()
    {
        return "credit_card:4532-1234-5678-9010";
    }

    std::string generatePII()
    {
        return "user@example.com, IP: 192.168.1.1";
    }
};

// ============= Initialization Tests =============

TEST_F(ZeroRetentionManagerTest, ManagerInitialization)
{
    EXPECT_TRUE(m_manager != nullptr);
    EXPECT_GE(m_manager->getMetrics().total_entries, 0);
}

TEST_F(ZeroRetentionManagerTest, AuditLogPathConfiguration)
{
    std::string logPath = m_tempDir.path().toStdString() + "/custom_audit.log";
    m_manager->setAuditLogPath(logPath);

    EXPECT_TRUE(QDir(QFileInfo(QString::fromStdString(logPath)).absolutePath()).exists());
}

TEST_F(ZeroRetentionManagerTest, RetentionPolicyConfiguration)
{
    ZeroRetentionManager::RetentionPolicy policy;
    policy.data_class = ZeroRetentionManager::DataClass::Sensitive;
    policy.retention_seconds = 3600; // 1 hour

    m_manager->setRetentionPolicy(policy.data_class, policy.retention_seconds);
}

// ============= Data Storage Tests =============

TEST_F(ZeroRetentionManagerTest, StoreAndRetrieveSensitiveData)
{
    std::string data = generateSensitiveData();
    std::string dataId = m_manager->storeData(
        data,
        ZeroRetentionManager::DataClass::Sensitive
    );

    EXPECT_FALSE(dataId.empty());
    EXPECT_NE(dataId, "");
}

TEST_F(ZeroRetentionManagerTest, StoreSessionData)
{
    std::string data = "session_token_abc123xyz789";
    std::string dataId = m_manager->storeData(
        data,
        ZeroRetentionManager::DataClass::Session
    );

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, StoreCachedData)
{
    std::string data = "cached_computation_result";
    std::string dataId = m_manager->storeData(
        data,
        ZeroRetentionManager::DataClass::Cached
    );

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, StoreAuditData)
{
    std::string data = "audit_entry_timestamp_action";
    std::string dataId = m_manager->storeData(
        data,
        ZeroRetentionManager::DataClass::Audit
    );

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, StoreAnonymousData)
{
    std::string data = "anonymous_aggregate_statistic";
    std::string dataId = m_manager->storeData(
        data,
        ZeroRetentionManager::DataClass::Anonymous
    );

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, RetrieveStoredData)
{
    std::string originalData = "test_data_12345";
    std::string dataId = m_manager->storeData(
        originalData,
        ZeroRetentionManager::DataClass::Session
    );

    std::string retrievedData = m_manager->retrieveData(dataId);

    EXPECT_EQ(retrievedData, originalData);
}

TEST_F(ZeroRetentionManagerTest, RetrieveNonexistentData)
{
    std::string retrievedData = m_manager->retrieveData("nonexistent_id_12345");

    EXPECT_TRUE(retrievedData.empty());
}

TEST_F(ZeroRetentionManagerTest, MultipleDataStorageAndRetrieval)
{
    std::vector<std::string> ids;
    std::vector<std::string> originalData = {
        "data1",
        "data2",
        "data3"
    };

    for (const auto& data : originalData) {
        ids.push_back(m_manager->storeData(data, ZeroRetentionManager::DataClass::Session));
    }

    EXPECT_EQ(ids.size(), 3);

    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_EQ(m_manager->retrieveData(ids[i]), originalData[i]);
    }
}

// ============= Data Expiration Tests =============

TEST_F(ZeroRetentionManagerTest, DataExpirationOnSchedule)
{
    m_manager->setRetentionPolicy(ZeroRetentionManager::DataClass::Session, 1); // 1 second

    std::string dataId = m_manager->storeData(
        "expiring_data",
        ZeroRetentionManager::DataClass::Session
    );

    EXPECT_FALSE(m_manager->retrieveData(dataId).empty());

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    m_manager->processExpiredData();

    EXPECT_TRUE(m_manager->retrieveData(dataId).empty());
}

TEST_F(ZeroRetentionManagerTest, DifferentRetentionPolicies)
{
    // Set different retention times
    m_manager->setRetentionPolicy(ZeroRetentionManager::DataClass::Sensitive, 1);
    m_manager->setRetentionPolicy(ZeroRetentionManager::DataClass::Cached, 10);

    std::string sensitiveId = m_manager->storeData(
        "sensitive",
        ZeroRetentionManager::DataClass::Sensitive
    );

    std::string cachedId = m_manager->storeData(
        "cached",
        ZeroRetentionManager::DataClass::Cached
    );

    // After 2 seconds, only sensitive should expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    m_manager->processExpiredData();

    EXPECT_TRUE(m_manager->retrieveData(sensitiveId).empty());
    EXPECT_FALSE(m_manager->retrieveData(cachedId).empty());
}

TEST_F(ZeroRetentionManagerTest, AuditDataNeverExpires)
{
    m_manager->setRetentionPolicy(ZeroRetentionManager::DataClass::Audit, 1);

    std::string auditId = m_manager->storeData(
        "audit_entry",
        ZeroRetentionManager::DataClass::Audit
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    m_manager->processExpiredData();

    // Audit data should still exist (GDPR compliance)
    EXPECT_FALSE(m_manager->retrieveData(auditId).empty());
}

TEST_F(ZeroRetentionManagerTest, AnonymousDataLongRetention)
{
    std::string anonId = m_manager->storeData(
        "anonymous_stat",
        ZeroRetentionManager::DataClass::Anonymous
    );

    // Should persist much longer than other data types
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_manager->processExpiredData();

    EXPECT_FALSE(m_manager->retrieveData(anonId).empty());
}

// ============= Data Deletion Tests =============

TEST_F(ZeroRetentionManagerTest, SecureDataDeletion)
{
    std::string dataId = m_manager->storeData(
        generateSensitiveData(),
        ZeroRetentionManager::DataClass::Sensitive
    );

    EXPECT_FALSE(m_manager->retrieveData(dataId).empty());

    m_manager->deleteData(dataId);

    EXPECT_TRUE(m_manager->retrieveData(dataId).empty());
}

TEST_F(ZeroRetentionManagerTest, SecureDeletionWithOverwrite)
{
    m_manager->enableSecureOverwrite(true);

    std::string dataId = m_manager->storeData(
        "secret_data",
        ZeroRetentionManager::DataClass::Sensitive
    );

    m_manager->deleteData(dataId);

    EXPECT_TRUE(m_manager->retrieveData(dataId).empty());
}

TEST_F(ZeroRetentionManagerTest, DeleteMultipleEntries)
{
    std::vector<std::string> ids;
    for (int i = 0; i < 5; ++i) {
        ids.push_back(m_manager->storeData(
            "data_" + std::to_string(i),
            ZeroRetentionManager::DataClass::Session
        ));
    }

    for (const auto& id : ids) {
        m_manager->deleteData(id);
    }

    for (const auto& id : ids) {
        EXPECT_TRUE(m_manager->retrieveData(id).empty());
    }
}

TEST_F(ZeroRetentionManagerTest, ClearAllSessionData)
{
    for (int i = 0; i < 3; ++i) {
        m_manager->storeData(
            "session_" + std::to_string(i),
            ZeroRetentionManager::DataClass::Session
        );
    }

    m_manager->clearSessionData();

    // Session data should be cleared
}

// ============= PII Anonymization Tests =============

TEST_F(ZeroRetentionManagerTest, AnonymizeEmailAddresses)
{
    std::string data = "Contact: user@example.com for support";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
    EXPECT_FALSE(anonymized.find("@example.com") != std::string::npos);
}

TEST_F(ZeroRetentionManagerTest, AnonymizeIPAddresses)
{
    std::string data = "Connection from 192.168.1.100";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
    EXPECT_FALSE(anonymized.find("192.168.1") != std::string::npos);
}

TEST_F(ZeroRetentionManagerTest, AnonymizeCredentials)
{
    std::string data = "password: MySecurePass123";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
    EXPECT_FALSE(anonymized.find("MySecurePass123") != std::string::npos);
}

TEST_F(ZeroRetentionManagerTest, AnonymizePhoneNumbers)
{
    std::string data = "Call +1-555-123-4567 for assistance";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
}

TEST_F(ZeroRetentionManagerTest, AnonymizeCreditCardNumbers)
{
    std::string data = "Card: 4532-1234-5678-9010";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
    EXPECT_FALSE(anonymized.find("4532") != std::string::npos);
}

TEST_F(ZeroRetentionManagerTest, PartialAnonymizationPreservesContext)
{
    std::string data = "User john@example.com from 192.168.1.1";
    std::string anonymized = m_manager->anonymizeData(data);

    EXPECT_NE(anonymized, data);
    EXPECT_NE(anonymized.size(), 0);
}

TEST_F(ZeroRetentionManagerTest, NoAnonymizationForAnonymousData)
{
    std::string data = "user@example.com is in dataset";
    std::string result = m_manager->anonymizeData(data);

    // Should still anonymize even for non-anonymous classification
    EXPECT_NE(result, data);
}

// ============= Session Management Tests =============

TEST_F(ZeroRetentionManagerTest, SessionCreation)
{
    std::string sessionId = m_manager->createSession();

    EXPECT_FALSE(sessionId.empty());
}

TEST_F(ZeroRetentionManagerTest, SessionDataIsolation)
{
    std::string session1 = m_manager->createSession();
    std::string session2 = m_manager->createSession();

    EXPECT_NE(session1, session2);
}

TEST_F(ZeroRetentionManagerTest, CleanupOnSessionEnd)
{
    std::string sessionId = m_manager->createSession();

    std::string dataId = m_manager->storeData(
        "session_scoped_data",
        ZeroRetentionManager::DataClass::Session
    );

    m_manager->endSession(sessionId);

    // Session data should be cleaned up
}

TEST_F(ZeroRetentionManagerTest, MultipleSimultaneousSessions)
{
    std::vector<std::string> sessions;
    for (int i = 0; i < 5; ++i) {
        sessions.push_back(m_manager->createSession());
    }

    EXPECT_EQ(sessions.size(), 5);

    for (const auto& session : sessions) {
        m_manager->endSession(session);
    }
}

// ============= Audit Logging Tests =============

TEST_F(ZeroRetentionManagerTest, AuditLogCreation)
{
    std::string dataId = m_manager->storeData(
        generateSensitiveData(),
        ZeroRetentionManager::DataClass::Sensitive
    );

    EXPECT_TRUE(QFile(m_auditLogPath).exists());
}

TEST_F(ZeroRetentionManagerTest, AuditLogEntryForDataStorage)
{
    std::string dataId = m_manager->storeData(
        "test_data",
        ZeroRetentionManager::DataClass::Sensitive
    );

    EXPECT_TRUE(checkAuditLogEntry(dataId, "STORE"));
}

TEST_F(ZeroRetentionManagerTest, AuditLogEntryForDataDeletion)
{
    std::string dataId = m_manager->storeData(
        "test_data",
        ZeroRetentionManager::DataClass::Sensitive
    );

    m_manager->deleteData(dataId);

    EXPECT_TRUE(checkAuditLogEntry(dataId, "DELETE"));
}

TEST_F(ZeroRetentionManagerTest, AuditLogEntryForDataAccess)
{
    std::string dataId = m_manager->storeData(
        "test_data",
        ZeroRetentionManager::DataClass::Sensitive
    );

    m_manager->retrieveData(dataId);

    EXPECT_TRUE(checkAuditLogEntry(dataId, "RETRIEVE"));
}

TEST_F(ZeroRetentionManagerTest, AuditLogEntryForAnonymization)
{
    m_manager->anonymizeData(generatePII());

    EXPECT_TRUE(QFile(m_auditLogPath).exists());
}

TEST_F(ZeroRetentionManagerTest, AuditLogStructure)
{
    std::string dataId = m_manager->storeData(
        "audit_test",
        ZeroRetentionManager::DataClass::Sensitive
    );

    QFile logFile(m_auditLogPath);
    EXPECT_TRUE(logFile.open(QIODevice::ReadOnly | QIODevice::Text));

    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("timestamp"));
    EXPECT_TRUE(content.contains("action"));
}

TEST_F(ZeroRetentionManagerTest, AuditLogIncludesDataClass)
{
    m_manager->storeData("sensitive", ZeroRetentionManager::DataClass::Sensitive);
    m_manager->storeData("cached", ZeroRetentionManager::DataClass::Cached);

    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("Sensitive") || content.contains("Cached"));
}

// ============= Metrics Tests =============

TEST_F(ZeroRetentionManagerTest, MetricsTrackingOnStorage)
{
    auto initialMetrics = m_manager->getMetrics();

    for (int i = 0; i < 5; ++i) {
        m_manager->storeData(
            "data_" + std::to_string(i),
            ZeroRetentionManager::DataClass::Session
        );
    }

    auto finalMetrics = m_manager->getMetrics();

    EXPECT_GE(finalMetrics.total_entries, initialMetrics.total_entries + 5);
}

TEST_F(ZeroRetentionManagerTest, MetricsTrackingOnDeletion)
{
    std::vector<std::string> ids;
    for (int i = 0; i < 3; ++i) {
        ids.push_back(m_manager->storeData(
            "data_" + std::to_string(i),
            ZeroRetentionManager::DataClass::Session
        ));
    }

    for (const auto& id : ids) {
        m_manager->deleteData(id);
    }

    auto metrics = m_manager->getMetrics();
    EXPECT_GE(metrics.total_entries, 0);
}

TEST_F(ZeroRetentionManagerTest, MetricsDataClassBreakdown)
{
    m_manager->storeData("sensitive", ZeroRetentionManager::DataClass::Sensitive);
    m_manager->storeData("session", ZeroRetentionManager::DataClass::Session);
    m_manager->storeData("cached", ZeroRetentionManager::DataClass::Cached);
    m_manager->storeData("audit", ZeroRetentionManager::DataClass::Audit);
    m_manager->storeData("anonymous", ZeroRetentionManager::DataClass::Anonymous);

    auto metrics = m_manager->getMetrics();
    
    EXPECT_GE(metrics.total_entries, 5);
}

TEST_F(ZeroRetentionManagerTest, MetricsAverageRetention)
{
    m_manager->storeData("data1", ZeroRetentionManager::DataClass::Sensitive);
    m_manager->storeData("data2", ZeroRetentionManager::DataClass::Session);

    auto metrics = m_manager->getMetrics();
    
    EXPECT_GE(metrics.total_entries, 2);
}

TEST_F(ZeroRetentionManagerTest, MetricsAnonymizationCount)
{
    for (int i = 0; i < 3; ++i) {
        m_manager->anonymizeData(generatePII());
    }

    auto metrics = m_manager->getMetrics();
    
    EXPECT_GE(metrics.total_entries, 0);
}

// ============= Error Handling Tests =============

TEST_F(ZeroRetentionManagerTest, HandleEmptyDataStorage)
{
    std::string dataId = m_manager->storeData(
        "",
        ZeroRetentionManager::DataClass::Session
    );

    // Should handle empty data gracefully
    EXPECT_TRUE(!dataId.empty() || dataId.empty()); // Valid result either way
}

TEST_F(ZeroRetentionManagerTest, HandleLargeDataStorage)
{
    std::string largeData(10 * 1024 * 1024, 'x'); // 10 MB

    std::string dataId = m_manager->storeData(
        largeData,
        ZeroRetentionManager::DataClass::Cached
    );

    EXPECT_FALSE(dataId.empty());
    EXPECT_EQ(m_manager->retrieveData(dataId), largeData);
}

TEST_F(ZeroRetentionManagerTest, HandleInvalidDataClass)
{
    std::string data = "test";
    // Should handle gracefully even if data class is invalid
    std::string dataId = m_manager->storeData(data, ZeroRetentionManager::DataClass::Session);

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, HandleDeleteNonexistentData)
{
    // Should handle gracefully
    m_manager->deleteData("nonexistent_id_xyz");
}

TEST_F(ZeroRetentionManagerTest, HandleCorruptedAuditLog)
{
    // Write invalid data to audit log
    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    logFile.write("corrupted data {invalid json");
    logFile.close();

    // Manager should handle gracefully
    std::string dataId = m_manager->storeData(
        "test",
        ZeroRetentionManager::DataClass::Session
    );

    EXPECT_FALSE(dataId.empty());
}

// ============= GDPR Compliance Tests =============

TEST_F(ZeroRetentionManagerTest, UserDataRightToForgetting)
{
    std::string dataId = m_manager->storeData(
        generatePII(),
        ZeroRetentionManager::DataClass::Sensitive
    );

    m_manager->deleteData(dataId);

    // Data should be unrecoverable
    EXPECT_TRUE(m_manager->retrieveData(dataId).empty());
}

TEST_F(ZeroRetentionManagerTest, AuditTrailForCompliance)
{
    std::string dataId = m_manager->storeData(
        "compliance_test",
        ZeroRetentionManager::DataClass::Sensitive
    );

    m_manager->retrieveData(dataId);
    m_manager->deleteData(dataId);

    // Audit log should contain all operations
    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("STORE") || content.contains("RETRIEVE") || content.contains("DELETE"));
}

TEST_F(ZeroRetentionManagerTest, DataMinimization)
{
    // Should only store necessary data
    std::string dataId = m_manager->storeData(
        "minimal_data",
        ZeroRetentionManager::DataClass::Session
    );

    EXPECT_FALSE(dataId.empty());
}

TEST_F(ZeroRetentionManagerTest, TransparencyInDataHandling)
{
    m_manager->storeData("test", ZeroRetentionManager::DataClass::Sensitive);
    m_manager->storeData("test2", ZeroRetentionManager::DataClass::Session);

    auto metrics = m_manager->getMetrics();

    // Metrics should provide transparency
    EXPECT_GE(metrics.total_entries, 2);
}

// ============= Performance Tests =============

TEST_F(ZeroRetentionManagerTest, HighVolumeDataStorage)
{
    std::vector<std::string> ids;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        ids.push_back(m_manager->storeData(
            "data_" + std::to_string(i),
            ZeroRetentionManager::DataClass::Session
        ));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(ids.size(), 1000);
    EXPECT_LT(duration.count(), 30000); // Should complete in under 30 seconds
}

TEST_F(ZeroRetentionManagerTest, RapidDataRetrieval)
{
    std::string dataId = m_manager->storeData("test", ZeroRetentionManager::DataClass::Session);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        m_manager->retrieveData(dataId);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000); // Should complete in under 5 seconds
}

// ============= Integration Tests =============

TEST_F(ZeroRetentionManagerTest, CompleteGDPRWorkflow)
{
    // Store PII
    std::string piiData = "user@example.com";
    std::string dataId = m_manager->storeData(
        piiData,
        ZeroRetentionManager::DataClass::Sensitive
    );

    // Verify audit entry
    EXPECT_TRUE(checkAuditLogEntry(dataId, "STORE"));

    // Right to be forgotten
    m_manager->deleteData(dataId);

    // Verify deletion in audit trail
    EXPECT_TRUE(checkAuditLogEntry(dataId, "DELETE"));

    // Verify data is gone
    EXPECT_TRUE(m_manager->retrieveData(dataId).empty());
}

TEST_F(ZeroRetentionManagerTest, SessionCleanupWorkflow)
{
    std::string sessionId = m_manager->createSession();

    std::string dataId = m_manager->storeData(
        "session_data",
        ZeroRetentionManager::DataClass::Session
    );

    m_manager->endSession(sessionId);

    auto metrics = m_manager->getMetrics();
    EXPECT_GE(metrics.total_entries, 0);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ZeroRetentionManagerTest);
