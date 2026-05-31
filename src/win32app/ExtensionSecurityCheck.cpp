#include "ExtensionSandboxManager.h"
#include "ExtensionManifestLoader.h"
#include <iostream>
#include <cassert>

using namespace RawrXD::Extensions;

int main() {
    auto& sm = ExtensionSandboxManager::GetInstance();
    std::string extId = "test-ext";

    // 1. Initial State Check (Fail-Closed)
    assert(sm.IsPathAllowed(extId, "C:\\Windows\\System32", PermissionType::FileRead) == false);
    
    // 2. Grant Read Permission
    sm.GrantPath(extId, "D:\\rawrxd\\projects", PermissionType::FileRead);
    assert(sm.IsPathAllowed(extId, "D:\\rawrxd\\projects\\test.cpp", PermissionType::FileRead) == true);
    
    // 3. Subpath Validation
    assert(sm.IsPathAllowed(extId, "D:\\rawrxd\\projects\\subdir\\deep.h", PermissionType::FileRead) == true);
    
    // 4. Boundary Protection (Traversal)
    assert(sm.IsPathAllowed(extId, "D:\\rawrxd\\projects\\..\\secret.txt", PermissionType::FileRead) == false);
    
    // 5. Write permission Isolation
    assert(sm.IsPathAllowed(extId, "D:\\rawrxd\\projects\\test.cpp", PermissionType::FileWrite) == false);
    
    std::cout << "Extension Sandbox Security Check: PASSED
";
    return 0;
}
