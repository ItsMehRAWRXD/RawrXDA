#pragma once
/*
 * QtReplacements.hpp
 * Pure C++20 replacements for Qt Framework
 * NO Qt dependencies - pure Win32 and STL only
 * NO logging or instrumentation
 */

#ifndef QTREPLACEMENTS_HPP
#define QTREPLACEMENTS_HPP

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <array>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <thread>

// ============================================================================
// QSTRING - Proper UTF-16 wrapper around std::wstring
// ============================================================================

class QString {
public:
    QString() = default;
    QString(const wchar_t* s) : m_data(s ? s : L"") {}
    QString(const std::wstring& s) : m_data(s) {}
    QString(const std::string& s) {
        if (s.empty()) {
            m_data.clear();
            return;
        }
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            m_data.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &m_data[0], wlen);
        }
    }
    explicit QString(const char* s) : QString(s ? std::string(s) : std::string("")) {}
    explicit QString(int n) : m_data(std::to_wstring(n)) {}
    explicit QString(double d) {
        wchar_t buf[64];
        swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.6g", d);
        m_data = buf;
    }
    
    static QString fromUtf8(const char* s) { return s ? QString(std::string(s)) : QString(); }
    static QString fromStdWString(const std::wstring& s) { return QString(s); }
    static QString fromStdString(const std::string& s) { return QString(s); }
    static QString number(int n) { return QString(n); }
    static QString number(double n, char format = 'g', int precision = 6) {
        wchar_t buf[64];
        if (format == 'f') {
            swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.*f", precision, n);
        } else {
            swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.*g", precision, n);
        }
        return QString(buf);
    }
    
    std::string toStdString() const {
        if (m_data.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, m_data.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) return "";
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, m_data.c_str(), -1, &s[0], len, nullptr, nullptr);
        return s;
    }
    
    std::string toUtf8() const { return toStdString(); }
    std::wstring toStdWString() const { return m_data; }
    const wchar_t* utf16() const { return m_data.c_str(); }
    const wchar_t* constData() const { return m_data.c_str(); }
    wchar_t* data() { return m_data.data(); }
    
    bool isEmpty() const { return m_data.empty(); }
    bool isNull() const { return m_data.empty(); }
    int length() const { return static_cast<int>(m_data.length()); }
    int size() const { return static_cast<int>(m_data.length()); }
    void clear() { m_data.clear(); }
    
    QString& append(const QString& s) { m_data += s.m_data; return *this; }
    QString& append(const std::string& s) { return append(QString(s)); }
    QString& append(wchar_t c) { m_data += c; return *this; }
    
    bool operator==(const QString& other) const { return m_data == other.m_data; }
    bool operator!=(const QString& other) const { return m_data != other.m_data; }
    bool operator<(const QString& other) const { return m_data < other.m_data; }
    bool operator<=(const QString& other) const { return m_data <= other.m_data; }
    bool operator>(const QString& other) const { return m_data > other.m_data; }
    bool operator>=(const QString& other) const { return m_data >= other.m_data; }
    
    QString arg(const QString& a) const {
        std::wstring res = m_data;
        size_t pos = res.find(L"%1");
        if (pos != std::wstring::npos) res.replace(pos, 2, a.m_data);
        return QString(res);
    }
    
    QString arg(int a) const { return arg(QString::number(a)); }
    QString arg(double a) const { return arg(QString::number(a)); }
    QString arg(const std::string& a) const { return arg(QString(a)); }
    
    QString mid(int pos, int n = -1) const {
        if (pos < 0 || pos >= (int)m_data.length()) return QString();
        if (n < 0) return QString(m_data.substr(pos));
        return QString(m_data.substr(pos, std::min((size_t)n, m_data.length() - pos)));
    }
    
    int indexOf(const QString& s, int from = 0) const {
        if (from < 0 || from >= (int)m_data.length()) return -1;
        auto p = m_data.find(s.m_data, from);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }
    
    int indexOf(wchar_t c, int from = 0) const {
        if (from < 0 || from >= (int)m_data.length()) return -1;
        auto p = m_data.find(c, from);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }

    int lastIndexOf(const QString& s) const {
        auto p = m_data.rfind(s.m_data);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }

    bool startsWith(const QString& s) const { return m_data.find(s.m_data) == 0; }
    bool endsWith(const QString& s) const {
        if (s.m_data.length() > m_data.length()) return false;
        return m_data.compare(m_data.length() - s.m_data.length(), s.m_data.length(), s.m_data) == 0;
    }

    QString trimmed() const {
        const wchar_t* ws = L" \t\n\r\f\v";
        size_t start = m_data.find_first_not_of(ws);
        if (start == std::wstring::npos) return QString();
        size_t end = m_data.find_last_not_of(ws);
        return QString(m_data.substr(start, end - start + 1));
    }

    std::vector<QString> split(const QString& sep, bool skipEmpty = false) const {
        std::vector<QString> res;
        if (sep.m_data.empty()) { if (!skipEmpty) res.push_back(*this); return res; }
        size_t start = 0, pos;
        while ((pos = m_data.find(sep.m_data, start)) != std::wstring::npos) {
            std::wstring part = m_data.substr(start, pos - start);
            if (!skipEmpty || !part.empty()) res.push_back(QString(part));
            start = pos + sep.m_data.length();
        }
        std::wstring part = m_data.substr(start);
        if (!skipEmpty || !part.empty()) res.push_back(QString(part));
        return res;
    }
    
    QString replace(const QString& before, const QString& after) const {
        std::wstring res = m_data;
        size_t pos = 0;
        while ((pos = res.find(before.m_data, pos)) != std::wstring::npos) {
            res.replace(pos, before.m_data.length(), after.m_data);
            pos += after.m_data.length();
        }
        return QString(res);
    }
    
    bool contains(const QString& s) const { return m_data.find(s.m_data) != std::wstring::npos; }
    
    int toInt(bool* ok = nullptr) const {
        try {
            int val = std::stoi(m_data);
            if (ok) *ok = true;
            return val;
        } catch (...) {
            if (ok) *ok = false;
            return 0;
        }
    }
    
    double toDouble(bool* ok = nullptr) const {
        try {
            double val = std::stod(m_data);
            if (ok) *ok = true;
            return val;
        } catch (...) {
            if (ok) *ok = false;
            return 0.0;
        }
    }

private:
    std::wstring m_data;
};

namespace Qt {
    enum Initialization { Uninitialized };
}

class QByteArray {
public:
    QByteArray() = default;
    QByteArray(const char* data, int len) {
        if (data && len > 0) {
            bytes_.assign(data, data + len);
        }
    }
    explicit QByteArray(const std::string& s)
        : bytes_(s.begin(), s.end()) {}
    explicit QByteArray(std::size_t size, char fill = '\0')
        : bytes_(size, fill) {}
    QByteArray(std::size_t size, Qt::Initialization)
        : bytes_(size) {}
    QByteArray(int size, Qt::Initialization)
        : bytes_(size > 0 ? static_cast<std::size_t>(size) : 0U) {}

    bool isEmpty() const { return bytes_.empty(); }
    bool empty() const { return bytes_.empty(); }
    int size() const { return static_cast<int>(bytes_.size()); }
    std::size_t length() const { return bytes_.size(); }

    const char* constData() const { return bytes_.empty() ? nullptr : bytes_.data(); }
    char* data() { return bytes_.empty() ? nullptr : bytes_.data(); }

    void resize(std::size_t size) { bytes_.resize(size); }
    void clear() { bytes_.clear(); }

    QByteArray& append(const QByteArray& other) {
        bytes_.insert(bytes_.end(), other.bytes_.begin(), other.bytes_.end());
        return *this;
    }
    QByteArray& append(const char* data, int len) {
        if (data && len > 0) {
            bytes_.insert(bytes_.end(), data, data + len);
        }
        return *this;
    }

    std::string toStdString() const {
        return std::string(bytes_.begin(), bytes_.end());
    }

    static QByteArray number(std::size_t value) {
        const std::string s = std::to_string(value);
        return QByteArray(s);
    }

private:
    std::vector<char> bytes_;
};

using QStringList = std::vector<QString>;
using QByteArrayList = std::vector<QByteArray>;

// ============================================================================
// QVARIANT - Simple value wrapper
// ============================================================================

class QVariant {
public:
    enum Type { Invalid, Bool, Int, Double, String, StringList, ByteArray, Map };
    
    QVariant() : m_type(Invalid) {}
    explicit QVariant(bool b) : m_type(Bool), m_value(b ? "1" : "0") {}
    explicit QVariant(int i) : m_type(Int), m_value(std::to_string(i)) {}
    explicit QVariant(double d) : m_type(Double), m_value(std::to_string(d)) {}
    explicit QVariant(const QString& s) : m_type(String), m_value(s.toStdString()) {}
    explicit QVariant(const std::string& s) : m_type(String), m_value(s) {}
    explicit QVariant(const char* s) : m_type(String), m_value(s ? s : "") {}
    explicit QVariant(const QStringList& l) : m_type(StringList) {
        for (const auto& s : l) {
            if (!m_value.empty()) m_value += "|";
            m_value += s.toStdString();
        }
    }
    
    bool toBool() const { return m_type == Bool && m_value == "1"; }
    int toInt() const { 
        try { return m_type == Int ? std::stoi(m_value) : 0; } 
        catch (...) { return 0; } 
    }
    double toDouble() const { 
        try { return m_type == Double ? std::stod(m_value) : 0.0; } 
        catch (...) { return 0.0; } 
    }
    QString toString() const { return QString::fromStdString(m_value); }
    std::string toStdString() const { return m_value; }
    
    bool isValid() const { return m_type != Invalid; }
    bool isNull() const { return m_value.empty(); }
    Type type() const { return m_type; }

private:
    Type m_type;
    std::string m_value;
};

// ============================================================================
// CONTAINER REPLACEMENTS
// ============================================================================

template <typename T>
using QList = std::vector<T>;

template <typename T>
using QVector = std::vector<T>;

template <typename K, typename V>
using QHash = std::unordered_map<K, V>;

template <typename K, typename V>
using QMap = std::map<K, V>;

template <typename T>
using QSet = std::set<T>;

template <typename T>
using QQueue = std::queue<T>;

template <typename T>
using QStack = std::stack<T>;

using QJsonObject = std::unordered_map<std::string, std::string>;
using QJsonArray = std::vector<std::string>;

// ============================================================================
// POINTER REPLACEMENTS
// ============================================================================

template <typename T>
using QPointer = T*;

template <typename T>
using QSharedPointer = std::shared_ptr<T>;

template <typename T>
using QWeakPointer = std::weak_ptr<T>;

template <typename T>
using QScopedPointer = std::unique_ptr<T>;

// ============================================================================
// QOBJECT - Minimal signals/slots stub
// ============================================================================

class QObject {
public:
    virtual ~QObject() = default;
    
protected:
    std::unordered_map<std::string, std::vector<std::function<void()>>> m_signals;

public:
    void emitSignal(const std::string& signalName) {
        auto it = m_signals.find(signalName);
        if (it != m_signals.end()) {
            for (auto& handler : it->second) {
                try { handler(); } catch (...) {}
            }
        }
    }

    void connectSignal(const std::string& signalName, std::function<void()> handler) {
        m_signals[signalName].push_back(handler);
    }
};

#define Q_OBJECT
#define Q_SIGNAL void
#define Q_SLOT void
#define signals protected
#define slots
#define emit

// ============================================================================
// QEVENT, QRECT, QSIZE, QPOINT - Geometry classes
// ============================================================================

struct QPoint {
    int x = 0, y = 0;
    QPoint() = default;
    QPoint(int x_, int y_) : x(x_), y(y_) {}
};

struct QSize {
    int width = 0, height = 0;
    QSize() = default;
    QSize(int w, int h) : width(w), height(h) {}
};

struct QRect {
    int x = 0, y = 0, width = 0, height = 0;
    QRect() = default;
    QRect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    bool contains(const QPoint& p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }
    QPoint topLeft() const { return QPoint(x, y); }
    QSize size() const { return QSize(width, height); }
};

// ============================================================================
// QFILE, QDIR, QFILEINFO - File system wrappers
// ============================================================================

class QFileInfo {
private:
    std::wstring m_path;
    
public:
    QFileInfo() = default;
    explicit QFileInfo(const std::wstring& path) : m_path(path) {}
    explicit QFileInfo(const std::string& path) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            m_wstring.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &m_path[0], wlen);
        }
    }
    explicit QFileInfo(const QString& path) : m_path(path.toStdWString()) {}
    
    QString filePath() const { return QString(m_path); }
    QString fileName() const {
        auto pos = m_path.find_last_of(L"\\/");
        return pos == std::wstring::npos ? QString(m_path) : QString(m_path.substr(pos + 1));
    }
    QString path() const {
        auto pos = m_path.find_last_of(L"\\/");
        return pos == std::wstring::npos ? QString(L"") : QString(m_path.substr(0, pos));
    }
    QString baseName() const {
        auto fname = fileName().toStdWString();
        auto dot = fname.find_last_of(L'.');
        return dot == std::wstring::npos ? QString(fname) : QString(fname.substr(0, dot));
    }
    QString suffix() const {
        auto fname = fileName().toStdWString();
        auto dot = fname.find_last_of(L'.');
        return dot == std::wstring::npos ? QString(L"") : QString(fname.substr(dot + 1));
    }
    
    bool exists() const {
        return GetFileAttributesW(m_path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    bool isFile() const {
        DWORD attr = GetFileAttributesW(m_path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }
    bool isDir() const {
        DWORD attr = GetFileAttributesW(m_path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

private:
    std::wstring m_wstring;  // temp for conversion
};

class QDir {
public:
    static bool mkpath(const QString& path) {
        return CreateDirectoryW(path.utf16(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    static bool exists(const QString& path) {
        DWORD attr = GetFileAttributesW(path.utf16());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }
    static QString homePath() {
        const wchar_t* home = _wgetenv(L"USERPROFILE");
        return QString(home ? home : L"C:\\");
    }
    static QString tempPath() {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        return QString(tmp);
    }
};

class QFile {
private:
    std::wstring m_path;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    
public:
    QFile() = default;
    explicit QFile(const QString& path) : m_path(path.toStdWString()) {}
    explicit QFile(const std::string& path) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            m_path.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &m_path[0], wlen);
        }
    }
    
    ~QFile() { close(); }
    
    bool open(int flags) {
        DWORD access = (flags & 1) ? GENERIC_READ : GENERIC_WRITE;  // 1 = read, 2 = write
        m_handle = CreateFileW(m_path.c_str(), access, 0, nullptr, 
                                (flags & 2) ? CREATE_ALWAYS : OPEN_EXISTING, 
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        return m_handle != INVALID_HANDLE_VALUE;
    }
    
    bool exists() const {
        return GetFileAttributesW(m_path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    
    void close() {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }
    
    bool remove() {
        close();
        return DeleteFileW(m_path.c_str()) != 0;
    }
    
    std::string readAll() {
        if (m_handle == INVALID_HANDLE_VALUE) return "";
        LARGE_INTEGER size;
        GetFileSizeEx(m_handle, &size);
        std::string content(size.QuadPart, '\0');
        DWORD read;
        ReadFile(m_handle, &content[0], size.QuadPart, &read, nullptr);
        return content;
    }
    
    bool write(const std::string& data) {
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DWORD written;
        return WriteFile(m_handle, data.c_str(), data.size(), &written, nullptr) != 0;
    }
    
    bool write(const char* data) {
        return write(data ? std::string(data) : std::string());
    }

    enum OpenMode { NotOpen = 0, ReadOnly = 1, WriteOnly = 2, ReadWrite = 3, Append = 4, Text = 8, Unbuffered = 16 };
};

// ============================================================================
// QIODevice mode constants
// ============================================================================

namespace QIODevice {
    static constexpr int NotOpen = 0;
    static constexpr int ReadOnly = 1;
    static constexpr int WriteOnly = 2;
    static constexpr int ReadWrite = 3;
    static constexpr int Append = 4;
    static constexpr int Text = 8;
}

// ============================================================================
// QPROCESS - Process execution stub
// ============================================================================

class QProcess : public QObject {
public:
    QProcess() = default;
    ~QProcess() { kill(); }
    
    void start(const QString& program, const QStringList& args) {
        std::wstring cmdline = program.toStdWString();
        for (const auto& arg : args) {
            cmdline += L" ";
            cmdline += arg.toStdWString();
        }
        
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        
        if (CreateProcessW(nullptr, &cmdline[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            m_processId = pi.dwProcessId;
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    
    void kill() { if (m_processId) TerminateProcess(OpenProcess(PROCESS_TERMINATE, FALSE, m_processId), 1); }
    bool waitForFinished(int msecs = -1) { return true; }  // Stub
    QString readAllStandardOutput() { return QString(); }
    QString readAllStandardError() { return QString(); }

private:
    DWORD m_processId = 0;
};

// ============================================================================
// QTIMER, QTHREAD - Threading stubs
// ============================================================================

class QTimer {
public:
    void start(int msec) { m_running = true; }
    void stop() { m_running = false; }
    bool isActive() const { return m_running; }
    void setInterval(int msec) { m_interval = msec; }
    
protected:
    int m_interval = 0;
    bool m_running = false;
};

class QThread {
public:
    virtual ~QThread() = default;
    virtual void run() {}
    void start() { m_thread = std::thread(&QThread::run, this); }
    void wait() { if (m_thread.joinable()) m_thread.join(); }
    bool isRunning() const { return m_thread.joinable(); }
    
private:
    std::thread m_thread;
};

// ============================================================================
// QCOLOR, QFONT, QBRUSH, QPEN - Graphics stubs
// ============================================================================

struct QColor {
    uint32_t rgba = 0;
    QColor() = default;
    QColor(int r, int g, int b, int a = 255) {
        rgba = (a << 24) | (r << 16) | (g << 8) | b;
    }
    static QColor fromRgb(int r, int g, int b) { return QColor(r, g, b); }
};

struct QFont {
    QString family;
    int pointSize = 12;
    bool bold = false;
    bool italic = false;
};

// ============================================================================
// QSTANDARDPATHS - Standard paths
// ============================================================================

namespace QStandardPaths {
    enum StandardLocation { DocumentsLocation = 0, AppDataLocation = 1, TempLocation = 2, HomeLocation = 3 };
    
    inline QString writableLocation(StandardLocation location) {
        wchar_t path[MAX_PATH];
        switch (location) {
            case AppDataLocation: {
                if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
                    return QString(path);
                }
                break;
            }
            case TempLocation: {
                GetTempPathW(MAX_PATH, path);
                return QString(path);
            }
            case HomeLocation: {
                const wchar_t* home = _wgetenv(L"USERPROFILE");
                return QString(home ? home : L"C:\\");
            }
            default: return QString();
        }
        return QString();
    }
}

// ============================================================================
// QENVIRONMENT - Environment variables
// ============================================================================

inline QString qEnvironmentVariable(const char* varName) {
    if (!varName) return QString();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, varName, -1, nullptr, 0);
    if (wlen <= 1) return QString();
    std::wstring wvar(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, varName, -1, &wvar[0], wlen);
    
    wchar_t buf[32768];
    DWORD len = GetEnvironmentVariableW(wvar.c_str(), buf, sizeof(buf)/sizeof(wchar_t));
    return len > 0 ? QString(buf) : QString();
}

#endif // QTREPLACEMENTS_HPP
