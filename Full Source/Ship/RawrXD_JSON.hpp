// RawrXD_JSON.hpp - High-Performance JSON Parser/Emitter
// Pure C++20 - No Qt Dependencies
// Zero external libraries

#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace RawrXD {

class JSONValue;
using JSONObject = std::map<std::string, JSONValue>;
using JSONArray = std::vector<JSONValue>;

class JSONValue {
public:
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JSONArray, JSONObject>;

private:
    Variant m_value;

public:
    JSONValue() : m_value(nullptr) {}
    JSONValue(std::nullptr_t) : m_value(nullptr) {}
    JSONValue(bool b) : m_value(b) {}
    JSONValue(int i) : m_value(static_cast<double>(i)) {}
    JSONValue(int64_t i) : m_value(static_cast<double>(i)) {}
    JSONValue(double d) : m_value(d) {}
    JSONValue(const char* s) : m_value(std::string(s)) {}
    JSONValue(const std::string& s) : m_value(s) {}
    JSONValue(std::string&& s) : m_value(std::move(s)) {}
    JSONValue(const JSONArray& a) : m_value(a) {}
    JSONValue(JSONArray&& a) : m_value(std::move(a)) {}
    JSONValue(const JSONObject& o) : m_value(o) {}
    JSONValue(JSONObject&& o) : m_value(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }
    bool isBool() const { return std::holds_alternative<bool>(m_value); }
    bool isNumber() const { return std::holds_alternative<double>(m_value); }
    bool isString() const { return std::holds_alternative<std::string>(m_value); }
    bool isArray() const { return std::holds_alternative<JSONArray>(m_value); }
    bool isObject() const { return std::holds_alternative<JSONObject>(m_value); }

    bool asBool() const { return std::get<bool>(m_value); }
    double asNumber() const { return std::get<double>(m_value); }
    const std::string& asString() const { return std::get<std::string>(m_value); }
    const JSONArray& asArray() const { return std::get<JSONArray>(m_value); }
    const JSONObject& asObject() const { return std::get<JSONObject>(m_value); }

    JSONValue& operator[](const std::string& key) {
        if (isNull()) m_value = JSONObject();
        return std::get<JSONObject>(m_value)[key];
    }

    const JSONValue& at(const std::string& key) const {
        return std::get<JSONObject>(m_value).at(key);
    }

    bool has(const std::string& key) const {
        if (!isObject()) return false;
        return asObject().find(key) != asObject().end();
    }

    size_t size() const {
        if (isArray()) return asArray().size();
        if (isObject()) return asObject().size();
        return 0;
    }

    std::string stringify(bool pretty = false, int indent = 0) const {
        std::ostringstream ss;
        if (std::holds_alternative<std::nullptr_t>(m_value)) {
            ss << "null";
        } else if (std::holds_alternative<bool>(m_value)) {
            ss << (std::get<bool>(m_value) ? "true" : "false");
        } else if (std::holds_alternative<double>(m_value)) {
            ss << std::get<double>(m_value);
        } else if (std::holds_alternative<std::string>(m_value)) {
            ss << "\"" << escapeString(std::get<std::string>(m_value)) << "\"";
        } else if (std::holds_alternative<JSONArray>(m_value)) {
            const auto& arr = std::get<JSONArray>(m_value);
            ss << "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (pretty) ss << "\n" << std::string(indent + 2, ' ');
                ss << arr[i].stringify(pretty, indent + 2);
                if (i < arr.size() - 1) ss << ",";
            }
            if (pretty && !arr.empty()) ss << "\n" << std::string(indent, ' ');
            ss << "]";
        } else if (std::holds_alternative<JSONObject>(m_value)) {
            const auto& obj = std::get<JSONObject>(m_value);
            ss << "{";
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) ss << ",";
                first = false;
                if (pretty) ss << "\n" << std::string(indent + 2, ' ');
                ss << "\"" << escapeString(k) << "\":" << (pretty ? " " : "") << v.stringify(pretty, indent + 2);
            }
            if (pretty && !obj.empty()) ss << "\n" << std::string(indent, ' ');
            ss << "}";
        }
        return ss.str();
    }

private:
    std::string escapeString(const std::string& s) const {
        std::ostringstream ss;
        for (char c : s) {
            switch (c) {
                case '\"': ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\b': ss << "\\b"; break;
                case '\f': ss << "\\f"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default: 
                    if ('\x00' <= c && c <= '\x1f') {
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                    } else {
                        ss << c;
                    }
                    break;
            }
        }
        return ss.str();
    }
};

class JSONParser {
public:
    static JSONValue Parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.length() && isspace(json[pos])) pos++;
    }

    static JSONValue parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.length()) return JSONValue();

        char c = json[pos];
        if (c == '{') return parseObject(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '"') return parseString(json, pos);
        if (c == 't' || c == 'f') return parseBool(json, pos);
        if (c == 'n') return parseNull(json, pos);
        if (isdigit(c) || c == '-') return parseNumber(json, pos);

        return JSONValue();
    }

    static JSONValue parseObject(const std::string& json, size_t& pos) {
        JSONObject obj;
        pos++; // skip {
        while (true) {
            skipWhitespace(json, pos);
            if (pos >= json.length() || json[pos] == '}') {
                if (pos < json.length()) pos++;
                break;
            }
            std::string key = parseString(json, pos).asString();
            skipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ':') pos++;
            obj[key] = parseValue(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') pos++;
        }
        return JSONValue(obj);
    }

    static JSONValue parseArray(const std::string& json, size_t& pos) {
        JSONArray arr;
        pos++; // skip [
        while (true) {
            skipWhitespace(json, pos);
            if (pos >= json.length() || json[pos] == ']') {
                if (pos < json.length()) pos++;
                break;
            }
            arr.push_back(parseValue(json, pos));
            skipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') pos++;
        }
        return JSONValue(arr);
    }

    static JSONValue parseString(const std::string& json, size_t& pos) {
        std::string s;
        pos++; // skip "
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.length()) break;
                char c = json[pos];
                switch (c) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': // Handle unicode escape
                        pos += 4; // Skip for now
                        break;
                    default: s += c; break;
                }
            } else {
                s += json[pos];
            }
            pos++;
        }
        if (pos < json.length()) pos++; // skip "
        return JSONValue(s);
    }

    static JSONValue parseBool(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JSONValue(true);
        } else {
            pos += 5;
            return JSONValue(false);
        }
    }

    static JSONValue parseNull(const std::string& json, size_t& pos) {
        pos += 4;
        return JSONValue(nullptr);
    }

    static JSONValue parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+')) {
            pos++;
        }
        return JSONValue(std::stod(json.substr(start, pos - start)));
    }
};

} // namespace RawrXD
