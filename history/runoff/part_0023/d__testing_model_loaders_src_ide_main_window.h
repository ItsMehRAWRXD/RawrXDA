// ide_main_window.h - IDE Main Window (Headless Stub)
// Converted from Qt QMainWindow to pure C++17 state management
// All GUI rendering removed; all state, commands, and logic preserved
#ifndef IDE_MAIN_WINDOW_H
#define IDE_MAIN_WINDOW_H

#include "autonomous_widgets.h"
#include "model_interface.h"
#include "intelligent_codebase_engine.h"
#include "common/callback_system.hpp"
#include "common/json_types.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

// Tab information (replaces QTabWidget tab)
struct EditorTab {
    std::string filePath;
    std::string fileName;
    std::string content;
    int cursorLine = 0;
    int cursorCol = 0;
    bool modified = false;
    std::string language;
    int scrollPosition = 0;
};

// Menu action (replaces QAction)
struct MenuAction {
    std::string id;
    std::string label;
    std::string shortcut;
    bool enabled = true;
    bool checked = false;
    std::function<void()> handler;
};

// Status bar segment
struct StatusBarSegment {
    std::string id;
    std::string text;
    std::string tooltip;
    int width = -1;   // -1 = stretch
};

// Find/Replace state
struct FindReplaceState {
    std::string findText;
    std::string replaceText;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool useRegex = false;
    bool replaceAll = false;
    int matchCount = 0;
    int currentMatch = 0;
};

// Theme
struct IDETheme {
    std::string name;
    std::string background;
    std::string foreground;
    std::string accent;
    std::string editorBackground;
    std::string editorForeground;
    std::string selectionColor;
    int fontSize = 12;
    std::string fontFamily = "Consolas";
};

// IDE settings
struct IDESettings {
    int tabSize = 4;
    bool useSpaces = true;
    bool wordWrap = false;
    bool showLineNumbers = true;
    bool showMinimap = true;
    bool autoSave = false;
    int autoSaveIntervalMs = 30000;
    IDETheme theme;
    std::vector<std::string> recentFiles;
    std::vector<std::string> recentProjects;
    std::string lastOpenProject;
    std::map<std::string, std::string> keybindings;
};

// Command palette command
struct PaletteCommand {
    std::string id;
    std::string label;
    std::string category;
    std::string shortcut;
    std::function<void()> handler;
};

class IDEMainWindow {
public:
    IDEMainWindow();
    ~IDEMainWindow();

    // File operations
    bool openFile(const std::string& filePath);
    bool saveFile(const std::string& filePath = "");
    bool saveAllFiles();
    bool closeFile(const std::string& filePath);
    void newFile(const std::string& language = "cpp");
    std::string getCurrentFilePath() const;
    std::string getCurrentFileContent() const;

    // Tab management
    void switchTab(int index);
    void closeTab(int index);
    int getTabCount() const;
    EditorTab getTab(int index) const;
    int getCurrentTabIndex() const;
    std::vector<EditorTab> getAllTabs() const;

    // Project operations
    bool openProject(const std::string& projectPath);
    void closeProject();
    std::string getProjectPath() const;
    std::vector<std::string> getProjectFiles() const;

    // Editor operations
    void setCursorPosition(int line, int col);
    void insertText(const std::string& text);
    void replaceSelection(const std::string& text);
    std::string getSelectedText() const;
    void goToLine(int line);
    void selectAll();

    // Find/Replace
    void find(const FindReplaceState& state);
    void findNext();
    void findPrevious();
    void replace(const FindReplaceState& state);
    void replaceAll(const FindReplaceState& state);
    FindReplaceState getFindState() const;

    // Command palette
    void registerCommand(const PaletteCommand& cmd);
    void executeCommand(const std::string& commandId);
    std::vector<PaletteCommand> searchCommands(const std::string& query) const;

    // Status bar
    void setStatusBarMessage(const std::string& message, int timeoutMs = 0);
    void addStatusBarSegment(const StatusBarSegment& segment);
    void updateStatusBarSegment(const std::string& id, const std::string& text);

    // Settings
    void setSettings(const IDESettings& settings);
    IDESettings getSettings() const;
    void saveSettings(const std::string& path = "");
    bool loadSettings(const std::string& path = "");

    // AI integration
    void setModelInterface(std::shared_ptr<ModelInterface> model);
    void setCodebaseEngine(std::shared_ptr<IntelligentCodebaseEngine> engine);
    void setWidgetManager(std::shared_ptr<AutonomousWidgetManager> widgets);

    // Output/Console
    void appendOutput(const std::string& text);
    void clearOutput();
    std::string getOutput() const;

    // Callbacks
    CallbackList<const std::string&> onFileOpened;
    CallbackList<const std::string&> onFileSaved;
    CallbackList<const std::string&> onFileClosed;
    CallbackList<const std::string&> onProjectOpened;
    CallbackList<int> onTabChanged;
    CallbackList<const std::string&> onCommandExecuted;
    CallbackList<const std::string&> onOutputAppended;
    CallbackList<const IDESettings&> onSettingsChanged;

private:
    void registerDefaultCommands();
    void updateWindowTitle();
    void addToRecentFiles(const std::string& path);

    mutable std::mutex m_mutex;
    std::vector<EditorTab> m_tabs;
    int m_currentTab = -1;
    std::string m_projectPath;
    IDESettings m_settings;
    FindReplaceState m_findState;
    std::vector<PaletteCommand> m_commands;
    std::vector<StatusBarSegment> m_statusSegments;
    std::string m_statusMessage;
    std::string m_outputBuffer;

    std::shared_ptr<ModelInterface> m_modelInterface;
    std::shared_ptr<IntelligentCodebaseEngine> m_codebaseEngine;
    std::shared_ptr<AutonomousWidgetManager> m_widgetManager;
};

#endif // IDE_MAIN_WINDOW_H
