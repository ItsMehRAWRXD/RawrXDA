#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

namespace RawrXD::Extensions {

// Forward declaration
class ExtensionAPIBridge;

using ExtensionInitFunc = bool (*)(ExtensionAPIBridge* api);
using ExtensionDeinitFunc = void (*)();

struct ExtensionRecord {
    void* handle = nullptr;
    ExtensionInitFunc init = nullptr;
    ExtensionDeinitFunc deinit = nullptr;
    std::string name;
    std::string version;
    bool active = false;
};

class ExtensionSystemHost {
public:
    static ExtensionSystemHost& getInstance();
    
    bool loadExtension(const std::string& path);
    bool unloadExtension(const std::string& name);
    void unloadAll();
    
    bool isExtensionLoaded(const std::string& name) const;
    std::vector<std::string> listLoaded() const;
    
    ExtensionAPIBridge* getAPIBridge() { return &m_apiBridge; }
    
    void broadcastEvent(const std::string& eventType, const std::string& payload);

private:
    ExtensionSystemHost() = default;
    ~ExtensionSystemHost() { unloadAll(); }
    
    ExtensionSystemHost(const ExtensionSystemHost&) = delete;
    ExtensionSystemHost& operator=(const ExtensionSystemHost&) = delete;
    
    mutable std::mutex m_mutex;
    std::map<std::string, ExtensionRecord> m_extensions;
    ExtensionAPIBridge m_apiBridge;
};

} // namespace RawrXD::Extensions
