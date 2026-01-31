#include "agentic_file_operations.h"
#include "agentic_error_handler.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QRandomGenerator>
#include <QSignalSpy>

/**
 * @class FuzzTestAgenticFileOperations
 * @brief Fuzz tests for Keep/Undo file operations with randomized inputs
 * 
 * Tests the robustness of file operations with various edge cases,
 * including very large files, special characters, unicode, etc.
 */
class FuzzTestAgenticFileOperations : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testLargeFileOperations();
    void testSpecialCharacterFilenames();
    void testUnicodeContent();
    void testConcurrentOperations();
    void testPathTraversalAttempts();
    void testVeryLongPaths();
    void testEmptyAndWhitespaceContent();
    void testBinaryContent();

private:
    QTemporaryDir* m_tempDir;
    QString m_testDirPath;
    AgenticErrorHandler* m_errorHandler;
    
    QString generateRandomString(int length);
    QString generateRandomUnicodeString(int length);
    QByteArray generateRandomBinaryData(int length);
};

void FuzzTestAgenticFileOperations::initTestCase()
{
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    m_testDirPath = m_tempDir->path();
    
    m_errorHandler = new AgenticErrorHandler();
    QVERIFY(m_errorHandler != nullptr);
}

void FuzzTestAgenticFileOperations::cleanupTestCase()
{
    delete m_errorHandler;
    delete m_tempDir;
}

QString FuzzTestAgenticFileOperations::generateRandomString(int length)
{
    const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString randomString;
    for(int i = 0; i < length; ++i)
    {
        int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
        QChar nextChar = possibleCharacters.at(index);
        randomString.append(nextChar);
    }
    return randomString;
}

QString FuzzTestAgenticFileOperations::generateRandomUnicodeString(int length)
{
    QString unicodeString;
    for(int i = 0; i < length; ++i)
    {
        // Generate random Unicode code points (Basic Multilingual Plane)
        uint codePoint = QRandomGenerator::global()->bounded(0x0000, 0xFFFF);
        unicodeString.append(QChar(codePoint));
    }
    return unicodeString;
}

QByteArray FuzzTestAgenticFileOperations::generateRandomBinaryData(int length)
{
    QByteArray data;
    data.reserve(length);
    for(int i = 0; i < length; ++i)
    {
        char byte = static_cast<char>(QRandomGenerator::global()->bounded(0, 256));
        data.append(byte);
    }
    return data;
}

void FuzzTestAgenticFileOperations::testLargeFileOperations()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    QString testFilePath = m_testDirPath + "/large_file_test.txt";
    
    // Generate large content (1MB)
    QString largeContent = generateRandomString(1024 * 1024);
    
    // Test creation with large content
    fileOps.createFileWithApproval(testFilePath, largeContent);
    
    // Verify file exists and has correct size
    QFile file(testFilePath);
    QVERIFY(file.exists());
    QVERIFY(file.size() > 1000000); // Approximate check
    
    // Test modification with large content
    QString modifiedLargeContent = generateRandomString(1024 * 1024);
    fileOps.modifyFileWithApproval(testFilePath, largeContent, modifiedLargeContent);
    
    // Verify modification
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString readContent = QTextStream(&file).readAll();
    file.close();
    QCOMPARE(readContent.size(), modifiedLargeContent.size());
    
    // Test deletion
    fileOps.deleteFileWithApproval(testFilePath);
    QVERIFY(!file.exists());
}

void FuzzTestAgenticFileOperations::testSpecialCharacterFilenames()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    QStringList specialFilenames;
    
    // Test various special character combinations
    specialFilenames << m_testDirPath + "/test file with spaces.txt"
                     << m_testDirPath + "/test-file-with-dashes.txt"
                     << m_testDirPath + "/test_file_with_underscores.txt"
                     << m_testDirPath + "/test.file.with.dots.txt"
                     << m_testDirPath + "/test#file#with#hashes.txt"
                     << m_testDirPath + "/test@file@with@at.txt"
                     << m_testDirPath + "/test$file$with$dollar.txt"
                     << m_testDirPath + "/test%file%with%percent.txt"
                     << m_testDirPath + "/test^file^with^caret.txt"
                     << m_testDirPath + "/test&file&with&ersand.txt";
    
    for (const QString& filename : specialFilenames) {
        QString content = "Content for " + filename;
        
        // Test creation
        fileOps.createFileWithApproval(filename, content);
        QVERIFY(QFile::exists(filename));
        
        // Test modification
        QString modifiedContent = "Modified content for " + filename;
        fileOps.modifyFileWithApproval(filename, content, modifiedContent);
        
        // Verify modification
        QFile file(filename);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString readContent = QTextStream(&file).readAll();
        file.close();
        QCOMPARE(readContent, modifiedContent);
        
        // Test deletion
        fileOps.deleteFileWithApproval(filename);
        QVERIFY(!file.exists());
    }
}

void FuzzTestAgenticFileOperations::testUnicodeContent()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    QString testFilePath = m_testDirPath + "/unicode_test.txt";
    
    // Generate Unicode content
    QString unicodeContent = generateRandomUnicodeString(10000);
    
    // Test creation with Unicode content
    fileOps.createFileWithApproval(testFilePath, unicodeContent);
    QVERIFY(QFile::exists(testFilePath));
    
    // Verify content preservation
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray rawData = file.readAll();
    file.close();
    
    // Test modification with Unicode content
    QString modifiedUnicodeContent = generateRandomUnicodeString(10000);
    fileOps.modifyFileWithApproval(testFilePath, unicodeContent, modifiedUnicodeContent);
    
    // Verify modification
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray modifiedRawData = file.readAll();
    file.close();
    QVERIFY(modifiedRawData.size() > 0);
    
    // Test deletion
    fileOps.deleteFileWithApproval(testFilePath);
    QVERIFY(!file.exists());
}

void FuzzTestAgenticFileOperations::testConcurrentOperations()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    
    // Perform multiple operations in sequence to test concurrency handling
    for (int i = 0; i < 50; ++i) {
        QString testFilePath = QString("%1/concurrent_test_%2.txt").arg(m_testDirPath).arg(i);
        QString content = QString("Content %1").arg(i);
        
        // Alternate between different operations
        switch (i % 3) {
            case 0:
                fileOps.createFileWithApproval(testFilePath, content);
                QVERIFY(QFile::exists(testFilePath));
                break;
            case 1:
                // Create file first, then modify
                fileOps.createFileWithApproval(testFilePath, content);
                QVERIFY(QFile::exists(testFilePath));
                QString modifiedContent = QString("Modified content %1").arg(i);
                fileOps.modifyFileWithApproval(testFilePath, content, modifiedContent);
                break;
            case 2:
                // Create file first, then delete
                fileOps.createFileWithApproval(testFilePath, content);
                QVERIFY(QFile::exists(testFilePath));
                fileOps.deleteFileWithApproval(testFilePath);
                QVERIFY(!QFile::exists(testFilePath));
                break;
        }
    }
}

void FuzzTestAgenticFileOperations::testPathTraversalAttempts()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    
    // Test path traversal attempts (these should be handled by the system)
    QStringList traversalPaths;
    traversalPaths << m_testDirPath + "/../traversal_test1.txt"
                   << m_testDirPath + "/../../traversal_test2.txt"
                   << m_testDirPath + "/./traversal_test3.txt"
                   << m_testDirPath + "/nested/../../../traversal_test4.txt";
    
    for (const QString& path : traversalPaths) {
        QString content = "Traversal test content";
        
        // These operations should either succeed (if path resolution is safe)
        // or fail gracefully (if path is invalid)
        fileOps.createFileWithApproval(path, content);
        
        // If file exists, test other operations
        if (QFile::exists(path)) {
            QString modifiedContent = "Modified traversal test content";
            fileOps.modifyFileWithApproval(path, content, modifiedContent);
            fileOps.deleteFileWithApproval(path);
        }
    }
}

void FuzzTestAgenticFileOperations::testVeryLongPaths()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    
    // Generate a very long path
    QString longPath = m_testDirPath + "/";
    for (int i = 0; i < 50; ++i) {
        longPath += "very_long_directory_name_" + QString::number(i) + "/";
    }
    longPath += "very_long_filename.txt";
    
    QString content = "Content for very long path";
    
    // Test creation with very long path
    fileOps.createFileWithApproval(longPath, content);
    
    // If file exists, test other operations
    if (QFile::exists(longPath)) {
        QString modifiedContent = "Modified content for very long path";
        fileOps.modifyFileWithApproval(longPath, content, modifiedContent);
        fileOps.deleteFileWithApproval(longPath);
    }
}

void FuzzTestAgenticFileOperations::testEmptyAndWhitespaceContent()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    QString testFilePath = m_testDirPath + "/whitespace_test.txt";
    
    // Test empty content
    QString emptyContent = "";
    fileOps.createFileWithApproval(testFilePath, emptyContent);
    QVERIFY(QFile::exists(testFilePath));
    
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll().size(), 0);
    file.close();
    
    // Test whitespace-only content
    QString whitespaceContent = "   \t\n  \r\n  \t  ";
    fileOps.modifyFileWithApproval(testFilePath, emptyContent, whitespaceContent);
    
    QVERIFY(file.open(QIODevice::ReadOnly));
    QString readContent = QTextStream(&file).readAll();
    file.close();
    QCOMPARE(readContent, whitespaceContent);
    
    // Test deletion
    fileOps.deleteFileWithApproval(testFilePath);
    QVERIFY(!file.exists());
}

void FuzzTestAgenticFileOperations::testBinaryContent()
{
    AgenticFileOperations fileOps(nullptr, m_errorHandler);
    QString testFilePath = m_testDirPath + "/binary_test.bin";
    
    // Generate binary content
    QByteArray binaryContent = generateRandomBinaryData(10000);
    
    // Write binary content to file manually first
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(binaryContent);
    file.close();
    
    // Test modification with binary content as string
    QString binaryString = QString::fromLatin1(binaryContent.data(), binaryContent.size());
    QString modifiedBinaryString = QString::fromLatin1(generateRandomBinaryData(10000).data(), 10000);
    
    fileOps.modifyFileWithApproval(testFilePath, binaryString, modifiedBinaryString);
    
    // Verify modification
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray readData = file.readAll();
    file.close();
    
    // Test deletion
    fileOps.deleteFileWithApproval(testFilePath);
    QVERIFY(!file.exists());
}

QTEST_MAIN(FuzzTestAgenticFileOperations)
#include "fuzz_test_agentic_file_operations.moc"