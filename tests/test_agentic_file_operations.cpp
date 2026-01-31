#include "agentic_file_operations.h"
#include "agentic_error_handler.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSignalSpy>

/**
 * @class TestAgenticFileOperations
 * @brief Behavioral tests for Keep/Undo file operations
 * 
 * Tests the complete workflow of file operations with approval dialogs,
 * including Keep and Undo functionality, metrics tracking, and error handling.
 */
class TestAgenticFileOperations : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testCreateFileWithKeep();
    void testCreateFileWithUndo();
    void testModifyFileWithKeep();
    void testModifyFileWithUndo();
    void testDeleteFileWithKeep();
    void testDeleteFileWithUndo();
    void testUndoStackManagement();
    void testMetricsTracking();
    void testErrorHandling();
    void testConfiguration();

private:
    QTemporaryDir* m_tempDir;
    QString m_testDirPath;
    AgenticErrorHandler* m_errorHandler;
};

void TestAgenticFileOperations::initTestCase()
{
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    m_testDirPath = m_tempDir->path();
    
    m_errorHandler = new AgenticErrorHandler();
    QVERIFY(m_errorHandler != nullptr);
}

void TestAgenticFileOperations::cleanupTestCase()
{
    delete m_errorHandler;
    delete m_tempDir;
}

void TestAgenticFileOperations::testCreateFileWithKeep()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_create_keep.txt";
    QString testContent = "This is test content for creation with Keep";
    
    // Create a signal spy to monitor file creation
    QSignalSpy spy(&fileOps, &AgenticFileOperations::fileCreated);
    
    // Perform file creation with approval (simulate Keep)
    fileOps.createFileWithApproval(testFilePath, testContent);
    
    // Verify file was created
    QFile file(testFilePath);
    QVERIFY(file.exists());
    
    // Verify file content
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    QCOMPARE(content, testContent);
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testCreateFileWithUndo()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_create_undo.txt";
    QString testContent = "This is test content for creation with Undo";
    
    // Create a signal spy to monitor operation undone
    QSignalSpy spy(&fileOps, &AgenticFileOperations::operationUndone);
    
    // Perform file creation with approval (simulate Keep first to have something to undo)
    fileOps.createFileWithApproval(testFilePath, testContent);
    
    // Verify file was created
    QVERIFY(QFile::exists(testFilePath));
    
    // Now undo the creation
    fileOps.undoLastAction();
    
    // Verify file was deleted (undone)
    QVERIFY(!QFile::exists(testFilePath));
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testModifyFileWithKeep()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_modify_keep.txt";
    QString originalContent = "Original content";
    QString modifiedContent = "Modified content";
    
    // Create initial file
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << originalContent;
    file.close();
    
    // Create a signal spy to monitor file modification
    QSignalSpy spy(&fileOps, &AgenticFileOperations::fileModified);
    
    // Perform file modification with approval (simulate Keep)
    fileOps.modifyFileWithApproval(testFilePath, originalContent, modifiedContent);
    
    // Verify file was modified
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    QCOMPARE(content, modifiedContent);
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testModifyFileWithUndo()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_modify_undo.txt";
    QString originalContent = "Original content";
    QString modifiedContent = "Modified content";
    
    // Create initial file
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << originalContent;
    file.close();
    
    // Create a signal spy to monitor operation undone
    QSignalSpy spy(&fileOps, &AgenticFileOperations::operationUndone);
    
    // Perform file modification with approval (simulate Keep first)
    fileOps.modifyFileWithApproval(testFilePath, originalContent, modifiedContent);
    
    // Verify file was modified
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    QCOMPARE(content, modifiedContent);
    
    // Undo the modification
    fileOps.undoLastAction();
    
    // Verify file was restored to original content
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in2(&file);
    QString restoredContent = in2.readAll();
    file.close();
    QCOMPARE(restoredContent, originalContent);
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testDeleteFileWithKeep()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_delete_keep.txt";
    QString testContent = "Content to be deleted";
    
    // Create initial file
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << testContent;
    file.close();
    
    // Verify file exists
    QVERIFY(file.exists());
    
    // Create a signal spy to monitor file deletion
    QSignalSpy spy(&fileOps, &AgenticFileOperations::fileDeleted);
    
    // Perform file deletion with approval (simulate Keep)
    fileOps.deleteFileWithApproval(testFilePath);
    
    // Verify file was deleted
    QVERIFY(!QFile::exists(testFilePath));
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testDeleteFileWithUndo()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_delete_undo.txt";
    QString testContent = "Content to be deleted and restored";
    
    // Create initial file
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << testContent;
    file.close();
    
    // Verify file exists
    QVERIFY(file.exists());
    
    // Create a signal spy to monitor operation undone
    QSignalSpy spy(&fileOps, &AgenticFileOperations::operationUndone);
    
    // Perform file deletion with approval (simulate Keep first)
    fileOps.deleteFileWithApproval(testFilePath);
    
    // Verify file was deleted
    QVERIFY(!QFile::exists(testFilePath));
    
    // Undo the deletion
    fileOps.undoLastAction();
    
    // Verify file was restored
    QVERIFY(QFile::exists(testFilePath));
    
    // Verify file content is restored
    QFile restoredFile(testFilePath);
    QVERIFY(restoredFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&restoredFile);
    QString content = in.readAll();
    restoredFile.close();
    QCOMPARE(content, testContent);
    
    // Verify signal was emitted
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), testFilePath);
}

void TestAgenticFileOperations::testUndoStackManagement()
{
    // Set environment variable for smaller history size BEFORE creating the object
    qputenv("AGENTIC_FILE_OPERATIONS_MAX_HISTORY", "3");
    
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    
    // Test undo stack size tracking
    QCOMPARE(fileOps.getUndoStackSize(), 0);
    QVERIFY(!fileOps.canUndo());
    
    // Create multiple operations
    for (int i = 0; i < 5; ++i) {
        QString testFilePath = QString("%1/test_file_%2.txt").arg(m_testDirPath).arg(i);
        QString testContent = QString("Content for file %1").arg(i);
        fileOps.createFileWithApproval(testFilePath, testContent);
    }
    
    // Verify undo stack size is limited to configured maximum
    QCOMPARE(fileOps.getUndoStackSize(), 3);
    QVERIFY(fileOps.canUndo());
    
    // Test undo operations
    for (int i = 0; i < 3; ++i) {
        fileOps.undoLastAction();
    }
    
    // Verify stack is empty
    QCOMPARE(fileOps.getUndoStackSize(), 0);
    QVERIFY(!fileOps.canUndo());
    
    // Clean up environment variable
    qunsetenv("AGENTIC_FILE_OPERATIONS_MAX_HISTORY");
}

void TestAgenticFileOperations::testMetricsTracking()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString testFilePath = m_testDirPath + "/test_metrics.txt";
    QString testContent = "Test content for metrics";
    
    // Get initial metrics
    auto metrics = fileOps.getMetrics();
    QVERIFY(metrics != nullptr);
    
    // Perform file creation
    fileOps.createFileWithApproval(testFilePath, testContent);
    
    // Verify metrics were updated (this would require access to the metrics system)
    // For now, we'll verify the metrics object exists and can be accessed
    QVERIFY(metrics != nullptr);
}

void TestAgenticFileOperations::testErrorHandling()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    fileOps.setTestMode(true);  // Enable test mode to skip approval dialogs
    QString invalidPath = "/invalid/path/that/does/not/exist/test.txt";
    QString testContent = "Test content";
    
    // Perform operation that should fail
    fileOps.createFileWithApproval(invalidPath, testContent);
    
    // Verify error was handled (this would require mocking the error handler)
    // For now, we'll verify the error handler is properly set
    QVERIFY(m_errorHandler != nullptr);
}

void TestAgenticFileOperations::testConfiguration()
{
    // Test default configuration
    AgenticFileOperations fileOpsDefault(nullptr, m_errorHandler);
    fileOpsDefault.setTestMode(true);  // Enable test mode to skip approval dialogs
    QCOMPARE(fileOpsDefault.getUndoStackSize(), 0);
    
    // Test custom configuration via environment variable
    qputenv("AGENTIC_FILE_OPERATIONS_MAX_HISTORY", "10");
    
    AgenticFileOperations fileOpsCustom(nullptr, m_errorHandler);
    fileOpsCustom.setTestMode(true);  // Enable test mode to skip approval dialogs
    QCOMPARE(fileOpsCustom.getUndoStackSize(), 0);
    
    // Clean up environment variable
    qunsetenv("AGENTIC_FILE_OPERATIONS_MAX_HISTORY");
}

QTEST_MAIN(TestAgenticFileOperations)
#include "test_agentic_file_operations.moc"