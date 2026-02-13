#pragma once

// C++20, no Qt. Persistent memory space for agent conversations/preferences.

#include <string>
#include <map>
#include <vector>

class MemorySpaceManager
{
public:
    static MemorySpaceManager& instance();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    int64_t limitBytes() const { return m_limitBytes; }
    void setLimitBytes(int64_t bytes) { m_limitBytes = bytes; }

    void persist(const std::map<std::string, std::string>& memoryMap);
    std::map<std::string, std::string> loadMemory() const;

    std::vector<std::string> listKeys() const;
    bool deleteKey(const std::string& key);
    void clearAll();
    int64_t currentSizeBytes() const;

private:
    MemorySpaceManager() = default;
    std::string memoryFilePath() const;
    std::string settingsFilePath() const;
    std::string readJson() const;
    bool writeJson(const std::string& json) const;
    void ensureConfig();

    bool m_enabled = false;
    int64_t m_limitBytes = 134217728;
};
