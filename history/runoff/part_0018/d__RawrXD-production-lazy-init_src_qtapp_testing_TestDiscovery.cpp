#include "TestDiscovery.h"
#include <QDirIterator>
#include <QProcess>
#include <QTextStream>
#include <QDebug>
#include <QElapsedTimer>

namespace RawrXD {

TestDiscovery::TestDiscovery(QObject* parent)
    : QObject(parent) {
}

DiscoveryResult TestDiscovery::discoverTests(const QString& workspacePath) {
    DiscoveryResult combined;
    combined.success = true;
    
    QList<TestFramework> frameworks = detectAllFrameworks(workspacePath);
    
    if (frameworks.isEmpty()) {
        combined.success = false;
        combined.errorMessage = "No test frameworks detected in workspace";
        return combined;
    }
    
    // Discover from each detected framework
    for (TestFramework fw : frameworks) {
        emit discoveryStarted(fw, workspacePath);
        
        DiscoveryResult result;
        switch (fw) {
            case TestFramework::GoogleTest:
                result = discoverGoogleTest(workspacePath);
                break;
            case TestFramework::Catch2:
                result = discoverCatch2(workspacePath);
                break;
            case TestFramework::PyTest:
                result = discoverPyTest(workspacePath);
                break;
            case TestFramework::Jest:
                result = discoverJest(workspacePath);
                break;
            case TestFramework::GoTest:
                result = discoverGoTest(workspacePath);
                break;
            case TestFramework::CargoTest:
                result = discoverCargoTest(workspacePath);
                break;
            case TestFramework::CTest:
                result = discoverCTest(workspacePath);
                break;
            default:
                continue;
        }
        
        if (result.success) {
            combined.suites.append(result.suites);
        }
    }
    
    combined.calculateTotals();
    emit discoveryFinished(combined);
    return combined;
}

DiscoveryResult TestDiscovery::discoverGoogleTest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::GoogleTest;
    
    // Find test executables (typically *_test, test_*, *_tests, *Test)
    QStringList testExes = findTestExecutables(workspacePath, "*test*");
    
    if (testExes.isEmpty()) {
        result.success = false;
        result.errorMessage = "No Google Test executables found";
        return result;
    }
    
    for (const QString& exe : testExes) {
        QString output = runDiscoveryCommand(exe, {"--gtest_list_tests"});
        if (output.isEmpty()) continue;
        
        QList<TestSuite> suites = parseGoogleTestOutput(output);
        for (auto& suite : suites) {
            suite.framework = TestFramework::GoogleTest;
        }
        result.suites.append(suites);
    }
    
    // Enrich with source file locations
    enrichTestsWithSourceInfo(result.suites, workspacePath);
    
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverCatch2(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::Catch2;
    
    QStringList testExes = findTestExecutables(workspacePath, "*test*");
    
    for (const QString& exe : testExes) {
        // Try Catch2 list command
        QString output = runDiscoveryCommand(exe, {"--list-tests"});
        if (output.isEmpty() || !output.contains("test cases")) continue;
        
        QList<TestSuite> suites = parseCatch2Output(output);
        for (auto& suite : suites) {
            suite.framework = TestFramework::Catch2;
        }
        result.suites.append(suites);
    }
    
    enrichTestsWithSourceInfo(result.suites, workspacePath);
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverPyTest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::PyTest;
    
    // Check if pytest is available
    QString output = runDiscoveryCommand("pytest", {"--version"});
    if (output.isEmpty() || !output.contains("pytest")) {
        result.success = false;
        result.errorMessage = "pytest not found in PATH";
        return result;
    }
    
    // Collect tests
    output = runDiscoveryCommand("pytest", {"--collect-only", "-q", workspacePath});
    if (!output.isEmpty()) {
        result.suites = parsePyTestOutput(output);
        for (auto& suite : result.suites) {
            suite.framework = TestFramework::PyTest;
        }
    }
    
    enrichTestsWithSourceInfo(result.suites, workspacePath);
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverJest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::Jest;
    
    // Check for package.json with jest
    QString packageJson = workspacePath + "/package.json";
    if (!QFile::exists(packageJson)) {
        result.success = false;
        result.errorMessage = "No package.json found";
        return result;
    }
    
    // Try jest --listTests
    QString output = runDiscoveryCommand("npx", {"jest", "--listTests", "--json"});
    if (!output.isEmpty()) {
        result.suites = parseJestOutput(output);
        for (auto& suite : result.suites) {
            suite.framework = TestFramework::Jest;
        }
    }
    
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverGoTest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::GoTest;
    
    // Find Go test files
    QStringList goTestFiles = findTestFiles(workspacePath, {"*_test.go"});
    
    if (goTestFiles.isEmpty()) {
        result.success = false;
        result.errorMessage = "No Go test files found";
        return result;
    }
    
    // Run go test -list for each package directory
    QSet<QString> packageDirs;
    for (const QString& file : goTestFiles) {
        QFileInfo fi(file);
        packageDirs.insert(fi.absolutePath());
    }
    
    for (const QString& dir : packageDirs) {
        QString output = runDiscoveryCommand("go", {"test", "-list", ".", "-C", dir});
        if (!output.isEmpty()) {
            QList<TestSuite> suites = parseGoTestOutput(output);
            for (auto& suite : suites) {
                suite.framework = TestFramework::GoTest;
                suite.filePath = dir;
            }
            result.suites.append(suites);
        }
    }
    
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverCargoTest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::CargoTest;
    
    // Check for Cargo.toml
    if (!QFile::exists(workspacePath + "/Cargo.toml")) {
        result.success = false;
        result.errorMessage = "No Cargo.toml found";
        return result;
    }
    
    // Run cargo test -- --list
    QString output = runDiscoveryCommand("cargo", {"test", "--", "--list", "--format", "terse"});
    if (!output.isEmpty()) {
        result.suites = parseCargoTestOutput(output);
        for (auto& suite : result.suites) {
            suite.framework = TestFramework::CargoTest;
        }
    }
    
    enrichTestsWithSourceInfo(result.suites, workspacePath);
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

DiscoveryResult TestDiscovery::discoverCTest(const QString& workspacePath) {
    DiscoveryResult result;
    result.framework = TestFramework::CTest;
    
    // Look for build directory with CTestTestfile.cmake
    QStringList buildDirs = {"build", "Build", "cmake-build-debug", "cmake-build-release", "_build"};
    QString ctestDir;
    
    for (const QString& dir : buildDirs) {
        QString path = workspacePath + "/" + dir;
        if (QFile::exists(path + "/CTestTestfile.cmake")) {
            ctestDir = path;
            break;
        }
    }
    
    if (ctestDir.isEmpty()) {
        result.success = false;
        result.errorMessage = "No CTest build directory found";
        return result;
    }
    
    // Run ctest -N to list tests
    QString output = runDiscoveryCommand("ctest", {"-N", "-C", ctestDir});
    if (!output.isEmpty()) {
        result.suites = parseCTestOutput(output);
        for (auto& suite : result.suites) {
            suite.framework = TestFramework::CTest;
        }
    }
    
    result.success = !result.suites.isEmpty();
    result.calculateTotals();
    return result;
}

TestFramework TestDiscovery::detectFramework(const QString& workspacePath) {
    // Priority order detection
    if (QFile::exists(workspacePath + "/Cargo.toml")) {
        return TestFramework::CargoTest;
    }
    if (QFile::exists(workspacePath + "/package.json")) {
        return TestFramework::Jest;
    }
    if (QFile::exists(workspacePath + "/go.mod")) {
        return TestFramework::GoTest;
    }
    if (!findTestFiles(workspacePath, {"test_*.py", "*_test.py"}).isEmpty()) {
        return TestFramework::PyTest;
    }
    if (!findTestExecutables(workspacePath, "*test*").isEmpty()) {
        return TestFramework::GoogleTest; // Assume gtest for C++ executables
    }
    
    return TestFramework::Unknown;
}

QList<TestFramework> TestDiscovery::detectAllFrameworks(const QString& workspacePath) {
    QList<TestFramework> frameworks;
    
    // Check for each framework independently
    if (QFile::exists(workspacePath + "/Cargo.toml")) {
        frameworks.append(TestFramework::CargoTest);
    }
    if (QFile::exists(workspacePath + "/package.json")) {
        frameworks.append(TestFramework::Jest);
    }
    if (QFile::exists(workspacePath + "/go.mod")) {
        frameworks.append(TestFramework::GoTest);
    }
    if (!findTestFiles(workspacePath, {"test_*.py", "*_test.py", "tests/**/*.py"}).isEmpty()) {
        frameworks.append(TestFramework::PyTest);
    }
    if (!findTestExecutables(workspacePath, "*test*").isEmpty()) {
        frameworks.append(TestFramework::GoogleTest);
    }
    
    // Check for CMake build directory
    QStringList buildDirs = {"build", "Build", "cmake-build-debug", "cmake-build-release"};
    for (const QString& dir : buildDirs) {
        if (QFile::exists(workspacePath + "/" + dir + "/CTestTestfile.cmake")) {
            frameworks.append(TestFramework::CTest);
            break;
        }
    }
    
    return frameworks;
}

// Helper implementations
QStringList TestDiscovery::findTestExecutables(const QString& path, const QString& pattern) {
    QStringList executables;
    QDirIterator it(path, {pattern}, QDir::Files | QDir::Executable, 
                    QDirIterator::Subdirectories);
    
    int depth = 0;
    while (it.hasNext() && depth < m_maxDepth) {
        QString file = it.next();
        QFileInfo fi(file);
        
        // Filter by patterns
        bool include = m_includePatterns.isEmpty();
        for (const QString& pat : m_includePatterns) {
            if (fi.fileName().contains(QRegularExpression(pat))) {
                include = true;
                break;
            }
        }
        
        bool exclude = false;
        for (const QString& pat : m_excludePatterns) {
            if (fi.fileName().contains(QRegularExpression(pat))) {
                exclude = true;
                break;
            }
        }
        
        if (include && !exclude) {
            executables.append(file);
        }
        depth++;
    }
    
    return executables;
}

QStringList TestDiscovery::findTestFiles(const QString& path, const QStringList& patterns) {
    QStringList files;
    
    for (const QString& pattern : patterns) {
        QDirIterator it(path, {pattern}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files.append(it.next());
        }
    }
    
    return files;
}

QString TestDiscovery::runDiscoveryCommand(const QString& command, const QStringList& args) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(command, args);
    
    if (!process.waitForStarted(m_timeoutSeconds * 1000)) {
        emit error(QString("Failed to start command: %1").arg(command));
        return QString();
    }
    
    if (!process.waitForFinished(m_timeoutSeconds * 1000)) {
        process.kill();
        emit error(QString("Command timeout: %1").arg(command));
        return QString();
    }
    
    if (process.exitStatus() != QProcess::NormalExit) {
        return QString();
    }
    
    return QString::fromUtf8(process.readAll());
}

// Parser implementations
QList<TestSuite> TestDiscovery::parseGoogleTestOutput(const QString& output) {
    QList<TestSuite> suites;
    TestSuite currentSuite;
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        
        // Test suite line ends with '.'
        if (line.endsWith('.')) {
            if (!currentSuite.name.isEmpty()) {
                suites.append(currentSuite);
            }
            currentSuite = TestSuite();
            currentSuite.name = line.left(line.length() - 1);
        }
        // Test case line starts with whitespace
        else if (line.startsWith("  ")) {
            TestCase tc;
            tc.name = line.trimmed();
            tc.suite = currentSuite.name;
            tc.id = currentSuite.name + "::" + tc.name;
            tc.framework = TestFramework::GoogleTest;
            currentSuite.cases.append(tc);
        }
    }
    
    if (!currentSuite.name.isEmpty()) {
        suites.append(currentSuite);
    }
    
    return suites;
}

QList<TestSuite> TestDiscovery::parseCatch2Output(const QString& output) {
    QList<TestSuite> suites;
    TestSuite suite;
    suite.name = "Catch2Tests";
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith("=")) continue;
        
        // Test case line
        TestCase tc;
        tc.name = line;
        tc.id = line;
        tc.framework = TestFramework::Catch2;
        suite.cases.append(tc);
    }
    
    if (!suite.cases.isEmpty()) {
        suites.append(suite);
    }
    
    return suites;
}

QList<TestSuite> TestDiscovery::parsePyTestOutput(const QString& output) {
    QList<TestSuite> suites;
    QMap<QString, TestSuite> suiteMap;
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith("<")) continue;
        
        // Format: file.py::TestClass::test_method or file.py::test_function
        QStringList parts = line.split("::");
        if (parts.isEmpty()) continue;
        
        QString filePath = parts[0];
        QString suiteName = parts.size() > 2 ? parts[1] : "Module";
        QString testName = parts.last();
        
        if (!suiteMap.contains(suiteName)) {
            TestSuite suite;
            suite.name = suiteName;
            suite.filePath = filePath;
            suiteMap[suiteName] = suite;
        }
        
        TestCase tc;
        tc.name = testName;
        tc.suite = suiteName;
        tc.id = line;
        tc.filePath = filePath;
        tc.framework = TestFramework::PyTest;
        suiteMap[suiteName].cases.append(tc);
    }
    
    return suiteMap.values();
}

QList<TestSuite> TestDiscovery::parseJestOutput(const QString& output) {
    QList<TestSuite> suites;
    TestSuite suite;
    suite.name = "JestTests";
    
    // Simple line-by-line parsing (Jest JSON output would be better)
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        
        // Jest test file paths
        if (line.endsWith(".test.js") || line.endsWith(".spec.js") || 
            line.endsWith(".test.ts") || line.endsWith(".spec.ts")) {
            
            TestCase tc;
            tc.name = QFileInfo(line).fileName();
            tc.filePath = line;
            tc.id = line;
            tc.framework = TestFramework::Jest;
            suite.cases.append(tc);
        }
    }
    
    if (!suite.cases.isEmpty()) {
        suites.append(suite);
    }
    
    return suites;
}

QList<TestSuite> TestDiscovery::parseGoTestOutput(const QString& output) {
    QList<TestSuite> suites;
    TestSuite suite;
    suite.name = "GoTests";
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty() || !line.startsWith("Test")) continue;
        
        TestCase tc;
        tc.name = line;
        tc.id = line;
        tc.framework = TestFramework::GoTest;
        suite.cases.append(tc);
    }
    
    if (!suite.cases.isEmpty()) {
        suites.append(suite);
    }
    
    return suites;
}

QList<TestSuite> TestDiscovery::parseCargoTestOutput(const QString& output) {
    QList<TestSuite> suites;
    TestSuite suite;
    suite.name = "RustTests";
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty() || line.contains(": test")) continue;
        
        // Remove ": test" suffix
        QString testName = line.split(":").first().trimmed();
        if (testName.isEmpty()) continue;
        
        TestCase tc;
        tc.name = testName;
        tc.id = testName;
        tc.framework = TestFramework::CargoTest;
        suite.cases.append(tc);
    }
    
    if (!suite.cases.isEmpty()) {
        suites.append(suite);
    }
    
    return suites;
}

QList<TestSuite> TestDiscovery::parseCTestOutput(const QString& output) {
    QList<TestSuite> suites;
    TestSuite suite;
    suite.name = "CTests";
    
    QTextStream stream(const_cast<QString*>(&output));
    QString line;
    
    QRegularExpression testRe("Test\\s+#\\d+:\\s+(.+)");
    
    while (stream.readLineInto(&line)) {
        QRegularExpressionMatch match = testRe.match(line);
        if (match.hasMatch()) {
            TestCase tc;
            tc.name = match.captured(1).trimmed();
            tc.id = tc.name;
            tc.framework = TestFramework::CTest;
            suite.cases.append(tc);
        }
    }
    
    if (!suite.cases.isEmpty()) {
        suites.append(suite);
    }
    
    return suites;
}

void TestDiscovery::enrichTestsWithSourceInfo(QList<TestSuite>& suites, const QString& workspacePath) {
    // This is a simplified version - production would use AST parsing or regex matching
    for (TestSuite& suite : suites) {
        for (TestCase& tc : suite.cases) {
            if (tc.filePath.isEmpty()) {
                auto [file, line] = findTestInSource(tc.name, workspacePath);
                tc.filePath = file;
                tc.lineNumber = line;
            }
        }
    }
}

QPair<QString, int> TestDiscovery::findTestInSource(const QString& testName, const QString& workspacePath) {
    // Simplified source search - would need more sophisticated matching
    QStringList sourceFiles = findTestFiles(workspacePath, {"*.cpp", "*.cc", "*.h", "*.hpp", "*.py", "*.rs", "*.go"});
    
    for (const QString& file : sourceFiles) {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        
        QTextStream in(&f);
        int lineNum = 1;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains(testName)) {
                return {file, lineNum};
            }
            lineNum++;
        }
    }
    
    return {QString(), -1};
}

} // namespace RawrXD
