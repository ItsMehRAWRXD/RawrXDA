// Canonical JSON types for the Qt-free tree.
//
// Important: the agent stack ships a minimal header-only JSON (`src/agent/simple_json.hpp`)
// that defines a global `JsonValue` class. Do not define a second `JsonValue` type here,
// or it will collide across translation units.
#pragma once

#include "agent/simple_json.hpp"

using JsonObject = JsonValue::Object;
using JsonArray = JsonValue::Array;

struct JsonDoc {
    static std::string toJson(const JsonValue& v) { return v.toJsonString(); }
    static std::string toJson(const JsonObject& obj) { return JsonValue(obj).toJsonString(); }
    static std::string toJson(const JsonArray& arr) { return JsonValue(arr).toJsonString(); }
};
