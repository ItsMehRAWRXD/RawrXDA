/**
 * @file editor_agent_integration_win32.cpp
 * @brief Win32 native implementation of agentic editor features
 *
 * Provides ghost text suggestions, context menus, and AI-powered completions
 * for Win32 EDIT/RichEdit controls.
 */

#ifdef _WIN32
#include "editor_agent_integration_win32.h"
#include "code_suggestion_engine.h"
#include "production_logger.h"
#include <richedit.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>

EditorAgentIntegrationWin32::EditorAgentIntegrationWin32(
    HWND hwndEditor,
    AgenticExecutor* agenticExecutor,
    AgenticTools* agenticTools)
    : m_hwndEditor(hwndEditor)
    , m_agenticExecutor(agenticExecutor)
    , m_agenticTools(agenticTools)
    , m_suggestionEngine(new CodeSuggestionEngine())
{
    LOG_INFO(ProductionLogger::AGENTIC, "EditorAgentIntegration initialized for HWND");

    // Create context menu
    m_contextMenu = CreatePopupMenu();
    if (m_contextMenu) {
        AppendMenuA(m_contextMenu, MF_STRING, CMD_CODE_COMPLETION, "Code Completion\tCtrl+Space");
        AppendMenuA(m_contextMenu, MF_STRING, CMD_EXPLAIN_CODE, "Explain Code");
        AppendMenuA(m_contextMenu, MF_STRING, CMD_REFACTOR_CODE, "Suggest Refactoring");
        AppendMenuA(m_contextMenu, MF_STRING, CMD_GENERATE_TESTS, "Generate Tests");
        AppendMenuA(m_contextMenu, MF_STRING, CMD_FIND_BUGS, "Find Bugs");
        AppendMenuA(m_contextMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuA(m_contextMenu, MF_STRING, CMD_TOGGLE_GHOST_TEXT, "Toggle Ghost Text");
    }

    // Create ghost text font (italic, slightly smaller)
    LOGFONTA lf = {};
    lf.lfHeight = -12;
    lf.lfItalic = TRUE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpyA(lf.lfFaceName, "Consolas");
    m_ghostTextFont = CreateFontIndirectA(&lf);
}

EditorAgentIntegrationWin32::~EditorAgentIntegrationWin32()
{
    if (m_contextMenu) {
        DestroyMenu(m_contextMenu);
    }
    if (m_ghostTextFont) {
        DeleteObject(m_ghostTextFont);
    }
    if (m_hwndGhostOverlay && IsWindow(m_hwndGhostOverlay)) {
        DestroyWindow(m_hwndGhostOverlay);
    }
    if (m_suggestionEngine) {
        delete m_suggestionEngine;
    }
}

void EditorAgentIntegrationWin32::setGhostTextEnabled(bool enabled)
{
    m_ghostTextEnabled = enabled;
    if (!enabled) {
        clearGhostText();
    }
    LOG_INFO(ProductionLogger::AGENTIC, 
             enabled ? "Ghost text ENABLED" : "Ghost text DISABLED");
}

void EditorAgentIntegrationWin32::setFileType(const std::string& fileType)
{
    m_fileType = fileType;
    LOG_DEBUG(ProductionLogger::AGENTIC, "File type set to: " + fileType);
}

void EditorAgentIntegrationWin32::setAutoSuggestions(bool enabled)
{
    m_autoSuggestions = enabled;
    LOG_INFO(ProductionLogger::AGENTIC,
             enabled ? "Auto-suggestions ENABLED" : "Auto-suggestions DISABLED");
}

void EditorAgentIntegrationWin32::triggerSuggestion(const GhostTextContext& context)
{
    if (!m_ghostTextEnabled || !m_agenticExecutor) {
        return;
    }

    GhostTextContext ctx = context.currentLine.empty() ? extractContext() : context;
    generateSuggestion(ctx);
}

bool EditorAgentIntegrationWin32::acceptSuggestion()
{
    if (m_currentSuggestion.text.empty()) {
        LOG_DEBUG(ProductionLogger::AGENTIC, "No suggestion to accept");
        return false;
    }

    insertTextAtCursor(m_currentSuggestion.text);

    LOG_INFO(ProductionLogger::AGENTIC, "Suggestion accepted: " + 
             m_currentSuggestion.text.substr(0, 50));

    if (m_suggestionCallback) {
        m_suggestionCallback(m_currentSuggestion.text);
    }

    clearGhostText();
    return true;
}

void EditorAgentIntegrationWin32::dismissSuggestion()
{
    clearGhostText();
    LOG_DEBUG(ProductionLogger::AGENTIC, "Suggestion dismissed");
}

void EditorAgentIntegrationWin32::clearGhostText()
{
    m_currentSuggestion.text.clear();
    m_currentSuggestion.explanation.clear();
    m_ghostTextRow = -1;
    m_ghostTextColumn = -1;

    // Redraw editor to remove ghost text overlay
    if (m_hwndEditor && IsWindow(m_hwndEditor)) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
}

bool EditorAgentIntegrationWin32::handleKeyDown(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN) {
        return false;
    }

    // Check for Control key
    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    // TAB: Trigger suggestion
    if (wParam == VK_TAB && !ctrlDown) {
        if (m_ghostTextEnabled && m_agenticExecutor) {
            triggerSuggestion();
            return true; // Consume the TAB key
        }
    }

    // Ctrl+ENTER: Accept suggestion
    if (wParam == VK_RETURN && ctrlDown) {
        if (!m_currentSuggestion.text.empty()) {
            acceptSuggestion();
            return true;
        }
    }

    // ESC: Dismiss suggestion
    if (wParam == VK_ESCAPE) {
        if (!m_currentSuggestion.text.empty()) {
            dismissSuggestion();
            return true;
        }
    }

    // Ctrl+Space: Trigger code completion
    if (wParam == VK_SPACE && ctrlDown) {
        actionCodeCompletion();
        return true;
    }

    // Clear ghost text when typing regular characters
    if (wParam >= 0x20 && wParam <= 0xFF && !ctrlDown) {
        if (!m_currentSuggestion.text.empty()) {
            clearGhostText();
        }
    }

    return false;
}

void EditorAgentIntegrationWin32::showContextMenu(int x, int y)
{
    if (!m_contextMenu) {
        return;
    }

    // Update toggle menu item state
    MENUITEMINFOA mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    mii.fState = m_ghostTextEnabled ? MFS_CHECKED : MFS_UNCHECKED;
    SetMenuItemInfoA(m_contextMenu, CMD_TOGGLE_GHOST_TEXT, FALSE, &mii);

    // Show menu
    TrackPopupMenu(m_contextMenu, TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0, m_hwndEditor, nullptr);
}

bool EditorAgentIntegrationWin32::handleContextMenuCommand(int commandId)
{
    switch (commandId) {
        case CMD_CODE_COMPLETION:
            actionCodeCompletion();
            return true;
        case CMD_EXPLAIN_CODE:
            actionExplainCode();
            return true;
        case CMD_REFACTOR_CODE:
            actionRefactorCode();
            return true;
        case CMD_GENERATE_TESTS:
            actionGenerateTests();
            return true;
        case CMD_FIND_BUGS:
            actionFindBugs();
            return true;
        case CMD_TOGGLE_GHOST_TEXT:
            setGhostTextEnabled(!m_ghostTextEnabled);
            return true;
        default:
            return false;
    }
}

void EditorAgentIntegrationWin32::updateGhostTextDisplay()
{
    if (!m_ghostTextEnabled || m_currentSuggestion.text.empty()) {
        return;
    }

    // TODO: Implement ghost text overlay rendering
    // Options:
    // 1. Use transparent layered window over edit control
    // 2. Use WM_PAINT subclassing to draw ghost text
    // 3. Use owner-draw custom control
    
    // For now, just log that we have a suggestion
    LOG_DEBUG(ProductionLogger::AGENTIC, 
              "Ghost text ready: " + m_currentSuggestion.text.substr(0, 30));
}

void EditorAgentIntegrationWin32::setSuggestionCallback(std::function<void(const std::string&)> callback)
{
    m_suggestionCallback = callback;
}

// ============================================================================
// Private Methods
// ============================================================================

GhostTextContext EditorAgentIntegrationWin32::extractContext() const
{
    GhostTextContext context;
    context.fileType = m_fileType;
    context.currentLine = getCurrentLine();
    context.previousLines = getPreviousLines(10);

    auto cursorPos = getCursorPosition();
    context.cursorColumn = cursorPos.second;

    return context;
}

void EditorAgentIntegrationWin32::generateSuggestion(const GhostTextContext& context)
{
    if (!m_suggestionEngine) {
        LOG_ERROR(ProductionLogger::AGENTIC, "SuggestionEngine not initialized");
        return;
    }

    LOG_DEBUG(ProductionLogger::AGENTIC, "Generating suggestion for: " + context.currentLine);

    // Use the built-in suggestion engine to generate recommendations
    CodeSuggestion suggestion = m_suggestionEngine->generateSuggestion(
        context.currentLine,
        context.previousLines,
        "complete"
    );

    // Store the suggestion
    m_currentSuggestion.text = suggestion.text;
    m_currentSuggestion.explanation = suggestion.explanation;
    m_currentSuggestion.confidence = suggestion.confidence;
    m_currentSuggestion.isComplete = suggestion.isComplete;

    LOG_INFO(ProductionLogger::AGENTIC,
             "Suggestion generated: '" + suggestion.text.substr(0, 50) + 
             "' (confidence: " + std::to_string(suggestion.confidence) + ")");

    auto cursorPos = getCursorPosition();
    m_ghostTextRow = cursorPos.first;
    m_ghostTextColumn = cursorPos.second;

    renderGhostText(m_currentSuggestion.text, m_ghostTextRow, m_ghostTextColumn);
}

void EditorAgentIntegrationWin32::renderGhostText(const std::string& text, int row, int column)
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        LOG_WARNING(ProductionLogger::AGENTIC, "Cannot render ghost text: invalid editor HWND");
        return;
    }
    
    m_ghostTextRow = row;
    m_ghostTextColumn = column;
    m_ghostTextVisible = true;
    
    // Get editor DC for rendering
    HDC hdc = GetDC(m_hwndEditor);
    if (!hdc) {
        LOG_WARNING(ProductionLogger::AGENTIC, "Failed to get editor device context");
        return;
    }
    
    try {
        // Get current editor content to find actual cursor position
        int editorLength = (int)SendMessageA(m_hwndEditor, WM_GETTEXTLENGTH, 0, 0);
        
        // Get current cursor position as character offset
        DWORD startSel = 0, endSel = 0;
        SendMessageA(m_hwndEditor, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
        
        // Get character format to determine line height
        CHARFORMATA cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_SIZE;
        SendMessageA(m_hwndEditor, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        // Set up font for ghost text (italic, light gray)
        HFONT oldFont = (HFONT)SelectObject(hdc, m_ghostTextFont);
        COLORREF oldColor = SetTextColor(hdc, RGB(128, 128, 128));
        int oldBkMode = SetBkMode(hdc, TRANSPARENT);
        
        // Get editor client area
        RECT clientRect;
        GetClientRect(m_hwndEditor, &clientRect);
        
        // Calculate approximate position based on character count
        // Each character is roughly 8 pixels wide in Consolas 11pt
        // Each line is roughly 16 pixels tall
        int estimatedCharWidth = 8;
        int estimatedLineHeight = 16;
        
        int posX = clientRect.left + 5 + (column * estimatedCharWidth);
        int posY = clientRect.top + 5 + (row * estimatedLineHeight);
        
        // Clamp to visible area
        if (posX < clientRect.left + 5) posX = clientRect.left + 5;
        if (posX > clientRect.right - 100) posX = clientRect.right - 100;
        if (posY < clientRect.top + 5) posY = clientRect.top + 5;
        if (posY > clientRect.bottom - 20) posY = clientRect.bottom - 20;
        
        // Draw ghost text with ellipsis if too long
        std::string displayText = text;
        if (displayText.length() > 60) {
            displayText = text.substr(0, 60) + "...";
        }
        
        // Replace newlines with space for single-line display
        for (char& c : displayText) {
            if (c == '\n' || c == '\r') c = ' ';
        }
        
        TextOutA(hdc, posX, posY, displayText.c_str(), (int)displayText.length());
        
        // Restore DC state
        SetBkMode(hdc, oldBkMode);
        SetTextColor(hdc, oldColor);
        SelectObject(hdc, oldFont);
        
        LOG_DEBUG(ProductionLogger::AGENTIC, 
                  "Ghost text rendered: '" + displayText + "' at pixel (" + 
                  std::to_string(posX) + "," + std::to_string(posY) + ")");
    } catch (const std::exception& e) {
        LOG_ERROR(ProductionLogger::AGENTIC, "Exception rendering ghost text: " + std::string(e.what()));
    }
    
    ReleaseDC(m_hwndEditor, hdc);
}

std::pair<int, int> EditorAgentIntegrationWin32::getCursorPosition() const
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        return {0, 0};
    }

    // Get current selection (cursor position)
    DWORD start = 0, end = 0;
    SendMessageA(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    // Get line number from character position
    int lineNumber = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, start, 0);
    
    // Get line start position
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineNumber, 0);
    
    // Column is offset from line start
    int column = (int)(start - lineStart);

    return {lineNumber, column};
}

std::string EditorAgentIntegrationWin32::getCurrentLine() const
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        return "";
    }

    auto cursorPos = getCursorPosition();
    int lineNumber = cursorPos.first;

    // Get line length
    int lineLength = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, lineNumber, 0);
    if (lineLength <= 0) {
        return "";
    }

    // Get line text
    std::vector<char> buffer(lineLength + 1, 0);
    *((WORD*)buffer.data()) = (WORD)lineLength;
    
    int copied = (int)SendMessageA(m_hwndEditor, EM_GETLINE, lineNumber, (LPARAM)buffer.data());
    if (copied > 0) {
        return std::string(buffer.data(), copied);
    }

    return "";
}

std::string EditorAgentIntegrationWin32::getPreviousLines(int lineCount) const
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        return "";
    }

    auto cursorPos = getCursorPosition();
    int currentLine = cursorPos.first;

    std::ostringstream result;
    int startLine = (currentLine > lineCount) ? (currentLine - lineCount) : 0;

    for (int i = startLine; i < currentLine; ++i) {
        int lineLength = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, i, 0);
        if (lineLength > 0) {
            std::vector<char> buffer(lineLength + 1, 0);
            *((WORD*)buffer.data()) = (WORD)lineLength;
            
            int copied = (int)SendMessageA(m_hwndEditor, EM_GETLINE, i, (LPARAM)buffer.data());
            if (copied > 0) {
                result << std::string(buffer.data(), copied) << "\n";
            }
        }
    }

    return result.str();
}

void EditorAgentIntegrationWin32::insertTextAtCursor(const std::string& text)
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        return;
    }

    // Replace current selection with text
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());
}

void EditorAgentIntegrationWin32::actionCodeCompletion()
{
    LOG_INFO(ProductionLogger::AGENTIC, "Action: Code Completion");
    triggerSuggestion();
}

void EditorAgentIntegrationWin32::actionExplainCode()
{
    LOG_INFO(ProductionLogger::AGENTIC, "Action: Explain Code");
    
    if (!m_agenticExecutor) {
        return;
    }

    // Get selected text or current line
    std::string code = getCurrentLine();
    std::string prompt = "Explain this code:\n" + code;

    // Execute via agentic executor
    // Result would be displayed in chat panel
}

void EditorAgentIntegrationWin32::actionRefactorCode()
{
    LOG_INFO(ProductionLogger::AGENTIC, "Action: Refactor Code");
    
    if (!m_agenticExecutor) {
        return;
    }

    std::string code = getCurrentLine();
    std::string prompt = "Suggest refactoring for:\n" + code;

    // Execute via agentic executor
}

void EditorAgentIntegrationWin32::actionGenerateTests()
{
    LOG_INFO(ProductionLogger::AGENTIC, "Action: Generate Tests");
    
    if (!m_agenticExecutor) {
        return;
    }

    std::string code = getCurrentLine();
    std::string prompt = "Generate unit tests for:\n" + code;

    // Execute via agentic executor
}

void EditorAgentIntegrationWin32::actionFindBugs()
{
    LOG_INFO(ProductionLogger::AGENTIC, "Action: Find Bugs");
    
    if (!m_agenticExecutor) {
        return;
    }

    std::string code = getCurrentLine();
    std::string prompt = "Find bugs or issues in:\n" + code;

    // Execute via agentic executor
}

#endif // _WIN32


