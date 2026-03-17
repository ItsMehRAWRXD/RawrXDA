// ============================================================================
// File: test_agent_hot_patcher_qtest.cpp
// 
// Purpose: Qt Test for AgentHotPatcher
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include <QtTest>
#include <QCoreApplication>
#include "agent_hot_patcher.hpp"

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

private:
    AgentHotPatcher* m_patcher;
};

void TestAgentHotPatcher::initTestCase()
{
    m_patcher = new AgentHotPatcher();
    m_patcher->initialize("", 0);
}

void TestAgentHotPatcher::cleanupTestCase()
{
    delete m_patcher;
}

void TestAgentHotPatcher::testInitialization()
{
    QVERIFY(m_patcher != nullptr);
    QVERIFY(m_patcher->isHotPatchingEnabled());
}

void TestAgentHotPatcher::testHallucinationDetection()
{
    // Test refusal detection
    HallucinationDetection detection = m_patcher->detectHallucination("I cannot perform this action", QJsonObject());
    QVERIFY(!detection.detectionId.isEmpty());
    QCOMPARE(detection.hallucinationType, QString("refusal"));
    
    // Test path hallucination detection
    HallucinationDetection pathDetection = m_patcher->detectHallucination("The file is at /nonexistent/path/file.txt", QJsonObject());
    QVERIFY(!pathDetection.detectionId.isEmpty());
    QCOMPARE(pathDetection.hallucinationType, QString("invalid_path"));
}

void TestAgentHotPatcher::testHallucinationCorrection()
{
    HallucinationDetection detection;
    detection.detectionId = "test-id";
    detection.hallucinationType = "refusal";
    detection.correctionStrategy = "bypass_refusal";
    
    QString corrected = m_patcher->correctHallucination(detection);
    QVERIFY(!corrected.isEmpty());
    QCOMPARE(corrected, QString("Proceeding with the requested action."));
}

void TestAgentHotPatcher::testNavigationErrorFix()
{
    NavigationFix fix = m_patcher->fixNavigationError("/invalid/path", QJsonObject());
    QVERIFY(!fix.fixId.isEmpty());
    // The fix might not change the path if no matching pattern is found
    QVERIFY(!fix.reasoning.isEmpty());
}

void TestAgentHotPatcher::testBehaviorPatches()
{
    QString output = "Contact: john.doe@example.com";
    QString patched = m_patcher->applyBehaviorPatches(output);
    // The email should be redacted by the behavior patch
    QVERIFY(patched.contains("user@example.com"));
}

void TestAgentHotPatcher::testStatistics()
{
    QJsonObject stats = m_patcher->getCorrectionStatistics();
    QVERIFY(stats.contains("totalHallucinationsDetected"));
    QVERIFY(stats.contains("hallucinationsCorrected"));
    QVERIFY(stats.contains("navigationFixesApplied"));
}

QTEST_MAIN(TestAgentHotPatcher)
#include "test_agent_hot_patcher_qtest.moc"