/**
 * @file Win32Application.hpp
 * @brief Main application class that wires features -> framework -> kernel
 */

#pragma once

#include "Win32Window.hpp"
#include "Win32Menu.hpp"
#include "Win32ContextMenu.hpp"
#include "Win32Settings.hpp"
#include "../agent/RawrXD_AgentKernel.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace RawrXD::Win32 {

class Win32Application {
public:
    Win32Application();
    ~Win32Application();

    // Application lifecycle
    bool initialize();
    int run();
    void shutdown();

    // Feature wiring
    void wireFeaturesToFramework();
    void wireFrameworkToKernel();
    void wireKernelToFeatures();

    // Feature management
    void registerFeature(const std::string& name, std::function<void()> initFunc);
    void initializeFeature(const std::string& name);
    bool isFeatureEnabled(const std::string& name) const;

    // Menu wiring
    void wireMenuCommands();
    void wireContextMenus();
    void wireSettings();

    // Navigation flow
    void navigateToFeature(const std::string& feature);
    void navigateBack();
    void navigateForward();
    std::string getCurrentFeature() const;

    // Breadcrumb system
    void updateBreadcrumbs(const std::string& path);
    std::string getBreadcrumbPath() const;

    // Command palette
    void showCommandPalette();
    void executeCommand(const std::string& command);

    // Chat system wiring
    void wireChatSystem();
    void openChat();
    void openQuickChat();
    void newChatEditor();
    void newChatWindow();
    void configureInlineSuggestions();
    void manageChat();

    // File operations wiring
    void wireFileOperations();
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void revealInExplorer();
    void copyFilePath();

    // Settings wiring
    void wireSettingsMenu();
    void showSettings();
    void applySetting(const std::string& key, const SettingValue& value);

    // Window management
    HWND getMainWindow() const;
    void setWindowTitle(const std::string& title);

    // Agent kernel access
    std::shared_ptr<AgentKernel> getAgentKernel();

private:
    std::unique_ptr<Win32Window> m_mainWindow;
    std::unique_ptr<Win32Menu> m_mainMenu;
    std::unique_ptr<Win32Settings> m_settings;
    std::shared_ptr<AgentKernel> m_agentKernel;

    // Feature registry
    std::unordered_map<std::string, std::function<void()>> m_features;
    std::unordered_map<std::string, bool> m_featureStates;

    // Navigation stack
    std::vector<std::string> m_navigationStack;
    size_t m_currentNavIndex = 0;

    // Breadcrumb path
    std::string m_breadcrumbPath;

    // Command registry
    std::unordered_map<std::string, std::function<void()>> m_commands;

    // Window creation
    void createMainWindow();
    void setupWindowCallbacks();

    // Message handling
    static LRESULT CALLBACK ApplicationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Feature wiring implementations
    void wireFileExplorerFeature();
    void wireEditorFeature();
    void wireTerminalFeature();
    void wireDebugFeature();
    void wireSourceControlFeature();
    void wireExtensionsFeature();
    void wireSearchFeature();
    void wireRunFeature();
    void wireHelpFeature();

    // Framework wiring implementations
    void wireWindowFramework();
    void wireMenuFramework();
    void wireEventFramework();
    void wireSettingsFramework();

    // Kernel wiring implementations
    void wireAgentKernel();
    void wireFileSystemKernel();
    void wireNetworkKernel();
    void wireProcessKernel();

    // Hotpatching system
    void initializeHotpatching();
    void applyHotpatch(const std::string& patchId);
};

} // namespace RawrXD::Win32