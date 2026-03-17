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

        // ---- Unified iterators: dereference always yields json& ----
        struct json_iterator {
            using vec_it = std::vector<json>::iterator;
            using map_it = std::map<std::string, json>::iterator;
            bool is_arr;
            vec_it vi; map_it mi;
            json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi() {}
            json_iterator(map_it a, map_it) : is_arr(false), vi(), mi(a) {}
            json& operator*() { return is_arr ? *vi : mi->second; }
            json* operator->() { return is_arr ? &(*vi) : &(mi->second); }
            json_iterator& operator++() { if (is_arr) ++vi; else ++mi; return *this; }
            bool operator!=(const json_iterator& o) const { return is_arr ? vi != o.vi : mi != o.mi; }
            bool operator==(const json_iterator& o) const { return !(*this != o); }
        };
        struct const_json_iterator {
            using vec_it = std::vector<json>::const_iterator;
            using map_it = std::map<std::string, json>::const_iterator;
            bool is_arr;
            vec_it vi; map_it mi;
            const_json_iterator(vec_it a, vec_it) : is_arr(true), vi(a), mi() {}
            const_json_iterator(map_it a, map_it) : is_arr(false), vi(), mi(a) {}
            const json& operator*() const { return is_arr ? *vi : mi->second; }
            const json* operator->() const { return is_arr ? &(*vi) : &(mi->second); }
            const_json_iterator& operator++() { if (is_arr) ++vi; else ++mi; return *this; }
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
             auto begin() { return obj ? obj->begin() : std::make_shared<std::map<std::string, json>>()->begin(); }
             auto end() { return obj ? obj->end() : std::make_shared<std::map<std::string, json>>()->end(); }
        };
        ItemsProxy items() { return {object_}; }
    };
}
