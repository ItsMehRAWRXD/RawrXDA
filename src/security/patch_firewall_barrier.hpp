/**
 * @file patch_firewall_barrier.hpp
 * @brief Patch Firewall Phase 1: Data Plane Isolation
 * Enforces HIDDEN+SYSTEM attributes on sensitive directories at startup
 * to prevent VS Code patch-storm freeze.
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace rawrxd::security {

class PatchFirewallBarrier {
public:
    /// Initialize the patch firewall (call once at app startup)
    static bool initialize();
    
    /// Verify that data plane directories have HIDDEN+SYSTEM attributes
    static bool verify_attributes();
    
private:
    // Directories that must be invisible to file crawlers
    static constexpr const char* DATA_PLANE_DIRS[] = {
        "D:\\rawrxd\\OllamaModels",
        "D:\\rawrxd\\blobs",
        "F:\\OllamaModels",
    };
    
    static constexpr int DATA_PLANE_DIR_COUNT = 3;
    
    // FILE_ATTRIBUTE_HIDDEN (0x2) | FILE_ATTRIBUTE_SYSTEM (0x4) = 0x6
    static constexpr DWORD GHOST_ATTRIBUTES = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    
    /// Apply HIDDEN+SYSTEM attributes to a single directory
    static bool apply_ghost_attributes(const std::string& dir_path);
    
    /// Check if a directory has HIDDEN+SYSTEM attributes
    static bool has_ghost_attributes(const std::string& dir_path);
};

} // namespace rawrxd::security
