#include "extensions/extension_api_bridge.h"
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <vector>
#include <chrono>
#include <iomanip>

namespace RawrXD::Extensions {
    int32_t ExtensionAPIBridge::registerCommand(const char* id, const char* label, CommandCallback cb, void* userData) {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        CommandReg reg;
        reg.id = id;
        reg.label = label;
        reg.callback = cb;
        reg.userData = userData;
        m_commands[id] = reg;
        return static_cast<int32_t>(m_commands.size());
    }

    void ExtensionAPIBridge::unregisterCommand(const char* id) {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        m_commands.erase(id);
    }

    void ExtensionAPIBridge::executeCommand(const char* id) {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        auto it = m_commands.find(id);
        if (it != m_commands.end() && it->second.callback) {
            it->second.callback(it->second.userData);
        }
    }

    int32_t ExtensionAPIBridge::showMessageBox(const char* title, const char* message, uint32_t flags) {
        return MessageBoxA(nullptr, message, title, flags);
    }

    void ExtensionAPIBridge::showStatusBarMessage(const char* message) {
        if (!message) return;
        // Post to IDE status bar via Win32 message or direct callback if available
        HWND hwndStatus = FindWindowExA(GetForegroundWindow(), nullptr, "msctls_statusbar32", nullptr);
        if (hwndStatus) {
            SendMessageA(hwndStatus, SB_SETTEXTA, 0, (LPARAM)message);
        }
        // Also log for persistence
        OutputDebugStringA("[StatusBar] ");
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }

    void ExtensionAPIBridge::logMessage(int32_t level, const char* message) {
        if (!message) return;
        const char* prefix = "[INFO] ";
        switch (level) {
            case 0: prefix = "[DEBUG] "; break;
            case 1: prefix = "[INFO] "; break;
            case 2: prefix = "[WARN] "; break;
            case 3: prefix = "[ERROR] "; break;
            case 4: prefix = "[FATAL] "; break;
        }
        OutputDebugStringA(prefix);
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
        // Also write to log file if configured
        const char* logPath = getenv("RAWRXD_EXTENSION_LOG");
        if (logPath) {
            std::ofstream log(logPath, std::ios::app);
            if (log) {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                log << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << " " << prefix << message << "\n";
            }
        }
    }

    bool ExtensionAPIBridge::readFile(const char* path, char** outData, size_t* outLen) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) return false;
        
        *outLen = size;
        *outData = new char[size];
        memcpy(*outData, buffer.data(), size);
        return true;
    }

    bool ExtensionAPIBridge::writeFile(const char* path, const char* data, size_t len) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(data, len);
        return file.good();
    }

    const char* ExtensionAPIBridge::getConfiguration(const char* section, const char* key) {
        if (!section || !key) return "";
        std::lock_guard<std::mutex> lock(m_configMutex);
        auto secIt = m_config.find(section);
        if (secIt == m_config.end()) return "";
        auto keyIt = secIt->second.find(key);
        if (keyIt == secIt->second.end()) return "";
        return keyIt->second.c_str();
    }

    void ExtensionAPIBridge::setConfiguration(const char* section, const char* key, const char* value) {
        if (!section || !key || !value) return;
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config[section][key] = value;
        // Persist to file if path is set
        if (!m_configPath.empty()) {
            std::ofstream file(m_configPath, std::ios::trunc);
            if (file) {
                for (const auto& sec : m_config) {
                    file << "[" << sec.first << "]\n";
                    for (const auto& kv : sec.second) {
                        file << kv.first << "=" << kv.second << "\n";
                    }
                    file << "\n";
                }
            }
        }
    }

    void ExtensionAPIBridge::subscribeToEvent(const char* eventType, CommandCallback callback, void* userData) {
        if (!eventType || !callback) return;
        std::lock_guard<std::mutex> lock(m_eventMutex);
        EventSub sub;
        sub.callback = callback;
        sub.userData = userData;
        m_eventSubs[eventType].push_back(sub);
    }

    void ExtensionAPIBridge::publishEvent(const char* eventType, void* eventData) {
        if (!eventType) return;
        std::lock_guard<std::mutex> lock(m_eventMutex);
        auto it = m_eventSubs.find(eventType);
        if (it != m_eventSubs.end()) {
            for (const auto& sub : it->second) {
                if (sub.callback) {
                    sub.callback(eventData);
                }
            }
        }
    }
}
