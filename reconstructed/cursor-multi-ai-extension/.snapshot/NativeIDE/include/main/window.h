#pragma once

#include "native_ide.h"
#include <memory>

// Forward declarations
class IDEApplication;
namespace NativeIDE {
    class ProjectManager;
    class GitIntegration;
    class EditorCore;
    class PluginManager;
    struct GitStatus;
}

class MainWindow {
private:
    IDEApplication* m_app;
    HWND m_hWnd;
    HWND m_hEditor;
    HWND m_hProjectTree;
    HWND m_hOutputWindow;
    HWND m_hStatusBar;
    HWND m_hToolbar;
    
    // Direct2D resources
    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    IDWriteFactory* m_pDWriteFactory;
    IDWriteTextFormat* m_pTextFormat;
    ID2D1SolidColorBrush* m_pTextBrush;
    ID2D1SolidColorBrush* m_pBackgroundBrush;
    
    // Window state
    bool m_isMaximized;
    RECT m_windowRect;
    
    // Splitter controls
    int m_treeWidth;
    int m_outputHeight;
    bool m_draggingVerticalSplitter;
    bool m_draggingHorizontalSplitter;
    int m_lastMouseX;
    int m_lastMouseY;
    
    // Core components
    std::unique_ptr<NativeIDE::ProjectManager> m_projectManager;
    std::unique_ptr<NativeIDE::GitIntegration> m_gitIntegration;
    std::unique_ptr<NativeIDE::EditorCore> m_editorCore;
    std::unique_ptr<NativeIDE::PluginManager> m_pluginManager;
    
public:
    explicit MainWindow(IDEApplication* app);
    ~MainWindow();
    
    bool Create();
    void Show(int nCmdShow);
    void Close();
    
    HWND GetHandle() const { return m_hWnd; }
    HWND GetEditorHandle() const { return m_hEditor; }
    HWND GetProjectTreeHandle() const { return m_hProjectTree; }
    HWND GetOutputWindowHandle() const { return m_hOutputWindow; }
    
    // UI updates
    void UpdateTitle(const std::wstring& title = L"");
    void UpdateStatusBar(const std::wstring& text);
    void RefreshProjectTree();
    void AppendOutput(const std::string& text);
    void ClearOutput();
    
private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    void OnCreate();
    void OnDestroy();
    void OnSize(WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnCommand(WPARAM wParam);
    void OnMenuCommand(int menuId);
    void OnKeyDown(WPARAM wParam, LPARAM lParam);
    void OnMouseDown(WPARAM wParam, LPARAM lParam);
    void OnMouseUp(WPARAM wParam, LPARAM lParam);
    void OnMouseMove(WPARAM wParam, LPARAM lParam);
    bool OnSetCursor(LPARAM lParam);
    
    bool InitializeDirect2D();
    void CleanupDirect2D();
    
    // Layout management
    void AdaptLayoutToWindowSize();
    void SaveLayoutSettings();
    void LoadLayoutSettings();
    
    // Event handlers for IDE components
    void HandleFileEvent(const std::wstring& eventType, const std::wstring& filePath);
    void HandleGitStatusChanged(const NativeIDE::GitStatus& status);
    
    // Core component initialization
    bool InitializeComponents();
    bool CreateChildWindows();
    void CreateMenuBar();
    void CreateToolbar();
    void CreateStatusBar();
    void LayoutChildWindows();
    
    void RegisterWindowClass();
};