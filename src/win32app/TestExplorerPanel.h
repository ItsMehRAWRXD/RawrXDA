#ifndef TESTEXPLORERPANEL_H
#define TESTEXPLORERPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
// #include "test_runner_integration.h" // TODO: Add test runner integration

class TestExplorerPanel : public QWidget {
    Q_OBJECT

public:
    explicit TestExplorerPanel(QWidget* parent = nullptr);
    ~TestExplorerPanel() override = default;

    void initialize();

signals:
    void runAllRequested();
    void runSelectedRequested();
    void generateTestsRequested();
    void showCoverageRequested();

private slots:
    void onRunAll();
    void onRunSelected();
    void onGenerateTests();
    void onShowCoverage();
    void onTestStarted(const QString& suite, const QString& test);
    void onTestFinished(const QString& suite, const QString& test, bool success);
    void onSuiteFinished(const QString& suite, int passed, int failed, int total);

private:
    void setupUI();
    void updateStats();

    QTreeWidget* m_testTree;
    QProgressBar* m_overallProgress;
    QLabel* m_statsLabel;
    
    QToolBar* m_toolBar;
    QAction* m_runAllAction;
    QAction* m_runSelectedAction;
    QAction* m_generateTestsAction;
    QAction* m_showCoverageAction;

    int m_totalTests = 0;
    int m_passedTests = 0;
    int m_failedTests = 0;
};

#endif // TESTEXPLORERPANEL_H
