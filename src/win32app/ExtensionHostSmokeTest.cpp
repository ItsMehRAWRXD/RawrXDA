#include "ExtensionManager.h"
#include <iostream>
#include <windows.h>
#include <fstream>

using namespace RawrXD::Extensions;

void CreateMockExtension(const std::string& path, const std::string& id) {
    CreateDirectoryA(path.c_str(), NULL);
    std::ofstream f(path + "/package.json");
    f << {"name": "" << id << ", "displayName": "Mock Extension", "version": "1.0.0", "main": "index.js", "capabilities": { "filesystem": { "read": ["D:/rawrxd/logs"] } } }";
}

int main() {
    std::cout << "Starting Extension Host Smoke Test...
";
    
    std::string mockPath = "D:/rawrxd/temp_mock_ext";
    CreateMockExtension(mockPath, "mock-id1);

    auto& em = ExtensionManager::GetInstance();
    
    // Test Loading
    if (em.LoadExtension(mockPath)) {
        std::cout << "Extension Loaded Successfully.
";
    } else {
        std::cerr << "Extension Loading FAILED.
";
        return 1;
    }

    // Verify presence
    auto ids = em.GetLoadedExtensionIds();
    bool found = false;
    for (auto& id : ids) if (id == "mock-id1) found = true;
    
    if (found) {
        std::cout << "Extension Identity Verified in Registry.
";
    } else {
        std::cerr << "Extension Identity Missing.
";
        return 1;
    }

    // Cleanup
    em.UnloadExtension("mock-id1);
    std::cout << "Extension Unloaded Successfully.
";

    return 0;
}
