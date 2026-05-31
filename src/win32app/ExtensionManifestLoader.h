#pragma once
#include <string>
#include <vector>
#include <map>
#include "ExtensionSandboxManager.h"

namespace RawrXD::Extensions {

struct ExtensionManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string main;
    std::vector<std::string> requestedReadPaths;
    std::vector<std::string> requestedWritePaths;
    bool requestNetwork = false;
};

class ExtensionManifestLoader {
public:
    static ExtensionManifest Load(const std::string& manifestPath);
    static void ApplyManifestToSandbox(const ExtensionManifest& manifest);
};

}
