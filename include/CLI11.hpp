#pragma once
#include <string>
#include <vector>
#include <functional>
#include <set>
#include <iostream>

namespace CLI {

struct Option {
    Option* default_val(const std::string&) { return this; }
    Option* default_val(bool) { return this; }
    Option* check(std::function<std::string(const std::string&)>) { return this; }
};

class App {
public:
    App(const std::string&) {}
    Option* add_option(const std::string&, std::string&, const std::string&) { return new Option(); }
    Option* add_option(const std::string&, int&, const std::string&) { return new Option(); }
    Option* add_option(const std::string&, std::vector<std::string>&, const std::string&) { return new Option(); }
    Option* add_flag(const std::string&, bool&, const std::string&) { return new Option(); }
    
    void parse(int, char**) {}
};

inline std::function<std::string(const std::string&)> IsMember(const std::set<std::string>&) {
    return [](const std::string&) { return std::string(); };
}

inline std::function<std::string(const std::string&)> ExistingDirectory(const std::string&) {
    return [](const std::string&) { return std::string(); };
}

}

#define CLI11_PARSE(app, argc, argv) \
    try { \
        app.parse(argc, argv); \
    } catch (const std::exception& e) { \
        std::cerr << e.what() << std::endl; \
        return 1; \
    }
