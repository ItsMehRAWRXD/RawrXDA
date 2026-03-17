#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <vector>

// Forward declarations
class NativeFileTree;
class AgenticBrowser;
class AgentOrchestra;
class NativeWidget;
class MultiPaneLayout;

// ============================================================================
// PRODUCTION AGENTIC IDE - Main Window & Orchestrator
// ============================================================================

class ProductionAgenticIDE {
public:
    ProductionAgenticIDE();
    ~ProductionAgenticIDE();

    // Window management
    int run();
    void showWindow();
    void closeWindow();
    HWND getWindowHandle() const { return m_hwnd; }

    // Component access
    NativeFileTree* getFileTree() const { return m_fileTree.get(); }
    AgenticBrowser* getBrowser() const { return m_agenticBrowser.get(); }
    AgentOrchestra* getAgentOrchestra() const { return m_agentOrchestra.get(); }

    // File operations
    void onFileOpen();
    void onFileSave();
    void onFileTreeDoubleClicked(const std::string& filePath);
    void onFileTreeContextMenu(const std::string& filePath);

    // View operations
    void toggleFileTreePane();
    void toggleTerminalPane();
    void toggleChatPane();
    void toggleBrowserPane();
    void resetLayout();

    // Paint operations
    void newPaintTab();
    void openPaint();
    void savePaint();

    // Code operations
    void newCodeTab();
    void openCode();
    void saveCode();

    // Chat operations
    void newChatTab();
    void sendChatMessage();

    // Analysis operations
    void analyzeCurrentFile();
    void analyzeProject();

    // Agentic operations
    void sendToAgent(const std::string& message);
    void selectModel(const std::string& model);
    void startVoiceInput();

    // Window procedure
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    std::unique_ptr<NativeFileTree> m_fileTree;
    std::unique_ptr<AgenticBrowser> m_agenticBrowser;
    std::unique_ptr<AgentOrchestra> m_agentOrchestra;
    std::unique_ptr<MultiPaneLayout> m_layout;

    // UI Components
    HWND m_mainTabWidget = nullptr;
    HWND m_terminal = nullptr;
    HWND m_statusBar = nullptr;
    HWND m_menuBar = nullptr;
    HWND m_toolBar = nullptr;

    // State
    bool m_fileTreeVisible = true;
    bool m_terminalVisible = true;
    bool m_chatVisible = true;
    bool m_browserVisible = false;

    void setupGUI();
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createLayout();
    void handleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

#endif
