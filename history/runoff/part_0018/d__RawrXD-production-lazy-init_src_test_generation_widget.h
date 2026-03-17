/**
 * @file test_generation_widget.h
 * @brief Enhanced Test Generation UI with Coverage Visualization
 * 
 * Complete test generation interface with:
 * - Automated test case generation
 * - Coverage visualization
 * - Test execution and results
 * - Mutation testing support
 * - Fuzz testing configuration
 * - Export test suites
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include "test_generation_engine.h"

class QTreeWidget;
class QTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QTabWidget;
class QSplitter;
class TestGenerator;

/**
 * @class TestGenerationWidget
 * @brief Complete test generation and management UI
 */
class TestGenerationWidget : public QWidget {
    Q_OBJECT

public:
    explicit TestGenerationWidget(QWidget* parent = nullptr);
    ~TestGenerationWidget() override;

    void initialize();
    void generateTestsForFile(const QString& filePath);
    void generateTestsForProject(const QString& projectPath);

public slots:
    void onGenerateTests();
    void onRunTests();
    void onExportTests();
    void onViewCoverage();
    void onTestCaseSelected(const QString& testId);
    void onGenerationProgress(int current, int total);
    void onTestExecutionFinished(const QString& testId, bool passed);

signals:
    void testsGenerated(int count);
    void testExecutionStarted();
    void testExecutionCompleted(int passed, int failed);
    void coverageUpdated(double percentage);

private:
    void setupUI();
    void createGenerationPanel();
    void createExecutionPanel();
    void createCoveragePanel();
    void updateCoverageVisualization(const CoverageReport& report);
    void displayTestCase(const TestCase& testCase);
    
    TestGenerator* m_generator = nullptr;
    
    QTreeWidget* m_testTree = nullptr;
    QTextEdit* m_testCodeView = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statsLabel = nullptr;
    QPushButton* m_generateBtn = nullptr;
    QPushButton* m_runBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    
    QVector<TestCase> m_generatedTests;
    CoverageReport m_coverageReport;
    QString m_projectPath;
};
