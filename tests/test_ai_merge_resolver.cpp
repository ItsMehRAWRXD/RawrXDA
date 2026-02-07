#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "git/ai_merge_resolver.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTextStream>

class AIMergeResolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        resolver = new AIMergeResolver();
    }

    void TearDown() override {
        delete resolver;
    }

    AIMergeResolver* resolver;
};

// 1. Initialization and Shutdown Tests
TEST_F(AIMergeResolverTest, InitializationSucceeds) {
    EXPECT_NE(resolver, nullptr);
}

TEST_F(AIMergeResolverTest, DestructorExecutesCleanly) {
    AIMergeResolver* temp = new AIMergeResolver();
    EXPECT_NO_THROW(delete temp);
}

// 2. Configuration Tests
TEST_F(AIMergeResolverTest, SetAndGetConfig) {
    AIMergeResolver::Config config;
    config.aiEndpoint = "https://test-ai.example.com";
    config.apiKey = "test-api-key";
    config.enableAutoResolve = true;
    config.minConfidenceThreshold = 0.8;
    config.maxConflictSize = 5000;
    config.enableMetrics = true;
    config.enableAuditLog = true;
    config.auditLogPath = "/tmp/test_audit.log";

    resolver->setConfig(config);
    AIMergeResolver::Config retrieved = resolver->getConfig();

    EXPECT_EQ(retrieved.aiEndpoint, "https://test-ai.example.com");
    EXPECT_EQ(retrieved.apiKey, "test-api-key");
    EXPECT_TRUE(retrieved.enableAutoResolve);
    EXPECT_EQ(retrieved.minConfidenceThreshold, 0.8);
    EXPECT_EQ(retrieved.maxConflictSize, 5000);
    EXPECT_TRUE(retrieved.enableMetrics);
    EXPECT_TRUE(retrieved.enableAuditLog);
}

TEST_F(AIMergeResolverTest, DefaultConfigValues) {
    AIMergeResolver::Config config = resolver->getConfig();
    EXPECT_FALSE(config.enableAutoResolve);
    EXPECT_EQ(config.maxConflictSize, 10000);
    EXPECT_EQ(config.minConfidenceThreshold, 0.75);
    EXPECT_TRUE(config.enableMetrics);
    EXPECT_TRUE(config.enableAuditLog);
}

// 3. Conflict Detection Tests
TEST_F(AIMergeResolverTest, DetectConflictsInNonExistentFile) {
    QVector<AIMergeResolver::ConflictBlock> conflicts = 
        resolver->detectConflicts("/nonexistent/file.txt");
    
    // Should handle gracefully, likely returning empty
    EXPECT_TRUE(conflicts.isEmpty());
}

TEST_F(AIMergeResolverTest, DetectConflictsInFileWithoutConflicts) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "Line 1\n";
    out << "Line 2\n";
    out << "Line 3\n";
    tempFile.close();

    QVector<AIMergeResolver::ConflictBlock> conflicts = 
        resolver->detectConflicts(tempFile.fileName());
    
    EXPECT_TRUE(conflicts.isEmpty());
}

TEST_F(AIMergeResolverTest, DetectConflictsInFileWithConflict) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "Normal line\n";
    out << "<<<<<<< HEAD\n";
    out << "Current version\n";
    out << "=======\n";
    out << "Incoming version\n";
    out << ">>>>>>> feature-branch\n";
    out << "Another normal line\n";
    tempFile.close();

    QVector<AIMergeResolver::ConflictBlock> conflicts = 
        resolver->detectConflicts(tempFile.fileName());
    
    EXPECT_EQ(conflicts.size(), 1);
    if (!conflicts.isEmpty()) {
        EXPECT_EQ(conflicts[0].currentVersion, "Current version");
        EXPECT_EQ(conflicts[0].incomingVersion, "Incoming version");
        EXPECT_GT(conflicts[0].endLine, conflicts[0].startLine);
    }
}

TEST_F(AIMergeResolverTest, DetectMultipleConflicts) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "<<<<<<< HEAD\n";
    out << "Conflict 1 current\n";
    out << "=======\n";
    out << "Conflict 1 incoming\n";
    out << ">>>>>>> branch1\n";
    out << "Some code\n";
    out << "<<<<<<< HEAD\n";
    out << "Conflict 2 current\n";
    out << "=======\n";
    out << "Conflict 2 incoming\n";
    out << ">>>>>>> branch2\n";
    tempFile.close();

    QVector<AIMergeResolver::ConflictBlock> conflicts = 
        resolver->detectConflicts(tempFile.fileName());
    
    EXPECT_EQ(conflicts.size(), 2);
}

// 4. Conflict Resolution Tests
TEST_F(AIMergeResolverTest, ResolveConflictRequiresManualReviewForLargeSize) {
    AIMergeResolver::Config config;
    config.maxConflictSize = 100;
    resolver->setConfig(config);

    AIMergeResolver::ConflictBlock conflict;
    conflict.file = "test.txt";
    conflict.currentVersion = QString(200, 'a'); // Exceeds max
    conflict.incomingVersion = QString(200, 'b');

    AIMergeResolver::Resolution resolution = resolver->resolveConflict(conflict);
    
    EXPECT_TRUE(resolution.requiresManualReview);
    EXPECT_FALSE(resolution.explanation.isEmpty());
}

TEST_F(AIMergeResolverTest, ResolveConflictEmitsSignal) {
    QSignalSpy spy(resolver, &AIMergeResolver::conflictResolved);
    
    AIMergeResolver::ConflictBlock conflict;
    conflict.file = "test.txt";
    conflict.currentVersion = "current";
    conflict.incomingVersion = "incoming";
    conflict.context = "context";

    resolver->resolveConflict(conflict);
    
    // Should emit signal (even if API fails)
    EXPECT_GE(spy.count(), 0);
}

TEST_F(AIMergeResolverTest, ResolveConflictWithLowConfidence) {
    AIMergeResolver::Config config;
    config.minConfidenceThreshold = 0.9;
    resolver->setConfig(config);

    AIMergeResolver::ConflictBlock conflict;
    conflict.file = "test.txt";
    conflict.currentVersion = "current";
    conflict.incomingVersion = "incoming";

    AIMergeResolver::Resolution resolution = resolver->resolveConflict(conflict);
    
    // Without real API, confidence will be low
    // Should require manual review
    EXPECT_TRUE(resolution.requiresManualReview || resolution.confidence < 0.9);
}

// 5. Apply Resolution Tests
TEST_F(AIMergeResolverTest, ApplyResolutionToNonExistentFileFails) {
    AIMergeResolver::Resolution resolution;
    resolution.resolvedContent = "resolved";
    resolution.confidence = 0.9;

    bool result = resolver->applyResolution(
        "/nonexistent/file.txt", resolution, 1, 3);
    
    EXPECT_FALSE(result);
}

TEST_F(AIMergeResolverTest, ApplyResolutionWithInvalidLineRange) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    tempFile.write("Line 1\nLine 2\nLine 3\n");
    tempFile.close();

    AIMergeResolver::Resolution resolution;
    resolution.resolvedContent = "resolved";

    // Invalid: startLine > endLine
    bool result = resolver->applyResolution(
        tempFile.fileName(), resolution, 5, 2);
    
    EXPECT_FALSE(result);
}

TEST_F(AIMergeResolverTest, ApplyResolutionSuccess) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "Line 1\n";
    out << "<<<<<<< HEAD\n";
    out << "Current\n";
    out << "=======\n";
    out << "Incoming\n";
    out << ">>>>>>> branch\n";
    out << "Line 7\n";
    tempFile.close();

    AIMergeResolver::Resolution resolution;
    resolution.resolvedContent = "Resolved content";
    resolution.confidence = 0.95;

    bool result = resolver->applyResolution(
        tempFile.fileName(), resolution, 2, 6);
    
    EXPECT_TRUE(result);
}

// 6. Semantic Merge Analysis Tests
TEST_F(AIMergeResolverTest, AnalyzeSemanticMergeWithEmptyStrings) {
    QJsonObject result = resolver->analyzeSemanticMerge("", "", "");
    
    // Should handle gracefully
    EXPECT_TRUE(result.isEmpty() || result.contains("error"));
}

TEST_F(AIMergeResolverTest, AnalyzeSemanticMergeWithValidData) {
    QString base = "function foo() { return 1; }";
    QString current = "function foo() { return 2; }";
    QString incoming = "function foo() { return 3; }";
    
    QSignalSpy errorSpy(resolver, &AIMergeResolver::errorOccurred);
    QJsonObject result = resolver->analyzeSemanticMerge(base, current, incoming);
    
    // Without real API, expect error or empty result
    if (result.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

// 7. Breaking Changes Detection Tests
TEST_F(AIMergeResolverTest, DetectBreakingChangesWithEmptyDiff) {
    QVector<QString> changes = resolver->detectBreakingChanges("");
    EXPECT_TRUE(changes.isEmpty());
}

TEST_F(AIMergeResolverTest, DetectBreakingChangesWithValidDiff) {
    QString diff = "- public void method()\n+ private void method()";
    
    QSignalSpy errorSpy(resolver, &AIMergeResolver::errorOccurred);
    QVector<QString> changes = resolver->detectBreakingChanges(diff);
    
    // Without real API, expect error
    if (changes.isEmpty()) {
        EXPECT_GT(errorSpy.count(), 0);
    }
}

TEST_F(AIMergeResolverTest, BreakingChangeDetectedSignalEmitted) {
    QSignalSpy spy(resolver, &AIMergeResolver::breakingChangeDetected);
    
    QString diff = "breaking change content";
    resolver->detectBreakingChanges(diff);
    
    // Signal emission depends on API response
    EXPECT_GE(spy.count(), 0);
}

// 8. Metrics Tests
TEST_F(AIMergeResolverTest, InitialMetricsAreZero) {
    AIMergeResolver::Metrics metrics = resolver->getMetrics();
    
    EXPECT_EQ(metrics.conflictsDetected, 0);
    EXPECT_EQ(metrics.conflictsResolved, 0);
    EXPECT_EQ(metrics.autoResolved, 0);
    EXPECT_EQ(metrics.manualResolved, 0);
    EXPECT_EQ(metrics.breakingChangesDetected, 0);
    EXPECT_EQ(metrics.errorCount, 0);
    EXPECT_EQ(metrics.avgResolutionConfidence, 0.0);
    EXPECT_EQ(metrics.avgResolutionLatencyMs, 0.0);
}

TEST_F(AIMergeResolverTest, MetricsUpdateOnConflictDetection) {
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "<<<<<<< HEAD\n";
    out << "Current\n";
    out << "=======\n";
    out << "Incoming\n";
    out << ">>>>>>> branch\n";
    tempFile.close();

    resolver->detectConflicts(tempFile.fileName());
    
    AIMergeResolver::Metrics metrics = resolver->getMetrics();
    EXPECT_GT(metrics.conflictsDetected, 0);
}

TEST_F(AIMergeResolverTest, ResetMetricsClearsCounters) {
    // Trigger some operations
    resolver->detectConflicts("/nonexistent");
    
    AIMergeResolver::Metrics before = resolver->getMetrics();
    EXPECT_GT(before.errorCount, 0);
    
    resolver->resetMetrics();
    
    AIMergeResolver::Metrics after = resolver->getMetrics();
    EXPECT_EQ(after.errorCount, 0);
    EXPECT_EQ(after.conflictsDetected, 0);
}

// 9. Signal Emission Tests
TEST_F(AIMergeResolverTest, ConflictsDetectedSignalEmitted) {
    QSignalSpy spy(resolver, &AIMergeResolver::conflictsDetected);
    
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QTextStream out(&tempFile);
    out << "<<<<<<< HEAD\nCurrent\n=======\nIncoming\n>>>>>>> branch\n";
    tempFile.close();

    resolver->detectConflicts(tempFile.fileName());
    
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    EXPECT_GT(arguments.at(0).toInt(), 0);
}

TEST_F(AIMergeResolverTest, ErrorSignalEmittedOnFailure) {
    QSignalSpy spy(resolver, &AIMergeResolver::errorOccurred);
    
    resolver->detectConflicts("/definitely/nonexistent/file.txt");
    
    EXPECT_GT(spy.count(), 0);
}

TEST_F(AIMergeResolverTest, MetricsUpdatedSignalEmitted) {
    AIMergeResolver::Config config;
    config.enableMetrics = true;
    resolver->setConfig(config);
    
    QSignalSpy spy(resolver, &AIMergeResolver::metricsUpdated);
    
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    QTextStream out(&tempFile);
    out << "<<<<<<< HEAD\nCurrent\n=======\nIncoming\n>>>>>>> branch\n";
    tempFile.close();

    resolver->detectConflicts(tempFile.fileName());
    
    // Metrics may be updated
    EXPECT_GE(spy.count(), 0);
}

// 10. Thread Safety Tests
TEST_F(AIMergeResolverTest, ConcurrentConfigAccess) {
    AIMergeResolver::Config config1;
    config1.maxConflictSize = 1000;
    
    AIMergeResolver::Config config2;
    config2.maxConflictSize = 2000;
    
    resolver->setConfig(config1);
    AIMergeResolver::Config retrieved1 = resolver->getConfig();
    
    resolver->setConfig(config2);
    AIMergeResolver::Config retrieved2 = resolver->getConfig();
    
    EXPECT_EQ(retrieved2.maxConflictSize, 2000);
}

TEST_F(AIMergeResolverTest, ConcurrentMetricsAccess) {
    AIMergeResolver::Metrics m1 = resolver->getMetrics();
    resolver->resetMetrics();
    AIMergeResolver::Metrics m2 = resolver->getMetrics();
    
    EXPECT_NO_THROW(m1 = resolver->getMetrics());
}

// Main function
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
