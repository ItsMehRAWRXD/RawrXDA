#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <vector>

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

    inline void set_level(level::level_enum) {}
    inline void set_pattern(const std::string&) {}

    class logger {
    public:
        void log(level::level_enum lvl, const std::string& msg) {
            const char* s = "[INFO]";
            switch(lvl) {
                case level::debug: s = "[DEBUG]"; break;
                case level::warn: s = "[WARN]"; break;
                case level::err: s = "[ERROR]"; break;
                case level::critical: s = "[CRITICAL]"; break;
                default: break;
            }
            std::cout << s << " " << msg << std::endl;
        }

        void set_level(level::level_enum) {}
        
        // Dummy sinks vector
        struct sink_dummy {};
        std::vector<std::shared_ptr<sink_dummy>> m_sinks;
        std::vector<std::shared_ptr<sink_dummy>>& sinks() { return m_sinks; }
    };

    inline std::shared_ptr<logger> stdout_color_mt(const std::string&) {
        return std::make_shared<logger>();
    }

    namespace sinks {
        struct basic_file_sink_mt : public logger::sink_dummy {
            basic_file_sink_mt(const std::string&, bool) {}
        };
    }
}
