// =============================================================================
// Qt Cross-Platform File Operations - Comprehensive Examples
// =============================================================================
// Status: Phase 1 Examples
// Demonstrates: Windows, Linux, macOS file operations in unified API
// =============================================================================

#pragma once

#include "qt_cross_platform_file_ops.hpp"
#include <QDebug>
#include <QCoreApplication>

// =============================================================================
// Example 1: Basic File I/O (Windows + POSIX unified)
// =============================================================================

void example1_basic_file_io() {
    qDebug() << "=== Example 1: Basic File I/O (Cross-Platform) ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    qDebug() << "Platform:" << file_ops->getPlatformName();
    
    // Write a file
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "test.txt";
    
    int fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        QByteArray data = "Hello from cross-platform Qt wrapper!";
        auto result = file_ops->writeFile(fd, data);
        qDebug() << "Write result:" << result.detail;
        file_ops->closeFile(fd);
    }
    
    // Read the file back
    fd = file_ops->openFile(test_file, O_RDONLY);
    if (fd >= 0) {
        QByteArray output;
        auto result = file_ops->readFile(fd, output, 1024);
        qDebug() << "Read result:" << result.detail;
        qDebug() << "Content:" << QString::fromUtf8(output);
        file_ops->closeFile(fd);
    }
}

// =============================================================================
// Example 2: Memory-Mapped File Access (POSIX-optimized)
// =============================================================================

void example2_memory_mapped_files() {
    qDebug() << "=== Example 2: Memory-Mapped Files ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    // Create a test file
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "large_file.bin";
    
    int fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        // Write 1 MB of test data
        QByteArray chunk(1024 * 1024, 'A');
        file_ops->writeFile(fd, chunk);
        file_ops->closeFile(fd);
    }
    
    // Map the file into memory
    MappedFile mapped = file_ops->mmapFile(test_file);
    if (mapped.is_valid) {
        qDebug() << "File mapped successfully, size:" << mapped.mapped_size;
        
        // Access data directly from mapped memory
        QByteArray content = file_ops->readMappedFile(mapped);
        qDebug() << "First 50 bytes:" << content.left(50);
        
        file_ops->unmapFile(mapped);
    }
}

// =============================================================================
// Example 3: Directory Enumeration with File Metadata
// =============================================================================

void example3_directory_enumeration() {
    qDebug() << "=== Example 3: Directory Enumeration ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    QString test_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                      file_ops->getPathSeparator() + "qt_test_dir";
    
    // Create test directory structure
    file_ops->createDirectoryRecursive(test_dir);
    file_ops->createDirectoryRecursive(test_dir + file_ops->getPathSeparator() + "subdir");
    
    // Create some test files
    for (int i = 0; i < 3; ++i) {
        QString filename = test_dir + file_ops->getPathSeparator() + 
                          QString("test_%1.txt").arg(i);
        int fd = file_ops->openFile(filename, O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            QByteArray data = QString("Test file %1").arg(i).toUtf8();
            file_ops->writeFile(fd, data);
            file_ops->closeFile(fd);
        }
    }
    
    // List directory contents
    auto entries = file_ops->listDirectory(test_dir);
    qDebug() << "Directory listing for" << test_dir << ":";
    
    for (const auto& entry : entries) {
        if (entry.filename == "." || entry.filename == "..") {
            continue;
        }
        
        qDebug() << "  Name:" << entry.filename
                << "| Dir:" << entry.is_directory
                << "| Size:" << entry.size
                << "| Full:" << entry.full_path;
    }
}

// =============================================================================
// Example 4: File Metadata and Permissions
// =============================================================================

void example4_file_metadata() {
    qDebug() << "=== Example 4: File Metadata & Permissions ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "metadata_test.txt";
    
    // Create file
    int fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT, 0644);
    if (fd >= 0) {
        QByteArray data = "Test data for metadata";
        file_ops->writeFile(fd, data);
        file_ops->closeFile(fd);
    }
    
    // Get file info
    auto info = file_ops->getFileInfo(test_file);
    
    qDebug() << "File:" << test_file;
    qDebug() << "  Exists:" << info.exists;
    qDebug() << "  Size:" << info.size;
    qDebug() << "  Readable:" << info.is_readable;
    qDebug() << "  Writable:" << info.is_writable;
    qDebug() << "  Executable:" << info.is_executable;
    qDebug() << "  Modified (Unix timestamp):" << info.modified_time;
    
    // Change permissions (POSIX systems)
    if (!file_ops->isWindowsPlatform()) {
        file_ops->changePermissions(test_file, 0755);
        info = file_ops->getFileInfo(test_file);
        qDebug() << "  After chmod 0755 - Executable:" << info.is_executable;
    }
}

// =============================================================================
// Example 5: Large File Reading with Progress Tracking
// =============================================================================

void example5_large_file_streaming() {
    qDebug() << "=== Example 5: Large File Streaming ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    auto large_reader = new QtLargeFileReader(file_ops);
    
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "streaming_test.bin";
    
    // Create a 5 MB test file
    int fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        QByteArray chunk(512 * 1024, 'X');
        for (int i = 0; i < 10; ++i) {
            file_ops->writeFile(fd, chunk);
        }
        file_ops->closeFile(fd);
    }
    
    // Stream read with progress callback
    QByteArray full_content;
    qint64 chunks_received = 0;
    
    auto on_chunk = [&](const QByteArray& chunk) {
        chunks_received++;
        qDebug() << "Received chunk" << chunks_received << "size:" << chunk.size();
    };
    
    auto result = large_reader->readLargeFileStreaming(test_file, on_chunk, 256 * 1024);
    qDebug() << "Stream read result:" << result.detail;
    qDebug() << "Total chunks:" << chunks_received;
    
    delete large_reader;
}

// =============================================================================
// Example 6: Batch File Operations
// =============================================================================

void example6_batch_operations() {
    qDebug() << "=== Example 6: Batch File Operations ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    auto batch_ops = new QtBatchFileOps(file_ops);
    
    QString base_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                      file_ops->getPathSeparator() + "batch_test";
    
    QString src_dir = base_dir + file_ops->getPathSeparator() + "src";
    QString dst_dir = base_dir + file_ops->getPathSeparator() + "dst";
    
    file_ops->createDirectoryRecursive(src_dir);
    file_ops->createDirectoryRecursive(dst_dir);
    
    // Create test files in source
    for (int i = 0; i < 5; ++i) {
        QString filename = src_dir + file_ops->getPathSeparator() + 
                          QString("file_%1.txt").arg(i);
        int fd = file_ops->openFile(filename, O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            QByteArray data = QString("File %1 content").arg(i).toUtf8();
            file_ops->writeFile(fd, data);
            file_ops->closeFile(fd);
        }
    }
    
    // Copy entire directory
    bool copy_success = batch_ops->copyDirectoryRecursive(src_dir, dst_dir + file_ops->getPathSeparator() + "copy");
    qDebug() << "Directory copy success:" << copy_success;
    
    // Calculate checksum
    QString test_file = src_dir + file_ops->getPathSeparator() + "file_0.txt";
    QString md5 = batch_ops->calculateFileChecksum(test_file, "MD5");
    qDebug() << "MD5 checksum:" << md5;
    
    delete batch_ops;
}

// =============================================================================
// Example 7: Cross-Platform Path Handling
// =============================================================================

void example7_path_handling() {
    qDebug() << "=== Example 7: Cross-Platform Path Handling ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    QString unix_path = "/home/user/documents/file.txt";
    QString windows_path = "C:\\Users\\User\\Documents\\file.txt";
    
    qDebug() << "Platform:" << file_ops->getPlatformName();
    qDebug() << "Path separator:" << file_ops->getPathSeparator();
    
    // Convert to native path
    QString native_unix = file_ops->convertPathToNative(unix_path);
    QString native_windows = file_ops->convertPathToNative(windows_path);
    
    qDebug() << "Unix path converted:" << native_unix;
    qDebug() << "Windows path converted:" << native_windows;
}

// =============================================================================
// Example 8: File Seeking and Position Tracking
// =============================================================================

void example8_file_seeking() {
    qDebug() << "=== Example 8: File Seeking ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "seek_test.txt";
    
    // Create file with structured data
    int fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        QByteArray data = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        file_ops->writeFile(fd, data);
        file_ops->closeFile(fd);
    }
    
    // Open for reading and seek
    fd = file_ops->openFile(test_file, O_RDONLY);
    if (fd >= 0) {
        // Seek to position 10
        qint64 pos = file_ops->seekFile(fd, 10, SEEK_SET);
        qDebug() << "Seeked to position:" << pos;
        
        // Read from position
        QByteArray buffer;
        file_ops->readFile(fd, buffer, 5);
        qDebug() << "Read from position 10:" << QString::fromUtf8(buffer);
        
        // Get current position
        qint64 current_pos = file_ops->tellFile(fd);
        qDebug() << "Current position:" << current_pos;
        
        file_ops->closeFile(fd);
    }
}

// =============================================================================
// Example 9: Platform-Specific Operations Check
// =============================================================================

void example9_platform_checks() {
    qDebug() << "=== Example 9: Platform-Specific Checks ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    qDebug() << "Platform detection results:";
    qDebug() << "  Windows:" << file_ops->isWindowsPlatform();
    qDebug() << "  Linux:" << file_ops->isLinuxPlatform();
    qDebug() << "  macOS:" << file_ops->isMacOSPlatform();
    qDebug() << "  Supported:" << file_ops->isPlatformSupported();
    
    // Conditional code example
    if (file_ops->isWindowsPlatform()) {
        qDebug() << "Running Windows-specific code path";
    } else if (file_ops->isLinuxPlatform()) {
        qDebug() << "Running Linux-specific code path";
    } else if (file_ops->isMacOSPlatform()) {
        qDebug() << "Running macOS-specific code path";
    }
}

// =============================================================================
// Example 10: Error Handling and Recovery
// =============================================================================

void example10_error_handling() {
    qDebug() << "=== Example 10: Error Handling ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    // Attempt to open non-existent file
    int fd = file_ops->openFile("/nonexistent/path/file.txt", O_RDONLY);
    if (fd < 0) {
        qDebug() << "File open failed as expected for non-existent path";
    }
    
    // Create file safely
    QString test_file = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                       file_ops->getPathSeparator() + "error_test.txt";
    
    fd = file_ops->openFile(test_file, O_WRONLY | O_CREAT, 0644);
    if (fd >= 0) {
        QByteArray data = "Test data";
        auto write_result = file_ops->writeFile(fd, data);
        
        if (write_result.success) {
            qDebug() << "Write succeeded:" << write_result.detail;
        } else {
            qDebug() << "Write failed:" << write_result.detail
                    << "Error code:" << write_result.errorCode;
        }
        
        file_ops->closeFile(fd);
    }
}

// =============================================================================
// Example 11: Qt Integration and Signals
// =============================================================================

class CrossPlatformFileExample : public QObject {
    Q_OBJECT
    
public:
    explicit CrossPlatformFileExample(QObject* parent = nullptr)
        : QObject(parent), m_file_ops(nullptr) {
        
        m_file_ops = QtCrossPlatformFileOpsGlobal::instance();
        
        // Connect signals
        connect(m_file_ops, &QtCrossPlatformFileOps::platformDetected,
                this, &CrossPlatformFileExample::onPlatformDetected);
    }
    
public slots:
    void onPlatformDetected(const QString& platform_name) {
        qDebug() << "Platform detected:" << platform_name;
    }
    
private:
    QtCrossPlatformFileOps* m_file_ops;
};

void example11_qt_integration() {
    qDebug() << "=== Example 11: Qt Integration ===";
    
    CrossPlatformFileExample example;
    // Signals already connected
}

// =============================================================================
// Example 12: Recursive Directory Processing
// =============================================================================

void example12_recursive_directory() {
    qDebug() << "=== Example 12: Recursive Directory Processing ===";
    
    auto file_ops = QtCrossPlatformFileOpsGlobal::instance();
    
    QString base_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                      file_ops->getPathSeparator() + "recursive_test";
    
    // Create nested structure
    file_ops->createDirectoryRecursive(base_dir + "/a/b/c");
    file_ops->createDirectoryRecursive(base_dir + "/a/d");
    
    // Function to recursively process directories
    std::function<void(const QString&, int)> process_dir = 
        [&](const QString& path, int depth) {
            auto entries = file_ops->listDirectory(path);
            
            for (const auto& entry : entries) {
                if (entry.filename == "." || entry.filename == "..") {
                    continue;
                }
                
                QString indent = QString("  ").repeated(depth);
                if (entry.is_directory) {
                    qDebug() << indent << "[DIR]" << entry.filename;
                    process_dir(entry.full_path, depth + 1);
                } else {
                    qDebug() << indent << "[FILE]" << entry.filename 
                            << "(" << entry.size << "bytes)";
                }
            }
        };
    
    process_dir(base_dir, 0);
}

#endif // QT_CROSS_PLATFORM_FILE_OPS_EXAMPLES_HPP
