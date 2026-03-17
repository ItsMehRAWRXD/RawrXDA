#pragma once

#include "TestDiscovery.h"
#include "TestExecutor.h"
#include "CoverageCollector.h"
#include "TestOutputPanel.h"
#include <QDockWidget>
#include <QTreeWidget>
#include <QToolBar>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QSplitter>
#include <QTabWidget>

namespace RawrXD {

class TestRunnerPanel : public QDockWidget {
    Q_OBJECT
    
public:
    explicit TestRunnerPanel(QWidget* parent = nullptr);
    ~TestRunnerPanel();
    
    // Workspace management
    void setWorkspace(const QString& path);
    QString workspace() const { return m_workspacePath; }
    
    // Test discovery
    void discoverTests();
    void refreshTests();
    
    // Test execution
    void runAllTests();
    void runSelectedTests();
    void runFailedTests();
    void debugSelectedTest();
    void stopExecution();
    
    // UI state
    void expandAll();
    void collapseAll();
    void clearResults();
    
signals:
    void testSelected(const TestCase& test);
    void testDoubleClicked(const TestCase& test);
    void workspaceChanged(const QString& path);
    
private slots:
    // Discovery slots
    void onDiscoveryStarted(TestFramework framework, const QString& path);
    void onDiscoveryFinished(const DiscoveryResult& result);
    void onDiscoveryError(const QString& message);
    
    // Execution slots
    void onExecutionStarted(int totalTests);
    void onTestStarted(const TestCase& test);
    void onTestFinished(const TestExecutionResult& result);
    void onExecutionFinished(int passed, int failed, int skipped);
    void onExecutionProgress(int current, int total);
    void onTestOutput(const QString& testId, const QString& output);
    
    // UI slots
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onTreeContextMenu(const QPoint& pos);
    void onFilterTextChanged(const QString& text);
    void onFrameworkFilterChanged(int index);
    void onStatusFilterChanged(int index);
    void onRunButtonClicked();
    void onDebugButtonClicked();
    void onStopButtonClicked();
    void onRefreshButtonClicked();
    
private:
    void setupUi();
    void createToolbar();
    void createTreeView();
    void createOutputView();
    void createStatusBar();
    void refreshCoverage();
    
    // Tree management
    void populateTree(const QList<TestSuite>& suites);
    void updateTreeItem(const TestExecutionResult& result);
    QTreeWidgetItem* findTreeItem(const QString& testId);
    void updateTreeIcons();
    void applyFilters();
    
    // UI helpers
    QIcon getStatusIcon(TestStatus status);
    QColor getStatusColor(TestStatus status);
    QString formatDuration(double ms);
    void updateStatusLabel();
    void updateProgressBar(int current, int total);
    
    // Components
    QWidget* m_mainWidget;
    QToolBar* m_toolbar;
    QTreeWidget* m_testTree;
    QTextEdit* m_outputView;
    TestOutputPanel* m_outputDetailsPanel;
    QTreeWidget* m_coverageTree;
    QTabWidget* m_bottomTabs;
    QSplitter* m_splitter;
    
    // Toolbar widgets
    QPushButton* m_runBtn;
    QPushButton* m_runSelectedBtn;
    QPushButton* m_runFailedBtn;
    QPushButton* m_debugBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_refreshBtn;
    QLineEdit* m_filterEdit;
    QComboBox* m_frameworkFilter;
    QComboBox* m_statusFilter;
    
    // Status bar
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_statsLabel;
    
    // Test management
    TestDiscovery* m_discovery;
    TestExecutor* m_executor;
    CoverageCollector* m_coverageCollector;
    QString m_workspacePath;
    
    // Test data
    QList<TestSuite> m_testSuites;
    QMap<QString, TestExecutionResult> m_testResults;
    QMap<QString, QTreeWidgetItem*> m_treeItemMap;
    
    // Statistics
    int m_totalTests = 0;
    int m_passedTests = 0;
    int m_failedTests = 0;
    int m_skippedTests = 0;
    int m_runningTests = 0;
    double m_totalDurationMs = 0.0;
    
    // UI state
    bool m_discovering = false;
    bool m_running = false;
};

} // namespace RawrXD
