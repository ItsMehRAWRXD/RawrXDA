/**
 * @file Win32Editor.hpp
 * @brief Code Editor component for Win32 IDE
 */

#pragma once

#include "Win32Window.hpp"
#include "Win32ContextMenu.hpp"
#include "../agent/RawrXD_AgentKernel.hpp"
#include <string>
#include <vector>
#include <functional>

namespace RawrXD::Win32 {

struct EditorTab {
    std::string title;
    std::string filePath;
    bool modified;
    bool pinned;
};

class Win32Editor {
public:
    Win32Editor(std::shared_ptr<AgentKernel> agentKernel);
    ~Win32Editor();

    // Tab management
    bool openFile(const std::string& filePath);
    bool closeFile(const std::string& filePath);
    bool saveFile(const std::string& filePath);
    bool saveFileAs(const std::string& oldPath, const std::string& newPath);
    void closeAllFiles();
    void closeOtherFiles(const std::string& keepPath);

    // Editor operations
    void setText(const std::string& filePath, const std::string& text);
    std::string getText(const std::string& filePath) const;
    void setSelection(const std::string& filePath, int start, int end);
    std::pair<int, int> getSelection(const std::string& filePath) const;
    void gotoLine(const std::string& filePath, int line);
    int getCurrentLine(const std::string& filePath) const;

    // Find/replace
    bool findText(const std::string& filePath, const std::string& text, bool caseSensitive = false);
    bool replaceText(const std::string& filePath, const std::string& find, const std::string& replace);
    int replaceAll(const std::string& filePath, const std::string& find, const std::string& replace);

    // Syntax highlighting
    void setLanguage(const std::string& filePath, const std::string& language);
    std::string detectLanguage(const std::string& filePath) const;
    void reloadSyntaxRules();

    // Editor settings
    void setWordWrap(const std::string& filePath, bool enabled);
    void setLineNumbers(const std::string& filePath, bool enabled);
    void setMinimap(const std::string& filePath, bool enabled);
    void setFont(const std::string& filePath, const std::string& fontName, int size);
    void setTheme(const std::string& filePath, const std::string& theme);

    // Context menus
    void showEditorContextMenu(int x, int y);
    void showTabContextMenu(int x, int y);

    // Chat integration
    void showInlineSuggestion(const std::string& filePath, const std::string& suggestion);
    void acceptInlineSuggestion(const std::string& filePath);
    void rejectInlineSuggestion(const std::string& filePath);
    void openChatAtPosition(const std::string& filePath, int line, int column);

    // Navigation
    void goToDefinition(const std::string& filePath);
    void findReferences(const std::string& filePath);
    void peekDefinition(const std::string& filePath);
    void showProblems(const std::string& filePath);

    // Events
    void setOnFileModified(std::function<void(const std::string&)> handler);
    void setOnFileSaved(std::function<void(const std::string&)> handler);
    void setOnTabChanged(std::function<void(const std::string&)> handler);
    void setOnSelectionChanged(std::function<void(const std::string&)> handler);
    void setOnCursorMove(std::function<void(const std::string&, int, int)> handler);

    // UI integration
    HWND createEditorWindow(HWND parent, int x, int y, int width, int height);
    void updateEditorWindow();
    void setActiveTab(const std::string& filePath);
    std::string getActiveTab() const;

private:
    std::shared_ptr<AgentKernel> m_agentKernel;
    std::vector<EditorTab> m_tabs;
    std::string m_activeTab;
    HWND m_tabControl;
    HWND m_editorArea;
    HWND m_parentHwnd;

    std::unique_ptr<Win32ContextMenu> m_editorContextMenu;
    std::unique_ptr<Win32ContextMenu> m_tabContextMenu;

    std::function<void(const std::string&)> m_onFileModified;
    std::function<void(const std::string&)> m_onFileSaved;
    std::function<void(const std::string&)> m_onTabChanged;
    std::function<void(const std::string&)> m_onSelectionChanged;
    std::function<void(const std::string&, int, int)> m_onCursorMove;

    // Internal methods
    void createContextMenus();
    void updateTabControl();
    void createEditorForTab(const EditorTab& tab);
    void destroyEditorForTab(const std::string& filePath);
    void handleTabNotification(WPARAM wParam, LPARAM lParam);
    void handleEditorNotification(WPARAM wParam, LPARAM lParam);

    // Window procedures
    static LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TabWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace RawrXD::Win32