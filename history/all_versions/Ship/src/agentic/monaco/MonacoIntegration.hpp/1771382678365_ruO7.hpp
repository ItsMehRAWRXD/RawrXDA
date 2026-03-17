#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// WebView2 headers - conditionally included
#ifdef HAS_WEBVIEW2
#include <WebView2.h>
#include <wil/com.h>
#endif

// ============================================================================
// Monaco Integration for RawrXD Win32 IDE
// ============================================================================

// Monaco editor configuration
struct MonacoConfig {
    std::wstring theme = L"vs-dark";
    std::wstring language = L"cpp";
    bool minimap = true;
    bool wordWrap = false;
    bool lineNumbers = true;
    int fontSize = 14;
    std::wstring fontFamily = L"Consolas, 'Courier New', monospace";
    bool readOnly = false;
    bool contextMenu = true;
    bool multiCursor = true;
    bool folding = true;
    bool bracketMatching = true;
    bool autoClosingBrackets = true;
    bool autoClosingQuotes = true;
    bool formatOnType = true;
    bool formatOnPaste = true;
    bool suggestOnTriggerCharacters = true;
    bool acceptSuggestionOnEnter = true;
    bool tabCompletion = true;
    bool wordBasedSuggestions = true;
    bool parameterHints = true;
    bool hover = true;
    bool occurrencesHighlight = true;
    bool selectionHighlight = true;
    bool codeLens = false;
    bool colorDecorators = true;
    bool lightbulb = true;
    bool links = true;
    bool unicodeHighlight = true;
    bool renderWhitespace = false;
    bool renderControlCharacters = false;
    bool fontLigatures = true;
    bool smoothScrolling = true;
    bool cursorBlinking = true;
    std::wstring cursorStyle = L"line";
    int cursorWidth = 1;
    bool cursorSmoothCaretAnimation = true;
    bool mouseWheelZoom = true;
    bool multiCursorModifier = true;
    bool accessibilitySupport = true;
    bool codeActionsOnSave = true;
    std::vector<std::wstring> codeActionsOnSaveActions = {L"source.fixAll", L"source.organizeImports"};
};

// Monaco editor events
struct MonacoEvent {
    std::wstring type;
    std::wstring data;
    long long timestamp;
};

// Monaco completion item
struct MonacoCompletionItem {
    std::wstring label;
    std::wstring kind;
    std::wstring detail;
    std::wstring documentation;
    std::wstring insertText;
    std::vector<std::wstring> commitCharacters;
    bool preselect = false;
    std::wstring sortText;
    std::wstring filterText;
    std::wstring range;
    std::wstring command;
};

// Monaco diagnostic
struct MonacoDiagnostic {
    std::wstring message;
    std::wstring severity;
    int startLineNumber;
    int startColumn;
    int endLineNumber;
    int endColumn;
    std::wstring source;
    std::wstring code;
    std::vector<std::wstring> tags;
    std::vector<std::wstring> relatedInformation;
};

// Monaco editor interface
class IMonacoEditor {
public:
    virtual ~IMonacoEditor() = default;

    // Core editor operations
    virtual bool Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Content operations
    virtual void SetText(const std::wstring& text) = 0;
    virtual std::wstring GetText() const = 0;
    virtual void SetLanguage(const std::wstring& language) = 0;
    virtual std::wstring GetLanguage() const = 0;

    // Selection operations
    virtual void SetSelection(int startLine, int startColumn, int endLine, int endColumn) = 0;
    virtual void GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const = 0;
    virtual void RevealLine(int lineNumber, bool atTop = false) = 0;
    virtual void RevealRange(int startLine, int endLine) = 0;

    // Configuration
    virtual void UpdateConfig(const MonacoConfig& config) = 0;
    virtual void SetTheme(const std::wstring& theme) = 0;
    virtual void SetFontSize(int size) = 0;
    virtual void SetReadOnly(bool readOnly) = 0;

    // IntelliSense and completion
    virtual void TriggerIntelliSense() = 0;
    virtual void ShowCompletions(const std::vector<MonacoCompletionItem>& items) = 0;
    virtual void HideCompletions() = 0;

    // Diagnostics
    virtual void SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) = 0;
    virtual void ClearDiagnostics() = 0;

    // Event handling
    virtual void SetEventCallback(std::function<void(const MonacoEvent&)> callback) = 0;

    // Layout
    virtual void Resize(const RECT& rect) = 0;
    virtual void Focus() = 0;
    virtual bool HasFocus() const = 0;

    // Undo/Redo
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    virtual bool CanUndo() const = 0;
    virtual bool CanRedo() const = 0;

    // Find/Replace
    virtual void Find(const std::wstring& searchText) = 0;
    virtual void Replace(const std::wstring& searchText, const std::wstring& replaceText) = 0;
    virtual void FindNext() = 0;
    virtual void FindPrevious() = 0;

    // Folding
    virtual void FoldAll() = 0;
    virtual void UnfoldAll() = 0;
    virtual void FoldLevel(int level) = 0;

    // Commands
    virtual void ExecuteCommand(const std::wstring& command) = 0;
    virtual void ExecuteCommand(const std::wstring& command, const std::vector<std::wstring>& args) = 0;
};

// Factory functions
std::unique_ptr<IMonacoEditor> CreateMonacoEditor();

// Utility functions
bool IsWebView2RuntimeInstalled();
std::wstring GetWebView2Version();
HRESULT InstallWebView2Runtime();
void InitializeMonacoIntegration();
void ShutdownMonacoIntegration();

// Error handling
class MonacoException : public std::exception {
public:
    MonacoException(const std::string& message, HRESULT hr = S_OK) : m_message(message), m_hr(hr) {}
    const char* what() const noexcept override { return m_message.c_str(); }
    HRESULT GetHResult() const { return m_hr; }

private:
    std::string m_message;
    HRESULT m_hr;
};

// Global Monaco integration state
extern bool g_monacoInitialized;
extern std::unique_ptr<IMonacoEditor> g_monacoEditor;

#endif // MONACO_INTEGRATION_HPP