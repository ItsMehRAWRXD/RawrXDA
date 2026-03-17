#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "engine/react_ide_generator.h"

// Forward declarations to avoid including heavy headers in header
struct IWebBrowser2;

struct TabInfo {
    std::wstring filePath;
    std::wstring content;
    bool modified;
};

struct ExtensionInfo {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    std::wstring author;
    std::wstring version;
    bool installed;
};

class IDEWindow {
public:
    IDEWindow();
    ~IDEWindow();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Main Window Procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // UI Creation Methods
    void CreateMainWindow(HINSTANCE hInstance);
    void CreateMenuBar();
    void CreateToolBar();
    void CreateStatusBar();
    void CreateEditorControl();
    void CreateFileExplorer();
    void CreateTerminalPanel();
    void CreateOutputPanel();
    void CreateTabControl();
    void CreateWebBrowser();
    void LoadSession();
    void SaveSession();

    // Command Handlers
    void OnNewFile();
    void OnOpenFile();
    void OnSaveFile();
    void OnRunScript();
    void OnSwitchTab(int tabIndex);
    void OnCloseTab(int tabIndex);
    void ToggleCommandPalette();
    void ExecutePaletteSelection();
    void HideParameterHint();

    // Marketplace
    void SearchMarketplace(const std::wstring& searchText);
    void ShowExtensionDetails(const ExtensionInfo& ext);
    void InstallExtension(const ExtensionInfo& ext);
    void UninstallExtension(const ExtensionInfo& ext);
    void HideMarketplace();

    // Helper Methods
    void PopulateFileTree(const std::wstring& rootPath);
    void PopulatePowerShellCmdlets();
    void PopulateCommandPalette();
    
    // Static Window Procs for Subclassing
    static LRESULT CALLBACK EditorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Member Variables
    HWND hwnd_;
    HWND hEditor_;
    HWND hFileTree_;
    HWND hTerminal_;
    HWND hOutput_;
    HWND hStatusBar_;
    HWND hToolBar_;
    HWND hTabControl_;
    HWND hWebBrowser_;
    HWND hAutocompleteList_;
    HWND hParameterHint_;
    HWND hFindDialog_;
    HWND hReplaceDialog_;
    HWND hCommandPalette_;
    
    // Marketplace UI
    HWND hMarketplaceList_;
    HWND hMarketplaceSearch_;
    std::vector<ExtensionInfo> marketplaceExtensions_;

    IWebBrowser2* pWebBrowser_;
    HINSTANCE hInstance_;
    WNDPROC originalEditorProc_;

    // State Variables
    bool isModified_;
    int nextTabId_;
    std::map<int, TabInfo> openTabs_;
    int activeTabId_;
    int selectedAutocompleteIndex_;
    bool autocompleteVisible_;
    
    // Search State
    int lastSearchPos_;
    bool lastSearchCaseSensitive_;
    bool lastSearchRegex_;

    // Configuration / Theme
    std::wstring sessionPath_;
    std::wstring currentFolderPath_;
    std::wstring currentFilePath_;
    std::wstring extensionsPath_;

    COLORREF keywordColor_;
    COLORREF cmdletColor_;
    COLORREF stringColor_;
    COLORREF commentColor_;
    COLORREF variableColor_;
    COLORREF backgroundColor_;
    COLORREF textColor_;
    
    // Lists
    std::vector<std::wstring> keywordList_;
    std::vector<std::wstring> cmdletList_;
    std::vector<std::wstring> variableList_;
};
