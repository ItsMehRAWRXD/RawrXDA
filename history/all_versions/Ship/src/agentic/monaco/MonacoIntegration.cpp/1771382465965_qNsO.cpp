#include "MonacoIntegration.hpp"
#include <Richedit.h>
#include <commctrl.h>
#include <algorithm>

// Load RichEdit library
#pragma comment(lib, "riched20.lib")

// ============================================================================
// MonacoRichEditEditor Implementation (Fallback)
// ============================================================================

MonacoRichEditEditor::MonacoRichEditEditor()
    : m_hwndEditor(nullptr)
    , m_parentWindow(nullptr)
    , m_initialized(false)
    , m_currentLanguage(L"cpp")
{
    // Load RichEdit
    LoadLibraryW(L"riched20.dll");
}

MonacoRichEditEditor::~MonacoRichEditEditor() {
    Shutdown();
}

bool MonacoRichEditEditor::Initialize(HWND parentWindow, const RECT& rect, const MonacoConfig& config) {
    if (m_initialized) return true;

    m_parentWindow = parentWindow;
    m_rect = rect;
    m_config = config;

    CreateEditorWindow(parentWindow, rect);
    if (!m_hwndEditor) return false;

    m_initialized = true;
    ApplyThemeColors();
    UpdateLineNumbers();

    return true;
}

void MonacoRichEditEditor::Shutdown() {
    if (m_hwndEditor) {
        DestroyWindow(m_hwndEditor);
        m_hwndEditor = nullptr;
    }
    m_initialized = false;
}

void MonacoRichEditEditor::SetText(const std::wstring& text) {
    if (!m_hwndEditor) return;
    m_currentText = text;
    SetWindowTextW(m_hwndEditor, text.c_str());
    ApplySyntaxHighlighting();
}

std::wstring MonacoRichEditEditor::GetText() const {
    if (!m_hwndEditor) return m_currentText;

    int length = GetWindowTextLengthW(m_hwndEditor);
    if (length == 0) return L"";

    std::vector<wchar_t> buffer(length + 1);
    GetWindowTextW(m_hwndEditor, buffer.data(), length + 1);
    return std::wstring(buffer.data());
}

void MonacoRichEditEditor::SetLanguage(const std::wstring& language) {
    m_currentLanguage = language;
    ApplySyntaxHighlighting();
}

void MonacoRichEditEditor::SetSelection(int startLine, int startColumn, int endLine, int endColumn) {
    if (!m_hwndEditor) return;

    int startChar = GetCharFromLineColumn(startLine, startColumn);
    int endChar = GetCharFromLineColumn(endLine, endColumn);

    SendMessage(m_hwndEditor, EM_SETSEL, startChar, endChar);
}

void MonacoRichEditEditor::GetSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const {
    if (!m_hwndEditor) return;

    DWORD start = 0, end = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    startLine = GetLineFromChar(start);
    startColumn = start - SendMessage(m_hwndEditor, EM_LINEINDEX, startLine, 0);
    endLine = GetLineFromChar(end);
    endColumn = end - SendMessage(m_hwndEditor, EM_LINEINDEX, endLine, 0);
}

void MonacoRichEditEditor::RevealLine(int lineNumber, bool atTop) {
    if (!m_hwndEditor) return;

    int charIndex = SendMessage(m_hwndEditor, EM_LINEINDEX, lineNumber, 0);
    if (charIndex != -1) {
        SendMessage(m_hwndEditor, EM_LINESCROLL, 0, lineNumber - SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));
    }
}

void MonacoRichEditEditor::RevealRange(int startLine, int endLine) {
    RevealLine(startLine, false);
}

void MonacoRichEditEditor::UpdateConfig(const MonacoConfig& config) {
    m_config = config;
    ApplyThemeColors();
    UpdateLineNumbers();
}

void MonacoRichEditEditor::SetTheme(const std::wstring& theme) {
    m_config.theme = theme;
    ApplyThemeColors();
}

void MonacoRichEditEditor::SetFontSize(int size) {
    m_config.fontSize = size;
    ApplyThemeColors();
}

void MonacoRichEditEditor::SetReadOnly(bool readOnly) {
    m_config.readOnly = readOnly;
    SendMessage(m_hwndEditor, EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);
}

void MonacoRichEditEditor::TriggerIntelliSense() {
    // Basic implementation - could show a popup with suggestions
    ShowIntelliSensePopup();
}

void MonacoRichEditEditor::ShowCompletions(const std::vector<MonacoCompletionItem>& items) {
    m_completions = items;
    ShowIntelliSensePopup();
}

void MonacoRichEditEditor::HideCompletions() {
    HideIntelliSensePopup();
}

void MonacoRichEditEditor::SetDiagnostics(const std::vector<MonacoDiagnostic>& diagnostics) {
    m_diagnostics = diagnostics;
    ApplySyntaxHighlighting(); // Re-apply highlighting to show diagnostics
}

void MonacoRichEditEditor::ClearDiagnostics() {
    m_diagnostics.clear();
    ApplySyntaxHighlighting();
}

void MonacoRichEditEditor::Resize(const RECT& rect) {
    m_rect = rect;
    if (m_hwndEditor) {
        MoveWindow(m_hwndEditor, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }
}

void MonacoRichEditEditor::Focus() {
    if (m_hwndEditor) {
        SetFocus(m_hwndEditor);
    }
}

bool MonacoRichEditEditor::HasFocus() const {
    return m_hwndEditor && GetFocus() == m_hwndEditor;
}

void MonacoRichEditEditor::Undo() {
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
    }
}

void MonacoRichEditEditor::Redo() {
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_REDO, 0, 0);
    }
}

bool MonacoRichEditEditor::CanUndo() const {
    return m_hwndEditor && SendMessage(m_hwndEditor, EM_CANUNDO, 0, 0);
}

bool MonacoRichEditEditor::CanRedo() const {
    return m_hwndEditor && SendMessage(m_hwndEditor, EM_CANREDO, 0, 0);
}

void MonacoRichEditEditor::Find(const std::wstring& searchText) {
    if (!m_hwndEditor || searchText.empty()) return;

    FINDTEXTW ft = {0};
    ft.lpstrText = searchText.c_str();

    int pos = SendMessage(m_hwndEditor, EM_FINDTEXTW, FR_DOWN, (LPARAM)&ft);
    if (pos != -1) {
        SendMessage(m_hwndEditor, EM_SETSEL, pos, pos + searchText.length());
        RevealLine(GetLineFromChar(pos), false);
    }
}

void MonacoRichEditEditor::Replace(const std::wstring& searchText, const std::wstring& replaceText) {
    if (!m_hwndEditor) return;

    // Get current selection
    DWORD start, end;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    // Replace
    SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)replaceText.c_str());
}

void MonacoRichEditEditor::FindNext() {
    // Implementation would need to store last search
}

void MonacoRichEditEditor::FindPrevious() {
    // Implementation would need to store last search
}

void MonacoRichEditEditor::FoldAll() {
    // RichEdit doesn't support folding natively
}

void MonacoRichEditEditor::UnfoldAll() {
    // RichEdit doesn't support folding natively
}

void MonacoRichEditEditor::FoldLevel(int level) {
    // RichEdit doesn't support folding natively
}

void MonacoRichEditEditor::ExecuteCommand(const std::wstring& command) {
    // Basic command execution - could be extended
    if (command == L"editor.action.selectAll") {
        SendMessage(m_hwndEditor, EM_SETSEL, 0, -1);
    } else if (command == L"editor.action.copy") {
        SendMessage(m_hwndEditor, WM_COPY, 0, 0);
    } else if (command == L"editor.action.paste") {
        SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
    } else if (command == L"editor.action.cut") {
        SendMessage(m_hwndEditor, WM_CUT, 0, 0);
    }
}

void MonacoRichEditEditor::ExecuteCommand(const std::wstring& command, const std::vector<std::wstring>& args) {
    ExecuteCommand(command);
}

// Private helper methods
void MonacoRichEditEditor::CreateEditorWindow(HWND parent, const RECT& rect) {
    m_hwndEditor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        RICHEDIT_CLASSW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (m_hwndEditor) {
        // Subclass the window
        SetWindowLongPtr(m_hwndEditor, GWLP_USERDATA, (LONG_PTR)this);
        SetWindowLongPtr(m_hwndEditor, GWLP_WNDPROC, (LONG_PTR)EditorWndProc);

        // Set font
        LOGFONTW lf = {0};
        lf.lfHeight = -MulDiv(m_config.fontSize, GetDeviceCaps(GetDC(m_hwndEditor), LOGPIXELSY), 72);
        wcscpy_s(lf.lfFaceName, m_config.fontFamily.c_str());
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;

        HFONT hFont = CreateFontIndirectW(&lf);
        SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

void MonacoRichEditEditor::DestroyEditorWindow() {
    if (m_hwndEditor) {
        DestroyWindow(m_hwndEditor);
        m_hwndEditor = nullptr;
    }
}

void MonacoRichEditEditor::ApplySyntaxHighlighting() {
    if (!m_hwndEditor) return;

    // Basic syntax highlighting for C++
    if (m_currentLanguage == L"cpp" || m_currentLanguage == L"c") {
        std::wstring text = GetText();
        // This is a simplified implementation - a full implementation would use
        // more sophisticated parsing and RTF formatting
        SetText(text); // Re-set to trigger re-display
    }
}

void MonacoRichEditEditor::UpdateLineNumbers() {
    // RichEdit doesn't have built-in line numbers
    // Could be implemented with a separate control
}

void MonacoRichEditEditor::HandleKeyInput(WPARAM wParam, LPARAM lParam) {
    // Handle special key combinations
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        switch (wParam) {
            case 'Z':
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    Redo();
                } else {
                    Undo();
                }
                return;
            case 'Y':
                Redo();
                return;
            case 'A':
                SendMessage(m_hwndEditor, EM_SETSEL, 0, -1);
                return;
            case 'C':
                SendMessage(m_hwndEditor, WM_COPY, 0, 0);
                return;
            case 'V':
                SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
                return;
            case 'X':
                SendMessage(m_hwndEditor, WM_CUT, 0, 0);
                return;
            case VK_SPACE:
                TriggerIntelliSense();
                return;
        }
    }

    // Handle Tab key for indentation
    if (wParam == VK_TAB) {
        if (GetKeyState(VK_SHIFT) & 0x8000) {
            // Unindent
            DWORD start, end;
            SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            int startLine = GetLineFromChar(start);
            int endLine = GetLineFromChar(end);

            for (int line = startLine; line <= endLine; ++line) {
                int lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
                DWORD lineEnd = lineStart + SendMessage(m_hwndEditor, EM_LINELENGTH, lineStart, 0);

                // Check if line starts with tab or spaces
                if (lineEnd > lineStart) {
                    char firstChar;
                    SendMessage(m_hwndEditor, EM_GETLINE, line, (LPARAM)&firstChar);
                    if (firstChar == '\t' || firstChar == ' ') {
                        SendMessage(m_hwndEditor, EM_SETSEL, lineStart, lineStart + 1);
                        SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)L"");
                    }
                }
            }
        } else {
            // Indent
            SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)L"\t");
        }
        return;
    }
}

void MonacoRichEditEditor::HandleMouseInput(UINT message, WPARAM wParam, LPARAM lParam) {
    // Handle mouse events if needed
}

int MonacoRichEditEditor::GetLineFromChar(int charIndex) const {
    if (!m_hwndEditor) return 0;
    return SendMessage(m_hwndEditor, EM_LINEFROMCHAR, charIndex, 0);
}

int MonacoRichEditEditor::GetColumnFromChar(int charIndex) const {
    if (!m_hwndEditor) return 0;
    int line = GetLineFromChar(charIndex);
    int lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
    return charIndex - lineStart;
}

int MonacoRichEditEditor::GetCharFromLineColumn(int line, int column) const {
    if (!m_hwndEditor) return 0;
    int lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
    return lineStart + column;
}

void MonacoRichEditEditor::ShowIntelliSensePopup() {
    // Basic implementation - could create a popup window with suggestions
    if (m_eventCallback) {
        MonacoEvent event;
        event.type = L"intellisense.show";
        event.data = L"Basic IntelliSense popup";
        event.timestamp = GetTickCount64();
        m_eventCallback(event);
    }
}

void MonacoRichEditEditor::HideIntelliSensePopup() {
    if (m_eventCallback) {
        MonacoEvent event;
        event.type = L"intellisense.hide";
        event.timestamp = GetTickCount64();
        m_eventCallback(event);
    }
}

void MonacoRichEditEditor::ApplyThemeColors() {
    if (!m_hwndEditor) return;

    // Set background color
    SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, m_config.theme == L"vs-dark" ? RGB(30, 30, 30) : RGB(255, 255, 255));

    // Set text color
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = m_config.theme == L"vs-dark" ? RGB(212, 212, 212) : RGB(0, 0, 0);
    SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Update font
    LOGFONTW lf = {0};
    lf.lfHeight = -MulDiv(m_config.fontSize, GetDeviceCaps(GetDC(m_hwndEditor), LOGPIXELSY), 72);
    wcscpy_s(lf.lfFaceName, m_config.fontFamily.c_str());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;

    HFONT hFont = CreateFontIndirectW(&lf);
    SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
    DeleteObject(hFont);
}

LRESULT CALLBACK MonacoRichEditEditor::EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MonacoRichEditEditor* editor = (MonacoRichEditEditor*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (editor) {
        return editor->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MonacoRichEditEditor::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            HandleKeyInput(wParam, lParam);
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            HandleMouseInput(msg, wParam, lParam);
            break;
        case WM_CHAR:
            // Handle character input
            if (m_eventCallback) {
                MonacoEvent event;
                event.type = L"content.changed";
                event.timestamp = GetTickCount64();
                m_eventCallback(event);
            }
            break;
    }

    return CallWindowProcW(DefWindowProcW, hwnd, msg, wParam, lParam);
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<IMonacoEditor> CreateMonacoEditor() {
#ifdef HAS_WEBVIEW2
    return std::make_unique<MonacoWebView2Editor>();
#else
    return std::make_unique<MonacoRichEditEditor>();
#endif
}

std::unique_ptr<IMonacoLanguageService> CreateMonacoLanguageService() {
    // Basic implementation - would need full implementation
    return nullptr;
}

std::unique_ptr<IMonacoThemeManager> CreateMonacoThemeManager() {
    // Basic implementation - would need full implementation
    return nullptr;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool IsWebView2RuntimeInstalled() {
#ifdef HAS_WEBVIEW2
    // Check if WebView2 is installed
    wil::com_ptr<ICoreWebView2Environment> environment;
    return SUCCEEDED(CreateCoreWebView2Environment(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&environment](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                environment = env;
                return S_OK;
            }).Get()));
#else
    return false;
#endif
}

std::wstring GetWebView2Version() {
    return L"Not available (fallback mode)";
}

HRESULT InstallWebView2Runtime() {
    return E_NOTIMPL;
}

void InitializeMonacoIntegration() {
    g_monacoInitialized = true;
}

void ShutdownMonacoIntegration() {
    if (g_monacoEditor) {
        g_monacoEditor->Shutdown();
        g_monacoEditor.reset();
    }
    g_monacoInitialized = false;
}

// ============================================================================
// Global State
// ============================================================================

bool g_monacoInitialized = false;
std::unique_ptr<IMonacoEditor> g_monacoEditor;
std::unique_ptr<IMonacoLanguageService> g_monacoLanguageService;
std::unique_ptr<IMonacoThemeManager> g_monacoThemeManager;