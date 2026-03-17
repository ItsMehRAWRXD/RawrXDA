#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <iomanip>

namespace nlohmann {

/**
 * Lightweight JSON implementation for AI Toolkit
 * Provides essential JSON serialization/deserialization without external dependencies
 */

class json {
public:
    enum value_t {
        null,
        object,
        array,
        string,
        number,
        boolean
    };

    // Constructors
    json() : m_type(null) {}
    json(std::nullptr_t) : m_type(null) {}
    json(bool b) : m_type(boolean), m_bool(b) {}
    json(int i) : m_type(number), m_number(static_cast<double>(i)) {}
    json(unsigned int i) : m_type(number), m_number(static_cast<double>(i)) {}
    json(long l) : m_type(number), m_number(static_cast<double>(l)) {}
    json(unsigned long l) : m_type(number), m_number(static_cast<double>(l)) {}
    json(long long ll) : m_type(number), m_number(static_cast<double>(ll)) {}
    json(unsigned long long ll) : m_type(number), m_number(static_cast<double>(ll)) {}
    json(double d) : m_type(number), m_number(d) {}
    json(float f) : m_type(number), m_number(static_cast<double>(f)) {}
    json(const std::string& s) : m_type(string), m_string(s) {}
    json(const char* s) : m_type(string), m_string(s ? s : "") {}
    json(const json& other) { *this = other; }
    json(json&& other) noexcept { *this = std::move(other); }

    // Object constructor
    json(const std::initializer_list<std::pair<std::string, json>>& init) : m_type(object) {
        for (const auto& [key, value] : init) {
            m_object[key] = value;
        }
    }

    // Array constructor
    json(const std::initializer_list<json>& init) : m_type(array) {
        for (const auto& item : init) {
            m_array.push_back(item);
        }
    }

    ~json() = default;

    json& operator=(const json& other) {
        if (this != &other) {
            m_type = other.m_type;
            m_string = other.m_string;
            m_number = other.m_number;
            m_bool = other.m_bool;
            m_object = other.m_object;
            m_array = other.m_array;
        }
        return *this;
    }

    json& operator=(json&& other) noexcept {
        if (this != &other) {
            m_type = other.m_type;
            m_string = std::move(other.m_string);
            m_number = other.m_number;
            m_bool = other.m_bool;
            m_object = std::move(other.m_object);
            m_array = std::move(other.m_array);
            other.m_type = null;
        }
        return *this;
    }

    // Type checking
    value_t type() const { return m_type; }
    bool is_null() const { return m_type == null; }
    bool is_object() const { return m_type == object; }
    bool is_array() const { return m_type == array; }
    bool is_string() const { return m_type == string; }
    bool is_number() const { return m_type == number; }
    bool is_boolean() const { return m_type == boolean; }

    // Object access
    json& operator[](const std::string& key) {
        if (m_type == null) m_type = object;
        if (m_type != object) throw std::runtime_error("Cannot index non-object as object");
        return m_object[key];
    }

    const json& operator[](const std::string& key) const {
        if (m_type != object) throw std::runtime_error("Cannot index non-object as object");
        auto it = m_object.find(key);
        if (it != m_object.end()) return it->second;
        static json null_val;
        return null_val;
    }

    // Array access
    json& operator[](size_t idx) {
        if (m_type == null) m_type = array;
        if (m_type != array) throw std::runtime_error("Cannot index non-array as array");
        if (idx >= m_array.size()) m_array.resize(idx + 1);
        return m_array[idx];
    }

    const json& operator[](size_t idx) const {
        if (m_type != array) throw std::runtime_error("Cannot index non-array as array");
        if (idx >= m_array.size()) throw std::out_of_range("Array index out of range");
        return m_array[idx];
    }

    // contains
    bool contains(const std::string& key) const {
        if (m_type != object) return false;
        return m_object.find(key) != m_object.end();
    }

    // value with default
    json value(const std::string& key, const json& default_val) const {
        if (m_type != object) return default_val;
        auto it = m_object.find(key);
        return (it != m_object.end()) ? it->second : default_val;
    }

    // Specializations defined outside class
    template<typename T>
    T get() const;


    // Array operations
    void push_back(const json& val) {
        if (m_type == null) m_type = array;
        if (m_type != array) throw std::runtime_error("Cannot push_back to non-array");
        m_array.push_back(val);
    }

    void push_back(json&& val) {
        if (m_type == null) m_type = array;
        if (m_type != array) throw std::runtime_error("Cannot push_back to non-array");
        m_array.push_back(std::move(val));
    }

    size_t size() const {
        if (m_type == array) return m_array.size();
        if (m_type == object) return m_object.size();
        if (m_type == string) return m_string.size();
        return 0;
    }

    bool empty() const {
        if (m_type == array) return m_array.empty();
        if (m_type == object) return m_object.empty();
        if (m_type == string) return m_string.empty();
        return m_type == null;
    }

    // Iteration for objects
    auto begin() {
        if (m_type != object) throw std::runtime_error("Cannot iterate non-object");
        return m_object.begin();
    }

    auto end() {
        if (m_type != object) throw std::runtime_error("Cannot iterate non-object");
        return m_object.end();
    }

    auto begin() const {
        if (m_type != object) throw std::runtime_error("Cannot iterate non-object");
        return m_object.begin();
    }

    auto end() const {
        if (m_type != object) throw std::runtime_error("Cannot iterate non-object");
        return m_object.end();
    }

    // Serialization
    std::string dump(int indent = -1) const {
        return dump_impl(indent, 0);
    }

    // Deserialization
    static json parse(const std::string& str) {
        size_t pos = 0;
        skip_whitespace(str, pos);
        return parse_value(str, pos);
    }

    static json object_type() {
        json j;
        j.m_type = object;
        return j;
    }

    static json array_type() {
        json j;
        j.m_type = array;
        return j;
    }

private:
    value_t m_type;
    std::string m_string;
    double m_number = 0.0;
    bool m_bool = false;
    std::map<std::string, json> m_object;
    std::vector<json> m_array;

    static void skip_whitespace(const std::string& str, size_t& pos) {
        while (pos < str.length() && std::isspace(str[pos])) pos++;
    }

    static json parse_value(const std::string& str, size_t& pos) {
        skip_whitespace(str, pos);
        if (pos >= str.length()) throw std::runtime_error("Unexpected end of input");

        if (str[pos] == '{') return parse_object(str, pos);
        if (str[pos] == '[') return parse_array(str, pos);
        if (str[pos] == '"') return parse_string(str, pos);
        if (str[pos] == 't' || str[pos] == 'f') return parse_boolean(str, pos);
        if (str[pos] == 'n') return parse_null(str, pos);
        if (std::isdigit(str[pos]) || str[pos] == '-') return parse_number(str, pos);

        throw std::runtime_error("Invalid JSON value");
    }

    static json parse_object(const std::string& str, size_t& pos) {
        json obj = json::object_type();
        pos++; // skip '{'
        skip_whitespace(str, pos);

        if (pos < str.length() && str[pos] == '}') {
            pos++;
            return obj;
        }

        while (pos < str.length()) {
            skip_whitespace(str, pos);
            if (str[pos] != '"') throw std::runtime_error("Expected string key in object");

            std::string key = parse_string(str, pos).get<std::string>();
            skip_whitespace(str, pos);
            if (pos >= str.length() || str[pos] != ':') throw std::runtime_error("Expected ':' in object");
            pos++;

            obj[key] = parse_value(str, pos);
            skip_whitespace(str, pos);

            if (pos >= str.length()) throw std::runtime_error("Unexpected end of object");
            if (str[pos] == '}') {
                pos++;
                break;
            }
            if (str[pos] != ',') throw std::runtime_error("Expected ',' or '}' in object");
            pos++;
        }
        return obj;
    }

    static json parse_array(const std::string& str, size_t& pos) {
        json arr = json::array_type();
        pos++; // skip '['
        skip_whitespace(str, pos);

        if (pos < str.length() && str[pos] == ']') {
            pos++;
            return arr;
        }

        while (pos < str.length()) {
            arr.push_back(parse_value(str, pos));
            skip_whitespace(str, pos);

            if (pos >= str.length()) throw std::runtime_error("Unexpected end of array");
            if (str[pos] == ']') {
                pos++;
                break;
            }
            if (str[pos] != ',') throw std::runtime_error("Expected ',' or ']' in array");
            pos++;
            skip_whitespace(str, pos);
        }
        return arr;
    }

    static json parse_string(const std::string& str, size_t& pos) {
        pos++; // skip opening quote
        std::string result;
        while (pos < str.length() && str[pos] != '"') {
            if (str[pos] == '\\') {
                pos++;
                if (pos >= str.length()) throw std::runtime_error("Unexpected end in string escape");
                switch (str[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += str[pos];
                }
            } else {
                result += str[pos];
            }
            pos++;
        }
        if (pos >= str.length()) throw std::runtime_error("Unterminated string");
        pos++; // skip closing quote
        return json(result);
    }

    static json parse_number(const std::string& str, size_t& pos) {
        size_t start = pos;
        if (str[pos] == '-') pos++;
        while (pos < str.length() && (std::isdigit(str[pos]) || str[pos] == '.' || str[pos] == 'e' || str[pos] == 'E' || str[pos] == '+' || str[pos] == '-')) {
            pos++;
        }
        return json(std::stod(str.substr(start, pos - start)));
    }

    static json parse_boolean(const std::string& str, size_t& pos) {
        if (str.substr(pos, 4) == "true") {
            pos += 4;
            return json(true);
        } else if (str.substr(pos, 5) == "false") {
            pos += 5;
            return json(false);
        }
        throw std::runtime_error("Invalid boolean");
    }

    static json parse_null(const std::string& str, size_t& pos) {
        if (str.substr(pos, 4) == "null") {
            pos += 4;
            return json();
        }
        throw std::runtime_error("Invalid null");
    }

    std::string dump_impl(int indent, int current_indent) const {
        std::ostringstream oss;
        std::string indent_str(current_indent, ' ');
        std::string next_indent_str(current_indent + (indent > 0 ? indent : 0), ' ');

        switch (m_type) {
            case null:
                oss << "null";
                break;
            case boolean:
                oss << (m_bool ? "true" : "false");
                break;
            case number: {
                if (std::isfinite(m_number)) {
                    long long int_part = static_cast<long long>(m_number);
                    if (m_number == int_part) {
                        oss << int_part;
                    } else {
                        oss << std::fixed << std::setprecision(10) << m_number;
                        std::string num_str = oss.str();
                        // Remove trailing zeros
                        while (num_str.back() == '0') num_str.pop_back();
                        if (num_str.back() == '.') num_str.pop_back();
                        return num_str;
                    }
                } else {
                    oss << "null";
                }
                break;
            }
            case string:
                oss << '"';
                for (char c : m_string) {
                    switch (c) {
                        case '"': oss << "\\\""; break;
                        case '\\': oss << "\\\\"; break;
                        case '\b': oss << "\\b"; break;
                        case '\f': oss << "\\f"; break;
                        case '\n': oss << "\\n"; break;
                        case '\r': oss << "\\r"; break;
                        case '\t': oss << "\\t"; break;
                        default:
                            if (c < 32) {
                                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                            } else {
                                oss << c;
                            }
                    }
                }
                oss << '"';
                break;
            case array:
                oss << "[";
                if (!m_array.empty() && indent >= 0) oss << "\n";
                for (size_t i = 0; i < m_array.size(); i++) {
                    if (indent >= 0) oss << next_indent_str;
                    oss << m_array[i].dump_impl(indent, current_indent + (indent > 0 ? indent : 0));
                    if (i < m_array.size() - 1) oss << ",";
                    if (indent >= 0) oss << "\n";
                }
                if (!m_array.empty() && indent >= 0) oss << indent_str;
                oss << "]";
                break;
            case object:
                oss << "{";
                if (!m_object.empty() && indent >= 0) oss << "\n";
                bool first = true;
                for (const auto& [key, value] : m_object) {
                    if (!first && indent < 0) oss << ",";
                    if (indent >= 0 && !first) oss << ",\n";
                    if (indent >= 0) oss << next_indent_str;
                    oss << "\"" << key << "\":";
                    if (indent >= 0) oss << " ";
                    oss << value.dump_impl(indent, current_indent + (indent > 0 ? indent : 0));
                    first = false;
                }
                if (!m_object.empty() && indent >= 0) oss << "\n" << indent_str;
                oss << "}";
                break;
        }
        return oss.str();
    }
};

// Specializations
template<>
inline std::string json::get<std::string>() const {
    if (m_type == string) return m_string;
    if (m_type == number) return std::to_string(static_cast<long long>(m_number));
    if (m_type == boolean) return m_bool ? "true" : "false";
    if (m_type == null) return "null";
    throw std::runtime_error("Cannot get string from this JSON type");
}

template<>
inline int json::get<int>() const {
    if (m_type == number) return static_cast<int>(m_number);
    throw std::runtime_error("Cannot get int from this JSON type");
}

template<>
inline double json::get<double>() const {
    if (m_type == number) return m_number;
    throw std::runtime_error("Cannot get double from this JSON type");
}

template<>
inline bool json::get<bool>() const {
    if (m_type == boolean) return m_bool;
    throw std::runtime_error("Cannot get bool from this JSON type");
}

template<>
inline std::vector<json> json::get<std::vector<json>>() const {
    if (m_type == array) return m_array;
    throw std::runtime_error("Cannot get array from this JSON type");
}

} // namespace nlohmann
