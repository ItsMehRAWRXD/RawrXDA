<<<<<<< HEAD
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <initializer_list>
#include <sstream>
#include <iostream>
#include <type_traits>

namespace nlohmann {
    class json {
    private:
        std::shared_ptr<std::string> value_;
        std::shared_ptr<std::map<std::string, json>> object_;
        std::shared_ptr<std::vector<json>> array_;
        int type_; // 0=null, 1=string, 2=number, 3=bool, 4=object, 5=array

    public:
        enum class value_t {
            null, object, array, string, boolean, number_integer, number_unsigned, number_float, binary, discarded
        };

        json() : type_(0) {}
        
        // Remove explicit to allow implicit conversions
        json(const std::string& s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        json(const char* s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        json(int n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(long long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(unsigned long long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {} 
        json(unsigned int n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(unsigned long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(double n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(bool b) : value_(std::make_shared<std::string>(b ? "true" : "false")), type_(3) {}
        
        template<typename T, typename = std::enable_if_t<!std::is_same_v<T, json>>>
        json(const std::vector<T>& v) {
            type_ = 5;
            array_ = std::make_shared<std::vector<json>>();
            for (const auto& e : v) array_->push_back(json(e));
        }
        
        json(const std::vector<json>& v) {
            type_ = 5;
            array_ = std::make_shared<std::vector<json>>(v);
        }

        json(std::initializer_list<json> init) {
            bool isObject = true;
            for (const auto& elem : init) {
                if (!elem.isArray() || elem.size() != 2 || !elem.at(0).isString()) {
                    isObject = false;
                    break;
                }
            }

            if (isObject && init.size() > 0) {
                type_ = 4;
                object_ = std::make_shared<std::map<std::string, json>>();
                for (const auto& elem : init) {
                    (*object_)[elem.at(0).get<std::string>()] = elem.at(1);
                }
            } else {
                type_ = 5;
                array_ = std::make_shared<std::vector<json>>(init);
            }
        }
        
        json(const std::map<std::string, json>& m) {
            type_ = 4;
            object_ = std::make_shared<std::map<std::string, json>>(m);
        }

        json& operator=(bool b) { *this = json(b); return *this; }
        json& operator=(int n) { *this = json(n); return *this; }
        json& operator=(long long n) { *this = json(n); return *this; }
        json& operator=(unsigned long long n) { *this = json(n); return *this; }
        json& operator=(unsigned int n) { *this = json(n); return *this; }
        json& operator=(unsigned long n) { *this = json(n); return *this; }
        json& operator=(double n) { *this = json(n); return *this; }
        json& operator=(const std::string& s) { *this = json(s); return *this; }
        json& operator=(const char* s) { *this = json(s); return *this; }
        template<typename T> json& operator=(const std::vector<T>& v) { *this = json(v); return *this; }

        json(const json& other) = default;
        json(json&& other) = default;
        json& operator=(const json& other) = default;
        json& operator=(json&& other) = default;

        static json parse(const std::string& str) { 
            // Very naive parser for simple values or empty object checking
            if (str == "{}") return json::object();
            if (str == "[]") return json::array();
            return json(str); // For now just wrap content
        }
        // 3-arg overload: parse(str, callback, allow_exceptions)
        static json parse(const std::string& str, void* /*cb*/, bool allow_exceptions) {
            json result = parse(str);
            if (!allow_exceptions) { /* never throws in our mini impl */ }
            return result;
        }
        bool is_discarded() const { return type_ == 0 && !value_ && !object_ && !array_; }
        static json object() { json j; j.type_ = 4; j.object_ = std::make_shared<std::map<std::string, json>>(); return j; }
        static json object(std::initializer_list<std::pair<const std::string, json>> init) {
            json j = object();
            for (const auto& kv : init) {
                (*j.object_)[kv.first] = kv.second;
=======
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>

namespace nlohmann {

/**
 * Lightweight JSON implementation for AI Toolkit
 * Provides essential JSON serialization/deserialization without external dependencies
 * Uses enum class value_t to avoid name collisions with factory methods
 */

class json {
public:
    enum class value_t {
        null,
        object,
        array,
        string,
        number,
        boolean
    };

    // Constructors
    json() : m_type(value_t::null) {}
    json(std::nullptr_t) : m_type(value_t::null) {}
    json(bool b) : m_type(value_t::boolean), m_bool(b) {}
    json(int i) : m_type(value_t::number), m_number(static_cast<double>(i)) {}
    json(unsigned int i) : m_type(value_t::number), m_number(static_cast<double>(i)) {}
    json(long l) : m_type(value_t::number), m_number(static_cast<double>(l)) {}
    json(unsigned long l) : m_type(value_t::number), m_number(static_cast<double>(l)) {}
    json(long long ll) : m_type(value_t::number), m_number(static_cast<double>(ll)) {}
    json(unsigned long long ll) : m_type(value_t::number), m_number(static_cast<double>(ll)) {}
    json(double d) : m_type(value_t::number), m_number(d) {}
    json(float f) : m_type(value_t::number), m_number(static_cast<double>(f)) {}
    json(const std::string& s) : m_type(value_t::string), m_string(s) {}
    json(const char* s) : m_type(value_t::string), m_string(s ? s : "") {}
    json(const json& other) { *this = other; }
    json(json&& other) noexcept { *this = std::move(other); }

    // Object constructor from initializer_list of pairs
    json(const std::initializer_list<std::pair<std::string, json>>& init) : m_type(value_t::object) {
        for (const auto& [key, value] : init) {
            m_object[key] = value;
        }
    }

    // Array constructor from initializer_list of json
    json(const std::initializer_list<json>& init) : m_type(value_t::array) {
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
            other.m_type = value_t::null;
        }
        return *this;
    }

    // Assignment from initializer_list<pair> (creates object)
    json& operator=(const std::initializer_list<std::pair<std::string, json>>& init) {
        m_type = value_t::object;
        m_object.clear();
        m_array.clear();
        m_string.clear();
        for (const auto& [key, value] : init) {
            m_object[key] = value;
        }
        return *this;
    }

    // Type checking
    value_t type() const { return m_type; }
    bool is_null() const { return m_type == value_t::null; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_number() const { return m_type == value_t::number; }
    bool is_boolean() const { return m_type == value_t::boolean; }

    // Exception type alias (compatibility with real nlohmann/json)
    using exception = std::runtime_error;

    // Implicit conversion operators for nlohmann-style usage
    operator std::string() const {
        if (m_type == value_t::string) return m_string;
        if (m_type == value_t::number) return std::to_string(static_cast<long long>(m_number));
        if (m_type == value_t::boolean) return m_bool ? "true" : "false";
        if (m_type == value_t::null) return "";
        throw std::runtime_error("Cannot convert JSON to string");
    }

    operator bool() const {
        if (m_type == value_t::boolean) return m_bool;
        if (m_type == value_t::number) return m_number != 0.0;
        if (m_type == value_t::null) return false;
        if (m_type == value_t::string) return !m_string.empty();
        return true; // objects and arrays are truthy
    }

    operator int() const {
        if (m_type == value_t::number) return static_cast<int>(m_number);
        throw std::runtime_error("Cannot convert JSON to int");
    }

    operator double() const {
        if (m_type == value_t::number) return m_number;
        throw std::runtime_error("Cannot convert JSON to double");
    }

    operator uint64_t() const {
        if (m_type == value_t::number) return static_cast<uint64_t>(m_number);
        throw std::runtime_error("Cannot convert JSON to uint64_t");
    }

    // Stream extraction (parse from istream)
    friend std::istream& operator>>(std::istream& is, json& j) {
        std::string content((std::istreambuf_iterator<char>(is)),
                            std::istreambuf_iterator<char>());
        j = json::parse(content);
        return is;
    }

    // Stream insertion (serialize to ostream)
    friend std::ostream& operator<<(std::ostream& os, const json& j) {
        os << j.dump();
        return os;
    }

    // Object access
    json& operator[](const std::string& key) {
        if (m_type == value_t::null) m_type = value_t::object;
        if (m_type != value_t::object) throw std::runtime_error("Cannot index non-object as object");
        return m_object[key];
    }

    const json& operator[](const std::string& key) const {
        if (m_type != value_t::object) throw std::runtime_error("Cannot index non-object as object");
        auto it = m_object.find(key);
        if (it != m_object.end()) return it->second;
        static json null_val;
        return null_val;
    }

    // Array access
    json& operator[](size_t idx) {
        if (m_type == value_t::null) m_type = value_t::array;
        if (m_type != value_t::array) throw std::runtime_error("Cannot index non-array as array");
        if (idx >= m_array.size()) m_array.resize(idx + 1);
        return m_array[idx];
    }

    const json& operator[](size_t idx) const {
        if (m_type != value_t::array) throw std::runtime_error("Cannot index non-array as array");
        if (idx >= m_array.size()) throw std::out_of_range("Array index out of range");
        return m_array[idx];
    }

    // contains
    bool contains(const std::string& key) const {
        if (m_type != value_t::object) return false;
        return m_object.find(key) != m_object.end();
    }

    // value with default (json overload)
    json value(const std::string& key, const json& default_val) const {
        if (m_type != value_t::object) return default_val;
        auto it = m_object.find(key);
        return (it != m_object.end()) ? it->second : default_val;
    }

    // Specializations defined outside class
    template<typename T>
    T get() const;

    // Array operations
    void push_back(const json& val) {
        if (m_type == value_t::null) m_type = value_t::array;
        if (m_type != value_t::array) throw std::runtime_error("Cannot push_back to non-array");
        m_array.push_back(val);
    }

    // push_back from initializer_list<pair> (pushes a json object)
    void push_back(const std::initializer_list<std::pair<std::string, json>>& init) {
        if (m_type == value_t::null) m_type = value_t::array;
        if (m_type != value_t::array) throw std::runtime_error("Cannot push_back to non-array");
        json obj(init);
        m_array.push_back(std::move(obj));
    }

    // Typed value with default
    template<typename T>
    T value(const std::string& key, const T& default_val) const {
        auto it = m_object.find(key);
        if (it != m_object.end()) {
             try {
                return it->second.get<T>();
             } catch(...) {}
        }
        return default_val;
    }

    std::string value(const std::string& key, const char* default_val) const {
        return value<std::string>(key, std::string(default_val));
    }

    json& operator=(size_t n) { *this = json(static_cast<unsigned long long>(n)); return *this; }

    template<typename T>
    json(const std::vector<T>& v) : m_type(value_t::array) {
        for(const auto& e : v) m_array.push_back(json(e));
    }

    template<typename T>
    json& operator=(const std::vector<T>& v) {
        m_type = value_t::array;
        m_array.clear();
        for(const auto& e : v) m_array.push_back(json(e));
        return *this;
    }

    size_t size() const {
        if (m_type == value_t::array) return m_array.size();
        if (m_type == value_t::object) return m_object.size();
        if (m_type == value_t::string) return m_string.size();
        return 0;
    }

    bool empty() const {
        if (m_type == value_t::array) return m_array.empty();
        if (m_type == value_t::object) return m_object.empty();
        if (m_type == value_t::string) return m_string.empty();
        return m_type == value_t::null;
    }

    // ── Polymorphic iterator (supports both array and object iteration) ──
    class iterator {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = json;
        using pointer           = json*;
        using reference         = json&;
        using iterator_category = std::forward_iterator_tag;

        iterator() : m_is_array(true) {}
        explicit iterator(std::vector<json>::iterator it) : m_is_array(true), m_arr_it(it) {}
        explicit iterator(std::map<std::string, json>::iterator it) : m_is_array(false), m_obj_it(it) {}

        reference operator*() { return m_is_array ? *m_arr_it : m_obj_it->second; }
        pointer   operator->() { return m_is_array ? &(*m_arr_it) : &(m_obj_it->second); }

        iterator& operator++() {
            if (m_is_array) ++m_arr_it; else ++m_obj_it;
            return *this;
        }
        iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        bool operator==(const iterator& o) const {
            return m_is_array ? (m_arr_it == o.m_arr_it) : (m_obj_it == o.m_obj_it);
        }
        bool operator!=(const iterator& o) const { return !(*this == o); }

        const std::string& key() const { return m_obj_it->first; }
        reference value() { return m_is_array ? *m_arr_it : m_obj_it->second; }

    private:
        bool m_is_array;
        std::vector<json>::iterator m_arr_it;
        std::map<std::string, json>::iterator m_obj_it;
    };

    class const_iterator {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = const json;
        using pointer           = const json*;
        using reference         = const json&;
        using iterator_category = std::forward_iterator_tag;

        const_iterator() : m_is_array(true) {}
        explicit const_iterator(std::vector<json>::const_iterator it) : m_is_array(true), m_arr_it(it) {}
        explicit const_iterator(std::map<std::string, json>::const_iterator it) : m_is_array(false), m_obj_it(it) {}

        reference operator*()  const { return m_is_array ? *m_arr_it : m_obj_it->second; }
        pointer   operator->() const { return m_is_array ? &(*m_arr_it) : &(m_obj_it->second); }

        const_iterator& operator++() {
            if (m_is_array) ++m_arr_it; else ++m_obj_it;
            return *this;
        }
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        bool operator==(const const_iterator& o) const {
            return m_is_array ? (m_arr_it == o.m_arr_it) : (m_obj_it == o.m_obj_it);
        }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }

        const std::string& key() const { return m_obj_it->first; }
        reference value() const { return m_is_array ? *m_arr_it : m_obj_it->second; }

    private:
        bool m_is_array;
        std::vector<json>::const_iterator m_arr_it;
        std::map<std::string, json>::const_iterator m_obj_it;
    };

    // Iteration for arrays and objects
    iterator begin() {
        if (m_type == value_t::array) return iterator(m_array.begin());
        if (m_type == value_t::object) return iterator(m_object.begin());
        throw std::runtime_error("Cannot iterate non-container JSON type");
    }

    iterator end() {
        if (m_type == value_t::array) return iterator(m_array.end());
        if (m_type == value_t::object) return iterator(m_object.end());
        throw std::runtime_error("Cannot iterate non-container JSON type");
    }

    const_iterator begin() const {
        if (m_type == value_t::array) return const_iterator(m_array.begin());
        if (m_type == value_t::object) return const_iterator(m_object.begin());
        throw std::runtime_error("Cannot iterate non-container JSON type");
    }

    const_iterator end() const {
        if (m_type == value_t::array) return const_iterator(m_array.end());
        if (m_type == value_t::object) return const_iterator(m_object.end());
        throw std::runtime_error("Cannot iterate non-container JSON type");
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

    // Factory methods (legacy names)
    static json object_type() {
        json j;
        j.m_type = value_t::object;
        return j;
    }

    static json array_type() {
        json j;
        j.m_type = value_t::array;
        return j;
    }

    // nlohmann-compatible factory methods
    static json object() {
        return object_type();
    }

    static json object(const std::initializer_list<std::pair<std::string, json>>& init) {
        json j;
        j.m_type = value_t::object;
        for (const auto& [key, val] : init) {
            j.m_object[key] = val;
        }
        return j;
    }

    static json array() {
        return array_type();
    }

    static json array(const std::initializer_list<json>& init) {
        json j;
        j.m_type = value_t::array;
        for (const auto& item : init) {
            j.m_array.push_back(item);
        }
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

            std::string key = parse_string(str, pos).m_string;
            skip_whitespace(str, pos);
            if (pos >= str.length() || str[pos] != ':') throw std::runtime_error("Expected ':' in object");
            pos++;

            obj[key] = parse_value(str, pos);
            skip_whitespace(str, pos);

            if (pos >= str.length()) throw std::runtime_error("Unexpected end of object");
            if (str[pos] == '}') {
                pos++;
                break;
>>>>>>> origin/main
            }
            return j;
        }
        static json array() { json j; j.type_ = 5; j.array_ = std::make_shared<std::vector<json>>(); return j; }
        static json array(std::initializer_list<json> init) { json j; j.type_ = 5; j.array_ = std::make_shared<std::vector<json>>(init); return j; }

        json& operator[](const std::string& key) {
            if (type_ == 0) type_ = 4;
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return (*object_)[key];
        }

        json& operator[](const char* key) { return operator[](std::string(key)); }

        const json& operator[](const std::string& key) const {
            if (object_ && object_->count(key)) return object_->at(key);
            static json null_json; return null_json;
        }

        const json& operator[](const char* key) const { return operator[](std::string(key)); }
        
        json& operator[](size_t index) {
            if (type_ == 0) type_ = 5;
            if (!array_) array_ = std::make_shared<std::vector<json>>();
            if (index >= array_->size()) array_->resize(index + 1);
            return (*array_)[index];
        }

        const json& operator[](size_t index) const {
            if (array_ && index < array_->size()) return (*array_)[index];
            static json null_json; return null_json;
        }

        bool contains(const std::string& key) const { return object_ && object_->count(key) > 0; }
        
        // Extended type checks for nlohmann compatibility
        bool is_null() const { return type_ == 0; }
        bool is_string() const { return type_ == 1; }
        bool is_number() const { return type_ == 2; }
        bool is_number_integer() const { return type_ == 2 && value_ && value_->find('.') == std::string::npos; }
        bool is_number_float() const { return type_ == 2; }
        bool is_boolean() const { return type_ == 3; }
        bool is_object() const { return type_ == 4; }
        bool is_array() const { return type_ == 5; }
        bool is_primitive() const { return type_ <= 3; }
        bool is_structured() const { return type_ >= 4; }

        bool isArray() const { return is_array(); }
        bool isString() const { return is_string(); }
        bool isNull() const { return is_null(); }
        
        value_t type() const {
             if (type_ == 0) return value_t::null;
            if (type_ == 1) return value_t::string;
            if (type_ == 2) return value_t::number_integer; // Default to int for simplification
            if (type_ == 3) return value_t::boolean;
            if (type_ == 4) return value_t::object;
            if (type_ == 5) return value_t::array;
            return value_t::null;
        }

        template<typename T>
        T value(const std::string& key, const T& default_value) const {
            if (object_ && object_->count(key)) {
                return (*object_)[key].get<T>();
            }
            return default_value;
        }

        std::string value(const std::string& key, const char* default_value) const {
            if (object_ && object_->count(key)) {
                return (*object_)[key].get<std::string>();
            }
            return std::string(default_value);
        }

        template<typename T>
        T get() const {
            if (type_ == 0) return T();
             if constexpr (std::is_same_v<T, bool>) {
                if (type_ == 3) return value_ && (*value_ == "true" || *value_ == "1");
                return false;
            } else if constexpr (std::is_integral_v<T>) {
                if (type_ == 2 && value_) return (T)std::stoll(*value_);
                return T(0);
            } else if constexpr (std::is_floating_point_v<T>) {
                if (type_ == 2 && value_) return (T)std::stod(*value_);
                return T(0.0);
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (type_ == 1 && value_) return *value_;
                if (type_ == 2 && value_) return *value_;
                if (type_ == 3 && value_) return *value_;
                return "";
            }
            return T();
        }

        operator bool() const { return get<bool>(); }
        operator std::string() const { return get<std::string>(); }
        operator int() const { return get<int>(); }
        operator long long() const { return get<long long>(); }
        operator unsigned long long() const { return get<unsigned long long>(); }
        operator unsigned int() const { return (unsigned int)get<long long>(); }
        operator double() const { return get<double>(); }

        bool operator==(const json& other) const {
            if (type_ != other.type_) return false;
            if (type_ == 0) return true;
            if (type_ <= 3) {
                if (!value_ || !other.value_) return (!value_ && !other.value_);
                return *value_ == *other.value_;
            }
            return dump() == other.dump();
        }
<<<<<<< HEAD
        bool operator!=(const json& other) const { return !(*this == other); }
        bool operator==(const char* s) const { return get<std::string>() == std::string(s ? s : ""); }
        bool operator!=(const char* s) const { return !(*this == s); }
        bool operator==(const std::string& s) const { return get<std::string>() == s; }
        bool operator!=(const std::string& s) const { return !(*this == s); }
        friend bool operator==(const char* s, const json& j) { return j == s; }
        friend bool operator!=(const char* s, const json& j) { return !(j == s); }
        friend bool operator==(const std::string& s, const json& j) { return j == s; }
        friend bool operator!=(const std::string& s, const json& j) { return !(j == s); }

        void push_back(const json& j) {
            if (type_ != 5) {
                type_ = 5;
                array_ = std::make_shared<std::vector<json>>();
            }
            if (!array_) array_ = std::make_shared<std::vector<json>>();
            array_->push_back(j);
        }

        size_t size() const {
            if (type_ == 5 && array_) return array_->size();
            if (type_ == 4 && object_) return object_->size();
            return 0;
        }

        bool empty() const {
            if (type_ == 5 && array_) return array_->empty();
            if (type_ == 4 && object_) return object_->empty();
            return true;
        }
        
        json& at(size_t i) { return (*this)[i]; }
        const json& at(size_t i) const { return (*this)[i]; }
        json& at(const std::string& k) { return (*this)[k]; }
        const json& at(const std::string& k) const { return (*this)[k]; }

        // Erase key from object
        size_t erase(const std::string& key) {
            if (type_ == 4 && object_) return object_->erase(key);
            return 0;
        }

        std::string dump(int indent = -1) const {
            if (type_ == 0) return "null";
            if (type_ == 1) return "\"" + (value_ ? *value_ : "") + "\"";
            if (type_ == 2) return value_ ? *value_ : "0";
            if (type_ == 3) return value_ ? *value_ : "false";
            
            if (type_ == 5) {
                std::string s = "[";
                if (array_) {
                    for (size_t i = 0; i < array_->size(); ++i) {
                        s += array_->at(i).dump();
                        if (i < array_->size() - 1) s += ",";
                    }
                }
                s += "]";
                return s;
            }
            
            if (type_ == 4) {
                std::string s = "{";
                if (object_) {
                    size_t i = 0;
                    for (auto const& [key, val] : *object_) {
                        s += "\"" + key + "\":" + val.dump();
                        if (i < object_->size() - 1) s += ",";
                        i++;
                    }
                }
                s += "}";
                return s;
            }
            return "{}";
        }

        // ---- Unified iterators: dereference yields json& ----
        // For both array and object iteration via range-based for.
        // Object iterators also expose .first (key) and .second (value ref).
        struct json_iterator {
            using vec_it = std::vector<json>::iterator;
            using map_it = std::map<std::string, json>::iterator;
            bool is_arr;
            vec_it vi; map_it mi;
            // Proxy for ->first / ->second access on object iterators
            std::string first;   // key (valid for objects only)
            json* second_ptr_ = nullptr;
            json& second() { return *second_ptr_; }

            json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi() {}
            json_iterator(map_it a, map_it) : is_arr(false), vi(), mi(a) { sync(); }
            void sync() { if (!is_arr) { first = mi->first; second_ptr_ = &mi->second; } }
            const std::string& key() const { return first; }
            json& value() { return *second_ptr_; }
            json& operator*() { return is_arr ? *vi : mi->second; }
            json* operator->() { return is_arr ? &(*vi) : &(mi->second); }
            json_iterator& operator++() { if (is_arr) ++vi; else { ++mi; sync(); } return *this; }
            bool operator!=(const json_iterator& o) const { return is_arr ? vi != o.vi : mi != o.mi; }
            bool operator==(const json_iterator& o) const { return !(*this != o); }
        };
        struct const_json_iterator {
            using vec_it = std::vector<json>::const_iterator;
            using map_it = std::map<std::string, json>::const_iterator;
            bool is_arr;
            vec_it vi; map_it mi;
            std::string first;
            const json* second_ptr_ = nullptr;
            const json& second() const { return *second_ptr_; }

            const_json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi() {}
            const_json_iterator(map_it a, map_it) : is_arr(false), vi(), mi(a) { sync(); }
            void sync() { if (!is_arr) { first = mi->first; second_ptr_ = &mi->second; } }
            const std::string& key() const { return first; }
            const json& value() const { return *second_ptr_; }
            const json& operator*() const { return is_arr ? *vi : mi->second; }
            const json* operator->() const { return is_arr ? &(*vi) : &(mi->second); }
            const_json_iterator& operator++() { if (is_arr) ++vi; else { ++mi; sync(); } return *this; }
            bool operator!=(const const_json_iterator& o) const { return is_arr ? vi != o.vi : mi != o.mi; }
            bool operator==(const const_json_iterator& o) const { return !(*this != o); }
        };

        auto begin() { 
            if (type_ == 5) { if (!array_) array_ = std::make_shared<std::vector<json>>(); return json_iterator(array_->begin(), array_->end()); }
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>(); return json_iterator(object_->begin(), object_->end()); 
        }
        auto end() { 
            if (type_ == 5) { if (!array_) array_ = std::make_shared<std::vector<json>>(); return json_iterator(array_->end(), array_->end()); }
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>(); return json_iterator(object_->end(), object_->end()); 
        }
        auto begin() const { 
            if (type_ == 5 && array_) return const_json_iterator(array_->begin(), array_->end());
            if (object_) return const_json_iterator(object_->begin(), object_->end());
            static std::vector<json> ev; return const_json_iterator(ev.begin(), ev.end());
        }
        auto end() const { 
            if (type_ == 5 && array_) return const_json_iterator(array_->end(), array_->end());
            if (object_) return const_json_iterator(object_->end(), object_->end());
            static std::vector<json> ev; return const_json_iterator(ev.end(), ev.end());
        }

        struct ItemsProxy {
             std::shared_ptr<std::map<std::string, json>> obj;
             auto begin() { return obj ? obj->begin() : std::map<std::string, json>::iterator(); }
             auto end() { return obj ? obj->end() : std::map<std::string, json>::iterator(); }
        };
        struct ConstItemsProxy {
             std::shared_ptr<std::map<std::string, json>> obj;
             auto begin() const { return obj ? obj->cbegin() : std::map<std::string, json>::const_iterator(); }
             auto end() const { return obj ? obj->cend() : std::map<std::string, json>::const_iterator(); }
        };
        ItemsProxy items() { return {object_}; }
        ConstItemsProxy items() const { return {object_}; }
    };
}
=======
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
            case value_t::null:
                oss << "null";
                break;
            case value_t::boolean:
                oss << (m_bool ? "true" : "false");
                break;
            case value_t::number: {
                if (std::isfinite(m_number)) {
                    long long int_part = static_cast<long long>(m_number);
                    if (m_number == int_part) {
                        oss << int_part;
                    } else {
                        oss << std::fixed << std::setprecision(10) << m_number;
                        std::string num_str = oss.str();
                        while (num_str.back() == '0') num_str.pop_back();
                        if (num_str.back() == '.') num_str.pop_back();
                        return num_str;
                    }
                } else {
                    oss << "null";
                }
                break;
            }
            case value_t::string:
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
            case value_t::array:
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
            case value_t::object:
                oss << "{";
                if (!m_object.empty() && indent >= 0) oss << "\n";
                {
                    bool first = true;
                    for (const auto& [key, val] : m_object) {
                        if (!first && indent < 0) oss << ",";
                        if (indent >= 0 && !first) oss << ",\n";
                        if (indent >= 0) oss << next_indent_str;
                        oss << "\"" << key << "\":";
                        if (indent >= 0) oss << " ";
                        oss << val.dump_impl(indent, current_indent + (indent > 0 ? indent : 0));
                        first = false;
                    }
                }
                if (!m_object.empty() && indent >= 0) oss << "\n" << indent_str;
                oss << "}";
                break;
        }
        return oss.str();
    }
};

// Template specializations for get<T>()
template<>
inline std::string json::get<std::string>() const {
    if (m_type == value_t::string) return m_string;
    if (m_type == value_t::number) return std::to_string(static_cast<long long>(m_number));
    if (m_type == value_t::boolean) return m_bool ? "true" : "false";
    if (m_type == value_t::null) return "null";
    throw std::runtime_error("Cannot get string from this JSON type");
}

template<>
inline int json::get<int>() const {
    if (m_type == value_t::number) return static_cast<int>(m_number);
    throw std::runtime_error("Cannot get int from this JSON type");
}

template<>
inline double json::get<double>() const {
    if (m_type == value_t::number) return m_number;
    throw std::runtime_error("Cannot get double from this JSON type");
}

template<>
inline bool json::get<bool>() const {
    if (m_type == value_t::boolean) return m_bool;
    throw std::runtime_error("Cannot get bool from this JSON type");
}

template<>
inline unsigned long long json::get<unsigned long long>() const {
    if (m_type == value_t::number) return static_cast<unsigned long long>(m_number);
    throw std::runtime_error("Cannot get unsigned long long from this JSON type");
}

template<>
inline long long json::get<long long>() const {
    if (m_type == value_t::number) return static_cast<long long>(m_number);
    throw std::runtime_error("Cannot get long long from this JSON type");
}

template<>
inline float json::get<float>() const {
    if (m_type == value_t::number) return static_cast<float>(m_number);
    throw std::runtime_error("Cannot get float from this JSON type");
}

template<>
inline std::vector<json> json::get<std::vector<json>>() const {
    if (m_type == value_t::array) return m_array;
    throw std::runtime_error("Cannot get array from this JSON type");
}

} // namespace nlohmann
>>>>>>> origin/main
