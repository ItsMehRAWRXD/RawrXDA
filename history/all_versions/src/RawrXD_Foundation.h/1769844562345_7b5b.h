#pragma once
// RawrXD_Foundation.h
// Zero Qt dependencies - Core types for RAWRXD IDE
// This replaces ALL Qt core types (QString, QByteArray, QList, QMap, etc.)

#ifndef RAWRXD_FOUNDATION_H
#define RAWRXD_FOUNDATION_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef RAWRXD_NO_QT
#define RAWRXD_NO_QT
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
#include <cwchar>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>

namespace RawrXD {

// ═════════════════════════════════════════════════════════════════════════════
// UTF-8 / UTF-16 Conversion (Replace QString UTF handling)
// ═════════════════════════════════════════════════════════════════════════════

inline std::wstring Utf8ToWide(const char* utf8, int len = -1) {
    if (!utf8) return {};
    if (len < 0) len = static_cast<int>(strlen(utf8));
    if (len == 0) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, result.data(), wlen);
    return result;
}

inline std::string WideToUtf8(const wchar_t* wide, int len = -1) {
    if (!wide) return {};
    if (len < 0) len = static_cast<int>(wcslen(wide));
    if (len == 0) return {};
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide, len, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return {};
    std::string result(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, len, result.data(), ulen, nullptr, nullptr);
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
// String (Replace QString)
// ═════════════════════════════════════════════════════════════════════════════

class String {
    std::wstring data;
    
public:
    String() = default;
    String(const wchar_t* s) : data(s ? s : L"") {}
    String(const std::wstring& s) : data(s) {}
    String(const char* utf8) : data(Utf8ToWide(utf8)) {}
    String(const std::string& utf8) : data(Utf8ToWide(utf8.c_str(), static_cast<int>(utf8.length()))) {}
    String(wchar_t c) : data(1, c) {}
    
    // Access
    const wchar_t* c_str() const { return data.c_str(); }
    const std::wstring& toStdWString() const { return data; }
    std::string toUtf8() const { return WideToUtf8(data.c_str(), static_cast<int>(data.length())); }
    const wchar_t* constData() const { return data.data(); }
    wchar_t* data_ptr() { return data.data(); }
    
    // Properties
    bool isEmpty() const { return data.empty(); }
    bool isNull() const { return data.empty(); }
    int length() const { return static_cast<int>(data.length()); }
    int size() const { return static_cast<int>(data.size()); }
    
    // Element access
    wchar_t at(int i) const { return data.at(static_cast<size_t>(i)); }
    wchar_t operator[](int i) const { return data[static_cast<size_t>(i)]; }
    wchar_t& operator[](int i) { return data[static_cast<size_t>(i)]; }
    
    // Substrings
    String mid(int pos, int len = -1) const {
        if (pos < 0) pos = 0;
        if (pos >= static_cast<int>(data.length())) return {};
        if (len < 0 || pos + len > static_cast<int>(data.length())) 
            len = static_cast<int>(data.length()) - pos;
        return String(data.substr(pos, len));
    }
    String left(int n) const { return mid(0, n); }
    String right(int n) const { 
        return n >= static_cast<int>(data.length()) ? *this : 
               mid(static_cast<int>(data.length()) - n); 
    }
    String sliced(int pos, int len = -1) const { return mid(pos, len); }
    String chopped(int n) const { return left(length() - n); }
    
    // Search
    int indexOf(const String& s, int from = 0) const {
        if (from < 0) from = 0;
        auto pos = data.find(s.data, from);
        return pos == std::wstring::npos ? -1 : static_cast<int>(pos);
    }
    int indexOf(wchar_t c, int from = 0) const {
        if (from < 0) from = 0;
        auto pos = data.find(c, from);
        return pos == std::wstring::npos ? -1 : static_cast<int>(pos);
    }
    int lastIndexOf(const String& s, int from = -1) const {
        if (from < 0) from = static_cast<int>(data.length());
        auto pos = data.rfind(s.data, from);
        return pos == std::wstring::npos ? -1 : static_cast<int>(pos);
    }
    int lastIndexOf(wchar_t c, int from = -1) const {
        if (from < 0) from = static_cast<int>(data.length());
        auto pos = data.rfind(c, from);
        return pos == std::wstring::npos ? -1 : static_cast<int>(pos);
    }
    bool contains(const String& s) const { return indexOf(s) >= 0; }
    bool contains(wchar_t c) const { return indexOf(c) >= 0; }
    bool startsWith(const String& s) const { 
        return data.compare(0, s.data.length(), s.data) == 0; 
    }
    bool startsWith(wchar_t c) const { return !data.empty() && data[0] == c; }
    bool endsWith(const String& s) const { 
        if (s.data.length() > data.length()) return false;
        return data.compare(data.length() - s.data.length(), s.data.length(), s.data) == 0;
    }
    bool endsWith(wchar_t c) const { return !data.empty() && data.back() == c; }
    
    // Modification
    String& append(const String& s) { data += s.data; return *this; }
    String& append(wchar_t c) { data += c; return *this; }
    String& prepend(const String& s) { data = s.data + data; return *this; }
    String& insert(int pos, const String& s) { 
        data.insert(pos, s.data); 
        return *this; 
    }
    String& remove(int pos, int len) {
        data.erase(pos, len);
        return *this;
    }
    String& remove(const String& s) {
        size_t pos = 0;
        while ((pos = data.find(s.data, pos)) != std::wstring::npos) {
            data.erase(pos, s.data.length());
        }
        return *this;
    }
    String& replace(int pos, int len, const String& s) {
        data.replace(pos, len, s.data);
        return *this;
    }
    String& replace(const String& before, const String& after) {
        size_t pos = 0;
        while ((pos = data.find(before.data, pos)) != std::wstring::npos) {
            data.replace(pos, before.data.length(), after.data);
            pos += after.data.length();
        }
        return *this;
    }
    String trimmed() const {
        size_t start = 0;
        while (start < data.length() && iswspace(data[start])) ++start;
        size_t end = data.length();
        while (end > start && iswspace(data[end-1])) --end;
        return String(data.substr(start, end - start));
    }
    String simplified() const {
        String result = trimmed();
        bool inSpace = false;
        std::wstring out;
        for (wchar_t c : result.data) {
            if (iswspace(c)) {
                if (!inSpace) {
                    out += L' ';
                    inSpace = true;
                }
            } else {
                out += c;
                inSpace = false;
            }
        }
        return String(out);
    }
    String toLower() const {
        std::wstring result = data;
        std::transform(result.begin(), result.end(), result.begin(), ::towlower);
        return String(result);
    }
    String toUpper() const {
        std::wstring result = data;
        std::transform(result.begin(), result.end(), result.begin(), ::towupper);
        return String(result);
    }
    
    // Comparison
    int compare(const String& other, bool caseSensitive = true) const {
        if (caseSensitive) return data.compare(other.data);
        return toLower().data.compare(other.toLower().data);
    }
    bool operator==(const String& o) const { return data == o.data; }
    bool operator!=(const String& o) const { return data != o.data; }
    bool operator<(const String& o) const { return data < o.data; }
    bool operator>(const String& o) const { return data > o.data; }
    bool operator<=(const String& o) const { return data <= o.data; }
    bool operator>=(const String& o) const { return data >= o.data; }
    
    // Concatenation
    String operator+(const String& o) const { return String(data + o.data); }
    String operator+(wchar_t c) const { return String(data + c); }
    String& operator+=(const String& o) { data += o.data; return *this; }
    String& operator+=(wchar_t c) { data += c; return *this; }
    
    // Conversion
    int toInt(bool* ok = nullptr, int base = 10) const;
    uint32_t toUInt(bool* ok = nullptr, int base = 10) const;
    int64_t toLongLong(bool* ok = nullptr, int base = 10) const;
    double toDouble(bool* ok = nullptr) const;
    float toFloat(bool* ok = nullptr) const;
    
    // Static constructors
    static String fromUtf8(const char* s, int len = -1) { 
        return String(std::string(s, len < 0 ? strlen(s) : len)); 
    }
    static String fromLatin1(const char* s, int len = -1);
    static String fromLocal8Bit(const char* s, int len = -1);
    static String fromRawData(const wchar_t* s, int len) { 
        return String(std::wstring(s, len)); 
    }
    static String number(int n, int base = 10);
    static String number(uint32_t n, int base = 10);
    static String number(int64_t n, int base = 10);
    static String number(double n, char fmt = 'g', int prec = 6);
    static String asprintf(const char* format, ...);
    
    // Iterators
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
};

// Global operators
inline String operator+(const wchar_t* a, const String& b) { return String(a) + b; }
inline String operator+(const char* a, const String& b) { return String(a) + b; }
inline String operator+(wchar_t a, const String& b) { return String(a) + b; }

// ═════════════════════════════════════════════════════════════════════════════
// ByteArray (Replace QByteArray)
// ═════════════════════════════════════════════════════════════════════════════

class ByteArray {
    std::vector<uint8_t> data;
    
public:
    ByteArray() = default;
    ByteArray(int size) : data(size) {}
    ByteArray(int size, char c) : data(size, static_cast<uint8_t>(c)) {}
    ByteArray(const char* s) : data(reinterpret_cast<const uint8_t*>(s), 
                                     reinterpret_cast<const uint8_t*>(s) + strlen(s)) {}
    ByteArray(const char* s, int len) : data(reinterpret_cast<const uint8_t*>(s), 
                                              reinterpret_cast<const uint8_t*>(s) + len) {}
    ByteArray(const ByteArray& other) = default;
    ByteArray(ByteArray&& other) noexcept = default;
    
    ByteArray& operator=(const ByteArray& other) = default;
    ByteArray& operator=(ByteArray&& other) noexcept = default;
    
    // Access
    const char* constData() const { return reinterpret_cast<const char*>(data.data()); }
    char* data_ptr() { return reinterpret_cast<char*>(data.data()); }
    const uint8_t* constData_uint8() const { return data.data(); }
    uint8_t* data_uint8() { return data.data(); }
    
    // Properties
    bool isEmpty() const { return data.empty(); }
    bool isNull() const { return data.empty(); }
    int size() const { return static_cast<int>(data.size()); }
    int count() const { return static_cast<int>(data.size()); }
    int capacity() const { return static_cast<int>(data.capacity()); }
    
    // Element access
    char at(int i) const { return static_cast<char>(data.at(static_cast<size_t>(i))); }
    char operator[](int i) const { return static_cast<char>(data[static_cast<size_t>(i)]); }
    char& operator[](int i) { return reinterpret_cast<char&>(data[static_cast<size_t>(i)]); }
    
    // Modification
    ByteArray& append(const char* s, int len) {
        data.insert(data.end(), reinterpret_cast<const uint8_t*>(s), 
                    reinterpret_cast<const uint8_t*>(s) + len);
        return *this;
    }
    ByteArray& append(const ByteArray& other) {
        data.insert(data.end(), other.data.begin(), other.data.end());
        return *this;
    }
    ByteArray& append(char c) { data.push_back(static_cast<uint8_t>(c)); return *this; }
    ByteArray& prepend(const ByteArray& other) {
        data.insert(data.begin(), other.data.begin(), other.data.end());
        return *this;
    }
    ByteArray& insert(int pos, const ByteArray& other) {
        data.insert(data.begin() + pos, other.data.begin(), other.data.end());
        return *this;
    }
    ByteArray& remove(int pos, int len) {
        data.erase(data.begin() + pos, data.begin() + pos + len);
        return *this;
    }
    ByteArray& clear() { data.clear(); return *this; }
    ByteArray& resize(int size) { data.resize(size); return *this; }
    ByteArray& reserve(int size) { data.reserve(size); return *this; }
    ByteArray& squeeze() { data.shrink_to_fit(); return *this; }
    
    // Search
    int indexOf(const ByteArray& pattern, int from = 0) const;
    int lastIndexOf(const ByteArray& pattern, int from = -1) const;
    bool contains(const ByteArray& pattern) const { return indexOf(pattern) >= 0; }
    int count(const ByteArray& pattern) const;
    
    // Comparison
    bool operator==(const ByteArray& o) const { return data == o.data; }
    bool operator!=(const ByteArray& o) const { return data != o.data; }
    bool operator<(const ByteArray& o) const { return data < o.data; }
    
    // Conversion
    String toString() const { return String::fromUtf8(constData(), size()); }
    String toLatin1String() const;
    String toHex() const;
    String toBase64() const;
    static ByteArray fromHex(const String& hex);
    static ByteArray fromBase64(const String& base64);
    static ByteArray fromRawData(const char* s, int len);
    
    // Iterators
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
};

// ═════════════════════════════════════════════════════════════════════════════
// Vector (Replace QList/QVector)
// ═════════════════════════════════════════════════════════════════════════════

template<typename T>
class Vector : public std::vector<T> {
public:
    Vector() = default;
    Vector(int size) : std::vector<T>(size) {}
    Vector(int size, const T& val) : std::vector<T>(size, val) {}
    Vector(std::initializer_list<T> init) : std::vector<T>(init) {}
    
    // Qt-compatible methods
    void append(const T& v) { this->push_back(v); }
    void append(T&& v) { this->push_back(std::move(v)); }
    void prepend(const T& v) { this->insert(this->begin(), v); }
    void removeAt(int i) { this->erase(this->begin() + i); }
    void removeOne(const T& v) {
        auto it = std::find(this->begin(), this->end(), v);
        if (it != this->end()) this->erase(it);
    }
    void removeAll(const T& v) {
        this->erase(std::remove(this->begin(), this->end(), v), this->end());
    }
    bool contains(const T& v) const { 
        return std::find(this->begin(), this->end(), v) != this->end(); 
    }
    int indexOf(const T& v) const {
        auto it = std::find(this->begin(), this->end(), v);
        return it == this->end() ? -1 : static_cast<int>(it - this->begin());
    }
    int lastIndexOf(const T& v) const {
        auto it = std::find(this->rbegin(), this->rend(), v);
        return it == this->rend() ? -1 : static_cast<int>(this->rend() - it - 1);
    }
    T& first() { return this->front(); }
    T& last() { return this->back(); }
    const T& first() const { return this->front(); }
    const T& last() const { return this->back(); }
    T takeAt(int i) {
        T val = std::move((*this)[i]);
        removeAt(i);
        return val;
    }
    T takeFirst() { return takeAt(0); }
    T takeLast() { 
        T val = std::move(this->back());
        this->pop_back();
        return val;
    }
    int count() const { return static_cast<int>(this->size()); }
    int size() const { return static_cast<int>(std::vector<T>::size()); }
    bool isEmpty() const { return this->empty(); }
    void clear() { std::vector<T>::clear(); }
    
    // Qt-style value/find
    const T& value(int i) const { return (*this)[i]; }
    const T& value(int i, const T& defaultValue) const {
        return (i >= 0 && i < size()) ? (*this)[i] : defaultValue;
    }
    
    // Insert/append variations
    void insert(int i, const T& v) { this->insert(this->begin() + i, v); }
    void replace(int i, const T& v) { (*this)[i] = v; }
    void swap(int i, int j) { std::swap((*this)[i], (*this)[j]); }
    
    // Move
    void move(int from, int to) {
        if (from == to) return;
        T val = std::move((*this)[from]);
        if (from < to) {
            std::move(this->begin() + from + 1, this->begin() + to + 1, this->begin() + from);
        } else {
            std::move_backward(this->begin() + to, this->begin() + from, this->begin() + from + 1);
        }
        (*this)[to] = std::move(val);
    }
    
    // Resize
    void resize(int size) { std::vector<T>::resize(size); }
    void reserve(int size) { std::vector<T>::reserve(size); }
    void squeeze() { std::vector<T>::shrink_to_fit(); }
};

// ═════════════════════════════════════════════════════════════════════════════
// Map (Replace QMap)
// ═════════════════════════════════════════════════════════════════════════════

template<typename K, typename V>
class Map : public std::map<K, V> {
public:
    // Qt-compatible methods
    bool contains(const K& k) const { return this->find(k) != this->end(); }
    void insert(const K& k, const V& v) { (*this)[k] = v; }
    void insert(const std::pair<K, V>& p) { std::map<K, V>::insert(p); }
    void remove(const K& k) { this->erase(k); }
    int removeIf(std::function<bool(const K&, const V&)> pred);
    
    Vector<K> keys() const {
        Vector<K> result;
        for (const auto& p : *this) result.append(p.first);
        return result;
    }
    Vector<V> values() const {
        Vector<V> result;
        for (const auto& p : *this) result.append(p.second);
        return result;
    }
    int count() const { return static_cast<int>(this->size()); }
    int size() const { return static_cast<int>(std::map<K, V>::size()); }
    bool isEmpty() const { return this->empty(); }
    
    // Access with default
    V value(const K& k, const V& defaultValue = V()) const {
        auto it = this->find(k);
        return it != this->end() ? it->second : defaultValue;
    }
    
    // Find
    typename std::map<K, V>::iterator find(const K& k) { return std::map<K, V>::find(k); }
    typename std::map<K, V>::const_iterator find(const K& k) const { return std::map<K, V>::find(k); }
};

// HashMap (Replace QHash)
template<typename K, typename V>
class HashMap : public std::unordered_map<K, V> {
public:
    bool contains(const K& k) const { return this->find(k) != this->end(); }
    void insert(const K& k, const V& v) { (*this)[k] = v; }
    void remove(const K& k) { this->erase(k); }
    Vector<K> keys() const {
        Vector<K> result;
        for (const auto& p : *this) result.append(p.first);
        return result;
    }
    Vector<V> values() const {
        Vector<V> result;
        for (const auto& p : *this) result.append(p.second);
        return result;
    }
    int count() const { return static_cast<int>(this->size()); }
    bool isEmpty() const { return this->empty(); }
};

// ═════════════════════════════════════════════════════════════════════════════
// Set (Replace QSet)
// ═════════════════════════════════════════════════════════════════════════════

template<typename T>
class Set : public std::unordered_set<T> {
public:
    void insert(const T& v) { std::unordered_set<T>::insert(v); }
    void remove(const T& v) { std::unordered_set<T>::erase(v); }
    bool contains(const T& v) const { return this->find(v) != this->end(); }
    Vector<T> toList() const { return Vector<T>(this->begin(), this->end()); }
    int count() const { return static_cast<int>(this->size()); }
    bool isEmpty() const { return this->empty(); }
    
    Set<T> intersect(const Set<T>& other) const {
        Set<T> result;
        for (const auto& v : *this) {
            if (other.contains(v)) result.insert(v);
        }
        return result;
    }
    Set<T> unite(const Set<T>& other) const {
        Set<T> result = *this;
        for (const auto& v : other) result.insert(v);
        return result;
    }
    Set<T> subtract(const Set<T>& other) const {
        Set<T> result;
        for (const auto& v : *this) {
            if (!other.contains(v)) result.insert(v);
        }
        return result;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Geometry Types (Replace QPoint, QSize, QRect)
// ═════════════════════════════════════════════════════════════════════════════

struct Point {
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x_, int y_) : x(x_), y(y_) {}
    explicit Point(const POINT& p) : x(p.x), y(p.y) {}
    
    bool isNull() const { return x == 0 && y == 0; }
    int manhattanLength() const { return abs(x) + abs(y); }
    
    Point& operator+=(const Point& p) { x += p.x; y += p.y; return *this; }
    Point& operator-=(const Point& p) { x -= p.x; y -= p.y; return *this; }
    Point& operator*=(int factor) { x *= factor; y *= factor; return *this; }
    Point& operator/=(int divisor) { x /= divisor; y /= divisor; return *this; }
    
    Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }
    Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }
    Point operator*(int factor) const { return Point(x * factor, y * factor); }
    Point operator/(int divisor) const { return Point(x / divisor, y / divisor); }
    
    bool operator==(const Point& p) const { return x == p.x && y == p.y; }
    bool operator!=(const Point& p) const { return !(*this == p); }
    
    operator POINT() const { POINT p = { x, y }; return p; }
};

struct Size {
    int width, height;
    Size() : width(0), height(0) {}
    Size(int w, int h) : width(w), height(h) {}
    explicit Size(const SIZE& s) : width(s.cx), height(s.cy) {}
    
    bool isNull() const { return width == 0 && height == 0; }
    bool isEmpty() const { return width <= 0 || height <= 0; }
    bool isValid() const { return width >= 0 && height >= 0; }
    
    int area() const { return width * height; }
    
    Size& operator+=(const Size& s) { width += s.width; height += s.height; return *this; }
    Size& operator-=(const Size& s) { width -= s.width; height -= s.height; return *this; }
    Size& operator*=(int factor) { width *= factor; height *= factor; return *this; }
    Size& operator/=(int divisor) { width /= divisor; height /= divisor; return *this; }
    
    Size operator+(const Size& s) const { return Size(width + s.width, height + s.height); }
    Size operator-(const Size& s) const { return Size(width - s.width, height - s.height); }
    Size operator*(int factor) const { return Size(width * factor, height * factor); }
    Size operator/(int divisor) const { return Size(width / divisor, height / divisor); }
    
    bool operator==(const Size& s) const { return width == s.width && height == s.height; }
    bool operator!=(const Size& s) const { return !(*this == s); }
    
    operator SIZE() const { SIZE s = { width, height }; return s; }
};

struct Rect {
    int x, y, width, height;
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    Rect(const Point& p, const Size& s) : x(p.x), y(p.y), width(s.width), height(s.height) {}
    explicit Rect(const RECT& r) : x(r.left), y(r.top), width(r.right - r.left), height(r.bottom - r.top) {}
    
    bool isNull() const { return width == 0 && height == 0 && x == 0 && y == 0; }
    bool isEmpty() const { return width <= 0 || height <= 0; }
    bool isValid() const { return width >= 0 && height >= 0; }
    
    int left() const { return x; }
    int top() const { return y; }
    int right() const { return x + width; }
    int bottom() const { return y + height; }
    
    Point topLeft() const { return Point(x, y); }
    Point topRight() const { return Point(x + width, y); }
    Point bottomLeft() const { return Point(x, y + height); }
    Point bottomRight() const { return Point(x + width, y + height); }
    Point center() const { return Point(x + width/2, y + height/2); }
    
    Size size() const { return Size(width, height); }
    
    bool contains(const Point& p) const { 
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height; 
    }
    bool contains(const Rect& r) const {
        return r.x >= x && r.right() <= right() && r.y >= y && r.bottom() <= bottom();
    }
    bool intersects(const Rect& r) const {
        return !(r.right() <= x || r.x >= right() || r.bottom() <= y || r.y >= bottom());
    }
    Rect intersected(const Rect& r) const {
        int x1 = std::max(x, r.x);
        int y1 = std::max(y, r.y);
        int x2 = std::min(right(), r.right());
        int y2 = std::min(bottom(), r.bottom());
        if (x2 <= x1 || y2 <= y1) return Rect();
        return Rect(x1, y1, x2 - x1, y2 - y1);
    }
    Rect united(const Rect& r) const {
        int x1 = std::min(x, r.x);
        int y1 = std::min(y, r.y);
        int x2 = std::max(right(), r.right());
        int y2 = std::max(bottom(), r.bottom());
        return Rect(x1, y1, x2 - x1, y2 - y1);
    }
    void adjust(int dx1, int dy1, int dx2, int dy2) {
        x += dx1; y += dy1; width += dx2 - dx1; height += dy2 - dy1;
    }
    Rect adjusted(int dx1, int dy1, int dx2, int dy2) const {
        Rect r = *this;
        r.adjust(dx1, dy1, dx2, dy2);
        return r;
    }
    void translate(int dx, int dy) { x += dx; y += dy; }
    void translate(const Point& p) { x += p.x; y += p.y; }
    Rect translated(int dx, int dy) const { return Rect(x + dx, y + dy, width, height); }
    
    bool operator==(const Rect& r) const { 
        return x == r.x && y == r.y && width == r.width && height == r.height; 
    }
    bool operator!=(const Rect& r) const { return !(*this == r); }
    
    operator RECT() const { RECT r = { x, y, x + width, y + height }; return r; }
};

// ═════════════════════════════════════════════════════════════════════════════
// Color (Replace QColor)
// ═════════════════════════════════════════════════════════════════════════════

struct Color {
    uint8_t r, g, b, a;
    Color() : r(0), g(0), b(0), a(255) {}
    Color(int red, int green, int blue, int alpha = 255) 
        : r(static_cast<uint8_t>(red)), g(static_cast<uint8_t>(green)), 
          b(static_cast<uint8_t>(blue)), a(static_cast<uint8_t>(alpha)) {}
    Color(uint32_t rgba) 
        : r((rgba >> 16) & 0xFF), g((rgba >> 8) & 0xFF), b(rgba & 0xFF), a((rgba >> 24) & 0xFF) {}
    
    static Color fromRgb(int r, int g, int b) { return Color(r, g, b); }
    static Color fromRgba(int r, int g, int b, int a) { return Color(r, g, b, a); }
    static Color fromRgbF(float r, float g, float b, float a = 1.0f) {
        return Color(static_cast<int>(r * 255), static_cast<int>(g * 255), 
                     static_cast<int>(b * 255), static_cast<int>(a * 255));
    }
    
    int red() const { return r; }
    int green() const { return g; }
    int blue() const { return b; }
    int alpha() const { return a; }
    
    void setRed(int red) { r = static_cast<uint8_t>(red); }
    void setGreen(int green) { g = static_cast<uint8_t>(green); }
    void setBlue(int blue) { b = static_cast<uint8_t>(blue); }
    void setAlpha(int alpha) { a = static_cast<uint8_t>(alpha); }
    
    uint32_t rgb() const { return (r << 16) | (g << 8) | b; }
    uint32_t rgba() const { return (a << 24) | (r << 16) | (g << 8) | b; }
    uint32_t argb() const { return (a << 24) | (r << 16) | (g << 8) | b; }
    uint32_t abgr() const { return (a << 24) | (b << 16) | (g << 8) | r; }
    
    Color lighter(int factor = 150) const;
    Color darker(int factor = 200) const;
    
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Cyan;
    static const Color Magenta;
    static const Color Yellow;
    static const Color Gray;
    static const Color LightGray;
    static const Color DarkGray;
    static const Color Transparent;
};

// ═════════════════════════════════════════════════════════════════════════════
// DateTime (Replace QDateTime)
// ═════════════════════════════════════════════════════════════════════════════

class DateTime {
    FILETIME ft;
    
public:
    DateTime();
    static DateTime currentDateTime();
    static DateTime fromString(const String& s, const String& format);
    
    String toString(const String& format) const;
    int64_t toMSecsSinceEpoch() const;
    static DateTime fromMSecsSinceEpoch(int64_t msecs);
    
    bool operator<(const DateTime& other) const;
    bool operator>(const DateTime& other) const;
    int64_t msecsTo(const DateTime& other) const;
    int64_t secsTo(const DateTime& other) const;
    
    DateTime addMSecs(int64_t msecs) const;
    DateTime addSecs(int s) const;
    DateTime addDays(int days) const;
};

// ═════════════════════════════════════════════════════════════════════════════
// URL (Replace QUrl)
// ═════════════════════════════════════════════════════════════════════════════

class Url {
    String scheme;
    String host;
    int port = -1;
    String path;
    String query;
    String fragment;
    
public:
    Url() = default;
    explicit Url(const String& url);
    
    bool isValid() const;
    bool isEmpty() const;
    bool isLocalFile() const { return scheme == L"file"; }
    
    String toString() const;
    String toEncoded() const;
    String toLocalFile() const;
    static Url fromLocalFile(const String& path);
    
    String getScheme() const { return scheme; }
    String getHost() const { return host; }
    int getPort() const { return port; }
    String getPath() const { return path; }
    String getQuery() const { return query; }
    String getFragment() const { return fragment; }
    
    void setScheme(const String& s) { scheme = s; }
    void setHost(const String& h) { host = h; }
    void setPort(int p) { port = p; }
    void setPath(const String& p) { path = p; }
    void setQuery(const String& q) { query = q; }
    void setFragment(const String& f) { fragment = f; }
    
    String getFileName() const;
};

// ═════════════════════════════════════════════════════════════════════════════
// Global Functions
// ═════════════════════════════════════════════════════════════════════════════

inline int qRound(double d) { return static_cast<int>(d + (d >= 0 ? 0.5 : -0.5)); }
inline int64_t qRound64(double d) { return static_cast<int64_t>(d + (d >= 0 ? 0.5 : -0.5)); }

} // namespace RawrXD

#endif // RAWRXD_FOUNDATION_H
