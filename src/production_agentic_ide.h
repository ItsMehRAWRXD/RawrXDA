#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

// Panel types for the IDE
enum class PanelType {
    Paint,
    Code,
    Chat,
    Terminal,
    Sidebar,
    Unknown
};

// Panel structure to track open panels
struct Panel {
    PanelType type;
    HWND hwnd;
    std::wstring filename;
    std::wstring content;
    bool modified;
    HWND parentSplitter;
    
    Panel() : type(PanelType::Unknown), hwnd(NULL), modified(false), parentSplitter(NULL) {}
    Panel(PanelType t, HWND h) : type(t), hwnd(h), modified(false), parentSplitter(NULL) {}
};

// Splitter configuration
struct SplitLayout {
    HWND leftPanel;
    HWND rightPanel;
    HWND topPanel;
    HWND bottomPanel;
    int dividerPosH;  // Horizontal splitter position
    int dividerPosV;  // Vertical splitter position
    bool isVertical;
    
    SplitLayout() : leftPanel(NULL), rightPanel(NULL), topPanel(NULL), bottomPanel(NULL),
                    dividerPosH(50), dividerPosV(50), isVertical(false) {}
};

// Main IDE window class
class ProductionAgenticIDE {
public:
    ProductionAgenticIDE();
    ~ProductionAgenticIDE();
    
    HWND Create();
    int Run();
    
    // File Operations
    void onNewPaint();
    void onNewCode();
    void onNewChat();
    void onSave();
    void onSaveAs();
    void onOpen();
    void onClose();
    void onCloseAll();
    
    // Edit Operations
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onSelectAll();
    void onFind();
    void onReplace();
    
    // View Operations
    void onToggleSidebar();
    void onToggleTerminal();
    void onResetLayout();
    void onZoomIn();
    void onZoomOut();
    void onFullscreen();
    
    // Panel Management
    void onNextPanel();
    void onPrevPanel();
    void onSplitVertical();
    void onSplitHorizontal();
    
    // Public accessors
    HWND GetMainWindow() const { return m_hWnd; }
    HWND GetCurrentPanel() const { return m_pCurrentPanel ? m_pCurrentPanel->hwnd : NULL; }
    
private:
    // Window procedures
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ChildWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    LRESULT OnCreate();
    LRESULT OnSize(int width, int height);
    LRESULT OnCommand(WPARAM wParam, LPARAM lParam);
    LRESULT OnDestroy();
    void OnPaint();
    
    // UI Creation
    void CreateMenuBar();
    void CreateStatusBar();
    void CreateToolBar();
    void CreatePanelContainer();
    void UpdateLayout();
    void UpdateStatusBar(const std::wstring& message);
    
    // Panel Management
    Panel* CreatePanel(PanelType type, const std::wstring& filename = L"");
    void ClosePanel(Panel* panel);
    void ActivatePanel(Panel* panel);
    void CloseAllPanels();
    
    // File Operations
    std::wstring ShowOpenFileDialog();
    std::wstring ShowSaveFileDialog(const std::wstring& defaultName = L"");
    bool SavePanelContent(Panel* panel);
    bool LoadFileIntoPanel(Panel* panel, const std::wstring& filename);
    
    // Undo/Redo stacks
    void PushUndoState();
    void PopUndoState();
    void PushRedoState();
    
    // Clipboard operations
    void CopyToClipboard(const std::wstring& text);
    std::wstring GetFromClipboard();
    
    // Member variables
    HWND m_hWnd;
    HWND m_hStatusBar;
    HWND m_hToolBar;
    HWND m_hMainContainer;
    HWND m_hSidebar;
    HWND m_hTerminal;
    
    std::vector<std::unique_ptr<Panel>> m_panels;
    Panel* m_pCurrentPanel;
    Panel* m_pPaintPanel;
    Panel* m_pCodePanel;
    Panel* m_pChatPanel;
    
    SplitLayout m_splitLayout;
    
    // UI State
    int m_zoomLevel;
    bool m_fullscreen;
    bool m_sidebarVisible;
    bool m_terminalVisible;
    bool m_findDialogOpen;
    
    // Undo/Redo
    std::vector<std::wstring> m_undoStack;
    std::vector<std::wstring> m_redoStack;
    static const int MAX_UNDO_DEPTH = 50;
    
    // Application instance
    HINSTANCE m_hInstance;
    HWND m_hFindDialog;
    HWND m_hReplaceDialog;
    
    // Tab management
    int m_currentPanelIndex;
    
    static const wchar_t* CLASS_NAME;
    static const wchar_t* WINDOW_TITLE;
};
