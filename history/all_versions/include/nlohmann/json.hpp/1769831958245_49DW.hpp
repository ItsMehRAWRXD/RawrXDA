#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nlohmann {

struct json {
    enum value_t { null_t, object_t, array_t, string_t, number_t, boolean_t };
    value_t m_type = null_t;
    std::map<std::string, json> m_object;
    std::vector<json> m_array;
    std::string m_string;
    double m_number = 0;
    bool m_bool = false;

    // CTORS
    json() = default;
    json(std::nullptr_t) : m_type(null_t) {}
    json(bool v) : m_type(boolean_t), m_bool(v) {}
    json(int v) : m_type(number_t), m_number(v) {}
    json(unsigned int v) : m_type(number_t), m_number(v) {}
    json(long v) : m_type(number_t), m_number(v) {}
    json(unsigned long v) : m_type(number_t), m_number(v) {}
    json(long long v) : m_type(number_t), m_number(static_cast<double>(v)) {}
    json(unsigned long long v) : m_type(number_t), m_number(static_cast<double>(v)) {}
    json(double v) : m_type(number_t), m_number(v) {}
    json(const std::string& v) : m_type(string_t), m_string(v) {}
    json(const char* v) : m_type(string_t), m_string(v) {}

    // Methods
    static json object() { json j; j.m_type = object_t; return j; }
    static json array() { json j; j.m_type = array_t; return j; }
    static json make_object() { return object(); }
    static json make_array() { return array(); }
    static json parse(const std::string& s) { 
        if (s.find('[') != std::string::npos) return array();
        return object(); 
    }

    bool is_null() const { return m_type == null_t; }
    bool is_object() const { return m_type == object_t; }
    bool is_array() const { return m_type == array_t; }
    bool is_string() const { return m_type == string_t; }
    bool is_number() const { return m_type == number_t; }
    bool is_boolean() const { return m_type == boolean_t; }

    bool empty() const {
        if (m_type == null_t) return true;
        if (m_type == array_t) return m_array.empty();
        if (m_type == object_t) return m_object.empty();
        return false;
    }

    // Check if key exists (object)
    size_t count(const std::string& key) const {
        if (m_type != object_t) return 0;
        return m_object.count(key);
    }
    
    // Check if key exists (object) - C++20 style but nlohmann has it
    bool contains(const std::string& key) const {
        if (m_type != object_t) return false;
        return m_object.find(key) != m_object.end();
    }

    // Helper to allow dump()
    std::string dump(int indent = -1) const {
        if (m_type == string_t) return "\"" + m_string + "\"";
        if (m_type == number_t) return std::to_string(m_number);
        if (m_type == boolean_t) return m_bool ? "true" : "false";
        if (m_type == null_t) return "null";
        if (m_type == object_t) return "{}";
        if (m_type == array_t) return "[]";
        return "";
    }

    // Accessors
    json& operator[](const std::string& key) {
        if (m_type == null_t) m_type = object_t;
        return m_object[key];
    }
    const json& operator[](const std::string& key) const {
        static json empty;
        auto it = m_object.find(key);
        if (it != m_object.end()) return it->second;
        return empty;
    }
    // const char* overloads to avoid ambiguity with implicit integer conversion
    json& operator[](const char* key) {
        return operator[](std::string(key));
    }
    const json& operator[](const char* key) const {
        return operator[](std::string(key));
    }
    
    // Array access
    void push_back(const json& val) {
        if (m_type == null_t) m_type = array_t;
        if (m_type == array_t) m_array.push_back(val);
    }
    
    size_t size() const {
        if (m_type == array_t) return m_array.size();
        if (m_type == object_t) return m_object.size();
        return 0;
    }
    
    // Iterators for object
    auto begin() { return m_object.begin(); }
    auto end() { return m_object.end(); }
    auto begin() const { return m_object.begin(); }
    auto end() const { return m_object.end(); }

    // Implicit Conversions
    operator int() const { return static_cast<int>(m_number); }
    operator long() const { return static_cast<long>(m_number); }
    operator long long() const { return static_cast<long long>(m_number); }
    operator unsigned int() const { return static_cast<unsigned int>(m_number); }
    operator double() const { return m_number; }
    operator std::string() const { return m_string; }
    operator bool() const { return m_bool; }

    // Explicit get
    template<typename T>
    T get() const { 
        // Simple manual dispatch since we can't easily rely on full type_traits without including it
        // But since this is header only stub, we can rely on implicit conversion of the return value
        return static_cast<T>(*this); 
    }
};

// Specialize get
template<> inline std::string json::get<std::string>() const { return m_string; }
template<> inline double json::get<double>() const { return m_number; }
template<> inline int json::get<int>() const { return (int)m_number; }
template<> inline int64_t json::get<int64_t>() const { return (int64_t)m_number; }
template<> inline bool json::get<bool>() const { return m_bool; }

}
