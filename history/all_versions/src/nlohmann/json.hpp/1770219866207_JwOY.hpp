#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace nlohmann {
    class json {
    private:
        std::shared_ptr<std::string> value_;
        std::shared_ptr<std::map<std::string, json>> object_;
        std::shared_ptr<std::vector<json>> array_;
        int type_; // 0=null, 1=string, 2=number, 3=bool, 4=object, 5=array

    public:
        json() : type_(0) {}
        
        explicit json(const std::string& s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        explicit json(const char* s) : value_(std::make_shared<std::string>(s)), type_(1) {}
        explicit json(int n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        explicit json(long long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        explicit json(unsigned long long n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        explicit json(size_t n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        explicit json(double n) : value_(std::make_shared<std::string>(std::to_string(n))), type_(2) {}
        explicit json(bool b) : value_(std::make_shared<std::string>(b ? "true" : "false")), type_(3) {}

        // Assignment operators for primitive types
        json& operator=(bool b) {
            value_ = std::make_shared<std::string>(b ? "true" : "false");
            type_ = 3;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(int n) {
            value_ = std::make_shared<std::string>(std::to_string(n));
            type_ = 2;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(long long n) {
            value_ = std::make_shared<std::string>(std::to_string(n));
            type_ = 2;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(unsigned long long n) {
            value_ = std::make_shared<std::string>(std::to_string(n));
            type_ = 2;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(size_t n) {
            value_ = std::make_shared<std::string>(std::to_string(n));
            type_ = 2;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(double n) {
            value_ = std::make_shared<std::string>(std::to_string(n));
            type_ = 2;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(const std::string& s) {
            value_ = std::make_shared<std::string>(s);
            type_ = 1;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(const char* s) {
            value_ = std::make_shared<std::string>(s);
            type_ = 1;
            object_.reset();
            array_.reset();
            return *this;
        }

        json& operator=(const json& other) = default;
        json& operator=(json&& other) = default;

        static json parse(const std::string& str) { return json(str); }
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

        const json& operator[](const std::string& key) const {
            if (object_ && object_->count(key)) {
                return object_->at(key);
            }
            static json null_json;
            return null_json;
        }

        bool contains(const std::string& key) const {
            return object_ && object_->count(key) > 0;
        }

        json& at(const std::string& key) {
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return (*object_)[key];
        }

        const json& at(const std::string& key) const {
            if (object_ && object_->count(key)) {
                return object_->at(key);
            }
            static json null_json;
            return null_json;
        }

        // Type conversion methods
        template<typename T>
        T get() const {
            if constexpr (std::is_same_v<T, bool>) {
                return value_ && (*value_ == "true" || *value_ == "1");
            } else if constexpr (std::is_same_v<T, int>) {
                return value_ ? std::stoi(*value_) : 0;
            } else if constexpr (std::is_same_v<T, long long>) {
                return value_ ? std::stoll(*value_) : 0;
            } else if constexpr (std::is_same_v<T, unsigned long long>) {
                return value_ ? std::stoull(*value_) : 0;
            } else if constexpr (std::is_same_v<T, size_t>) {
                return value_ ? std::stoull(*value_) : 0;
            } else if constexpr (std::is_same_v<T, double>) {
                return value_ ? std::stod(*value_) : 0.0;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return value_ ? *value_ : "";
            }
            return T();
        }

        // Implicit conversions
        operator bool() const {
            return value_ && (*value_ == "true" || *value_ == "1");
        }

        operator std::string() const {
            return value_ ? *value_ : "";
        }

        operator int() const {
            return value_ ? std::stoi(*value_) : 0;
        }

        operator long long() const {
            return value_ ? std::stoll(*value_) : 0;
        }

        operator unsigned long long() const {
            return value_ ? std::stoull(*value_) : 0;
        }

        operator size_t() const {
            return value_ ? std::stoull(*value_) : 0;
        }

        operator double() const {
            return value_ ? std::stod(*value_) : 0.0;
        }

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

        std::string dump() const { return value_ ? *value_ : "{}"; }

        // Iterator support for range-based for loops on objects
        class ObjectIterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = std::pair<const std::string&, json&>;

            ObjectIterator(std::map<std::string, json>::iterator it) : it_(it) {}

            std::pair<const std::string&, json&> operator*() {
                return {it_->first, it_->second};
            }

            ObjectIterator& operator++() {
                ++it_;
                return *this;
            }

            ObjectIterator operator++(int) {
                auto tmp = *this;
                ++it_;
                return tmp;
            }

            bool operator==(const ObjectIterator& other) const { return it_ == other.it_; }
            bool operator!=(const ObjectIterator& other) const { return it_ != other.it_; }

        private:
            std::map<std::string, json>::iterator it_;
        };

        // For structured bindings and items() iteration
        struct ItemsProxy {
            std::shared_ptr<std::map<std::string, json>> obj;

            auto begin() {
                if (!obj) obj = std::make_shared<std::map<std::string, json>>();
                return obj->begin();
            }

            auto end() {
                if (!obj) obj = std::make_shared<std::map<std::string, json>>();
                return obj->end();
            }
        };

        ItemsProxy items() {
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return ItemsProxy{object_};
        }

        // Regular iteration for maps
        auto begin() {
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return object_->begin();
        }

        auto end() {
            if (!object_) object_ = std::make_shared<std::map<std::string, json>>();
            return object_->end();
        }

        auto begin() const {
            if (object_) return object_->begin();
            static auto empty = std::map<std::string, json>();
            return empty.begin();
        }

        auto end() const {
            if (object_) return object_->end();
            static auto empty = std::map<std::string, json>();
            return empty.end();
        }
    };
}

#endif