/**
 * @file json_types.hpp
 * @brief Minimal JSON types for Qt-free operation
 *
 * Replaces QJsonObject, QJsonArray, QJsonDocument, QJsonValue
 * with STL-based types. Manual serializer per project spec.
 */
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstdint>
#include <variant>
#include <stdexcept>

/**
 * @brief Simple JSON value - wraps a std::variant of common types
 */
struct JsonValue {
    using Null = std::monostate;
    using Object = std::unordered_map<std::string, JsonValue>;
    using Array  = std::vector<JsonValue>;

    std::variant<Null, bool, int64_t, double, std::string, Object, Array> data;

    JsonValue() : data(Null{}) {}
    JsonValue(bool v) : data(v) {}
    JsonValue(int v) : data(static_cast<int64_t>(v)) {}
    JsonValue(int64_t v) : data(v) {}
    JsonValue(double v) : data(v) {}
    JsonValue(const char* v) : data(std::string(v)) {}
    JsonValue(const std::string& v) : data(v) {}
    JsonValue(std::string&& v) : data(std::move(v)) {}
    JsonValue(const Object& v) : data(v) {}
    JsonValue(Object&& v) : data(std::move(v)) {}
    JsonValue(const Array& v) : data(v) {}
    JsonValue(Array&& v) : data(std::move(v)) {}

    bool isNull()    const { return std::holds_alternative<Null>(data); }
    bool isBool()    const { return std::holds_alternative<bool>(data); }
    bool isInt()     const { return std::holds_alternative<int64_t>(data); }
    bool isDouble()  const { return std::holds_alternative<double>(data); }
    bool isString()  const { return std::holds_alternative<std::string>(data); }
    bool isObject()  const { return std::holds_alternative<Object>(data); }
    bool isArray()   const { return std::holds_alternative<Array>(data); }

    bool        toBool(bool def = false)          const { return isBool()   ? std::get<bool>(data)        : def; }
    int64_t     toInt(int64_t def = 0)            const { return isInt()    ? std::get<int64_t>(data)     : def; }
    double      toDouble(double def = 0.0)        const { return isDouble() ? std::get<double>(data)      : def; }
    std::string toString(const std::string& def = "") const { return isString() ? std::get<std::string>(data) : def; }

    const Object& toObject() const { return std::get<Object>(data); }
    Object&       toObject()       { return std::get<Object>(data); }
    const Array&  toArray()  const { return std::get<Array>(data); }
    Array&        toArray()        { return std::get<Array>(data); }
};

/**
 * @brief JSON object type (string → JsonValue)
 */
using JsonObject = std::unordered_map<std::string, JsonValue>;

/**
 * @brief JSON array type (vector of JsonValue)
 */
using JsonArray = std::vector<JsonValue>;

/**
 * @brief Minimal JSON document: parse / serialize
 */
struct JsonDoc {
    JsonValue root;

    JsonDoc() = default;
    explicit JsonDoc(const JsonObject& obj) : root(obj) {}
    explicit JsonDoc(const JsonArray& arr)  : root(arr) {}
    explicit JsonDoc(JsonValue val) : root(std::move(val)) {}

    /**
     * @brief Serialize JsonValue to compact JSON string
     */
    static std::string toJson(const JsonValue& v) {
        std::ostringstream out;
        serialize(out, v);
        return out.str();
    }
    static std::string toJson(const JsonObject& obj) { return toJson(JsonValue(obj)); }
    static std::string toJson(const JsonArray& arr)  { return toJson(JsonValue(arr)); }

    std::string toJson() const { return toJson(root); }

private:
    static void serialize(std::ostringstream& out, const JsonValue& v) {
        if (v.isNull()) { out << "null"; return; }
        if (v.isBool()) { out << (v.toBool() ? "true" : "false"); return; }
        if (v.isInt())  { out << v.toInt(); return; }
        if (v.isDouble()) { out << v.toDouble(); return; }
        if (v.isString()) {
            out << '"';
            for (char c : v.toString()) {
                switch (c) {
                    case '"':  out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n";  break;
                    case '\r': out << "\\r";  break;
                    case '\t': out << "\\t";  break;
                    default:   out << c;      break;
                }
            }
            out << '"';
            return;
        }
        if (v.isObject()) {
            out << '{';
            bool first = true;
            for (const auto& [k, val] : v.toObject()) {
                if (!first) out << ',';
                first = false;
                out << '"' << k << "\":";
                serialize(out, val);
            }
            out << '}';
            return;
        }
        if (v.isArray()) {
            out << '[';
            bool first = true;
            for (const auto& elem : v.toArray()) {
                if (!first) out << ',';
                first = false;
                serialize(out, elem);
            }
            out << ']';
            return;
        }
    }
};
