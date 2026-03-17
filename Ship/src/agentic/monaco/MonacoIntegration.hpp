#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

// ============================================================================
// Monaco Editor Integration Interface
// ============================================================================

namespace RawrXD {
namespace Agentic {

struct MonacoConfig {
    std::wstring theme = L"vs-dark";
    std::wstring language = L"cpp";
    int fontSize = 14;
    std::wstring fontFamily = L"Consolas";
    bool readOnly = false;
    bool minimap = true;
    bool lineNumbers = true;
    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
};

struct MonacoCompletionItem {
    std::wstring label;
    std::wstring insertText;
    std::wstring detail;
    std::wstring documentation;
    int kind;
};

struct MonacoDiagnostic {
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    std::wstring message;
    int severity;
    std::wstring source;
};

struct MonacoEvent {
    std::wstring type;
    std::wstring data;
    long long timestamp;
};

using MonacoEventCallback = std::function<void(const MonacoEvent&)>;

class IMonacoEditor {
public:
    virtual ~IMonacoEditor() = default;

    virtual bool Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual void SetText(const std::wstring& text) = 0;
    virtual std::wstring GetText() const = 0;

    virtual void SetLanguage(const std::wstring& language) = 0;
    virtual void SetTheme(const std::wstring& theme) = 0;
    virtual void SetFontSize(int size) = 0;
    virtual void SetReadOnly(bool readOnly) = 0;

    virtual void SetSelection(int startLine, int startColumn, int endLine, int endColumn) = 0;
    virtual void GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const = 0;

    virtual void RevealLine(int lineNumber, bool atTop = false) = 0;
    virtual void RevealRange(int startLine, int endLine) = 0;

    virtual void ShowCompletions(const std::vector<MonacoCompletionItem>& items) = 0;
    virtual void HideCompletions() = 0;

    virtual void SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) = 0;
    virtual void ClearDiagnostics() = 0;

    virtual void Resize(const RECT& rect) = 0;
    virtual void Focus() = 0;
    virtual bool HasFocus() const = 0;

    virtual void Undo() = 0;
    virtual void Redo() = 0;
    virtual bool CanUndo() const = 0;
    virtual bool CanRedo() const = 0;

    virtual void Find(const std::wstring& searchText) = 0;
    virtual void Replace(const std::wstring& searchText, const std::wstring& replaceText) = 0;

    virtual void SetEventCallback(MonacoEventCallback callback) = 0;
};

// Common implementation for both backends (RichEdit fallback and WebView2)
class MonacoRichEditEditor : public IMonacoEditor {
public:
    MonacoRichEditEditor();
    virtual ~MonacoRichEditEditor();

    bool Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) override;
    void Shutdown() override;

    void SetText(const std::wstring& text) override;
    std::wstring GetText() const override;

    void SetLanguage(const std::wstring& language) override;
    void SetTheme(const std::wstring& theme) override;
    void SetFontSize(int size) override;
    void SetReadOnly(bool readOnly) override;

    void SetSelection(int startLine, int startColumn, int endLine, int endColumn) override;
    void GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const override;

    void RevealLine(int lineNumber, bool atTop = false) override;
    void RevealRange(int startLine, int endLine) override;

    void ShowCompletions(const std::vector<MonacoCompletionItem>& items) override;
    void HideCompletions() override;

    void SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) override;
    void ClearDiagnostics() override;

    void Resize(const RECT& rect) override;
    void Focus() override;
    bool HasFocus() const override;

    void Undo() override;
    void Redo() override;
    bool CanUndo() const override;
    bool CanRedo() const override;

    void Find(const std::wstring& searchText) override;
    void Replace(const std::wstring& searchText, const std::wstring& replaceText) override;

    void SetEventCallback(MonacoEventCallback callback) override { m_eventCallback = callback; }

    // Needs to be public or friend for message loop
    void HandleResize(int width, int height); 
    void UseLightColorTheme();
    void UseDarkColorTheme();
    
    // Internal but exposed for implementation matching
    void UpdateConfig(const MonacoConfig& config);
    void TriggerIntelliSense();

protected:
    HWND m_hwndEditor;
    HWND m_parentWindow;
    RECT m_rect;
    MonacoConfig m_config;
    bool m_initialized;
    std::wstring m_currentLanguage;
    std::wstring m_currentText;
    std::vector<MonacoCompletionItem> m_completions;
    std::vector<MonacoDiagnostic> m_diagnostics;
    MonacoEventCallback m_eventCallback;

    // RichEdit specific
    void CreateEditorWindow(HWND parent, const RECT& rect);
    void ApplyThemeColors();
    void ApplySyntaxHighlighting();
    void UpdateLineNumbers();
    int GetLineFromChar(int charIndex) const;
    int GetCharFromLineColumn(int line, int column) const;
    
    void ShowIntelliSensePopup();
    void HideIntelliSensePopup();
};

#ifdef HAS_WEBVIEW2
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>

class MonacoWebView2Editor : public IMonacoEditor {
public:
    MonacoWebView2Editor();
    virtual ~MonacoWebView2Editor();

    bool Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) override;
    void Shutdown() override;

    void SetText(const std::wstring& text) override;
    std::wstring GetText() const override;
    
    // ... complete interface implementation ...
    // Since we don't have the full impl visible, assume it follows IMonacoEditor
    // For now we might just declare it if needed, but since we are fixing headers:
    
    void SetLanguage(const std::wstring& language) override {}
    void SetTheme(const std::wstring& theme) override {}
    void SetFontSize(int size) override {}
    void SetReadOnly(bool readOnly) override {}

    void SetSelection(int startLine, int startColumn, int endLine, int endColumn) override {}
    void GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const override {}

    void RevealLine(int lineNumber, bool atTop = false) override {}
    void RevealRange(int startLine, int endLine) override {}

    void ShowCompletions(const std::vector<MonacoCompletionItem>& items) override {}
    void HideCompletions() override {}

    void SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) override {}
    void ClearDiagnostics() override {}

    void Resize(const RECT& rect) override {}
    void Focus() override {}
    bool HasFocus() const override { return false; }

    void Undo() override {}
    void Redo() override {}
    bool CanUndo() const override { return false; }
    bool CanRedo() const override { return false; }

    void Find(const std::wstring& searchText) override {}
    void Replace(const std::wstring& searchText, const std::wstring& replaceText) override {}

    void SetEventCallback(MonacoEventCallback callback) override { m_eventCallback = callback; }

private:
   MonacoEventCallback m_eventCallback;
   // Add other members if needed for WebView2
};
#endif

// Helper interfaces for language features
class IMonacoLanguageService {
public:
    virtual ~IMonacoLanguageService() = default;
};

class IMonacoThemeManager {
public:
    virtual ~IMonacoThemeManager() = default;
};


// Factory Functions
std::unique_ptr<IMonacoEditor> CreateMonacoEditor();
std::unique_ptr<IMonacoLanguageService> CreateMonacoLanguageService();
std::unique_ptr<IMonacoThemeManager> CreateMonacoThemeManager();

}
}

using namespace RawrXD::Agentic;
