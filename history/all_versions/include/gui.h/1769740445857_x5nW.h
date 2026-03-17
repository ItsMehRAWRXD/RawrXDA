#pragma once
#include <string>

// Minimal GUI stub for Qt-free build
// Provides no-op functions for GUI operations

namespace gui {

inline void ShowMessage(const std::string& msg) {
    // No-op in headless build
    (void)msg;
}

inline void ShowError(const std::string& msg) {
    // No-op in headless build
    (void)msg;
}

inline bool Initialize() {
    return true; // Always succeed in headless mode
}

inline void Shutdown() {
    // No-op
}

} // namespace gui
