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
#include "ide/chat_panel_integration.h"
#include "marketplace/vsix_loader.h"

// Forward declarations to avoid including heavy headers in header
struct IWebBrowser2;
namespace rawrxd::ide { class ChatPanelIntegration; }
namespace rawrxd::marketplace { class VsixLoader; }

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
    void Run();
    void Shutdown();

    // Main Window Procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void ExecutePowerShellCommand(const std::wstring& command);  // Also callable from TerminalProc

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
    void CreateNewTab(const std::wstring& title, const std::wstring& filePath);
    void UpdateTabTitle(int tabId, const std::wstring& title);
    void SaveCurrentTab();
    void LoadTabContent(int tabId);
    int GetCurrentTabId();
    void LoadFileIntoEditor(const std::wstring& filePath);
    void ReadTerminalOutput(HANDLE hPipeRead, HANDLE hProcess);
    void ApplySyntaxHighlighting();
    void UpdateStatusBar();
    std::wstring GetCurrentWord();
    std::wstring GetCurrentLine();

    // Autocomplete & Hints
    void ShowAutocompleteList(const std::wstring& partialText);
    void HideAutocompleteList();
    void UpdateAutocompletePosition();
    void SelectAutocompleteItem(int index);
    void InsertAutocompleteSelection();
    void ShowParameterHint(const std::wstring& cmdlet);
    void ParsePowerShellVariables();

    // Generator Integration
    void GenerateProject();

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
    
    IWebBrowser2* pWebBrowser_;
    HINSTANCE hInstance_;
    WNDPROC originalEditorProc_;

    bool isModified_;
    int nextTabId_;
    int activeTabId_;
    std::wstring sessionPath_;
    
    // Autocomplete state
    int selectedAutocompleteIndex_;
    bool autocompleteVisible_;
    std::vector<std::wstring> autocompleteItems_;
    
    // Search State
    std::wstring lastSearchText_;
    int lastSearchPos_;
    bool lastSearchCaseSensitive_;
    bool lastSearchRegex_;

    // Syntax Highlighting Colors
    COLORREF keywordColor_;
    COLORREF cmdletColor_;
    COLORREF stringColor_;
    COLORREF commentColor_;
    COLORREF variableColor_;
    COLORREF backgroundColor_;
    COLORREF textColor_;

    std::map<int, TabInfo> openTabs_;
    std::vector<std::wstring> powerShellCmdlets_;
    std::vector<std::wstring> commandPaletteItems_;
    
    // Additional state
    std::wstring currentFilePath_;
    std::wstring currentFolderPath_;
    
    // Marketplace
    HWND hMarketplaceSearch_;
    HWND hMarketplaceList_;
    std::vector<ExtensionInfo> marketplaceExtensions_;
    
    // Autocomplete lists
    std::vector<std::wstring> cmdletList_;
    std::vector<std::wstring> keywordList_;
    std::vector<std::wstring> variableList_;
    
    // Generator
    std::unique_ptr<ReactIDEGenerator> ideGenerator_;
    
    // Chat and Extensions Integration
    std::unique_ptr<rawrxd::ide::ChatPanelIntegration> chatPanel_;
    std::unique_ptr<rawrxd::marketplace::VsixLoader> extensionLoader_;
    
    // Static editor window procedure
    static LRESULT CALLBACK EditorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
