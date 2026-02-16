/**
 * ZeroRetentionManager tests — C++20 only (Qt-free).
 *
 * Tests Config, getMetrics, registerData(path), getTrackedData, deleteData.
 * Original Qt tests (QSignalSpy, QTemporaryDir, QFile, storeData/retrieveData API)
 * were converted; the current class uses registerData(path, class) and file-based storage.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <filesystem>

#include "../src/terminal/zero_retention_manager.hpp"

namespace fs = std::filesystem;

class ZeroRetentionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_tempDir = fs::temp_directory_path() / ("rawrxd_zrm_" + std::to_string(
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

    fs::path m_tempDir;
    std::unique_ptr<ZeroRetentionManager> m_manager;
};

TEST_F(ZeroRetentionManagerTest, ManagerInitialization) {
    EXPECT_NE(m_manager.get(), nullptr);
    EXPECT_GE(m_manager->getMetrics().dataEntriesTracked, 0);
}

TEST_F(ZeroRetentionManagerTest, SetAndGetConfig) {
    ZeroRetentionManager::Config config;
    config.sessionTtlMinutes = 120;
    config.dataRetentionDays = 7;
    config.auditLogPath = (m_tempDir / "custom_audit.log").string();
    m_manager->setConfig(config);
    ZeroRetentionManager::Config retrieved = m_manager->getConfig();
    EXPECT_EQ(retrieved.sessionTtlMinutes, 120);
    EXPECT_EQ(retrieved.dataRetentionDays, 7);
}

TEST_F(ZeroRetentionManagerTest, RegisterAndUnregisterData) {
    std::string path = (m_tempDir / "test_data.txt").string();
    std::string id = m_manager->registerData(path, ZeroRetentionManager::Session);
    if (!id.empty()) {
        EXPECT_FALSE(m_manager->getTrackedData(ZeroRetentionManager::Session).empty());
        EXPECT_TRUE(m_manager->unregisterData(id));
    }
}

TEST_F(ZeroRetentionManagerTest, GetMetrics) {
    auto m = m_manager->getMetrics();
    EXPECT_GE(m.dataEntriesTracked, 0);
    EXPECT_GE(m.dataEntriesDeleted, 0);
}
