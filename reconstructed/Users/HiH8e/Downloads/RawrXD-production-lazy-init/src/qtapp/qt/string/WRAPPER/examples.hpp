// =============================================================================
// Qt String Wrapper MASM - Usage Examples
// =============================================================================
// Purpose: 15+ complete, copy-paste ready examples for all major use cases
// Author: AI Toolkit / GitHub Copilot
// Date: December 29, 2025
// Status: Production Ready
// =============================================================================

#pragma once

#include "qt_string_wrapper_masm.hpp"
#include <QDebug>

// =============================================================================
// Example 1: Basic String Creation and Manipulation
// =============================================================================

void example_1_basic_string_operations() {
    qDebug() << "=== Example 1: Basic String Operations ===";
    
    QtStringWrapperMASM wrapper;
    
    // Create a string
    QString text = "Hello, World!";
    qDebug() << "Created string:" << text;
    
    // Get length
    void* qstring = wrapper_qstring_create();
    if (qstring) {
        // Append text
        auto result = wrapper.appendToString(qstring, text);
        if (result.success) {
            qDebug() << "Appended successfully, length:" << result.length;
        }
        
        // Get length via MASM
        uint32_t length = wrapper.getStringLength(qstring);
        qDebug() << "String length from MASM:" << length;
        
        // Clean up
        wrapper.deleteString(qstring);
    }
}

// =============================================================================
// Example 2: String Encoding Conversions
// =============================================================================

void example_2_encoding_conversions() {
    qDebug() << "=== Example 2: Encoding Conversions ===";
    
    QtStringWrapperMASM wrapper;
    void* qstring = wrapper_qstring_create();
    
    // Create string with UTF-8 text
    QString original = "Café ☕ Unicode Test";
    auto result = wrapper.appendToString(qstring, original);
    
    if (result.success) {
        // Convert to UTF-8 buffer
        char utf8_buffer[512] = {0};
        QtStringOpResult utf8_result = wrapper_qstring_to_utf8(qstring, utf8_buffer, sizeof(utf8_buffer) - 1);
        
        if (utf8_result.success) {
            qDebug() << "UTF-8 bytes:" << utf8_buffer;
            qDebug() << "UTF-8 length:" << utf8_result.length;
        }
        
        // Convert from UTF-8
        QString another = "New String";
        QByteArray utf8_data = another.toUtf8();
        wrapper_qstring_from_utf8(qstring, utf8_data.constData(), utf8_data.length());
    }
    
    wrapper.deleteString(qstring);
}

// =============================================================================
// Example 3: String Searching and Matching
// =============================================================================

void example_3_string_search() {
    qDebug() << "=== Example 3: String Search ===";
    
    QtStringWrapperMASM wrapper;
    void* qstring = wrapper_qstring_create();
    
    QString haystack = "The quick brown fox jumps over the lazy dog";
    wrapper.appendToString(qstring, haystack);
    
    // Search for substring
    QString needle = "brown fox";
    auto find_result = wrapper.findInString(qstring, needle);
    
    if (find_result.success) {
        qDebug() << "Found at position:" << find_result.length;
    } else {
        qDebug() << "Not found";
    }
    
    // String comparison
    void* qstring2 = wrapper_qstring_create();
    wrapper.appendToString(qstring2, "The quick brown fox jumps over the lazy dog");
    
    int cmp = wrapper.compareStrings(qstring, qstring2);
    qDebug() << "Comparison result:" << (cmp == 0 ? "Equal" : "Different");
    
    wrapper.deleteString(qstring);
    wrapper.deleteString(qstring2);
}

// =============================================================================
// Example 4: Byte Array Creation and Management
// =============================================================================

void example_4_byte_array_operations() {
    qDebug() << "=== Example 4: Byte Array Operations ===";
    
    QtStringWrapperMASM wrapper;
    void* qba = wrapper.createByteArray(256);
    
    if (qba) {
        // Append data
        QByteArray data = "Binary Data: \x00\x01\x02\x03";
        auto result = wrapper.appendToByteArray(qba, data);
        
        if (result.success) {
            qDebug() << "Appended" << result.length << "bytes";
        }
        
        // Get length
        uint32_t length = wrapper.getByteArrayLength(qba);
        qDebug() << "Byte array length:" << length;
        
        // Get byte at position
        if (length > 0) {
            uint8_t first = wrapper.getByteAt(qba, 0);
            qDebug() << "First byte:" << (int)first;
        }
        
        // Clean up
        wrapper.deleteByteArray(qba);
    }
}

// =============================================================================
// Example 5: Byte Array Hex Conversion
// =============================================================================

void example_5_hex_conversion() {
    qDebug() << "=== Example 5: Hex Conversion ===";
    
    QtStringWrapperMASM wrapper;
    void* qba = wrapper.createByteArray(128);
    
    // Append some binary data
    QByteArray binary_data = QByteArray::fromHex("48656C6C6F");  // "Hello"
    wrapper.appendToByteArray(qba, binary_data);
    
    // Convert to hex string
    char hex_output[256] = {0};
    QtStringOpResult hex_result = wrapper_qbytearray_to_hex(qba, hex_output, sizeof(hex_output) - 1);
    
    if (hex_result.success) {
        qDebug() << "Hex representation:" << hex_output;
    }
    
    wrapper.deleteByteArray(qba);
}

// =============================================================================
// Example 6: File Creation and Basic Operations
// =============================================================================

void example_6_file_operations() {
    qDebug() << "=== Example 6: File Operations ===";
    
    QtStringWrapperMASM wrapper;
    
    // Create file handle
    QString filepath = wrapper.generateTempFilePath("qt_test");
    QtFileHandle* file = wrapper.createFileHandle(filepath);
    
    if (file) {
        // Open for writing
        auto open_result = wrapper.openFile(file, filepath, FileOpenMode::Write);
        
        if (open_result.success) {
            qDebug() << "File opened for writing";
            
            // Write data
            QByteArray data = "Hello from MASM wrapper!\n";
            auto write_result = wrapper.writeFile(file, data);
            
            if (write_result.success) {
                qDebug() << "Written" << write_result.length << "bytes";
            }
            
            // Close file
            wrapper.closeFile(file);
        }
        
        // Clean up
        wrapper.deleteFileHandle(file);
    }
}

// =============================================================================
// Example 7: File Reading Operations
// =============================================================================

void example_7_file_reading() {
    qDebug() << "=== Example 7: File Reading ===";
    
    QtStringWrapperMASM wrapper;
    
    // Create temp file first
    QString filepath = wrapper.generateTempFilePath("qt_read");
    QtFileHandle* write_file = wrapper.createFileHandle(filepath);
    
    if (write_file) {
        wrapper.openFile(write_file, filepath, FileOpenMode::Write);
        QByteArray write_data = "Test file content";
        wrapper.writeFile(write_file, write_data);
        wrapper.closeFile(write_file);
        wrapper.deleteFileHandle(write_file);
    }
    
    // Now read the file
    QtFileHandle* read_file = wrapper.createFileHandle(filepath);
    if (read_file) {
        wrapper.openFile(read_file, filepath, FileOpenMode::Read);
        
        // Read all at once
        QByteArray file_content = wrapper.readFileAll(read_file);
        qDebug() << "File content:" << QString::fromUtf8(file_content);
        
        // Or read in chunks
        QByteArray chunk;
        auto read_result = wrapper.readFile(read_file, chunk, 1024);
        if (read_result.success) {
            qDebug() << "Read" << read_result.length << "bytes in chunk";
        }
        
        wrapper.closeFile(read_file);
        wrapper.deleteFileHandle(read_file);
    }
}

// =============================================================================
// Example 8: File Seek and Tell Operations
// =============================================================================

void example_8_file_seek() {
    qDebug() << "=== Example 8: File Seek and Tell ===";
    
    QtStringWrapperMASM wrapper;
    QString filepath = wrapper.generateTempFilePath("qt_seek");
    
    QtFileHandle* file = wrapper.createFileHandle(filepath);
    if (file) {
        wrapper.openFile(file, filepath, FileOpenMode::Write);
        
        // Write sample data
        QByteArray data = "0123456789";
        wrapper.writeFile(file, data);
        
        // Seek to position 5
        int64_t seek_result = wrapper.seekFile(file, 5);
        qDebug() << "Seeked to position:" << seek_result;
        
        // Get current position
        int64_t current_pos = wrapper.tellFile(file);
        qDebug() << "Current position:" << current_pos;
        
        wrapper.closeFile(file);
        wrapper.deleteFileHandle(file);
    }
}

// =============================================================================
// Example 9: File Existence and Size Checks
// =============================================================================

void example_9_file_info() {
    qDebug() << "=== Example 9: File Info ===";
    
    QtStringWrapperMASM wrapper;
    QString filepath = wrapper.generateTempFilePath("qt_info");
    
    // Create a file
    QtFileHandle* file = wrapper.createFileHandle(filepath);
    if (file) {
        wrapper.openFile(file, filepath, FileOpenMode::Write);
        QByteArray data = "Some content";
        wrapper.writeFile(file, data);
        wrapper.closeFile(file);
        
        // Check existence
        bool exists = wrapper.fileExists(filepath);
        qDebug() << "File exists:" << exists;
        
        // Get size
        int64_t size = wrapper.getFileSize(file);
        qDebug() << "File size:" << size;
        
        wrapper.deleteFileHandle(file);
    }
}

// =============================================================================
// Example 10: File Rename and Remove
// =============================================================================

void example_10_file_rename_remove() {
    qDebug() << "=== Example 10: File Rename and Remove ===";
    
    QtStringWrapperMASM wrapper;
    
    QString filepath1 = wrapper.generateTempFilePath("qt_original");
    QString filepath2 = wrapper.generateTempFilePath("qt_renamed");
    
    // Create original file
    QtFileHandle* file = wrapper.createFileHandle(filepath1);
    if (file) {
        wrapper.openFile(file, filepath1, FileOpenMode::Write);
        QByteArray data = "Original name";
        wrapper.writeFile(file, data);
        wrapper.closeFile(file);
        wrapper.deleteFileHandle(file);
    }
    
    // Rename it
    bool rename_success = wrapper.renameFile(filepath1, filepath2);
    qDebug() << "Rename successful:" << rename_success;
    
    // Remove it
    bool remove_success = wrapper.removeFile(filepath2);
    qDebug() << "Remove successful:" << remove_success;
}

// =============================================================================
// Example 11: Multiple String Operations in Sequence
// =============================================================================

void example_11_string_pipeline() {
    qDebug() << "=== Example 11: String Pipeline ===";
    
    QtStringWrapperMASM wrapper;
    
    void* s1 = wrapper_qstring_create();
    void* s2 = wrapper_qstring_create();
    
    // Build strings through pipeline
    wrapper.appendToString(s1, "Hello");
    wrapper.appendToString(s1, " ");
    wrapper.appendToString(s1, "World");
    
    // Copy to second string
    QString content = wrapper.getStringAsQString(s1);
    wrapper.appendToString(s2, content);
    
    // Verify both are same
    int cmp = wrapper.compareStrings(s1, s2);
    qDebug() << "Strings equal:" << (cmp == 0);
    
    // Replace operation
    wrapper.replaceInString(s1, "World", "MASM");
    QString updated = wrapper.getStringAsQString(s1);
    qDebug() << "Updated string:" << updated;
    
    wrapper.deleteString(s1);
    wrapper.deleteString(s2);
}

// =============================================================================
// Example 12: Batch Byte Array Operations
// =============================================================================

void example_12_byte_batch() {
    qDebug() << "=== Example 12: Batch Byte Array Operations ===";
    
    QtStringWrapperMASM wrapper;
    void* qba = wrapper.createByteArray(512);
    
    // Append multiple chunks
    for (int i = 0; i < 5; ++i) {
        QByteArray chunk = QString("Chunk %1 ").arg(i).toUtf8();
        wrapper.appendToByteArray(qba, chunk);
    }
    
    // Get final content
    QByteArray result = wrapper.getByteArrayAsQByteArray(qba);
    qDebug() << "Final content:" << QString::fromUtf8(result);
    qDebug() << "Total length:" << result.length();
    
    wrapper.deleteByteArray(qba);
}

// =============================================================================
// Example 13: Error Handling and Recovery
// =============================================================================

void example_13_error_handling() {
    qDebug() << "=== Example 13: Error Handling ===";
    
    QtStringWrapperMASM wrapper;
    
    // Try invalid operations
    auto result = wrapper.appendToString(nullptr, "test");
    if (!result.success) {
        qDebug() << "Error code:" << result.error_code;
        qDebug() << "Error message:" << wrapper.getLastError();
    }
    
    // Recover and continue
    void* qstring = wrapper_qstring_create();
    if (qstring) {
        auto valid_result = wrapper.appendToString(qstring, "Valid operation");
        if (valid_result.success) {
            qDebug() << "Recovery successful";
        }
        wrapper.deleteString(qstring);
    }
}

// =============================================================================
// Example 14: Performance Monitoring
// =============================================================================

void example_14_statistics() {
    qDebug() << "=== Example 14: Statistics and Monitoring ===";
    
    QtStringWrapperMASM wrapper;
    
    // Clear statistics
    wrapper.clearStatistics();
    
    // Perform operations
    void* qstring = wrapper_qstring_create();
    for (int i = 0; i < 100; ++i) {
        wrapper.appendToString(qstring, QString("Operation %1").arg(i));
    }
    wrapper.deleteString(qstring);
    
    // Get statistics
    QtStringStatistics stats = wrapper.getStatistics();
    qDebug() << "Total allocations:" << stats.total_allocations;
    qDebug() << "Total deallocations:" << stats.total_deallocations;
    qDebug() << "Current allocated:" << stats.current_allocated;
    qDebug() << "String conversions:" << stats.string_conversions;
    qDebug() << "File operations:" << stats.file_operations;
}

// =============================================================================
// Example 15: Temp Directory Utilities
// =============================================================================

void example_15_temp_utilities() {
    qDebug() << "=== Example 15: Temp Utilities ===";
    
    QtStringWrapperMASM wrapper;
    
    // Get temp directory
    QString temp_dir = wrapper.getTempDirectory();
    qDebug() << "Temp directory:" << temp_dir;
    
    // Generate multiple temp paths
    for (int i = 0; i < 3; ++i) {
        QString temp_file = wrapper.generateTempFilePath(QString("test_%1").arg(i));
        qDebug() << "Temp file" << i << ":" << temp_file;
    }
    
    // Create and cleanup temp files
    QString temp = wrapper.generateTempFilePath("cleanup_test");
    QtFileHandle* file = wrapper.createFileHandle(temp);
    if (file) {
        wrapper.openFile(file, temp, FileOpenMode::Write);
        wrapper.writeFile(file, QByteArray("temp"));
        wrapper.closeFile(file);
        wrapper.deleteFileHandle(file);
        
        // Cleanup
        wrapper.removeFile(temp);
        qDebug() << "Temp file cleaned up";
    }
}

// =============================================================================
// Example 16: Integration with MainWindow (Qt Application)
// =============================================================================

class StringWrapperWidget : public QObject {
    Q_OBJECT

public:
    StringWrapperWidget(QObject* parent = nullptr) : QObject(parent), m_wrapper() {
        connect(&m_wrapper, &QtStringWrapperMASM::operationCompleted,
                this, &StringWrapperWidget::onOperationCompleted);
        connect(&m_wrapper, &QtStringWrapperMASM::errorOccurred,
                this, &StringWrapperWidget::onErrorOccurred);
    }

    void processText(const QString& input) {
        void* qstring = wrapper_qstring_create();
        if (qstring) {
            auto result = m_wrapper.appendToString(qstring, input);
            if (result.success) {
                m_last_result = m_wrapper.getStringAsQString(qstring);
                emit textProcessed(m_last_result);
            }
            m_wrapper.deleteString(qstring);
        }
    }

    QString getLastResult() const { return m_last_result; }

signals:
    void textProcessed(const QString& result);

private slots:
    void onOperationCompleted(const QString& operation, bool success) {
        qDebug() << "Operation completed:" << operation << "Success:" << success;
    }

    void onErrorOccurred(const QString& operation, const QString& error) {
        qDebug() << "Error in" << operation << ":" << error;
    }

private:
    QtStringWrapperMASM m_wrapper;
    QString m_last_result;
};

// =============================================================================
// Master Example Executor
// =============================================================================

void run_all_examples() {
    qDebug() << "\n========== RUNNING ALL EXAMPLES ==========\n";
    
    example_1_basic_string_operations();
    qDebug() << "\n";
    
    example_2_encoding_conversions();
    qDebug() << "\n";
    
    example_3_string_search();
    qDebug() << "\n";
    
    example_4_byte_array_operations();
    qDebug() << "\n";
    
    example_5_hex_conversion();
    qDebug() << "\n";
    
    example_6_file_operations();
    qDebug() << "\n";
    
    example_7_file_reading();
    qDebug() << "\n";
    
    example_8_file_seek();
    qDebug() << "\n";
    
    example_9_file_info();
    qDebug() << "\n";
    
    example_10_file_rename_remove();
    qDebug() << "\n";
    
    example_11_string_pipeline();
    qDebug() << "\n";
    
    example_12_byte_batch();
    qDebug() << "\n";
    
    example_13_error_handling();
    qDebug() << "\n";
    
    example_14_statistics();
    qDebug() << "\n";
    
    example_15_temp_utilities();
    qDebug() << "\n";
    
    qDebug() << "========== ALL EXAMPLES COMPLETE ==========\n";
}

#endif // QT_STRING_WRAPPER_EXAMPLES_HPP
