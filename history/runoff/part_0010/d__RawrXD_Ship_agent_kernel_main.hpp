// agent_kernel_main.hpp - Core Types, JSON Parser, String Utilities
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <regex>

namespace RawrXD {

// Type aliases
using String = std::wstring;
template<typename T> using Vector = std::vector<T>;
template<typename K, typename V> using Map = std::map<K, V>;
template<typename T> using Optional = std::optional<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using UniquePtr = std::unique_ptr<T>;

// JSON Value type (recursive variant)
struct JsonValue;
using JsonObject = std::map<String, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue : std::variant<std::nullptr_t, bool, int64_t, double, String, JsonArray, JsonObject> {
    using variant::variant;
};

// String Utilities
class StringUtils {
public:
    static std::string ToUtf8(const String& wide) {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string utf8(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
        return utf8;
    }

    static String FromUtf8(const std::string& utf8) {
        if (utf8.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (size <= 0) return {};
        String wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), size);
        return wide;
    }

    static Vector<String> Split(const String& str, const String& delim) {
        Vector<String> result;
        size_t start = 0, end;
        while ((end = str.find(delim, start)) != String::npos) {
            result.push_back(str.substr(start, end - start));
            start = end + delim.length();
        }
        result.push_back(str.substr(start));
        return result;
    }

    static String Join(const Vector<String>& parts, const String& delim) {
        if (parts.empty()) return {};
        String result = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            result += delim + parts[i];
        }
        return result;
    }

    static String Trim(const String& str) {
        size_t start = str.find_first_not_of(L" \t\r\n");
        if (start == String::npos) return {};
        size_t end = str.find_last_not_of(L" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    static bool StartsWith(const String& str, const String& prefix) {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    }

    static bool EndsWith(const String& str, const String& suffix) {
        return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static String Replace(const String& str, const String& from, const String& to) {
        String result = str;
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != String::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        return result;
    }

    static String ToLower(const String& str) {
        String result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::towlower);
        return result;
    }

    static String ToUpper(const String& str) {
        String result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::towupper);
        return result;
    }
};

// JSON Parser
class JsonParser {
public:
    static Optional<JsonValue> Parse(const std::string& json) {
        size_t pos = 0;
        return ParseValue(json, pos);
    }

    static std::string Serialize(const JsonValue& value, int indent = 0) {
        return SerializeValue(value, indent, 0);
    }

private:
    static void SkipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    }

    static Optional<JsonValue> ParseValue(const std::string& json, size_t& pos) {
        SkipWhitespace(json, pos);
        if (pos >= json.size()) return std::nullopt;

        char c = json[pos];
        if (c == 'n') return ParseNull(json, pos);
        if (c == 't' || c == 'f') return ParseBool(json, pos);
        if (c == '"') return ParseString(json, pos);
        if (c == '[') return ParseArray(json, pos);
        if (c == '{') return ParseObject(json, pos);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber(json, pos);
        return std::nullopt;
    }

    static Optional<JsonValue> ParseNull(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "null") == 0) { pos += 4; return JsonValue(nullptr); }
        return std::nullopt;
    }

    static Optional<JsonValue> ParseBool(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "true") == 0) { pos += 4; return JsonValue(true); }
        if (json.compare(pos, 5, "false") == 0) { pos += 5; return JsonValue(false); }
        return std::nullopt;
    }

    static Optional<JsonValue> ParseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        bool isDouble = false;
        if (json[pos] == '-') ++pos;
        while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos < json.size() && json[pos] == '.') { isDouble = true; ++pos; while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos; }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) { isDouble = true; ++pos; if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos; while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos; }
        std::string numStr = json.substr(start, pos - start);
        if (isDouble) return JsonValue(std::stod(numStr));
        return JsonValue(static_cast<int64_t>(std::stoll(numStr)));
    }

    static Optional<JsonValue> ParseString(const std::string& json, size_t& pos) {
        if (json[pos] != '"') return std::nullopt;
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': if (pos + 4 < json.size()) { pos += 4; } break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        if (pos >= json.size()) return std::nullopt;
        ++pos;
        return JsonValue(StringUtils::FromUtf8(result));
    }

    static Optional<JsonValue> ParseArray(const std::string& json, size_t& pos) {
        if (json[pos] != '[') return std::nullopt;
        ++pos;
        JsonArray arr;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') { ++pos; return JsonValue(arr); }
        while (true) {
            auto val = ParseValue(json, pos);
            if (!val) return std::nullopt;
            arr.push_back(*val);
            SkipWhitespace(json, pos);
            if (pos >= json.size()) return std::nullopt;
            if (json[pos] == ']') { ++pos; return JsonValue(arr); }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
    }

    static Optional<JsonValue> ParseObject(const std::string& json, size_t& pos) {
        if (json[pos] != '{') return std::nullopt;
        ++pos;
        JsonObject obj;
        SkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') { ++pos; return JsonValue(obj); }
        while (true) {
            SkipWhitespace(json, pos);
            auto keyOpt = ParseString(json, pos);
            if (!keyOpt || !std::holds_alternative<String>(*keyOpt)) return std::nullopt;
            String key = std::get<String>(*keyOpt);
            SkipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return std::nullopt;
            ++pos;
            auto val = ParseValue(json, pos);
            if (!val) return std::nullopt;
            obj[key] = *val;
            SkipWhitespace(json, pos);
            if (pos >= json.size()) return std::nullopt;
            if (json[pos] == '}') { ++pos; return JsonValue(obj); }
            if (json[pos] != ',') return std::nullopt;
            ++pos;
        }
    }

    static std::string SerializeValue(const JsonValue& value, int indent, int depth) {
        std::string ind(depth * indent, ' ');
        std::string ind2((depth + 1) * indent, ' ');
        std::string nl = indent > 0 ? "\n" : "";
        std::string sp = indent > 0 ? " " : "";

        if (std::holds_alternative<std::nullptr_t>(value)) return "null";
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
        if (std::holds_alternative<int64_t>(value)) return std::to_string(std::get<int64_t>(value));
        if (std::holds_alternative<double>(value)) { std::ostringstream oss; oss << std::get<double>(value); return oss.str(); }
        if (std::holds_alternative<String>(value)) return "\"" + EscapeString(StringUtils::ToUtf8(std::get<String>(value))) + "\"";
        if (std::holds_alternative<JsonArray>(value)) {
            const auto& arr = std::get<JsonArray>(value);
            if (arr.empty()) return "[]";
            std::string result = "[" + nl;
            for (size_t i = 0; i < arr.size(); ++i) {
                result += ind2 + SerializeValue(arr[i], indent, depth + 1);
                if (i < arr.size() - 1) result += ",";
                result += nl;
            }
            return result + ind + "]";
        }
        if (std::holds_alternative<JsonObject>(value)) {
            const auto& obj = std::get<JsonObject>(value);
            if (obj.empty()) return "{}";
            std::string result = "{" + nl;
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                result += ind2 + "\"" + EscapeString(StringUtils::ToUtf8(k)) + "\":" + sp + SerializeValue(v, indent, depth + 1);
                if (i < obj.size() - 1) result += ",";
                result += nl;
                ++i;
            }
            return result + ind + "}";
        }
        return "null";
    }

    static std::string EscapeString(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

} // namespace RawrXD
