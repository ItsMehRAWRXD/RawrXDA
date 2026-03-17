#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    using json = nlohmann::json;
    std::string s = R"({"version":"1.2.3","ok":true})";
    try {
        json j = json::parse(s);
        std::string v;
        if (j.contains("version") && j["version"].is_string()) v = j["version"].get<std::string>();
        bool ok = false;
        if (j.contains("ok") && j["ok"].is_boolean()) ok = j["ok"].get<bool>();
        std::cout << "version=" << v << " ok=" << ok << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "json parse exception: " << e.what() << "\n";
        return 2;
    }
}
