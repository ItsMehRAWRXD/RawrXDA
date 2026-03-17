// unified_hotpatch_manager.cpp — Implementation
// Converted from Qt (QObject signals) to plain C++17

#include "unified_hotpatch_manager.hpp"
#include <iostream>

// All implementation is inline in the header via method bodies.
// This .cpp exists for additional functionality and to maintain
// the same file structure as the original Qt version.

// Additional helper: batch patching
PatchResult batchHotpatch(UnifiedHotpatchManager& manager,
                          const std::vector<std::pair<std::string, std::string>>& patches) {
    int successCount = 0;
    int failCount = 0;

    for (const auto& [name, region] : patches) {
        auto result = manager.performHotpatch(name, region, {});
        if (result.success) successCount++;
        else failCount++;
    }

    if (failCount == 0) {
        return PatchResult::ok("All " + std::to_string(successCount) + " patches applied");
    }
    return PatchResult::error(std::to_string(failCount) + " of " +
                              std::to_string(patches.size()) + " patches failed");
}
