#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <type_traits>
#include <iostream>
#include <initializer_list>

namespace nlohmann {

class json {
public:
    // Internal enum with non-conflicting names
    enum value_t {
        null_t,
        object_t,
        array_t,
        string_t,
        number_t,
        boolean_t
    };

private:
    value_t m_type = null_t;
    std::map<std::string, json> m_object;
    std::vector<json> m_array;
    std::string m_string;
    double m_number = 0;
    bool m_bool = false;

public:
    json() : m_type(null_t) {}
    json(std::nullptr_t) : m_type(null_t) {}
    json(const std::string& v) : m_type(string_t), m_string(v) {}
    json(const char* v) : m_type(string_t), m_string(v) {}
    json(int v) : m_type(number_t), m_number(v) {}
    json(int64_t v) : m_type(number_t), m_number(static_cast<double>(v)) {}
    json(double v) : m_type(number_t), m_number(v) {}
    json(bool v) : m_type(boolean_t), m_bool(v) {}
    json(value_t t) : m_type(t) {}

    // Initializer list constructor for object/array
    // Ambiguous if not careful. 
    // Usually json j = {{"k", v}} type stuff.
    // For now, let's trust explicit factories mostly.
    
    // Static helpers used by code
    static json object() {
        json j;
        j.m_type = object_t;
        return j;
    }

    static json array() {
        json j;
        j.m_type = array_t;
        return j;
    }

    static json make_object() { return object(); }
    static json make_array() { return array(); }

    // Support json::object({{"k",v}, ...})

    // Operator []
    json& operator[](const std::string& key) {
        if (m_type == null_t) m_type = object_t;
        if (m_type != object_t) throw std::runtime_error("Type is not object");
        return m_object[key];
    }
    
    const json& operator[](const std::string& key) const {
        if (m_type != object_t) throw std::runtime_error("Type is not object");
        auto it = m_object.find(key);
        if (it == m_object.end()) {
             // For simplify, return nice error or throw
             throw std::runtime_error("Key not found: " + key);
        }
        return it->second;
    }
    
    // value(key, default)
    template<typename T>
    T value(const std::string& key, T default_value) const {
        if (m_type != object_t) return default_value;
        auto it = m_object.find(key);
        if (it == m_object.end()) return default_value;
        return it->second.get<T>();
    }
    
    // Specialization for returning json
    json value(const std::string& key, json default_value) const {
        if (m_type != object_t) return default_value;
        auto it = m_object.find(key);
        if (it == m_object.end()) return default_value;
        return it->second;
    }

    // Accessors
    template<typename T>
    T get() const {
        if constexpr (std::is_same_v<T, std::string>) {
            if (m_type == string_t) return m_string;
            if (m_type == number_t) return std::to_string(static_cast<long long>(m_number));
            if (m_type == boolean_t) return m_bool ? "true" : "false";
            return ""; 
        } else if constexpr (std::is_same_v<T, int>) {
            if (m_type == number_t) return static_cast<int>(m_number);
            return 0;
        } else if constexpr (std::is_same_v<T, double>) {
            if (m_type == number_t) return m_number;
            return 0.0;
        } else if constexpr (std::is_same_v<T, bool>) {
            if (m_type == boolean_t) return m_bool;
            return false;
        } else if constexpr (std::is_same_v<T, std::vector<json>>) {
            if (m_type == array_t) return m_array;
            return {};
        } else {
             // Fallback
             return T(); 
        }
    }
    
    void push_back(const json& val) {
        if (m_type == null_t) m_type = array_t;
        if (m_type != array_t) throw std::runtime_error("Type is not array");
        m_array.push_back(val);
    }
    
    // Support implicit conversion to bool for checks?
    // "if (!j.empty())"
    bool empty() const {
        if (m_type == null_t) return true;
        if (m_type == array_t) return m_array.empty();
        if (m_type == object_t) return m_object.empty();
        return false;
    }
    
    std::string dump(int indent = -1) const {
        if (m_type == string_t) return "\"" + m_string + "\"";
        if (m_type == number_t) return std::to_string(m_number);
        if (m_type == boolean_t) return m_bool ? "true" : "false";
        if (m_type == null_t) return "null";
        if (m_type == array_t) {
            std::string s = "[";
            for (size_t i = 0; i < m_array.size(); ++i) {
                if (i > 0) s += ",";
                s += m_array[i].dump();
            }
            s += "]";
            return s;
        }
        if (m_type == object_t) {
            std::string s = "{";
            bool first = true;
            for (const auto& kv : m_object) {
                if (!first) s += ",";
                s += "\"" + kv.first + "\":" + kv.second.dump();
                first = false;
            }
            s += "}";
            return s;
        }
        return "";
    }
};

} // namespace nlohmann
