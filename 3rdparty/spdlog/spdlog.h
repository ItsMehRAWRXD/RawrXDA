// spdlog stub — minimal compatibility shim for builds without full spdlog
#pragma once
#include <cstdio>
#include <string>

namespace spdlog {
    template<typename... Args>
    inline void info(const char* fmt, Args&&...) { }
    template<typename... Args>
    inline void warn(const char* fmt, Args&&...) { }
    template<typename... Args>
    inline void error(const char* fmt, Args&&...) { }
    template<typename... Args>
    inline void debug(const char* fmt, Args&&...) { }
    template<typename... Args>
    inline void trace(const char* fmt, Args&&...) { }
    template<typename... Args>
    inline void critical(const char* fmt, Args&&...) { }
    
    inline void set_level(int) { }
    inline void set_pattern(const std::string&) { }
    
    namespace level {
        enum level_enum { trace, debug, info, warn, err, critical, off };
    }
}
