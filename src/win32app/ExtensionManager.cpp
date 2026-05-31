#include "ExtensionManager.h"
#include "ExtensionManifestLoader.h"
#include <iostream>

namespace RawrXD::Extensions {

ExtensionManager& ExtensionManager::GetInstance() {
    static ExtensionManager instance;
    return instance;
}

bool ExtensionManager::LoadExtension(const std::string& extensionRootPath) {
    std::string manifestPath = extensionRootPath + "/package.json";
    auto manifest = ExtensionManifestLoader::Load(manifestPath);
    
    if (manifest.id.empty()) {
        std::cerr << "[ExtensionManager] Failed to load manifest from: " << manifestPath << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_extensions.find(manifest.id) != m_extensions.end()) {
        std::cout << "[ExtensionManager] Extension already loaded: " << manifest.id << std::endl;
        return true;
    }

    // Apply permissions before starting the process
    ExtensionManifestLoader::ApplyManifestToSandbox(manifest);

    // Create and start extension instance
    auto instance = std::make_shared<ExtensionInstance>(manifest.id);
    
    // In a real implementation, 'manifest.main' would be passed to the runner.
    // For now, we spawn our MASM isolated process.
    if (instance->Start()) {
        m_extensions[manifest.id] = instance;
        std::cout << "[ExtensionManager] Successfully loaded: " << manifest.name << " (" << manifest.id << ")" << std::endl;
        return true;
    }

    return false;
}

void ExtensionManager::UnloadExtension(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extensionId);
    if (it != m_extensions.end()) {
        it->second->Stop();
        m_extensions.erase(it);
        std::cout << "[ExtensionManager] Unloaded: " << extensionId << std::endl;
    }
}

void ExtensionManager::UnloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_extensions) {
        pair.second->Stop();
    }
    m_extensions.clear();
}

std::shared_ptr<ExtensionInstance> ExtensionManager::GetExtension(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extensionId);
    return (it != m_extensions.end()) ? it->second : nullptr;
}

std::vector<std::string> ExtensionManager::GetLoadedExtensionIds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (auto const& [id, _] : m_extensions) ids.push_back(id);
    return ids;
}

}
