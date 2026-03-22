// StdReplacements.hpp — Pure C++20 / Win32. Native types only.
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

// WideString — std::wstring with utility helpers for Win32 text
class WideString {
public:
    WideString() = default;
    WideString(const wchar_t* str) : m_data(str ? str : L"") {}
    WideString(const String& str) : m_data(str) {}
    WideString(const std::string& utf8) : m_data(StringUtils::FromUtf8(utf8)) {}
    WideString(const char* utf8) : m_data(StringUtils::FromUtf8(utf8 ? utf8 : "")) {}

    static WideString fromStdString(const std::string& utf8) { return WideString(utf8); }
    static WideString fromStdWString(const String& wide) { return WideString(wide); }
    static WideString fromUtf8(const char* utf8, int size = -1) {
        return WideString(size < 0 ? std::string(utf8) : std::string(utf8, size));
    }

    std::string toStdString() const { return StringUtils::ToUtf8(m_data); }
    const String& toStdWString() const { return m_data; }
    const wchar_t* data() const { return m_data.c_str(); }
    const wchar_t* c_str() const { return m_data.c_str(); }

    bool isEmpty() const { return m_data.empty(); }
    int length() const { return static_cast<int>(m_data.size()); }
    int size() const { return static_cast<int>(m_data.size()); }
    void clear() { m_data.clear(); }

    WideString& append(const WideString& other) { m_data += other.m_data; return *this; }
    WideString& prepend(const WideString& other) { m_data = other.m_data + m_data; return *this; }

    WideString trimmed() const { return WideString(StringUtils::Trim(m_data)); }
    WideString toLower() const { return WideString(StringUtils::ToLower(m_data)); }
    WideString toUpper() const { return WideString(StringUtils::ToUpper(m_data)); }

    bool startsWith(const WideString& prefix) const { return StringUtils::StartsWith(m_data, prefix.m_data); }
    bool endsWith(const WideString& suffix) const { return StringUtils::EndsWith(m_data, suffix.m_data); }
    bool contains(const WideString& substr) const { return m_data.find(substr.m_data) != String::npos; }

    int indexOf(const WideString& substr, int from = 0) const {
        auto pos = m_data.find(substr.m_data, from);
        return pos == String::npos ? -1 : static_cast<int>(pos);
    }

    int lastIndexOf(const WideString& substr) const {
        auto pos = m_data.rfind(substr.m_data);
        return pos == String::npos ? -1 : static_cast<int>(pos);
    }

    int lastIndexOf(wchar_t ch) const {
        auto pos = m_data.rfind(ch);
        return pos == String::npos ? -1 : static_cast<int>(pos);
    }

    WideString mid(int pos, int len = -1) const {
        if (pos < 0 || pos >= static_cast<int>(m_data.size())) return {};
        return WideString(len < 0 ? m_data.substr(pos) : m_data.substr(pos, len));
    }

    WideString left(int n) const { return WideString(m_data.substr(0, n)); }
    WideString right(int n) const { return WideString(m_data.size() > static_cast<size_t>(n) ? m_data.substr(m_data.size() - n) : m_data); }

    WideString replace(const WideString& before, const WideString& after) const {
        return WideString(StringUtils::Replace(m_data, before.m_data, after.m_data));
    }

    Vector<WideString> split(const WideString& sep) const {
        auto parts = StringUtils::Split(m_data, sep.m_data);
        Vector<WideString> result;
        for (const auto& p : parts) result.push_back(WideString(p));
        return result;
    }

    WideString arg(const WideString& a) const {
        String result = m_data;
        for (int i = 1; i <= 9; ++i) {
            String placeholder = L"%" + std::to_wstring(i);
            auto pos = result.find(placeholder);
            if (pos != String::npos) {
                result.replace(pos, placeholder.size(), a.m_data);
                break;
            }
        }
        return WideString(result);
    }

    template<typename T>
    WideString arg(T val) const { return arg(WideString(std::to_wstring(val))); }

    bool operator==(const WideString& other) const { return m_data == other.m_data; }
    bool operator!=(const WideString& other) const { return m_data != other.m_data; }
    bool operator<(const WideString& other) const { return m_data < other.m_data; }
    WideString operator+(const WideString& other) const { return WideString(m_data + other.m_data); }
    WideString& operator+=(const WideString& other) { m_data += other.m_data; return *this; }
    wchar_t operator[](int i) const { return m_data[i]; }

private:
    String m_data;
};

// WideStringList — vector of wide strings
class WideStringList : public Vector<WideString> {
public:
    using Vector<WideString>::Vector;

    WideString join(const WideString& sep) const {
        if (empty()) return {};
        WideString result = (*this)[0];
        for (size_t i = 1; i < size(); ++i) {
            result += sep + (*this)[i];
        }
        return result;
    }

    bool contains(const WideString& str) const {
        return std::find(begin(), end(), str) != end();
    }

    WideStringList filter(const WideString& pattern) const {
        WideStringList result;
        for (const auto& s : *this) {
            if (s.contains(pattern)) result.push_back(s);
        }
        return result;
    }

    void removeDuplicates() {
        Vector<WideString> unique;
        for (const auto& s : *this) {
            if (std::find(unique.begin(), unique.end(), s) == unique.end()) {
                unique.push_back(s);
            }
        }
        *this = WideStringList(unique.begin(), unique.end());
    }
};

// Variant — std::any-based tagged holder
class Variant {
public:
    Variant() = default;
    Variant(std::nullptr_t) {}
    Variant(bool v) : m_data(v), m_type(Type::Bool) {}
    Variant(int v) : m_data(v), m_type(Type::Int) {}
    Variant(int64_t v) : m_data(v), m_type(Type::Int64) {}
    Variant(double v) : m_data(v), m_type(Type::Double) {}
    Variant(const WideString& v) : m_data(v), m_type(Type::String) {}
    Variant(const char* v) : m_data(WideString(v)), m_type(Type::String) {}
    Variant(const JsonValue& v) : m_data(v), m_type(Type::Json) {}

    enum class Type { Invalid, Bool, Int, Int64, Double, String, Json, List, Map };

    bool isValid() const { return m_data.has_value(); }
    bool isNull() const { return !m_data.has_value(); }
    Type type() const { return m_type; }

    bool toBool() const { return m_data.has_value() ? std::any_cast<bool>(m_data) : false; }
    int toInt() const { return m_data.has_value() ? std::any_cast<int>(m_data) : 0; }
    int64_t toInt64() const { return m_data.has_value() ? std::any_cast<int64_t>(m_data) : 0; }
    double toDouble() const { return m_data.has_value() ? std::any_cast<double>(m_data) : 0.0; }
    WideString toString() const { return m_data.has_value() ? std::any_cast<WideString>(m_data) : WideString(); }

    template<typename T>
    T value() const { return std::any_cast<T>(m_data); }

    template<typename T>
    static Variant fromValue(const T& v) { Variant var; var.m_data = v; return var; }

private:
    std::any m_data;
    Type m_type = Type::Invalid;
};

// StdMap — ordered map with convenience helpers
template<typename K, typename V>
class StdMap : public std::map<K, V> {
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

// StdUnorderedMap — hash map with convenience helpers
template<typename K, typename V>
class StdUnorderedMap : public std::unordered_map<K, V> {
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

// JsonDoc — JSON document wrapper
class JsonDoc {
public:
    JsonDoc() = default;
    explicit JsonDoc(const JsonValue& val) : m_root(val) {}
    explicit JsonDoc(const JsonObject& obj) : m_root(obj) {}
    explicit JsonDoc(const JsonArray& arr) : m_root(arr) {}

    static JsonDoc fromJson(const std::string& json) {
        auto val = JsonParser::Parse(json);
        return val ? JsonDoc(*val) : JsonDoc();
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

// StdFile — narrow stream file I/O
class StdFile {
public:
    StdFile() = default;
    explicit StdFile(const WideString& name) : m_name(name.toStdWString()) {}

    bool open(const wchar_t* mode) {
        (void)mode;
        m_stream.open(std::filesystem::path(m_name), std::ios::in | std::ios::binary);
        return m_stream.is_open();
    }

    WideString readAll() {
        if (!m_stream.is_open()) return {};
        std::string content((std::istreambuf_iterator<char>(m_stream)), 
                            std::istreambuf_iterator<char>());
        return WideString(content);
    }

    void close() {
        if (m_stream.is_open()) m_stream.close();
    }

    static bool exists(const WideString& fileName) {
        return std::filesystem::exists(fileName.toStdWString());
    }

private:
    std::wstring m_name;
    std::ifstream m_stream;
};

// StdDir — directory helpers
class StdDir {
public:
    StdDir() : m_path(std::filesystem::current_path()) {}
    explicit StdDir(const WideString& path) : m_path(path.toStdWString()) {}

    WideString path() const { return WideString(m_path.wstring()); }
    WideString absolutePath() const { return WideString(std::filesystem::absolute(m_path).wstring()); }

    bool exists() const { return std::filesystem::exists(m_path) && std::filesystem::is_directory(m_path); }
    static bool exists(const WideString& path) { return std::filesystem::exists(path.toStdWString()); }

    bool mkdir(const WideString& name) const { return std::filesystem::create_directory(m_path / name.toStdWString()); }
    bool mkpath(const WideString& path) const { return std::filesystem::create_directories(m_path / path.toStdWString()); }
    static bool mkpath(const WideString& path, bool) { return std::filesystem::create_directories(path.toStdWString()); }

    WideStringList entryList(const WideStringList& filters = {}) const {
        WideStringList result;
        for (const auto& entry : std::filesystem::directory_iterator(m_path)) {
            WideString name(entry.path().filename().wstring());
            if (filters.empty() || MatchesFilters(name, filters)) {
                result.push_back(name);
            }
        }
        return result;
    }

    WideString filePath(const WideString& name) const {
        return WideString((m_path / name.toStdWString()).wstring());
    }

    static WideString currentPath() { return WideString(std::filesystem::current_path().wstring()); }
    static bool setCurrent(const WideString& path) { std::filesystem::current_path(path.toStdWString()); return true; }

    static WideString separator() { return WideString(L"\\"); }

private:
    static bool MatchesFilters(const WideString& name, const WideStringList& filters) {
        for (const auto& filter : filters) {
            if (name.endsWith(filter.mid(1))) return true; // simple *.ext matching
        }
        return false;
    }

    std::filesystem::path m_path;
};

// FileInfo — path metadata
class FileInfo {
public:
    FileInfo() = default;
    explicit FileInfo(const WideString& path) : m_path(path.toStdWString()) {}

    bool exists() const { return std::filesystem::exists(m_path); }
    bool isFile() const { return std::filesystem::is_regular_file(m_path); }
    bool isDir() const { return std::filesystem::is_directory(m_path); }
    bool isReadable() const { return exists(); }
    bool isWritable() const { return exists(); }

    WideString fileName() const { return WideString(m_path.filename().wstring()); }
    WideString baseName() const { return WideString(m_path.stem().wstring()); }
    WideString suffix() const { return WideString(m_path.extension().wstring()); }
    WideString absolutePath() const { return WideString(m_path.parent_path().wstring()); }
    WideString absoluteFilePath() const { return WideString(std::filesystem::absolute(m_path).wstring()); }
    WideString canonicalFilePath() const { return WideString(std::filesystem::canonical(m_path).wstring()); }

    int64_t size() const { return static_cast<int64_t>(std::filesystem::file_size(m_path)); }

private:
    std::filesystem::path m_path;
};

// Timer — periodic callback
class Timer {
public:
    Timer() = default;
    ~Timer() { stop(); }

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

// DateTime — system time
class DateTime {
public:
    DateTime() = default;

    static DateTime currentDateTime() {
        DateTime dt;
        dt.m_time = std::chrono::system_clock::now();
        return dt;
    }

    int64_t toMSecsSinceEpoch() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(m_time.time_since_epoch()).count();
    }

    static DateTime fromMSecsSinceEpoch(int64_t msecs) {
        DateTime dt;
        dt.m_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(msecs));
        return dt;
    }

    WideString toString(const WideString& format = WideString(L"yyyy-MM-dd hh:mm:ss")) const {
        (void)format;
        auto time = std::chrono::system_clock::to_time_t(m_time);
        struct tm tm_info;
        localtime_s(&tm_info, &time);
        wchar_t buffer[128];
        wcsftime(buffer, 128, L"%Y-%m-%d %H:%M:%S", &tm_info);
        return WideString(buffer);
    }

    int64_t secsTo(const DateTime& other) const {
        return std::chrono::duration_cast<std::chrono::seconds>(other.m_time - m_time).count();
    }

    int64_t msecsTo(const DateTime& other) const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(other.m_time - m_time).count();
    }

private:
    std::chrono::system_clock::time_point m_time;
};

// Uuid — GUID
class Uuid {
public:
    Uuid() = default;

    static Uuid createUuid() {
        Uuid uuid;
        GUID guid;
        CoCreateGuid(&guid);
        uuid.m_guid = guid;
        return uuid;
    }

    WideString toString() const {
        wchar_t buffer[64];
        swprintf_s(buffer, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            m_guid.Data1, m_guid.Data2, m_guid.Data3,
            m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
            m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7]);
        return WideString(buffer);
    }

    bool isNull() const { return m_guid.Data1 == 0 && m_guid.Data2 == 0 && m_guid.Data3 == 0; }

private:
    GUID m_guid = {};
};

// Mutex — std::mutex wrapper
class Mutex {
public:
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool tryLock() { return m_mutex.try_lock(); }

private:
    std::mutex m_mutex;
};

class MutexLocker {
public:
    explicit MutexLocker(Mutex* mutex) : m_mutex(mutex) { if (m_mutex) m_mutex->lock(); }
    ~MutexLocker() { if (m_mutex) m_mutex->unlock(); }

private:
    Mutex* m_mutex;
};

// StdThread — background work unit
class StdThread {
public:
    StdThread() = default;
    virtual ~StdThread() { wait(); }

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

// StdRegex — regex helper
class StdRegex {
public:
    StdRegex() = default;
    explicit StdRegex(const WideString& pattern) : m_pattern(pattern.toStdString()) {}

    bool isValid() const { return true; }

    struct Match {
        bool hasMatch = false;
        Vector<WideString> capturedGroups;

        WideString group(int index) const {
            return index < static_cast<int>(capturedGroups.size()) ? capturedGroups[index] : WideString();
        }
    };

    Match match(const WideString& subject) const {
        Match result;
        std::smatch matches;
        std::string subj = subject.toStdString();
        if (std::regex_search(subj, matches, m_pattern)) {
            result.hasMatch = true;
            for (const auto& m : matches) {
                result.capturedGroups.push_back(WideString(m.str()));
            }
        }
        return result;
    }

    Vector<Match> globalMatch(const WideString& subject) const {
        Vector<Match> results;
        std::string subj = subject.toStdString();
        auto begin = std::sregex_iterator(subj.begin(), subj.end(), m_pattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            Match m;
            m.hasMatch = true;
            for (const auto& sub : *it) {
                m.capturedGroups.push_back(WideString(sub.str()));
            }
            results.push_back(m);
        }
        return results;
    }

private:
    std::regex m_pattern;
};

} // namespace RawrXD
