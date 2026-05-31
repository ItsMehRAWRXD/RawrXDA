// ============================================================================
// Win32IDE_EditorCommands.cpp — Unified Editor Operations Dispatch System
// ============================================================================
// Centralized command registry and execution system for all editor operations:
//   - Refactoring (extract function/variable, rename)
//   - Code folding/unfolding
//   - Comment/uncomment operations
//   - Auto-formatting
//   - Bracket matching
//   - Multi-cursor operations
//
// Architecture: Each operation is a pure function + registry entry.
// Operations are keybinding-agnostic (can be triggered by menu, hotkey, or LSP)
// ============================================================================

#include "Win32IDE.h"
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <regex>

// ============================================================================
// EDITOR OPERATION REGISTRY
// ============================================================================

struct EditorOperation
{
    std::string name;
    std::function<bool(Win32IDE& ide)> execute;
    std::string description;
    bool needsSelection;
    std::string defaultKeybinding;
};

static std::unordered_map<std::string, EditorOperation> g_editorOps;

void Win32IDE::registerEditorOperation(
    const std::string& name,
    std::function<bool(Win32IDE&)> execute,
    const std::string& description,
    bool needsSelection,
    const std::string& keybinding)
{
    g_editorOps[name] = {name, execute, description, needsSelection, keybinding};
    logInfo("[Editor] Registered operation: " + name);
}

bool Win32IDE::executeEditorOperation(const std::string& opName)
{
    auto it = g_editorOps.find(opName);
    if (it == g_editorOps.end())
    {
        logWarning("executeEditorOperation", "Operation not found: " + opName);
        return false;
    }

    const auto& op = it->second;
    
    // Check if operation requires selection
    if (op.needsSelection)
    {
        CHARRANGE sel;
        SendMessage(m_hRichEdit, EM_EXGETSEL, 0, (LPARAM)&sel);
        if (sel.cpMin == sel.cpMax)
        {
            appendToOutput("[Editor] " + op.name + " requires text selection.", "General", OutputSeverity::Warning);
            return false;
        }
    }

    try
    {
        bool result = op.execute(*this);
        if (result)
        {
            logInfo("[Editor] Operation succeeded: " + op.name);
        }
        return result;
    }
    catch (const std::exception& ex)
    {
        logError("executeEditorOperation", op.name + " failed: " + std::string(ex.what()));
        return false;
    }
}

// ============================================================================
// COMMENT/UNCOMMENT OPERATIONS
// ============================================================================

bool Win32IDE::operationToggleComment()
{
    if (m_currentFile.empty())
        return false;

    CHARRANGE sel;
    SendMessage(m_hRichEdit, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Get line range from selection
    int startLine = m_editorBuffer.getLineFromPos(sel.cpMin);
    int endLine = m_editorBuffer.getLineFromPos(sel.cpMax);

    std::string commentPrefix = getCommentPrefixForFile(m_currentFile);
    if (commentPrefix.empty())
    {
        appendToOutput("[Editor] Comment syntax unknown for this file type.", "General", OutputSeverity::Warning);
        return false;
    }

    bool isCommented = false;
    std::string firstLineText = m_editorBuffer.getLine(startLine);
    
    // Check if first line is already commented
    size_t trimPos = firstLineText.find_first_not_of(" \t");
    if (trimPos != std::string::npos)
    {
        isCommented = (firstLineText.substr(trimPos, commentPrefix.length()) == commentPrefix);
    }

    // Toggle comments on all selected lines
    for (int lineIdx = startLine; lineIdx <= endLine && lineIdx < m_editorBuffer.lineCount(); ++lineIdx)
    {
        std::string line = m_editorBuffer.getLine(lineIdx);
        if (line.empty())
            continue;

        size_t insertPos = line.find_first_not_of(" \t");
        if (insertPos == std::string::npos)
            insertPos = 0;

        if (isCommented)
        {
            // Remove comment prefix
            if (line.substr(insertPos, commentPrefix.length()) == commentPrefix)
            {
                line.erase(insertPos, commentPrefix.length());
            }
        }
        else
        {
            // Add comment prefix
            line.insert(insertPos, commentPrefix + " ");
        }

        m_editorBuffer.replaceLine(lineIdx, line);
    }

    // Mark as modified
    m_modified = true;
    InvalidateRect(m_hRichEdit, nullptr, FALSE);

    appendToOutput("[Editor] " + std::string(isCommented ? "Uncommented" : "Commented") + " " +
                   std::to_string(endLine - startLine + 1) + " line(s).",
                   "General", OutputSeverity::Info);
    return true;
}

std::string Win32IDE::getCommentPrefixForFile(const std::string& filePath) const
{
    std::string ext = filePath;
    size_t lastDot = ext.find_last_of(".");
    if (lastDot == std::string::npos)
        return "";

    ext = ext.substr(lastDot);

    // Language-specific comment prefixes
    if (ext == ".cpp" || ext == ".h" || ext == ".c" || ext == ".hpp")
        return "//";
    if (ext == ".py")
        return "#";
    if (ext == ".js" || ext == ".ts" || ext == ".jsx" || ext == ".tsx")
        return "//";
    if (ext == ".html" || ext == ".xml" || ext == ".xaml")
        return "<!--";
    if (ext == ".json")
        return ""; // JSON doesn't support comments (standard)
    if (ext == ".ps1")
        return "#";
    if (ext == ".asm" || ext == ".s" || ext == ".asm64")
        return ";";

    return "//";  // Default to C++ style
}

// ============================================================================
// BRACKET MATCHING & HIGHLIGHTING
// ============================================================================

bool Win32IDE::operationFindMatchingBracket()
{
    CHARRANGE sel;
    SendMessage(m_hRichEdit, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin >= sel.cpMax)
    {
        appendToOutput("[Editor] No bracket selected.", "General", OutputSeverity::Info);
        return false;
    }

    std::string allText = m_editorBuffer.getText();
    if (sel.cpMin >= (int)allText.length())
        return false;

    char openChar = allText[sel.cpMin];
    char closeChar = 0;
    int direction = 1;

    // Determine bracket type and search direction
    if (openChar == '(' || openChar == '[' || openChar == '{')
    {
        if (openChar == '(') closeChar = ')';
        else if (openChar == '[') closeChar = ']';
        else if (openChar == '{') closeChar = '}';
        direction = 1;  // Search forward
    }
    else if (openChar == ')' || openChar == ']' || openChar == '}')
    {
        if (openChar == ')') openChar = '(';
        else if (openChar == ']') openChar = '[';
        else if (openChar == '}') openChar = '{';
        direction = -1;  // Search backward
    }
    else
    {
        return false;  // Not a bracket
    }

    // Find matching bracket
    int depth = 1;
    int matchPos = -1;

    if (direction == 1)
    {
        for (int i = sel.cpMin + 1; i < (int)allText.length() && depth > 0; ++i)
        {
            if (allText[i] == openChar) depth++;
            else if (allText[i] == closeChar)
            {
                depth--;
                if (depth == 0) matchPos = i;
            }
        }
    }
    else
    {
        for (int i = sel.cpMin - 1; i >= 0 && depth > 0; --i)
        {
            if (allText[i] == openChar) depth++;
            else if (allText[i] == closeChar)
            {
                depth--;
                if (depth == 0) matchPos = i;
            }
        }
    }

    if (matchPos >= 0)
    {
        // Highlight matching bracket with visual feedback
        CHARRANGE match;
        match.cpMin = matchPos;
        match.cpMax = matchPos + 1;
        SendMessage(m_hRichEdit, EM_EXSETSEL, 0, (LPARAM)&match);
        
        // Apply bold formatting to both bracket and match  
        CHARFORMAT cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_BOLD | CFM_BACKCOLOR;
        cf.dwEffects = CFE_BOLD;
        cf.crBackColor = RGB(200, 220, 255);  // Light blue background
        
        SendMessage(m_hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        // Also highlight the original bracket
        SendMessage(m_hRichEdit, EM_EXSETSEL, 0, (LPARAM)&sel);
        SendMessage(m_hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        // Scroll to show matched bracket
        SendMessage(m_hRichEdit, EM_SCROLLCARET, 0, 0);
        
        appendToOutput("[Editor] Bracket matched at position " + std::to_string(matchPos) + ".", "General", OutputSeverity::Info);
        return true;
    }

    appendToOutput("[Editor] No matching bracket found.", "General", OutputSeverity::Warning);
    return false;
}

// ============================================================================
// CODE FOLDING OPERATIONS
// ============================================================================

struct RegiionMarker
{
    int startLine;
    int endLine;
    std::string name;
    bool folded;
};

bool Win32IDE::operationFoldRegion()
{
    // Find the current region containing cursor position
    CHARRANGE sel;
    SendMessage(m_hRichEdit, EM_EXGETSEL, 0, (LPARAM)&sel);
    int currentLine = m_editorBuffer.getLineFromPos(sel.cpMin);

    // Look for region markers: #region, #pragma region, or { ... }
    std::string text = m_editorBuffer.getText();
    std::regex regionStart(R"(#\s*region\s+(\w+)|{\s*\/\/\s*region\s+(\w+))");
    std::regex regionEnd(R"(#\s*endregion|{\s*\/\/\s*endregion)");

    // Find matching fold region and collapse it
    // (Implementation would scan tokens and set fold markers)
    
    appendToOutput("[Editor] Code folding - implementation pending folding map.", "General", OutputSeverity::Info);
    return false;
}

bool Win32IDE::operationUnfoldRegion()
{
    // Similar to fold, but expand region
    appendToOutput("[Editor] Region unfolding available once fold markers are set.", "General", OutputSeverity::Info);
    return false;
}

// ============================================================================
// AUTO-INDENTATION
// ============================================================================

bool Win32IDE::operationAutoIndent()
{
    CHARRANGE sel;
    SendMessage(m_hRichEdit, EM_EXGETSEL, 0, (LPARAM)&sel);
    int lineIdx = m_editorBuffer.getLineFromPos(sel.cpMin);

    if (lineIdx <= 0)
        return false;

    std::string prevLine = m_editorBuffer.getLine(lineIdx - 1);
    std::string currLine = m_editorBuffer.getLine(lineIdx);

    // Get previous line's indentation
    size_t prevIndent = 0;
    while (prevIndent < prevLine.length() && (prevLine[prevIndent] == ' ' || prevLine[prevIndent] == '\t'))
        prevIndent++;

    // Check if previous line ends with {, [, or (
    size_t lastNonWhite = prevLine.find_last_not_of(" \t\r\n");
    int indentChange = 0;
    if (lastNonWhite != std::string::npos)
    {
        if (prevLine[lastNonWhite] == '{' || prevLine[lastNonWhite] == '[' || prevLine[lastNonWhite] == '(')
            indentChange = 4;  // Add one indent level
    }

    // Check if current line starts with }, ], or )
    size_t currFirstNonWhite = currLine.find_first_not_of(" \t");
    if (currFirstNonWhite != std::string::npos)
    {
        if (currLine[currFirstNonWhite] == '}' || currLine[currFirstNonWhite] == ']' || currLine[currFirstNonWhite] == ')')
            indentChange -= 4;
    }

    // Apply indentation
    int newIndent = std::max(0, (int)prevIndent + indentChange);
    std::string indentation(newIndent, ' ');
    currLine = indentation + currLine.substr(currFirstNonWhite != std::string::npos ? currFirstNonWhite : 0);

    m_editorBuffer.replaceLine(lineIdx, currLine);
    m_modified = true;

    return true;
}

// ============================================================================
// INITIALIZATION: Register all editor operations on startup
// ============================================================================

void Win32IDE::initEditorCommandSystem()
{
    logFunction("initEditorCommandSystem");

    // Comment/Uncomment
    registerEditorOperation(
        "editor.toggleComment",
        [](Win32IDE& ide) { return ide.operationToggleComment(); },
        "Toggle comment on selected lines",
        true,
        "Ctrl+/"
    );

    // Bracket Matching
    registerEditorOperation(
        "editor.findMatchingBracket",
        [](Win32IDE& ide) { return ide.operationFindMatchingBracket(); },
        "Find matching bracket",
        false,
        "Ctrl+Shift+\\"
    );

    // Code Folding
    registerEditorOperation(
        "editor.foldRegion",
        [](Win32IDE& ide) { return ide.operationFoldRegion(); },
        "Fold code region",
        false,
        "Ctrl+Shift+]"
    );

    registerEditorOperation(
        "editor.unfoldRegion",
        [](Win32IDE& ide) { return ide.operationUnfoldRegion(); },
        "Unfold code region",
        false,
        "Ctrl+Shift+["
    );

    // Auto-Indentation
    registerEditorOperation(
        "editor.autoIndent",
        [](Win32IDE& ide) { return ide.operationAutoIndent(); },
        "Auto-indent current line",
        false,
        ""
    );

    logInfo("[Editor] Command system initialized with " + std::to_string(g_editorOps.size()) + " operations");
    
    // Initialize refactoring engine (wires 3 refactoring operations)
    initRefactoringEngine();
}
