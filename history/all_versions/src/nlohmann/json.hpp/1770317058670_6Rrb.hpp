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
        json() : type_(0) {}
        
        // Remove explicit to allow implicit conversions
        json(const std::string& s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        json(const char* s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        json(int n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(long long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(unsigned int n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(double n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        json(bool b) : value_(std::make_shared<std::string>(b ? "true" : "false")), type_(3) {}
        
        // Object constructor from initializer list of pairs
        // e.g. json j = {{"key", "value"}, {"key2", 2}};
        // We use a specific signature often used by nlohmann json users
        // Note: This is an approximation. Real nlohmann json is much more complex.
        // For our use case (steps.push_back({{"key", val}, ...})), it is likely std::initializer_list<json>
        // where each element is an object? No, likely an initializer list that forms an object.
        
        // This constructor handles: json j = {{"k", "v"}, {"k2", "v2"}};
        // Inner elements {"k", "v"} are constructed as json (array of 2 strings/vals).
        json(std::initializer_list<json> init) {
            // Heuristic: If all elements are arrays of size 2 and the first element is a string, treat as Object.
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
        
        // Allow construction from map
        json(const std::map<std::string, json>& m) {
            type_ = 4;
            object_ = std::make_shared<std::map<std::string, json>>(m);
        }

        // Assignment operators
        json& operator=(bool b) { *this = json(b); return *this; }
        json& operator=(int n) { *this = json(n); return *this; }
        json& operator=(long long n) { *this = json(n); return *this; }
        json& operator=(unsigned int n) { *this = json(n); return *this; }
        json& operator=(double n) { *this = json(n); return *this; }
        json& operator=(const std::string& s) { *this = json(s); return *this; }
        json& operator=(const char* s) { *this = json(s); return *this; }

        json(const json& other) = default;
        json(json&& other) = default;
        json& operator=(const json& other) = default;
        json& operator=(json&& other) = default;

        static json parse(const std::string& str) { return json(str); } // Mock parse
        
        static json object() { 
            json j; 
            j.type_ = 4; 
            j.object_ = std::make_shared<std::map<std::string, json>>(); 
            return j; 
        }
        
        static json array() { 
            json j; 
            j.type_ = 5; 
            j.array_ = std::make_shared<std::vector<json>>(); 
            return j; 
        }

        json& operator[](const std::string& key) {
            if (type_ == 0) type_ = 4;
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return (*object_)[key];
        }

        json& operator[](const char* key) {
            return operator[](std::string(key));
        }

        const json& operator[](const std::string& key) const {
            if (object_ && object_->count(key)) {
                return object_->at(key);
            }
            static json null_json;
            return null_json;
        }

        const json& operator[](const char* key) const {
            return operator[](std::string(key));
        }
        
        // Array access
        json& operator[](size_t index) {
            if (type_ == 0) type_ = 5;
            if (!array_) array_ = std::make_shared<std::vector<json>>();
            if (index >= array_->size()) array_->resize(index + 1);
            return (*array_)[index];
        }

        const json& operator[](size_t index) const {
            if (array_ && index < array_->size()) return (*array_)[index];
            static json null_json;
            return null_json;
        }

        bool contains(const std::string& key) const {
            return object_ && object_->count(key) > 0;
        }
        
        bool isArray() const { return type_ == 5; }
        bool isString() const { return type_ == 1; }
        bool isNull() const { return type_ == 0; }
        
        // value() method with default
        template<typename T>
        T value(const std::string& key, const T& default_value) const {
            if (object_ && object_->count(key)) {
                return (*object_)[key].get<T>();
            }
            return default_value;
        }

        template<typename T>
        T get() const {
            if (type_ == 0) return T();
            
             if constexpr (std::is_same_v<T, bool>) {
                if (type_ == 3) return value_ && (*value_ == "true" || *value_ == "1");
                return false;
            } else if constexpr (std::is_same_v<T, int>) {
                if (type_ == 2 && value_) return std::stoi(*value_);
                return 0;
            } else if constexpr (std::is_same_v<T, long long>) {
                if (type_ == 2 && value_) return std::stoll(*value_);
                return 0;
            } else if constexpr (std::is_same_v<T, size_t>) {
                if (type_ == 2 && value_) return std::stoull(*value_);
                return 0;
            } else if constexpr (std::is_same_v<T, double>) {
                if (type_ == 2 && value_) return std::stod(*value_);
                return 0.0;
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (type_ == 1 && value_) return *value_;
                if (type_ == 2 && value_) return *value_;
                if (type_ == 3 && value_) return *value_;
                return "";
            }
            return T();
        }

        // Implicit conversions
        operator bool() const { return get<bool>(); }
        operator std::string() const { return get<std::string>(); }
        operator int() const { return get<int>(); }
        operator long long() const { return get<long long>(); }
        operator unsigned int() const { return (unsigned int)get<long long>(); }
        operator double() const { return get<double>(); }

        // Array operations
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
            if (type_ == 1) return "\"" + (value_ ? *value_ : "") + "\""; // Should escape quotes
            if (type_ == 2) return value_ ? *value_ : "0";
            if (type_ == 3) return value_ ? *value_ : "false";
            
            if (type_ == 5) { // Array
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
            
            if (type_ == 4) { // Object
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
        
        // Iterators
        auto begin() { if (!object_) object_ = std::make_shared<std::map<std::string, json>>(); return object_->begin(); }
        auto end() { if (!object_) object_ = std::make_shared<std::map<std::string, json>>(); return object_->end(); }
        auto begin() const { if (object_) return object_->begin(); static std::map<std::string, json> e; return e.begin(); }
        auto end() const { if (object_) return object_->end(); static std::map<std::string, json> e; return e.end(); }

        struct ItemsProxy {
             std::shared_ptr<std::map<std::string, json>> obj;
             auto begin() { return obj ? obj->begin() : std::make_shared<std::map<std::string, json>>()->begin(); }
             auto end() { return obj ? obj->end() : std::make_shared<std::map<std::string, json>>()->end(); }
        };
        ItemsProxy items() { return {object_}; }
    };
}
