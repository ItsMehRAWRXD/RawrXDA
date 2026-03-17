#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <WebView2.h>
#include <wil/com.h>

// Forward declarations
class MonacoEditor;
class MonacoLanguageService;
class MonacoIntelliSense;
class MonacoThemeManager;

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

// Monaco language service interface
class IMonacoLanguageService {
public:
    virtual ~IMonacoLanguageService() = default;

    virtual bool RegisterLanguage(const std::wstring& languageId, const std::wstring& extensions) = 0;
    virtual void SetLanguageConfiguration(const std::wstring& languageId, const std::wstring& config) = 0;
    virtual void SetMonarchTokenizer(const std::wstring& languageId, const std::wstring& tokenizer) = 0;
    virtual void SetThemeData(const std::wstring& themeName, const std::wstring& themeData) = 0;
    virtual void RegisterCompletionProvider(const std::wstring& languageId, std::function<std::vector<MonacoCompletionItem>(const std::wstring&)> provider) = 0;
    virtual void RegisterHoverProvider(const std::wstring& languageId, std::function<std::wstring(const std::wstring&)> provider) = 0;
    virtual void RegisterDefinitionProvider(const std::wstring& languageId, std::function<std::vector<std::wstring>(const std::wstring&)> provider) = 0;
    virtual void RegisterReferenceProvider(const std::wstring& languageId, std::function<std::vector<std::wstring>(const std::wstring&)> provider) = 0;
    virtual void RegisterRenameProvider(const std::wstring& languageId, std::function<bool(const std::wstring&, const std::wstring&)> provider) = 0;
};

// Monaco theme manager
class IMonacoThemeManager {
public:
    virtual ~IMonacoThemeManager() = default;

    virtual void RegisterTheme(const std::wstring& name, const std::wstring& themeData) = 0;
    virtual void SetTheme(const std::wstring& name) = 0;
    virtual std::vector<std::wstring> GetAvailableThemes() const = 0;
    virtual std::wstring GetCurrentTheme() const = 0;
    virtual void DefineTheme(const std::wstring& name, const std::wstring& base, const std::vector<std::pair<std::wstring, std::wstring>>& colors, const std::vector<std::pair<std::wstring, std::wstring>>& tokenColors) = 0;
};

// WebView2-based Monaco editor implementation
class MonacoWebView2Editor : public IMonacoEditor {
private:
    wil::com_ptr<ICoreWebView2> m_webView;
    wil::com_ptr<ICoreWebView2Controller> m_controller;
    wil::com_ptr<ICoreWebView2Environment> m_environment;
    HWND m_parentWindow;
    RECT m_rect;
    MonacoConfig m_config;
    std::function<void(const MonacoEvent&)> m_eventCallback;
    bool m_initialized;
    std::wstring m_currentText;
    std::wstring m_currentLanguage;

    // WebView2 event handlers
    EventRegistrationToken m_navigationCompletedToken;
    EventRegistrationToken m_webMessageReceivedToken;
    EventRegistrationToken m_documentTitleChangedToken;

public:
    MonacoWebView2Editor();
    ~MonacoWebView2Editor() override;

    // IMonacoEditor implementation
    bool Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_initialized; }

    void SetText(const std::wstring& text) override;
    std::wstring GetText() const override;
    void SetLanguage(const std::wstring& language) override;
    std::wstring GetLanguage() const override { return m_currentLanguage; }

    void SetSelection(int startLine, int startColumn, int endLine, int endColumn) override;
    void GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const override;
    void RevealLine(int lineNumber, bool atTop = false) override;
    void RevealRange(int startLine, int endLine) override;

    void UpdateConfig(const MonacoConfig& config) override;
    void SetTheme(const std::wstring& theme) override;
    void SetFontSize(int size) override;
    void SetReadOnly(bool readOnly) override;

    void TriggerIntelliSense() override;
    void ShowCompletions(const std::vector<MonacoCompletionItem>& items) override;
    void HideCompletions() override;

    void SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) override;
    void ClearDiagnostics() override;

    void SetEventCallback(std::function<void(const MonacoEvent&)> callback) override { m_eventCallback = callback; }

    void Resize(const RECT& rect) override;
    void Focus() override;
    bool HasFocus() const override;

    void Undo() override;
    void Redo() override;
    bool CanUndo() const override;
    bool CanRedo() const override;

    void Find(const std::wstring& searchText) override;
    void Replace(const std::wstring& searchText, const std::wstring& replaceText) override;
    void FindNext() override;
    void FindPrevious() override;

    void FoldAll() override;
    void UnfoldAll() override;
    void FoldLevel(int level) override;

    void ExecuteCommand(const std::wstring& command) override;
    void ExecuteCommand(const std::wstring& command, const std::vector<std::wstring>& args) override;

private:
    // WebView2 event handlers
    HRESULT OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args);
    HRESULT OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);
    HRESULT OnDocumentTitleChanged(ICoreWebView2* sender, IUnknown* args);

    // Helper methods
    void SendMessageToMonaco(const std::wstring& message);
    void ExecuteJavaScript(const std::wstring& script);
    void LoadMonacoHTML();
    std::wstring GenerateMonacoHTML() const;
    std::wstring EscapeJavaScriptString(const std::wstring& str) const;
};

// Factory functions
std::unique_ptr<IMonacoEditor> CreateMonacoEditor();
std::unique_ptr<IMonacoLanguageService> CreateMonacoLanguageService();
std::unique_ptr<IMonacoThemeManager> CreateMonacoThemeManager();

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
extern std::unique_ptr<IMonacoLanguageService> g_monacoLanguageService;
extern std::unique_ptr<IMonacoThemeManager> g_monacoThemeManager;

#endif // MONACO_INTEGRATION_HPP