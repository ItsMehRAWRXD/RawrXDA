#pragma once

#include <string>
#include <vector>

/**
 * Minimal SettingsManager for non-Qt (GMake/MinGW) builds.
 * Persists to a simple key-value store (in-memory or file).
 */
class SettingsManager {
public:
    SettingsManager(const std::string& org, const std::string& app);
    void setValue(const std::string& key, const std::string& value);
    void setValue(const std::string& key, int value);
    void setValue(const std::string& key, double value);
    void setValue(const std::string& key, bool value);
    void setValue(const std::string& key, const std::vector<std::string>& list);
    std::string value(const std::string& key, const std::string& defaultVal = "") const;
    int value(const std::string& key, int defaultVal) const;
    double value(const std::string& key, double defaultVal) const;
    bool value(const std::string& key, bool defaultVal) const;
    std::vector<std::string> value(const std::string& key) const; // for string list
    void sync();
private:
    std::string m_org;
    std::string m_app;
    std::string m_path;
};
