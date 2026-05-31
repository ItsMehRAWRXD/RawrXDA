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
            size_t pos = 0;
            return parse_value(str, pos);
        }
        // 3-arg overload: parse(str, callback, allow_exceptions)
        static json parse(const std::string& str, void* /*cb*/, bool /*allow_exceptions*/) {
            return parse(str);
        }

    private:
        static void skip_ws(const std::string& s, size_t& pos) {
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n'))
                ++pos;
        }

        static json parse_value(const std::string& s, size_t& pos) {
            skip_ws(s, pos);
            if (pos >= s.size()) return json();
            char c = s[pos];
            if (c == '{') return parse_object(s, pos);
            if (c == '[') return parse_array(s, pos);
            if (c == '"') return parse_string_value(s, pos);
            if (c == 't' || c == 'f') return parse_bool(s, pos);
            if (c == 'n') return parse_null(s, pos);
            return parse_number(s, pos);
        }

        static std::string parse_string_raw(const std::string& s, size_t& pos) {
            if (pos >= s.size() || s[pos] != '"') return "";
            ++pos; // skip opening quote
            std::string out;
            while (pos < s.size()) {
                char c = s[pos++];
                if (c == '"') return out;
                if (c == '\\' && pos < s.size()) {
                    char e = s[pos++];
                    switch (e) {
                        case '"': out += '"'; break;
                        case '\\': out += '\\'; break;
                        case '/': out += '/'; break;
                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'u': {
                            // consume 4 hex digits, emit '?'
                            for (int i = 0; i < 4 && pos < s.size(); ++i) ++pos;
                            out += '?';
                            break;
                        }
                        default: out += e; break;
                    }
                } else {
                    out += c;
                }
            }
            return out;
        }

        static json parse_string_value(const std::string& s, size_t& pos) {
            return json(parse_string_raw(s, pos));
        }

        static json parse_number(const std::string& s, size_t& pos) {
            size_t start = pos;
            bool is_float = false;
            if (pos < s.size() && s[pos] == '-') ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
            if (pos < s.size() && s[pos] == '.') { is_float = true; ++pos; while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos; }
            if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) { is_float = true; ++pos; if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos; while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos; }
            std::string numStr = s.substr(start, pos - start);
            json j;
            j.type_ = 2;
            j.value_ = std::make_shared<std::string>(numStr);
            return j;
        }

        static json parse_bool(const std::string& s, size_t& pos) {
            if (s.compare(pos, 4, "true") == 0) { pos += 4; return json(true); }
            if (s.compare(pos, 5, "false") == 0) { pos += 5; return json(false); }
            return json();
        }

        static json parse_null(const std::string& s, size_t& pos) {
            if (s.compare(pos, 4, "null") == 0) { pos += 4; }
            return json();
        }

        static json parse_object(const std::string& s, size_t& pos) {
            json j = json::object();
            ++pos; // skip '{'
            skip_ws(s, pos);
            if (pos < s.size() && s[pos] == '}') { ++pos; return j; }
            while (pos < s.size()) {
                skip_ws(s, pos);
                std::string key = parse_string_raw(s, pos);
                skip_ws(s, pos);
                if (pos < s.size() && s[pos] == ':') ++pos;
                json val = parse_value(s, pos);
                (*j.object_)[key] = std::move(val);
                skip_ws(s, pos);
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == '}') { ++pos; break; }
                break; // malformed
            }
            return j;
        }

        static json parse_array(const std::string& s, size_t& pos) {
            json j = json::array();
            ++pos; // skip '['
            skip_ws(s, pos);
            if (pos < s.size() && s[pos] == ']') { ++pos; return j; }
            while (pos < s.size()) {
                json val = parse_value(s, pos);
                j.array_->push_back(std::move(val));
                skip_ws(s, pos);
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == ']') { ++pos; break; }
                break; // malformed
            }
            return j;
        }

    public:
        bool is_discarded() const { return type_ == 0 && !value_ && !object_ && !array_; }
        static bool accept(const std::string& str) {
            try { size_t pos = 0; parse_value(str, pos); return true; } catch (...) { return false; }
        }
        static json object() { json j; j.type_ = 4; j.object_ = std::make_shared<std::map<std::string, json>>(); return j; }
        static json object(std::initializer_list<std::pair<const std::string, json>> init) {
            json j = object();
            for (const auto& kv : init) {
                (*j.object_)[kv.first] = kv.second;
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
        // Clear the JSON value
        void clear() {
            type_ = 0;
            value_.reset();
            object_.reset();
            array_.reset();
        }
        static std::string escape_json(const std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out += c; break;
                }
            }
            return out;
        }

        std::string dump(int indent = -1) const {
            if (type_ == 0) return "null";
            if (type_ == 1) return "\"" + escape_json(value_ ? *value_ : "") + "\"";
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
            vec_it vi; map_it mi, mi_end;
            // Proxy for ->first / ->second access on object iterators
            std::string first;   // key (valid for objects only)
            json* second_ptr_ = nullptr;
            json& second() { return *second_ptr_; }

            json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi(), mi_end() {}
            json_iterator(map_it a, map_it b) : is_arr(false), vi(), mi(a), mi_end(b) { sync(); }
            void sync() {
                if (is_arr || mi == map_it() || mi == mi_end) {
                    first.clear();
                    second_ptr_ = nullptr;
                    return;
                }
                first = mi->first;
                second_ptr_ = &mi->second;
            }
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
            vec_it vi; map_it mi, mi_end;
            std::string first;
            const json* second_ptr_ = nullptr;
            const json& second() const { return *second_ptr_; }

            const_json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi(), mi_end() {}
            const_json_iterator(map_it a, map_it b) : is_arr(false), vi(), mi(a), mi_end(b) { sync(); }
            void sync() {
                if (is_arr || mi == map_it() || mi == mi_end) {
                    first.clear();
                    second_ptr_ = nullptr;
                    return;
                }
                first = mi->first;
                second_ptr_ = &mi->second;
            }
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
             auto begin() {
                 if (obj) return obj->begin();
                 // Return begin() from a static empty map to avoid null dereference
                 static std::map<std::string, json> empty_map;
                 return empty_map.begin();
             }
             auto end() {
                 if (obj) return obj->end();
                 static std::map<std::string, json> empty_map;
                 return empty_map.end();
             }
        };
        struct ConstItemsProxy {
             std::shared_ptr<std::map<std::string, json>> obj;
             auto begin() const {
                 if (obj) return obj->cbegin();
                 static const std::map<std::string, json> empty_map;
                 return empty_map.cbegin();
             }
             auto end() const {
                 if (obj) return obj->cend();
                 static const std::map<std::string, json> empty_map;
                 return empty_map.cend();
             }
        };
        ItemsProxy items() { return {object_}; }
        ConstItemsProxy items() const { return {object_}; }
    };
}
