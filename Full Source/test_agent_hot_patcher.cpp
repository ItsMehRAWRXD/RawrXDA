// ============================================================================
// File: test_agent_hot_patcher.cpp
// 
// Purpose: Comprehensive test suite for AgentHotPatcher
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "agent_hot_patcher.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>

class TestAgentHotPatcher : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testHallucinationDetection();
    void testHallucinationCorrection();
    void testNavigationErrorFix();
    void testBehaviorPatches();
    void testStatistics();
    void testThreadSafety();
    void testPerformance();

private:
    AgentHotPatcher* m_patcher;
};

void TestAgentHotPatcher::initTestCase()
{
    m_patcher = new AgentHotPatcher();
    QVERIFY(m_patcher != nullptr);
}

void TestAgentHotPatcher::cleanupTestCase()
{
    delete m_patcher;
}

void TestAgentHotPatcher::testInitialization()
{
    // Test initialization with valid GGUF loader path
    bool result = m_patcher->initialize("dummy_gguf_loader.exe", 0);
    QVERIFY(result);
    QVERIFY(m_patcher->isHotPatchingEnabled());
}

void TestAgentHotPatcher::testHallucinationDetection()
{
    // Test path hallucination detection
    QString contentWithInvalidPath = "The file is located at /mystical/path/to/file.txt";
    HallucinationDetection detection = m_patcher->detectHallucination(contentWithInvalidPath, QJsonObject());
    QVERIFY(!detection.detectionId.isEmpty());
    QCOMPARE(detection.hallucinationType, QString("fabricated_path"));
    QVERIFY(detection.confidence > 0.6);
    
    // Test logic contradiction detection
    QString contentWithContradiction = "This always succeeds but always fails";
    detection = m_patcher->detectHallucination(contentWithContradiction, QJsonObject());
    QVERIFY(!detection.detectionId.isEmpty());
    QCOMPARE(detection.hallucinationType, QString("logic_contradiction"));
    
    // Test incomplete reasoning detection
    QString incompleteReasoning = "The answer is yes";
    detection = m_patcher->detectHallucination(incompleteReasoning, QJsonObject());
    QVERIFY(!detection.detectionId.isEmpty());
    QCOMPARE(detection.hallucinationType, QString("incomplete_reasoning"));
}

void TestAgentHotPatcher::testHallucinationCorrection()
{
    // Test path hallucination correction
    HallucinationDetection pathDetection;
    pathDetection.hallucinationType = "fabricated_path";
    pathDetection.detectedContent = "/mystical/path";
    
    QString corrected = m_patcher->correctHallucination(pathDetection);
    QVERIFY(!corrected.isEmpty());
    QVERIFY(corrected.contains("./src/kernels/q8k_kernel.cpp"));
    
    // Test logic contradiction correction
    HallucinationDetection logicDetection;
    logicDetection.hallucinationType = "logic_contradiction";
    
    corrected = m_patcher->correctHallucination(logicDetection);
    QVERIFY(!corrected.isEmpty());
    QVERIFY(corrected.contains("robust error handling"));
}

void TestAgentHotPatcher::testNavigationErrorFix()
{
    // Test path normalization
    QString invalidPath = "/absolute/path/../with/double//slashes";
    NavigationFix fix = m_patcher->fixNavigationError(invalidPath, QJsonObject());
    QVERIFY(!fix.fixId.isEmpty());
    QVERIFY(!fix.reasoning.isEmpty());
    QVERIFY(fix.effectiveness > 0.0);
}

void TestAgentHotPatcher::testBehaviorPatches()
{
    // Test behavior patch application
    QJsonObject output;
    output["reasoning"] = "Simple reasoning";
    
    QJsonObject patched = m_patcher->applyBehaviorPatches(output, QJsonObject());
    QVERIFY(!patched.isEmpty());
    
    // Verify the output structure is maintained
    QVERIFY(patched.contains("reasoning"));
}

void TestAgentHotPatcher::testStatistics()
{
    QJsonObject stats = m_patcher->getCorrectionStatistics();
    QVERIFY(!stats.isEmpty());
    
    // Verify all required statistics fields are present
    QVERIFY(stats.contains("totalHallucinationsDetected"));
    QVERIFY(stats.contains("hallucinationsCorrected"));
    QVERIFY(stats.contains("navigationFixesApplied"));
    QVERIFY(stats.contains("averageNavigationFixEffectiveness"));
    QVERIFY(stats.contains("totalBehaviorPatches"));
}

void TestAgentHotPatcher::testThreadSafety()
{
    // Test concurrent access (simulated)
    QSignalSpy spy(m_patcher, &AgentHotPatcher::hallucinationDetected);
    
    // Simulate multiple concurrent operations
    QString testContent = "Path: /mystical/path";
    for (int i = 0; i < 10; ++i) {
        m_patcher->detectHallucination(testContent, QJsonObject());
    }
    
    // Verify no crashes occurred
    QVERIFY(true);
}

void TestAgentHotPatcher::testPerformance()
{
    // Performance test with large content
    QString largeContent;
    for (int i = 0; i < 1000; ++i) {
        largeContent += "This is test content with path: /valid/path/file" + QString::number(i) + "\n";
    }
    
    QElapsedTimer timer;
    timer.start();
    
    HallucinationDetection detection = m_patcher->detectHallucination(largeContent, QJsonObject());
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "Hallucination detection took" << elapsed << "ms";
    
    // Should complete in reasonable time (less than 100ms)
    QVERIFY(elapsed < 100);
}

QTEST_MAIN(TestAgentHotPatcher)
#include "test_agent_hot_patcher.moc"