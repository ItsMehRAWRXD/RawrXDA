#pragma once

#include <string>
#include <unordered_map>

namespace RawrXD {

class Settings {
public:
    Settings();
    ~Settings();

    // Load settings from file
    bool Load(const std::string& path);
    
    // Save settings to file
    bool Save(const std::string& path) const;
    
    // Get/Set string setting
    std::string Get(const std::string& key, const std::string& default_value = "") const;
    void Set(const std::string& key, const std::string& value);
    
    // Get/Set int setting
    int GetInt(const std::string& key, int default_value = 0) const;
    void SetInt(const std::string& key, int value);
    
    // Get/Set bool setting
    bool GetBool(const std::string& key, bool default_value = false) const;
    void SetBool(const std::string& key, bool value);

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace RawrXD