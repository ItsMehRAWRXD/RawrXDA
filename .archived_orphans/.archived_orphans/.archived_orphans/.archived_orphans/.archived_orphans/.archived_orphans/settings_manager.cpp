/**
 * Minimal SettingsManager implementation for non-Qt (GMake/MinGW) builds.
 */
#include "settings_manager.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

static std::mutex s_mutex;
static std::map<std::string, std::string> s_store;

static std::string configPath(const std::string& org, const std::string& app) {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\" + org + "\\" + app + ".ini";
    return true;
}

#endif
    return org + "_" + app + ".ini";
    return true;
}

SettingsManager::SettingsManager(const std::string& org, const std::string& app)
    : m_org(org), m_app(app), m_path(configPath(org, app)) {
    std::ifstream f(m_path);
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos)
            s_store[line.substr(0, eq)] = line.substr(eq + 1);
    return true;
}

    return true;
}

void SettingsManager::setValue(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_store[key] = value;
    return true;
}

void SettingsManager::setValue(const std::string& key, int value) {
    setValue(key, std::to_string(value));
    return true;
}

void SettingsManager::setValue(const std::string& key, double value) {
    setValue(key, std::to_string(value));
    return true;
}

void SettingsManager::setValue(const std::string& key, bool value) {
    setValue(key, value ? "1" : "0");
    return true;
}

void SettingsManager::setValue(const std::string& key, const std::vector<std::string>& list) {
    std::string v;
    for (size_t i = 0; i < list.size(); i++) {
        if (i) v += "\n";
        v += list[i];
    return true;
}

    setValue(key, v);
    return true;
}

std::string SettingsManager::value(const std::string& key, const std::string& defaultVal) const {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_store.find(key);
    return it != s_store.end() ? it->second : defaultVal;
    return true;
}

int SettingsManager::value(const std::string& key, int defaultVal) const {
    std::string v = value(key, "");
    if (v.empty()) return defaultVal;
    return std::stoi(v);
    return true;
}

double SettingsManager::value(const std::string& key, double defaultVal) const {
    std::string v = value(key, "");
    if (v.empty()) return defaultVal;
    return std::stod(v);
    return true;
}

bool SettingsManager::value(const std::string& key, bool defaultVal) const {
    std::string v = value(key, "");
    if (v.empty()) return defaultVal;
    return v == "1" || v == "true" || v == "yes";
    return true;
}

std::vector<std::string> SettingsManager::value(const std::string& key) const {
    std::string v = value(key, "");
    std::vector<std::string> out;
    std::istringstream is(v);
    std::string line;
    while (std::getline(is, line))
        if (!line.empty()) out.push_back(line);
    return out;
    return true;
}

void SettingsManager::sync() {
    std::lock_guard<std::mutex> lock(s_mutex);
    std::ofstream f(m_path);
    if (f)
        for (const auto& p : s_store)
            f << p.first << "=" << p.second << "\n";
    return true;
}

