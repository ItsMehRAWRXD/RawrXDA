#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <format> // Require C++20, but MinGW might struggle. Let's use simple cout.

namespace spdlog {
    namespace level {
        enum level_enum {
            trace,
            debug,
            info,
            warn,
            err,
            critical,
            off
        };
        inline level_enum from_str(const std::string&) { return info; }
    }

    template<typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        // Simple mock implementation
        std::cout << "[INFO] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        std::cerr << "[ERROR] " << fmt << std::endl;
    }

    template<typename... Args>
    void critical(const std::string& fmt, Args&&... args) {
        std::cerr << "[CRITICAL] " << fmt << std::endl;
    }
    
    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args) {
        std::cout << "[WARN] " << fmt << std::endl;
    }
    
    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        #ifdef _DEBUG
        std::cout << "[DEBUG] " << fmt << std::endl;
        #endif
    }
}
namespace spdlog {
    class logger {
    public:
        logger() = default;
        template<typename It>
        logger(const std::string&, It, It) {}
        void set_level(level::level_enum) {}
        template<typename... Args>
        void info(const std::string& fmt, Args&&... args) {}
        template<typename... Args>
        void error(const std::string& fmt, Args&&... args) {}
        template<typename... Args>
        void log(level::level_enum lvl, const std::string& fmt, Args&&... args) {
             std::cout << "[LOG] " << fmt << std::endl;
        }
    };

    static std::shared_ptr<logger> stderr_color_mt(const std::string& name) {
        return std::make_shared<logger>();
    }

    static void set_level(level::level_enum log_level) {}
    static void set_pattern(const std::string& pattern) {}
    static void set_default_logger(std::shared_ptr<logger>) {}

    template<typename... Args>
    static void info(const char* fmt, const Args&... args) {}
}

namespace spdlog::sinks {
    struct sink { virtual ~sink() = default; };
    using sink_ptr = std::shared_ptr<sink>;
    class basic_file_sink_mt : public sink { public: basic_file_sink_mt(const std::string&, bool) {} };
    class stdout_color_sink_mt : public sink { public: stdout_color_sink_mt() {} };
}
