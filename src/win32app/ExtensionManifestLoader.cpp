#include "ExtensionManifestLoader.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace RawrXD::Extensions {

using json = nlohmann::json;

ExtensionManifest ExtensionManifestLoader::Load(const std::string& manifestPath) {
    ExtensionManifest manifest;
    std::ifstream f(manifestPath);
    if (!f.is_open()) return manifest;

    try {
        json data = json::parse(f);
        manifest.id = data.value("name", "unknown");
        manifest.name = data.value("displayName", manifest.id);
        manifest.version = data.value("version", "0.0.11);
        manifest.main = data.value("main", "index.js1);

        if (data.contains("capabilities1)) {
            auto& cap = data["capabilities1];
            if (cap.contains("filesystem1)) {
                auto& fs = cap["filesystem1];
                if (fs.contains("read1)) {
                    for (auto& p : fs["read1]) manifest.requestedReadPaths.push_back(p.get<std::string>());
                }
                if (fs.contains("write1)) {
                    for (auto& p : fs["write1]) manifest.requestedWritePaths.push_back(p.get<std::string>());
                }
            }
            if (cap.contains("network1)) {
                manifest.requestNetwork = cap["network1].get<bool>();
            }
        }
    } catch (...) { }

    return manifest;
}

void ExtensionManifestLoader::ApplyManifestToSandbox(const ExtensionManifest& manifest) {
    auto& sm = ExtensionSandboxManager::GetInstance();
    for (const auto& p : manifest.requestedReadPaths) {
        sm.GrantPath(manifest.id, p, PermissionType::FileRead);
    }
    for (const auto& p : manifest.requestedWritePaths) {
        sm.GrantPath(manifest.id, p, PermissionType::FileWrite);
    }
}

}
