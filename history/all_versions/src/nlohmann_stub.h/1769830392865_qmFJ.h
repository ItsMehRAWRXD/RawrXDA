#pragma once
#include <map>
#include <string>
#include <vector>

namespace nlohmann {

class json {
public:
    enum class Type { Null, Object, Array, String, Number, Boolean };
    
    json() : type(Type::Null) {}
    json(int) : type(Type::Number) {}
    json(double) : type(Type::Number) {}
    json(bool) : type(Type::Boolean) {}
    json(const std::string&) : type(Type::String) {}
    json(const char* s) : type(Type::String) {}
    
    // Explicit array/object construction?
    // We'll just be generic
    
    Type type;
    
    // Basic access
    json& operator[](const std::string& key) {
        type = Type::Object;
        return object_values[key];
    }
    
    json& operator[](int index) {
        type = Type::Array;
        if (index >= array_values.size()) array_values.resize(index + 1);
        return array_values[index];
    }
    
    // Output
    std::string dump(int indent = -1) const { return "{}"; }
    
private:
    std::map<std::string, json> object_values;
    std::vector<json> array_values;
};

}
