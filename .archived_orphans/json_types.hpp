#pragma once
/**
 * RawrXD minimal JSON types — stub for release_agent, agentic_copilot_bridge, terminal, etc.
 * Provides JsonObject, JsonArray, JsonDoc::toJson for build compatibility.
 */
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <sstream>
#include <variant>

namespace RawrXD {

struct JsonVal {
    using Obj = std::map<std::string, JsonVal>;
    using Arr = std::vector<JsonVal>;
    std::variant<std::string, bool, int64_t, double, Obj, Arr> v;
    JsonVal() : v(std::string()) {}
    JsonVal(const std::string& s) : v(s) {}
    JsonVal(const char* s) : v(std::string(s)) {}
    JsonVal(bool b) : v(b) {}
    JsonVal(int64_t i) : v(i) {}
    JsonVal(double d) : v(d) {}
    JsonVal(const Obj& o) : v(o) {}
    JsonVal(Obj&& o) : v(std::move(o)) {}
    JsonVal(const Arr& a) : v(a) {}
    JsonVal(Arr&& a) : v(std::move(a)) {}
};

using JsonObject = std::map<std::string, JsonVal>;
using JsonArray = std::vector<JsonVal>;

struct JsonDoc {
    static std::string toJson(const JsonObject& obj) {
        std::ostringstream os;
        os << "{";
        bool first = true;
        for (const auto& p : obj) {
            if (!first) os << ",";
            os << "\"" << p.first << "\":";
            std::visit([&os](auto&& x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, std::string>) os << "\"" << x << "\"";
                else if constexpr (std::is_same_v<T, bool>) os << (x ? "true" : "false");
                else if constexpr (std::is_same_v<T, double>) os << x;
                else if constexpr (std::is_same_v<T, int64_t>) os << x;
                else if constexpr (std::is_same_v<T, JsonVal::Obj>) os << "{}";
                else if constexpr (std::is_same_v<T, JsonVal::Arr>) os << "[]";
                else os << x;
            }, p.second.v);
            first = false;
        }
        os << "}";
        return os.str();
    }
};

using JsonValue = JsonVal;
} // namespace RawrXD

// Allow unqualified use for release_agent and others
using RawrXD::JsonVal;
using RawrXD::JsonValue;
using RawrXD::JsonObject;
using RawrXD::JsonArray;
using RawrXD::JsonDoc;
