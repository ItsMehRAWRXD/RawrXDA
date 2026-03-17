/**
 * @file Win32Application.cpp
 * @brief Implementation of main application wiring system
 */

#include "Win32Application.hpp"
#include "Win32FileExplorer.hpp"
#include "Win32Editor.hpp"
#include "Win32Terminal.hpp"
#include "Win32Debugger.hpp"
#include "Win32SourceControl.hpp"
#include "Win32Extensions.hpp"
#include "Win32Search.hpp"
#include "Win32Run.hpp"
#include "Win32Help.hpp"
#include <windowsx.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <string>
#include <format>

namespace RawrXD::Win32 {

Win32Application::Win32Application()
    : m_agentKernel(std::make_shared<AgentKernel>())
{
}

Win32Application::~Win32Application()
{
    shutdown();
}

bool Win32Application::initialize()
{
    // Initialize agent kernel first
    if (!m_agentKernel->initialize()) {
        return false;
    }

    // Create main window
    createMainWindow();
    if (!m_mainWindow->isValid()) {
        return false;
    }

    // Create menu system
    m_mainMenu = std::make_unique<Win32Menu>();
    m_mainMenu->attachToWindow(m_mainWindow->getHandle());

    // Create settings system
    m_settings = std::make_unique<Win32Settings>();

    // Wire everything together
    wireFeaturesToFramework();
    wireFrameworkToKernel();
    wireKernelToFeatures();

    // Initialize hotpatching system
    initializeHotpatching();

    return true;
}

int Win32Application::run()
{
    return m_mainWindow->run();
}

void Win32Application::shutdown()
{
    if (m_mainWindow) {
        m_mainWindow->shutdown();
    }
    if (m_agentKernel) {
        m_agentKernel->shutdown();
    }
}

void Win32Application::wireFeaturesToFramework()
{
    // Register all features
    registerFeature("FileExplorer", [this]() { wireFileExplorerFeature(); });
    registerFeature("Editor", [this]() { wireEditorFeature(); });
    registerFeature("Terminal", [this]() { wireTerminalFeature(); });
    registerFeature("Debug", [this]() { wireDebugFeature(); });
    registerFeature("SourceControl", [this]() { wireSourceControlFeature(); });
    registerFeature("Extensions", [this]() { wireExtensionsFeature(); });
    registerFeature("Search", [this]() { wireSearchFeature(); });
    registerFeature("Run", [this]() { wireRunFeature(); });
    registerFeature("Help", [this]() { wireHelpFeature(); });

    // Wire framework components
    wireWindowFramework();
    wireMenuFramework();
    wireEventFramework();
    wireSettingsFramework();

    // Wire kernel components
    wireAgentKernel();
    wireFileSystemKernel();
    wireNetworkKernel();
    wireProcessKernel();
}

void Win32Application::wireFrameworkToKernel()
{
    // Wire window events to kernel
    m_mainWindow->setOnClose([this]() {
        m_agentKernel->shutdown();
        PostQuitMessage(0);
    });

    m_mainWindow->setOnResize([this](int width, int height) {
        m_agentKernel->setWindowSize(width, height);
    });

    // Wire menu commands to kernel
    wireMenuCommands();

    // Wire context menus
    wireContextMenus();

    // Wire settings
    wireSettings();
}

void Win32Application::wireKernelToFeatures()
{
    // Wire kernel events to features
    m_agentKernel->setOnFileChanged([this](const std::string& path) {
        // Notify file explorer and editor
        if (isFeatureEnabled("FileExplorer")) {
            // Update file explorer
        }
        if (isFeatureEnabled("Editor")) {
            // Update editor
        }
    });

    m_agentKernel->setOnProcessOutput([this](const std::string& output) {
        if (isFeatureEnabled("Terminal")) {
            // Send to terminal
        }
    });

    m_agentKernel->setOnAgentResponse([this](const std::string& response) {
        if (isFeatureEnabled("Editor")) {
            // Send to chat/inline suggestions
        }
    });
}

void Win32Application::registerFeature(const std::string& name, std::function<void()> initFunc)
{
    m_features[name] = initFunc;
    m_featureStates[name] = false;
}

void Win32Application::initializeFeature(const std::string& name)
{
    auto it = m_features.find(name);
    if (it != m_features.end() && !m_featureStates[name]) {
        it->second();
        m_featureStates[name] = true;
    }
}

bool Win32Application::isFeatureEnabled(const std::string& name) const
{
    auto it = m_featureStates.find(name);
    return it != m_featureStates.end() && it->second;
}

void Win32Application::wireMenuCommands()
{
    // File menu
    m_mainMenu->addMenuItem("File", {"New File", "Ctrl+N", 0, true, false, [this]() {
        newFile();
    }});

    m_mainMenu->addMenuItem("File", {"Open File...", "Ctrl+O", 0, true, false, [this]() {
        openFile();
    }});

    m_mainMenu->addMenuItem("File", {"Save", "Ctrl+S", 0, true, false, [this]() {
        saveFile();
    }});

    m_mainMenu->addMenuItem("File", {"Save As...", "Ctrl+Shift+S", 0, true, false, [this]() {
        saveFileAs();
    }});

    // Edit menu
    m_mainMenu->addMenuItem("Edit", {"Cut", "Ctrl+X", 0, true, false, []() {
        // Implement cut
    }});

    m_mainMenu->addMenuItem("Edit", {"Copy", "Ctrl+C", 0, true, false, []() {
        // Implement copy
    }});

    m_mainMenu->addMenuItem("Edit", {"Paste", "Ctrl+V", 0, true, false, []() {
        // Implement paste
    }});

    // View menu
    m_mainMenu->addMenuItem("View", {"Command Palette...", "Ctrl+Shift+P", 0, true, false, [this]() {
        showCommandPalette();
    }});

    m_mainMenu->addMenuItem("View", {"Open Chat", "", 0, true, false, [this]() {
        openChat();
    }});

    m_mainMenu->addMenuItem("View", {"Open Quick Chat", "", 0, true, false, [this]() {
        openQuickChat();
    }});

    m_mainMenu->addMenuItem("View", {"New Chat Editor", "", 0, true, false, [this]() {
        newChatEditor();
    }});

    m_mainMenu->addMenuItem("View", {"New Chat Window", "", 0, true, false, [this]() {
        newChatWindow();
    }});

    m_mainMenu->addMenuItem("View", {"Configure Inline Suggestions", "", 0, true, false, [this]() {
        configureInlineSuggestions();
    }});

    m_mainMenu->addMenuItem("View", {"Manage Chat", "", 0, true, false, [this]() {
        manageChat();
    }});

    // Help menu
    m_mainMenu->addMenuItem("Help", {"Settings", "", 0, true, false, [this]() {
        showSettings();
    }});

    // Build the complete menu
    m_mainMenu->createMainMenu();
}

void Win32Application::wireContextMenus()
{
    // File context menu wiring will be implemented in feature wiring
}

void Win32Application::wireSettings()
{
    // Add settings
    m_settings->addSetting({
        "editor.autoSave", "Auto Save", "Automatically save files when focus is lost",
        SettingValue(true), "Editor", [this](const SettingValue& value) {
            applySetting("editor.autoSave", value);
        }
    });

    m_settings->addSetting({
        "editor.wordWrap", "Word Wrap", "Enable word wrapping in editor",
        SettingValue(true), "Editor", [this](const SettingValue& value) {
            applySetting("editor.wordWrap", value);
        }
    });

    m_settings->addSetting({
        "terminal.shell", "Terminal Shell", "Default shell for terminal",
        SettingValue("powershell.exe"), "Terminal", [this](const SettingValue& value) {
            applySetting("terminal.shell", value);
        }
    });

    // Load settings from file
    m_settings->loadFromFile("settings.json");
}

void Win32Application::navigateToFeature(const std::string& feature)
{
    // Add to navigation stack
    if (m_currentNavIndex < m_navigationStack.size() - 1) {
        m_navigationStack.resize(m_currentNavIndex + 1);
    }
    m_navigationStack.push_back(feature);
    m_currentNavIndex = m_navigationStack.size() - 1;

    // Initialize feature if not already
    initializeFeature(feature);

    // Update breadcrumbs
    updateBreadcrumbs(feature);
}

void Win32Application::navigateBack()
{
    if (m_currentNavIndex > 0) {
        m_currentNavIndex--;
        updateBreadcrumbs(m_navigationStack[m_currentNavIndex]);
    }
}

void Win32Application::navigateForward()
{
    if (m_currentNavIndex < m_navigationStack.size() - 1) {
        m_currentNavIndex++;
        updateBreadcrumbs(m_navigationStack[m_currentNavIndex]);
    }
}

std::string Win32Application::getCurrentFeature() const
{
    if (m_navigationStack.empty()) return "";
    return m_navigationStack[m_currentNavIndex];
}

void Win32Application::updateBreadcrumbs(const std::string& path)
{
    m_breadcrumbPath = path;
    // Update UI breadcrumb display
    if (m_mainWindow) {
        m_mainWindow->setTitle("RawrXD IDE - " + path);
    }
}

std::string Win32Application::getBreadcrumbPath() const
{
    return m_breadcrumbPath;
}

void Win32Application::showCommandPalette()
{
    // Create command palette dialog
    HWND hwnd = m_mainWindow->getHandle();
    
    // Show quick command input
    // This would be implemented as a modal dialog or overlay
}

void Win32Application::executeCommand(const std::string& command)
{
    auto it = m_commands.find(command);
    if (it != m_commands.end()) {
        it->second();
    }
}

void Win32Application::wireChatSystem()
{
    // Register chat commands
    m_commands["chat.open"] = [this]() { openChat(); };
    m_commands["chat.quick"] = [this]() { openQuickChat(); };
    m_commands["chat.newEditor"] = [this]() { newChatEditor(); };
    m_commands["chat.newWindow"] = [this]() { newChatWindow(); };
    m_commands["chat.configureInline"] = [this]() { configureInlineSuggestions(); };
    m_commands["chat.manage"] = [this]() { manageChat(); };
}

void Win32Application::openChat()
{
    navigateToFeature("Chat");
    // Open main chat panel
}

void Win32Application::openQuickChat()
{
    // Open quick chat overlay
}

void Win32Application::newChatEditor()
{
    // Create new chat editor tab
}

void Win32Application::newChatWindow()
{
    // Create new chat window
}

void Win32Application::configureInlineSuggestions()
{
    // Show inline suggestions configuration
}

void Win32Application::manageChat()
{
    // Show chat management dialog
}

void Win32Application::wireFileOperations()
{
    m_commands["file.new"] = [this]() { newFile(); };
    m_commands["file.open"] = [this]() { openFile(); };
    m_commands["file.save"] = [this]() { saveFile(); };
    m_commands["file.saveAs"] = [this]() { saveFileAs(); };
    m_commands["file.reveal"] = [this]() { revealInExplorer(); };
    m_commands["file.copyPath"] = [this]() { copyFilePath(); };
}

void Win32Application::newFile()
{
    // Create new file dialog
    navigateToFeature("Editor");
    // Create new editor tab
}

void Win32Application::openFile()
{
    OPENFILENAMEA ofn;
    char fileName[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_mainWindow->getHandle();
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        navigateToFeature("Editor");
        // Open file in editor
    }
}

void Win32Application::saveFile()
{
    // Save current file
    if (isFeatureEnabled("Editor")) {
        // Call editor save
    }
}

void Win32Application::saveFileAs()
{
    OPENFILENAMEA ofn;
    char fileName[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_mainWindow->getHandle();
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameA(&ofn)) {
        // Save file
    }
}

void Win32Application::revealInExplorer()
{
    if (isFeatureEnabled("FileExplorer")) {
        // Reveal current file in explorer
    }
}

void Win32Application::copyFilePath()
{
    if (isFeatureEnabled("Editor")) {
        // Copy current file path to clipboard
        if (OpenClipboard(m_mainWindow->getHandle())) {
            EmptyClipboard();
            // Set clipboard data
            CloseClipboard();
        }
    }
}

void Win32Application::wireSettingsMenu()
{
    m_commands["settings.show"] = [this]() { showSettings(); };
}

void Win32Application::showSettings()
{
    m_settings->showSettingsDialog(m_mainWindow->getHandle());
}

void Win32Application::applySetting(const std::string& key, const SettingValue& value)
{
    // Apply setting to relevant features
    if (key == "editor.autoSave") {
        // Apply to editor
    } else if (key == "terminal.shell") {
        // Apply to terminal
    }
    
    // Save settings
    m_settings->saveToFile("settings.json");
}

HWND Win32Application::getMainWindow() const
{
    return m_mainWindow ? m_mainWindow->getHandle() : nullptr;
}

void Win32Application::setWindowTitle(const std::string& title)
{
    if (m_mainWindow) {
        m_mainWindow->setTitle(title);
    }
}

std::shared_ptr<AgentKernel> Win32Application::getAgentKernel()
{
    return m_agentKernel;
}

void Win32Application::createMainWindow()
{
    Win32Window::WindowConfig config;
    config.title = "RawrXD Sovereign IDE";
    config.width = 1200;
    config.height = 800;
    config.x = CW_USEDEFAULT;
    config.y = CW_USEDEFAULT;
    config.style = WS_OVERLAPPEDWINDOW;
    config.className = "RawrXDIDE";

    m_mainWindow = std::make_unique<Win32Window>(config);
    setupWindowCallbacks();
}

void Win32Application::setupWindowCallbacks()
{
    m_mainWindow->setOnClose([this]() {
        shutdown();
    });

    m_mainWindow->setOnResize([this](int width, int height) {
        // Handle window resize
    });

    m_mainWindow->setOnKeyDown([this](int keyCode, bool ctrl, bool shift, bool alt) {
        // Handle keyboard shortcuts
        if (ctrl && keyCode == 'N') {
            newFile();
        } else if (ctrl && keyCode == 'O') {
            openFile();
        } else if (ctrl && keyCode == 'S') {
            saveFile();
        } else if (ctrl && shift && keyCode == 'S') {
            saveFileAs();
        } else if (ctrl && shift && keyCode == 'P') {
            showCommandPalette();
        }
    });
}

LRESULT CALLBACK Win32Application::ApplicationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_COMMAND:
            // Handle menu commands
            break;
        case WM_CONTEXTMENU:
            // Handle context menus
            break;
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Feature wiring implementations
void Win32Application::wireFileExplorerFeature()
{
    // Implementation details
}

void Win32Application::wireEditorFeature()
{
    // Implementation details
}

void Win32Application::wireTerminalFeature()
{
    // Implementation details
}

void Win32Application::wireDebugFeature()
{
    // Implementation details
}

void Win32Application::wireSourceControlFeature()
{
    // Implementation details
}

void Win32Application::wireExtensionsFeature()
{
    // Implementation details
}

void Win32Application::wireSearchFeature()
{
    // Implementation details
}

void Win32Application::wireRunFeature()
{
    // Implementation details
}

void Win32Application::wireHelpFeature()
{
    // Implementation details
}

// Framework wiring implementations
void Win32Application::wireWindowFramework()
{
    // Window framework wiring
}

void Win32Application::wireMenuFramework()
{
    // Menu framework wiring
}

void Win32Application::wireEventFramework()
{
    // Event framework wiring
}

void Win32Application::wireSettingsFramework()
{
    // Settings framework wiring
}

// Kernel wiring implementations
void Win32Application::wireAgentKernel()
{
    // Agent kernel wiring
}

void Win32Application::wireFileSystemKernel()
{
    // File system kernel wiring
}

void Win32Application::wireNetworkKernel()
{
    // Network kernel wiring
}

void Win32Application::wireProcessKernel()
{
    // Process kernel wiring
}

void Win32Application::initializeHotpatching()
{
    // Initialize hotpatching system
}

void Win32Application::applyHotpatch(const std::string& patchId)
{
    // Apply hotpatch
}

} // namespace RawrXD::Win32