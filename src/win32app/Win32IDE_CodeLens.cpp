// ============================================================================
// Win32IDE_CodeLens.cpp — Feature 18: CodeLens (Reference Counts)
// Phantom "N references" text rendered above function declarations
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <regex>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF CODELENS_TEXT        = RGB(150, 150, 150);
static const COLORREF CODELENS_HOVER_TEXT  = RGB(100, 185, 255);
static const COLORREF CODELENS_SEPARATOR   = RGB(60, 60, 60);

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initCodeLens() {
    m_codeLensEntries.clear();
    m_codeLensEnabled = true;
    m_codeLensFont = CreateFontA(-dpiScale(11), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    LOG_INFO("[CodeLens] Initialized");
}

void Win32IDE::shutdownCodeLens() {
    m_codeLensEntries.clear();
    if (m_codeLensFont) { DeleteObject(m_codeLensFont); m_codeLensFont = nullptr; }
    LOG_INFO("[CodeLens] Shutdown");
}

// ── Refresh CodeLens ───────────────────────────────────────────────────────
void Win32IDE::refreshCodeLens() {
    m_codeLensEntries.clear();
    if (!m_codeLensEnabled || !m_hwndEditor) return;

    // Get editor content
    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0 || totalLen > 500000) return; // Skip enormous files

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    // Parse for function/method/class declarations
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    std::vector<std::pair<std::string, int>> declarations; // name, line

    while (std::getline(stream, line)) {
        lineNum++;

        // Skip empty/comment lines
        size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string::npos) continue;
        std::string trimmed = line.substr(indent);
        if (trimmed.empty() || trimmed[0] == '/' || trimmed[0] == '*' || trimmed[0] == '#') continue;

        // Class/struct declarations
        if (trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) {
            size_t nameStart = trimmed.find(' ') + 1;
            size_t nameEnd = trimmed.find_first_of(" :{;", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            std::string name = trimmed.substr(nameStart, nameEnd - nameStart);
            if (!name.empty() && name != "{") {
                declarations.push_back({name, lineNum});
            }
            continue;
        }

        // Function definitions — look for "word(" pattern that's a definition
        size_t parenPos = trimmed.find('(');
        if (parenPos != std::string::npos && parenPos > 1) {
            // Skip control structures
            if (trimmed.find("if") == 0 || trimmed.find("for") == 0 ||
                trimmed.find("while") == 0 || trimmed.find("switch") == 0 ||
                trimmed.find("return") == 0 || trimmed.find("case") == 0) continue;

            // Check if it looks like a definition (has { somewhere)
            bool hasBody = trimmed.find('{') != std::string::npos ||
                           trimmed.find(';') == std::string::npos;

            if (hasBody) {
                // Extract function name
                size_t nameEnd = parenPos;
                while (nameEnd > 0 && trimmed[nameEnd-1] == ' ') nameEnd--;
                size_t nameStart = nameEnd;
                while (nameStart > 0 && (isalnum(trimmed[nameStart-1]) ||
                       trimmed[nameStart-1] == '_' || trimmed[nameStart-1] == ':'))
                    nameStart--;

                std::string funcName = trimmed.substr(nameStart, nameEnd - nameStart);
                // Strip class prefix for matching
                size_t colonPos = funcName.rfind("::");
                std::string shortName = (colonPos != std::string::npos)
                    ? funcName.substr(colonPos + 2) : funcName;

                if (!shortName.empty() && shortName != "new" && shortName != "delete" &&
                    shortName != "main") {
                    declarations.push_back({shortName, lineNum});
                }
            }
        }
    }

    // Count references for each declaration
    for (auto& [name, declLine] : declarations) {
        // Simple count: how many times does this name appear in the content?
        int count = 0;
        size_t pos = 0;
        while ((pos = content.find(name, pos)) != std::string::npos) {
            // Verify it's a whole word match
            bool wholeWord = true;
            if (pos > 0 && (isalnum(content[pos-1]) || content[pos-1] == '_'))
                wholeWord = false;
            size_t end = pos + name.size();
            if (end < content.size() && (isalnum(content[end]) || content[end] == '_'))
                wholeWord = false;

            if (wholeWord) count++;
            pos = end;
        }

        // Subtract 1 for the declaration itself
        int refCount = (std::max)(0, count - 1);

        CodeLensEntry item;
        item.line = declLine;
        item.referenceCount = refCount;
        item.color = CODELENS_TEXT;
        item.command = "findReferences";

        if (refCount == 0) {
            item.text = "0 references";
        } else if (refCount == 1) {
            item.text = "1 reference";
        } else {
            item.text = std::to_string(refCount) + " references";
        }

        m_codeLensEntries.push_back(item);
    }

    // Invalidate editor for repaint
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, NULL, FALSE);

    LOG_INFO("[CodeLens] Computed " + std::to_string(m_codeLensEntries.size()) + " items");
}

// ── Render (called from editor paint) ──────────────────────────────────────
void Win32IDE::renderCodeLens(HDC hdc, int lineY, int lineNumber) {
    if (!m_codeLensEnabled || m_codeLensEntries.empty()) return;

    // Find CodeLens for this line
    for (auto& item : m_codeLensEntries) {
        if (item.line == lineNumber) {
            // Render above the line
            HFONT oldFont = (HFONT)SelectObject(hdc, m_codeLensFont);
            SetBkMode(hdc, TRANSPARENT);

            TEXTMETRICA tm;
            GetTextMetricsA(hdc, &tm);
            int codeLensH = tm.tmHeight;

            // Position: above the current line, with some margin
            int renderY = lineY - codeLensH - 2;
            int renderX = dpiScale(60); // After gutter

            // Draw text
            SetTextColor(hdc, item.color);
            TextOutA(hdc, renderX, renderY, item.text.c_str(), (int)item.text.size());

            SelectObject(hdc, oldFont);
            break;
        }
    }
}

// ── Click Handler ──────────────────────────────────────────────────────────
void Win32IDE::onCodeLensClick(int line) {
    // Find the CodeLens item for this line
    for (auto& item : m_codeLensEntries) {
        if (item.line == line) {
            if (item.command == "findReferences") {
                // Trigger find references for the symbol on this line
                if (m_hwndEditor) {
                    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line - 1, 0);
                    CHARRANGE cr = {lineIndex, lineIndex};
                    SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
                    cmdLSPFindReferences();
                }
            }
            break;
        }
    }
}

// ── Compute for specific function ──────────────────────────────────────────
void Win32IDE::computeCodeLensForFunction(const std::string& funcName, int line) {
    if (!m_hwndEditor) return;

    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0) return;

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    // Count occurrences
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find(funcName, pos)) != std::string::npos) {
        bool wholeWord = true;
        if (pos > 0 && (isalnum(content[pos-1]) || content[pos-1] == '_'))
            wholeWord = false;
        size_t end = pos + funcName.size();
        if (end < content.size() && (isalnum(content[end]) || content[end] == '_'))
            wholeWord = false;
        if (wholeWord) count++;
        pos = end;
    }

    int refCount = (std::max)(0, count - 1);

    // Update or add item
    for (auto& item : m_codeLensEntries) {
        if (item.line == line) {
            item.referenceCount = refCount;
            item.text = std::to_string(refCount) + (refCount == 1 ? " reference" : " references");
            return;
        }
    }

    CodeLensEntry item;
    item.line = line;
    item.referenceCount = refCount;
    item.text = std::to_string(refCount) + (refCount == 1 ? " reference" : " references");
    item.color = CODELENS_TEXT;
    item.command = "findReferences";
    m_codeLensEntries.push_back(item);
}
