// HexMagConsole.h — Headless console output buffer
// Converted from Qt (QPlainTextEdit, QTime) to pure C++17
// Preserves ALL original functionality: log output, hex dump display

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <iostream>
#include <functional>

class HexMagConsole {
public:
    HexMagConsole() = default;
    ~HexMagConsole() = default;

    // Append a timestamped log line
    void appendLog(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        struct tm local_tm;
#ifdef _WIN32
        localtime_s(&local_tm, &t);
#else
        localtime_r(&t, &local_tm);
#endif
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                 local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);

        std::string line = std::string("[") + timeBuf + "] " + text;
        m_lines.push_back(line);
        std::cout << line << std::endl;

        if (m_onAppend) m_onAppend(line);
    }

    // Display hex dump of data buffer
    void displayHex(const uint8_t* data, size_t length, uint64_t baseAddr = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::ostringstream oss;
        oss << "=== HEX DUMP (" << length << " bytes) ===" << std::endl;

        for (size_t i = 0; i < length; i += 16) {
            // Address column
            oss << std::hex << std::setw(8) << std::setfill('0')
                << (baseAddr + i) << "  ";

            // Hex bytes
            for (size_t j = 0; j < 16; j++) {
                if (j == 8) oss << " ";
                if (i + j < length) {
                    oss << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(data[i + j]) << " ";
                } else {
                    oss << "   ";
                }
            }

            // ASCII column
            oss << " |";
            for (size_t j = 0; j < 16 && (i + j) < length; j++) {
                char c = static_cast<char>(data[i + j]);
                oss << (c >= 32 && c < 127 ? c : '.');
            }
            oss << "|" << std::endl;
        }

        std::string dump = oss.str();
        m_lines.push_back(dump);
        std::cout << dump;

        if (m_onAppend) m_onAppend(dump);
    }

    // Clear all log lines
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.clear();
    }

    // Get all buffered log lines
    const std::vector<std::string>& lines() const { return m_lines; }

    // Get last N lines
    std::vector<std::string> lastLines(size_t count) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (count >= m_lines.size()) return m_lines;
        return std::vector<std::string>(m_lines.end() - count, m_lines.end());
    }

    // Callback for new log entries
    std::function<void(const std::string&)> m_onAppend;

private:
    std::vector<std::string> m_lines;
    mutable std::mutex m_mutex;
};
