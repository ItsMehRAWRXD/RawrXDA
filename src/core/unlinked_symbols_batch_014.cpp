// Fallback implementations for GUI components not available in headless builds.
#include <memory>

// Forward declare the classes to satisfy the compiler for the unique_ptr types.
class OutlinePanel {};
class DockingPaneManager {};

namespace Win32IDE {
    std::unique_ptr<OutlinePanel> createOutlinePanel() { 
        // Return a minimal implementation instead of nullptr
        return std::make_unique<OutlinePanel>(); 
    }
    std::unique_ptr<DockingPaneManager> createDockingPaneManager() { 
        // Return a minimal implementation instead of nullptr
        return std::make_unique<DockingPaneManager>(); 
    }
    void goToLine(int line, int col) {
        // No-op in headless mode — maintains API compatibility
        (void)line; (void)col;
    }
}
