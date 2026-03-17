#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

namespace RawrXD {

// Test framework types
enum class TestFramework {
    Unknown,
    GoogleTest,      // C++ gtest/gmock
    Catch2,          // C++ Catch2
    PyTest,          // Python pytest
    Unittest,        // Python unittest
    Jest,            // JavaScript/TypeScript
    Mocha,           // JavaScript
    GoTest,          // Go testing
    CargoTest,       // Rust cargo test
    JUnit,           // Java JUnit
    CTest,           // CMake CTest
    Custom           // User-defined
};

// Test status
enum class TestStatus {
    NotRun,
    Running,
    Passed,
    Failed,
    Skipped,
    Timeout,
    Error
};

// Individual test case
struct TestCase {
    QString id;              // Unique identifier
    QString name;            // Display name
    QString suite;           // Test suite/class name
    QString filePath;        // Source file path
    int lineNumber = -1;     // Line number in source
    TestFramework framework = TestFramework::Unknown;
    TestStatus status = TestStatus::NotRun;
    QString output;          // Test output/error message
    double durationMs = 0.0; // Execution time
    QStringList tags;        // Test tags/labels
    
    QString fullName() const {
        return suite.isEmpty() ? name : suite + "::" + name;
    }
};

// Test suite/group
struct TestSuite {
    QString name;
    QString filePath;
    TestFramework framework = TestFramework::Unknown;
    QList<TestCase> cases;
    int passCount = 0;
    int failCount = 0;
    int skipCount = 0;
    
    void updateCounts() {
        passCount = failCount = skipCount = 0;
        for (const auto& tc : cases) {
            if (tc.status == TestStatus::Passed) passCount++;
            else if (tc.status == TestStatus::Failed) failCount++;
            else if (tc.status == TestStatus::Skipped) skipCount++;
        }
    }
};

// Discovery result
struct DiscoveryResult {
    bool success = false;
    QString errorMessage;
    QList<TestSuite> suites;
    int totalTests = 0;
    TestFramework framework = TestFramework::Unknown;
    
    void calculateTotals() {
        totalTests = 0;
        for (const auto& suite : suites) {
            totalTests += suite.cases.size();
        }
    }
};

/**
 * @brief TestDiscovery - Discovers tests across multiple frameworks
 * 
 * Supports:
 * - Google Test (gtest): --gtest_list_tests
 * - Catch2: --list-tests
 * - PyTest: --collect-only
 * - Jest: --listTests
 * - Go: go test -list
 * - Cargo: cargo test -- --list
 * - CTest: ctest -N
 */
class TestDiscovery : public QObject {
    Q_OBJECT
    
public:
    explicit TestDiscovery(QObject* parent = nullptr);
    
    // Discover tests in workspace
    DiscoveryResult discoverTests(const QString& workspacePath);
    
    // Discover specific framework
    DiscoveryResult discoverGoogleTest(const QString& workspacePath);
    DiscoveryResult discoverCatch2(const QString& workspacePath);
    DiscoveryResult discoverPyTest(const QString& workspacePath);
    DiscoveryResult discoverJest(const QString& workspacePath);
    DiscoveryResult discoverGoTest(const QString& workspacePath);
    DiscoveryResult discoverCargoTest(const QString& workspacePath);
    DiscoveryResult discoverCTest(const QString& workspacePath);
    
    // Framework detection
    TestFramework detectFramework(const QString& workspacePath);
    QList<TestFramework> detectAllFrameworks(const QString& workspacePath);
    
    // Configuration
    void setMaxDepth(int depth) { m_maxDepth = depth; }
    void setIncludePatterns(const QStringList& patterns) { m_includePatterns = patterns; }
    void setExcludePatterns(const QStringList& patterns) { m_excludePatterns = patterns; }
    void setTimeout(int seconds) { m_timeoutSeconds = seconds; }
    
signals:
    void discoveryStarted(TestFramework framework, const QString& path);
    void discoveryProgress(int current, int total);
    void discoveryFinished(const DiscoveryResult& result);
    void error(const QString& message);
    
private:
    // Helper methods
    QStringList findTestExecutables(const QString& path, const QString& pattern);
    QStringList findTestFiles(const QString& path, const QStringList& patterns);
    bool isTestExecutable(const QString& filePath, TestFramework framework);
    QString runDiscoveryCommand(const QString& command, const QStringList& args);
    
    // Parsers for each framework
    QList<TestSuite> parseGoogleTestOutput(const QString& output);
    QList<TestSuite> parseCatch2Output(const QString& output);
    QList<TestSuite> parsePyTestOutput(const QString& output);
    QList<TestSuite> parseJestOutput(const QString& output);
    QList<TestSuite> parseGoTestOutput(const QString& output);
    QList<TestSuite> parseCargoTestOutput(const QString& output);
    QList<TestSuite> parseCTestOutput(const QString& output);
    
    // Source file analysis
    void enrichTestsWithSourceInfo(QList<TestSuite>& suites, const QString& workspacePath);
    QPair<QString, int> findTestInSource(const QString& testName, const QString& workspacePath);
    
    int m_maxDepth = 10;
    int m_timeoutSeconds = 30;
    QStringList m_includePatterns;
    QStringList m_excludePatterns;
};

} // namespace RawrXD
