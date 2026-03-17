#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <type_traits>

namespace nlohmann {

struct json {
    enum value_t { null_t, object_t, array_t, string_t, number_t, boolean_t };
    value_t m_type = null_t;
    
    // Unified storage backing both Array and Object (values)
    std::vector<json> m_elements; 
    // Lookup for Object keys
    std::map<std::string, size_t> m_lookup;

    std::string m_string;
    double m_number = 0;
    bool m_bool = false;

    // CTORS
    json() = default;
    json(std::nullptr_t) : m_type(null_t) {}
    json(bool v) : m_type(boolean_t), m_bool(v) {}
    
    // Numbers
    json(int v) : m_type(number_t), m_number(v) {}
    json(long v) : m_type(number_t), m_number(v) {}
    json(long long v) : m_type(number_t), m_number(static_cast<double>(v)) {}
    json(unsigned int v) : m_type(number_t), m_number(v) {}
    json(unsigned long v) : m_type(number_t), m_number(v) {}
    json(unsigned long long v) : m_type(number_t), m_number(static_cast<double>(v)) {}
    json(double v) : m_type(number_t), m_number(v) {}
    
    // Strings
    json(const std::string& v) : m_type(string_t), m_string(v) {}
    json(const char* v) : m_type(string_t), m_string(v) {}
    
    // Initializer list (for objects/arrays)
    json(std::initializer_list<json> init) {
        // Heuristic: check if this looks like an object pair [key, value]
        bool is_object_pairs = true;
        for (const auto& j : init) {
            if (!j.is_array() || j.size() != 2 || !j[0].is_string()) {
                is_object_pairs = false;
                break;
            }
        }

        if (is_object_pairs && init.size() > 0) {
            m_type = object_t;
            for (const auto& j : init) {
                // j is [key, value]
                (*this)[j[0].get<std::string>()] = j[1];
            }
        } else {
            m_type = array_t;
            m_elements = init; // copy vector
        }
    }

    // Factory methods
    static json object() { json j; j.m_type = object_t; return j; }
    static json array() { json j; j.m_type = array_t; return j; }
    static json make_object() { return object(); }
    static json make_array() { return array(); }
    static json parse(const std::string& s) { 
        if (s.find('[') != std::string::npos) return array();
        return object(); 
    }

    // Type checks
    bool is_null() const { return m_type == null_t; }
    bool is_object() const { return m_type == object_t; }
    bool is_array() const { return m_type == array_t; }
    bool is_string() const { return m_type == string_t; }
    bool is_number() const { return m_type == number_t; }
    bool is_boolean() const { return m_type == boolean_t; }

    bool contains(const std::string& key) const {
        return m_lookup.find(key) != m_lookup.end();
    }
    
    size_t size() const { return m_elements.size(); }

    // Object Accessors
    json& operator[](const std::string& key) {
        if (m_type == null_t) m_type = object_t;
        if (m_lookup.find(key) == m_lookup.end()) {
             m_lookup[key] = m_elements.size();
             m_elements.push_back(json());
        }
        return m_elements[m_lookup[key]];
    }
    
    const json& operator[](const std::string& key) const {
        static json empty;
        auto it = m_lookup.find(key);
        if (it != m_lookup.end()) return m_elements[it->second];
        return empty;
    }

    // Fix implicit conversion ambiguity for char* keys
    json& operator[](const char* key) { return operator[](std::string(key)); }
    const json& operator[](const char* key) const { return operator[](std::string(key)); }

    // Disambiguate 0 (null) -> const char* by providing explicit integer overloads forwarding to size_t
    json& operator[](int index) { return operator[](static_cast<size_t>(index)); }
    const json& operator[](int index) const { return operator[](static_cast<size_t>(index)); }

    // Array Accessors
    json& operator[](size_t index) {
        if (index < m_elements.size()) return m_elements[index];
        static json dummy; return dummy;
    }
    const json& operator[](size_t index) const {
        if (index < m_elements.size()) return m_elements[index];
        static json dummy; return dummy;
    }

    void push_back(const json& val) {
        if (m_type == null_t) m_type = array_t;
        m_elements.push_back(val);
    }

    // Iterators (Unified to vector)
    auto begin() { return m_elements.begin(); }
    auto end() { return m_elements.end(); }
    auto begin() const { return m_elements.begin(); }
    auto end() const { return m_elements.end(); }

    // Dumping
    std::string dump(int = -1) const {
        if (m_type == string_t) return m_string; // Or escaped quotes? Simple return for now.
        if (m_type == number_t) return std::to_string(m_number);
        if (m_type == boolean_t) return m_bool ? "true" : "false";
        if (m_type == object_t) return "{\"obj\": \"mock\"}"; // minimalistic
        if (m_type == array_t) return "[]";
        return "";
    }

    // Value helper
    template<typename T>
    T value(const std::string& key, const T& default_value) const {
        auto it = m_lookup.find(key);
        if (it != m_lookup.end()) {
            return m_elements[it->second].get<T>();
        }
        return default_value;
    }

    // Conversions
    operator std::string() const { return dump(); }
    operator int() const { return static_cast<int>(m_number); }
    operator long() const { return static_cast<long>(m_number); }
    operator long long() const { return static_cast<long long>(m_number); }
    operator double() const { return m_number; }
    operator bool() const { return m_bool; }

    template<typename T>
    T get() const { 
        if constexpr (std::is_same_v<T, std::string>) return dump();
        return static_cast<T>(*this); 
    }
};

} // namespace nlohmann
