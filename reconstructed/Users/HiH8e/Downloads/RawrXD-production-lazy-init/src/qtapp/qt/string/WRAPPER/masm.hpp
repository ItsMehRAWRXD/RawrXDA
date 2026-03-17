// =============================================================================
// Qt String Wrapper MASM - C++ Header for Interoperability
// =============================================================================
// Purpose: Pure MASM wrapper for QString, QByteArray, QFile operations
//          C++ class interface to access MASM functions
// Author: AI Toolkit / GitHub Copilot
// Date: December 29, 2025
// Status: Production Ready
// Platform: Qt 6.x, Windows x64
// =============================================================================

#pragma once

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <cstring>
#include <cstdint>

// =============================================================================
// Structure Definitions (Mirror MASM definitions)
// =============================================================================

// String operation result
struct QtStringOpResult {
    uint32_t success;           // Success flag (1=success, 0=failure)
    uint32_t error_code;        // Error code (0=no error)
    uint32_t length;            // Output length or detail
    uint64_t reserved;          // Reserved for future use
};

// String buffer management
struct QtStringBuffer {
    void* data_ptr;             // Pointer to buffer data
    uint32_t capacity;          // Total capacity in bytes
    uint32_t length;            // Current valid length
    uint32_t flags;             // Flags (bit 0: owned, bit 1: locked)
};

// Operation context
struct QtStringOpContext {
    uint32_t operation_type;    // Type of operation
    void* source_ptr;           // Source pointer
    void* dest_ptr;             // Destination pointer
    uint32_t encoding;          // Encoding type
    uint32_t options;           // Operation options
    uint64_t timestamp_ms;      // Timestamp for cache/timeout
};

// File handle (opaque to C++)
struct QtFileHandle {
    void* handle_value;         // File handle value
    uint32_t open_mode;         // Open mode flags
    uint32_t access_rights;     // Access rights
    uint64_t reserved;          // Reserved
};

// Statistics structure
struct QtStringStatistics {
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t current_allocated;
    uint64_t string_conversions;
    uint64_t file_operations;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t last_error_time_ms;
};

// =============================================================================
// Enumeration Constants
// =============================================================================

enum class StringEncoding : uint32_t {
    UTF8 = 0,
    UTF16LE = 1,
    UTF16BE = 2,
    ASCII = 3,
    LATIN1 = 4
};

enum class FileOpenMode : uint32_t {
    Read = 1,
    Write = 2,
    Append = 4,
    Binary = 8,
    Text = 16
};

enum class StringOperation : uint32_t {
    Concat = 0,
    Substring = 1,
    Replace = 2,
    Trim = 3,
    Reverse = 4,
    Uppercase = 5,
    Lowercase = 6,
    Find = 7,
    Split = 8
};

enum class ErrorCode : uint32_t {
    Success = 0,
    InvalidPtr = 1,
    BufferTooSmall = 2,
    OutOfMemory = 3,
    InvalidEncoding = 4,
    FileNotFound = 5,
    FileAccessDeny = 6,
    FileLocked = 7,
    OperationTimeout = 8,
    InvalidParameter = 9,
    Unknown = 255
};

// =============================================================================
// Extern C Function Declarations
// =============================================================================

extern "C" {
    // QString operations
    void* wrapper_qstring_create(void);
    uint32_t wrapper_qstring_delete(void* qstring);
    QtStringOpResult wrapper_qstring_append(void* qstring, const char* data, uint32_t length);
    uint32_t wrapper_qstring_clear(void* qstring);
    uint32_t wrapper_qstring_length(void* qstring);
    QtStringOpResult wrapper_qstring_to_utf8(void* qstring, char* buffer, uint32_t size);
    QtStringOpResult wrapper_qstring_from_utf8(void* qstring, const char* utf8_data, uint32_t length);
    QtStringOpResult wrapper_qstring_to_latin1(void* qstring, char* buffer, uint32_t size);
    QtStringOpResult wrapper_qstring_from_latin1(void* qstring, const char* latin1_data, uint32_t length);
    QtStringOpResult wrapper_qstring_substring(void* qstring, uint32_t pos, uint32_t len, char* output, uint32_t out_size);
    int64_t wrapper_qstring_find(void* qstring, const char* search, uint32_t start);
    QtStringOpResult wrapper_qstring_replace(void* qstring, const char* find, const char* replace, uint32_t flags);
    QtStringOpResult wrapper_qstring_split(void* qstring, const char* sep, void** output_array, uint32_t* count);
    QtStringOpResult wrapper_qstring_trim(void* qstring);
    QtStringOpResult wrapper_qstring_uppercase(void* qstring);
    QtStringOpResult wrapper_qstring_lowercase(void* qstring);
    int32_t wrapper_qstring_compare(void* qstring1, void* qstring2);

    // QByteArray operations
    void* wrapper_qbytearray_create(uint32_t initial_size);
    uint32_t wrapper_qbytearray_delete(void* qbytearray);
    QtStringOpResult wrapper_qbytearray_append(void* qbytearray, const uint8_t* data, uint32_t length);
    uint32_t wrapper_qbytearray_clear(void* qbytearray);
    uint32_t wrapper_qbytearray_length(void* qbytearray);
    uint8_t wrapper_qbytearray_at(void* qbytearray, uint32_t position);
    uint32_t wrapper_qbytearray_set(void* qbytearray, uint32_t position, uint8_t value);
    uint32_t wrapper_qbytearray_contains(void* qbytearray, const uint8_t* data, uint32_t length);
    int64_t wrapper_qbytearray_find(void* qbytearray, const uint8_t* search, uint32_t length, uint32_t start);
    QtStringOpResult wrapper_qbytearray_from_hex(void* qbytearray, const char* hex_string);
    QtStringOpResult wrapper_qbytearray_to_hex(void* qbytearray, char* output, uint32_t size);
    QtStringOpResult wrapper_qbytearray_compress(void* qbytearray, void* output, uint32_t algorithm);
    QtStringOpResult wrapper_qbytearray_decompress(void* qbytearray, void* output);

    // QFile operations
    QtFileHandle* wrapper_qfile_create(const char* filepath);
    uint32_t wrapper_qfile_delete(QtFileHandle* file);
    QtStringOpResult wrapper_qfile_open(QtFileHandle* file, const char* filepath, uint32_t mode);
    uint32_t wrapper_qfile_close(QtFileHandle* file);
    int64_t wrapper_qfile_read(QtFileHandle* file, uint8_t* buffer, uint32_t size);
    int64_t wrapper_qfile_write(QtFileHandle* file, const uint8_t* data, uint32_t size);
    int64_t wrapper_qfile_seek(QtFileHandle* file, int64_t offset);
    int64_t wrapper_qfile_tell(QtFileHandle* file);
    int64_t wrapper_qfile_read_all(QtFileHandle* file, uint8_t* buffer, uint32_t max_size);
    uint32_t wrapper_qfile_exists(const char* filepath);
    uint32_t wrapper_qfile_remove(const char* filepath);
    uint32_t wrapper_qfile_rename(const char* oldpath, const char* newpath);
    int64_t wrapper_qfile_size(QtFileHandle* file);

    // Utility operations
    void* wrapper_qt_alloc(uint64_t size);
    void wrapper_qt_free(void* ptr);
    void* wrapper_qt_mutex_create(void);
    void wrapper_qt_mutex_delete(void* mutex);
    void wrapper_qt_mutex_lock(void* mutex);
    void wrapper_qt_mutex_unlock(void* mutex);
    QtStringOpResult wrapper_qt_get_temp_path(char* buffer, uint32_t size);
    QtStringStatistics wrapper_qt_get_statistics(void);
    void wrapper_qt_clear_statistics(void);

    // POSIX file operations
    int64_t wrapper_posix_open(const char* path, uint32_t flags, uint32_t mode);
    int32_t wrapper_posix_close(int64_t fd);
    int64_t wrapper_posix_read(int64_t fd, uint8_t* buffer, uint32_t count);
    int64_t wrapper_posix_write(int64_t fd, const uint8_t* data, uint32_t count);
    int32_t wrapper_posix_stat(const char* path, void* stat_buffer);

    // Additional encoding conversions
    QtStringOpResult wrapper_qstring_to_utf16(void* qstring, uint16_t* output, uint32_t size);
    void* wrapper_qstring_from_utf16(const uint16_t* utf16_data, uint32_t length);
    QtStringOpResult wrapper_qstring_to_utf32(void* qstring, uint32_t* output, uint32_t size);
    void* wrapper_qstring_from_utf32(const uint32_t* utf32_data, uint32_t length);
    QtStringOpResult wrapper_qstring_to_windows1252(void* qstring, uint8_t* output, uint32_t size);
    void* wrapper_qstring_from_windows1252(const uint8_t* data, uint32_t length);
    QtStringOpResult wrapper_qstring_to_iso88591(void* qstring, uint8_t* output, uint32_t size);
    void* wrapper_qstring_from_iso88591(const uint8_t* data, uint32_t length);

    // String formatting operations
    void* wrapper_qstring_sprintf(const char* format, void* varargs);
    void* wrapper_qstring_format(void* template_qstring, void** args, uint32_t count);
    QtStringOpResult wrapper_qstring_replace_placeholder(void* qstring, const char* placeholder, const char* replacement);

    // Advanced compression
    int64_t wrapper_qbytearray_compress_lzma(const uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_capacity);
    int64_t wrapper_qbytearray_decompress_lzma(const uint8_t* compressed, uint32_t size, uint8_t* output, uint32_t capacity);
    int64_t wrapper_qbytearray_compress_brotli(const uint8_t* input, uint32_t size, uint8_t* output, uint32_t capacity);
    int64_t wrapper_qbytearray_decompress_brotli(const uint8_t* compressed, uint32_t size, uint8_t* output, uint32_t capacity);
    int64_t wrapper_qbytearray_compress_lz4(const uint8_t* input, uint32_t size, uint8_t* output, uint32_t capacity);
    int64_t wrapper_qbytearray_decompress_lz4(const uint8_t* compressed, uint32_t size, uint8_t* output, uint32_t capacity);

    // Network file operations
    int64_t wrapper_net_http_get(const char* url, uint8_t* output, uint32_t size);
    int64_t wrapper_net_http_post(const char* url, const uint8_t* data, uint32_t data_size, uint8_t* response, uint32_t resp_size);
    int64_t wrapper_net_ftp_get(const char* ftp_url, uint8_t* output, uint32_t size);
    int32_t wrapper_net_ftp_put(const char* ftp_url, const uint8_t* data, uint32_t size);
    void* wrapper_net_websocket_open(const char* ws_url);
    int64_t wrapper_net_websocket_send(void* ws_handle, const uint8_t* data, uint32_t length);
    int64_t wrapper_net_websocket_recv(void* ws_handle, uint8_t* buffer, uint32_t size);
    int32_t wrapper_net_websocket_close(void* ws_handle);

    // Async file operations
    void* wrapper_qfile_open_async(const char* path, uint32_t mode, void* callback);
    void* wrapper_qfile_read_async(QtFileHandle* file, uint8_t* buffer, uint32_t size, void* callback);
    void* wrapper_qfile_write_async(QtFileHandle* file, const uint8_t* data, uint32_t size, void* callback);
    int32_t wrapper_qfile_close_async(QtFileHandle* file, void* callback);
    int32_t wrapper_async_wait(void* op_handle, uint32_t timeout_ms);
    int32_t wrapper_async_cancel(void* op_handle);
}

// =============================================================================
// QtStringWrapperMASM Class (C++ Wrapper)
    uint32_t wrapper_qbytearray_set(void* qbytearray, uint32_t position, uint8_t value);
    uint32_t wrapper_qbytearray_contains(void* qbytearray, uint8_t value);
    int64_t wrapper_qbytearray_find(void* qbytearray, const uint8_t* data, uint32_t length, uint32_t start);
    QtStringOpResult wrapper_qbytearray_from_hex(void* qbytearray, const char* hex_string);
    QtStringOpResult wrapper_qbytearray_to_hex(void* qbytearray, char* output, uint32_t out_size);
    QtStringOpResult wrapper_qbytearray_compress(void* qbytearray, void** output);
    QtStringOpResult wrapper_qbytearray_decompress(void* qbytearray, void** output);

    // QFile operations
    QtFileHandle* wrapper_qfile_create(const char* filepath);
    uint32_t wrapper_qfile_delete(QtFileHandle* file);
    QtStringOpResult wrapper_qfile_open(QtFileHandle* file, const char* filepath, uint32_t mode);
    uint32_t wrapper_qfile_close(QtFileHandle* file);
    QtStringOpResult wrapper_qfile_read(QtFileHandle* file, uint8_t* buffer, uint32_t size);
    QtStringOpResult wrapper_qfile_write(QtFileHandle* file, const uint8_t* data, uint32_t length);
    int64_t wrapper_qfile_seek(QtFileHandle* file, int64_t offset);
    int64_t wrapper_qfile_tell(QtFileHandle* file);
    QtStringOpResult wrapper_qfile_read_all(QtFileHandle* file, uint8_t** output, uint32_t* size);
    uint32_t wrapper_qfile_exists(const char* filepath);
    uint32_t wrapper_qfile_remove(const char* filepath);
    uint32_t wrapper_qfile_rename(const char* oldpath, const char* newpath);
    int64_t wrapper_qfile_size(QtFileHandle* file);

    // Utility operations
    void* wrapper_qt_alloc(uint64_t size);
    uint32_t wrapper_qt_free(void* ptr);
    void* wrapper_qt_mutex_create(void);
    uint32_t wrapper_qt_mutex_delete(void* mutex);
    uint32_t wrapper_qt_mutex_lock(void* mutex);
    uint32_t wrapper_qt_mutex_unlock(void* mutex);
    QtStringOpResult wrapper_qt_get_temp_path(char* buffer, uint32_t size);
    uint32_t wrapper_qt_get_statistics(QtStringStatistics* stats);
    uint32_t wrapper_qt_clear_statistics(void);
}

// =============================================================================
// C++ Wrapper Class
// =============================================================================

class QtStringWrapperMASM : public QObject {
    Q_OBJECT

public:
    explicit QtStringWrapperMASM(QObject* parent = nullptr);
    ~QtStringWrapperMASM();

    // =========================================================================
    // QString Operations
    // =========================================================================

    QString createString();
    bool deleteString(void* qstring_ptr);
    QtStringOpResult appendToString(void* qstring_ptr, const QString& text);
    bool clearString(void* qstring_ptr);
    uint32_t getStringLength(void* qstring_ptr);
    QString getStringAsQString(void* qstring_ptr);
    bool setStringFromQString(void* qstring_ptr, const QString& text);
    QtStringOpResult findInString(void* qstring_ptr, const QString& search);
    QtStringOpResult replaceInString(void* qstring_ptr, const QString& find, const QString& replace);
    int compareStrings(void* qstring1, void* qstring2);

    // =========================================================================
    // QByteArray Operations
    // =========================================================================

    void* createByteArray(uint32_t initial_size = 0);
    bool deleteByteArray(void* qba_ptr);
    QtStringOpResult appendToByteArray(void* qba_ptr, const QByteArray& data);
    bool clearByteArray(void* qba_ptr);
    uint32_t getByteArrayLength(void* qba_ptr);
    QByteArray getByteArrayAsQByteArray(void* qba_ptr);
    bool setByteArrayFromQByteArray(void* qba_ptr, const QByteArray& data);
    uint8_t getByteAt(void* qba_ptr, uint32_t position);
    bool setByteAt(void* qba_ptr, uint32_t position, uint8_t value);

    // =========================================================================
    // QFile Operations
    // =========================================================================

    QtFileHandle* createFileHandle(const QString& filepath);
    bool deleteFileHandle(QtFileHandle* file);
    QtStringOpResult openFile(QtFileHandle* file, const QString& filepath, FileOpenMode mode);
    bool closeFile(QtFileHandle* file);
    QtStringOpResult readFile(QtFileHandle* file, QByteArray& output, uint32_t max_size);
    QtStringOpResult writeFile(QtFileHandle* file, const QByteArray& data);
    int64_t seekFile(QtFileHandle* file, int64_t offset);
    int64_t tellFile(QtFileHandle* file);
    QByteArray readFileAll(QtFileHandle* file);
    bool fileExists(const QString& filepath);
    bool removeFile(const QString& filepath);
    bool renameFile(const QString& oldpath, const QString& newpath);
    int64_t getFileSize(QtFileHandle* file);

    // =========================================================================
    // Utility Methods
    // =========================================================================

    QString getTempDirectory();
    QString generateTempFilePath(const QString& prefix = "qt_temp");
    QtStringStatistics getStatistics();
    bool clearStatistics();
    QString getLastError();
    ErrorCode getLastErrorCode();
    bool isInitialized() const { return m_initialized; }

    // =========================================================================
    // POSIX File Operations (Linux/macOS)
    // =========================================================================

    int64_t posixOpen(const QString& path, uint32_t flags, uint32_t mode = 0644);
    int posixClose(int64_t fd);
    int64_t posixRead(int64_t fd, QByteArray& buffer, uint32_t count);
    int64_t posixWrite(int64_t fd, const QByteArray& data);
    QtStringOpResult posixStat(const QString& path);

    // =========================================================================
    // Additional String Encoding Conversions
    // =========================================================================

    QByteArray stringToUTF16(void* qstring_ptr);
    void* stringFromUTF16(const QByteArray& utf16_data);
    QByteArray stringToUTF32(void* qstring_ptr);
    void* stringFromUTF32(const QByteArray& utf32_data);
    QByteArray stringToWindows1252(void* qstring_ptr);
    void* stringFromWindows1252(const QByteArray& data);
    QByteArray stringToISO88591(void* qstring_ptr);
    void* stringFromISO88591(const QByteArray& data);

    // =========================================================================
    // String Formatting Functions
    // =========================================================================

    QString formatSprintf(const QString& format, ...);
    QString formatTemplate(const QString& template_str, const QStringList& args);
    QString replacePlaceholder(void* qstring_ptr, const QString& placeholder, const QString& value);

    // =========================================================================
    // Advanced Compression Algorithms
    // =========================================================================

    QByteArray compressLZMA(const QByteArray& data);
    QByteArray decompressLZMA(const QByteArray& compressed);
    QByteArray compressBrotli(const QByteArray& data);
    QByteArray decompressBrotli(const QByteArray& compressed);
    QByteArray compressLZ4(const QByteArray& data);
    QByteArray decompressLZ4(const QByteArray& compressed);

    // =========================================================================
    // Network File Operations
    // =========================================================================

    QByteArray httpGet(const QString& url, int timeout_ms = 30000);
    QByteArray httpPost(const QString& url, const QByteArray& post_data, int timeout_ms = 30000);
    QByteArray ftpGet(const QString& ftp_url);
    bool ftpPut(const QString& ftp_url, const QByteArray& data);
    void* websocketOpen(const QString& ws_url);
    int64_t websocketSend(void* ws_handle, const QByteArray& message);
    QByteArray websocketRecv(void* ws_handle, uint32_t max_size = 65536);
    bool websocketClose(void* ws_handle);

    // =========================================================================
    // Async File Operations
    // =========================================================================

    struct AsyncOperation {
        void* handle;
        bool completed;
        int64_t result;
        QString error;
    };

    AsyncOperation* openFileAsync(const QString& filepath, FileOpenMode mode);
    AsyncOperation* readFileAsync(QtFileHandle* file, uint32_t size);
    AsyncOperation* writeFileAsync(QtFileHandle* file, const QByteArray& data);
    bool closeFileAsync(QtFileHandle* file);
    bool asyncWait(AsyncOperation* op, uint32_t timeout_ms = 5000);
    bool asyncCancel(AsyncOperation* op);

signals:
    void operationCompleted(const QString& operation, bool success);
    void errorOccurred(const QString& operation, const QString& error_detail);
    void asyncOperationCompleted(AsyncOperation* op);

private:
    mutable QMutex m_mutex;
    bool m_initialized;
    ErrorCode m_last_error;
    QString m_last_error_string;
    QString m_temp_dir;

    void setError(ErrorCode code, const QString& message);
    QString errorCodeToString(ErrorCode code) const;
    QString encodingToString(StringEncoding enc) const;
};

// =============================================================================
// Inline Helper Functions
// =============================================================================

inline bool isSuccess(const QtStringOpResult& result) {
    return result.success != 0 && result.error_code == 0;
}

inline ErrorCode resultToErrorCode(uint32_t code) {
    return static_cast<ErrorCode>(code);
}

inline QString resultToQString(const QtStringOpResult& result) {
    if (result.success) {
        return QString("Operation successful (length: %1)").arg(result.length);
    } else {
        return QString("Operation failed (error: %1)").arg(result.error_code);
    }
}

#endif // QT_STRING_WRAPPER_MASM_HPP
