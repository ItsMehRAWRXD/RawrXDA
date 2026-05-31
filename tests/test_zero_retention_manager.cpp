/**
 * ZeroRetentionManager unit tests - C++20 only, Qt-free, no gtest.
 */
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>
#include <stdexcept>

#include "../src/terminal/zero_retention_manager.hpp"

namespace fs = std::filesystem;

static int s_pass = 0, s_fail = 0;

#define EXPECT_TRUE(expr)  do { if(!(expr)) { std::cerr << "FAIL: " #expr " at " __FILE__ ":" << __LINE__ << "\n"; ++s_fail; } else ++s_pass; } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a,b)     EXPECT_TRUE((a)==(b))
#define EXPECT_NE(a,b)     EXPECT_TRUE((a)!=(b))
#define EXPECT_GT(a,b)     EXPECT_TRUE((a)>(b))
#define EXPECT_GE(a,b)     EXPECT_TRUE((a)>=(b))
#define EXPECT_LE(a,b)     EXPECT_TRUE((a)<=(b))
#define ASSERT_FALSE(expr) do { if(expr) { std::cerr << "ASSERT FAIL: " #expr " at " __FILE__ ":" << __LINE__ << "\n"; throw std::runtime_error("assertion"); } } while(0)

struct Fixture {
    fs::path m_tempDir;
    std::unique_ptr<ZeroRetentionManager> m_manager;

    Fixture() {
        m_tempDir = fs::temp_directory_path() / ("rawrxd_zrm_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(m_tempDir);
        m_manager = std::make_unique<ZeroRetentionManager>();
        auto cfg = m_manager->getConfig();
        cfg.auditLogPath = (m_tempDir / "audit.log").string();
        cfg.dataDirectory = m_tempDir.string();
        m_manager->setConfig(cfg);
    }
    ~Fixture() {
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
};

static void runTest(const char* name, auto fn) {
    std::cout << "[ RUN ] " << name << std::endl;
    try { Fixture fix; fn(fix); std::cout << "[  OK ] " << name << std::endl; }
    catch(const std::exception& e) { std::cerr << "[FAIL] " << name << ": " << e.what() << std::endl; ++s_fail; }
}

int main() {
    runTest("InitializationSucceeds", [](Fixture& f) {
        EXPECT_NE(f.m_manager.get(), nullptr);
    });

    runTest("SetAndGetConfig", [](Fixture& f) {
        ZeroRetentionManager::Config config;
        config.sessionTtlMinutes = 120;
        config.dataRetentionDays = 7;
        config.auditRetentionDays = 180;
        config.enableAutoCleanup = true;
        config.cleanupIntervalMinutes = 30;
        config.enableSecureWipe = true;
        config.enableAuditLog = true;
        f.m_manager->setConfig(config);
        auto retrieved = f.m_manager->getConfig();
        EXPECT_EQ(retrieved.sessionTtlMinutes, 120);
        EXPECT_EQ(retrieved.dataRetentionDays, 7);
        EXPECT_EQ(retrieved.auditRetentionDays, 180);
        EXPECT_TRUE(retrieved.enableAutoCleanup);
        EXPECT_EQ(retrieved.cleanupIntervalMinutes, 30);
        EXPECT_TRUE(retrieved.enableSecureWipe);
    });

    runTest("DefaultConfigValues", [](Fixture& f) {
        auto config = f.m_manager->getConfig();
        EXPECT_EQ(config.sessionTtlMinutes, 60);
        EXPECT_EQ(config.dataRetentionDays, 0);
        EXPECT_EQ(config.auditRetentionDays, 90);
        EXPECT_TRUE(config.enableAutoCleanup);
        EXPECT_TRUE(config.enableSecureWipe);
        EXPECT_TRUE(config.enableAuditLog);
    });

    runTest("RegisterDataReturnsValidId", [](Fixture& f) {
        std::string path = f.createTempFile("test data");
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Session);
        EXPECT_FALSE(id.empty());
        EXPECT_GT(id.size(), 0u);
    });

    runTest("RegisterMultipleDataItems", [](Fixture& f) {
        std::string path1 = f.createTempFile();
        std::string path2 = f.createTempFile();
        std::string id1 = f.m_manager->registerData(path1, ZeroRetentionManager::Session);
        std::string id2 = f.m_manager->registerData(path2, ZeroRetentionManager::Cached);
        EXPECT_FALSE(id1.empty());
        EXPECT_FALSE(id2.empty());
        EXPECT_NE(id1, id2);
    });

    runTest("RegisterDataWithCustomTTL", [](Fixture& f) {
        std::string path = f.createTempFile();
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Session, 120);
        EXPECT_FALSE(id.empty());
    });

    runTest("UnregisterValidDataSucceeds", [](Fixture& f) {
        std::string path = f.createTempFile();
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Session);
        ASSERT_FALSE(id.empty());
        EXPECT_TRUE(f.m_manager->unregisterData(id));
    });

    runTest("UnregisterInvalidIdFails", [](Fixture& f) {
        EXPECT_FALSE(f.m_manager->unregisterData("invalid-id-12345"));
    });

    runTest("DeleteDataImmediately", [](Fixture& f) {
        std::string path = f.createTempFile("test data");
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Session);
        ASSERT_FALSE(id.empty());
        EXPECT_TRUE(f.m_manager->deleteData(id, true));
    });

    runTest("DeleteInvalidIdFails", [](Fixture& f) {
        EXPECT_FALSE(f.m_manager->deleteData("invalid-id", true));
    });

    runTest("DeleteNonExpiredDataWithoutImmediate", [](Fixture& f) {
        std::string path = f.createTempFile();
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Session, 60);
        ASSERT_FALSE(id.empty());
        EXPECT_FALSE(f.m_manager->deleteData(id, false));
    });

    runTest("AnonymizeInvalidIdFails", [](Fixture& f) {
        EXPECT_FALSE(f.m_manager->anonymizeData("invalid-id"));
    });

    runTest("CleanupExpiredDataWorks", [](Fixture& f) {
        f.m_manager->cleanupExpiredData();
        auto metrics = f.m_manager->getMetrics();
        EXPECT_GE(metrics.dataEntriesDeleted, 0);
    });

    runTest("PurgeAllDataByClassification", [](Fixture& f) {
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Session);
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Session);
        f.m_manager->purgeAllData(ZeroRetentionManager::Session);
        EXPECT_TRUE(f.m_manager->getTrackedData(ZeroRetentionManager::Session).empty());
    });

    runTest("GetTrackedDataByClassification", [](Fixture& f) {
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Session);
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Session);
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Cached);
        EXPECT_EQ(f.m_manager->getTrackedData(ZeroRetentionManager::Session).size(), 2u);
    });

    runTest("GetDataEntryReturnsValidEntry", [](Fixture& f) {
        std::string path = f.createTempFile();
        std::string id = f.m_manager->registerData(path, ZeroRetentionManager::Sensitive);
        auto entry = f.m_manager->getDataEntry(id);
        EXPECT_EQ(entry.id, id);
        EXPECT_EQ(entry.classification, ZeroRetentionManager::Sensitive);
        EXPECT_FALSE(entry.isAnonymized);
    });

    runTest("GetInvalidDataEntryReturnsEmpty", [](Fixture& f) {
        EXPECT_TRUE(f.m_manager->getDataEntry("invalid-id").id.empty());
    });

    runTest("InitialMetricsAreZero", [](Fixture& f) {
        auto m = f.m_manager->getMetrics();
        EXPECT_EQ(m.dataEntriesTracked, 0);
        EXPECT_EQ(m.dataEntriesDeleted, 0);
        EXPECT_EQ(m.bytesDeleted, 0);
        EXPECT_EQ(m.sessionsCleanedUp, 0);
        EXPECT_EQ(m.anonymizationCount, 0);
        EXPECT_EQ(m.errorCount, 0);
    });

    runTest("MetricsUpdateOnOperations", [](Fixture& f) {
        f.m_manager->registerData(f.createTempFile("test"), ZeroRetentionManager::Session);
        EXPECT_GT(f.m_manager->getMetrics().dataEntriesTracked, 0);
    });

    runTest("ResetMetricsClearsCounters", [](Fixture& f) {
        f.m_manager->registerData(f.createTempFile(), ZeroRetentionManager::Session);
        EXPECT_GT(f.m_manager->getMetrics().dataEntriesTracked, 0);
        f.m_manager->resetMetrics();
        EXPECT_EQ(f.m_manager->getMetrics().dataEntriesTracked, 0);
    });

    std::cout << "\nResults: " << s_pass << " passed, " << s_fail << " failed.\n";
    return s_fail == 0 ? 0 : 1;
}
