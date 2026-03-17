#pragma once
#include <string>
#include <vector>
#include <map>

namespace nlohmann {
    class json {
    public:
        json() {}
        json(const std::string& str) {}
        static json parse(const std::string& str) { return json(); }
        std::string dump() const { return "{}"; }
        json operator[](const std::string& key) { return json(); }
        const json operator[](const std::string& key) const { return json(); }
    };
}
