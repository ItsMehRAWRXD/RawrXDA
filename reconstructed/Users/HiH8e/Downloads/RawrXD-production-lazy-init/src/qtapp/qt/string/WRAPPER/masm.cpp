// =============================================================================
// Qt String Wrapper MASM - C++ Implementation
// =============================================================================
// Purpose: Implementation of pure MASM wrapper for Qt string/file operations
// Author: AI Toolkit / GitHub Copilot
// Date: December 29, 2025
// Status: Production Ready
// =============================================================================

#include "qt_string_wrapper_masm.hpp"
#include <QDebug>
#include <QStandardPaths>
#include <QUuid>

// =============================================================================
// Constructor & Destructor
// =============================================================================

QtStringWrapperMASM::QtStringWrapperMASM(QObject* parent)
    : QObject(parent),
      m_initialized(false),
      m_last_error(ErrorCode::Success) {
    
    QMutexLocker lock(&m_mutex);
    
    // Initialize temp directory
    m_temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (m_temp_dir.isEmpty()) {
        m_temp_dir = QDir::tempPath();
    }
    
    m_initialized = true;
    qDebug() << "QtStringWrapperMASM initialized at" << m_temp_dir;
}

QtStringWrapperMASM::~QtStringWrapperMASM() {
    QMutexLocker lock(&m_mutex);
    m_initialized = false;
}

// =============================================================================
// Error Handling
// =============================================================================

void QtStringWrapperMASM::setError(ErrorCode code, const QString& message) {
    QMutexLocker lock(&m_mutex);
    m_last_error = code;
    m_last_error_string = message;
    emit errorOccurred("QtStringWrapperMASM", message);
}

QString QtStringWrapperMASM::errorCodeToString(ErrorCode code) const {
    switch (code) {
        case ErrorCode::Success:
            return "No error";
        case ErrorCode::InvalidPtr:
            return "Invalid pointer";
        case ErrorCode::BufferTooSmall:
            return "Buffer too small";
        case ErrorCode::OutOfMemory:
            return "Out of memory";
        case ErrorCode::InvalidEncoding:
            return "Invalid encoding";
        case ErrorCode::FileNotFound:
            return "File not found";
        case ErrorCode::FileAccessDeny:
            return "File access denied";
        case ErrorCode::FileLocked:
            return "File locked";
        case ErrorCode::OperationTimeout:
            return "Operation timeout";
        case ErrorCode::InvalidParameter:
            return "Invalid parameter";
        case ErrorCode::Unknown:
        default:
            return "Unknown error";
    }
}

QString QtStringWrapperMASM::getLastError() {
    QMutexLocker lock(&m_mutex);
    return m_last_error_string;
}

ErrorCode QtStringWrapperMASM::getLastErrorCode() {
    QMutexLocker lock(&m_mutex);
    return m_last_error;
}

// =============================================================================
// QString Operations
// =============================================================================

QString QtStringWrapperMASM::createString() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) {
        setError(ErrorCode::InvalidParameter, "Wrapper not initialized");
        return QString();
    }
    
    void* ptr = wrapper_qstring_create();
    if (!ptr) {
        setError(ErrorCode::OutOfMemory, "Failed to allocate string");
        return QString();
    }
    
    qDebug() << "Created string at" << ptr;
    return QString();
}

bool QtStringWrapperMASM::deleteString(void* qstring_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring_ptr) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return false;
    }
    
    uint32_t result = wrapper_qstring_delete(qstring_ptr);
    
    if (result == 0) {
        emit operationCompleted("deleteString", true);
        return true;
    } else {
        setError(static_cast<ErrorCode>(result), "Failed to delete string");
        return false;
    }
}

QtStringOpResult QtStringWrapperMASM::appendToString(void* qstring_ptr, const QString& text) {
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!qstring_ptr) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return result;
    }
    
    QByteArray utf8_data = text.toUtf8();
    result = wrapper_qstring_append(qstring_ptr, utf8_data.constData(), utf8_data.length());
    
    if (result.success == 0) {
        setError(static_cast<ErrorCode>(result.error_code), "Failed to append string");
    } else {
        emit operationCompleted("appendToString", true);
    }
    
    return result;
}

bool QtStringWrapperMASM::clearString(void* qstring_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring_ptr) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return false;
    }
    
    uint32_t result = wrapper_qstring_clear(qstring_ptr);
    
    if (result == 0) {
        emit operationCompleted("clearString", true);
        return true;
    } else {
        setError(static_cast<ErrorCode>(result), "Failed to clear string");
        return false;
    }
}

uint32_t QtStringWrapperMASM::getStringLength(void* qstring_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring_ptr) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return 0;
    }
    
    return wrapper_qstring_length(qstring_ptr);
}

QString QtStringWrapperMASM::getStringAsQString(void* qstring_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring_ptr) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return QString();
    }
    
    // Allocate temporary buffer for conversion
    char buffer[4096] = {0};
    QtStringOpResult result = wrapper_qstring_to_utf8(qstring_ptr, buffer, sizeof(buffer) - 1);
    
    if (result.success) {
        return QString::fromUtf8(buffer);
    } else {
        setError(static_cast<ErrorCode>(result.error_code), "Failed to convert string to UTF8");
        return QString();
    }
}

bool QtStringWrapperMASM::setStringFromQString(void* qstring_ptr, const QString& text) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring_ptr) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return false;
    }
    
    QByteArray utf8_data = text.toUtf8();
    QtStringOpResult result = wrapper_qstring_from_utf8(qstring_ptr, utf8_data.constData(), utf8_data.length());
    
    if (result.success) {
        emit operationCompleted("setStringFromQString", true);
        return true;
    } else {
        setError(static_cast<ErrorCode>(result.error_code), "Failed to set string from QString");
        return false;
    }
}

QtStringOpResult QtStringWrapperMASM::findInString(void* qstring_ptr, const QString& search) {
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!qstring_ptr) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    QByteArray search_data = search.toUtf8();
    int64_t pos = wrapper_qstring_find(qstring_ptr, search_data.constData(), 0);
    
    if (pos >= 0) {
        result.success = 1;
        result.length = static_cast<uint32_t>(pos);
    } else {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidParameter);
    }
    
    return result;
}

QtStringOpResult QtStringWrapperMASM::replaceInString(
    void* qstring_ptr, const QString& find, const QString& replace) {
    
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!qstring_ptr) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    QByteArray find_data = find.toUtf8();
    QByteArray replace_data = replace.toUtf8();
    
    result = wrapper_qstring_replace(qstring_ptr, find_data.constData(), replace_data.constData(), 0);
    
    return result;
}

int QtStringWrapperMASM::compareStrings(void* qstring1, void* qstring2) {
    QMutexLocker lock(&m_mutex);
    
    if (!qstring1 || !qstring2) {
        setError(ErrorCode::InvalidPtr, "Null string pointer");
        return -2;
    }
    
    return wrapper_qstring_compare(qstring1, qstring2);
}

// =============================================================================
// QByteArray Operations
// =============================================================================

void* QtStringWrapperMASM::createByteArray(uint32_t initial_size) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) {
        setError(ErrorCode::InvalidParameter, "Wrapper not initialized");
        return nullptr;
    }
    
    void* ptr = wrapper_qbytearray_create(initial_size);
    if (!ptr) {
        setError(ErrorCode::OutOfMemory, "Failed to allocate byte array");
    }
    
    return ptr;
}

bool QtStringWrapperMASM::deleteByteArray(void* qba_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return false;
    }
    
    uint32_t result = wrapper_qbytearray_delete(qba_ptr);
    return result == 0;
}

QtStringOpResult QtStringWrapperMASM::appendToByteArray(void* qba_ptr, const QByteArray& data) {
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!qba_ptr) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    result = wrapper_qbytearray_append(qba_ptr, reinterpret_cast<const uint8_t*>(data.constData()), data.length());
    
    return result;
}

bool QtStringWrapperMASM::clearByteArray(void* qba_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return false;
    }
    
    uint32_t result = wrapper_qbytearray_clear(qba_ptr);
    return result == 0;
}

uint32_t QtStringWrapperMASM::getByteArrayLength(void* qba_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return 0;
    }
    
    return wrapper_qbytearray_length(qba_ptr);
}

QByteArray QtStringWrapperMASM::getByteArrayAsQByteArray(void* qba_ptr) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return QByteArray();
    }
    
    uint32_t length = wrapper_qbytearray_length(qba_ptr);
    QByteArray result;
    result.resize(length);
    
    for (uint32_t i = 0; i < length; ++i) {
        result[i] = static_cast<char>(wrapper_qbytearray_at(qba_ptr, i));
    }
    
    return result;
}

bool QtStringWrapperMASM::setByteArrayFromQByteArray(void* qba_ptr, const QByteArray& data) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return false;
    }
    
    // Clear first
    if (wrapper_qbytearray_clear(qba_ptr) != 0) {
        setError(ErrorCode::InvalidParameter, "Failed to clear byte array");
        return false;
    }
    
    // Append data
    QtStringOpResult result = wrapper_qbytearray_append(
        qba_ptr, reinterpret_cast<const uint8_t*>(data.constData()), data.length());
    
    return result.success != 0;
}

uint8_t QtStringWrapperMASM::getByteAt(void* qba_ptr, uint32_t position) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return 0;
    }
    
    return wrapper_qbytearray_at(qba_ptr, position);
}

bool QtStringWrapperMASM::setByteAt(void* qba_ptr, uint32_t position, uint8_t value) {
    QMutexLocker lock(&m_mutex);
    
    if (!qba_ptr) {
        setError(ErrorCode::InvalidPtr, "Null byte array pointer");
        return false;
    }
    
    uint32_t result = wrapper_qbytearray_set(qba_ptr, position, value);
    return result == 0;
}

// =============================================================================
// QFile Operations
// =============================================================================

QtFileHandle* QtStringWrapperMASM::createFileHandle(const QString& filepath) {
    QMutexLocker lock(&m_mutex);
    
    if (filepath.isEmpty()) {
        setError(ErrorCode::InvalidParameter, "Empty filepath");
        return nullptr;
    }
    
    QByteArray path_utf8 = filepath.toUtf8();
    QtFileHandle* file = wrapper_qfile_create(path_utf8.constData());
    
    if (!file) {
        setError(ErrorCode::OutOfMemory, "Failed to allocate file handle");
    }
    
    return file;
}

bool QtStringWrapperMASM::deleteFileHandle(QtFileHandle* file) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return false;
    }
    
    uint32_t result = wrapper_qfile_delete(file);
    return result == 0;
}

QtStringOpResult QtStringWrapperMASM::openFile(
    QtFileHandle* file, const QString& filepath, FileOpenMode mode) {
    
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!file) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    QByteArray path_utf8 = filepath.toUtf8();
    result = wrapper_qfile_open(file, path_utf8.constData(), static_cast<uint32_t>(mode));
    
    return result;
}

bool QtStringWrapperMASM::closeFile(QtFileHandle* file) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return false;
    }
    
    uint32_t result = wrapper_qfile_close(file);
    return result == 0;
}

QtStringOpResult QtStringWrapperMASM::readFile(QtFileHandle* file, QByteArray& output, uint32_t max_size) {
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!file) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    uint8_t* buffer = new uint8_t[max_size];
    result = wrapper_qfile_read(file, buffer, max_size);
    
    if (result.success) {
        output = QByteArray(reinterpret_cast<const char*>(buffer), result.length);
    }
    
    delete[] buffer;
    return result;
}

QtStringOpResult QtStringWrapperMASM::writeFile(QtFileHandle* file, const QByteArray& data) {
    QMutexLocker lock(&m_mutex);
    
    QtStringOpResult result = {0, 0, 0, 0};
    
    if (!file) {
        result.error_code = static_cast<uint32_t>(ErrorCode::InvalidPtr);
        return result;
    }
    
    result = wrapper_qfile_write(file, reinterpret_cast<const uint8_t*>(data.constData()), data.length());
    
    return result;
}

int64_t QtStringWrapperMASM::seekFile(QtFileHandle* file, int64_t offset) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return -1;
    }
    
    return wrapper_qfile_seek(file, offset);
}

int64_t QtStringWrapperMASM::tellFile(QtFileHandle* file) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return -1;
    }
    
    return wrapper_qfile_tell(file);
}

QByteArray QtStringWrapperMASM::readFileAll(QtFileHandle* file) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return QByteArray();
    }
    
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    QtStringOpResult result = wrapper_qfile_read_all(file, &buffer, &size);
    
    QByteArray output;
    if (result.success && buffer) {
        output = QByteArray(reinterpret_cast<const char*>(buffer), size);
    }
    
    return output;
}

bool QtStringWrapperMASM::fileExists(const QString& filepath) {
    QMutexLocker lock(&m_mutex);
    
    QByteArray path_utf8 = filepath.toUtf8();
    uint32_t result = wrapper_qfile_exists(path_utf8.constData());
    
    return result != 0;
}

bool QtStringWrapperMASM::removeFile(const QString& filepath) {
    QMutexLocker lock(&m_mutex);
    
    QByteArray path_utf8 = filepath.toUtf8();
    uint32_t result = wrapper_qfile_remove(path_utf8.constData());
    
    return result == 0;
}

bool QtStringWrapperMASM::renameFile(const QString& oldpath, const QString& newpath) {
    QMutexLocker lock(&m_mutex);
    
    QByteArray old_utf8 = oldpath.toUtf8();
    QByteArray new_utf8 = newpath.toUtf8();
    uint32_t result = wrapper_qfile_rename(old_utf8.constData(), new_utf8.constData());
    
    return result == 0;
}

int64_t QtStringWrapperMASM::getFileSize(QtFileHandle* file) {
    QMutexLocker lock(&m_mutex);
    
    if (!file) {
        setError(ErrorCode::InvalidPtr, "Null file handle");
        return -1;
    }
    
    return wrapper_qfile_size(file);
}

// =============================================================================
// Utility Methods
// =============================================================================

QString QtStringWrapperMASM::getTempDirectory() {
    QMutexLocker lock(&m_mutex);
    return m_temp_dir;
}

QString QtStringWrapperMASM::generateTempFilePath(const QString& prefix) {
    QMutexLocker lock(&m_mutex);
    
    QString filename = prefix + "_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    return m_temp_dir + "/" + filename;
}

QtStringStatistics QtStringWrapperMASM::getStatistics() {
    QMutexLocker lock(&m_mutex);
    
    QtStringStatistics stats = {0, 0, 0, 0, 0, 0, 0, 0};
    wrapper_qt_get_statistics(&stats);
    
    return stats;
}

bool QtStringWrapperMASM::clearStatistics() {
    QMutexLocker lock(&m_mutex);
    
    uint32_t result = wrapper_qt_clear_statistics();
    return result == 0;
}

QString QtStringWrapperMASM::encodingToString(StringEncoding enc) const {
    switch (enc) {
        case StringEncoding::UTF8:
            return "UTF-8";
        case StringEncoding::UTF16LE:
            return "UTF-16LE";
        case StringEncoding::UTF16BE:
            return "UTF-16BE";
        case StringEncoding::ASCII:
            return "ASCII";
        case StringEncoding::LATIN1:
            return "Latin1";
        default:
            return "Unknown";
    }
}
