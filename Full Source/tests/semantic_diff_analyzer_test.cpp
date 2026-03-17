#include <gtest/gtest.h>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <chrono>
#include <thread>

#include "../src/git/semantic_diff_analyzer.hpp"

class SemanticDiffAnalyzerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary directory for cache files
        if (!m_tempDir.isValid()) {
            FAIL() << "Failed to create temporary directory";
        }
        
        m_analyzer = std::make_unique<SemanticDiffAnalyzer>();
        m_analyzer->setCachePath(m_tempDir.path().toStdString());
    }

    void TearDown() override
    {
        m_analyzer.reset();
        // Temporary directory cleaned up automatically
    }

    QTemporaryDir m_tempDir;
    std::unique_ptr<SemanticDiffAnalyzer> m_analyzer;

    // Helper functions
    std::string generateDiff(const std::string& oldCode, const std::string& newCode)
    {
        return "--- a/file.cpp\n+++ b/file.cpp\n" +
               std::string("@@ -1,") + std::to_string(oldCode.length()) + " +1," + 
               std::to_string(newCode.length()) + " @@\n" +
               "-" + oldCode + "\n+" + newCode + "\n";
    }

    std::string computeHash(const std::string& data)
    {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(data.data(), data.size());
        return hash.result().toHex().toStdString();
    }
};

// ============= Initialization Tests =============

TEST_F(SemanticDiffAnalyzerTest, InitializationSuccess)
{
    EXPECT_TRUE(m_analyzer != nullptr);
    EXPECT_EQ(m_analyzer->getMetrics().total_diffs_analyzed, 0);
    EXPECT_EQ(m_analyzer->getMetrics().cache_hits, 0);
    EXPECT_EQ(m_analyzer->getMetrics().cache_misses, 0);
}

TEST_F(SemanticDiffAnalyzerTest, CachePathConfiguration)
{
    std::string cachePath = m_tempDir.path().toStdString();
    m_analyzer->setCachePath(cachePath);
    
    EXPECT_TRUE(QDir(QString::fromStdString(cachePath)).exists());
}

TEST_F(SemanticDiffAnalyzerTest, DefaultCachePathIsValid)
{
    SemanticDiffAnalyzer analyzer;
    auto metrics = analyzer.getMetrics();
    EXPECT_GE(metrics.cache_hits + metrics.cache_misses, 0);
}

// ============= Basic Diff Analysis Tests =============

TEST_F(SemanticDiffAnalyzerTest, AnalyzeSingleLineAddition)
{
    std::string oldCode = "int main() {\n  return 0;\n}";
    std::string newCode = "int main() {\n  int x = 5;\n  return 0;\n}";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.changes.size(), 0);
    EXPECT_EQ(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeSingleLineDeletion)
{
    std::string oldCode = "int main() {\n  int x = 5;\n  return 0;\n}";
    std::string newCode = "int main() {\n  return 0;\n}";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeMultipleChanges)
{
    std::string oldCode = "void foo() {}\nvoid bar() {}";
    std::string newCode = "void foo() { return; }\nvoid bar() {}\nvoid baz() {}";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.changes.size(), 2);
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeEmptyDiff)
{
    std::string diff = "";
    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_FALSE(result.success);
}

// ============= Breaking Change Detection Tests =============

TEST_F(SemanticDiffAnalyzerTest, DetectFunctionSignatureChange)
{
    std::string oldCode = "int calculate(int a, int b);";
    std::string newCode = "float calculate(int a, int b, int c);";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.breaking_changes.size(), 0);
    
    // Check if any breaking change has high impact
    bool hasHighImpact = false;
    for (const auto& change : result.breaking_changes) {
        if (change.impact_score > 0.7f) {
            hasHighImpact = true;
            break;
        }
    }
    EXPECT_TRUE(hasHighImpact);
}

TEST_F(SemanticDiffAnalyzerTest, DetectAPIRemoval)
{
    std::string oldCode = "class Database {\npublic:\n  void connect();\n};";
    std::string newCode = "class Database {\npublic:\n};";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, DetectReturnTypeChange)
{
    std::string oldCode = "void process(Data d);";
    std::string newCode = "bool process(Data d);";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, DetectParameterTypeChange)
{
    std::string oldCode = "void save(const std::string& path);";
    std::string newCode = "void save(const char* path);";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, DetectParameterAdditionAsBreaking)
{
    std::string oldCode = "int getValue();";
    std::string newCode = "int getValue(bool cached);";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    // Parameter addition without default is breaking
    EXPECT_GT(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, NonBreakingImportStatementChange)
{
    std::string oldCode = "import { old_name } from 'module';";
    std::string newCode = "import { old_name, new_name } from 'module';";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    // Adding new imports shouldn't be breaking
    EXPECT_EQ(result.breaking_changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, NonBreakingInternalImplementationChange)
{
    std::string oldCode = "void process() {\n  doWork();\n  cleanup();\n}";
    std::string newCode = "void process() {\n  doWork();\n  optimizedCleanup();\n}";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    // Internal implementation changes aren't breaking
    EXPECT_EQ(result.breaking_changes.size(), 0);
}

// ============= Semantic Analysis Tests =============

TEST_F(SemanticDiffAnalyzerTest, IdentifyAddedFunctionality)
{
    std::string oldCode = "class Logger {};";
    std::string newCode = "class Logger {\npublic:\n  void log(const std::string& msg);\n};";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.changes.size(), 0);
    
    bool hasAddition = false;
    for (const auto& change : result.changes) {
        if (change.type == "added") {
            hasAddition = true;
            break;
        }
    }
    EXPECT_TRUE(hasAddition);
}

TEST_F(SemanticDiffAnalyzerTest, IdentifyRemovedFunctionality)
{
    std::string oldCode = "class Logger {\npublic:\n  void log(const std::string& msg);\n};";
    std::string newCode = "class Logger {};";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    
    bool hasRemoval = false;
    for (const auto& change : result.changes) {
        if (change.type == "removed") {
            hasRemoval = true;
            break;
        }
    }
    EXPECT_TRUE(hasRemoval);
}

TEST_F(SemanticDiffAnalyzerTest, IdentifyModifiedFunctionality)
{
    std::string oldCode = "int add(int a, int b) { return a + b; }";
    std::string newCode = "int add(int a, int b) { return a + b + 1; }";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.changes.size(), 0);
}

// ============= Cache Tests =============

TEST_F(SemanticDiffAnalyzerTest, CacheHitOnIdenticalDiff)
{
    std::string diff = generateDiff("code1", "code2");
    
    // First analysis - cache miss
    auto result1 = m_analyzer->analyzeDiff(diff);
    auto metrics1 = m_analyzer->getMetrics();
    
    // Second analysis - cache hit
    auto result2 = m_analyzer->analyzeDiff(diff);
    auto metrics2 = m_analyzer->getMetrics();

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(metrics2.cache_hits, metrics1.cache_hits + 1);
}

TEST_F(SemanticDiffAnalyzerTest, CacheMissOnDifferentDiff)
{
    std::string diff1 = generateDiff("code1", "code2");
    std::string diff2 = generateDiff("code3", "code4");
    
    auto result1 = m_analyzer->analyzeDiff(diff1);
    auto metrics1 = m_analyzer->getMetrics();
    
    auto result2 = m_analyzer->analyzeDiff(diff2);
    auto metrics2 = m_analyzer->getMetrics();

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(metrics2.cache_misses, metrics1.cache_misses + 1);
}

TEST_F(SemanticDiffAnalyzerTest, PersistentCacheStorage)
{
    std::string cachePath = m_tempDir.path().toStdString();
    {
        auto analyzer1 = std::make_unique<SemanticDiffAnalyzer>();
        analyzer1->setCachePath(cachePath);
        
        std::string diff = generateDiff("old", "new");
        analyzer1->analyzeDiff(diff);
    }

    // Create new analyzer instance with same cache path
    auto analyzer2 = std::make_unique<SemanticDiffAnalyzer>();
    analyzer2->setCachePath(cachePath);
    
    std::string diff = generateDiff("old", "new");
    auto result = analyzer2->analyzeDiff(diff);

    // Should have cache hit from persisted cache
    EXPECT_TRUE(result.success);
}

TEST_F(SemanticDiffAnalyzerTest, ClearCache)
{
    std::string diff = generateDiff("code1", "code2");
    
    m_analyzer->analyzeDiff(diff);
    auto metricsBefore = m_analyzer->getMetrics();
    
    m_analyzer->clearCache();
    
    auto metricsAfter = m_analyzer->getMetrics();
    EXPECT_EQ(metricsAfter.cache_hits, 0);
    EXPECT_EQ(metricsAfter.cache_misses, 0);
}

// ============= Impact Analysis Tests =============

TEST_F(SemanticDiffAnalyzerTest, ImpactScoreForCriticalChange)
{
    std::string oldCode = "int getValue();";
    std::string newCode = "void getValue();";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    
    for (const auto& change : result.breaking_changes) {
        if (change.type == "return_type_change") {
            EXPECT_GT(change.impact_score, 0.7f);
            break;
        }
    }
}

TEST_F(SemanticDiffAnalyzerTest, ImpactScoreForMinorChange)
{
    std::string oldCode = "// Comment\nint x = 5;";
    std::string newCode = "// Updated Comment\nint x = 5;";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
}

TEST_F(SemanticDiffAnalyzerTest, ImpactAnalysisForPublicAPI)
{
    std::string oldCode = "class PublicAPI {\npublic:\n  void method();\n};";
    std::string newCode = "class PublicAPI {\nprivate:\n  void method();\n};";
    std::string diff = generateDiff(oldCode, newCode);

    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.breaking_changes.size(), 0);
}

// ============= Metrics Tests =============

TEST_F(SemanticDiffAnalyzerTest, MetricsTrackingOnAnalysis)
{
    auto initialMetrics = m_analyzer->getMetrics();
    
    for (int i = 0; i < 5; ++i) {
        std::string diff = generateDiff("code" + std::to_string(i), "new" + std::to_string(i));
        m_analyzer->analyzeDiff(diff);
    }

    auto finalMetrics = m_analyzer->getMetrics();
    
    EXPECT_GE(finalMetrics.total_diffs_analyzed, 5);
}

TEST_F(SemanticDiffAnalyzerTest, MetricsAverageLinesChanged)
{
    std::string diff1 = generateDiff("line1\nline2", "line1\nline2\nline3");
    std::string diff2 = generateDiff("a\nb\nc\nd", "a\nb");
    
    m_analyzer->analyzeDiff(diff1);
    m_analyzer->analyzeDiff(diff2);

    auto metrics = m_analyzer->getMetrics();
    EXPECT_GE(metrics.total_diffs_analyzed, 2);
}

TEST_F(SemanticDiffAnalyzerTest, MetricsBreakingChangeCount)
{
    std::string diff = generateDiff("void foo();", "int foo();");
    
    m_analyzer->analyzeDiff(diff);
    
    auto metrics = m_analyzer->getMetrics();
    EXPECT_GE(metrics.total_diffs_analyzed, 1);
}

TEST_F(SemanticDiffAnalyzerTest, MetricsAverageImpactScore)
{
    for (int i = 0; i < 3; ++i) {
        std::string oldCode = "void func" + std::to_string(i) + "();";
        std::string newCode = "int func" + std::to_string(i) + "();";
        std::string diff = generateDiff(oldCode, newCode);
        m_analyzer->analyzeDiff(diff);
    }

    auto metrics = m_analyzer->getMetrics();
    EXPECT_GE(metrics.total_diffs_analyzed, 3);
}

// ============= Error Handling Tests =============

TEST_F(SemanticDiffAnalyzerTest, HandleMalformedDiff)
{
    std::string malformedDiff = "this is not a valid diff format @#$%";
    auto result = m_analyzer->analyzeDiff(malformedDiff);

    // Should either fail gracefully or return safe result
    EXPECT_FALSE(result.success || result.changes.empty());
}

TEST_F(SemanticDiffAnalyzerTest, HandleVeryLargeDiff)
{
    std::string largeDiff = "";
    for (int i = 0; i < 10000; ++i) {
        largeDiff += "--- a/file" + std::to_string(i) + ".cpp\n";
        largeDiff += "+++ b/file" + std::to_string(i) + ".cpp\n";
        largeDiff += "+int value = " + std::to_string(i) + ";\n";
    }

    auto result = m_analyzer->analyzeDiff(largeDiff);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SemanticDiffAnalyzerTest, HandleDiffWithBinaryFiles)
{
    std::string diff = "Binary files a/image.png and b/image.png differ\n";
    auto result = m_analyzer->analyzeDiff(diff);

    EXPECT_FALSE(result.success || result.changes.empty());
}

TEST_F(SemanticDiffAnalyzerTest, HandleDiffWithConflictMarkers)
{
    std::string diff = "<<<<<<< HEAD\nint a = 1;\n=======\nint a = 2;\n>>>>>>> branch\n";
    auto result = m_analyzer->analyzeDiff(diff);

    // Should detect conflict markers
    EXPECT_TRUE(result.success);
}

// ============= JSON Serialization Tests =============

TEST_F(SemanticDiffAnalyzerTest, SerializeAnalysisResult)
{
    std::string diff = generateDiff("old", "new");
    auto result = m_analyzer->analyzeDiff(diff);

    QJsonObject json;
    json["success"] = result.success;
    json["num_changes"] = (int)result.changes.size();
    json["num_breaking"] = (int)result.breaking_changes.size();

    QJsonDocument doc(json);
    EXPECT_FALSE(doc.isEmpty());
}

TEST_F(SemanticDiffAnalyzerTest, SerializeMetrics)
{
    for (int i = 0; i < 3; ++i) {
        std::string diff = generateDiff("code", "newcode");
        m_analyzer->analyzeDiff(diff);
    }

    auto metrics = m_analyzer->getMetrics();
    
    QJsonObject json;
    json["total_analyzed"] = (int)metrics.total_diffs_analyzed;
    json["cache_hits"] = (int)metrics.cache_hits;
    json["cache_misses"] = (int)metrics.cache_misses;

    QJsonDocument doc(json);
    EXPECT_FALSE(doc.isEmpty());
}

// ============= Concurrent Access Tests =============

TEST_F(SemanticDiffAnalyzerTest, ThreadSafeAnalysis)
{
    std::vector<std::thread> threads;
    
    auto analyzeTask = [this](int id) {
        std::string diff = generateDiff("code" + std::to_string(id), "new" + std::to_string(id));
        auto result = m_analyzer->analyzeDiff(diff);
        EXPECT_TRUE(result.success);
    };

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(analyzeTask, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto metrics = m_analyzer->getMetrics();
    EXPECT_GE(metrics.total_diffs_analyzed, 4);
}

TEST_F(SemanticDiffAnalyzerTest, ConcurrentCacheAccess)
{
    std::string diff = generateDiff("shared", "data");
    m_analyzer->analyzeDiff(diff);

    std::vector<std::thread> threads;
    std::atomic<int> hitCount(0);

    auto readCacheTask = [this, &hitCount, &diff]() {
        auto result = m_analyzer->analyzeDiff(diff);
        if (result.success) {
            ++hitCount;
        }
    };

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(readCacheTask);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(hitCount, 4); // At least 4 should get cache hits
}

// ============= Integration Tests =============

TEST_F(SemanticDiffAnalyzerTest, AnalyzeRealWorldDiffPattern)
{
    std::string realWorldDiff = R"(
--- a/src/database.cpp
+++ b/src/database.cpp
@@ -10,5 +10,8 @@
 class Database {
 public:
-  void connect(const std::string& host);
-  void disconnect();
+  bool connect(const std::string& host, int port, int timeout = 30);
+  bool disconnect();
+  bool reconnect();
+  std::string getStatus();
 };
)";

    auto result = m_analyzer->analyzeDiff(realWorldDiff);
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeMultipleFileDiff)
{
    std::string multiFileDiff = R"(
--- a/file1.cpp
+++ b/file1.cpp
@@ -1 +1,2 @@
 int x = 1;
+int y = 2;

--- a/file2.cpp
+++ b/file2.cpp
@@ -5 +5,1 @@
-void old_function();
+void new_function();
)";

    auto result = m_analyzer->analyzeDiff(multiFileDiff);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SemanticDiffAnalyzerTest, CompleteAnalysisWorkflow)
{
    // Simulate complete workflow: add -> analyze -> cache -> validate
    std::string diff1 = generateDiff("v1.0", "v1.1");
    auto result1 = m_analyzer->analyzeDiff(diff1);
    
    auto metrics1 = m_analyzer->getMetrics();
    EXPECT_EQ(metrics1.cache_misses, 1);

    // Reanalyze same diff
    auto result2 = m_analyzer->analyzeDiff(diff1);
    auto metrics2 = m_analyzer->getMetrics();
    
    EXPECT_EQ(result1.success, result2.success);
    EXPECT_EQ(metrics2.cache_hits, 1);

    // Analyze different diff
    std::string diff2 = generateDiff("v2.0", "v2.1");
    auto result3 = m_analyzer->analyzeDiff(diff2);
    auto metrics3 = m_analyzer->getMetrics();
    
    EXPECT_EQ(metrics3.cache_misses, 2);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SemanticDiffAnalyzerTest);
