#ifndef TEST_RUNNER_INTEGRATION_H
#define TEST_RUNNER_INTEGRATION_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTreeWidget>
#include <QStandardItemModel>

namespace RawrXD {

struct TestResult {
    QString name;
    QString status; // "passed", "failed", "skipped", "error"
    QString message;
    qint64 durationMs;
    QString output;
    QString error;
    QDateTime timestamp;
    
    TestResult() : durationMs(0) {}
    TestResult(const QString& n, const QString& s, const QString& m = QString())
        : name(n), status(s), message(m), durationMs(0), timestamp(QDateTime::currentDateTime()) {}
};

struct TestSuite {
    QString name;
    QString executable;
    QString workingDir;
    QVector<TestResult> results;
    int totalTests;
    int passedTests;
    int failedTests;
    int skippedTests;
    qint64 totalDuration;
    
    TestSuite() : totalTests(0), passedTests(0), failedTests(0), skippedTests(0), totalDuration(0) {}
    TestSuite(const QString& n, const QString& exe) 
        : name(n), executable(exe), totalTests(0), passedTests(0), failedTests(0), skippedTests(0), totalDuration(0) {}
    
    double getPassRate() const {
        if (totalTests == 0) return 0.0;
        return static_cast<double>(passedTests) / totalTests * 100.0;
    }
};

class TestRunnerIntegration : public QObject {
    Q_OBJECT

public:
    static TestRunnerIntegration& instance();
    
    void initialize(QTabWidget* outputTabs, QTreeWidget* testTree, QPlainTextEdit* outputConsole);
    void shutdown();
    
    // Test execution
    bool runTestSuite(const QString& suiteName, const QString& executable, const QString& workingDir = QString());
    bool runTest(const QString& testName, const QString& executable, const QStringList& args = QStringList());
    void stopCurrentTest();
    
    // Test management
    void addTestSuite(const QString& name, const QString& executable, const QString& workingDir = QString());
    bool removeTestSuite(const QString& name);
    QVector<TestSuite> getTestSuites() const;
    
    // Results and statistics
    TestSuite getSuiteResults(const QString& suiteName) const;
    QVector<TestResult> getFailedTests() const;
    void clearResults();
    
    // Output integration
    void setOutputTab(QTabWidget* tabs);
    void setTestTree(QTreeWidget* tree);
    void setOutputConsole(QPlainTextEdit* console);
    
    // Configuration
    void setAutoScrollOutput(bool enable);
    void setShowPassedTests(bool show);
    void setTimeout(int seconds);
    
signals:
    void testStarted(const QString& suiteName, const QString& testName);
    void testFinished(const QString& suiteName, const QString& testName, bool success);
    void suiteStarted(const QString& suiteName);
    void suiteFinished(const QString& suiteName, int passed, int failed, int total);
    void outputReceived(const QString& output);
    void errorReceived(const QString& error);

private slots:
    void onTestProcessReadyRead();
    void onTestProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTestProcessError(QProcess::ProcessError error);

private:
    TestRunnerIntegration() = default;
    ~TestRunnerIntegration();
    
    void parseTestOutput(const QByteArray& output);
    void updateTestTree();
    void updateOutputConsole(const QString& text);
    void createTestTab();
    
    QTabWidget* outputTabs_ = nullptr;
    QTreeWidget* testTree_ = nullptr;
    QPlainTextEdit* outputConsole_ = nullptr;
    QProcess* currentProcess_ = nullptr;
    
    QHash<QString, TestSuite> testSuites_;
    TestSuite* currentSuite_ = nullptr;
    TestResult* currentTest_ = nullptr;
    
    bool autoScrollOutput_ = true;
    bool showPassedTests_ = true;
    int timeoutSeconds_ = 300; // 5 minutes
    
    QString currentOutput_;
    QString currentError_;
};

// Convenience macros
#define TEST_RUN_SUITE(name, exe) RawrXD::TestRunnerIntegration::instance().runTestSuite(name, exe)
#define TEST_RUN(name, exe) RawrXD::TestRunnerIntegration::instance().runTest(name, exe)
#define TEST_STOP() RawrXD::TestRunnerIntegration::instance().stopCurrentTest()
#define TEST_ADD_SUITE(name, exe) RawrXD::TestRunnerIntegration::instance().addTestSuite(name, exe)

} // namespace RawrXD

#endif // TEST_RUNNER_INTEGRATION_H