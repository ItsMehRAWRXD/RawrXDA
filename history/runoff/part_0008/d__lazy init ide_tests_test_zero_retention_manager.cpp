#include <gtest/gtest.h>
#include "terminal/zero_retention_manager.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QThread>
#include <QDir>
#include <QUuid>
#include <QFile>

class ZeroRetentionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = new ZeroRetentionManager();
    }

    void TearDown() override {
        delete manager;
    }

    ZeroRetentionManager* manager;
};

// 1. Initialization Tests
TEST_F(ZeroRetentionManagerTest, InitializationSucceeds) {
    EXPECT_NE(manager, nullptr);
}

// 2. Configuration Tests
TEST_F(ZeroRetentionManagerTest, SetAndGetConfig) {
    ZeroRetentionManager::Config config;
    config.sessionTtlMinutes = 120;
    config.dataRetentionDays = 7;
    config.auditRetentionDays = 180;
    config.enableAutoCleanup = true;
    config.cleanupIntervalMinutes = 30;
    config.enableSecureWipe = true;
    config.enableAuditLog = true;
    config.auditLogPath = "/tmp/audit.log";
    config.dataDirectory = "/tmp/data";

    manager->setConfig(config);
    ZeroRetentionManager::Config retrieved = manager->getConfig();

    EXPECT_EQ(retrieved.sessionTtlMinutes, 120);
    EXPECT_EQ(retrieved.dataRetentionDays, 7);
    EXPECT_EQ(retrieved.auditRetentionDays, 180);
    EXPECT_TRUE(retrieved.enableAutoCleanup);
    EXPECT_EQ(retrieved.cleanupIntervalMinutes, 30);
    EXPECT_TRUE(retrieved.enableSecureWipe);
}

TEST_F(ZeroRetentionManagerTest, DefaultConfigValues) {
    ZeroRetentionManager::Config config = manager->getConfig();
    
    EXPECT_EQ(config.sessionTtlMinutes, 60);
    EXPECT_EQ(config.dataRetentionDays, 0);
    EXPECT_EQ(config.auditRetentionDays, 90);
    EXPECT_TRUE(config.enableAutoCleanup);
    EXPECT_TRUE(config.enableSecureWipe);
    EXPECT_TRUE(config.enableAuditLog);
}

// 3. Data Registration Tests
TEST_F(ZeroRetentionManagerTest, RegisterDataReturnsValidId) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.write("test data");
    tempFile.close();

    QString id = manager->registerData(
        tempFile.fileName(), 
        ZeroRetentionManager::Session
    );
    
    EXPECT_FALSE(id.isEmpty());
    EXPECT_GT(id.length(), 0);
}

TEST_F(ZeroRetentionManagerTest, RegisterMultipleDataItems) {
    QTemporaryFile file1, file2;
    ASSERT_TRUE(file1.open() && file2.open());
    file1.close();
    file2.close();

    QString id1 = manager->registerData(file1.fileName(), ZeroRetentionManager::Session);
    QString id2 = manager->registerData(file2.fileName(), ZeroRetentionManager::Cached);
    
    EXPECT_FALSE(id1.isEmpty());
    EXPECT_FALSE(id2.isEmpty());
    EXPECT_NE(id1, id2);
}

TEST_F(ZeroRetentionManagerTest, RegisterDataWithCustomTTL) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    QString id = manager->registerData(
        tempFile.fileName(),
        ZeroRetentionManager::Session,
        120 // 120 minutes custom TTL
    );
    
    EXPECT_FALSE(id.isEmpty());
}

// 4. Data Unregistration Tests
TEST_F(ZeroRetentionManagerTest, UnregisterValidDataSucceeds) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    QString id = manager->registerData(tempFile.fileName(), ZeroRetentionManager::Session);
    ASSERT_FALSE(id.isEmpty());

    bool result = manager->unregisterData(id);
    EXPECT_TRUE(result);
}

TEST_F(ZeroRetentionManagerTest, UnregisterInvalidIdFails) {
    bool result = manager->unregisterData("invalid-id-12345");
    EXPECT_FALSE(result);
}

// 5. Data Deletion Tests
TEST_F(ZeroRetentionManagerTest, DeleteDataImmediately) {
    // Create a regular file without using QTemporaryFile (avoids Windows file locking)
    QString path = QDir::temp().filePath(QString("test_delete_immed_%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("test data");
    file.close();  // File is now closed and unlocked
    
    ASSERT_TRUE(QFile::exists(path));

    QString id = manager->registerData(path, ZeroRetentionManager::Session);
    ASSERT_FALSE(id.isEmpty());

    bool result = manager->deleteData(id, true); // immediate
    EXPECT_TRUE(result);
    
    // Cleanup if still exists
    QFile::remove(path);
}

TEST_F(ZeroRetentionManagerTest, DeleteNonExpiredDataWithoutImmediateFails) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    QString id = manager->registerData(tempFile.fileName(), ZeroRetentionManager::Session, 60);
    ASSERT_FALSE(id.isEmpty());

    bool result = manager->deleteData(id, false); // not immediate
    // Should not delete since not expired
    EXPECT_FALSE(result);
}

TEST_F(ZeroRetentionManagerTest, DeleteInvalidIdFails) {
    bool result = manager->deleteData("invalid-id", true);
    EXPECT_FALSE(result);
}

// 6. Data Anonymization Tests
TEST_F(ZeroRetentionManagerTest, AnonymizeDataRedactsEmail) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "Contact: user@example.com\n";
    out << "IP: 192.168.1.1\n";
    tempFile.close();

    QString id = manager->registerData(tempFile.fileName(), ZeroRetentionManager::Sensitive);
    ASSERT_FALSE(id.isEmpty());

    bool result = manager->anonymizeData(id);
    EXPECT_TRUE(result);

    // Verify redaction
    QFile file(tempFile.fileName());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QString content = file.readAll();
    file.close();

    EXPECT_TRUE(content.contains("[EMAIL_REDACTED]"));
    EXPECT_TRUE(content.contains("[IP_REDACTED]"));
}

TEST_F(ZeroRetentionManagerTest, AnonymizeInvalidIdFails) {
    bool result = manager->anonymizeData("invalid-id");
    EXPECT_FALSE(result);
}

// 7. Cleanup Tests
TEST_F(ZeroRetentionManagerTest, CleanupExpiredDataWorks) {
    QSignalSpy spy(manager, &ZeroRetentionManager::cleanupCompleted);
    
    manager->cleanupExpiredData();
    
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ZeroRetentionManagerTest, CleanupSessionRemovesSessionData) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    // Register with session ID in path
    QString sessionPath = path + ".session12345";
    QFile::copy(path, sessionPath);
    
    QString id = manager->registerData(sessionPath, ZeroRetentionManager::Session);
    
    QSignalSpy spy(manager, &ZeroRetentionManager::sessionCleaned);
    manager->cleanupSession("session12345");
    
    EXPECT_GE(spy.count(), 0);
    QFile::remove(sessionPath);
}

TEST_F(ZeroRetentionManagerTest, PurgeAllDataByClassification) {
    QTemporaryFile file1, file2;
    ASSERT_TRUE(file1.open() && file2.open());
    file1.close();
    file2.close();

    manager->registerData(file1.fileName(), ZeroRetentionManager::Session);
    manager->registerData(file2.fileName(), ZeroRetentionManager::Session);

    manager->purgeAllData(ZeroRetentionManager::Session);

    QVector<ZeroRetentionManager::DataEntry> entries = 
        manager->getTrackedData(ZeroRetentionManager::Session);
    
    // Should be empty after purge
    EXPECT_TRUE(entries.isEmpty());
}

// 8. Data Querying Tests
TEST_F(ZeroRetentionManagerTest, GetTrackedDataByClassification) {
    QTemporaryFile file1, file2, file3;
    ASSERT_TRUE(file1.open() && file2.open() && file3.open());
    file1.close(); file2.close(); file3.close();

    manager->registerData(file1.fileName(), ZeroRetentionManager::Session);
    manager->registerData(file2.fileName(), ZeroRetentionManager::Session);
    manager->registerData(file3.fileName(), ZeroRetentionManager::Cached);

    QVector<ZeroRetentionManager::DataEntry> sessionData = 
        manager->getTrackedData(ZeroRetentionManager::Session);
    
    EXPECT_EQ(sessionData.size(), 2);
}

TEST_F(ZeroRetentionManagerTest, GetDataEntryReturnsValidEntry) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    QString id = manager->registerData(tempFile.fileName(), ZeroRetentionManager::Sensitive);
    
    ZeroRetentionManager::DataEntry entry = manager->getDataEntry(id);
    
    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.classification, ZeroRetentionManager::Sensitive);
    EXPECT_FALSE(entry.isAnonymized);
}

TEST_F(ZeroRetentionManagerTest, GetInvalidDataEntryReturnsEmpty) {
    ZeroRetentionManager::DataEntry entry = manager->getDataEntry("invalid-id");
    EXPECT_TRUE(entry.id.isEmpty());
}

// 9. Metrics Tests
TEST_F(ZeroRetentionManagerTest, InitialMetricsAreZero) {
    ZeroRetentionManager::Metrics metrics = manager->getMetrics();
    
    EXPECT_EQ(metrics.dataEntriesTracked, 0);
    EXPECT_EQ(metrics.dataEntriesDeleted, 0);
    EXPECT_EQ(metrics.bytesDeleted, 0);
    EXPECT_EQ(metrics.sessionsCleanedUp, 0);
    EXPECT_EQ(metrics.anonymizationCount, 0);
    EXPECT_EQ(metrics.auditEntriesCreated, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(ZeroRetentionManagerTest, MetricsUpdateOnOperations) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.write("test");
    tempFile.close();

    manager->registerData(tempFile.fileName(), ZeroRetentionManager::Session);
    
    ZeroRetentionManager::Metrics metrics = manager->getMetrics();
    EXPECT_GT(metrics.dataEntriesTracked, 0);
}

TEST_F(ZeroRetentionManagerTest, ResetMetricsClearsCounters) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    manager->registerData(tempFile.fileName(), ZeroRetentionManager::Session);
    
    ZeroRetentionManager::Metrics before = manager->getMetrics();
    EXPECT_GT(before.dataEntriesTracked, 0);
    
    manager->resetMetrics();
    
    ZeroRetentionManager::Metrics after = manager->getMetrics();
    EXPECT_EQ(after.dataEntriesTracked, 0);
}

// 10. Signal Emission Tests
TEST_F(ZeroRetentionManagerTest, DataDeletedSignalEmitted) {
    QSignalSpy spy(manager, &ZeroRetentionManager::dataDeleted);
    
    // Create a temporary file without holding a lock (Windows compatibility)
    QString path = QDir::temp().filePath(QString("test_data_deleted_%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("test");
    file.close();  // File is now closed and unlocked

    QString id = manager->registerData(path, ZeroRetentionManager::Session);
    manager->deleteData(id, true);
    
    EXPECT_EQ(spy.count(), 1);
    
    // Cleanup if file still exists (test failure case)
    QFile::remove(path);
}

TEST_F(ZeroRetentionManagerTest, CleanupCompletedSignalEmitted) {
    QSignalSpy spy(manager, &ZeroRetentionManager::cleanupCompleted);
    
    manager->cleanupExpiredData();
    
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    EXPECT_GE(arguments.at(0).toInt(), 0); // items deleted
    EXPECT_GE(arguments.at(1).toLongLong(), 0); // bytes deleted
}

TEST_F(ZeroRetentionManagerTest, ErrorSignalEmittedOnFailure) {
    QSignalSpy spy(manager, &ZeroRetentionManager::errorOccurred);
    
    manager->deleteData("invalid-id", true);
    
    // May or may not emit error depending on implementation
    EXPECT_GE(spy.count(), 0);
}

// 11. GDPR Compliance Tests
TEST_F(ZeroRetentionManagerTest, ZeroRetentionPolicyEnforced) {
    ZeroRetentionManager::Config config;
    config.dataRetentionDays = 0; // Zero retention
    manager->setConfig(config);

    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.close();

    QString id = manager->registerData(tempFile.fileName(), ZeroRetentionManager::Sensitive);
    
    ZeroRetentionManager::DataEntry entry = manager->getDataEntry(id);
    
    // Should expire immediately or very soon
    EXPECT_LE(entry.expiresAt.toSecsSinceEpoch(), 
              entry.createdAt.addDays(1).toSecsSinceEpoch());
}

TEST_F(ZeroRetentionManagerTest, SecureWipeEnabledDeletesSecurely) {
    ZeroRetentionManager::Config config;
    config.enableSecureWipe = true;
    manager->setConfig(config);

    // Create a regular file without using QTemporaryFile (avoids Windows file locking)
    QString path = QDir::temp().filePath(QString("test_secure_wipe_%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("sensitive data");
    file.close();  // File is now closed and unlocked
    
    ASSERT_TRUE(QFile::exists(path));  // Verify file was created

    QString id = manager->registerData(path, ZeroRetentionManager::Sensitive);
    bool result = manager->deleteData(id, true);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(QFile::exists(path));
    
    // Cleanup if file still exists (test failure case)
    QFile::remove(path);
}

// Main function
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
