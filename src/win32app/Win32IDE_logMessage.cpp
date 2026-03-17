// =============================================================================
// Win32IDE_logMessage.cpp — Build variant: LogMessage fallback for targets that link
// Win32IDE panel code but not the full GUI (e.g. RawrEngine). Production impl:
// writes to OutputDebugString and %APPDATA%\RawrXD\ide.log.
// =============================================================================
#include "Win32IDE.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace {
    std::mutex s_logMutex;
    std::string s_logPath;
    bool s_logPathResolved = false;

    std::string getLogPath() {
        if (s_logPathResolved) return s_logPath;
        std::lock_guard<std::mutex> lock(s_logMutex);
        if (s_logPathResolved) return s_logPath;
        char appdata[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
            s_logPath = std::string(appdata) + "\\RawrXD\\ide.log";
            s_logPathResolved = true;
        }
        return s_logPath;
    }
}

void Win32IDE::logMessage(const std::string& category, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf = {};
    localtime_s(&tm_buf, &t);
    std::ostringstream line;
    line << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << " [" << category << "] " << message << "\n";
    std::string str = line.str();

    OutputDebugStringA(str.c_str());

    std::string path = getLogPath();
    if (!path.empty()) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        std::filesystem::path p(path);
        std::filesystem::create_directories(p.parent_path());
        std::ofstream f(path, std::ios::app);
        if (f.is_open())
            f << str;
    }
}
