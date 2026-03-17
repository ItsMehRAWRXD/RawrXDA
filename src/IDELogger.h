// Minimal logger shim to satisfy CLI/engine builds when full IDE logger is absent.
#pragma once
#include <string>
#include <cstdio>

inline void IDELogger_Log(const char* tag, const char* msg, int level) {
    (void)level;
    std::fprintf(stderr, "[%s] %s\n", tag ? tag : "IDE", msg ? msg : "");
}

inline void IDELogger_Log(const std::string& tag, const std::string& msg, int level = 0) {
    IDELogger_Log(tag.c_str(), msg.c_str(), level);
}
