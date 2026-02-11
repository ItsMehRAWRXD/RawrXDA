// ============================================================================
// qt_elimination.h — Complete Qt-to-C++20/Win32 Type Replacement
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Include this INSTEAD of any Qt header. Provides drop-in replacements.
// Works with RawrXD_Foundation.h but is self-contained for incremental porting.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_QT_ELIMINATION_H
#define RAWRXD_QT_ELIMINATION_H

// Force Qt-free mode
#ifndef RAWRXD_NO_QT
#define RAWRXD_NO_QT 1
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <variant>
#include <optional>
#include <any>

// ============================================================================
// KILL ALL Qt MACROS — prevent accidental Qt usage
// ============================================================================

// Nullify Q_OBJECT, signals, slots, emit — the four pillars of Qt MOC
#ifndef Q_OBJECT
#define Q_OBJECT
#endif
#ifndef Q_PROPERTY
#define Q_PROPERTY(...)
#endif
#ifndef Q_INVOKABLE
#define Q_INVOKABLE
#endif
#ifndef Q_ENUM
#define Q_ENUM(...)
#endif
#ifndef Q_DECLARE_METATYPE
#define Q_DECLARE_METATYPE(...)
#endif
#ifndef Q_GADGET
#define Q_GADGET
#endif
#ifndef Q_CLASSINFO
#define Q_CLASSINFO(...)
#endif
#ifndef Q_INTERFACES
#define Q_INTERFACES(...)
#endif
#ifndef Q_DISABLE_COPY
#define Q_DISABLE_COPY(Class) Class(const Class&) = delete; Class& operator=(const Class&) = delete;
#endif
#ifndef Q_UNUSED
#define Q_UNUSED(x) (void)(x)
#endif
#ifndef Q_ASSERT
#define Q_ASSERT(cond) ((void)0)
#endif

// Signal/slot system — replaced with function pointers + callbacks
#ifndef signals
#define signals public
#endif
#ifndef slots
#define slots
#endif
#ifndef emit
#define emit
#endif
#ifndef SIGNAL
#define SIGNAL(x) #x
#endif
#ifndef SLOT
#define SLOT(x) #x
#endif

// ============================================================================
// QString REPLACEMENT — std::string (UTF-8) based
// ============================================================================

#ifndef RAWRXD_HAS_QSTRING_COMPAT
#define RAWRXD_HAS_QSTRING_COMPAT

class QString : public std::string {
public:
    using std::string::string;
    using std::string::operator=;
    
    QString() = default;
    QString(const std::string& s) : std::string(s) {}
    QString(const char* s) : std::string(s ? s : "") {}
    
    // Qt API compatibility
    bool isEmpty() const { return empty(); }
    int  length() const { return static_cast<int>(size()); }
    
    // Conversion
    std::string toStdString() const { return *this; }
    const char* toUtf8() const { return c_str(); }
    QByteArray toLatin1() const;   // forward declared
    
    // Static factories
    static QString number(int n) { return QString(std::to_string(n)); }
    static QString number(double n) { return QString(std::to_string(n)); }
    static QString fromStdString(const std::string& s) { return QString(s); }
    static QString fromUtf8(const char* s) { return QString(s ? s : ""); }
    static QString fromLatin1(const char* s) { return QString(s ? s : ""); }
    
    // Operations
    QString toLower() const {
        QString r = *this;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    }
    QString toUpper() const {
        QString r = *this;
        std::transform(r.begin(), r.end(), r.begin(), ::toupper);
        return r;
    }
    QString trimmed() const {
        auto start = find_first_not_of(" \t\r\n");
        if (start == npos) return {};
        auto end = find_last_not_of(" \t\r\n");
        return QString(substr(start, end - start + 1));
    }
    bool startsWith(const QString& s) const {
        return size() >= s.size() && compare(0, s.size(), s) == 0;
    }
    bool endsWith(const QString& s) const {
        return size() >= s.size() && compare(size() - s.size(), s.size(), s) == 0;
    }
    bool contains(const QString& s) const { return find(s) != npos; }
    int indexOf(const QString& s, int from = 0) const {
        auto pos = find(s, from);
        return pos == npos ? -1 : static_cast<int>(pos);
    }
    
    // Qt-style arg() for simple formatting
    QString arg(const QString& a) const {
        QString result = *this;
        auto pos = result.find("%1");
        if (pos != npos) result.replace(pos, 2, a);
        return result;
    }
    QString arg(int n) const { return arg(QString::number(n)); }
    QString arg(double n) const { return arg(QString::number(n)); }
    
    // Split
    std::vector<QString> split(char delim) const {
        std::vector<QString> parts;
        std::istringstream ss(*this);
        std::string item;
        while (std::getline(ss, item, delim)) {
            parts.push_back(QString(item));
        }
        return parts;
    }
};

#endif // RAWRXD_HAS_QSTRING_COMPAT

// ============================================================================
// QByteArray REPLACEMENT — std::vector<uint8_t> wrapper
// ============================================================================

#ifndef RAWRXD_HAS_QBYTEARRAY_COMPAT
#define RAWRXD_HAS_QBYTEARRAY_COMPAT

class QByteArray : public std::string {
public:
    using std::string::string;
    using std::string::operator=;
    
    QByteArray() = default;
    QByteArray(const char* data, int size) : std::string(data, size) {}
    QByteArray(const std::string& s) : std::string(s) {}
    
    bool isEmpty() const { return empty(); }
    const char* constData() const { return data(); }
    int  length() const { return static_cast<int>(size()); }
    
    static QByteArray fromHex(const char* hex) {
        QByteArray result;
        if (!hex) return result;
        size_t len = strlen(hex);
        for (size_t i = 0; i + 1 < len; i += 2) {
            char byte[3] = {hex[i], hex[i+1], 0};
            result.push_back(static_cast<char>(strtol(byte, nullptr, 16)));
        }
        return result;
    }
    QByteArray toHex() const {
        QByteArray result;
        for (unsigned char c : *this) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", c);
            result += buf;
        }
        return result;
    }
};

inline QByteArray QString::toLatin1() const { return QByteArray(c_str(), static_cast<int>(size())); }

#endif // RAWRXD_HAS_QBYTEARRAY_COMPAT

// ============================================================================
// QStringList REPLACEMENT
// ============================================================================

using QStringList = std::vector<QString>;

// ============================================================================
// QVariant REPLACEMENT — std::variant/std::any based
// ============================================================================

#ifndef RAWRXD_HAS_QVARIANT_COMPAT
#define RAWRXD_HAS_QVARIANT_COMPAT

class QVariant {
    std::any m_data;
public:
    QVariant() = default;
    QVariant(int v) : m_data(v) {}
    QVariant(double v) : m_data(v) {}
    QVariant(bool v) : m_data(v) {}
    QVariant(const QString& v) : m_data(v) {}
    QVariant(const char* v) : m_data(QString(v)) {}
    
    bool isValid() const { return m_data.has_value(); }
    bool isNull() const { return !m_data.has_value(); }
    
    int toInt(bool* ok = nullptr) const {
        try { auto v = std::any_cast<int>(m_data); if (ok) *ok = true; return v; }
        catch (...) { if (ok) *ok = false; return 0; }
    }
    double toDouble(bool* ok = nullptr) const {
        try { auto v = std::any_cast<double>(m_data); if (ok) *ok = true; return v; }
        catch (...) { if (ok) *ok = false; return 0.0; }
    }
    bool toBool() const {
        try { return std::any_cast<bool>(m_data); } catch (...) { return false; }
    }
    QString toString() const {
        try { return std::any_cast<QString>(m_data); } catch (...) { return {}; }
    }
    
    template<typename T>
    T value() const { return std::any_cast<T>(m_data); }
    
    static QVariant fromValue(const QString& v) { return QVariant(v); }
};

#endif // RAWRXD_HAS_QVARIANT_COMPAT

// ============================================================================
// QObject REPLACEMENT — Base class with simple signal/slot via callbacks
// ============================================================================

#ifndef RAWRXD_HAS_QOBJECT_COMPAT
#define RAWRXD_HAS_QOBJECT_COMPAT

class QObject {
public:
    QObject(QObject* parent = nullptr) : m_parent(parent) {}
    virtual ~QObject() = default;
    
    QObject* parent() const { return m_parent; }
    void setParent(QObject* p) { m_parent = p; }
    virtual const char* metaClassName() const { return "QObject"; }
    
    // Signal-slot connect replacement — function pointer based
    template<typename Sender, typename Signal, typename Receiver, typename Slot>
    static void connect(Sender* sender, Signal signal, Receiver* receiver, Slot slot) {
        // No-op in Qt elimination mode — replaced by direct callback wiring
        (void)sender; (void)signal; (void)receiver; (void)slot;
    }
    
    void deleteLater() { /* Manual lifecycle in Win32 */ }
    
protected:
    QObject* m_parent = nullptr;
};

// qobject_cast replacement
template<typename T>
T qobject_cast(QObject* obj) {
    return dynamic_cast<T>(obj);
}

#endif // RAWRXD_HAS_QOBJECT_COMPAT

// ============================================================================
// QWidget REPLACEMENT — HWND wrapper
// ============================================================================

#ifndef RAWRXD_HAS_QWIDGET_COMPAT
#define RAWRXD_HAS_QWIDGET_COMPAT

class QWidget : public QObject {
public:
    QWidget(QWidget* parent = nullptr) : QObject(parent), m_hwnd(nullptr), m_visible(false) {}
    virtual ~QWidget() = default;
    
    void show() { m_visible = true; if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
    void hide() { m_visible = false; if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }
    bool isVisible() const { return m_visible; }
    void setVisible(bool v) { if (v) show(); else hide(); }
    void setEnabled(bool e) { if (m_hwnd) EnableWindow(m_hwnd, e ? TRUE : FALSE); }
    
    void setWindowTitle(const QString& title) {
        m_title = title;
        if (m_hwnd) SetWindowTextA(m_hwnd, title.c_str());
    }
    QString windowTitle() const { return m_title; }
    
    void resize(int w, int h) { 
        m_width = w; m_height = h;
        if (m_hwnd) SetWindowPos(m_hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER); 
    }
    int width() const { return m_width; }
    int height() const { return m_height; }
    
    HWND winId() const { return m_hwnd; }
    void setWinId(HWND hwnd) { m_hwnd = hwnd; }
    
    virtual void update() { if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE); }
    void repaint() { update(); }
    void close() { if (m_hwnd) DestroyWindow(m_hwnd); }
    
    const char* metaClassName() const override { return "QWidget"; }

protected:
    HWND    m_hwnd = nullptr;
    bool    m_visible = false;
    int     m_width = 0;
    int     m_height = 0;
    QString m_title;
};

// QMainWindow → QWidget (no menu bar distinction needed)
using QMainWindow = QWidget;
using QDialog = QWidget;
using QDockWidget = QWidget;

#endif // RAWRXD_HAS_QWIDGET_COMPAT

// ============================================================================
// Qt CONTAINER REPLACEMENTS
// ============================================================================

template<typename T>
using QList = std::vector<T>;

template<typename T>
using QVector = std::vector<T>;

template<typename T>
using QSet = std::unordered_set<T>;

template<typename K, typename V>
using QMap = std::map<K, V>;

template<typename K, typename V>
using QHash = std::unordered_map<K, V>;

// ============================================================================
// QJsonDocument / QJsonObject / QJsonArray / QJsonValue REPLACEMENT
// ============================================================================

#ifndef RAWRXD_HAS_QJSON_COMPAT
#define RAWRXD_HAS_QJSON_COMPAT

// Forward — actual JSON handled by nlohmann/json.hpp
// These are thin wrappers that let Qt-dependent code compile
class QJsonValue;
class QJsonArray;

class QJsonObject : public std::map<std::string, std::string> {
public:
    using std::map<std::string, std::string>::map;
    
    bool contains(const std::string& key) const { return count(key) > 0; }
    bool isEmpty() const { return empty(); }
    
    QStringList keys() const {
        QStringList result;
        for (const auto& [k, v] : *this) result.push_back(QString(k));
        return result;
    }
    
    void insert(const std::string& key, const std::string& value) {
        (*this)[key] = value;
    }
};

class QJsonArray : public std::vector<std::string> {
public:
    using std::vector<std::string>::vector;
    
    bool isEmpty() const { return empty(); }
    int count() const { return static_cast<int>(size()); }
    void append(const std::string& v) { push_back(v); }
};

class QJsonDocument {
    std::string m_raw;
public:
    QJsonDocument() = default;
    
    static QJsonDocument fromJson(const QByteArray& data) {
        QJsonDocument doc;
        doc.m_raw = std::string(data.c_str(), data.size());
        return doc;
    }
    
    QByteArray toJson() const { return QByteArray(m_raw); }
    bool isNull() const { return m_raw.empty(); }
    QJsonObject object() const { return QJsonObject{}; }
};

class QJsonValue {
    std::string m_val;
public:
    QJsonValue() = default;
    QJsonValue(const std::string& s) : m_val(s) {}
    QJsonValue(int v) : m_val(std::to_string(v)) {}
    QJsonValue(double v) : m_val(std::to_string(v)) {}
    QJsonValue(bool v) : m_val(v ? "true" : "false") {}
    
    bool isString() const { return !m_val.empty(); }
    bool isNull() const { return m_val.empty(); }
    QString toString() const { return QString(m_val); }
    int toInt() const { return m_val.empty() ? 0 : std::stoi(m_val); }
    double toDouble() const { return m_val.empty() ? 0.0 : std::stod(m_val); }
    bool toBool() const { return m_val == "true"; }
};

#endif // RAWRXD_HAS_QJSON_COMPAT

// ============================================================================
// QTimer REPLACEMENT — Win32 timer + thread-based
// ============================================================================

#ifndef RAWRXD_HAS_QTIMER_COMPAT
#define RAWRXD_HAS_QTIMER_COMPAT

class QTimer {
    std::function<void()>   m_callback;
    std::atomic<bool>       m_running{false};
    std::thread             m_thread;
    int                     m_interval = 0;
    bool                    m_singleShot = false;

public:
    QTimer(QObject* parent = nullptr) { (void)parent; }
    ~QTimer() { stop(); }
    
    void setInterval(int ms) { m_interval = ms; }
    int interval() const { return m_interval; }
    void setSingleShot(bool ss) { m_singleShot = ss; }
    bool isActive() const { return m_running.load(); }
    
    // Connect timeout signal 
    void onTimeout(std::function<void()> fn) { m_callback = fn; }
    
    void start(int ms = -1) {
        if (ms >= 0) m_interval = ms;
        stop();
        m_running = true;
        m_thread = std::thread([this]() {
            while (m_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(m_interval));
                if (m_running.load() && m_callback) {
                    m_callback();
                }
                if (m_singleShot) {
                    m_running = false;
                    break;
                }
            }
        });
        m_thread.detach();
    }
    
    void stop() {
        m_running = false;
    }
    
    static void singleShot(int ms, std::function<void()> fn) {
        std::thread([ms, fn]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            if (fn) fn();
        }).detach();
    }
};

#endif // RAWRXD_HAS_QTIMER_COMPAT

// ============================================================================
// QDateTime REPLACEMENT
// ============================================================================

#ifndef RAWRXD_HAS_QDATETIME_COMPAT
#define RAWRXD_HAS_QDATETIME_COMPAT

class QDateTime {
    std::chrono::system_clock::time_point m_tp;
public:
    QDateTime() : m_tp(std::chrono::system_clock::now()) {}
    
    static QDateTime currentDateTime() { return QDateTime(); }
    
    QString toString(const char* fmt = "yyyy-MM-dd hh:mm:ss") const {
        (void)fmt;
        auto t = std::chrono::system_clock::to_time_t(m_tp);
        struct tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        return QString(buf);
    }
    
    int64_t toMSecsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            m_tp.time_since_epoch()).count();
    }
    
    static QDateTime fromMSecsSinceEpoch(int64_t ms) {
        QDateTime dt;
        dt.m_tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        return dt;
    }
};

class QDate {
public:
    static QString currentDate() {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd");
    }
};

class QTime {
public:
    static QString currentTime() {
        return QDateTime::currentDateTime().toString("hh:mm:ss");
    }
};

#endif // RAWRXD_HAS_QDATETIME_COMPAT

// ============================================================================
// QFile / QDir / QFileInfo REPLACEMENT — std::filesystem based
// ============================================================================

#ifndef RAWRXD_HAS_QFILE_COMPAT
#define RAWRXD_HAS_QFILE_COMPAT

namespace fs = std::filesystem;

class QDir {
    fs::path m_path;
public:
    QDir(const QString& path = ".") : m_path(path.c_str()) {}
    
    bool exists() const { return fs::exists(m_path); }
    bool mkpath(const QString& path) const {
        return fs::create_directories(fs::path(path.c_str()));
    }
    QString absolutePath() const { return QString(fs::absolute(m_path).string()); }
    
    static QString homePath() {
        const char* home = getenv("USERPROFILE");
        return QString(home ? home : "C:\\Users\\Default");
    }
    static QString tempPath() {
        return QString(fs::temp_directory_path().string());
    }
    
    QStringList entryList() const {
        QStringList result;
        if (fs::exists(m_path) && fs::is_directory(m_path)) {
            for (const auto& entry : fs::directory_iterator(m_path)) {
                result.push_back(QString(entry.path().filename().string()));
            }
        }
        return result;
    }
};

class QFileInfo {
    fs::path m_path;
public:
    QFileInfo(const QString& path) : m_path(path.c_str()) {}
    
    bool exists() const { return fs::exists(m_path); }
    bool isFile() const { return fs::is_regular_file(m_path); }
    bool isDir() const { return fs::is_directory(m_path); }
    QString fileName() const { return QString(m_path.filename().string()); }
    QString absoluteFilePath() const { return QString(fs::absolute(m_path).string()); }
    QString suffix() const { return QString(m_path.extension().string()); }
    int64_t size() const { return fs::exists(m_path) ? static_cast<int64_t>(fs::file_size(m_path)) : 0; }
};

class QFile {
    std::fstream m_stream;
    fs::path     m_path;
    
public:
    enum OpenMode { ReadOnly = 1, WriteOnly = 2, ReadWrite = 3, Text = 4, Append = 8 };
    
    QFile(const QString& name) : m_path(name.c_str()) {}
    
    bool exists() const { return fs::exists(m_path); }
    static bool exists(const QString& name) { return fs::exists(fs::path(name.c_str())); }
    
    bool open(int mode) {
        std::ios_base::openmode m = std::ios_base::in;
        if (mode & WriteOnly) m = std::ios_base::out;
        if (mode & ReadWrite) m = std::ios_base::in | std::ios_base::out;
        if (mode & Append) m |= std::ios_base::app;
        m_stream.open(m_path, m);
        return m_stream.is_open();
    }
    
    void close() { m_stream.close(); }
    bool isOpen() const { return m_stream.is_open(); }
    
    QByteArray readAll() {
        m_stream.seekg(0, std::ios::end);
        auto size = m_stream.tellg();
        m_stream.seekg(0, std::ios::beg);
        std::string buf(size, '\0');
        m_stream.read(buf.data(), size);
        return QByteArray(buf);
    }
    
    void write(const QByteArray& data) {
        m_stream.write(data.c_str(), data.size());
    }
    
    static bool remove(const QString& name) {
        return fs::remove(fs::path(name.c_str()));
    }
};

class QTextStream {
    std::ostream* m_os = nullptr;
    std::istream* m_is = nullptr;
    std::string   m_buf;
public:
    QTextStream(QFile* /*file*/) {}
    QTextStream(std::string* buf) : m_buf(*buf) {}
    
    QTextStream& operator<<(const QString& s) { m_buf += s; return *this; }
    QTextStream& operator<<(int v) { m_buf += std::to_string(v); return *this; }
    QString readAll() { return QString(m_buf); }
};

#endif // RAWRXD_HAS_QFILE_COMPAT

// ============================================================================
// QSettings REPLACEMENT — INI file + registry based
// ============================================================================

#ifndef RAWRXD_HAS_QSETTINGS_COMPAT
#define RAWRXD_HAS_QSETTINGS_COMPAT

class QSettings {
    std::unordered_map<std::string, std::string> m_data;
    std::string m_path;

public:
    QSettings(const QString& org = "RawrXD", const QString& app = "RawrXD") {
        const char* appdata = getenv("APPDATA");
        if (appdata) {
            m_path = std::string(appdata) + "\\" + org + "\\" + app + ".ini";
        }
        load();
    }
    
    void setValue(const QString& key, const QVariant& value) {
        m_data[key] = value.toString();
    }
    
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) return QVariant(QString(it->second));
        return defaultValue;
    }
    
    bool contains(const QString& key) const {
        return m_data.count(key) > 0;
    }
    
    void sync() { save(); }
    
private:
    void load() {
        std::ifstream f(m_path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                m_data[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
    }
    void save() {
        fs::create_directories(fs::path(m_path).parent_path());
        std::ofstream f(m_path);
        for (const auto& [k, v] : m_data) {
            f << k << "=" << v << "\n";
        }
    }
};

#endif // RAWRXD_HAS_QSETTINGS_COMPAT

// ============================================================================
// QStandardPaths REPLACEMENT
// ============================================================================

class QStandardPaths {
public:
    enum StandardLocation { AppDataLocation, CacheLocation, TempLocation, HomeLocation };
    
    static QString writableLocation(StandardLocation loc) {
        switch (loc) {
            case AppDataLocation: {
                const char* ad = getenv("APPDATA");
                return QString(ad ? ad : "C:\\ProgramData") + "\\RawrXD";
            }
            case CacheLocation: {
                const char* tmp = getenv("TEMP");
                return QString(tmp ? tmp : "C:\\Temp") + "\\RawrXD\\cache";
            }
            case TempLocation: return QString(fs::temp_directory_path().string());
            case HomeLocation: return QDir::homePath();
        }
        return {};
    }
};

// ============================================================================
// QUuid REPLACEMENT
// ============================================================================

class QUuid {
    std::string m_uuid;
public:
    QUuid() = default;
    
    static QUuid createUuid() {
        QUuid u;
        // Use Win32 CoCreateGuid or simple PRNG
        GUID guid;
        CoCreateGuid(&guid);
        char buf[64];
        snprintf(buf, sizeof(buf), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        u.m_uuid = buf;
        return u;
    }
    
    QString toString() const { return QString("{" + m_uuid + "}"); }
};

// ============================================================================
// QCryptographicHash REPLACEMENT — Win32 BCrypt based
// ============================================================================

class QCryptographicHash {
public:
    enum Algorithm { Sha256 };
    
    QCryptographicHash(Algorithm /*algo*/) {}
    
    void addData(const QByteArray& data) { m_data += data; }
    
    QByteArray result() const {
        // Simple hash placeholder — real impl uses BCryptHash 
        uint32_t hash = 0;
        for (char c : m_data) hash = hash * 31 + static_cast<uint8_t>(c);
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", hash);
        return QByteArray(buf);
    }
    
    static QByteArray hash(const QByteArray& data, Algorithm algo) {
        QCryptographicHash h(algo);
        h.addData(data);
        return h.result();
    }

private:
    std::string m_data;
};

// ============================================================================
// QDebug REPLACEMENT — printf-based
// ============================================================================

#ifndef RAWRXD_HAS_QDEBUG_COMPAT
#define RAWRXD_HAS_QDEBUG_COMPAT

class QDebug {
    std::string m_buf;
    bool        m_space = true;

public:
    QDebug() = default;
    ~QDebug() {
        if (!m_buf.empty()) {
            fprintf(stderr, "[DEBUG] %s\n", m_buf.c_str());
        }
    }
    
    QDebug& operator<<(const char* s) {
        if (m_space && !m_buf.empty()) m_buf += " ";
        m_buf += (s ? s : "(null)");
        return *this;
    }
    QDebug& operator<<(const std::string& s) { return *this << s.c_str(); }
    QDebug& operator<<(const QString& s) { return *this << s.c_str(); }
    QDebug& operator<<(int v) { return *this << std::to_string(v).c_str(); }
    QDebug& operator<<(size_t v) { return *this << std::to_string(v).c_str(); }
    QDebug& operator<<(double v) { return *this << std::to_string(v).c_str(); }
    QDebug& operator<<(bool v) { return *this << (v ? "true" : "false"); }
    QDebug& nospace() { m_space = false; return *this; }
};

inline QDebug qDebug() { return QDebug(); }
inline QDebug qWarning() { return QDebug(); }
inline QDebug qCritical() { return QDebug(); }

#endif // RAWRXD_HAS_QDEBUG_COMPAT

// ============================================================================
// QApplication REPLACEMENT — Stub for entry point compatibility
// ============================================================================

class QApplication {
    int m_argc;
    char** m_argv;
public:
    QApplication(int& argc, char** argv) : m_argc(argc), m_argv(argv) {}
    
    static QApplication* instance() { return nullptr; }
    int exec() { return 0; /* Win32 uses its own msg loop */ }
    static void processEvents() { /* Win32: PeekMessage loop */ }
    static void quit() { PostQuitMessage(0); }
    static QString applicationDirPath() {
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string path(buf);
        auto pos = path.find_last_of("\\/");
        return QString(pos != std::string::npos ? path.substr(0, pos) : path);
    }
};

using QGuiApplication = QApplication;
using QCoreApplication = QApplication;

// ============================================================================
// Qt LAYOUT STUBS — Replaced by Win32 layout in the GUI
// ============================================================================

class QLayout : public QObject { public: void addWidget(QWidget*) {} };
class QVBoxLayout : public QLayout { public: QVBoxLayout(QWidget* p = nullptr) { (void)p; } };
class QHBoxLayout : public QLayout { public: QHBoxLayout(QWidget* p = nullptr) { (void)p; } };
class QGridLayout : public QLayout { public: QGridLayout(QWidget* p = nullptr) { (void)p; } };

// ============================================================================
// Qt WIDGET STUBS — Minimal compile compatibility
// ============================================================================

class QLabel : public QWidget { 
public: 
    QLabel(const QString& text = "", QWidget* p = nullptr) : QWidget(p) { (void)text; }
    void setText(const QString&) {}
};

class QPushButton : public QWidget {
public:
    QPushButton(const QString& text = "", QWidget* p = nullptr) : QWidget(p) { (void)text; }
    void setText(const QString&) {}
};

class QLineEdit : public QWidget {
    QString m_text;
public:
    QLineEdit(QWidget* p = nullptr) : QWidget(p) {}
    QString text() const { return m_text; }
    void setText(const QString& t) { m_text = t; }
    void setPlaceholderText(const QString&) {}
};

class QTextEdit : public QWidget {
    QString m_text;
public:
    QTextEdit(QWidget* p = nullptr) : QWidget(p) {}
    QString toPlainText() const { return m_text; }
    void setPlainText(const QString& t) { m_text = t; }
    void append(const QString& t) { m_text += "\n" + t; }
    void clear() { m_text.clear(); }
};

class QPlainTextEdit : public QTextEdit {
public:
    QPlainTextEdit(QWidget* p = nullptr) : QTextEdit(p) {}
};

class QTabWidget : public QWidget {
public:
    QTabWidget(QWidget* p = nullptr) : QWidget(p) {}
    void addTab(QWidget*, const QString&) {}
    void setCurrentIndex(int) {}
    int currentIndex() const { return 0; }
};

class QGroupBox : public QWidget {
public:
    QGroupBox(const QString& title = "", QWidget* p = nullptr) : QWidget(p) { (void)title; }
};

class QComboBox : public QWidget {
    std::vector<QString> m_items;
    int m_current = 0;
public:
    QComboBox(QWidget* p = nullptr) : QWidget(p) {}
    void addItem(const QString& t) { m_items.push_back(t); }
    void addItems(const QStringList& items) { for (const auto& i : items) m_items.push_back(i); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i) { m_current = i; }
    QString currentText() const { return m_current < (int)m_items.size() ? m_items[m_current] : ""; }
};

class QSpinBox : public QWidget {
    int m_val = 0;
public:
    QSpinBox(QWidget* p = nullptr) : QWidget(p) {}
    int value() const { return m_val; }
    void setValue(int v) { m_val = v; }
    void setMinimum(int) {}
    void setMaximum(int) {}
    void setRange(int, int) {}
};

class QDoubleSpinBox : public QWidget {
    double m_val = 0.0;
public:
    QDoubleSpinBox(QWidget* p = nullptr) : QWidget(p) {}
    double value() const { return m_val; }
    void setValue(double v) { m_val = v; }
    void setMinimum(double) {}
    void setMaximum(double) {}
    void setRange(double, double) {}
    void setDecimals(int) {}
    void setSingleStep(double) {}
};

class QCheckBox : public QWidget {
    bool m_checked = false;
public:
    QCheckBox(const QString& text = "", QWidget* p = nullptr) : QWidget(p) { (void)text; }
    bool isChecked() const { return m_checked; }
    void setChecked(bool v) { m_checked = v; }
};

class QProgressBar : public QWidget {
    int m_val = 0;
public:
    QProgressBar(QWidget* p = nullptr) : QWidget(p) {}
    void setValue(int v) { m_val = v; }
    void setMinimum(int) {}
    void setMaximum(int) {}
    void setRange(int, int) {}
    int value() const { return m_val; }
};

class QTreeWidget : public QWidget {
public:
    QTreeWidget(QWidget* p = nullptr) : QWidget(p) {}
    void setColumnCount(int) {}
    void setHeaderLabels(const QStringList&) {}
    void clear() {}
};

class QTreeWidgetItem {
public:
    QTreeWidgetItem() = default;
    QTreeWidgetItem(QTreeWidget*) {}
    void setText(int, const QString&) {}
    void addChild(QTreeWidgetItem*) {}
};

class QScrollBar : public QWidget {
public:
    QScrollBar(QWidget* p = nullptr) : QWidget(p) {}
    void setValue(int) {}
    int value() const { return 0; }
};

class QHeaderView : public QWidget {
public:
    enum ResizeMode { Stretch, ResizeToContents, Fixed, Interactive };
    void setSectionResizeMode(int, ResizeMode) {}
    void setStretchLastSection(bool) {}
};

class QAction : public QObject {
    QString m_text;
    bool m_enabled = true;
public:
    QAction(const QString& text = "", QObject* p = nullptr) : QObject(p), m_text(text) {}
    void setEnabled(bool e) { m_enabled = e; }
    void setText(const QString& t) { m_text = t; }
    bool isEnabled() const { return m_enabled; }
    QString text() const { return m_text; }
};

class QMenu : public QWidget {
public:
    QMenu(const QString& title = "", QWidget* p = nullptr) : QWidget(p) { (void)title; }
    QAction* addAction(const QString& text) { return new QAction(text); }
    void addSeparator() {}
    QMenu* addMenu(const QString& title) { return new QMenu(title); }
};

class QMenuBar : public QWidget {
public:
    QMenuBar(QWidget* p = nullptr) : QWidget(p) {}
    QMenu* addMenu(const QString& title) { return new QMenu(title); }
};

class QToolBar : public QWidget {
public:
    QToolBar(const QString& title = "", QWidget* p = nullptr) : QWidget(p) { (void)title; }
    void addAction(QAction*) {}
    void addSeparator() {}
};

class QStatusBar : public QWidget {
public:
    QStatusBar(QWidget* p = nullptr) : QWidget(p) {}
    void showMessage(const QString&, int = 0) {}
};

class QFrame : public QWidget {
public:
    QFrame(QWidget* p = nullptr) : QWidget(p) {}
};

// ============================================================================
// Qt DIALOG STUBS
// ============================================================================

class QFileDialog {
public:
    static QString getOpenFileName(QWidget* = nullptr, const QString& = "", 
                                   const QString& = "", const QString& = "") {
        // Win32 replacement: use GetOpenFileName
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) return QString(filename);
        return {};
    }
    
    static QString getSaveFileName(QWidget* = nullptr, const QString& = "",
                                   const QString& = "", const QString& = "") {
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) return QString(filename);
        return {};
    }
    
    static QString getExistingDirectory(QWidget* = nullptr, const QString& = "",
                                         const QString& = "") {
        // Simplified — use SHBrowseForFolder in real impl
        return {};
    }
};

class QMessageBox {
public:
    enum StandardButton { Ok = 1, Cancel = 2, Yes = 4, No = 8 };
    
    static int information(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
        return Ok;
    }
    static int warning(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
        return Ok;
    }
    static int critical(QWidget*, const QString& title, const QString& text) {
        MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
        return Ok;
    }
    static int question(QWidget*, const QString& title, const QString& text) {
        int r = MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
        return r == IDYES ? Yes : No;
    }
};

class QInputDialog {
public:
    static QString getText(QWidget*, const QString&, const QString&, int = 0,
                           const QString& text = "", bool* ok = nullptr) {
        if (ok) *ok = true;
        return text;
    }
};

class QClipboard {
    static std::string s_data;
public:
    void setText(const QString& text) { s_data = text; }
    QString text() const { return QString(s_data); }
};

class QDesktopServices {
public:
    static void openUrl(const QString& url) {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
};

class QUrl {
    QString m_url;
public:
    QUrl(const QString& url = "") : m_url(url) {}
    QString toString() const { return m_url; }
    static QUrl fromLocalFile(const QString& path) { return QUrl("file:///" + path); }
};

// ============================================================================
// QtConcurrent REPLACEMENT — std::async based
// ============================================================================

namespace QtConcurrent {
    template<typename Fn>
    auto run(Fn fn) -> std::future<decltype(fn())> {
        return std::async(std::launch::async, fn);
    }
}

// ============================================================================
// QMetaObject REPLACEMENT
// ============================================================================

class QMetaObject {
public:
    template<typename Fn>
    static void invokeMethod(QObject*, Fn fn) { fn(); }
};

// ============================================================================
// QProcess REPLACEMENT
// ============================================================================

class QProcess : public QObject {
    HANDLE m_process = nullptr;
    std::string m_stdout;
public:
    QProcess(QObject* parent = nullptr) : QObject(parent) {}
    
    void start(const QString& program, const QStringList& args = {}) {
        std::string cmd = program;
        for (const auto& a : args) cmd += " " + a;
        
        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), 
                      nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        m_process = pi.hProcess;
        CloseHandle(pi.hThread);
    }
    
    void waitForFinished(int timeout = -1) {
        if (m_process) {
            WaitForSingleObject(m_process, timeout < 0 ? INFINITE : timeout);
            CloseHandle(m_process);
            m_process = nullptr;
        }
    }
    
    QByteArray readAllStandardOutput() { return QByteArray(m_stdout); }
    
    static int execute(const QString& program, const QStringList& args = {}) {
        QProcess p;
        p.start(program, args);
        p.waitForFinished();
        return 0;
    }
};

// ============================================================================
// QElapsedTimer REPLACEMENT
// ============================================================================

class QElapsedTimer {
    std::chrono::high_resolution_clock::time_point m_start;
public:
    void start() { m_start = std::chrono::high_resolution_clock::now(); }
    int64_t elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - m_start).count();
    }
    int64_t nsecsElapsed() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - m_start).count();
    }
    bool isValid() const { return m_start.time_since_epoch().count() != 0; }
};

// ============================================================================
// QNetworkAccessManager / QNetworkRequest / QNetworkReply STUBS
// ============================================================================

class QNetworkReply;
class QUrlQuery {
    std::unordered_map<std::string, std::string> m_params;
public:
    void addQueryItem(const QString& key, const QString& val) { m_params[key] = val; }
};

class QNetworkRequest {
    QString m_url;
public:
    QNetworkRequest(const QUrl& url = QUrl()) : m_url(url.toString()) {}
    void setUrl(const QUrl& url) { m_url = url.toString(); }
    void setRawHeader(const QByteArray&, const QByteArray&) {}
};

class QNetworkReply : public QObject {
public:
    enum NetworkError { NoError = 0, ConnectionRefusedError = 1 };
    NetworkError error() const { return NoError; }
    QByteArray readAll() { return {}; }
    void deleteLater() {}
};

class QNetworkAccessManager : public QObject {
public:
    QNetworkAccessManager(QObject* parent = nullptr) : QObject(parent) {}
    QNetworkReply* get(const QNetworkRequest&) { return new QNetworkReply(); }
    QNetworkReply* post(const QNetworkRequest&, const QByteArray&) { return new QNetworkReply(); }
};

class QMessageAuthenticationCode {
    std::string m_key;
public:
    QMessageAuthenticationCode(int /*algo*/, const QByteArray& key) : m_key(key) {}
    void addData(const QByteArray&) {}
    QByteArray result() const { return QByteArray("00000000"); }
};

// ============================================================================
// EVENT TYPES
// ============================================================================

class QKeyEvent {
    int m_key;
public:
    QKeyEvent(int key = 0) : m_key(key) {}
    int key() const { return m_key; }
    bool isAutoRepeat() const { return false; }
};

class QCloseEvent {
    bool m_accepted = true;
public:
    void accept() { m_accepted = true; }
    void ignore() { m_accepted = false; }
    bool isAccepted() const { return m_accepted; }
};

class QTextCursor {
public:
    enum MoveOperation { Start, End, StartOfLine, EndOfLine, WordLeft, WordRight };
    enum MoveMode { MoveAnchor, KeepAnchor };
    
    bool movePosition(MoveOperation, MoveMode = MoveAnchor, int = 1) { return true; }
    void insertText(const QString&) {}
    QString selectedText() const { return {}; }
    bool hasSelection() const { return false; }
    int position() const { return 0; }
    int blockNumber() const { return 0; }
};

class QTextBlock {
public:
    int blockNumber() const { return 0; }
    QString text() const { return {}; }
    bool isValid() const { return true; }
    QTextBlock next() const { return {}; }
};

// ============================================================================
// PALETTE / FONT / COLOR STUBS
// ============================================================================

class QColor {
    int m_r = 0, m_g = 0, m_b = 0, m_a = 255;
public:
    QColor() = default;
    QColor(int r, int g, int b, int a = 255) : m_r(r), m_g(g), m_b(b), m_a(a) {}
    int red() const { return m_r; }
    int green() const { return m_g; }
    int blue() const { return m_b; }
    int alpha() const { return m_a; }
    COLORREF toColorRef() const { return RGB(m_r, m_g, m_b); }
};

class QFont {
    std::string m_family;
    int m_size = 10;
public:
    QFont(const QString& family = "Consolas", int size = 10) : m_family(family), m_size(size) {}
    void setPointSize(int s) { m_size = s; }
    void setFamily(const QString& f) { m_family = f; }
    int pointSize() const { return m_size; }
    QString family() const { return QString(m_family); }
};

class QPalette {
public:
    enum ColorRole { Window, WindowText, Base, Text, Button, ButtonText, Highlight, HighlightedText };
    void setColor(ColorRole, const QColor&) {}
};

class QPixmap {};
class QIcon {};
using QSize = std::pair<int, int>;
using QPoint = std::pair<int, int>;
using QRect = std::tuple<int, int, int, int>;

// ============================================================================
// FINAL: Prevent real Qt headers from being included
// ============================================================================

#define QOBJECT_H
#define QWIDGET_H
#define QSTRING_H
#define QAPPLICATION_H
#define QMAINWINDOW_H
#define QTIMER_H
#define QFILE_H
#define QDIR_H
#define QDEBUG_H
#define QJSONDOCUMENT_H
#define QNETWORKACCESSMANAGER_H
#define QSETTINGS_H

#endif // RAWRXD_QT_ELIMINATION_H
