// ============================================================================
// win32ide_docking_integration.cpp - Win32IDE Advanced Docking Integration
// ============================================================================
// Example integration showing how to wire the Advanced Docking System into
// the Win32IDE main window.
//
// Usage: Include this in Win32IDE_Core.cpp or similar initialization file
// ============================================================================

#include "../ui/advanced_docking_system.h"
#include <windows.h>
#include <iostream>

using namespace RawrXD::UI;

namespace RawrXD {
namespace Win32IDE {

// ============================================================================
// Docking Integration Class
// ============================================================================
class DockingIntegration {
public:
    static DockingIntegration& instance() {
        static DockingIntegration inst;
        return inst;
    }

    bool initialize(HWND hwndMain) {
        if (initialized_) return true;

        hwndMain_ = hwndMain;

        // Initialize the docking manager
        auto& manager = DockingManager::instance();
        if (!manager.initialize(hwndMain)) {
            std::cerr << "[DockingIntegration] Failed to initialize DockingManager\n";
            return false;
        }

        // Configure default layout
        auto& config = manager.getConfig();
        config.leftSidebarWidth = 250;
        config.rightSidebarWidth = 250;
        config.bottomPanelHeight = 200;
        config.showTabCloseButtons = true;
        config.enableTabDragDrop = true;
        config.restoreLayoutOnStartup = true;
        config.layoutFilePath = "win32ide_layout.json";

        // Create side panels
        createSidePanels();

        // Create bottom panel
        createBottomPanel();

        // Setup main editor area
        setupMainEditorArea();

        // Restore previous layout if available
        manager.restoreLayout();

        // Register menu handlers
        registerMenuHandlers();

        initialized_ = true;
        std::cout << "[DockingIntegration] Initialized successfully\n";
        return true;
    }

    void shutdown() {
        if (!initialized_) return;

        // Save current layout
        auto& manager = DockingManager::instance();
        manager.saveLayout();
        manager.shutdown();

        initialized_ = false;
        std::cout << "[DockingIntegration] Shutdown complete\n";
    }

    void updateLayout() {
        if (!initialized_) return;
        DockingManager::instance().updateLayout();
    }

    // Menu command handlers
    void onViewLeftSidebar() {
        auto& manager = DockingManager::instance();
        manager.togglePanel(DockZone::Left);
    }

    void onViewRightSidebar() {
        auto& manager = DockingManager::instance();
        manager.togglePanel(DockZone::Right);
    }

    void onViewBottomPanel() {
        auto& manager = DockingManager::instance();
        manager.togglePanel(DockZone::Bottom);
    }

    void onViewResetLayout() {
        auto& manager = DockingManager::instance();
        manager.resetLayout();
    }

    void onFileNew() {
        auto* mainGroup = DockingManager::instance().getMainTabGroup();
        if (!mainGroup) return;

        auto tab = std::make_shared<TabItem>(
            generateTabId(),
            "Untitled-" + std::to_string(untitledCount_++),
            nullptr  // Will be set when editor created
        );
        tab->isModified = false;
        tab->onCloseCallback = [tab]() {
            // Handle tab close
        };
        tab->onActivateCallback = [tab]() {
            // Handle tab activation
        };

        mainGroup->addTab(tab);
        mainGroup->activateTab(tab->id);
    }

    void onFileOpen(const std::string& filePath) {
        auto* mainGroup = DockingManager::instance().getMainTabGroup();
        if (!mainGroup) return;

        // Check if already open
        auto existingTab = mainGroup->getTab(filePath);
        if (existingTab) {
            mainGroup->activateTab(filePath);
            return;
        }

        auto tab = std::make_shared<TabItem>(
            filePath,
            getFileName(filePath),
            nullptr  // Will be set when editor created
        );
        tab->isModified = false;
        tab->tooltip = filePath;

        mainGroup->addTab(tab);
        mainGroup->activateTab(filePath);
    }

    // Window message handler (call from WndProc)
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (!initialized_) return DefWindowProc(hwnd, msg, wParam, lParam);

        // Let docking manager handle layout-related messages
        auto& manager = DockingManager::instance();
        return manager.handleMessage(hwnd, msg, wParam, lParam);
    }

    // Getters for external access
    HWND getMainWindow() const { return hwndMain_; }
    bool isInitialized() const { return initialized_; }

private:
    DockingIntegration() = default;
    ~DockingIntegration() = default;
    DockingIntegration(const DockingIntegration&) = delete;
    DockingIntegration& operator=(const DockingIntegration&) = delete;

    void createSidePanels() {
        auto& manager = DockingManager::instance();

        // Left sidebar - Explorer
        auto* leftPanel = manager.createPanel(DockZone::Left, "explorer");
        if (leftPanel) {
            leftPanel->setTitle("Explorer");
            leftPanel->setState(PanelState::Expanded);

            // Add file explorer content
            auto* tabGroup = leftPanel->createTabGroup("explorer_tabs");
            if (tabGroup) {
                auto tab = std::make_shared<TabItem>("files", "Files", nullptr);
                tabGroup->addTab(tab);
            }
        }

        // Right sidebar - Properties/Outline
        auto* rightPanel = manager.createPanel(DockZone::Right, "properties");
        if (rightPanel) {
            rightPanel->setTitle("Properties");
            rightPanel->setState(PanelState::Collapsed);

            auto* tabGroup = rightPanel->createTabGroup("properties_tabs");
            if (tabGroup) {
                auto outlineTab = std::make_shared<TabItem>("outline", "Outline", nullptr);
                tabGroup->addTab(outlineTab);
            }
        }
    }

    void createBottomPanel() {
        auto& manager = DockingManager::instance();

        auto* bottomPanel = manager.createPanel(DockZone::Bottom, "bottom");
        if (bottomPanel) {
            bottomPanel->setTitle("Panel");
            bottomPanel->setState(PockState::Collapsed);

            auto* tabGroup = bottomPanel->createTabGroup("bottom_tabs");
            if (tabGroup) {
                auto terminalTab = std::make_shared<TabItem>("terminal", "Terminal", nullptr);
                auto outputTab = std::make_shared<TabItem>("output", "Output", nullptr);
                auto debugTab = std::make_shared<TabItem>("debug", "Debug Console", nullptr);

                tabGroup->addTab(terminalTab);
                tabGroup->addTab(outputTab);
                tabGroup->addTab(debugTab);
            }
        }
    }

    void setupMainEditorArea() {
        auto& manager = DockingManager::instance();

        // Get the main tab group (center area)
        auto* mainGroup = manager.getMainTabGroup();
        if (mainGroup) {
            // Create initial welcome tab
            auto welcomeTab = std::make_shared<TabItem>(
                "welcome",
                "Welcome",
                nullptr
            );
            welcomeTab->isPinned = true;
            mainGroup->addTab(welcomeTab);
            mainGroup->activateTab("welcome");
        }
    }

    void registerMenuHandlers() {
        // These would connect to actual menu commands in the full implementation
        // For now, just log that handlers are registered
        std::cout << "[DockingIntegration] Menu handlers registered\n";
    }

    std::string generateTabId() {
        static int counter = 0;
        return "tab_" + std::to_string(++counter);
    }

    std::string getFileName(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }

    // State
    bool initialized_ = false;
    HWND hwndMain_ = nullptr;
    int untitledCount_ = 1;
};

// ============================================================================
// C API for easy integration
// ============================================================================

extern "C" {

__declspec(dllexport) bool RawrXD_Docking_Initialize(HWND hwndMain) {
    return DockingIntegration::instance().initialize(hwndMain);
}

__declspec(dllexport) void RawrXD_Docking_Shutdown() {
    DockingIntegration::instance().shutdown();
}

__declspec(dllexport) void RawrXD_Docking_UpdateLayout() {
    DockingIntegration::instance().updateLayout();
}

__declspec(dllexport) void RawrXD_Docking_ToggleLeftSidebar() {
    DockingIntegration::instance().onViewLeftSidebar();
}

__declspec(dllexport) void RawrXD_Docking_ToggleRightSidebar() {
    DockingIntegration::instance().onViewRightSidebar();
}

__declspec(dllexport) void RawrXD_Docking_ToggleBottomPanel() {
    DockingIntegration::instance().onViewBottomPanel();
}

__declspec(dllexport) void RawrXD_Docking_ResetLayout() {
    DockingIntegration::instance().onViewResetLayout();
}

__declspec(dllexport) void RawrXD_Docking_NewFile() {
    DockingIntegration::instance().onFileNew();
}

__declspec(dllexport) void RawrXD_Docking_OpenFile(const char* filePath) {
    if (filePath) {
        DockingIntegration::instance().onFileOpen(filePath);
    }
}

__declspec(dllexport) LRESULT RawrXD_Docking_HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DockingIntegration::instance().handleMessage(hwnd, msg, wParam, lParam);
}

} // extern "C"

} // namespace Win32IDE
} // namespace RawrXD

// ============================================================================
// Integration Example (for Win32IDE_Core.cpp)
// ============================================================================
/*
// In Win32IDE::Initialize():
#include "win32ide_docking_integration.cpp"

bool Win32IDE::Initialize() {
    // ... existing initialization ...
    
    // Initialize advanced docking
    if (!RawrXD::Win32IDE::DockingIntegration::instance().initialize(hwndMain_)) {
        LogError("Failed to initialize docking system");
        return false;
    }
    
    // ... rest of initialization ...
    return true;
}

// In Win32IDE WndProc:
LRESULT CALLBACK Win32IDE::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let docking system handle layout messages first
    LRESULT result = RawrXD::Win32IDE::DockingIntegration::instance().handleMessage(hwnd, msg, wParam, lParam);
    if (result != 0) return result;
    
    // ... rest of message handling ...
}

// Menu handlers:
void Win32IDE::OnViewLeftSidebar() {
    RawrXD::Win32IDE::DockingIntegration::instance().onViewLeftSidebar();
}

void Win32IDE::OnViewRightSidebar() {
    RawrXD::Win32IDE::DockingIntegration::instance().onViewRightSidebar();
}

void Win32IDE::OnViewBottomPanel() {
    RawrXD::Win32IDE::DockingIntegration::instance().onViewBottomPanel();
}

void Win32IDE::OnFileNew() {
    RawrXD::Win32IDE::DockingIntegration::instance().onFileNew();
}

void Win32IDE::OnFileOpen(const std::string& path) {
    RawrXD::Win32IDE::DockingIntegration::instance().onFileOpen(path);
}
*/
