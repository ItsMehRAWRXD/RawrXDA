#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Hotpatch system for runtime code updates
class HotpatchSystem {
public:
    HotpatchSystem();
    ~HotpatchSystem();
    
    // Initialize the hotpatch system
    bool Initialize();
    
    // Apply a hotpatch
    bool ApplyPatch(const std::string& patch_id);
    
    // Revert a hotpatch
    bool RevertPatch(const std::string& patch_id);
    
    // Get available patches
    std::vector<std::string> GetAvailablePatches() const;
    
    // Get available patches (alias for GetAvailablePatches)
    std::vector<std::string> getAvailablePatches() const { return GetAvailablePatches(); }
    
    // Apply patch (alias for ApplyPatch)
    bool applyPatch(const std::string& patch_id) { return ApplyPatch(patch_id); }
    
    // Revert patch (alias for RevertPatch)
    bool revertPatch(const std::string& patch_id) { return RevertPatch(patch_id); }
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD