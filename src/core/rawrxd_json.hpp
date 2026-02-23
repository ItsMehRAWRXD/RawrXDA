// rawrxd_json.hpp — In-house minimal JSON (from-scratch, no nlohmann)
// Parse and serialize JSON for new code. STL only.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sstream>

namespace RawrXD {

class JsonValue;

using JsonObject = std::map<std::string, JsonValue>;
using JsonArray  = std::vector<JsonValue>;

enum class JsonType { Null, Bool, Number, String, Array, Object };

class JsonValue {
public:
    JsonValue() : m_type(JsonType::Null) {}
    explicit JsonValue(bool b) : m_type(JsonType::Bool), m_bool(b) {}
    explicit JsonValue(double n) : m_type(JsonType::Number), m_number(n) {}
    explicit JsonValue(const std::string& s) : m_type(JsonType::String), m_string(s) {}
    explicit JsonValue(std::string&& s) : m_type(JsonType::String), m_string(std::move(s)) {}
    explicit JsonValue(const char* s) : m_type(JsonType::String), m_string(s ? s : "") {}
    explicit JsonValue(const JsonArray& a) : m_type(JsonType::Array), m_array(a) {}
    explicit JsonValue(JsonArray&& a) : m_type(JsonType::Array), m_array(std::move(a)) {}
    explicit JsonValue(const JsonObject& o) : m_type(JsonType::Object), m_object(o) {}
    explicit JsonValue(JsonObject&& o) : m_type(JsonType::Object), m_object(std::move(o)) {}

    JsonType type() const { return m_type; }
    bool is_null()   const { return m_type == JsonType::Null; }
    bool is_bool()   const { return m_type == JsonType::Bool; }
    bool is_number() const { return m_type == JsonType::Number; }
    bool is_string() const { return m_type == JsonType::String; }
    bool is_array()  const { return m_type == JsonType::Array; }
    bool is_object() const { return m_type == JsonType::Object; }

    bool        get_bool()   const { if (m_type != JsonType::Bool)   throw std::runtime_error("JsonValue: not bool");   return m_bool; }
    double      get_number() const { if (m_type != JsonType::Number) throw std::runtime_error("JsonValue: not number"); return m_number; }
    std::string get_string() const { if (m_type != JsonType::String) throw std::runtime_error("JsonValue: not string"); return m_string; }
    JsonArray&  get_array()  { if (m_type != JsonType::Array)  throw std::runtime_error("JsonValue: not array");  return m_array; }
    const JsonArray&  get_array()  const { if (m_type != JsonType::Array)  throw std::runtime_error("JsonValue: not array");  return m_array; }
    JsonObject& get_object() { if (m_type != JsonType::Object) throw std::runtime_error("JsonValue: not object"); return m_object; }
    const JsonObject& get_object() const { if (m_type != JsonType::Object) throw std::runtime_error("JsonValue: not object"); return m_object; }

    template<typename T> T get() const;
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    JsonValue& operator[](size_t i);
    const JsonValue& operator[](size_t i) const;

    bool contains(const std::string& key) const { return is_object() && m_object.find(key) != m_object.end(); }
    JsonValue value(const std::string& key, const JsonValue& defaultVal) const;

    std::string dump(bool pretty = false, int indent = 0) const;

    static JsonValue parse(const std::string& str);
    static JsonValue parse(const char* str, size_t len);

private:
    JsonType m_type = JsonType::Null;
    bool m_bool = false;
    double m_number = 0;
    std::string m_string;
    JsonArray m_array;
    JsonObject m_object;

    static size_t skipSpace(const char* s, size_t i, size_t len);
    static size_t parseValue(const char* s, size_t len, size_t i, JsonValue& out);
    static size_t parseString(const char* s, size_t len, size_t i, std::string& out);
    static std::string escape(const std::string& s);
};

template<> inline bool        JsonValue::get<bool>()        const { return get_bool(); }
template<> inline double      JsonValue::get<double>()     const { return get_number(); }
template<> inline std::string JsonValue::get<std::string>() const { return get_string(); }

inline JsonValue& JsonValue::operator[](const std::string& key) {
    if (m_type != JsonType::Object) throw std::runtime_error("JsonValue: not object");
    return m_object[key];
}
inline const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (m_type != JsonType::Object) throw std::runtime_error("JsonValue: not object");
    auto it = m_object.find(key);
    if (it == m_object.end()) throw std::runtime_error("JsonValue: key not found");
    return it->second;
}
inline JsonValue& JsonValue::operator[](size_t i) {
    if (m_type != JsonType::Array) throw std::runtime_error("JsonValue: not array");
    if (i >= m_array.size()) throw std::runtime_error("JsonValue: index out of range");
    return m_array[i];
}
inline const JsonValue& JsonValue::operator[](size_t i) const {
    if (m_type != JsonType::Array) throw std::runtime_error("JsonValue: not array");
    if (i >= m_array.size()) throw std::runtime_error("JsonValue: index out of range");
    return m_array[i];
}
inline JsonValue JsonValue::value(const std::string& key, const JsonValue& defaultVal) const {
    if (m_type != JsonType::Object) return defaultVal;
    auto it = m_object.find(key);
    if (it == m_object.end()) return defaultVal;
    return it->second;
}

inline std::string JsonValue::escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

inline std::string JsonValue::dump(bool pretty, int indent) const {
    std::string pad;
    if (pretty) pad = std::string(static_cast<size_t>(indent), ' ');
    std::string pad2 = pretty ? pad + "  " : pad;
    switch (m_type) {
        case JsonType::Null:   return "null";
        case JsonType::Bool:   return m_bool ? "true" : "false";
        case JsonType::Number: {
            std::ostringstream o;
            if (m_number == static_cast<double>(static_cast<int64_t>(m_number)))
                o << static_cast<int64_t>(m_number);
            else
                o << m_number;
            return o.str();
        }
        case JsonType::String:  return "\"" + escape(m_string) + "\"";
        case JsonType::Array: {
            std::string r = "[";
            for (size_t i = 0; i < m_array.size(); i++) {
                if (i) r += ",";
                if (pretty) r += "\n" + pad2;
                r += m_array[i].dump(pretty, indent + 2);
            }
            if (pretty && !m_array.empty()) r += "\n" + pad;
            r += "]";
            return r;
        }
        case JsonType::Object: {
            std::string r = "{";
            size_t idx = 0;
            for (const auto& kv : m_object) {
                if (idx++) r += ",";
                if (pretty) r += "\n" + pad2;
                r += "\"" + escape(kv.first) + "\":" + (pretty ? " " : "") + kv.second.dump(pretty, indent + 2);
            }
            if (pretty && !m_object.empty()) r += "\n" + pad;
            r += "}";
            return r;
        }
    }
    return "null";
}

inline size_t JsonValue::skipSpace(const char* s, size_t i, size_t len) {
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    return i;
}

inline size_t JsonValue::parseString(const char* s, size_t len, size_t i, std::string& out) {
    if (i >= len || s[i] != '"') return i;
    i++;
    out.clear();
    while (i < len) {
        if (s[i] == '"') { i++; return i; }
        if (s[i] == '\\') {
            i++;
            if (i >= len) break;
            if (s[i] == 'n') { out += '\n'; i++; continue; }
            if (s[i] == 'r') { out += '\r'; i++; continue; }
            if (s[i] == 't') { out += '\t'; i++; continue; }
            if (s[i] == '"' || s[i] == '\\') { out += s[i]; i++; continue; }
        }
        out += s[i++];
    }
    return i;
}

inline size_t JsonValue::parseValue(const char* s, size_t len, size_t i, JsonValue& out) {
    i = skipSpace(s, i, len);
    if (i >= len) return i;
    if (s[i] == 'n' && i + 4 <= len && s[i+1]=='u' && s[i+2]=='l' && s[i+3]=='l') { out = JsonValue(); return i + 4; }
    if (s[i] == 't' && i + 4 <= len && s[i+1]=='r' && s[i+2]=='u' && s[i+3]=='e')  { out = JsonValue(true);  return i + 4; }
    if (s[i] == 'f' && i + 5 <= len && s[i+1]=='a' && s[i+2]=='l' && s[i+3]=='s' && s[i+4]=='e') { out = JsonValue(false); return i + 5; }
    if (s[i] == '"') {
        std::string str;
        i = parseString(s, len, i, str);
        out = JsonValue(std::move(str));
        return i;
    }
    if (s[i] == '[') {
        JsonArray arr;
        i++;
        i = skipSpace(s, i, len);
        while (i < len && s[i] != ']') {
            JsonValue v;
            i = parseValue(s, len, i, v);
            arr.push_back(std::move(v));
            i = skipSpace(s, i, len);
            if (i < len && s[i] == ',') { i++; i = skipSpace(s, i, len); }
        }
        if (i < len && s[i] == ']') i++;
        out = JsonValue(std::move(arr));
        return i;
    }
    if (s[i] == '{') {
        JsonObject obj;
        i++;
        i = skipSpace(s, i, len);
        while (i < len && s[i] != '}') {
            std::string key;
            if (s[i] != '"') return i;
            i = parseString(s, len, i, key);
            i = skipSpace(s, i, len);
            if (i >= len || s[i] != ':') return i;
            i++;
            i = skipSpace(s, i, len);
            JsonValue val;
            i = parseValue(s, len, i, val);
            obj[std::move(key)] = std::move(val);
            i = skipSpace(s, i, len);
            if (i < len && s[i] == ',') { i++; i = skipSpace(s, i, len); }
        }
        if (i < len && s[i] == '}') i++;
        out = JsonValue(std::move(obj));
        return i;
    }
    // number
    size_t start = i;
    if (s[i] == '-') i++;
    while (i < len && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+')) i++;
    if (i > start) {
        std::string numStr(s + start, s + i);
        double n = std::strtod(numStr.c_str(), nullptr);
        out = JsonValue(n);
        return i;
    }
    return i;
}

inline JsonValue JsonValue::parse(const std::string& str) {
    JsonValue out;
    size_t i = parseValue(str.data(), str.size(), 0, out);
    (void)i;
    return out;
}
inline JsonValue JsonValue::parse(const char* str, size_t len) {
    JsonValue out;
    parseValue(str, len, 0, out);
    return out;
}

} // namespace RawrXD
