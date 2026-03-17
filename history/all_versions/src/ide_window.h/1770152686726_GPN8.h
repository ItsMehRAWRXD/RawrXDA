#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <map>

// Forward declarations to avoid including heavy headers in header
struct IWebBrowser2;

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

    // Command Handlers
    void OnNewFile();
    void OnOpenFile();
    void OnSaveFile();
    void OnRunScript();
    void OnSwitchTab(int tabIndex);
    void OnCloseTab(int tabIndex);
    void ToggleCommandPalette();
    void PopluateCommandPalette();
    void ExecutePaletteSelection();
    void HideParameterHint();
    
    // Editor Features
    void FormatTrimTrailingWhitespace();
    void ToggleLineComment();
    void DuplicateLine();
    void DeleteLine();
    void SortSelectedLines();
    void ListFunctions();
    std::wstring DetectLanguage();
    void UpdateStatusBar();
    std::wstring GetCurrentWord();
    std::wstring GetCurrentLine();
    void ShowAutocompleteList(const std::wstring& partialText);
    void SelectAutocompleteItem(int index);
    void InsertAutocompleteSelection();
    void HideAutocompleteList();
    void UpdateAutocompletePosition();
    void ShowParameterHint(const std::wstring& cmdlet);
    void ParsePowerShellVariables();
    void LoadFileIntoEditor(const std::wstring& filePath);
    void ExecutePowerShellCommand(const std::wstring& command);

    // Tab Management
    void CreateNewTab(const std::wstring& title, const std::wstring& filePath);
    void SaveCurrentTab();
    void LoadTabContent(int tabId);
    void UpdateTabTitle(int tabId, const std::wstring& newTitle);
    int GetCurrentTabId();
    
    // Marketplace Features
    struct ExtensionInfo {
        std::wstring id;
        std::wstring name;
        std::wstring publisher;
        std::wstring version;
        std::wstring description;
        std::wstring downloadUrl;
        int downloads;
        float rating;
        bool installed;
        std::wstring installPath;
    };
    
    void CreateMarketplaceWindow();
    void ShowMarketplace();
    void HideMarketplace();
    void SearchMarketplace(const std::wstring& query);
    void PopulateMarketplaceList(const std::vector<ExtensionInfo>& extensions);
    void ShowExtensionDetails(const ExtensionInfo& ext);
    void InstallExtension(const ExtensionInfo& ext);
    void UninstallExtension(const ExtensionInfo& ext);
    void LoadInstalledExtensions();
    std::vector<ExtensionInfo> QueryVSCodeMarketplace(const std::wstring& query);
    std::vector<ExtensionInfo> QueryVSMarketplace(const std::wstring& query);
    bool DownloadFile(const std::wstring& url, const std::wstring& destPath);
    bool ExtractVSIX(const std::wstring& vsixPath, const std::wstring& destPath);

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
    HWND hMarketplaceWindow_;
    HWND hMarketplaceSearch_;
    HWND hMarketplaceList_;
    HWND hMarketplaceDetails_;
    HWND hMarketplaceInstallBtn_;
    HWND hMarketplaceDlg_ = nullptr;

    // Hotpatch UI Handles  
    HWND hHotpatchDlg_ = nullptr;

    IWebBrowser2* pWebBrowser_;
    HINSTANCE hInstance_;
    WNDPROC originalEditorProc_;

    // State Variables
    bool isModified_;
    int nextTabId_;
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
    std::vector<ExtensionInfo> marketplaceExtensions_;
    std::vector<ExtensionInfo> installedExtensions_;
    
    struct TabInfo {
        std::wstring filePath;
        std::wstring content;
        bool modified;
    };
    std::map<int, TabInfo> openTabs_;
};
