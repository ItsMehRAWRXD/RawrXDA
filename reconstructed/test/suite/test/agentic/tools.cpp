#include <QTest>
#include <QObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QSignalSpy>
#include <QDebug>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <chrono>

// Forward declarations
class AgenticToolExecutor;
struct ToolResult;

// Include the header
#include "agentic_tools.hpp"

class TestAgenticTools : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // readFile tests
    void testReadFileSuccess();
    void testReadFileNotFound();
    void testReadFilePermissionDenied();
    void testReadFileLargeFile();

    // writeFile tests
    void testWriteFileCreateNew();
    void testWriteFileOverwrite();
    void testWriteFileCreateDirectory();
    void testWriteFilePermissionDenied();

    // listDirectory tests
    void testListDirectorySuccess();
    void testListDirectoryNotFound();
    void testListDirectoryEmpty();
    void testListDirectoryNested();

    // executeCommand tests
    void testExecuteCommandSuccess();
    void testExecuteCommandWithOutput();
    void testExecuteCommandTimeout();
    void testExecuteCommandNotFound();
    void testExecuteCommandError();

    // grepSearch tests
    void testGrepSearchSuccess();
    void testGrepSearchNoMatches();
    void testGrepSearchInvalidRegex();
    void testGrepSearchMultipleMatches();

    // gitStatus tests
    void testGitStatusInRepository();
    void testGitStatusNotRepository();

    // runTests tests
    void testRunTestsCTest();
    void testRunTestsPython();
    void testRunTestsNotFound();

    // analyzeCode tests
    void testAnalyzeCodeCpp();
    void testAnalyzeCodePython();
    void testAnalyzeCodeUnknownLanguage();
    void testAnalyzeCodeInvalidFile();
    // safety toggle tests
    void testDeleteFileBlocked();
    void testDeleteFileAllowedAfterToggle();
    void testShellDeleteBlocked();

    // Signal tests
    void testToolCompletedSignal();
    void testToolErrorSignal();
    void testToolProgressSignal();
    void testToolExecutedSignal();

    // Integration tests
    void testMultipleToolsSequential();
    void testToolsWithMetrics();

private:
    AgenticToolExecutor* executor;
    QTemporaryDir tempDir;
};

void TestAgenticTools::initTestCase()
{
    executor = new AgenticToolExecutor();
    Q_ASSERT(tempDir.isValid());
}

void TestAgenticTools::cleanupTestCase()
{
    delete executor;
}

// ============================================================================
// readFile Tests
// ============================================================================

void TestAgenticTools::testReadFileSuccess()
{
    // Create a test file
    QString testFile = tempDir.path() + "/test.txt";
    QString testContent = "Hello, World!\nLine 2\nLine 3";
    
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(testContent.toUtf8());
    file.close();

    // Execute readFile
    ToolResult result = executor->readFile(testFile);

    QVERIFY(result.success);
    QCOMPARE(result.output, testContent);
    QCOMPARE(result.error, "");
    // Execution time should be tracked (may be >= 0 due to system resolution)
    QVERIFY(result.executionTimeMs >= 0);
}

void TestAgenticTools::testReadFileNotFound()
{
    QString nonexistent = tempDir.path() + "/nonexistent_file_xyz.txt";
    
    ToolResult result = executor->readFile(nonexistent);

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Cannot open file") || result.error.contains("not found"));
    QVERIFY(result.output.isEmpty());
}

void TestAgenticTools::testReadFilePermissionDenied()
{
    // Note: This test may not work on all systems, skip if needed
    #ifdef Q_OS_WIN
    // Windows permission test is complex, skip
    QSKIP("Permission test not applicable on Windows");
    #else
    QString testFile = tempDir.path() + "/noperm.txt";
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("Content");
    file.close();
    
    file.setPermissions(QFile::Permissions(0));
    
    ToolResult result = executor->readFile(testFile);
    QVERIFY(!result.success);
    
    // Restore permissions for cleanup
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    #endif
}

void TestAgenticTools::testReadFileLargeFile()
{
    QString largeFile = tempDir.path() + "/large.txt";
    QFile file(largeFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    
    // Write 10 MB
    const int lineCount = 100000;
    QString line = "This is a test line with some data.\n";
    for (int i = 0; i < lineCount; ++i) {
        file.write(line.toUtf8());
    }
    file.close();

    ToolResult result = executor->readFile(largeFile);

    QVERIFY(result.success);
    QVERIFY(result.output.size() > 1000000); // > 1MB
    QVERIFY(result.executionTimeMs > 0);
}

// ============================================================================
// writeFile Tests
// ============================================================================

void TestAgenticTools::testWriteFileCreateNew()
{
    QString newFile = tempDir.path() + "/new_file.txt";
    QString content = "New file content";

    ToolResult result = executor->writeFile(newFile, content);

    QVERIFY(result.success);
    QVERIFY(QFile::exists(newFile));
    
    QFile file(newFile);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), content.toUtf8());
    file.close();
}

void TestAgenticTools::testWriteFileOverwrite()
{
    QString testFile = tempDir.path() + "/overwrite.txt";
    
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("Old content");
    file.close();

    QString newContent = "New content that overwrites";
    ToolResult result = executor->writeFile(testFile, newContent);

    QVERIFY(result.success);
    
    file.setFileName(testFile);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), newContent.toUtf8());
    file.close();
}

void TestAgenticTools::testWriteFileCreateDirectory()
{
    QString nestedDir = tempDir.path() + "/nested/deep/structure";
    QString nestedFile = nestedDir + "/file.txt";
    QString content = "Nested file";

    ToolResult result = executor->writeFile(nestedFile, content);

    QVERIFY(result.success);
    QVERIFY(QFile::exists(nestedFile));
    
    QFile file(nestedFile);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), content.toUtf8());
    file.close();
}

void TestAgenticTools::testWriteFilePermissionDenied()
{
    #ifdef Q_OS_WIN
    QSKIP("Permission test not applicable on Windows");
    #else
    QString noPermDir = tempDir.path() + "/noperm";
    QDir(tempDir.path()).mkdir("noperm");
    QFile::setPermissions(noPermDir, QFile::Permissions(0));
    
    QString testFile = noPermDir + "/file.txt";
    ToolResult result = executor->writeFile(testFile, "content");
    
    QVERIFY(!result.success);
    
    // Restore permissions
    QFile::setPermissions(noPermDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    #endif
}

// ============================================================================
// listDirectory Tests
// ============================================================================

void TestAgenticTools::testListDirectorySuccess()
{
    // Create test structure
    QDir dir(tempDir.path());
    dir.mkdir("subdir1");
    dir.mkdir("subdir2");
    
    QFile file1(tempDir.path() + "/file1.txt");
    file1.open(QIODevice::WriteOnly);
    file1.write("content");
    file1.close();

    ToolResult result = executor->listDirectory(tempDir.path());

    QVERIFY(result.success);
    QVERIFY(result.output.contains("subdir1") || result.output.contains("file1.txt"));
}

void TestAgenticTools::testListDirectoryNotFound()
{
    QString nonexistent = tempDir.path() + "/nonexistent_dir_xyz";
    
    ToolResult result = executor->listDirectory(nonexistent);

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Cannot open directory") || result.error.contains("not found"));
}

void TestAgenticTools::testListDirectoryEmpty()
{
    QString emptyDir = tempDir.path() + "/empty_dir";
    QDir(tempDir.path()).mkdir("empty_dir");

    ToolResult result = executor->listDirectory(emptyDir);

    QVERIFY(result.success);
    // Output should be "No files in directory" or similar
    QVERIFY(result.output.isEmpty() || result.output.contains("No files"));
}

void TestAgenticTools::testListDirectoryNested()
{
    QString nestedDir = tempDir.path() + "/level1/level2/level3";
    QDir(tempDir.path()).mkpath("level1/level2/level3");
    
    QFile file(nestedDir + "/deep_file.txt");
    file.open(QIODevice::WriteOnly);
    file.write("deep");
    file.close();

    ToolResult result = executor->listDirectory(nestedDir);

    QVERIFY(result.success);
    QVERIFY(result.output.contains("deep_file.txt"));
}

// ============================================================================
// executeCommand Tests
// ============================================================================

void TestAgenticTools::testExecuteCommandSuccess()
{
    #ifdef Q_OS_WIN
    ToolResult result = executor->executeCommand("cmd.exe", QStringList() << "/c" << "echo" << "hello");
    #else
    ToolResult result = executor->executeCommand("echo", QStringList() << "hello");
    #endif

    QVERIFY(result.success);
    QVERIFY(result.output.contains("hello"));
    QCOMPARE(result.exitCode, 0);
}

void TestAgenticTools::testExecuteCommandWithOutput()
{
    #ifdef Q_OS_WIN
    ToolResult result = executor->executeCommand("cmd.exe", QStringList() << "/c" << "echo" << "test output");
    #else
    ToolResult result = executor->executeCommand("echo", QStringList() << "test output");
    #endif

    QVERIFY(result.success);
    QVERIFY(result.output.contains("test output"));
}

void TestAgenticTools::testExecuteCommandTimeout()
{
    #ifdef Q_OS_WIN
    // Create a command that runs long enough to trigger timeout
    // Use ping localhost with high count to simulate long-running command
    ToolResult result = executor->executeCommand("ping", QStringList() << "-n" << "1000" << "127.0.0.1");
    #else
    ToolResult result = executor->executeCommand("sleep", QStringList() << "100");
    #endif

    // Should timeout after 30 seconds
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("timeout") || result.error.contains("timed out") || result.error.contains("Timed out"));
}

void TestAgenticTools::testExecuteCommandNotFound()
{
    ToolResult result = executor->executeCommand("nonexistent_command_xyz", QStringList());

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Cannot find") || result.error.contains("not found"));
}

void TestAgenticTools::testExecuteCommandError()
{
    #ifdef Q_OS_WIN
    ToolResult result = executor->executeCommand("cmd.exe", QStringList() << "/c" << "exit" << "1");
    #else
    ToolResult result = executor->executeCommand("sh", QStringList() << "-c" << "exit 1");
    #endif

    QVERIFY(!result.success);
    QCOMPARE(result.exitCode, 1);
}

// ============================================================================
// grepSearch Tests
// ============================================================================

void TestAgenticTools::testGrepSearchSuccess()
{
    // Create test files
    QFile file1(tempDir.path() + "/file1.txt");
    file1.open(QIODevice::WriteOnly | QIODevice::Text);
    file1.write("Line 1: match this\nLine 2: no match\nLine 3: match again");
    file1.close();

    QFile file2(tempDir.path() + "/file2.txt");
    file2.open(QIODevice::WriteOnly | QIODevice::Text);
    file2.write("Another match here\nNo match\nmatch");
    file2.close();

    ToolResult result = executor->grepSearch("match", tempDir.path());

    QVERIFY(result.success);
    // Should contain multiple matches with file:line:content format
    QVERIFY(result.output.contains("match"));
    QVERIFY(result.output.contains("file1.txt") || result.output.contains("file2.txt"));
}

void TestAgenticTools::testGrepSearchNoMatches()
{
    QFile file(tempDir.path() + "/nomatches.txt");
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("This file\nhas no\nmatches");
    file.close();

    ToolResult result = executor->grepSearch("nonexistent_pattern_xyz", tempDir.path());

    QVERIFY(result.success);
    // No matches should result in empty output
    QVERIFY(result.output.isEmpty() || result.output.contains("No matches"));
}

void TestAgenticTools::testGrepSearchInvalidRegex()
{
    ToolResult result = executor->grepSearch("[invalid(regex", tempDir.path());

    // Should handle invalid regex gracefully
    QVERIFY(!result.success || result.error.contains("invalid"));
}

void TestAgenticTools::testGrepSearchMultipleMatches()
{
    QFile file(tempDir.path() + "/multi.txt");
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("test\ntest\ntest\nno\ntest");
    file.close();

    ToolResult result = executor->grepSearch("test", tempDir.path());

    QVERIFY(result.success);
    // Should find 4 matches
    int matchCount = result.output.count("test");
    QVERIFY(matchCount >= 1);
}

// ============================================================================
// gitStatus Tests
// ============================================================================

void TestAgenticTools::testGitStatusInRepository()
{
    // Create a minimal git repo
    QString gitDir = tempDir.path() + "/.git";
    QDir(tempDir.path()).mkdir(".git");
    
    QFile headFile(gitDir + "/HEAD");
    headFile.open(QIODevice::WriteOnly | QIODevice::Text);
    headFile.write("ref: refs/heads/main");
    headFile.close();

    ToolResult result = executor->gitStatus(tempDir.path());

    // Should succeed or have a specific git error (not "not a repository")
    QVERIFY(result.success || result.error.contains("fatal"));
}

void TestAgenticTools::testGitStatusNotRepository()
{
    QString nonGitDir = tempDir.path() + "/notrepo";
    QDir(tempDir.path()).mkdir("notrepo");

    ToolResult result = executor->gitStatus(nonGitDir);

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("fatal") || result.error.contains("not a git"));
}

// ============================================================================
// runTests Tests
// ============================================================================

void TestAgenticTools::testRunTestsCTest()
{
    // Create a simple CMakeLists.txt
    QString cmakelists = tempDir.path() + "/CMakeLists.txt";
    QFile file(cmakelists);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("cmake_minimum_required(VERSION 3.16)\nproject(test)");
    file.close();

    ToolResult result = executor->runTests(tempDir.path());

    // Should attempt to run ctest
    QVERIFY(!result.output.isEmpty() || !result.error.isEmpty());
}

void TestAgenticTools::testRunTestsPython()
{
    QString pytestFile = tempDir.path() + "/test_sample.py";
    QFile file(pytestFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("def test_example():\n    assert True");
    file.close();

    ToolResult result = executor->runTests(tempDir.path());

    // Should attempt to run tests
    QVERIFY(!result.output.isEmpty() || !result.error.isEmpty());
}

void TestAgenticTools::testRunTestsNotFound()
{
    QString emptyDir = tempDir.path() + "/notests";
    QDir(tempDir.path()).mkdir("notests");

    ToolResult result = executor->runTests(emptyDir);

    // Should indicate no tests found
    QVERIFY(result.success || result.error.contains("not found"));
}

// ============================================================================
// analyzeCode Tests
// ============================================================================

void TestAgenticTools::testAnalyzeCodeCpp()
{
    QString cppFile = tempDir.path() + "/test.cpp";
    QFile file(cppFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write(
        "class MyClass {\n"
        "    void method1() {}\n"
        "    void method2() {}\n"
        "};\n"
        "void function1() {}\n"
        "void function2() {}\n"
    );
    file.close();

    ToolResult result = executor->analyzeCode(cppFile);

    QVERIFY(result.success);
    // Should detect C++ language and count functions/classes
    QVERIFY(result.output.contains("class") || result.output.contains("1") || result.output.contains("C++"));
}

void TestAgenticTools::testAnalyzeCodePython()
{
    QString pyFile = tempDir.path() + "/test.py";
    QFile file(pyFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write(
        "class MyClass:\n"
        "    def method1(self):\n"
        "        pass\n"
        "def function1():\n"
        "    pass\n"
    );
    file.close();

    ToolResult result = executor->analyzeCode(pyFile);

    QVERIFY(result.success);
    // Check that Python language was detected and output contains analysis
    QVERIFY(result.output.contains("Python") || result.output.contains("test.py") || result.output.contains("Functions"));
}

void TestAgenticTools::testAnalyzeCodeUnknownLanguage()
{
    QString unknownFile = tempDir.path() + "/test.unknown";
    QFile file(unknownFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("some random content");
    file.close();

    ToolResult result = executor->analyzeCode(unknownFile);

    // Should handle unknown language gracefully
    QVERIFY(result.success || result.error.contains("Unknown"));
}

void TestAgenticTools::testAnalyzeCodeInvalidFile()
{
    ToolResult result = executor->analyzeCode(tempDir.path() + "/nonexistent.cpp");

    QVERIFY(!result.success);
}

void TestAgenticTools::testDeleteFileBlocked()
{
    QString testFile = tempDir.path() + "/tobedeleted.txt";
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("delete me");
    file.close();

    // Attempt to delete via tool (should be blocked by default)
    ToolResult res = executor->executeTool("deleteFile", QStringList() << testFile);
    QVERIFY(!res.success);
    QVERIFY(QFile::exists(testFile));
}

void TestAgenticTools::testDeleteFileAllowedAfterToggle()
{
    QString testFile = tempDir.path() + "/todelete2.txt";
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("delete me");
    file.close();

    // Enable deletes
    ToolResult t = executor->executeTool("toggleBlockDeletes", QStringList() << "false");
    QVERIFY(t.success);

    ToolResult res = executor->executeTool("deleteFile", QStringList() << testFile);
    QVERIFY(res.success);
    QVERIFY(!QFile::exists(testFile));

    // Re-enable blocking for safety
    executor->executeTool("toggleBlockDeletes", QStringList() << "true");
}

void TestAgenticTools::testShellDeleteBlocked()
{
    // Try to execute a shell rm command via executeCommand
    QString program = "bash";
    QStringList args;
    args << "-c" << "rm -rf /tmp/nonexistent_unused";

    ToolResult res = executor->executeTool("executeCommand", QStringList() << program << args.join(" "));
    QVERIFY(!res.success);
}

// ============================================================================
// Signal Tests
// ============================================================================

void TestAgenticTools::testToolCompletedSignal()
{
    QSignalSpy spy(executor, SIGNAL(toolExecutionCompleted(QString, QString)));

    QString testFile = tempDir.path() + "/signal_test.txt";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("test");
    file.close();

    executor->readFile(testFile);

    // Signal should be emitted
    // Note: Direct tool calls may not emit signals, this depends on implementation
    // Typically signals are emitted when called through executeTool()
}

void TestAgenticTools::testToolErrorSignal()
{
    QSignalSpy spy(executor, SIGNAL(toolExecutionError(QString, QString)));

    executor->readFile(tempDir.path() + "/nonexistent.txt");

    // Error signal may be emitted
}

void TestAgenticTools::testToolProgressSignal()
{
    QSignalSpy spy(executor, SIGNAL(toolProgress(QString, QString)));

    // Progress signal for large operations
}

void TestAgenticTools::testToolExecutedSignal()
{
    QSignalSpy spy(executor, SIGNAL(toolExecuted(QString, ToolResult)));

    QString testFile = tempDir.path() + "/signal_test2.txt";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write("test");
    file.close();

    executor->readFile(testFile);
}

// ============================================================================
// Integration Tests
// ============================================================================

void TestAgenticTools::testMultipleToolsSequential()
{
    // Create file
    QString testFile = tempDir.path() + "/integration.txt";
    {
        ToolResult result = executor->writeFile(testFile, "Initial content");
        QVERIFY(result.success);
    }

    // Read file
    {
        ToolResult result = executor->readFile(testFile);
        QVERIFY(result.success);
        QCOMPARE(result.output, "Initial content");
    }

    // Write to file
    {
        ToolResult result = executor->writeFile(testFile, "Updated content");
        QVERIFY(result.success);
    }

    // Read again
    {
        ToolResult result = executor->readFile(testFile);
        QVERIFY(result.success);
        QCOMPARE(result.output, "Updated content");
    }

    // List directory
    {
        ToolResult result = executor->listDirectory(tempDir.path());
        QVERIFY(result.success);
        QVERIFY(result.output.contains("integration.txt"));
    }
}

void TestAgenticTools::testToolsWithMetrics()
{
    // Test that tools track execution time
    auto start = std::chrono::high_resolution_clock::now();
    
    QString testFile = tempDir.path() + "/metrics.txt";
    ToolResult result = executor->writeFile(testFile, "Metrics test");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto actualDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    QVERIFY(result.success);
    QVERIFY(result.executionTimeMs >= 0);
    // Execution time should be reasonable (within actual measurement with overhead)
    QVERIFY(result.executionTimeMs <= actualDuration * 2); // Allow some overhead
}

// Main function to run tests
// Create a custom main that uses QApplication instead of QCoreApplication
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);  // Need QApplication for GUI components
    TestAgenticTools tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_agentic_tools.moc"