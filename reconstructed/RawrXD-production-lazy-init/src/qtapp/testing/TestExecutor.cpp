#include "TestExecutor.h"
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <random>
#include <algorithm>

namespace RawrXD {

namespace {

bool findDebuggerCommand(QString& program, QStringList& args, const QString& exe, const QStringList& testArgs) {
#ifdef Q_OS_WINDOWS
    QString devenv = QStandardPaths::findExecutable("devenv.exe");
    if (!devenv.isEmpty()) {
        program = devenv;
        args = QStringList{"/DebugExe", exe};
        args.append(testArgs);
        return true;
    }
    // Try gdb/lldb as fallback on Windows if present in PATH (e.g., mingw/llvm)
    QString gdb = QStandardPaths::findExecutable("gdb.exe");
    if (!gdb.isEmpty()) { program = gdb; args = QStringList{"-q", "--args", exe}; args.append(testArgs); return true; }
    QString lldb = QStandardPaths::findExecutable("lldb.exe");
    if (!lldb.isEmpty()) { program = lldb; args = QStringList{"--", exe}; args.append(testArgs); return true; }
    return false;
#else
    QString gdb = QStandardPaths::findExecutable("gdb");
    if (!gdb.isEmpty()) { program = gdb; args = QStringList{"-q", "--args", exe}; args.append(testArgs); return true; }
    QString lldb = QStandardPaths::findExecutable("lldb");
    if (!lldb.isEmpty()) { program = lldb; args = QStringList{"--", exe}; args.append(testArgs); return true; }
    return false;
#endif
}

}

TestExecutor::TestExecutor(QObject* parent)
    : QObject(parent) {
}

TestExecutor::~TestExecutor() {
    cancel();
}

void TestExecutor::executeTest(const TestCase& test, const TestExecutionOptions& options) {
    executeTests({test}, options);
}

void TestExecutor::executeSuite(const TestSuite& suite, const TestExecutionOptions& options) {
    executeTests(suite.cases, options);
}

void TestExecutor::executeTests(const QList<TestCase>& tests, const TestExecutionOptions& options) {
    if (m_running) {
        emit error("Execution already in progress");
        return;
    }
    
    m_running = true;
    m_paused = false;
    m_cancelled = false;
    m_completedCount = 0;
    m_passCount = 0;
    m_failCount = 0;
    m_skipCount = 0;
    m_totalCount = tests.size();
    m_currentOptions = options;
    m_pendingTests = tests;
    
    if (options.shuffleTests) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_pendingTests.begin(), m_pendingTests.end(), g);
    }
    
    emit executionStarted(m_totalCount);
    
    // Start initial batch
    int concurrency = options.runInParallel ? options.maxParallelJobs : 1;
    for (int i = 0; i < std::min(concurrency, static_cast<int>(m_pendingTests.size())); ++i) {
        startNextTest();
    }
}

void TestExecutor::executeAllSuites(const QList<TestSuite>& suites, const TestExecutionOptions& options) {
    QList<TestCase> allTests;
    for (const TestSuite& suite : suites) {
        allTests.append(suite.cases);
    }
    executeTests(allTests, options);
}

void TestExecutor::cancel() {
    if (!m_running) return;
    
    m_cancelled = true;
    
    // Kill all running processes
    for (auto it = m_runningTests.begin(); it != m_runningTests.end(); ++it) {
        if (it.key()->state() == QProcess::Running) {
            it.key()->kill();
        }
        if (it.value().timeoutTimer) {
            it.value().timeoutTimer->stop();
            it.value().timeoutTimer->deleteLater();
        }
    }
    
    m_runningTests.clear();
    m_pendingTests.clear();
    m_running = false;
    
    emit executionCancelled();
}

void TestExecutor::pause() {
    m_paused = true;
}

void TestExecutor::resume() {
    if (!m_paused) return;
    m_paused = false;
    
    // Resume pending tests
    int concurrency = m_currentOptions.runInParallel ? m_currentOptions.maxParallelJobs : 1;
    int toStart = concurrency - m_runningTests.size();
    
    for (int i = 0; i < toStart && !m_pendingTests.isEmpty(); ++i) {
        startNextTest();
    }
}

void TestExecutor::startNextTest() {
    if (m_paused || m_cancelled || m_pendingTests.isEmpty()) {
        if (m_runningTests.isEmpty() && m_pendingTests.isEmpty()) {
            m_running = false;
            emit executionFinished(m_passCount, m_failCount, m_skipCount);
        }
        return;
    }
    
    TestCase test = m_pendingTests.takeFirst();
    emit testStarted(test);
    
    // Dispatch based on framework
    switch (test.framework) {
        case TestFramework::GoogleTest:
            executeGoogleTest(test, m_currentOptions);
            break;
        case TestFramework::Catch2:
            executeCatch2Test(test, m_currentOptions);
            break;
        case TestFramework::PyTest:
            executePyTest(test, m_currentOptions);
            break;
        case TestFramework::Jest:
            executeJestTest(test, m_currentOptions);
            break;
        case TestFramework::GoTest:
            executeGoTest(test, m_currentOptions);
            break;
        case TestFramework::CargoTest:
            executeCargoTest(test, m_currentOptions);
            break;
        case TestFramework::CTest:
            executeCTest(test, m_currentOptions);
            break;
        default:
            emit error(QString("Unsupported framework for test: %1").arg(test.name));
            m_completedCount++;
            emit progress(m_completedCount, m_totalCount);
            startNextTest();
            break;
    }
}

void TestExecutor::executeGoogleTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    // Find executable from test ID
    QString executable = test.filePath; // Assuming filePath contains executable path
    QStringList args = {"--gtest_filter=" + test.fullName()};

    if (options.repeatTests) {
        args << QString("--gtest_repeat=%1").arg(options.repeatCount);
    }
    if (options.shuffleTests) {
        args << "--gtest_shuffle";
    }
    args.append(options.additionalArgs);

    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();

    // Setup timeout
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }

    m_runningTests[process] = rt;

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);

    QString program = executable; QStringList progArgs = args;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, executable, args)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = executable; progArgs = args;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executeCatch2Test(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QString executable = test.filePath;
    QStringList args = {test.name}; // Catch2 accepts test name directly
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString program = executable; QStringList progArgs = args;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, executable, args)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = executable; progArgs = args;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executePyTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QStringList args = {"-v", test.id}; // Use full test ID
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString exe = "pytest"; QStringList testArgs = args; QString program = exe; QStringList progArgs = testArgs;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, exe, testArgs)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = exe; progArgs = testArgs;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executeJestTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QStringList args = {"jest", test.filePath};
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString exe = "npx"; QStringList testArgs = args; QString program = exe; QStringList progArgs = testArgs;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, exe, testArgs)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = exe; progArgs = testArgs;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executeGoTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QString packageDir = test.filePath; // Directory containing test
    QStringList args = {"test", "-run", test.name, "-v"};
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    process->setWorkingDirectory(packageDir);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString exe = "go"; QStringList testArgs = args; QString program = exe; QStringList progArgs = testArgs;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, exe, testArgs)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = exe; progArgs = testArgs;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executeCargoTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QStringList args = {"test", test.name, "--", "--nocapture"};
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString exe = "cargo"; QStringList testArgs = args; QString program = exe; QStringList progArgs = testArgs;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, exe, testArgs)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = exe; progArgs = testArgs;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::executeCTest(const TestCase& test, const TestExecutionOptions& options) {
    QProcess* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    QStringList args = {"-R", test.name, "-V"};
    args.append(options.additionalArgs);
    
    RunningTest rt;
    rt.test = test;
    rt.process = process;
    rt.timer.start();
    
    if (options.timeoutSeconds > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(options.timeoutSeconds * 1000);
        connect(timer, &QTimer::timeout, this, &TestExecutor::onTestTimeout);
        timer->start();
        rt.timeoutTimer = timer;
    }
    
    m_runningTests[process] = rt;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TestExecutor::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &TestExecutor::onProcessError);
    connect(process, &QProcess::readyRead, this, &TestExecutor::onProcessOutput);
    
    QString exe = "ctest"; QStringList testArgs = args; QString program = exe; QStringList progArgs = testArgs;
    if (options.debugMode) {
        if (!findDebuggerCommand(program, progArgs, exe, testArgs)) {
            emit error("Debuggers not found (devenv/gdb/lldb). Running without debugger.");
            program = exe; progArgs = testArgs;
        }
    }
    process->start(program, progArgs);
}

void TestExecutor::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningTests.contains(process)) return;
    
    RunningTest rt = m_runningTests.take(process);
    
    if (rt.timeoutTimer) {
        rt.timeoutTimer->stop();
        rt.timeoutTimer->deleteLater();
    }
    
    TestExecutionResult result;
    
    // Parse based on framework
    switch (rt.test.framework) {
        case TestFramework::GoogleTest:
            result = parseGoogleTestResult(rt, exitCode);
            break;
        case TestFramework::Catch2:
            result = parseCatch2Result(rt, exitCode);
            break;
        case TestFramework::PyTest:
            result = parsePyTestResult(rt, exitCode);
            break;
        case TestFramework::Jest:
            result = parseJestResult(rt, exitCode);
            break;
        case TestFramework::GoTest:
            result = parseGoTestResult(rt, exitCode);
            break;
        case TestFramework::CargoTest:
            result = parseCargoTestResult(rt, exitCode);
            break;
        case TestFramework::CTest:
            result = parseCTestResult(rt, exitCode);
            break;
        default:
            result.testCase = rt.test;
            result.testCase.status = TestStatus::Error;
            break;
    }
    
    result.durationMs = rt.timer.elapsed();
    result.exitCode = exitCode;
    
    // Update counters
    m_completedCount++;
    if (result.isPassed()) m_passCount++;
    else if (result.isFailed()) m_failCount++;
    else if (result.isSkipped()) m_skipCount++;
    
    emit testFinished(result);
    emit progress(m_completedCount, m_totalCount);
    
    cleanupTest(process);
    
    // Check stop on failure
    if (m_currentOptions.stopOnFirstFailure && result.isFailed()) {
        cancel();
        return;
    }
    
    // Start next test
    startNextTest();
}

void TestExecutor::onProcessError(QProcess::ProcessError processError) {
    Q_UNUSED(processError);
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningTests.contains(process)) return;
    
    RunningTest rt = m_runningTests.take(process);
    
    TestExecutionResult result;
    result.testCase = rt.test;
    result.testCase.status = TestStatus::Error;
    result.errorOutput = process->errorString();
    result.durationMs = rt.timer.elapsed();
    
    m_completedCount++;
    m_failCount++;
    
    Q_EMIT testFinished(result);
    Q_EMIT progress(m_completedCount, m_totalCount);
    QString errorMsg = QString("Test execution error: %1 - %2").arg(rt.test.name).arg(result.errorOutput);
    Q_EMIT this->error(errorMsg);
    
    cleanupTest(process);
    startNextTest();
}

void TestExecutor::onProcessOutput() {
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningTests.contains(process)) return;
    
    QString output = QString::fromUtf8(process->readAll());
    m_runningTests[process].output.append(output);
    
    emit testOutput(m_runningTests[process].test.id, output);
}

void TestExecutor::onTestTimeout() {
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // Find the test associated with this timer
    for (auto it = m_runningTests.begin(); it != m_runningTests.end(); ++it) {
        if (it.value().timeoutTimer == timer) {
            QProcess* process = it.key();
            RunningTest rt = m_runningTests.take(process);
            
            process->kill();
            
            TestExecutionResult result;
            result.testCase = rt.test;
            result.testCase.status = TestStatus::Timeout;
            result.output = rt.output;
            result.durationMs = rt.timer.elapsed();
            result.failureMessage = QString("Test timeout after %1 seconds").arg(m_currentOptions.timeoutSeconds);
            
            m_completedCount++;
            m_failCount++;
            
            emit testFinished(result);
            emit progress(m_completedCount, m_totalCount);
            
            cleanupTest(process);
            startNextTest();
            break;
        }
    }
}

TestExecutionResult TestExecutor::parseGoogleTestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (exitCode == 0) {
        result.testCase.status = TestStatus::Passed;
    } else {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    }
    
    return result;
}

TestExecutionResult TestExecutor::parseCatch2Result(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (exitCode == 0 || rt.output.contains("All tests passed")) {
        result.testCase.status = TestStatus::Passed;
    } else {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    }
    
    return result;
}

TestExecutionResult TestExecutor::parsePyTestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (rt.output.contains("PASSED")) {
        result.testCase.status = TestStatus::Passed;
    } else if (rt.output.contains("SKIPPED")) {
        result.testCase.status = TestStatus::Skipped;
    } else if (rt.output.contains("FAILED")) {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    } else {
        result.testCase.status = TestStatus::Error;
    }
    
    return result;
}

TestExecutionResult TestExecutor::parseJestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (exitCode == 0) {
        result.testCase.status = TestStatus::Passed;
    } else {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    }
    
    return result;
}

TestExecutionResult TestExecutor::parseGoTestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (rt.output.contains("--- PASS:")) {
        result.testCase.status = TestStatus::Passed;
    } else if (rt.output.contains("--- SKIP:")) {
        result.testCase.status = TestStatus::Skipped;
    } else if (rt.output.contains("--- FAIL:")) {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    } else {
        result.testCase.status = TestStatus::Error;
    }
    
    return result;
}

TestExecutionResult TestExecutor::parseCargoTestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (rt.output.contains("test result: ok")) {
        result.testCase.status = TestStatus::Passed;
    } else if (rt.output.contains("test result: FAILED")) {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
        extractStackTrace(rt.output, result.stackTrace);
    } else {
        result.testCase.status = TestStatus::Error;
    }
    
    return result;
}

TestExecutionResult TestExecutor::parseCTestResult(const RunningTest& rt, int exitCode) {
    TestExecutionResult result;
    result.testCase = rt.test;
    result.output = rt.output;
    
    if (exitCode == 0 || rt.output.contains("Passed")) {
        result.testCase.status = TestStatus::Passed;
    } else {
        result.testCase.status = TestStatus::Failed;
        extractFailureMessage(rt.output, result.failureMessage);
    }
    
    return result;
}

void TestExecutor::cleanupTest(QProcess* process) {
    if (!process) return;
    process->deleteLater();
}

void TestExecutor::extractStackTrace(const QString& output, QStringList& stackTrace) {
    QRegularExpression stackRe("\\s+at .+|\\s+in .+:\\d+|\\s+#\\d+ .+");
    QRegularExpressionMatchIterator it = stackRe.globalMatch(output);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        stackTrace.append(match.captured(0).trimmed());
    }
}

void TestExecutor::extractFailureMessage(const QString& output, QString& message) {
    // Extract first assertion failure or error message
    QStringList lines = output.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.contains("FAILED") || trimmed.contains("Error") || 
            trimmed.contains("Assertion") || trimmed.contains("Expected")) {
            message = trimmed;
            break;
        }
    }
    
    if (message.isEmpty() && !lines.isEmpty()) {
        message = lines.first().trimmed();
    }
}

} // namespace RawrXD
