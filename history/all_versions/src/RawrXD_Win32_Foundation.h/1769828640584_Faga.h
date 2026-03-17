#pragma once
// RawrXD_Win32_Foundation.h
// Zero Qt dependencies - Pure Win32/C++20 foundation

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>

namespace RawrXD {

// UTF-8 <-> UTF-16 conversion helpers
inline std::wstring Utf8ToWide(const char* utf8, int len = -1) {
    if (!utf8) return {};
    if (len < 0) len = (int)strlen(utf8);
    if (len == 0) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, result.data(), wlen);
    return result;
}

inline std::string WideToUtf8(const wchar_t* wide, int len = -1) {
    if (!wide) return {};
    if (len < 0) len = (int)wcslen(wide);
    if (len == 0) return {};
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide, len, nullptr, 0, nullptr, nullptr);
    std::string result(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, len, result.data(), ulen, nullptr, nullptr);
    return result;
}

// QString replacement
class String {
    std::wstring data;
    
public:
    String() = default;
    String(const wchar_t* s) : data(s ? s : L"") {}
    String(const std::wstring& s) : data(s) {}
    String(const char* utf8) : data(Utf8ToWide(utf8)) {}
    String(const std::string& utf8) : data(Utf8ToWide(utf8.c_str(), (int)utf8.length())) {}
    
    const wchar_t* c_str() const { return data.c_str(); }
    const std::wstring& toStdWString() const { return data; }
    std::string toUtf8() const { return WideToUtf8(data.c_str(), (int)data.length()); }
    
    bool isEmpty() const { return data.empty(); }
    int length() const { return (int)data.length(); }
    int size() const { return (int)data.size(); }
    
    wchar_t at(int i) const { return data.at(i); }
    wchar_t operator[](int i) const { return data[i]; }
    
    String mid(int pos, int len = -1) const {
        if (pos < 0) pos = 0;
        if (pos >= (int)data.length()) return {};
        if (len < 0 || pos + len > (int)data.length()) len = (int)data.length() - pos;
        return String(data.substr(pos, len));
    }
    
    String left(int n) const { return mid(0, n); }
    String right(int n) const { return n >= (int)data.length() ? *this : mid((int)data.length() - n); }
    
    int indexOf(const String& s, int from = 0) const {
        if (from < 0) from = 0;
        auto pos = data.find(s.data, from);
        return pos == std::wstring::npos ? -1 : (int)pos;
    }
    
    int lastIndexOf(const String& s, int from = -1) const {
        if (from < 0) from = (int)data.length();
        auto pos = data.rfind(s.data, from);
        return pos == std::wstring::npos ? -1 : (int)pos;
    }
    
    bool startsWith(const String& s) const { return data.compare(0, s.data.length(), s.data) == 0; }
    bool endsWith(const String& s) const { 
        if (s.data.length() > data.length()) return false;
        return data.compare(data.length() - s.data.length(), s.data.length(), s.data) == 0;
    }
    
    String trimmed() const {
        size_t start = 0;
        while (start < data.length() && iswspace(data[start])) ++start;
        size_t end = data.length();
        while (end > start && iswspace(data[end-1])) --end;
        return String(data.substr(start, end - start));
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
    
    String& append(const String& s) { data += s.data; return *this; }
    String& prepend(const String& s) { data = s.data + data; return *this; }
    
    bool operator==(const String& o) const { return data == o.data; }
    bool operator!=(const String& o) const { return data != o.data; }
    bool operator<(const String& o) const { return data < o.data; }
    
    String operator+(const String& o) const { return String(data + o.data); }
    String& operator+=(const String& o) { data += o.data; return *this; }
    
    static String fromUtf8(const char* s, int len = -1) { return String(std::string(s, len < 0 ? strlen(s) : len)); }
    static String number(int n) { return String(std::to_wstring(n)); }
    static String number(double n, char fmt = 'g', int prec = 6) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*%c", prec, fmt, n);
        return String(buf);
    }
    static String fromRawData(const wchar_t* s, int len) { return String(std::wstring(s, len)); }
};

inline String operator+(const char* a, const String& b) { return String(a) + b; }
inline String operator+(const wchar_t* a, const String& b) { return String(a) + b; }

// QByteArray replacement
class ByteArray {
    std::vector<uint8_t> data;
    
public:
    ByteArray() = default;
    ByteArray(const char* s) : data((const uint8_t*)s, (const uint8_t*)s + strlen(s)) {}
    ByteArray(const char* s, int len) : data((const uint8_t*)s, (const uint8_t*)s + len) {}
    
    const char* constData() const { return (const char*)data.data(); }
    char* data_ptr() { return (char*)data.data(); }
    int size() const { return (int)data.size(); }
    bool isEmpty() const { return data.empty(); }
    
    void append(const char* s, int len) { data.insert(data.end(), (const uint8_t*)s, (const uint8_t*)s + len); }
    void append(const ByteArray& other) { data.insert(data.end(), other.data.begin(), other.data.end()); }
    void append(char c) { data.push_back((uint8_t)c); }
    void clear() { data.clear(); }
    void resize(int size) { data.resize(size); }
    
    char at(int i) const { return (char)data.at(i); }
    char operator[](int i) const { return (char)data[i]; }
    
    static ByteArray fromRawData(const char* s, int len) { return ByteArray(s, len); }
};

// QVariant replacement (simplified)
class Variant {
public:
    enum Type { Null, Bool, Int, LongLong, Double, String, ByteArray, Pointer };
    
private:
    Type type = Null;
    union {
        bool b;
        int64_t i;
        double d;
        void* ptr;
    };
    RawrXD::String s;
    ByteArray ba;
    
public:
    Variant() = default;
    Variant(bool v) : type(Bool), b(v) {}
    Variant(int v) : type(Int), i(v) {}
    Variant(int64_t v) : type(LongLong), i(v) {}
    Variant(double v) : type(Double), d(v) {}
    Variant(const RawrXD::String& v) : type(String), s(v) {}
    Variant(const char* v) : type(String), s(v) {}
    Variant(const ByteArray& v) : type(ByteArray), ba(v) {}
    template<typename T> Variant(T* p) : type(Pointer), ptr((void*)p) {}
    
    bool isNull() const { return type == Null; }
    bool isValid() const { return type != Null; }
    Type getType() const { return type; }
    
    bool toBool() const { return type == Bool ? b : false; }
    int toInt() const { return type == Int ? (int)i : (type == LongLong ? (int)i : 0); }
    int64_t toLongLong() const { return type == LongLong || type == Int ? i : 0; }
    double toDouble() const { return type == Double ? d : (type == Int || type == LongLong ? (double)i : 0.0); }
    RawrXD::String toString() const; // Implemented later to avoid circular dependency
    ByteArray toByteArray() const { return type == ByteArray ? ba : ByteArray(); }
    template<typename T> T* toPointer() const { return type == Pointer ? (T*)ptr : nullptr; }
};

inline RawrXD::String Variant::toString() const {
    if (type == String) return s;
    if (type == Int) return String::number((int)i);
    if (type == Double) return String::number(d);
    if (type == Bool) return b ? String(L"true") : String(L"false");
    return String();
}

// QList/QVector replacement
template<typename T>
class List : public std::vector<T> {
public:
    List() = default;
    List(std::initializer_list<T> init) : std::vector<T>(init) {}
    
    void append(const T& v) { this->push_back(v); }
    void prepend(const T& v) { this->insert(this->begin(), v); }
    void removeAt(int i) { this->erase(this->begin() + i); }
    void removeOne(const T& v) {
        auto it = std::find(this->begin(), this->end(), v);
        if (it != this->end()) this->erase(it);
    }
    bool contains(const T& v) const { return std::find(this->begin(), this->end(), v) != this->end(); }
    int indexOf(const T& v) const {
        auto it = std::find(this->begin(), this->end(), v);
        return it == this->end() ? -1 : (int)(it - this->begin());
    }
    T& first() { return this->front(); }
    T& last() { return this->back(); }
    const T& first() const { return this->front(); }
    const T& last() const { return this->back(); }
    int count() const { return (int)this->size(); }
    int size() const { return (int)std::vector<T>::size(); }
    bool isEmpty() const { return this->empty(); }
    void clear() { std::vector<T>::clear(); }
};

// QMap replacement
template<typename K, typename V>
class Map : public std::unordered_map<K, V> {
public:
    bool contains(const K& key) const { return this->find(key) != this->end(); }
    void insert(const K& key, const V& value) { (*this)[key] = value; }
    void remove(const K& key) { this->erase(key); }
    List<K> keys() const {
        List<K> result;
        for (const auto& p : *this) result.append(p.first);
        return result;
    }
    List<V> values() const {
        List<V> result;
        for (const auto& p : *this) result.append(p.second);
        return result;
    }
};

// QSet replacement
template<typename T>
class Set : public std::unordered_set<T> {
public:
    void insert(const T& v) { std::unordered_set<T>::insert(v); }
    void remove(const T& v) { std::unordered_set<T>::erase(v); }
    bool contains(const T& v) const { return this->find(v) != this->end(); }
    List<T> toList() const { return List<T>(this->begin(), this->end()); }
};

// QPoint/QSize/QRect replacements
struct Point {
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x_, int y_) : x(x_), y(y_) {}
    bool isNull() const { return x == 0 && y == 0; }
    Point& operator+=(const Point& p) { x += p.x; y += p.y; return *this; }
    Point& operator-=(const Point& p) { x -= p.x; y -= p.y; return *this; }
    Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }
    Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }
};

struct Size {
    int width, height;
    Size() : width(0), height(0) {}
    Size(int w, int h) : width(w), height(h) {}
    bool isNull() const { return width == 0 && height == 0; }
    bool isEmpty() const { return width <= 0 || height <= 0; }
    bool isValid() const { return width >= 0 && height >= 0; }
};

struct Rect {
    int x, y, width, height;
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    Rect(const Point& p, const Size& s) : x(p.x), y(p.y), width(s.width), height(s.height) {}
    
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
};

} // namespace RawrXD
