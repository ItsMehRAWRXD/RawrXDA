// QtReplacements.hpp - STL Replacements for Qt Types
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include <filesystem>
#include <fstream>
#include <codecvt>
#include <locale>
#include <unordered_map>
#include <any>
#include <queue>
#include <objbase.h>

#pragma comment(lib, "ole32.lib")

namespace RawrXD {

// QString replacement - uses std::wstring with utility methods
class QString {
public:
    QString() = default;
    QString(const wchar_t* str) : m_data(str ? str : L"") {}
    QString(const String& str) : m_data(str) {}
    QString(const std::string& utf8) : m_data(StringUtils::FromUtf8(utf8)) {}
    QString(const char* utf8) : m_data(StringUtils::FromUtf8(utf8 ? utf8 : "")) {}

    static QString fromStdString(const std::string& utf8) { return QString(utf8); }
    static QString fromStdWString(const String& wide) { return QString(wide); }
    static QString fromUtf8(const char* utf8, int size = -1) {
        return QString(size < 0 ? std::string(utf8) : std::string(utf8, size));
    }

    std::string toStdString() const { return StringUtils::ToUtf8(m_data); }
    const String& toStdWString() const { return m_data; }
    const wchar_t* data() const { return m_data.c_str(); }
    const wchar_t* c_str() const { return m_data.c_str(); }

    bool isEmpty() const { return m_data.empty(); }
    int length() const { return static_cast<int>(m_data.size()); }
    int size() const { return static_cast<int>(m_data.size()); }
    void clear() { m_data.clear(); }

    QString& append(const QString& other) { m_data += other.m_data; return *this; }
    QString& prepend(const QString& other) { m_data = other.m_data + m_data; return *this; }

    QString trimmed() const { return QString(StringUtils::Trim(m_data)); }
    QString toLower() const { return QString(StringUtils::ToLower(m_data)); }
    QString toUpper() const { return QString(StringUtils::ToUpper(m_data)); }

    bool startsWith(const QString& prefix) const { return StringUtils::StartsWith(m_data, prefix.m_data); }
    bool endsWith(const QString& suffix) const { return StringUtils::EndsWith(m_data, suffix.m_data); }
    bool contains(const QString& substr) const { return m_data.find(substr.m_data) != String::npos; }

    int indexOf(const QString& substr, int from = 0) const {
        auto pos = m_data.find(substr.m_data, from);
        return pos == String::npos ? -1 : static_cast<int>(pos);
    }

    QString mid(int pos, int len = -1) const {
        if (pos < 0 || pos >= static_cast<int>(m_data.size())) return {};
        return QString(len < 0 ? m_data.substr(pos) : m_data.substr(pos, len));
    }

    QString left(int n) const { return QString(m_data.substr(0, n)); }
    QString right(int n) const { return QString(m_data.size() > static_cast<size_t>(n) ? m_data.substr(m_data.size() - n) : m_data); }

    QString replace(const QString& before, const QString& after) const {
        return QString(StringUtils::Replace(m_data, before.m_data, after.m_data));
    }

    Vector<QString> split(const QString& sep) const {
        auto parts = StringUtils::Split(m_data, sep.m_data);
        Vector<QString> result;
        for (const auto& p : parts) result.push_back(QString(p));
        return result;
    }

    QString arg(const QString& a) const {
        String result = m_data;
        for (int i = 1; i <= 9; ++i) {
            String placeholder = L"%" + std::to_wstring(i);
            auto pos = result.find(placeholder);
            if (pos != String::npos) {
                result.replace(pos, placeholder.size(), a.m_data);
                break;
            }
        }
        return QString(result);
    }

    template<typename T>
    QString arg(T val) const { return arg(QString(std::to_wstring(val))); }

    bool operator==(const QString& other) const { return m_data == other.m_data; }
    bool operator!=(const QString& other) const { return m_data != other.m_data; }
    bool operator<(const QString& other) const { return m_data < other.m_data; }
    QString operator+(const QString& other) const { return QString(m_data + other.m_data); }
    QString& operator+=(const QString& other) { m_data += other.m_data; return *this; }
    wchar_t operator[](int i) const { return m_data[i]; }

private:
    String m_data;
};

// QStringList replacement
class QStringList : public Vector<QString> {
public:
    using Vector<QString>::Vector;

    QString join(const QString& sep) const {
        if (empty()) return {};
        QString result = (*this)[0];
        for (size_t i = 1; i < size(); ++i) {
            result += sep + (*this)[i];
        }
        return result;
    }

    bool contains(const QString& str) const {
        return std::find(begin(), end(), str) != end();
    }

    QStringList filter(const QString& pattern) const {
        QStringList result;
        for (const auto& s : *this) {
            if (s.contains(pattern)) result.push_back(s);
        }
        return result;
    }

    void removeDuplicates() {
        Vector<QString> unique;
        for (const auto& s : *this) {
            if (std::find(unique.begin(), unique.end(), s) == unique.end()) {
                unique.push_back(s);
            }
        }
        *this = QStringList(unique.begin(), unique.end());
    }
};

// QVariant replacement using std::any
class QVariant {
public:
    QVariant() = default;
    QVariant(std::nullptr_t) {}
    QVariant(bool v) : m_data(v), m_type(Type::Bool) {}
    QVariant(int v) : m_data(v), m_type(Type::Int) {}
    QVariant(int64_t v) : m_data(v), m_type(Type::Int64) {}
    QVariant(double v) : m_data(v), m_type(Type::Double) {}
    QVariant(const QString& v) : m_data(v), m_type(Type::String) {}
    QVariant(const char* v) : m_data(QString(v)), m_type(Type::String) {}
    QVariant(const JsonValue& v) : m_data(v), m_type(Type::Json) {}

    enum class Type { Invalid, Bool, Int, Int64, Double, String, Json, List, Map };

    bool isValid() const { return m_data.has_value(); }
    bool isNull() const { return !m_data.has_value(); }
    Type type() const { return m_type; }

    bool toBool() const { return m_data.has_value() ? std::any_cast<bool>(m_data) : false; }
    int toInt() const { return m_data.has_value() ? std::any_cast<int>(m_data) : 0; }
    int64_t toInt64() const { return m_data.has_value() ? std::any_cast<int64_t>(m_data) : 0; }
    double toDouble() const { return m_data.has_value() ? std::any_cast<double>(m_data) : 0.0; }
    QString toString() const { return m_data.has_value() ? std::any_cast<QString>(m_data) : QString(); }

    template<typename T>
    T value() const { return std::any_cast<T>(m_data); }

    template<typename T>
    static QVariant fromValue(const T& v) { QVariant var; var.m_data = v; return var; }

private:
    std::any m_data;
    Type m_type = Type::Invalid;
};

// QMap replacement
template<typename K, typename V>
class QMap : public std::map<K, V> {
public:
    using std::map<K, V>::map;

    bool contains(const K& key) const { return this->find(key) != this->end(); }

    V value(const K& key, const V& defaultValue = V()) const {
        auto it = this->find(key);
        return it != this->end() ? it->second : defaultValue;
    }

    void insert(const K& key, const V& value) { (*this)[key] = value; }
    void remove(const K& key) { this->erase(key); }

    Vector<K> keys() const {
        Vector<K> result;
        for (const auto& [k, v] : *this) result.push_back(k);
        return result;
    }

    Vector<V> values() const {
        Vector<V> result;
        for (const auto& [k, v] : *this) result.push_back(v);
        return result;
    }
};

// QHash replacement
template<typename K, typename V>
class QHash : public std::unordered_map<K, V> {
public:
    using std::unordered_map<K, V>::unordered_map;

    bool contains(const K& key) const { return this->find(key) != this->end(); }

    V value(const K& key, const V& defaultValue = V()) const {
        auto it = this->find(key);
        return it != this->end() ? it->second : defaultValue;
    }

    void insert(const K& key, const V& value) { (*this)[key] = value; }
    void remove(const K& key) { this->erase(key); }
};

// QJsonDocument replacement
class QJsonDocument {
public:
    QJsonDocument() = default;
    explicit QJsonDocument(const JsonValue& val) : m_root(val) {}
    explicit QJsonDocument(const JsonObject& obj) : m_root(obj) {}
    explicit QJsonDocument(const JsonArray& arr) : m_root(arr) {}

    static QJsonDocument fromJson(const std::string& json) {
        auto val = JsonParser::Parse(json);
        return val ? QJsonDocument(*val) : QJsonDocument();
    }

    std::string toJson(bool compact = false) const {
        return JsonParser::Serialize(m_root, compact ? 0 : 2);
    }

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_root); }
    bool isObject() const { return std::holds_alternative<JsonObject>(m_root); }
    bool isArray() const { return std::holds_alternative<JsonArray>(m_root); }

    JsonObject object() const { return std::holds_alternative<JsonObject>(m_root) ? std::get<JsonObject>(m_root) : JsonObject(); }
    JsonArray array() const { return std::holds_alternative<JsonArray>(m_root) ? std::get<JsonArray>(m_root) : JsonArray(); }

    const JsonValue& root() const { return m_root; }

private:
    JsonValue m_root;
};

// QFile replacement
class QFile {
public:
    QFile() = default;
    explicit QFile(const QString& path) : m_path(path.toStdWString()) {}

    void setFileName(const QString& path) { m_path = path.toStdWString(); close(); }
    QString fileName() const { return QString(m_path); }

    bool exists() const { return std::filesystem::exists(m_path); }
    static bool exists(const QString& path) { return std::filesystem::exists(path.toStdWString()); }

    bool open(std::ios::openmode mode) {
        m_stream.open(m_path, mode);
        return m_stream.is_open();
    }

    void close() { if (m_stream.is_open()) m_stream.close(); }
    bool isOpen() const { return m_stream.is_open(); }

    std::string readAll() {
        std::stringstream ss;
        ss << m_stream.rdbuf();
        return ss.str();
    }

    bool write(const std::string& data) {
        m_stream << data;
        return m_stream.good();
    }

    int64_t size() const {
        return static_cast<int64_t>(std::filesystem::file_size(m_path));
    }

    bool remove() { return std::filesystem::remove(m_path); }
    static bool remove(const QString& path) { return std::filesystem::remove(path.toStdWString()); }

private:
    std::filesystem::path m_path;
    std::fstream m_stream;
};

// QDir replacement
class QDir {
public:
    QDir() : m_path(std::filesystem::current_path()) {}
    explicit QDir(const QString& path) : m_path(path.toStdWString()) {}

    QString path() const { return QString(m_path.wstring()); }
    QString absolutePath() const { return QString(std::filesystem::absolute(m_path).wstring()); }

    bool exists() const { return std::filesystem::exists(m_path) && std::filesystem::is_directory(m_path); }
    static bool exists(const QString& path) { return std::filesystem::exists(path.toStdWString()); }

    bool mkdir(const QString& name) const { return std::filesystem::create_directory(m_path / name.toStdWString()); }
    bool mkpath(const QString& path) const { return std::filesystem::create_directories(m_path / path.toStdWString()); }
    static bool mkpath(const QString& path, bool) { return std::filesystem::create_directories(path.toStdWString()); }

    QStringList entryList(const QStringList& filters = {}) const {
        QStringList result;
        for (const auto& entry : std::filesystem::directory_iterator(m_path)) {
            QString name(entry.path().filename().wstring());
            if (filters.empty() || MatchesFilters(name, filters)) {
                result.push_back(name);
            }
        }
        return result;
    }

    QString filePath(const QString& name) const {
        return QString((m_path / name.toStdWString()).wstring());
    }

    static QString currentPath() { return QString(std::filesystem::current_path().wstring()); }
    static bool setCurrent(const QString& path) { std::filesystem::current_path(path.toStdWString()); return true; }

    static QString separator() { return QString(L"\\"); }

private:
    static bool MatchesFilters(const QString& name, const QStringList& filters) {
        for (const auto& filter : filters) {
            if (name.endsWith(filter.mid(1))) return true; // simple *.ext matching
        }
        return false;
    }

    std::filesystem::path m_path;
};

// QFileInfo replacement
class QFileInfo {
public:
    QFileInfo() = default;
    explicit QFileInfo(const QString& path) : m_path(path.toStdWString()) {}

    bool exists() const { return std::filesystem::exists(m_path); }
    bool isFile() const { return std::filesystem::is_regular_file(m_path); }
    bool isDir() const { return std::filesystem::is_directory(m_path); }
    bool isReadable() const { return exists(); }
    bool isWritable() const { return exists(); }

    QString fileName() const { return QString(m_path.filename().wstring()); }
    QString baseName() const { return QString(m_path.stem().wstring()); }
    QString suffix() const { return QString(m_path.extension().wstring()); }
    QString absolutePath() const { return QString(m_path.parent_path().wstring()); }
    QString absoluteFilePath() const { return QString(std::filesystem::absolute(m_path).wstring()); }
    QString canonicalFilePath() const { return QString(std::filesystem::canonical(m_path).wstring()); }

    int64_t size() const { return static_cast<int64_t>(std::filesystem::file_size(m_path)); }

private:
    std::filesystem::path m_path;
};

// QTimer replacement using Win32 timers
class QTimer {
public:
    QTimer() = default;
    ~QTimer() { stop(); }

    void setInterval(int msec) { m_interval = msec; }
    int interval() const { return m_interval; }

    void setSingleShot(bool single) { m_singleShot = single; }
    bool isSingleShot() const { return m_singleShot; }

    bool isActive() const { return m_running; }

    void setCallback(std::function<void()> callback) { m_callback = std::move(callback); }

    void start() {
        if (m_running) return;
        m_running = true;
        m_thread = std::thread([this]() {
            while (m_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(m_interval));
                if (m_running && m_callback) {
                    m_callback();
                    if (m_singleShot) {
                        m_running = false;
                        break;
                    }
                }
            }
        });
    }

    void stop() {
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
    }

    static void singleShot(int msec, std::function<void()> callback) {
        std::thread([msec, callback = std::move(callback)]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(msec));
            callback();
        }).detach();
    }

private:
    int m_interval = 0;
    bool m_singleShot = false;
    std::atomic<bool> m_running{false};
    std::function<void()> m_callback;
    std::thread m_thread;
};

// QDateTime replacement
class QDateTime {
public:
    QDateTime() = default;

    static QDateTime currentDateTime() {
        QDateTime dt;
        dt.m_time = std::chrono::system_clock::now();
        return dt;
    }

    int64_t toMSecsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(m_time.time_since_epoch()).count();
    }

    static QDateTime fromMSecsSinceEpoch(int64_t msecs) {
        QDateTime dt;
        dt.m_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(msecs));
        return dt;
    }

    QString toString(const QString& format = QString("yyyy-MM-dd hh:mm:ss")) const {
        auto time = std::chrono::system_clock::to_time_t(m_time);
        struct tm tm_info;
        localtime_s(&tm_info, &time);
        wchar_t buffer[128];
        wcsftime(buffer, 128, L"%Y-%m-%d %H:%M:%S", &tm_info);
        return QString(buffer);
    }

    int64_t secsTo(const QDateTime& other) const {
        return std::chrono::duration_cast<std::chrono::seconds>(other.m_time - m_time).count();
    }

    int64_t msecsTo(const QDateTime& other) const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(other.m_time - m_time).count();
    }

private:
    std::chrono::system_clock::time_point m_time;
};

// QUuid replacement
class QUuid {
public:
    QUuid() = default;

    static QUuid createUuid() {
        QUuid uuid;
        GUID guid;
        CoCreateGuid(&guid);
        uuid.m_guid = guid;
        return uuid;
    }

    QString toString() const {
        wchar_t buffer[64];
        swprintf_s(buffer, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            m_guid.Data1, m_guid.Data2, m_guid.Data3,
            m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
            m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7]);
        return QString(buffer);
    }

    bool isNull() const { return m_guid.Data1 == 0 && m_guid.Data2 == 0 && m_guid.Data3 == 0; }

private:
    GUID m_guid = {};
};

// QMutex replacement
class QMutex {
public:
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool tryLock() { return m_mutex.try_lock(); }

private:
    std::mutex m_mutex;
};

class QMutexLocker {
public:
    explicit QMutexLocker(QMutex* mutex) : m_mutex(mutex) { if (m_mutex) m_mutex->lock(); }
    ~QMutexLocker() { if (m_mutex) m_mutex->unlock(); }

private:
    QMutex* m_mutex;
};

// QThread replacement
class QThread {
public:
    QThread() = default;
    virtual ~QThread() { wait(); }

    virtual void run() {}

    void start() {
        m_running = true;
        m_thread = std::thread([this]() { run(); m_running = false; });
    }

    void wait() {
        if (m_thread.joinable()) m_thread.join();
    }

    bool isRunning() const { return m_running; }

    static void sleep(unsigned long secs) { std::this_thread::sleep_for(std::chrono::seconds(secs)); }
    static void msleep(unsigned long msecs) { std::this_thread::sleep_for(std::chrono::milliseconds(msecs)); }
    static void usleep(unsigned long usecs) { std::this_thread::sleep_for(std::chrono::microseconds(usecs)); }

protected:
    std::atomic<bool> m_running{false};

private:
    std::thread m_thread;
};

// QRegularExpression replacement
class QRegularExpression {
public:
    QRegularExpression() = default;
    explicit QRegularExpression(const QString& pattern) : m_pattern(pattern.toStdString()) {}

    bool isValid() const { return true; } // std::regex doesn't throw on construction

    struct Match {
        bool hasMatch = false;
        Vector<QString> captured;

        QString captured(int index) const {
            return index < static_cast<int>(captured.size()) ? captured[index] : QString();
        }
    };

    Match match(const QString& subject) const {
        Match result;
        std::smatch matches;
        std::string subj = subject.toStdString();
        if (std::regex_search(subj, matches, m_pattern)) {
            result.hasMatch = true;
            for (const auto& m : matches) {
                result.captured.push_back(QString(m.str()));
            }
        }
        return result;
    }

    Vector<Match> globalMatch(const QString& subject) const {
        Vector<Match> results;
        std::string subj = subject.toStdString();
        auto begin = std::sregex_iterator(subj.begin(), subj.end(), m_pattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            Match m;
            m.hasMatch = true;
            for (const auto& sub : *it) {
                m.captured.push_back(QString(sub.str()));
            }
            results.push_back(m);
        }
        return results;
    }

private:
    std::regex m_pattern;
};

} // namespace RawrXD
