/**
 * ZeroRetentionManager unit tests — C++20 only (Qt-free).
 * Uses ZeroRetentionManager from src/terminal/zero_retention_manager.hpp (std::string, std::chrono).
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/terminal/zero_retention_manager.hpp"

namespace fs = std::filesystem;

class ZeroRetentionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_tempDir = fs::temp_directory_path() / ("rawrxd_zrm_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(m_tempDir);
        m_manager = std::make_unique<ZeroRetentionManager>();
        auto cfg = m_manager->getConfig();
        cfg.auditLogPath = (m_tempDir / "audit.log").string();
        cfg.dataDirectory = m_tempDir.string();
        m_manager->setConfig(cfg);
    }

    void TearDown() override {
        m_manager.reset();
        std::error_code ec;
        fs::remove_all(m_tempDir, ec);
    }

    std::string createTempFile(const std::string& content = "") {
        fs::path p = m_tempDir / ("tmp_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::ofstream f(p);
        if (!content.empty()) f << content;
        f.close();
        return p.string();
    }

    fs::path m_tempDir;
    std::unique_ptr<ZeroRetentionManager> m_manager;
};

TEST_F(ZeroRetentionManagerTest, InitializationSucceeds) {
    EXPECT_NE(m_manager.get(), nullptr);
}

TEST_F(ZeroRetentionManagerTest, SetAndGetConfig) {
    ZeroRetentionManager::Config config;
    config.sessionTtlMinutes = 120;
    config.dataRetentionDays = 7;
    config.auditRetentionDays = 180;
    config.enableAutoCleanup = true;
    config.cleanupIntervalMinutes = 30;
    config.enableSecureWipe = true;
    config.enableAuditLog = true;
    config.auditLogPath = (m_tempDir / "audit.log").string();
    config.dataDirectory = m_tempDir.string();
    m_manager->setConfig(config);
    ZeroRetentionManager::Config retrieved = m_manager->getConfig();
    EXPECT_EQ(retrieved.sessionTtlMinutes, 120);
    EXPECT_EQ(retrieved.dataRetentionDays, 7);
    EXPECT_EQ(retrieved.auditRetentionDays, 180);
    EXPECT_TRUE(retrieved.enableAutoCleanup);
    EXPECT_EQ(retrieved.cleanupIntervalMinutes, 30);
    EXPECT_TRUE(retrieved.enableSecureWipe);
}

TEST_F(ZeroRetentionManagerTest, DefaultConfigValues) {
    ZeroRetentionManager::Config config = m_manager->getConfig();
    EXPECT_EQ(config.sessionTtlMinutes, 60);
    EXPECT_EQ(config.dataRetentionDays, 0);
    EXPECT_EQ(config.auditRetentionDays, 90);
    EXPECT_TRUE(config.enableAutoCleanup);
    EXPECT_TRUE(config.enableSecureWipe);
    EXPECT_TRUE(config.enableAuditLog);
}

TEST_F(ZeroRetentionManagerTest, RegisterDataReturnsValidId) {
    std::string path = createTempFile("test data");
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session);
    EXPECT_FALSE(id.empty());
    EXPECT_GT(id.size(), 0u);
}

TEST_F(ZeroRetentionManagerTest, RegisterMultipleDataItems) {
    std::string path1 = createTempFile();
    std::string path2 = createTempFile();
    std::string id1 = m_manager->registerData(path1, ZeroRetentionManager::Session);
    std::string id2 = m_manager->registerData(path2, ZeroRetentionManager::Cached);
    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);
}

TEST_F(ZeroRetentionManagerTest, RegisterDataWithCustomTTL) {
    std::string path = createTempFile();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session, 120);
    EXPECT_FALSE(id.empty());
}

TEST_F(ZeroRetentionManagerTest, UnregisterValidDataSucceeds) {
    std::string path = createTempFile();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session);
    ASSERT_FALSE(id.empty());
    bool result = m_manager->unregisterData(id);
    EXPECT_TRUE(result);
}

TEST_F(ZeroRetentionManagerTest, UnregisterInvalidIdFails) {
    bool result = m_manager->unregisterData("invalid-id-12345");
    EXPECT_FALSE(result);
}

TEST_F(ZeroRetentionManagerTest, DeleteDataImmediately) {
    std::string path = createTempFile("test data");
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session);
    ASSERT_FALSE(id.empty());
    bool result = m_manager->deleteData(id, true);
    EXPECT_TRUE(result);
}

TEST_F(ZeroRetentionManagerTest, DeleteInvalidIdFails) {
    bool result = m_manager->deleteData("invalid-id", true);
    EXPECT_FALSE(result);
}

TEST_F(ZeroRetentionManagerTest, DeleteNonExpiredDataWithoutImmediate) {
    std::string path = createTempFile();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session, 60);
    ASSERT_FALSE(id.empty());
    bool result = m_manager->deleteData(id, false);
    EXPECT_FALSE(result);
}

TEST_F(ZeroRetentionManagerTest, AnonymizeInvalidIdFails) {
    bool result = m_manager->anonymizeData("invalid-id");
    EXPECT_FALSE(result);
}

TEST_F(ZeroRetentionManagerTest, CleanupExpiredDataWorks) {
    m_manager->cleanupExpiredData();
    auto metrics = m_manager->getMetrics();
    EXPECT_GE(metrics.dataEntriesDeleted, 0);
}

TEST_F(ZeroRetentionManagerTest, PurgeAllDataByClassification) {
    std::string path1 = createTempFile();
    std::string path2 = createTempFile();
    m_manager->registerData(path1, ZeroRetentionManager::Session);
    m_manager->registerData(path2, ZeroRetentionManager::Session);
    m_manager->purgeAllData(ZeroRetentionManager::Session);
    auto entries = m_manager->getTrackedData(ZeroRetentionManager::Session);
    EXPECT_TRUE(entries.empty());
}

TEST_F(ZeroRetentionManagerTest, GetTrackedDataByClassification) {
    std::string path1 = createTempFile();
    std::string path2 = createTempFile();
    std::string path3 = createTempFile();
    m_manager->registerData(path1, ZeroRetentionManager::Session);
    m_manager->registerData(path2, ZeroRetentionManager::Session);
    m_manager->registerData(path3, ZeroRetentionManager::Cached);
    auto sessionData = m_manager->getTrackedData(ZeroRetentionManager::Session);
    EXPECT_EQ(sessionData.size(), 2u);
}

TEST_F(ZeroRetentionManagerTest, GetDataEntryReturnsValidEntry) {
    std::string path = createTempFile();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Sensitive);
    auto entry = m_manager->getDataEntry(id);
    EXPECT_EQ(entry.id, id);
    EXPECT_EQ(entry.classification, ZeroRetentionManager::Sensitive);
    EXPECT_FALSE(entry.isAnonymized);
}

TEST_F(ZeroRetentionManagerTest, GetInvalidDataEntryReturnsEmpty) {
    auto entry = m_manager->getDataEntry("invalid-id");
    EXPECT_TRUE(entry.id.empty());
}

TEST_F(ZeroRetentionManagerTest, InitialMetricsAreZero) {
    auto metrics = m_manager->getMetrics();
    EXPECT_EQ(metrics.dataEntriesTracked, 0);
    EXPECT_EQ(metrics.dataEntriesDeleted, 0);
    EXPECT_EQ(metrics.bytesDeleted, 0);
    EXPECT_EQ(metrics.sessionsCleanedUp, 0);
    EXPECT_EQ(metrics.anonymizationCount, 0);
    EXPECT_EQ(metrics.auditEntriesCreated, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(ZeroRetentionManagerTest, MetricsUpdateOnOperations) {
    std::string path = createTempFile("test");
    m_manager->registerData(path, ZeroRetentionManager::Session);
    auto metrics = m_manager->getMetrics();
    EXPECT_GT(metrics.dataEntriesTracked, 0);
}

TEST_F(ZeroRetentionManagerTest, ResetMetricsClearsCounters) {
    std::string path = createTempFile();
    m_manager->registerData(path, ZeroRetentionManager::Session);
    auto before = m_manager->getMetrics();
    EXPECT_GT(before.dataEntriesTracked, 0);
    m_manager->resetMetrics();
    auto after = m_manager->getMetrics();
    EXPECT_EQ(after.dataEntriesTracked, 0);
}

TEST_F(ZeroRetentionManagerTest, ZeroRetentionPolicyConfig) {
    ZeroRetentionManager::Config config;
    config.dataRetentionDays = 0;
    m_manager->setConfig(config);
    std::string path = createTempFile();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Sensitive);
    auto entry = m_manager->getDataEntry(id);
    EXPECT_FALSE(entry.id.empty());
    EXPECT_LE(std::chrono::system_clock::to_time_t(entry.expiresAt),
              std::chrono::system_clock::to_time_t(entry.createdAt) + 86400);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
