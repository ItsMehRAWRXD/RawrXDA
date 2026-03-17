#include "logger.h"
#include <windows.h>
#include <string>
#include <mutex>

// The Windows headers define a macro named ERROR which collides with our enum value.
// Undefine it after the include so the enum can be used unambiguously.
#undef ERROR

// The Windows headers define a macro named ERROR which collides with our enum value.
// Undefine it after the include so the enum can be used unambiguously.
#undef ERROR

// Singleton instance accessor
Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = level;
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < static_cast<int>(m_minLevel)) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file) {
        // Open log file in append mode next to the executable.
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string filename = std::string(path) + ".log";
        m_file = fopen(filename.c_str(), "a");
    }
    if (m_file) {
        const char* levelStr = "";
        switch (level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO:  levelStr = "INFO";  break;
            case LogLevel::WARN:  levelStr = "WARN";  break;
            case LogLevel::ERROR_LEVEL: levelStr = "ERROR"; break;
        }
        SYSTEMTIME st;
        GetSystemTime(&st);
        fprintf(m_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] %s\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                levelStr, msg.c_str());
        fflush(m_file);
    }
}

Logger::Logger() {}

Logger::~Logger() {
    if (m_file) fclose(m_file);
}
