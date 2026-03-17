#include "settings.h"
#include "gui.h"
#include <filesystem>
#include <fstream>

namespace RawrXD {

namespace {
void EnsureSettingsDir(const std::string& path) {
    std::filesystem::path p(path);
    auto dir = p.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
}
} // namespace

Settings::Settings() = default;
Settings::~Settings() = default;

bool Settings::Load(const std::string& path) {
    if (!std::filesystem::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        values_[key] = val;
    }
    return true;
}

bool Settings::Save(const std::string& path) const {
    EnsureSettingsDir(path);
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    for (const auto& kv : values_) {
        ofs << kv.first << "=" << kv.second << "\n";
    }
    return true;
}

std::string Settings::Get(const std::string& key, const std::string& default_value) const {
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : default_value;
}

void Settings::Set(const std::string& key, const std::string& value) {
    values_[key] = value;
}

int Settings::GetInt(const std::string& key, int default_value) const {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    try { return std::stoi(it->second); } catch (...) { return default_value; }
}

void Settings::SetInt(const std::string& key, int value) {
    values_[key] = std::to_string(value);
}

bool Settings::GetBool(const std::string& key, bool default_value) const {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    const std::string& v = it->second;
    return (v == "1" || v == "true" || v == "TRUE");
}

void Settings::SetBool(const std::string& key, bool value) {
    values_[key] = value ? "1" : "0";
}

} // namespace RawrXD
