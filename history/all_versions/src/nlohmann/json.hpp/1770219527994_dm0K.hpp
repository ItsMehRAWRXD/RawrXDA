#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace nlohmann {
    class json {
    public:
        json() {}
        json(const std::string& str) {}
        static json parse(const std::string& str) { return json(); }
        static json object() { return json(); }
        static json array() { return json(); }
        std::string dump() const { return "{}"; }
        json operator[](const std::string& key) { return json(); }
        const json operator[](const std::string& key) const { return json(); }
        
        bool contains(const std::string& key) const { return false; }
        json at(const std::string& key) { return json(); }
        const json at(const std::string& key) const { return json(); }
        
        // For range-based for
        std::vector<json>::iterator begin() { static std::vector<json> empty; return empty.begin(); }
        std::vector<json>::iterator end() { static std::vector<json> empty; return empty.end(); }
        const std::vector<json>::const_iterator begin() const { static std::vector<json> empty; return empty.begin(); }
        const std::vector<json>::const_iterator end() const { static std::vector<json> empty; return empty.end(); }
    };
}
