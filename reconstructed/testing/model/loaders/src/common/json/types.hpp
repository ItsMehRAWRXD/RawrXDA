// ============================================================================
// File: src/common/json_types.hpp
// Purpose: Lightweight JSON types replacing QJsonObject/QJsonArray/QJsonDocument
// No Qt dependency - pure C++17
// ============================================================================
#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>

/**
 * Forward declaration for recursive variant
 */
struct JsonValue;

using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

/**
 * @brief Lightweight JSON value type replacing QJsonValue
 */
struct JsonValue {
    enum Type { Null, Bool, Int, Double, String, Array, Object };

    using ValueType = std::variant<
        std::nullptr_t,
        bool,
        int,
        double,
        std::string,
        JsonArray,
        JsonObject
    >;

    ValueType data;

    JsonValue() : data(nullptr) {}
    JsonValue(std::nullptr_t) : data(nullptr) {}
    JsonValue(bool v) : data(v) {}
    JsonValue(int v) : data(v) {}
    JsonValue(double v) : data(v) {}
    JsonValue(const char* v) : data(std::string(v)) {}
    JsonValue(const std::string& v) : data(v) {}
    JsonValue(std::string&& v) : data(std::move(v)) {}
    JsonValue(const JsonArray& v) : data(v) {}
    JsonValue(JsonArray&& v) : data(std::move(v)) {}
    JsonValue(const JsonObject& v) : data(v) {}
    JsonValue(JsonObject&& v) : data(std::move(v)) {}

    Type type() const {
        if (std::holds_alternative<std::nullptr_t>(data)) return Null;
        if (std::holds_alternative<bool>(data)) return Bool;
        if (std::holds_alternative<int>(data)) return Int;
        if (std::holds_alternative<double>(data)) return Double;
        if (std::holds_alternative<std::string>(data)) return String;
        if (std::holds_alternative<JsonArray>(data)) return Array;
        if (std::holds_alternative<JsonObject>(data)) return Object;
        return Null;
    }

    bool isNull() const { return type() == Null; }
    bool isBool() const { return type() == Bool; }
    bool isInt() const { return type() == Int; }
    bool isDouble() const { return type() == Double || type() == Int; }
    bool isString() const { return type() == String; }
    bool isArray() const { return type() == Array; }
    bool isObject() const { return type() == Object; }

    bool toBool(bool def = false) const {
        if (auto* v = std::get_if<bool>(&data)) return *v;
        return def;
    }
    int toInt(int def = 0) const {
        if (auto* v = std::get_if<int>(&data)) return *v;
        if (auto* v = std::get_if<double>(&data)) return static_cast<int>(*v);
        return def;
    }
    double toDouble(double def = 0.0) const {
        if (auto* v = std::get_if<double>(&data)) return *v;
        if (auto* v = std::get_if<int>(&data)) return static_cast<double>(*v);
        return def;
    }
    std::string toString(const std::string& def = "") const {
        if (auto* v = std::get_if<std::string>(&data)) return *v;
        if (auto* v = std::get_if<int>(&data)) return std::to_string(*v);
        if (auto* v = std::get_if<double>(&data)) return std::to_string(*v);
        if (auto* v = std::get_if<bool>(&data)) return *v ? "true" : "false";
        return def;
    }
    const JsonArray& toArray() const {
        static const JsonArray empty;
        if (auto* v = std::get_if<JsonArray>(&data)) return *v;
        return empty;
    }
    JsonArray& toArray() {
        if (auto* v = std::get_if<JsonArray>(&data)) return *v;
        data = JsonArray{};
        return std::get<JsonArray>(data);
    }
    const JsonObject& toObject() const {
        static const JsonObject empty;
        if (auto* v = std::get_if<JsonObject>(&data)) return *v;
        return empty;
    }
    JsonObject& toObject() {
        if (auto* v = std::get_if<JsonObject>(&data)) return *v;
        data = JsonObject{};
        return std::get<JsonObject>(data);
    }

    bool operator==(const JsonValue& other) const { return data == other.data; }
    bool operator!=(const JsonValue& other) const { return data != other.data; }

    // Object-like access
    JsonValue& operator[](const std::string& key) {
        if (!isObject()) data = JsonObject{};
        return std::get<JsonObject>(data)[key];
    }
    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_val;
        if (auto* obj = std::get_if<JsonObject>(&data)) {
            auto it = obj->find(key);
            if (it != obj->end()) return it->second;
        }
        return null_val;
    }

    bool contains(const std::string& key) const {
        if (auto* obj = std::get_if<JsonObject>(&data)) {
            return obj->find(key) != obj->end();
        }
        return false;
    }

    void remove(const std::string& key) {
        if (auto* obj = std::get_if<JsonObject>(&data)) {
            obj->erase(key);
        }
    }

    int size() const {
        if (auto* arr = std::get_if<JsonArray>(&data)) return static_cast<int>(arr->size());
        if (auto* obj = std::get_if<JsonObject>(&data)) return static_cast<int>(obj->size());
        return 0;
    }

    void append(const JsonValue& val) {
        if (!isArray()) data = JsonArray{};
        std::get<JsonArray>(data).push_back(val);
    }
};

/**
 * @brief Simple JSON serializer (replacing QJsonDocument)
 */
namespace JsonSerializer {

    inline std::string escapeString(const std::string& s) {
        std::string result;
        result.reserve(s.size() + 10);
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }

    inline std::string serialize(const JsonValue& val) {
        switch (val.type()) {
            case JsonValue::Null: return "null";
            case JsonValue::Bool: return val.toBool() ? "true" : "false";
            case JsonValue::Int: return std::to_string(val.toInt());
            case JsonValue::Double: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.6g", val.toDouble());
                return buf;
            }
            case JsonValue::String:
                return "\"" + escapeString(val.toString()) + "\"";
            case JsonValue::Array: {
                std::string r = "[";
                const auto& arr = val.toArray();
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) r += ",";
                    r += serialize(arr[i]);
                }
                r += "]";
                return r;
            }
            case JsonValue::Object: {
                std::string r = "{";
                const auto& obj = val.toObject();
                bool first = true;
                for (const auto& [k, v] : obj) {
                    if (!first) r += ",";
                    r += "\"" + escapeString(k) + "\":" + serialize(v);
                    first = false;
                }
                r += "}";
                return r;
            }
        }
        return "null";
    }

    // Simple JSON parser (handles basic JSON)
    inline JsonValue parse(const std::string& json, size_t& pos);

    inline void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
               json[pos] == '\n' || json[pos] == '\r')) {
            ++pos;
        }
    }

    inline std::string parseString(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '"') return "";
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        if (pos < json.size()) ++pos; // skip closing quote
        return result;
    }

    inline JsonValue parse(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) return JsonValue();

        char c = json[pos];
        if (c == '"') {
            return JsonValue(parseString(json, pos));
        }
        if (c == '{') {
            ++pos;
            JsonObject obj;
            skipWhitespace(json, pos);
            while (pos < json.size() && json[pos] != '}') {
                skipWhitespace(json, pos);
                std::string key = parseString(json, pos);
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ':') ++pos;
                obj[key] = parse(json, pos);
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
            if (pos < json.size()) ++pos;
            return JsonValue(std::move(obj));
        }
        if (c == '[') {
            ++pos;
            JsonArray arr;
            skipWhitespace(json, pos);
            while (pos < json.size() && json[pos] != ']') {
                arr.push_back(parse(json, pos));
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
            if (pos < json.size()) ++pos;
            return JsonValue(std::move(arr));
        }
        if (c == 't' && json.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue(true);
        }
        if (c == 'f' && json.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue(false);
        }
        if (c == 'n' && json.substr(pos, 4) == "null") {
            pos += 4;
            return JsonValue();
        }
        // Number
        size_t start = pos;
        bool isDouble = false;
        if (json[pos] == '-') ++pos;
        while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
        if (pos < json.size() && json[pos] == '.') { isDouble = true; ++pos; }
        while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) { isDouble = true; ++pos; if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos; while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos; }
        std::string numStr = json.substr(start, pos - start);
        if (numStr.empty()) return JsonValue();
        if (isDouble) return JsonValue(std::stod(numStr));
        return JsonValue(std::stoi(numStr));
    }

    inline JsonValue parse(const std::string& json) {
        size_t pos = 0;
        return parse(json, pos);
    }

    inline JsonValue fromUtf8(const std::string& utf8) {
        return parse(utf8);
    }
}
