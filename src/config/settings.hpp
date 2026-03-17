#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace rawrxd::config {

using SettingValue = std::variant<bool, int, double, std::string>;

class Settings {
public:
    static Settings& instance() {
        static Settings instance;
        return instance;
    }

    // Load/Save to local JSON file (no external registry/cloud)
    bool load(const std::string& config_path);
    bool save(const std::string& config_path);

    // Get/Set settings for Editor/Terminal/Colors
    SettingValue get(const std::string& key);
    void set(const std::string& key, SettingValue value);

    // Reset settings to default
    void reset();

    // Notify registered components of configuration changes (LSP, UI)
    using ChangeHandler = std::function<void(const std::string& key, SettingValue value)>;
    void registerChangeHandler(const std::string& key, ChangeHandler handler);

    // Hardcoded project defaults for Zero-Bloat
    void applyZeroBloatDefaults();

private:
    std::string config_file_path;
    std::map<std::string, SettingValue> settings_map;
    std::map<std::string, std::vector<ChangeHandler>> change_handlers;
};

} // namespace rawrxd::config
