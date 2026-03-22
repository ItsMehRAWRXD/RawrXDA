/**
 * @file patch_firewall_barrier.cpp
 * @brief Implementation of Patch Firewall Phase 1
 */

#include "patch_firewall_barrier.hpp"

#include <iostream>

namespace rawrxd::security {

bool PatchFirewallBarrier::apply_ghost_attributes(const std::string& dir_path) {
    // Check if directory exists
    DWORD attribs = GetFileAttributesA(dir_path.c_str());
    DWORD error = (attribs == INVALID_FILE_ATTRIBUTES) ? GetLastError() : ERROR_SUCCESS;
    if (attribs == INVALID_FILE_ATTRIBUTES) {

        // Treat "not found" cases as non-fatal and skip silently
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true;
        }

        // Any other error is a real failure that should not be masked
        std::cerr << "[PATCH-FIREWALL] Failed to get attributes for " << dir_path
                  << " (error: " << error << ")\n";
        return false;
    }
    
    // Apply HIDDEN + SYSTEM attributes while preserving existing flags
    BOOL success = SetFileAttributesA(dir_path.c_str(), attribs | GHOST_ATTRIBUTES);
    
    if (!success) {
        std::cerr << "[PATCH-FIREWALL] Failed to set attributes on " << dir_path 
                  << " (error: " << GetLastError() << ")\n";
        return false;
    }
    
    return true;
}

bool PatchFirewallBarrier::has_ghost_attributes(const std::string& dir_path) {
    DWORD attribs = GetFileAttributesA(dir_path.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    return (attribs & GHOST_ATTRIBUTES) == GHOST_ATTRIBUTES;
}

bool PatchFirewallBarrier::initialize() {
    bool all_success = true;
    

    for (int i = 0; i < DATA_PLANE_DIR_COUNT; ++i) {
        const std::string& dir = DATA_PLANE_DIRS[i];
        
        bool applied = apply_ghost_attributes(dir);
        if (!applied) {
            all_success = false;
        } else {
            std::cout << "[PATCH-FIREWALL]   ✓ " << dir << " (HIDDEN+SYSTEM applied)\n";
        }
    }
    
    if (all_success) {
        std::cout << "[PATCH-FIREWALL] Phase 1 complete: Data plane is invisible\n";
    }
    
    return all_success;
}

bool PatchFirewallBarrier::verify_attributes() {
    bool all_verified = true;
    
    for (int i = 0; i < DATA_PLANE_DIR_COUNT; ++i) {
        const std::string& dir = DATA_PLANE_DIRS[i];
        
        if (!has_ghost_attributes(dir)) {
            std::cerr << "[PATCH-FIREWALL] WARN: " << dir << " does not have HIDDEN+SYSTEM\n";
            all_verified = false;
        }
    }
    
    return all_verified;
}

} // namespace rawrxd::security
