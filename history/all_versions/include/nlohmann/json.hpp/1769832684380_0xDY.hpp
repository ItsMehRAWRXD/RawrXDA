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
    
    // Key of THIS element (if it is a child of an object)
    std::string m_element_key;

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
        bool is_object_pairs = true;
        for (const auto& j : init) {
            if (!j.is_array() || j.size() != 2 || !j[size_t(0)].is_string()) {
                is_object_pairs = false;
                break;
            }
        }

        if (is_object_pairs && init.size() > 0) {
            m_type = object_t;
            for (const auto& j : init) {
                // j is [key, value]
                std::string k = j[size_t(0)].get<std::string>();
                json& child = (*this)[k];
                child = j[size_t(1)];
                child.m_element_key = k; // ensure key is set
            }
        } else {
            m_type = array_t;
            for(const auto& val : init) {
                m_elements.push_back(val);
            }
        }
    }

    // Factory methods
    static json object() { json j; j.m_type = object_t; return j; }
    static json array(std::initializer_list<json> init = {}) { 
        json j; 
        j.m_type = array_t; 
        for(const auto& val : init) j.m_elements.push_back(val);
        return j; 
    }
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
             json val;
             val.m_element_key = key; // Store key!
             m_elements.push_back(val);
        }
        return m_elements[m_lookup[key]];
    }
    
    const json& operator[](const std::string& key) const {
        static json empty;
        auto it = m_lookup.find(key);
        if (it != m_lookup.end()) return m_elements[it->second];
        return empty;
    }

    json& operator[](const char* key) { return operator[](std::string(key)); }
    const json& operator[](const char* key) const { return operator[](std::string(key)); }
    
    // Explicit int overloads to catch 0 -> size_t
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

    // Iterator
    struct iterator {
        using v_it = std::vector<json>::iterator;
        v_it current;
        bool operator!=(const iterator& o) const { return current != o.current; }
        bool operator==(const iterator& o) const { return current == o.current; }
        iterator& operator++() { ++current; return *this; }
        json& operator*() { return *current; }
        json* operator->() { return &(*current); }
        std::string key() const { return current->m_element_key; }
        json& value() const { return *current; }
    };

    struct const_iterator {
        using v_it = std::vector<json>::const_iterator;
        v_it current;
        bool operator!=(const const_iterator& o) const { return current != o.current; }
        bool operator==(const const_iterator& o) const { return current == o.current; }
        const_iterator& operator++() { ++current; return *this; }
        const json& operator*() const { return *current; }
        const json* operator->() const { return &(*current); }
        std::string key() const { return current->m_element_key; }
        const json& value() const { return *current; }
    };

    iterator begin() { return { m_elements.begin() }; }
    iterator end() { return { m_elements.end() }; }
    const_iterator begin() const { return { m_elements.begin() }; }
    const_iterator end() const { return { m_elements.end() }; }
    
    // explicit const begin/end
    const_iterator cbegin() const { return { m_elements.begin() }; }
    const_iterator cend() const { return { m_elements.end() }; }

    // Dumping
    std::string dump(int = -1) const {
        if (m_type == string_t) return m_string; 
        if (m_type == number_t) return std::to_string(m_number);
        if (m_type == boolean_t) return m_bool ? "true" : "false";
        if (m_type == object_t) return "{\"mock\": \"obj\"}";
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
