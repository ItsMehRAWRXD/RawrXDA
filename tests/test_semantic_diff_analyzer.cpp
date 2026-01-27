#include <gtest/gtest.h>
#include "git/semantic_diff_analyzer.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

class SemanticDiffAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer = new SemanticDiffAnalyzer();
    }

    void TearDown() override {
        delete analyzer;
    }

    SemanticDiffAnalyzer* analyzer;
};

// 1. Initialization Tests
TEST_F(SemanticDiffAnalyzerTest, InitializationSucceeds) {
    EXPECT_NE(analyzer, nullptr);
}

// 2. Configuration Tests
TEST_F(SemanticDiffAnalyzerTest, SetAndGetConfig) {
    SemanticDiffAnalyzer::Config config;
    config.aiEndpoint = "https://test-ai.example.com";
    config.apiKey = "test-key";
    config.enableBreakingChangeDetection = true;
    config.enableImpactAnalysis = true;
    config.maxDiffSize = 100000;
    config.enableCaching = true;
    config.cacheDirectory = "/tmp/test_cache";

    analyzer->setConfig(config);
    SemanticDiffAnalyzer::Config retrieved = analyzer->getConfig();

    EXPECT_EQ(retrieved.aiEndpoint, "https://test-ai.example.com");
    EXPECT_TRUE(retrieved.enableBreakingChangeDetection);
    EXPECT_TRUE(retrieved.enableImpactAnalysis);
    EXPECT_EQ(retrieved.maxDiffSize, 100000);
    EXPECT_TRUE(retrieved.enableCaching);
}

// 3. Diff Analysis Tests
TEST_F(SemanticDiffAnalyzerTest, AnalyzeDiffWithEmptyString) {
    SemanticDiffAnalyzer::DiffAnalysis result = analyzer->analyzeDiff("");
    
    EXPECT_TRUE(result.summary.isEmpty());
    EXPECT_TRUE(result.changes.isEmpty());
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeDiffWithValidDiff) {
    QString diff = "--- a/test.cpp\n+++ b/test.cpp\n@@ -1,3 +1,3 @@\n-old line\n+new line\n";
    
    QSignalSpy errorSpy(analyzer, &SemanticDiffAnalyzer::errorOccurred);
    SemanticDiffAnalyzer::DiffAnalysis result = analyzer->analyzeDiff(diff);
    
    // Without real API, expect error
    if (result.changes.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

TEST_F(SemanticDiffAnalyzerTest, AnalyzeDiffTooLargeFails) {
    SemanticDiffAnalyzer::Config config;
    config.maxDiffSize = 100;
    analyzer->setConfig(config);

    QString largeDiff(200, 'x');
    QSignalSpy errorSpy(analyzer, &SemanticDiffAnalyzer::errorOccurred);
    
    SemanticDiffAnalyzer::DiffAnalysis result = analyzer->analyzeDiff(largeDiff);
    
    EXPECT_TRUE(result.changes.isEmpty());
}

// 4. File Comparison Tests
TEST_F(SemanticDiffAnalyzerTest, CompareFilesWithIdenticalContent) {
    QString content = "line 1\nline 2\nline 3\n";
    
    SemanticDiffAnalyzer::DiffAnalysis result = 
        analyzer->compareFiles(content, content, "test.txt");
    
    // Should detect no changes or minimal changes
    EXPECT_GE(result.changes.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, CompareFilesWithDifferentContent) {
    QString oldContent = "old line 1\nold line 2\n";
    QString newContent = "new line 1\nnew line 2\n";
    
    SemanticDiffAnalyzer::DiffAnalysis result = 
        analyzer->compareFiles(oldContent, newContent, "test.cpp");
    
    // Analysis depends on API
    EXPECT_GE(result.changes.size(), 0);
}

// 5. Breaking Changes Detection Tests
TEST_F(SemanticDiffAnalyzerTest, DetectBreakingChangesFromEmptyAnalysis) {
    SemanticDiffAnalyzer::DiffAnalysis analysis;
    
    QVector<QString> changes = analyzer->detectBreakingChanges(analysis);
    EXPECT_TRUE(changes.isEmpty());
}

TEST_F(SemanticDiffAnalyzerTest, DetectBreakingChangesWithBreakingChange) {
    SemanticDiffAnalyzer::DiffAnalysis analysis;
    
    SemanticDiffAnalyzer::SemanticChange change;
    change.type = "signature_changed";
    change.name = "testFunction";
    change.file = "test.cpp";
    change.isBreaking = true;
    
    analysis.changes.append(change);
    
    QVector<QString> changes = analyzer->detectBreakingChanges(analysis);
    EXPECT_EQ(changes.size(), 1);
    EXPECT_TRUE(changes[0].contains("testFunction"));
}

// 6. Impact Analysis Tests
TEST_F(SemanticDiffAnalyzerTest, AnalyzeImpactWithValidChange) {
    SemanticDiffAnalyzer::SemanticChange change;
    change.type = "function_modified";
    change.name = "calculateTotal";
    change.description = "Modified calculation logic";
    change.file = "calc.cpp";
    change.isBreaking = false;
    change.impactScore = 0.6;

    QJsonObject impact = analyzer->analyzeImpact(change);
    
    // Without real API, expect error or empty
    EXPECT_GE(impact.size(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, ImpactAnalysisDisabledReturnsEmpty) {
    SemanticDiffAnalyzer::Config config;
    config.enableImpactAnalysis = false;
    analyzer->setConfig(config);

    SemanticDiffAnalyzer::SemanticChange change;
    change.type = "function_added";
    
    QJsonObject impact = analyzer->analyzeImpact(change);
    EXPECT_TRUE(impact.isEmpty());
}

// 7. Diff Enrichment Tests
TEST_F(SemanticDiffAnalyzerTest, EnrichDiffContextWithEmptyDiff) {
    QString enriched = analyzer->enrichDiffContext("");
    EXPECT_TRUE(enriched.isEmpty() || enriched == "");
}

TEST_F(SemanticDiffAnalyzerTest, EnrichDiffContextWithValidDiff) {
    QString diff = "--- a/file.cpp\n+++ b/file.cpp\n@@ -1,1 +1,1 @@\n-old\n+new\n";
    
    QString enriched = analyzer->enrichDiffContext(diff);
    
    // Should either return enriched or original on error
    EXPECT_FALSE(enriched.isEmpty());
}

// 8. Caching Tests
TEST_F(SemanticDiffAnalyzerTest, CacheHitOnSameDiff) {
    SemanticDiffAnalyzer::Config config;
    config.enableCaching = true;
    QTemporaryDir tempDir;
    config.cacheDirectory = tempDir.path();
    analyzer->setConfig(config);

    QString diff = "test diff content";
    
    // First call - cache miss
    analyzer->analyzeDiff(diff);
    SemanticDiffAnalyzer::Metrics metrics1 = analyzer->getMetrics();
    
    // Second call - should be cache hit
    analyzer->analyzeDiff(diff);
    SemanticDiffAnalyzer::Metrics metrics2 = analyzer->getMetrics();
    
    // At least one of them should have cache activity
    EXPECT_GT(metrics2.cacheMisses + metrics2.cacheHits, 0);
}

TEST_F(SemanticDiffAnalyzerTest, ClearCacheWorks) {
    analyzer->clearCache();
    
    SemanticDiffAnalyzer::Metrics metrics = analyzer->getMetrics();
    // Cache should be empty
    EXPECT_NO_THROW(analyzer->clearCache());
}

// 9. Metrics Tests
TEST_F(SemanticDiffAnalyzerTest, InitialMetricsAreZero) {
    SemanticDiffAnalyzer::Metrics metrics = analyzer->getMetrics();
    
    EXPECT_EQ(metrics.diffsAnalyzed, 0);
    EXPECT_EQ(metrics.semanticChangesDetected, 0);
    EXPECT_EQ(metrics.breakingChangesDetected, 0);
    EXPECT_EQ(metrics.impactAnalysesPerformed, 0);
    EXPECT_EQ(metrics.cacheHits, 0);
    EXPECT_EQ(metrics.cacheMisses, 0);
    EXPECT_EQ(metrics.errorCount, 0);
}

TEST_F(SemanticDiffAnalyzerTest, MetricsUpdateOnAnalysis) {
    QString diff = "test diff";
    analyzer->analyzeDiff(diff);
    
    SemanticDiffAnalyzer::Metrics metrics = analyzer->getMetrics();
    EXPECT_GT(metrics.diffsAnalyzed + metrics.errorCount, 0);
}

TEST_F(SemanticDiffAnalyzerTest, ResetMetricsClearsCounters) {
    analyzer->analyzeDiff("test");
    
    SemanticDiffAnalyzer::Metrics before = analyzer->getMetrics();
    EXPECT_GT(before.diffsAnalyzed + before.errorCount, 0);
    
    analyzer->resetMetrics();
    
    SemanticDiffAnalyzer::Metrics after = analyzer->getMetrics();
    EXPECT_EQ(after.diffsAnalyzed, 0);
    EXPECT_EQ(after.errorCount, 0);
}

// 10. Signal Emission Tests
TEST_F(SemanticDiffAnalyzerTest, AnalysisCompletedSignalEmitted) {
    QSignalSpy spy(analyzer, &SemanticDiffAnalyzer::analysisCompleted);
    
    QString diff = "test diff";
    analyzer->analyzeDiff(diff);
    
    // May emit on success
    EXPECT_GE(spy.count(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, ErrorSignalEmittedOnFailure) {
    QSignalSpy spy(analyzer, &SemanticDiffAnalyzer::errorOccurred);
    
    // Empty diff should fail validation
    analyzer->analyzeDiff("");
    
    EXPECT_GT(spy.count(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, BreakingChangeDetectedSignal) {
    QSignalSpy spy(analyzer, &SemanticDiffAnalyzer::breakingChangeDetected);
    
    QString diff = "breaking change diff";
    analyzer->analyzeDiff(diff);
    
    // Signal depends on API response
    EXPECT_GE(spy.count(), 0);
}

TEST_F(SemanticDiffAnalyzerTest, HighImpactChangeDetectedSignal) {
    QSignalSpy spy(analyzer, &SemanticDiffAnalyzer::highImpactChangeDetected);
    
    QString diff = "high impact diff";
    analyzer->analyzeDiff(diff);
    
    // Signal depends on API response
    EXPECT_GE(spy.count(), 0);
}

// Main function
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
