#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <functional>
#include "ide_orchestrator.h"
#include "monaco_integration.h"

namespace RawrXD {

class GUIMain {
public:
    GUIMain();
    ~GUIMain();
    
    // Non-copyable
    GUIMain(const GUIMain&) = delete;
    GUIMain& operator=(const GUIMain&) = delete;
    
    // Real GUI initialization
    std::expected<void, std::string> initialize(HINSTANCE hInstance);
    std::expected<void, std::string> run();
    void shutdown();
    
    // Real window management
    HWND getMainWindow() const { return m_mainWindow; }
    HWND getEditorWindow() const { return m_editorWindow; }
    
    // Real message handling
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Real menu handling
    void createMenus();
    void handleMenuCommand(int commandId);
    
    // Real toolbar
    void createToolbar();
    void updateToolbar();
    
    // Real status bar
    void createStatusBar();
    void updateStatusBar(const std::string& message);
    
    // Real docking
    void createDockingPanels();
    void updateDockingLayout();
    
    // Event handlers
    std::function<void()> onFileNew;
    std::function<void()> onFileOpen;
    std::function<void()> onFileSave;
    std::function<void()> onEditUndo;
    std::function<void()> onEditRedo;
    std::function<void()> onEditCut;
    std::function<void()> onEditCopy;
    std::function<void()> onEditPaste;
    std::function<void()> onBuild;
    std::function<void()> onRun;
    std::function<void()> onDebug;
    
private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_mainWindow = nullptr;
    HWND m_editorWindow = nullptr;
    HWND m_toolbar = nullptr;
    HWND m_statusBar = nullptr;
    HMENU m_mainMenu = nullptr;
    
    std::unique_ptr<IDEOrchestrator> m_ide;
    std::unique_ptr<MonacoEditor> m_editor;
    
    // Real window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessageInternal(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Real initialization helpers
    std::expected<void, std::string> registerWindowClass();
    std::expected<void, std::string> createMainWindow();
    std::expected<void, std::string> createEditorWindow();
    std::expected<void, std::string> setupLayout();
    
    // Real command handlers
    void onFileNewInternal();
    void onFileOpenInternal();
    void onFileSaveInternal();
    void onEditUndoInternal();
    void onEditRedoInternal();
    void onEditCutInternal();
    void onEditCopyInternal();
    void onEditPasteInternal();
    void onBuildInternal();
    void onRunInternal();
    void onDebugInternal();
};

} // namespace RawrXD
