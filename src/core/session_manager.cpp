#include "session_manager.h"
#include "settings_persistence.h"

namespace RawrXD::Core {
    SessionManager& SessionManager::getInstance() {
        static SessionManager inst;
        return inst;
    }

    bool SessionManager::saveSession(const std::string& path, const SessionData& data) {
        nlohmann::json j;
        j["workspacePath"] = data.workspacePath;
        j["openFiles"] = data.openFiles;
        j["activeFile"] = data.activeFile;
        j["cursorPositions"] = data.cursorPositions;
        j["windowWidth"] = data.windowWidth;
        j["windowHeight"] = data.windowHeight;
        j["sidebarVisible"] = data.sidebarVisible;
        j["sidebarWidth"] = data.sidebarWidth;

        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << j.dump(2);
        return true;
    }

    std::optional<SessionData> SessionManager::loadSession(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return std::nullopt;

        nlohmann::json j;
        try {
            in >> j;
        } catch (...) {
            return std::nullopt;
        }

        SessionData data;
        data.workspacePath = j.value("workspacePath", "");
        data.openFiles = j.value("openFiles", std::vector<std::string>{});
        data.activeFile = j.value("activeFile", "");
        data.cursorPositions = j.value("cursorPositions", std::map<std::string, std::pair<int,int>>{});
        data.windowWidth = j.value("windowWidth", 1280);
        data.windowHeight = j.value("windowHeight", 720);
        data.sidebarVisible = j.value("sidebarVisible", true);
        data.sidebarWidth = j.value("sidebarWidth", 250);
        return data;
    }

    void SessionManager::autoSave(const SessionData& data) {
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        std::string autoPath = std::string(path) + "rawrxd_session.json";
        saveSession(autoPath, data);
    }
}
