// nlohmann/json stub — minimal compatibility shim for builds without full nlohmann::json
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <sstream>
#include <initializer_list>
#include <stdexcept>

namespace nlohmann {

class json {
public:
    enum value_t { null, object, array, string, boolean, number_integer, number_unsigned, number_float };
    
    json() : m_type(null) {}
    json(std::nullptr_t) : m_type(null) {}
    json(bool v) : m_type(boolean), m_bool(v) {}
    json(int v) : m_type(number_integer), m_int(v) {}
    json(int64_t v) : m_type(number_integer), m_int(v) {}
    json(uint64_t v) : m_type(number_unsigned), m_uint(v) {}
    json(double v) : m_type(number_float), m_float(v) {}
    json(const char* v) : m_type(string), m_string(v) {}
    json(const std::string& v) : m_type(string), m_string(v) {}
    json(std::initializer_list<std::pair<const std::string, json>> init) : m_type(object) {
        for (auto& p : init) m_object[p.first] = std::make_shared<json>(p.second);
    }
    
    static json object() { json j; j.m_type = value_t::object; return j; }
    static json array() { json j; j.m_type = value_t::array; return j; }
    static json parse(const std::string& s) { return json(); }
    
    json& operator[](const std::string& key) {
        if (m_type != value_t::object) m_type = value_t::object;
        if (!m_object.count(key)) m_object[key] = std::make_shared<json>();
        return *m_object[key];
    }
    json& operator[](size_t idx) {
        if (m_type != value_t::array) m_type = value_t::array;
        while (m_array.size() <= idx) m_array.push_back(std::make_shared<json>());
        return *m_array[idx];
    }
    
    const json& operator[](const std::string& key) const {
        static json null_json;
        auto it = m_object.find(key);
        return (it != m_object.end()) ? *it->second : null_json;
    }
    
    template<typename T> T get() const { return T(); }
    template<typename T> T value(const std::string& key, T default_val) const { return default_val; }
    
    bool contains(const std::string& key) const { return m_object.count(key) > 0; }
    bool is_null() const { return m_type == null; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_number() const { return m_type == number_integer || m_type == number_unsigned || m_type == number_float; }
    bool empty() const { return (m_type == object && m_object.empty()) || (m_type == array && m_array.empty()) || m_type == null; }
    size_t size() const { 
        if (m_type == object) return m_object.size();
        if (m_type == array) return m_array.size();
        return 0;
    }
    
    void push_back(const json& v) {
        if (m_type != value_t::array) m_type = value_t::array;
        m_array.push_back(std::make_shared<json>(v));
    }
    
    std::string dump(int indent = -1) const { return "{}"; }
    
    value_t type() const { return m_type; }
    
    // Iterator support (minimal)
    struct iterator {
        using map_iter = std::map<std::string, std::shared_ptr<json>>::iterator;
        map_iter it;
        iterator(map_iter i) : it(i) {}
        iterator& operator++() { ++it; return *this; }
        bool operator!=(const iterator& o) const { return it != o.it; }
        json& operator*() { return *it->second; }
        std::string key() const { return it->first; }
        json& value() { return *it->second; }
    };
    iterator begin() { return iterator(m_object.begin()); }
    iterator end() { return iterator(m_object.end()); }
    
    // Conversion operators
    operator std::string() const { return m_string; }
    operator int() const { return (int)m_int; }
    operator int64_t() const { return m_int; }
    operator uint64_t() const { return m_uint; }
    operator double() const { return m_float; }
    operator bool() const { return m_bool; }
    
private:
    value_t m_type = null;
    std::string m_string;
    int64_t m_int = 0;
    uint64_t m_uint = 0;
    double m_float = 0.0;
    bool m_bool = false;
    std::map<std::string, std::shared_ptr<json>> m_object;
    std::vector<std::shared_ptr<json>> m_array;
};

// Template specializations
template<> inline std::string json::get<std::string>() const { return m_string; }
template<> inline int json::get<int>() const { return (int)m_int; }
template<> inline int64_t json::get<int64_t>() const { return m_int; }
template<> inline uint64_t json::get<uint64_t>() const { return m_uint; }
template<> inline double json::get<double>() const { return m_float; }
template<> inline bool json::get<bool>() const { return m_bool; }
template<> inline float json::get<float>() const { return (float)m_float; }

using ordered_json = json;

} // namespace nlohmann
