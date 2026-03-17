/**
 * @file Win32IDE_CommandBridge.cpp
 * @brief Bridge between Win32IDE legacy command system and CommandRegistry
 * 
 * This file integrates the production-ready CommandRegistry with the existing
 * Win32IDE command handling. It provides:
 * - Initialization of CommandRegistry on IDE startup
 * - Mapping of keyboard shortcuts to CommandRegistry
 * - Callback registration for command execution
 */

#include "Win32IDE.h"
#include "CommandRegistry.hpp"

namespace RawrXD::Win32 {

// ============================================================================
// COMMAND REGISTRY INITIALIZATION
// ============================================================================

void Win32IDE::initializeCommandRegistry()
{
    LOG_INFO("Initializing CommandRegistry...");
    
    // Initialize with main window handle
    CommandRegistry::instance().initialize(m_hwndMain);
    
    // Register IDE callbacks for commands that need Win32IDE context
    registerIDECallbacks();
    
    // Set workspace root if we have a current directory
    if (!m_currentExplorerPath.empty()) {
        CommandRegistry::instance().setWorkspaceRoot(m_currentExplorerPath);
    }
    
    LOG_INFO("CommandRegistry initialized successfully");
}

void Win32IDE::registerIDECallbacks()
{
    // These are commands that require Win32IDE context and can't be
    // fully handled in CommandRegistry alone
    
    auto& registry = CommandRegistry::instance();
    
    // File operations that need Win32IDE methods
    registry.setIDECallback(CommandID::FILE_NEW, [this]() { 
        newFile(); 
        updateTitleBarText();
    });
    
    registry.setIDECallback(CommandID::FILE_OPEN, [this]() { 
        openFile(); 
    });
    
    registry.setIDECallback(CommandID::FILE_SAVE, [this]() { 
        saveFile(); 
    });
    
    registry.setIDECallback(CommandID::FILE_SAVE_AS, [this]() { 
        saveFileAs(); 
    });
    
    registry.setIDECallback(CommandID::FILE_CLOSE, [this]() { 
        closeFile(); 
    });
    
    registry.setIDECallback(CommandID::FILE_EXIT, [this]() { 
        if (!m_fileModified || promptSaveChanges()) {
            PostQuitMessage(0);
        }
    });
    
    // Edit operations using editor handle
    registry.setIDECallback(CommandID::EDIT_UNDO, [this]() {
        if (m_hwndEditor) SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
    });
    
    registry.setIDECallback(CommandID::EDIT_REDO, [this]() {
        if (m_hwndEditor) SendMessage(m_hwndEditor, EM_REDO, 0, 0);
    });
    
    registry.setIDECallback(CommandID::EDIT_CUT, [this]() {
        if (m_hwndEditor) {
            SendMessage(m_hwndEditor, WM_CUT, 0, 0);
            m_fileModified = true;
        }
    });
    
    registry.setIDECallback(CommandID::EDIT_COPY, [this]() {
        if (m_hwndEditor) SendMessage(m_hwndEditor, WM_COPY, 0, 0);
    });
    
    registry.setIDECallback(CommandID::EDIT_PASTE, [this]() {
        if (m_hwndEditor) {
            SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
            m_fileModified = true;
        }
    });
    
    registry.setIDECallback(CommandID::EDIT_SELECT_ALL, [this]() {
        if (m_hwndEditor) SendMessage(m_hwndEditor, EM_SETSEL, 0, -1);
    });
    
    registry.setIDECallback(CommandID::EDIT_FIND, [this]() {
        showFindDialog();
    });
    
    registry.setIDECallback(CommandID::EDIT_REPLACE, [this]() {
        showReplaceDialog();
    });
    
    // View toggles
    registry.setIDECallback(CommandID::VIEW_EXPLORER, [this]() {
        toggleSidebar();
    });
    
    registry.setIDECallback(CommandID::VIEW_TERMINAL, [this]() {
        toggleTerminal();
    });
    
    registry.setIDECallback(CommandID::VIEW_OUTPUT, [this]() {
        toggleOutputPanel();
    });
    
    registry.setIDECallback(CommandID::VIEW_MINIMAP, [this]() {
        toggleMinimap();
    });
    
    // Terminal operations
    registry.setIDECallback(CommandID::TERMINAL_NEW, [this]() {
        startPowerShell();
    });
    
    registry.setIDECallback(CommandID::TERMINAL_NEW_POWERSHELL, [this]() {
        startPowerShell();
    });
    
    registry.setIDECallback(CommandID::TERMINAL_NEW_CMD, [this]() {
        startCommandPrompt();
    });
    
    registry.setIDECallback(CommandID::TERMINAL_CLEAR, [this]() {
        clearAllTerminals();
    });
    
    registry.setIDECallback(CommandID::TERMINAL_KILL, [this]() {
        stopTerminal();
    });

    // Splitting Terminals
    registry.setIDECallback(CommandID::TERMINAL_SPLIT, [this]() {
        splitTerminalHorizontal();
    });
    
    // Git Operations
    registry.setIDECallback(CommandID::VIEW_SOURCE_CONTROL, [this]() {
        showGitPanel();
    });
    
    registry.setIDECallback(CommandID::VIEW_SEARCH, [this]() {
        setSidebarView(SidebarView::Search);
    });

    registry.setIDECallback(CommandID::VIEW_DEBUG, [this]() {
        setSidebarView(SidebarView::RunDebug);
    });

    registry.setIDECallback(CommandID::VIEW_EXTENSIONS, [this]() {
        setSidebarView(SidebarView::Extensions);
    });
    
    // Help
    registry.setIDECallback(CommandID::HELP_ABOUT, [this]() {
        showAbout();
    });
    
    LOG_DEBUG("IDE callbacks registered with CommandRegistry");
}

// ============================================================================
// KEYBOARD SHORTCUT INTEGRATION
// ============================================================================

bool Win32IDE::handleKeyboardShortcutViaRegistry(UINT vkCode, bool ctrl, bool shift, bool alt)
{
    // Try to handle via CommandRegistry first
    if (CommandRegistry::instance().handleKeyboardShortcut(vkCode, ctrl, shift, alt)) {
        LOG_DEBUG("Keyboard shortcut handled by CommandRegistry");
        return true;
    }
    return false;
}

// ============================================================================
// COMMAND ROUTING INTEGRATION
// ============================================================================

bool Win32IDE::routeCommandViaRegistry(UINT commandId)
{
    // Try CommandRegistry first for unified command handling
    if (CommandRegistry::instance().execute(commandId)) {
        LOG_DEBUG("Command " + std::to_string(commandId) + " handled by CommandRegistry");
        return true;
    }
    return false;
}

// ============================================================================
// SETTINGS REGISTRY INTEGRATION
// ============================================================================

void Win32IDE::initializeSettingsRegistry()
{
    LOG_INFO("Initializing SettingsRegistry...");
    
    SettingsRegistry::instance().initialize();
    
    // Load settings from Windows Registry
    SettingsRegistry::instance().loadFromRegistry();
    
    // Apply settings to IDE
    applySettingsFromRegistry();
    
    LOG_INFO("SettingsRegistry initialized");
}

void Win32IDE::applySettingsFromRegistry()
{
    auto& settings = SettingsRegistry::instance();
    
    // Apply editor settings
    int fontSize = settings.getInt("editor.fontSize", 14);
    std::string fontFamily = settings.getString("editor.fontFamily", "Consolas");
    bool wordWrap = settings.getString("editor.wordWrap", "off") == "on";
    bool minimap = settings.getBool("editor.minimap.enabled", true);
    
    // Apply to editor if it exists
    if (m_hwndEditor) {
        // Update font
        HFONT hFont = CreateFontA(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, fontFamily.c_str());
        if (hFont) {
            SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }
    
    // Apply minimap visibility
    if (m_hwndMinimap) {
        ShowWindow(m_hwndMinimap, minimap ? SW_SHOW : SW_HIDE);
        m_minimapVisible = minimap;
    }
    
    // Apply window settings
    bool darkMode = settings.getBool("window.theme", true);
    if (darkMode != m_currentTheme.darkMode) {
        // Theme change would require refresh
    }
    
    LOG_DEBUG("Settings applied to IDE");
}

void Win32IDE::saveSettingsToRegistry()
{
    auto& settings = SettingsRegistry::instance();
    
    // Save current state
    settings.set("editor.minimap.enabled", m_minimapVisible);
    settings.set("window.sidebarVisible", m_sidebarVisible);
    
    // Persist to Windows Registry
    settings.saveToRegistry();
    
    LOG_INFO("Settings saved to registry");
}

} // namespace RawrXD::Win32
