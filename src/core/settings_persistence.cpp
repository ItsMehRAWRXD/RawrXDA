#include "settings_persistence.h"
#include <fstream>
#include <filesystem>
#include <windows.h>

namespace RawrXD::Core {
    SettingsPersistence& SettingsPersistence::getInstance() {
        static SettingsPersistence inst;
        return inst;
    }

    bool SettingsPersistence::load(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_path = path;
        std::ifstream in(path);
        if (!in.is_open()) {
            m_data = nlohmann::json::object();
            return false;
        }
        try {
            in >> m_data;
        } catch (...) {
            m_data = nlohmann::json::object();
            return false;
        }
        return true;
    }

    bool SettingsPersistence::save() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_path.empty()) return false;

        std::string tmp = m_path + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out.is_open()) return false;
            out << m_data.dump(2);
            out.flush();
        }
        // Atomic replace
        if (!ReplaceFileA(m_path.c_str(), tmp.c_str(), nullptr, 0, nullptr, nullptr)) {
            // Fallback: delete + rename
            DeleteFileA(m_path.c_str());
            MoveFileA(tmp.c_str(), m_path.c_str());
        }
        return true;
    }

    nlohmann::json SettingsPersistence::get(const std::string& key, const nlohmann::json& defaultVal) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_data.find(key);
        return (it != m_data.end()) ? *it : defaultVal;
    }

    void SettingsPersistence::set(const std::string& key, const nlohmann::json& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[key] = value;
    }

    void SettingsPersistence::remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.erase(key);
    }
}
