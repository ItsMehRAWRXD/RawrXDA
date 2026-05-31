// ============================================================================
// advanced_docking_demo.cpp - Demo Application for Advanced Docking System
// ============================================================================

#include "../ui/advanced_docking_system.h"
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace RawrXD::UI;

// ============================================================================
// Demo Application
// ============================================================================
class DockingDemoApp {
public:
    bool initialize(HINSTANCE hInstance) {
        // Register window class
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = windowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"RawrXD_DockingDemo";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        
        if (!RegisterClassEx(&wc)) {
            MessageBoxA(nullptr, "Failed to register window class", "Error", MB_OK);
            return false;
        }
        
        // Create main window
        hwnd_ = CreateWindowEx(
            0, L"RawrXD_DockingDemo", L"RawrXD Advanced Docking Demo",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
            nullptr, nullptr, hInstance, this
        );
        
        if (!hwnd_) {
            MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK);
            return false;
        }
        
        // Initialize docking manager
        auto& docking = DockingManager::instance();
        
        // Configure docking
        DockingConfig config;
        config.leftSidebarWidth = 280;
        config.rightSidebarWidth = 300;
        config.bottomPanelHeight = 250;
        config.leftSidebarState = PanelState::Expanded;
        config.rightSidebarState = PanelState::Collapsed;
        config.bottomPanelState = PanelState::Collapsed;
        config.showTabCloseButtons = true;
        config.enableTabDragDrop = true;
        config.showTabIcons = true;
        config.restoreLayoutOnStartup = true;
        config.layoutFilePath = "demo_docking_layout.json";
        docking.setConfig(config);
        
        // Initialize
        if (!docking.initialize(hwnd_)) {
            MessageBoxA(nullptr, "Failed to initialize docking system", "Error", MB_OK);
            return false;
        }
        
        // Setup panels
        setupPanels();
        
        // Setup main tab group
        setupMainTabGroup();
        
        // Update layout
        docking.updateLayout();
        
        return true;
    }
    
    void run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    void shutdown() {
        DockingManager::instance().shutdown();
        if (hwnd_) DestroyWindow(hwnd_);
    }
    
private:
    HWND hwnd_ = nullptr;
    
    void setupPanels() {
        auto& docking = DockingManager::instance();
        
        // Left panel - Explorer
        if (auto* leftPanel = docking.getPanel(DockZone::Left)) {
            leftPanel->setTitle("Explorer");
            
            // Create explorer content
            HWND explorerHwnd = CreateWindowEx(
                0, L"STATIC", L"Explorer Content",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
                0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
            );
            leftPanel->setContent(explorerHwnd);
            
            // Add tab group to explorer
            auto* tabGroup = leftPanel->createTabGroup("explorer_tabs");
            tabGroup->addTab(std::make_shared<TabItem>("files", "Files", explorerHwnd));
            tabGroup->addTab(std::make_shared<TabItem>("outline", "Outline", nullptr));
            tabGroup->addTab(std::make_shared<TabItem>("timeline", "Timeline", nullptr));
        }
        
        // Right panel - Properties
        if (auto* rightPanel = docking.getPanel(DockZone::Right)) {
            rightPanel->setTitle("Properties");
            
            HWND propsHwnd = CreateWindowEx(
                0, L"STATIC", L"Properties Content",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
                0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
            );
            rightPanel->setContent(propsHwnd);
            
            auto* tabGroup = rightPanel->createTabGroup("properties_tabs");
            tabGroup->addTab(std::make_shared<TabItem>("properties", "Properties", propsHwnd));
        }
        
        // Bottom panel - Terminal
        if (auto* bottomPanel = docking.getPanel(DockZone::Bottom)) {
            bottomPanel->setTitle("Terminal");
            
            HWND terminalHwnd = CreateWindowEx(
                0, L"EDIT", L"Terminal Output\r\n> ",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | 
                WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
            );
            bottomPanel->setContent(terminalHwnd);
            
            auto* tabGroup = bottomPanel->createTabGroup("terminal_tabs");
            tabGroup->addTab(std::make_shared<TabItem>("terminal", "Terminal", terminalHwnd));
            tabGroup->addTab(std::make_shared<TabItem>("output", "Output", nullptr));
            tabGroup->addTab(std::make_shared<TabItem>("debug", "Debug Console", nullptr));
        }
    }
    
    void setupMainTabGroup() {
        auto& docking = DockingManager::instance();
        
        if (auto* mainGroup = docking.getMainTabGroup()) {
            // Create editor windows
            HWND editor1 = CreateWindowEx(
                0, L"EDIT", L"// Main Editor\r\n// Welcome to RawrXD Advanced Docking\r\n\r\n"
                           L"Features:\r\n"
                           L"- Tab Groups with drag-drop\r\n"
                           L"- Collapsible Side Bars\r\n"
                           L"- Bottom Panels (Terminal/Output)\r\n"
                           L"- Split-pane layout\r\n"
                           L"- State persistence\r\n\r\n"
                           L"Try:\r\n"
                           L"- F1: Toggle Left Panel\r\n"
                           L"- F2: Toggle Right Panel\r\n"
                           L"- F3: Toggle Bottom Panel\r\n"
                           L"- Ctrl+T: New Tab\r\n"
                           L"- Ctrl+W: Close Tab\r\n"
                           L"- Drag tabs to reorder",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | 
                WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
            );
            
            HWND editor2 = CreateWindowEx(
                0, L"EDIT", L"// Secondary Editor",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | 
                WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
            );
            
            // Add tabs
            mainGroup->addTab(std::make_shared<TabItem>("main.cpp", "main.cpp", editor1));
            mainGroup->addTab(std::make_shared<TabItem>("README.md", "README.md", editor2));
            
            // Create the tab group window
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            RECT centerRect = {
                clientRect.left + 280,  // After left panel
                clientRect.top,
                clientRect.right - 300, // Before right panel
                clientRect.bottom - 250 // Before bottom panel
            };
            mainGroup->create(hwnd_, centerRect);
        }
    }
    
    void handleCommand(int id) {
        auto& docking = DockingManager::instance();
        
        switch (id) {
            case 1: // Toggle Left Panel
                docking.togglePanel(DockZone::Left);
                break;
            case 2: // Toggle Right Panel
                docking.togglePanel(DockZone::Right);
                break;
            case 3: // Toggle Bottom Panel
                docking.togglePanel(DockZone::Bottom);
                break;
            case 4: // Reset Layout
                docking.resetLayout();
                break;
            case 5: // Save Layout
                docking.saveLayout();
                MessageBoxA(hwnd_, "Layout saved!", "Info", MB_OK);
                break;
            case 6: // New Tab
                if (auto* mainGroup = docking.getMainTabGroup()) {
                    static int newTabCount = 0;
                    newTabCount++;
                    std::string title = "untitled-" + std::to_string(newTabCount) + ".txt";
                    
                    HWND editor = CreateWindowEx(
                        0, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_MULTILINE | 
                        WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                        0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr
                    );
                    
                    mainGroup->addTab(std::make_shared<TabItem>(title, title, editor));
                }
                break;
        }
    }
    
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        DockingDemoApp* app = nullptr;
        
        if (msg == WM_CREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<DockingDemoApp*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<DockingDemoApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }
        
        // Let docking manager handle messages first
        auto& docking = DockingManager::instance();
        LRESULT result = docking.handleMessage(hwnd, msg, wParam, lParam);
        if (result != 0) return result;
        
        switch (msg) {
            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED) {
                    docking.updateLayout();
                }
                return 0;
                
            case WM_COMMAND:
                if (app && HIWORD(wParam) == 0) {
                    app->handleCommand(LOWORD(wParam));
                }
                return 0;
                
            case WM_KEYDOWN:
                switch (wParam) {
                    case VK_F1:
                        if (app) app->handleCommand(1);
                        return 0;
                    case VK_F2:
                        if (app) app->handleCommand(2);
                        return 0;
                    case VK_F3:
                        if (app) app->handleCommand(3);
                        return 0;
                    case 'T':
                        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                            if (app) app->handleCommand(6);
                            return 0;
                        }
                        break;
                    case 'W':
                        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                            // Close active tab
                            if (auto* mainGroup = docking.getMainTabGroup()) {
                                if (auto* activeTab = mainGroup->getActiveTab()) {
                                    mainGroup->removeTab(activeTab->id);
                                }
                            }
                            return 0;
                        }
                        break;
                }
                break;
                
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
                
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                // Fill background
                RECT rect;
                GetClientRect(hwnd, &rect);
                FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
                
                EndPaint(hwnd, &ps);
                return 0;
            }
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
};

// ============================================================================
// Entry Point
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    // Create demo application
    DockingDemoApp app;
    
    if (!app.initialize(hInstance)) {
        return 1;
    }
    
    // Show instructions
    MessageBoxA(nullptr,
        "RawrXD Advanced Docking System Demo\n\n"
        "Controls:\n"
        "  F1 - Toggle Left Panel (Explorer)\n"
        "  F2 - Toggle Right Panel (Properties)\n"
        "  F3 - Toggle Bottom Panel (Terminal)\n"
        "  Ctrl+T - New Tab\n"
        "  Ctrl+W - Close Active Tab\n\n"
        "Features:\n"
        "  - Drag tabs to reorder\n"
        "  - Click panel titles to collapse/expand\n"
        "  - Resize panels by dragging gripper\n"
        "  - Layout auto-saves on exit",
        "Advanced Docking Demo",
        MB_OK | MB_ICONINFORMATION);
    
    // Run message loop
    app.run();
    
    // Cleanup
    app.shutdown();
    
    return 0;
}
