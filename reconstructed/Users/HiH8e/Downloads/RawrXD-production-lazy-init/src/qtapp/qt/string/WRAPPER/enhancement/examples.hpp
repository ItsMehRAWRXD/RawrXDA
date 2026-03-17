// =============================================================================
// Qt String Wrapper MASM - Enhancement Features Examples
// =============================================================================
// Date: December 29, 2025
// Status: Production Ready
// New Features: POSIX file ops, encodings, formatting, compression, network, async
// =============================================================================

#include "qt_string_wrapper_masm.hpp"
#include <QCoreApplication>
#include <QDebug>

// =============================================================================
// Example 17: POSIX File Operations (Linux/macOS support)
// =============================================================================

void example17_posix_file_operations() {
    qDebug() << "=== Example 17: POSIX File Operations ===";
    
    QtStringWrapperMASM wrapper;
    
    // Open file using POSIX syscalls
    QString filepath = "/tmp/test_posix.txt";
    int64_t fd = wrapper.posixOpen(filepath, 0x0001 | 0x0040 | 0x0200, 0644);  // O_WRONLY | O_CREAT | O_TRUNC
    
    if (fd >= 0) {
        qDebug() << "POSIX file opened, fd:" << fd;
        
        // Write data using POSIX write
        QByteArray data = "Hello from POSIX syscalls!\n";
        int64_t written = wrapper.posixWrite(fd, data);
        qDebug() << "Bytes written:" << written;
        
        // Close file
        wrapper.posixClose(fd);
        
        // Read file back
        fd = wrapper.posixOpen(filepath, 0x0000, 0);  // O_RDONLY
        QByteArray buffer(1024, 0);
        int64_t read_bytes = wrapper.posixRead(fd, buffer, 1024);
        qDebug() << "Read back:" << buffer.left(read_bytes);
        wrapper.posixClose(fd);
        
        // Get file statistics
        auto stat_result = wrapper.posixStat(filepath);
        if (isSuccess(stat_result)) {
            qDebug() << "File stat success, size:" << stat_result.length;
        }
    } else {
        qDebug() << "Failed to open file with POSIX";
    }
}

// =============================================================================
// Example 18: Additional String Encodings (UTF-16, UTF-32, Windows-1252)
// =============================================================================

void example18_additional_encodings() {
    qDebug() << "=== Example 18: Additional String Encodings ===";
    
    QtStringWrapperMASM wrapper;
    
    // Create test QString
    void* qs = wrapper.createString();
    wrapper.appendToString(qs, "Hello 世界 🌍");
    
    // Convert to UTF-16
    QByteArray utf16_data = wrapper.stringToUTF16(qs);
    qDebug() << "UTF-16 size:" << utf16_data.size() << "bytes";
    
    // Convert back from UTF-16
    void* qs_utf16 = wrapper.stringFromUTF16(utf16_data);
    QString recovered_utf16 = wrapper.getStringAsQString(qs_utf16);
    qDebug() << "Recovered from UTF-16:" << recovered_utf16;
    
    // Convert to UTF-32
    QByteArray utf32_data = wrapper.stringToUTF32(qs);
    qDebug() << "UTF-32 size:" << utf32_data.size() << "bytes (4 bytes per codepoint)";
    
    // Convert to Windows-1252
    void* qs_latin = wrapper.createString();
    wrapper.appendToString(qs_latin, "Café résumé");
    QByteArray win1252 = wrapper.stringToWindows1252(qs_latin);
    qDebug() << "Windows-1252:" << win1252.toHex();
    
    // Convert to ISO-8859-1
    QByteArray iso88591 = wrapper.stringToISO88591(qs_latin);
    qDebug() << "ISO-8859-1:" << iso88591.toHex();
    
    // Cleanup
    wrapper.deleteString(qs);
    wrapper.deleteString(qs_utf16);
    wrapper.deleteString(qs_latin);
}

// =============================================================================
// Example 19: String Formatting Functions (sprintf-style)
// =============================================================================

void example19_string_formatting() {
    qDebug() << "=== Example 19: String Formatting ===";
    
    QtStringWrapperMASM wrapper;
    
    // sprintf-style formatting
    QString formatted = wrapper.formatSprintf(
        "User: %s, Age: %d, Score: %f, ID: 0x%x",
        "Alice", 30, 95.5, 0xABCD
    );
    qDebug() << "Formatted:" << formatted;
    
    // Template-style formatting
    QStringList args;
    args << "Production" << "v2.5.1" << "2025-12-29";
    QString template_result = wrapper.formatTemplate(
        "Environment: {0}, Version: {1}, Date: {2}",
        args
    );
    qDebug() << "Template result:" << template_result;
    
    // Placeholder replacement
    void* qs = wrapper.createString();
    wrapper.appendToString(qs, "Welcome {{username}} to {{app_name}}!");
    wrapper.replacePlaceholder(qs, "{{username}}", "Alice");
    wrapper.replacePlaceholder(qs, "{{app_name}}", "RawrXD IDE");
    qDebug() << "Placeholder result:" << wrapper.getStringAsQString(qs);
    wrapper.deleteString(qs);
}

// =============================================================================
// Example 20: Advanced Compression Algorithms
// =============================================================================

void example20_advanced_compression() {
    qDebug() << "=== Example 20: Advanced Compression ===";
    
    QtStringWrapperMASM wrapper;
    
    // Prepare test data
    QByteArray original_data;
    for (int i = 0; i < 1000; i++) {
        original_data.append("This is test data for compression. ");
    }
    qDebug() << "Original size:" << original_data.size() << "bytes";
    
    // LZMA compression (highest ratio, slower)
    QByteArray lzma_compressed = wrapper.compressLZMA(original_data);
    qDebug() << "LZMA compressed:" << lzma_compressed.size() << "bytes"
             << "(" << (100.0 * lzma_compressed.size() / original_data.size()) << "% of original)";
    
    QByteArray lzma_decompressed = wrapper.decompressLZMA(lzma_compressed);
    qDebug() << "LZMA decompressed matches:" << (lzma_decompressed == original_data);
    
    // Brotli compression (good ratio, fast)
    QByteArray brotli_compressed = wrapper.compressBrotli(original_data);
    qDebug() << "Brotli compressed:" << brotli_compressed.size() << "bytes"
             << "(" << (100.0 * brotli_compressed.size() / original_data.size()) << "% of original)";
    
    QByteArray brotli_decompressed = wrapper.decompressBrotli(brotli_compressed);
    qDebug() << "Brotli decompressed matches:" << (brotli_decompressed == original_data);
    
    // LZ4 compression (fastest, lower ratio)
    QByteArray lz4_compressed = wrapper.compressLZ4(original_data);
    qDebug() << "LZ4 compressed:" << lz4_compressed.size() << "bytes"
             << "(" << (100.0 * lz4_compressed.size() / original_data.size()) << "% of original)";
    
    QByteArray lz4_decompressed = wrapper.decompressLZ4(lz4_compressed);
    qDebug() << "LZ4 decompressed matches:" << (lz4_decompressed == original_data);
}

// =============================================================================
// Example 21: Network File Operations (HTTP, FTP, WebSocket)
// =============================================================================

void example21_network_operations() {
    qDebug() << "=== Example 21: Network Operations ===";
    
    QtStringWrapperMASM wrapper;
    
    // HTTP GET request
    QString url = "http://example.com/data.json";
    QByteArray http_data = wrapper.httpGet(url, 30000);  // 30 second timeout
    if (!http_data.isEmpty()) {
        qDebug() << "HTTP GET received:" << http_data.size() << "bytes";
        qDebug() << "First 100 chars:" << http_data.left(100);
    }
    
    // HTTP POST request
    QByteArray post_data = "{\"action\":\"test\",\"value\":123}";
    QByteArray response = wrapper.httpPost("http://api.example.com/endpoint", post_data);
    qDebug() << "HTTP POST response:" << response;
    
    // FTP download
    QString ftp_url = "ftp://anonymous:user@ftp.example.com/pub/file.txt";
    QByteArray ftp_data = wrapper.ftpGet(ftp_url);
    if (!ftp_data.isEmpty()) {
        qDebug() << "FTP download successful:" << ftp_data.size() << "bytes";
    }
    
    // FTP upload
    QByteArray upload_data = "Test file content for FTP upload\n";
    bool ftp_success = wrapper.ftpPut("ftp://user:pass@ftp.example.com/upload/test.txt", upload_data);
    qDebug() << "FTP upload:" << (ftp_success ? "success" : "failed");
    
    // WebSocket connection
    void* ws = wrapper.websocketOpen("ws://echo.websocket.org");
    if (ws) {
        qDebug() << "WebSocket connected";
        
        // Send message
        QByteArray ws_message = "Hello WebSocket!";
        int64_t sent = wrapper.websocketSend(ws, ws_message);
        qDebug() << "WebSocket sent:" << sent << "bytes";
        
        // Receive echo
        QByteArray ws_response = wrapper.websocketRecv(ws, 1024);
        qDebug() << "WebSocket received:" << ws_response;
        
        // Close connection
        wrapper.websocketClose(ws);
        qDebug() << "WebSocket closed";
    }
}

// =============================================================================
// Example 22: Async File Operations (Non-blocking I/O)
// =============================================================================

void example22_async_file_operations() {
    qDebug() << "=== Example 22: Async File Operations ===";
    
    QtStringWrapperMASM wrapper;
    
    // Open file asynchronously
    QString filepath = wrapper.generateTempFilePath("async_test");
    auto* open_op = wrapper.openFileAsync(filepath, QtStringWrapperMASM::FileOpenMode::Write);
    
    if (open_op) {
        qDebug() << "Async open initiated...";
        
        // Wait for open to complete (with timeout)
        bool open_completed = wrapper.asyncWait(open_op, 5000);  // 5 second timeout
        
        if (open_completed && open_op->completed) {
            qDebug() << "File opened asynchronously, handle:" << open_op->handle;
            
            // Write data asynchronously
            QByteArray data = "Async write test data\n";
            auto* write_op = wrapper.writeFileAsync(
                static_cast<QtFileHandle*>(open_op->handle),
                data
            );
            
            if (write_op) {
                qDebug() << "Async write initiated...";
                
                // Wait for write to complete
                bool write_completed = wrapper.asyncWait(write_op, 5000);
                
                if (write_completed && write_op->completed) {
                    qDebug() << "Async write completed, bytes written:" << write_op->result;
                } else {
                    qDebug() << "Async write timeout or failed";
                }
            }
            
            // Close file
            wrapper.closeFileAsync(static_cast<QtFileHandle*>(open_op->handle));
            qDebug() << "File closed";
        } else {
            qDebug() << "Async open timeout or failed";
        }
    }
}

// =============================================================================
// Example 23: Combined Advanced Features Demo
// =============================================================================

void example23_combined_advanced_features() {
    qDebug() << "=== Example 23: Combined Advanced Features ===";
    
    QtStringWrapperMASM wrapper;
    
    // Scenario: Download JSON from HTTP, format message, compress, save async
    
    // 1. Download data via HTTP
    qDebug() << "Step 1: Downloading data via HTTP...";
    QByteArray json_data = wrapper.httpGet("http://api.example.com/config.json", 10000);
    
    if (!json_data.isEmpty()) {
        qDebug() << "Downloaded:" << json_data.size() << "bytes";
        
        // 2. Format status message
        QString status_msg = wrapper.formatSprintf(
            "Downloaded %d bytes from API at %s",
            json_data.size(),
            "2025-12-29T10:30:00Z"
        );
        qDebug() << status_msg;
        
        // 3. Compress using LZ4 (fast compression)
        qDebug() << "Step 2: Compressing with LZ4...";
        QByteArray compressed = wrapper.compressLZ4(json_data);
        qDebug() << "Compressed to:" << compressed.size() << "bytes"
                 << "(" << (100.0 * compressed.size() / json_data.size()) << "%)";
        
        // 4. Save compressed data asynchronously
        qDebug() << "Step 3: Saving asynchronously...";
        QString output_path = wrapper.generateTempFilePath("api_cache");
        
        auto* open_op = wrapper.openFileAsync(output_path, QtStringWrapperMASM::FileOpenMode::Write);
        if (wrapper.asyncWait(open_op, 5000) && open_op->completed) {
            auto* write_op = wrapper.writeFileAsync(
                static_cast<QtFileHandle*>(open_op->handle),
                compressed
            );
            
            if (wrapper.asyncWait(write_op, 5000) && write_op->completed) {
                qDebug() << "Saved to:" << output_path;
                qDebug() << "File size:" << write_op->result << "bytes";
            }
            
            wrapper.closeFileAsync(static_cast<QtFileHandle*>(open_op->handle));
        }
        
        // 5. Convert to UTF-16 for Windows API
        void* qs = wrapper.createString();
        wrapper.setStringFromQString(qs, status_msg);
        QByteArray utf16_status = wrapper.stringToUTF16(qs);
        qDebug() << "UTF-16 status message:" << utf16_status.size() << "bytes";
        wrapper.deleteString(qs);
    }
    
    qDebug() << "Combined demo complete!";
}

// =============================================================================
// Main Function - Run All Enhancement Examples
// =============================================================================

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "Qt String Wrapper MASM - Enhancement Features Examples";
    qDebug() << "=======================================================\n";
    
    example17_posix_file_operations();
    qDebug() << "";
    
    example18_additional_encodings();
    qDebug() << "";
    
    example19_string_formatting();
    qDebug() << "";
    
    example20_advanced_compression();
    qDebug() << "";
    
    example21_network_operations();
    qDebug() << "";
    
    example22_async_file_operations();
    qDebug() << "";
    
    example23_combined_advanced_features();
    qDebug() << "";
    
    qDebug() << "All enhancement examples completed!";
    
    return 0;
}
