#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include "model_loader_widget.hpp"
#include "widgets/hotpatch_panel.h"
#include "unified_hotpatch_manager.hpp"
#include "gguf_loader.hpp"
#include "model_memory_hotpatch.hpp"

class AutomatedIdeTester : public QObject {
    Q_OBJECT

public:
    explicit AutomatedIdeTester(QObject* parent = nullptr);
    
    // Main test entry points
    void runFullTestSuite();
    void runModelLoadingTests();
    void runHotpatcherTests();
    void runUITests();
    void runPerformanceTests();
    
    // Test configuration
    void setTestDataPath(const QString& path) { m_testDataPath = path; }
    void setHeadlessMode(bool headless) { m_headlessMode = headless; }
    
    // Results
    QJsonObject getTestResults() const { return m_results; }
    bool allTestsPassed() const { return m_allTestsPassed; }
    QString getReport() const;

signals:
    void testStarted(const QString& testName);
    void testFinished(const QString& testName, bool passed, const QString& message);
    void testProgress(int current, int total);
    void reportReady(const QString& report);

private slots:
    void onModelLoaded(bool success, const QString& error);
    void onHotpatchApplied(bool success, const QString& error);
    void onUITestComplete(bool success);

private:
    // Test methods
    bool testSafeTensorsLoading();
    bool testPyTorchLoading();
    bool testGGUFLoading();
    bool testMemoryHotpatcher();
    bool testByteLevelHotpatcher();
    bool testServerHotpatcher();
    bool testHotpatcherIntegration();
    bool testUIResponsiveness();
    bool testModelConversion();
    bool testPerformanceBaseline();
    
    // Utility methods
    void logTestResult(const QString& testName, bool passed, const QString& message = "");
    void waitForSignal(QObject* obj, const char* signal, int timeoutMs = 10000);
    bool loadTestModel(const QString& modelPath);
    bool applyTestHotpatch();
    
    // Test data
    QString m_testDataPath;
    bool m_headlessMode{false};
    bool m_allTestsPassed{true};
    int m_testsCompleted{0};
    int m_totalTests{0};
    
    // Test components
    ModelLoaderWidget* m_modelLoader{nullptr};
    HotpatchPanel* m_hotpatchPanel{nullptr};
    UnifiedHotpatchManager* m_hotpatchManager{nullptr};
    GGUFLoader* m_ggufLoader{nullptr};
    
    // Results storage
    QJsonObject m_results;
    QElapsedTimer m_testTimer;
};

// Test data generator for creating minimal test models
class TestModelGenerator {
public:
    static bool createTestSafeTensors(const QString& outputPath);
    static bool createTestPyTorch(const QString& outputPath);
    static bool createTestGGUF(const QString& outputPath);
    
private:
    static QByteArray createSafeTensorsHeader();
    static QByteArray createPyTorchHeader();
    static QByteArray createGGUFHeader();
};