/**
 * @file simple_json.hpp
 * @brief Minimal header-only JSON value type and utilities
 *
 * Replaces Qt JSON (QJsonObject, QJsonArray, QJsonDocument, QJsonValue)
 * with a lightweight C++20 implementation. No external dependencies.
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <ctime>

// ═══════════════════════════════════════════════════════════════════════════
// String Utility Helpers
// ═══════════════════════════════════════════════════════════════════════════

namespace strutil {

/**
 * @brief Split string by single-char delimiter
 */
inline std::vector<std::string> split(const std::string& s, char delimiter, bool skipEmpty = false) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end;
    while ((end = s.find(delimiter, start)) != std::string::npos) {
        std::string token = s.substr(start, end - start);
        if (!skipEmpty || !token.empty()) tokens.push_back(token);
        start = end + 1;
    }
    std::string last = s.substr(start);
    if (!skipEmpty || !last.empty()) tokens.push_back(last);
    return tokens;
}

/**
 * @brief Split string by string delimiter
 */
inline std::vector<std::string> split(const std::string& s, const std::string& delimiter, bool skipEmpty = false) {
    std::vector<std::string> tokens;
    if (delimiter.empty()) { tokens.push_back(s); return tokens; }
    size_t start = 0;
    size_t end;
    while ((end = s.find(delimiter, start)) != std::string::npos) {
        std::string token = s.substr(start, end - start);
        if (!skipEmpty || !token.empty()) tokens.push_back(token);
        start = end + delimiter.size();
    }
    std::string last = s.substr(start);
    if (!skipEmpty || !last.empty()) tokens.push_back(last);
    return tokens;
}

/**
 * @brief Join strings with separator
 */
inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += sep;
        result += parts[i];
    }
    return result;
}

/**
 * @brief Count occurrences of substring
 */
inline size_t countOccurrences(const std::string& str, const std::string& sub) {
    if (sub.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = str.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.size();
    }
    return count;
}

/**
 * @brief Replace all occurrences of 'from' with 'to'
 */
inline std::string replaceAll(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

/**
 * @brief Case-insensitive contains
 */
inline bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

/**
 * @brief Get current timestamp as "YYYYMMDD_HHMMSS"
 */
inline std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_now {};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_now);
    return std::string(buf);
}

/**
 * @brief Get current ISO timestamp
 */
inline std::string currentISOTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_now {};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_now);
    return std::string(buf);
}

} // namespace strutil

// ═══════════════════════════════════════════════════════════════════════════
// JsonValue — Minimal JSON value type
// ═══════════════════════════════════════════════════════════════════════════

class JsonValue {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };
    using Array  = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

private:
    Type        m_type   = Type::Null;
    bool        m_bool   = false;
    int64_t     m_int    = 0;
    double      m_double = 0.0;
    std::string m_string;
    Array       m_array;
    Object      m_object;

public:
    // ── Constructors ──────────────────────────────────────────────────
    JsonValue() = default;
    JsonValue(std::nullptr_t)             : m_type(Type::Null) {}
    JsonValue(bool v)                     : m_type(Type::Bool), m_bool(v) {}
    JsonValue(int v)                      : m_type(Type::Int), m_int(v) {}
    JsonValue(int64_t v)                  : m_type(Type::Int), m_int(v) {}
    JsonValue(unsigned int v)             : m_type(Type::Int), m_int(static_cast<int64_t>(v)) {}
    JsonValue(size_t v)                   : m_type(Type::Int), m_int(static_cast<int64_t>(v)) {}
    JsonValue(double v)                   : m_type(Type::Double), m_double(v) {}
    JsonValue(const char* v)              : m_type(v ? Type::String : Type::Null), m_string(v ? v : "") {}
    JsonValue(const std::string& v)       : m_type(Type::String), m_string(v) {}
    JsonValue(std::string&& v)            : m_type(Type::String), m_string(std::move(v)) {}
    JsonValue(const Array& v)             : m_type(Type::Array), m_array(v) {}
    JsonValue(Array&& v)                  : m_type(Type::Array), m_array(std::move(v)) {}
    JsonValue(const Object& v)            : m_type(Type::Object), m_object(v) {}
    JsonValue(Object&& v)                 : m_type(Type::Object), m_object(std::move(v)) {}

    // ── Type checks ──────────────────────────────────────────────────
    Type type()     const { return m_type; }
    bool isNull()   const { return m_type == Type::Null; }
    bool isBool()   const { return m_type == Type::Bool; }
    bool isInt()    const { return m_type == Type::Int; }
    bool isDouble() const { return m_type == Type::Double || m_type == Type::Int; }
    bool isString() const { return m_type == Type::String; }
    bool isArray()  const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    bool isEmpty() const {
        switch (m_type) {
            case Type::Null:   return true;
            case Type::String: return m_string.empty();
            case Type::Array:  return m_array.empty();
            case Type::Object: return m_object.empty();
            default:           return false;
        }
    }

    // ── Safe getters ─────────────────────────────────────────────────
    bool toBool(bool def = false) const {
        if (m_type == Type::Bool) return m_bool;
        if (m_type == Type::Int)  return m_int != 0;
        return def;
    }

    int toInt(int def = 0) const {
        if (m_type == Type::Int)    return static_cast<int>(m_int);
        if (m_type == Type::Double) return static_cast<int>(m_double);
        if (m_type == Type::Bool)   return m_bool ? 1 : 0;
        if (m_type == Type::String) {
            try { return std::stoi(m_string); } catch (...) {}
        }
        return def;
    }

    double toDouble(double def = 0.0) const {
        if (m_type == Type::Double) return m_double;
        if (m_type == Type::Int)    return static_cast<double>(m_int);
        if (m_type == Type::String) {
            try { return std::stod(m_string); } catch (...) {}
        }
        return def;
    }

    std::string toString(const std::string& def = "") const {
        if (m_type == Type::String) return m_string;
        if (m_type == Type::Int)    return std::to_string(m_int);
        if (m_type == Type::Double) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", m_double);
            return buf;
        }
        if (m_type == Type::Bool) return m_bool ? "true" : "false";
        return def;
    }

    Array toArray() const {
        return (m_type == Type::Array) ? m_array : Array{};
    }

    Object toObject() const {
        return (m_type == Type::Object) ? m_object : Object{};
    }

    // ── Object operations (auto-promote to Object on write) ──────────
    JsonValue& operator[](const std::string& key) {
        if (m_type != Type::Object) {
            m_type = Type::Object;
            m_object.clear();
        }
        return m_object[key];
    }

    bool contains(const std::string& key) const {
        if (m_type != Type::Object) return false;
        return m_object.find(key) != m_object.end();
    }

    JsonValue value(const std::string& key) const {
        if (m_type != Type::Object) return {};
        auto it = m_object.find(key);
        return (it != m_object.end()) ? it->second : JsonValue{};
    }

    void remove(const std::string& key) {
        if (m_type == Type::Object) {
            m_object.erase(key);
        }
    }

    std::vector<std::string> keys() const {
        std::vector<std::string> k;
        if (m_type == Type::Object) {
            k.reserve(m_object.size());
            for (const auto& [key, _] : m_object) {
                k.push_back(key);
            }
        }
        return k;
    }

    // ── Array operations (auto-promote to Array on write) ────────────
    void append(const JsonValue& v) {
        if (m_type != Type::Array) {
            m_type = Type::Array;
            m_array.clear();
        }
        m_array.push_back(v);
    }

    size_t size() const {
        if (m_type == Type::Array)  return m_array.size();
        if (m_type == Type::Object) return m_object.size();
        return 0;
    }

    const JsonValue& operator[](size_t i) const {
        static const JsonValue s_null;
        if (m_type != Type::Array || i >= m_array.size()) return s_null;
        return m_array[i];
    }

    JsonValue& operator[](size_t i) {
        if (m_type == Type::Array && i < m_array.size()) return m_array[i];
        static JsonValue s_null;
        s_null = {};
        return s_null;
    }

    // ── Array iteration ──────────────────────────────────────────────
    Array::const_iterator begin() const { return m_array.cbegin(); }
    Array::const_iterator end()   const { return m_array.cend(); }

    // ── Comparison ───────────────────────────────────────────────────
    bool operator==(const JsonValue& other) const {
        if (m_type != other.m_type) return false;
        switch (m_type) {
            case Type::Null:   return true;
            case Type::Bool:   return m_bool == other.m_bool;
            case Type::Int:    return m_int == other.m_int;
            case Type::Double: return m_double == other.m_double;
            case Type::String: return m_string == other.m_string;
            case Type::Array:  return m_array == other.m_array;
            case Type::Object: return m_object == other.m_object;
        }
        return false;
    }

    bool operator!=(const JsonValue& other) const { return !(*this == other); }

    // ── Serialization ────────────────────────────────────────────────
    std::string toJsonString() const {
        std::string out;
        serializeTo(out);
        return out;
    }

    // ── Helpers ──────────────────────────────────────────────────────
    static JsonValue fromStringList(const std::vector<std::string>& list) {
        Array arr;
        arr.reserve(list.size());
        for (const auto& s : list) {
            arr.emplace_back(s);
        }
        return JsonValue(std::move(arr));
    }

    static JsonValue makeObject() {
        JsonValue v;
        v.m_type = Type::Object;
        return v;
    }

    static JsonValue makeArray() {
        JsonValue v;
        v.m_type = Type::Array;
        return v;
    }

    // ═════════════════════════════════════════════════════════════════
    // JSON Parser (recursive descent)
    // ═════════════════════════════════════════════════════════════════
    static JsonValue parse(const std::string& json) {
        Parser p(json.c_str(), json.c_str() + json.size());
        return p.parseValue();
    }

private:
    // ── JSON serializer ──────────────────────────────────────────────
    static void escapeString(const std::string& s, std::string& out) {
        out += '"';
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        out += '"';
    }

    void serializeTo(std::string& out) const {
        switch (m_type) {
            case Type::Null:
                out += "null";
                break;
            case Type::Bool:
                out += m_bool ? "true" : "false";
                break;
            case Type::Int:
                out += std::to_string(m_int);
                break;
            case Type::Double: {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", m_double);
                out += buf;
                break;
            }
            case Type::String:
                escapeString(m_string, out);
                break;
            case Type::Array:
                out += '[';
                for (size_t i = 0; i < m_array.size(); ++i) {
                    if (i > 0) out += ',';
                    m_array[i].serializeTo(out);
                }
                out += ']';
                break;
            case Type::Object: {
                out += '{';
                bool first = true;
                for (const auto& [k, v] : m_object) {
                    if (!first) out += ',';
                    first = false;
                    escapeString(k, out);
                    out += ':';
                    v.serializeTo(out);
                }
                out += '}';
                break;
            }
        }
    }

    // ── JSON parser (recursive descent) ──────────────────────────────
    struct Parser {
        const char* pos;
        const char* end;

        Parser(const char* begin, const char* e) : pos(begin), end(e) {}

        void skipWS() {
            while (pos < end && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r'))
                ++pos;
        }

        char peek() { skipWS(); return (pos < end) ? *pos : '\0'; }
        char take() { skipWS(); return (pos < end) ? *pos++ : '\0'; }

        JsonValue parseValue() {
            skipWS();
            if (pos >= end) return {};
            char c = *pos;
            if (c == '"')                                  return parseString();
            if (c == '{')                                  return parseObject();
            if (c == '[')                                  return parseArray();
            if (c == 't')                                  return parseLiteral("true", JsonValue(true));
            if (c == 'f')                                  return parseLiteral("false", JsonValue(false));
            if (c == 'n')                                  return parseLiteral("null", JsonValue());
            if (c == '-' || (c >= '0' && c <= '9'))        return parseNumber();
            return {};
        }

        JsonValue parseString() {
            if (take() != '"') return {};
            std::string s;
            while (pos < end && *pos != '"') {
                if (*pos == '\\') {
                    ++pos;
                    if (pos >= end) break;
                    switch (*pos) {
                        case '"':  s += '"';  break;
                        case '\\': s += '\\'; break;
                        case '/':  s += '/';  break;
                        case 'b':  s += '\b'; break;
                        case 'f':  s += '\f'; break;
                        case 'n':  s += '\n'; break;
                        case 'r':  s += '\r'; break;
                        case 't':  s += '\t'; break;
                        case 'u': {
                            // Simplified: parse 4 hex digits, store as ASCII if possible
                            ++pos;
                            if (pos + 4 <= end) {
                                char hex[5] = {};
                                std::memcpy(hex, pos, 4);
                                unsigned int cp = 0;
                                if (std::sscanf(hex, "%x", &cp) == 1) {
                                    if (cp < 0x80) {
                                        s += static_cast<char>(cp);
                                    } else if (cp < 0x800) {
                                        s += static_cast<char>(0xC0 | (cp >> 6));
                                        s += static_cast<char>(0x80 | (cp & 0x3F));
                                    } else {
                                        s += static_cast<char>(0xE0 | (cp >> 12));
                                        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                        s += static_cast<char>(0x80 | (cp & 0x3F));
                                    }
                                }
                                pos += 3; // +1 done below
                            }
                            break;
                        }
                        default: s += *pos; break;
                    }
                } else {
                    s += *pos;
                }
                ++pos;
            }
            if (pos < end) ++pos; // skip closing quote
            return JsonValue(std::move(s));
        }

        JsonValue parseNumber() {
            const char* start = pos;
            bool isFloat = false;

            if (*pos == '-') ++pos;
            while (pos < end && *pos >= '0' && *pos <= '9') ++pos;
            if (pos < end && *pos == '.') { isFloat = true; ++pos; }
            while (pos < end && *pos >= '0' && *pos <= '9') ++pos;
            if (pos < end && (*pos == 'e' || *pos == 'E')) {
                isFloat = true; ++pos;
                if (pos < end && (*pos == '+' || *pos == '-')) ++pos;
                while (pos < end && *pos >= '0' && *pos <= '9') ++pos;
            }

            std::string numStr(start, pos);
            if (isFloat) {
                try { return JsonValue(std::stod(numStr)); } catch (...) {}
                return {};
            }
            try { return JsonValue(static_cast<int64_t>(std::stoll(numStr))); } catch (...) {}
            return {};
        }

        JsonValue parseObject() {
            if (take() != '{') return {};
            JsonValue obj = JsonValue::makeObject();

            if (peek() == '}') { take(); return obj; }

            while (pos < end) {
                skipWS();
                if (pos >= end || *pos != '"') break;

                JsonValue key = parseString();
                if (!key.isString()) break;

                skipWS();
                if (pos >= end || *pos != ':') break;
                ++pos; // skip ':'

                JsonValue val = parseValue();
                obj[key.toString()] = val;

                skipWS();
                if (pos < end && *pos == ',') { ++pos; continue; }
                break;
            }

            if (peek() == '}') take();
            return obj;
        }

        JsonValue parseArray() {
            if (take() != '[') return {};
            JsonValue arr = JsonValue::makeArray();

            if (peek() == ']') { take(); return arr; }

            while (pos < end) {
                arr.append(parseValue());
                skipWS();
                if (pos < end && *pos == ',') { ++pos; continue; }
                break;
            }

            if (peek() == ']') take();
            return arr;
        }

        JsonValue parseLiteral(const char* lit, JsonValue result) {
            size_t len = std::strlen(lit);
            if (static_cast<size_t>(end - pos) >= len && std::memcmp(pos, lit, len) == 0) {
                pos += len;
                return result;
            }
            return {};
        }
    };
};
