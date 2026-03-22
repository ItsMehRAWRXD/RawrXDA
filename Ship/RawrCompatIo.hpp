#pragma once
/* RawrCompatIo.hpp — Win32 + STL helpers for binary I/O and UTF-16 strings.
 * Single header; no third-party UI toolkit. Prefer Ship/StdReplacements.hpp for IDE-wide types.
 */
#ifndef RAWR_COMPAT_IO_HPP
#define RAWR_COMPAT_IO_HPP

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
#include <stack>
#include <shlobj.h>

namespace rawrxd {
namespace compat_io {


// UTF-16 string helper
// ============================================================================
// Utf16String around std::wstring
// ============================================================================

class Utf16String {
public:
    Utf16String() = default;
    Utf16String(const wchar_t* s) : m_data(s ? s : L"") {}
    Utf16String(const std::wstring& s) : m_data(s) {}
    Utf16String(const std::string& s) {
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
    explicit Utf16String(const char* s) : Utf16String(s ? std::string(s) : std::string("")) {}
    explicit Utf16String(int n) : m_data(std::to_wstring(n)) {}
    explicit Utf16String(double d) {
        wchar_t buf[64];
        swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.6g", d);
        m_data = buf;
    }
    
    static Utf16String fromUtf8(const char* s) { return s ? Utf16String(std::string(s)) : Utf16String(); }
    static Utf16String fromStdWString(const std::wstring& s) { return Utf16String(s); }
    static Utf16String fromStdString(const std::string& s) { return Utf16String(s); }
    static Utf16String number(int n) { return Utf16String(n); }
    static Utf16String number(double n, char format = 'g', int precision = 6) {
        wchar_t buf[64];
        if (format == 'f') {
            swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.*f", precision, n);
        } else {
            swprintf_s(buf, sizeof(buf)/sizeof(wchar_t), L"%.*g", precision, n);
        }
        return Utf16String(buf);
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
    
    Utf16String& append(const Utf16String& s) { m_data += s.m_data; return *this; }
    Utf16String& append(const std::string& s) { return append(Utf16String(s)); }
    Utf16String& append(wchar_t c) { m_data += c; return *this; }
    
    bool operator==(const Utf16String& other) const { return m_data == other.m_data; }
    bool operator!=(const Utf16String& other) const { return m_data != other.m_data; }
    bool operator<(const Utf16String& other) const { return m_data < other.m_data; }
    bool operator<=(const Utf16String& other) const { return m_data <= other.m_data; }
    bool operator>(const Utf16String& other) const { return m_data > other.m_data; }
    bool operator>=(const Utf16String& other) const { return m_data >= other.m_data; }
    
    Utf16String arg(const Utf16String& a) const {
        std::wstring res = m_data;
        size_t pos = res.find(L"%1");
        if (pos != std::wstring::npos) res.replace(pos, 2, a.m_data);
        return Utf16String(res);
    }
    
    Utf16String arg(int a) const { return arg(Utf16String::number(a)); }
    Utf16String arg(double a) const { return arg(Utf16String::number(a)); }
    Utf16String arg(const std::string& a) const { return arg(Utf16String(a)); }
    
    Utf16String mid(int pos, int n = -1) const {
        if (pos < 0 || pos >= (int)m_data.length()) return Utf16String();
        if (n < 0) return Utf16String(m_data.substr(pos));
        return Utf16String(m_data.substr(pos, std::min((size_t)n, m_data.length() - pos)));
    }
    
    int indexOf(const Utf16String& s, int from = 0) const {
        if (from < 0 || from >= (int)m_data.length()) return -1;
        auto p = m_data.find(s.m_data, from);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }
    
    int indexOf(wchar_t c, int from = 0) const {
        if (from < 0 || from >= (int)m_data.length()) return -1;
        auto p = m_data.find(c, from);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }

    int lastIndexOf(const Utf16String& s) const {
        auto p = m_data.rfind(s.m_data);
        return (p == std::wstring::npos) ? -1 : (int)p;
    }

    bool startsWith(const Utf16String& s) const { return m_data.find(s.m_data) == 0; }
    bool endsWith(const Utf16String& s) const {
        if (s.m_data.length() > m_data.length()) return false;
        return m_data.compare(m_data.length() - s.m_data.length(), s.m_data.length(), s.m_data) == 0;
    }

    Utf16String trimmed() const {
        const wchar_t* ws = L" \t\n\r\f\v";
        size_t start = m_data.find_first_not_of(ws);
        if (start == std::wstring::npos) return Utf16String();
        size_t end = m_data.find_last_not_of(ws);
        return Utf16String(m_data.substr(start, end - start + 1));
    }

    std::vector<Utf16String> split(const Utf16String& sep, bool skipEmpty = false) const {
        std::vector<Utf16String> res;
        if (sep.m_data.empty()) { if (!skipEmpty) res.push_back(*this); return res; }
        size_t start = 0, pos;
        while ((pos = m_data.find(sep.m_data, start)) != std::wstring::npos) {
            std::wstring part = m_data.substr(start, pos - start);
            if (!skipEmpty || !part.empty()) res.push_back(Utf16String(part));
            start = pos + sep.m_data.length();
        }
        std::wstring part = m_data.substr(start);
        if (!skipEmpty || !part.empty()) res.push_back(Utf16String(part));
        return res;
    }
    
    Utf16String replace(const Utf16String& before, const Utf16String& after) const {
        std::wstring res = m_data;
        size_t pos = 0;
        while ((pos = res.find(before.m_data, pos)) != std::wstring::npos) {
            res.replace(pos, before.m_data.length(), after.m_data);
            pos += after.m_data.length();
        }
        return Utf16String(res);
    }
    
    bool contains(const Utf16String& s) const { return m_data.find(s.m_data) != std::wstring::npos; }
    
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

namespace UninitializedFill {
    enum class Tag { Uninitialized };
}

class ByteBuffer {
public:
    ByteBuffer() = default;
    ByteBuffer(const char* data, int len) {
        if (data && len > 0) {
            bytes_.assign(data, data + len);
        }
    }
    explicit ByteBuffer(const std::string& s)
        : bytes_(s.begin(), s.end()) {}
    explicit ByteBuffer(std::size_t size, char fill = '\0')
        : bytes_(size, fill) {}
    ByteBuffer(std::size_t size, UninitializedFill::Tag)
        : bytes_(size) {}
    ByteBuffer(int size, UninitializedFill::Tag)
        : bytes_(size > 0 ? static_cast<std::size_t>(size) : 0U) {}

    bool isEmpty() const { return bytes_.empty(); }
    bool empty() const { return bytes_.empty(); }
    int size() const { return static_cast<int>(bytes_.size()); }
    std::size_t length() const { return bytes_.size(); }

    const char* constData() const { return bytes_.empty() ? nullptr : bytes_.data(); }
    char* data() { return bytes_.empty() ? nullptr : bytes_.data(); }

    void resize(std::size_t size) { bytes_.resize(size); }
    void reserve(std::size_t size) { bytes_.reserve(size); }
    void clear() { bytes_.clear(); }

    ByteBuffer& append(const ByteBuffer& other) {
        bytes_.insert(bytes_.end(), other.bytes_.begin(), other.bytes_.end());
        return *this;
    }
    ByteBuffer& append(const char* data, int len) {
        if (data && len > 0) {
            bytes_.insert(bytes_.end(), data, data + len);
        }
        return *this;
    }
    ByteBuffer& append(char c) {
        bytes_.push_back(c);
        return *this;
    }

    std::string toStdString() const {
        return std::string(bytes_.begin(), bytes_.end());
    }

    static ByteBuffer number(std::size_t value) {
        const std::string s = std::to_string(value);
        return ByteBuffer(s);
    }
    static ByteBuffer fromStdString(const std::string& s) {
        return ByteBuffer(s);
    }

private:
    std::vector<char> bytes_;
};

using Utf16StringList = std::vector<Utf16String>;
using ByteBufferList = std::vector<ByteBuffer>;

// ============================================================================
// Integer aliases + bounds helpers (portable naming)
// ============================================================================
using UInt8  = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;
using Int8   = std::int8_t;
using Int16  = std::int16_t;
using Int32  = std::int32_t;
using Int64  = std::int64_t;
using SizeType = std::size_t;
template<typename T> inline T rawrMax(T a, T b) { return (std::max)(a, b); }
template<typename T> inline T rawrMin(T a, T b) { return (std::min)(a, b); }
template<typename T> inline T rawrBound(T lo, T v, T hi) { return (std::clamp)(v, lo, hi); }

// ============================================================================
// Sequential byte device — abstract read interface for binary parsers
// ============================================================================
class SequentialByteDevice {
public:
    virtual ~SequentialByteDevice() = default;
    virtual int read(char* buf, int maxSize) = 0;
    virtual bool seek(size_t pos) = 0;
    virtual size_t size() const = 0;
    virtual bool isOpen() const = 0;
};

// ============================================================================
// Little-endian binary stream reader
// ============================================================================
class LittleEndianBinaryReader {
public:
    enum Status { Ok = 0, ReadPastEnd = 1, ReadCorruptData = 2 };
    enum ByteOrder { BigEndian, LittleEndian };
    LittleEndianBinaryReader() : m_dev(nullptr), m_order(LittleEndian), m_status(Ok) {}
    explicit LittleEndianBinaryReader(SequentialByteDevice* dev) : m_dev(dev), m_order(LittleEndian), m_status(Ok) {}
    void setDevice(SequentialByteDevice* dev) { m_dev = dev; m_status = Ok; }
    void setByteOrder(ByteOrder order) { m_order = order; }
    Status status() const { return m_status; }
    void setStatus(Status s) { m_status = s; }
private:
    SequentialByteDevice* m_dev;
    ByteOrder m_order;
    Status m_status;
    bool readBytes(char* buf, int len) {
        if (!m_dev || len <= 0) { if (m_dev) m_status = ReadCorruptData; return false; }
        int n = m_dev->read(buf, len);
        if (n != len) { m_status = ReadPastEnd; return false; }
        return true;
    }
public:
    inline LittleEndianBinaryReader& operator>>(uint8_t& v) { return readBytes(reinterpret_cast<char*>(&v), 1) ? *this : *this; }
    inline LittleEndianBinaryReader& operator>>(int8_t& v) { return readBytes(reinterpret_cast<char*>(&v), 1) ? *this : *this; }
    inline LittleEndianBinaryReader& operator>>(uint16_t& v) { if (!readBytes(reinterpret_cast<char*>(&v), 2)) return *this; if (m_order == BigEndian) v = (v>>8)|(v<<8); return *this; }
    inline LittleEndianBinaryReader& operator>>(int16_t& v) { if (!readBytes(reinterpret_cast<char*>(&v), 2)) return *this; if (m_order == BigEndian) v = static_cast<int16_t>((v>>8)|(v<<8)); return *this; }
    inline LittleEndianBinaryReader& operator>>(uint32_t& v) { if (!readBytes(reinterpret_cast<char*>(&v), 4)) return *this; if (m_order == BigEndian) v = ((v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|(v<<24)); return *this; }
    inline LittleEndianBinaryReader& operator>>(int32_t& v) { uint32_t u; if (!readBytes(reinterpret_cast<char*>(&u), 4)) return *this; if (m_order == BigEndian) u = ((u>>24)|((u>>8)&0xFF00)|((u<<8)&0xFF0000)|(u<<24)); v = static_cast<int32_t>(u); return *this; }
    inline LittleEndianBinaryReader& operator>>(uint64_t& v) { if (!readBytes(reinterpret_cast<char*>(&v), 8)) return *this; if (m_order == BigEndian) { uint64_t a = (v&0xFF)<<56, b = ((v>>8)&0xFF)<<48, c = ((v>>16)&0xFF)<<40, d = ((v>>24)&0xFF)<<32, e = ((v>>32)&0xFF)<<24, f = ((v>>40)&0xFF)<<16, g = ((v>>48)&0xFF)<<8, h = (v>>56)&0xFF; v = a|b|c|d|e|f|g|h; } return *this; }
    inline LittleEndianBinaryReader& operator>>(int64_t& v) { if (!readBytes(reinterpret_cast<char*>(&v), 8)) return *this; if (m_order == BigEndian) { uint64_t u = static_cast<uint64_t>(v); u = ((u>>56)|((u>>40)&0xFF00)|((u>>24)&0xFF0000)|((u>>8)&0xFF000000)|((u<<8)&0xFF00000000ULL)|((u<<24)&0xFF0000000000ULL)|((u<<40)&0xFF000000000000ULL)|(u<<56)); v = static_cast<int64_t>(u); } return *this; }
    inline LittleEndianBinaryReader& operator>>(float& v) { if (!readBytes(reinterpret_cast<char*>(&v), 4)) return *this; if (m_order == BigEndian) { uint32_t u; std::memcpy(&u, &v, 4); u = ((u>>24)|((u>>8)&0xFF00)|((u<<8)&0xFF0000)|(u<<24)); std::memcpy(&v, &u, 4); } return *this; }
    inline LittleEndianBinaryReader& operator>>(bool& v) { uint8_t u; if (!readBytes(reinterpret_cast<char*>(&u), 1)) return *this; v = (u != 0); return *this; }
    inline int readRawData(char* buf, int len) { return (readBytes(buf, len) ? len : 0); }
    inline int skipRawData(int len) { std::vector<char> tmp(static_cast<size_t>(len), 0); return readBytes(tmp.data(), len) ? len : 0; }
};

// ============================================================================
// DynamicValue — tagged string-backed variant for simple IPC / settings
// ============================================================================

class DynamicValue {
public:
    enum Type { Invalid, Bool, Int, Double, String, StringList, ByteArray, Map };
    
    DynamicValue() : m_type(Invalid) {}
    explicit DynamicValue(bool b) : m_type(Bool), m_value(b ? "1" : "0") {}
    explicit DynamicValue(int i) : m_type(Int), m_value(std::to_string(i)) {}
    explicit DynamicValue(double d) : m_type(Double), m_value(std::to_string(d)) {}
    explicit DynamicValue(const Utf16String& s) : m_type(String), m_value(s.toStdString()) {}
    explicit DynamicValue(const std::string& s) : m_type(String), m_value(s) {}
    explicit DynamicValue(const char* s) : m_type(String), m_value(s ? s : "") {}
    explicit DynamicValue(const Utf16StringList& l) : m_type(StringList) {
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
    Utf16String toString() const { return Utf16String::fromStdString(m_value); }
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
using Vector = std::vector<T>;

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template <typename K, typename V>
using OrderedMap = std::map<K, V>;

template <typename T>
using UniqueSet = std::set<T>;

template <typename T>
using FifoQueue = std::queue<T>;

template <typename T>
using Stack = std::stack<T>;

using JsonStringMap = std::unordered_map<std::string, std::string>;
using JsonStringVector = std::vector<std::string>;

// ============================================================================
// POINTER REPLACEMENTS
// ============================================================================

template <typename T>
using RawPtr = T*;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
using WeakPtr = std::weak_ptr<T>;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

// ============================================================================
// CallbackHub — string-keyed callback lists
// ============================================================================

class CallbackHub {
public:
    virtual ~CallbackHub() = default;
    
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

#define RAWR_CALLBACK_HUB
#define RAWR_SIGNAL_STUB void
#define RAWR_SLOT_STUB void
#define RAWR_SIGNALS_SECTION protected
#define RAWR_SLOTS_SECTION
#define RAWR_EMIT_STUB

// ============================================================================
// Integer geometry primitives (UI layout helpers)
// ============================================================================

struct IntPoint {
    int x = 0, y = 0;
    IntPoint() = default;
    IntPoint(int x_, int y_) : x(x_), y(y_) {}
};

struct IntSize {
    int width = 0, height = 0;
    IntSize() = default;
    IntSize(int w, int h) : width(w), height(h) {}
};

struct IntRect {
    int x = 0, y = 0, width = 0, height = 0;
    IntRect() = default;
    IntRect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    bool contains(const IntPoint& p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }
    IntPoint topLeft() const { return IntPoint(x, y); }
    IntSize size() const { return IntSize(width, height); }
};

// ============================================================================
// Path metadata, directory helpers, Win32 file handle
// ============================================================================

class PathMetadata {
private:
    std::wstring m_path;
    
public:
    PathMetadata() = default;
    explicit PathMetadata(const std::wstring& path) : m_path(path) {}
    explicit PathMetadata(const std::string& path) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            m_path.resize(static_cast<size_t>(wlen - 1));
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, m_path.data(), wlen);
        }
    }
    explicit PathMetadata(const Utf16String& path) : m_path(path.toStdWString()) {}
    
    Utf16String filePath() const { return Utf16String(m_path); }
    Utf16String fileName() const {
        auto pos = m_path.find_last_of(L"\\/");
        return pos == std::wstring::npos ? Utf16String(m_path) : Utf16String(m_path.substr(pos + 1));
    }
    Utf16String path() const {
        auto pos = m_path.find_last_of(L"\\/");
        return pos == std::wstring::npos ? Utf16String(L"") : Utf16String(m_path.substr(0, pos));
    }
    Utf16String baseName() const {
        auto fname = fileName().toStdWString();
        auto dot = fname.find_last_of(L'.');
        return dot == std::wstring::npos ? Utf16String(fname) : Utf16String(fname.substr(0, dot));
    }
    Utf16String suffix() const {
        auto fname = fileName().toStdWString();
        auto dot = fname.find_last_of(L'.');
        return dot == std::wstring::npos ? Utf16String(L"") : Utf16String(fname.substr(dot + 1));
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
};

class DirUtil {
public:
    static bool mkpath(const Utf16String& path) {
        return CreateDirectoryW(path.utf16(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    static bool exists(const Utf16String& path) {
        DWORD attr = GetFileAttributesW(path.utf16());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }
    static Utf16String homePath() {
        const wchar_t* home = _wgetenv(L"USERPROFILE");
        return Utf16String(home ? home : L"C:\\");
    }
    static Utf16String tempPath() {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        return Utf16String(tmp);
    }
};

class Win32File : public SequentialByteDevice {
private:
    std::wstring m_path;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    mutable size_t m_pos = 0;
    mutable size_t m_cachedSize = 0;
    mutable bool m_sizeCached = false;
    
public:
    Win32File() = default;
    explicit Win32File(const Utf16String& path) : m_path(path.toStdWString()) {}
    explicit Win32File(const std::string& path) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            m_path.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &m_path[0], wlen);
        }
    }
    
    ~Win32File() override { close(); }
    
    // SequentialByteDevice
    int read(char* buf, int maxSize) override {
        if (m_handle == INVALID_HANDLE_VALUE || !buf || maxSize <= 0) return -1;
        DWORD n = 0;
        if (!ReadFile(m_handle, buf, maxSize, &n, nullptr)) return -1;
        m_pos += n;
        return static_cast<int>(n);
    }
    bool seek(size_t pos) override {
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER li; li.QuadPart = static_cast<LONGLONG>(pos);
        if (SetFilePointerEx(m_handle, li, nullptr, FILE_BEGIN) == 0) return false;
        m_pos = pos;
        return true;
    }
    size_t size() const override {
        if (m_handle == INVALID_HANDLE_VALUE) return 0;
        if (m_sizeCached) return m_cachedSize;
        LARGE_INTEGER sz;
        if (GetFileSizeEx(m_handle, &sz)) { m_cachedSize = static_cast<size_t>(sz.QuadPart); m_sizeCached = true; return m_cachedSize; }
        return 0;
    }
    bool isOpen() const override { return m_handle != INVALID_HANDLE_VALUE; }
    
    HANDLE handle() const { return m_handle; }
    
    bool open(int flags) {
        DWORD access = (flags & 1) ? GENERIC_READ : GENERIC_WRITE;
        m_handle = CreateFileW(m_path.c_str(), access, FILE_SHARE_READ, nullptr,
                                (flags & 2) ? CREATE_ALWAYS : OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        m_pos = 0;
        m_sizeCached = false;
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
        m_sizeCached = false;
    }
    
    bool remove() {
        close();
        return DeleteFileW(m_path.c_str()) != 0;
    }
    
    std::string readAll() {
        if (m_handle == INVALID_HANDLE_VALUE) return "";
        size_t sz = size();
        std::string content(sz, '\0');
        DWORD rd;
        ReadFile(m_handle, &content[0], static_cast<DWORD>(sz), &rd, nullptr);
        return content;
    }
    
    // Read up to maxSize bytes (for GGUF/LLM adapter)
    std::vector<uint8_t> read(int maxSize) {
        std::vector<uint8_t> out;
        if (m_handle == INVALID_HANDLE_VALUE || maxSize <= 0) return out;
        out.resize(static_cast<size_t>(maxSize), 0);
        DWORD rd = 0;
        ReadFile(m_handle, out.data(), static_cast<DWORD>(maxSize), &rd, nullptr);
        out.resize(static_cast<size_t>(rd));
        m_pos += rd;
        return out;
    }
    
    bool write(const std::string& data) {
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DWORD written;
        return WriteFile(m_handle, data.c_str(), static_cast<DWORD>(data.size()), &written, nullptr) != 0;
    }
    
    bool write(const char* data) {
        return write(data ? std::string(data) : std::string());
    }

    enum OpenMode { NotOpen = 0, ReadOnly = 1, WriteOnly = 2, ReadWrite = 3, Append = 4, Text = 8, Unbuffered = 16 };
};

// ============================================================================
// Win32File::open(int) flag bits (do not use class name as a namespace)
// ============================================================================

namespace FileAccess {
    static constexpr int NotOpen = 0;
    static constexpr int ReadOnly = 1;
    static constexpr int WriteOnly = 2;
    static constexpr int ReadWrite = 3;
    static constexpr int Append = 4;
    static constexpr int Text = 8;
}

// ============================================================================
// Win32Process — minimal CreateProcess wrapper
// ============================================================================

class Win32Process : public CallbackHub {
public:
    Win32Process() = default;
    ~Win32Process() { kill(); }
    
    void start(const Utf16String& program, const Utf16StringList& args) {
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
    Utf16String readAllStandardOutput() { return Utf16String(); }
    Utf16String readAllStandardError() { return Utf16String(); }

private:
    DWORD m_processId = 0;
};

// ============================================================================
// SimpleTimer / JoinableThread
// ============================================================================

class SimpleTimer {
public:
    void start(int msec) { m_running = true; }
    void stop() { m_running = false; }
    bool isActive() const { return m_running; }
    void setInterval(int msec) { m_interval = msec; }
    
protected:
    int m_interval = 0;
    bool m_running = false;
};

class JoinableThread {
public:
    virtual ~JoinableThread() = default;
    virtual void run() {}
    void start() { m_thread = std::thread(&JoinableThread::run, this); }
    void wait() { if (m_thread.joinable()) m_thread.join(); }
    bool isRunning() const { return m_thread.joinable(); }
    
private:
    std::thread m_thread;
};

// ============================================================================
// RgbaColor / UiFont — minimal display hints
// ============================================================================

struct RgbaColor {
    uint32_t rgba = 0;
    RgbaColor() = default;
    RgbaColor(int r, int g, int b, int a = 255) {
        rgba = (a << 24) | (r << 16) | (g << 8) | b;
    }
    static RgbaColor fromRgb(int r, int g, int b) { return RgbaColor(r, g, b); }
};

struct UiFont {
    Utf16String family;
    int pointSize = 12;
    bool bold = false;
    bool italic = false;
};

// ============================================================================
// StandardPaths
// ============================================================================

namespace StandardPaths {
    enum StandardLocation { DocumentsLocation = 0, AppDataLocation = 1, TempLocation = 2, HomeLocation = 3 };
    
    inline Utf16String writableLocation(StandardLocation location) {
        wchar_t path[MAX_PATH];
        switch (location) {
            case AppDataLocation: {
                if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
                    return Utf16String(path);
                }
                break;
            }
            case TempLocation: {
                GetTempPathW(MAX_PATH, path);
                return Utf16String(path);
            }
            case HomeLocation: {
                const wchar_t* home = _wgetenv(L"USERPROFILE");
                return Utf16String(home ? home : L"C:\\");
            }
            default: return Utf16String();
        }
        return Utf16String();
    }
}

// ============================================================================
// Environment variable lookup (UTF-8 name → UTF-16 value)
// ============================================================================

inline Utf16String environmentVariableUtf16(const char* varName) {
    if (!varName) return Utf16String();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, varName, -1, nullptr, 0);
    if (wlen <= 1) return Utf16String();
    std::wstring wvar(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, varName, -1, &wvar[0], wlen);
    
    wchar_t buf[32768];
    DWORD len = GetEnvironmentVariableW(wvar.c_str(), buf, sizeof(buf)/sizeof(wchar_t));
    return len > 0 ? Utf16String(buf) : Utf16String();
}



} // namespace compat_io
} // namespace rawrxd
#endif // RAWR_COMPAT_IO_HPP
