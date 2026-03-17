// ============================================================================
// File: test_agent_hot_patcher.cpp
// 
// Purpose: Test application for AgentHotPatcher
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "agent_hot_patcher.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

class TestApplication : public QObject
{
    Q_OBJECT

public:
    TestApplication(QObject* parent = nullptr) : QObject(parent), m_patcher(new AgentHotPatcher(this))
    {
        connect(m_patcher, &AgentHotPatcher::hallucinationDetected, 
                this, &TestApplication::onHallucinationDetected);
        connect(m_patcher, &AgentHotPatcher::hallucinationCorrected, 
                this, &TestApplication::onHallucinationCorrected);
        connect(m_patcher, &AgentHotPatcher::navigationErrorFixed, 
                this, &TestApplication::onNavigationErrorFixed);
        connect(m_patcher, &AgentHotPatcher::behaviorPatchApplied, 
                this, &TestApplication::onBehaviorPatchApplied);
        connect(m_patcher, &AgentHotPatcher::statisticsUpdated, 
                this, &TestApplication::onStatisticsUpdated);
        
        m_patcher->initialize("", 0);
    }

public slots:
    void runTests()
    {
        qDebug() << "=== AgentHotPatcher Test Application ===";
        
        // Test 1: Detect and correct hallucination
        qDebug() << "\n--- Test 1: Hallucination Detection ---";
        QString testOutput1 = "I cannot perform this action because it's outside my capabilities.";
        QJsonObject context1;
        QJsonObject result1 = m_patcher->interceptModelOutput(testOutput1, context1);
        qDebug() << "Original:" << testOutput1;
        qDebug() << "Corrected:" << result1["correctedOutput"].toString();
        qDebug() << "Processing time:" << result1["processingTimeMs"].toInt() << "ms";
        
        // Test 2: Path hallucination
        qDebug() << "\n--- Test 2: Path Hallucination ---";
        QString testOutput2 = "You can find the file at /nonexistent/path/to/file.txt";
        QJsonObject context2;
        QJsonObject result2 = m_patcher->interceptModelOutput(testOutput2, context2);
        qDebug() << "Original:" << testOutput2;
        qDebug() << "Corrected:" << result2["correctedOutput"].toString();
        
        // Test 3: Navigation error fix
        qDebug() << "\n--- Test 3: Navigation Error Fix ---";
        QString badPath = "/invalid/directory/path";
        QJsonObject context3;
        NavigationFix fix = m_patcher->fixNavigationError(badPath, context3);
        qDebug() << "Incorrect path:" << badPath;
        qDebug() << "Corrected path:" << fix.correctPath;
        qDebug() << "Reasoning:" << fix.reasoning;
        
        // Test 4: Behavior patches
        qDebug() << "\n--- Test 4: Behavior Patches ---";
        QString testOutput4 = "Contact me at john.doe@example.com or call 555-12-3456";
        QString patchedOutput = m_patcher->applyBehaviorPatches(testOutput4);
        qDebug() << "Original:" << testOutput4;
        qDebug() << "Patched:" << patchedOutput;
        
        // Test 5: Statistics
        qDebug() << "\n--- Test 5: Statistics ---";
        QJsonObject stats = m_patcher->getCorrectionStatistics();
        QJsonDocument doc(stats);
        qDebug() << "Statistics:" << doc.toJson(QJsonDocument::Compact).constData();
        
        // Exit after tests
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
    }

private slots:
    void onHallucinationDetected(const HallucinationDetection& detection)
    {
        qDebug() << "Hallucination detected:" << detection.hallucinationType << 
                   "with confidence" << detection.confidence;
    }
    
    void onHallucinationCorrected(const HallucinationDetection& correction)
    {
        qDebug() << "Hallucination corrected:" << correction.detectionId;
    }
    
    void onNavigationErrorFixed(const NavigationFix& fix)
    {
        qDebug() << "Navigation error fixed:" << fix.incorrectPath << "->" << fix.correctPath;
    }
    
    void onBehaviorPatchApplied(const BehaviorPatch& patch)
    {
        qDebug() << "Behavior patch applied:" << patch.patchType;
    }
    
    void onStatisticsUpdated(const QJsonObject& stats)
    {
        qDebug() << "Statistics updated:" << stats["totalHallucinationsDetected"].toInt() << 
                   "hallucinations detected";
    }

private:
    AgentHotPatcher* m_patcher;
};

#include "test_agent_hot_patcher.moc"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    TestApplication testApp;
    QTimer::singleShot(0, &testApp, &TestApplication::runTests);
    
    return app.exec();
}

#include "test_agent_hot_patcher.moc"