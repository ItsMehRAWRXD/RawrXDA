#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <richedit.h>
#include "chatpanel.h"

// Forward declarations
namespace RawrXD { 
    class AIIntegrationHub; 
    class EditorWindow;
    class ChatPanel;
}
class AutonomousModelManager;
class IntelligentCodebaseEngine;
class AutonomousFeatureEngine;
struct IWebBrowser2;

class IDEWindow {
public:
    IDEWindow();
    ~IDEWindow();

    bool Initialize(HINSTANCE hInstance);
    HWND GetHwnd() const { return hwnd_; }

private:
    void CreateMainWindow(HINSTANCE hInstance);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PopulatePowerShellCmdlets();
    void Shutdown();

    // UI Elements
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
    
    // Components
    RawrXD::EditorWindow* pEditor;
    IWebBrowser2* pWebBrowser_;
    
    // AI Integration
    std::shared_ptr<AutonomousModelManager> modelManager;
    std::shared_ptr<IntelligentCodebaseEngine> codebaseEngine;
    std::shared_ptr<AutonomousFeatureEngine> featureEngine;
    std::shared_ptr<RawrXD::AIIntegrationHub> aiHub;
    
    // Agentic UI
    std::shared_ptr<RawrXD::ChatPanel> m_chatPanel;

    HINSTANCE hInstance_;
    WNDPROC originalEditorProc_;
    bool isModified_;
    int nextTabId_;
    int activeTabId_;
    int selectedAutocompleteIndex_;
    bool autocompleteVisible_;
    int lastSearchPos_;
    bool lastSearchCaseSensitive_;
    bool lastSearchRegex_;
    
    COLORREF keywordColor_;
    COLORREF cmdletColor_;
    COLORREF stringColor_;
    COLORREF commentColor_;
    COLORREF variableColor_;
    COLORREF backgroundColor_;
    COLORREF textColor_;
    
    std::wstring sessionPath_;
};
