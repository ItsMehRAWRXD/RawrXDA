/**
 * @file test_explorer_widget.h
 * @brief Production implementation of TestExplorerWidget
 * 
 * Provides a fully functional test explorer including:
 * - Test discovery (GTest, Catch2, pytest, etc.)
 * - Test tree view with hierarchy
 * - Test execution with real-time status
 * - Test filtering and searching
 * - Coverage integration
 * - Test history and statistics
 * 
 * Per AI Toolkit Production Readiness Instructions:
 * - NO SIMPLIFICATIONS - all logic must remain intact
 * - Full structured logging for observability
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QProcess>
#include <QToolBar>
#include <QProgressBar>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QElapsedTimer>

class TestExplorerWidget : public QWidget {
    Q_OBJECT

public:
    enum class TestStatus {
        Unknown,
        Pending,
        Running,
        Passed,
        Failed,
        Skipped,
        Timeout
    };
    Q_ENUM(TestStatus)

    enum class TestFramework {
        GTest,
        Catch2,
        PyTest,
        Jest,
        Mocha,
        NUnit,
        Custom
    };
    Q_ENUM(TestFramework)

    struct TestCase {
        QString id;
        QString name;
        QString suite;
        QString file;
        int line;
        TestStatus status;
        QString output;
        qint64 duration;  // milliseconds
        QString errorMessage;
    };

    struct TestSuite {
        QString name;
        QString file;
        QList<TestCase> tests;
        int passed;
        int failed;
        int skipped;
        qint64 totalDuration;
    };

    explicit TestExplorerWidget(QWidget* parent = nullptr);
    ~TestExplorerWidget() override;

    // Project configuration
    void setProjectPath(const QString& path);
    void setTestFramework(TestFramework framework);
    void setTestExecutable(const QString& path);
    void setTestCommand(const QString& command);
    void setTestFilter(const QString& filter);

    // Test discovery
    void discoverTests();
    void refreshTests();
    QList<TestSuite> getTestSuites() const;
    int getTotalTestCount() const;

    // Test execution
    void runAllTests();
    void runSelectedTests();
    void runFailedTests();
    void runTest(const QString& testId);
    void runSuite(const QString& suiteName);
    void stopTests();

    // Debug
    void debugSelectedTest();
    void debugTest(const QString& testId);

    // Results
    TestStatus getTestStatus(const QString& testId) const;
    QString getTestOutput(const QString& testId) const;
    QJsonObject getTestStatistics() const;

    // Coverage
    void enableCoverage(bool enable);
    double getCoveragePercent() const;

signals:
    void testDiscovered(const QString& suiteName, int testCount);
    void testStarted(const QString& testId);
    void testFinished(const QString& testId, TestStatus status);
    void allTestsFinished(int passed, int failed, int skipped);
    void testOutputReceived(const QString& testId, const QString& output);
    void coverageUpdated(double percent);
    void testDoubleClicked(const QString& file, int line);

public slots:
    void onRunAllClicked();
    void onRunSelectedClicked();
    void onStopClicked();
    void onRefreshClicked();
    void onFilterChanged(const QString& filter);
    void onTestItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    void populateTestTree();
    void updateTestItem(QTreeWidgetItem* item, const TestCase& test);
    void parseTestFile(const QString& filePath);
    void parseGTestOutput(const QString& output);
    void parseCatch2Output(const QString& output);
    void parsePyTestOutput(const QString& output);
    void updateStatistics();
    QIcon statusIcon(TestStatus status) const;
    QString statusString(TestStatus status) const;
    QColor statusColor(TestStatus status) const;
    void appendOutput(const QString& text);
    QTreeWidgetItem* findTestItem(const QString& testId) const;
    QTreeWidgetItem* findSuiteItem(const QString& suiteName) const;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QComboBox* m_frameworkCombo;
    QLineEdit* m_filterEdit;
    QPushButton* m_runAllButton;
    QPushButton* m_runSelectedButton;
    QPushButton* m_stopButton;
    QPushButton* m_refreshButton;
    QSplitter* m_splitter;
    QTreeWidget* m_testTree;
    QTextEdit* m_outputView;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_statsLabel;

    // Test state
    QString m_projectPath;
    TestFramework m_framework;
    QString m_testExecutable;
    QString m_testCommand;
    QString m_testFilter;
    QMap<QString, TestSuite> m_testSuites;
    QMap<QString, TestCase> m_tests;
    QProcess* m_testProcess;
    bool m_isRunning;
    QString m_currentTestId;
    QElapsedTimer m_testTimer;

    // Coverage
    bool m_coverageEnabled;
    double m_coveragePercent;

    // Statistics
    int m_totalTests;
    int m_passedTests;
    int m_failedTests;
    int m_skippedTests;
    qint64 m_totalDuration;

    // File watching
    QFileSystemWatcher* m_watcher;
};

