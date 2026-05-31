#include "extensions/extension_system_host.h"
#include "extensions/extension_api_bridge.h"
#include <windows.h>
#include <filesystem>

namespace RawrXD::Extensions {
    ExtensionSystemHost& ExtensionSystemHost::getInstance() {
        static ExtensionSystemHost inst;
        return inst;
    }

    bool ExtensionSystemHost::loadExtension(const std::string& path) {
        HMODULE hMod = LoadLibraryA(path.c_str());
        if (!hMod) return false;

        auto initFn = reinterpret_cast<ExtensionInitFunc>(GetProcAddress(hMod, "RawrXD_ExtensionInit"));
        auto deinitFn = reinterpret_cast<ExtensionDeinitFunc>(GetProcAddress(hMod, "RawrXD_ExtensionDeinit"));

        if (!initFn || !deinitFn) {
            FreeLibrary(hMod);
            return false;
        }

        ExtensionRecord rec;
        rec.handle = hMod;
        rec.init = initFn;
        rec.deinit = deinitFn;
        rec.name = std::filesystem::path(path).stem().string();

        if (!initFn(&m_apiBridge)) {
            FreeLibrary(hMod);
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_extensions[rec.name] = rec;
        return true;
    }

    bool ExtensionSystemHost::unloadExtension(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) return false;
        
        it->second.deinit();
        FreeLibrary(static_cast<HMODULE>(it->second.handle));
        m_extensions.erase(it);
        return true;
    }

    void ExtensionSystemHost::unloadAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [name, rec] : m_extensions) {
            rec.deinit();
            FreeLibrary(static_cast<HMODULE>(rec.handle));
        }
        m_extensions.clear();
    }

    bool ExtensionSystemHost::isExtensionLoaded(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_extensions.count(name) > 0;
    }

    std::vector<std::string> ExtensionSystemHost::listLoaded() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> names;
        for (const auto& [name, _] : m_extensions) names.push_back(name);
        return names;
    }

    void ExtensionSystemHost::broadcastEvent(const std::string& eventType, const std::string& payload) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Placeholder for event broadcasting to extensions
    }
}
