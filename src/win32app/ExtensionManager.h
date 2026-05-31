#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include "ExtensionInstance.h"
#include "ExtensionManifestLoader.h"

namespace RawrXD::Extensions {

class ExtensionManager {
public:
    static ExtensionManager& GetInstance();

    // Lifecycle Operations
    bool LoadExtension(const std::string& extensionRootPath);
    void UnloadExtension(const std::string& extensionId);
    void UnloadAll();

    // Resolution & Query
    std::shared_ptr<ExtensionInstance> GetExtension(const std::string& extensionId);
    std::vector<std::string> GetLoadedExtensionIds();

private:
    ExtensionManager() = default;
    ~ExtensionManager() { UnloadAll(); }

    std::map<std::string, std::shared_ptr<ExtensionInstance>> m_extensions;
    std::mutex m_mutex;
};

}
