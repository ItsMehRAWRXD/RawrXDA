// ============================================================================
// Win32IDE_OutlinePanel.cpp — Feature 15: Document Symbols Outline
// Enhanced outline view with filter, sort, icons, and LSP symbol kinds
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <functional>

// ── LSP Symbol Kinds — now declared in Win32IDE.h (Tier 2) ─────────────────

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF OUTLINE_BG            = RGB(37, 37, 38);
static const COLORREF OUTLINE_TEXT          = RGB(204, 204, 204);
static const COLORREF OUTLINE_FILTER_BG     = RGB(60, 60, 60);
static const COLORREF OUTLINE_SELECTED      = RGB(4, 57, 94);
static const COLORREF OUTLINE_HOVER         = RGB(45, 45, 46);
static const COLORREF OUTLINE_BORDER        = RGB(60, 60, 60);

// Symbol kind colors (VS Code style)
static COLORREF symbolKindColor(int kind) {
    switch (kind) {
    case Win32IDE::SK_Class:       case Win32IDE::SK_Interface: case Win32IDE::SK_Struct: return RGB(78, 201, 176);
    case Win32IDE::SK_Method:      case Win32IDE::SK_Function:  case Win32IDE::SK_Constructor: return RGB(220, 220, 170);
    case Win32IDE::SK_Variable:    case Win32IDE::SK_Field:     case Win32IDE::SK_Property: return RGB(156, 220, 254);
    case Win32IDE::SK_Enum:        case Win32IDE::SK_EnumMember: return RGB(184, 215, 163);
    case Win32IDE::SK_Constant:    return RGB(100, 150, 224);
    case Win32IDE::SK_Namespace:   case Win32IDE::SK_Module: case Win32IDE::SK_Package: return RGB(200, 200, 200);
    default:             return RGB(204, 204, 204);
    }
}

// Symbol kind icon text
static const char* symbolKindIcon(int kind) {
    switch (kind) {
    case Win32IDE::SK_Class:       return "C";
    case Win32IDE::SK_Interface:   return "I";
    case Win32IDE::SK_Struct:      return "S";
    case Win32IDE::SK_Method:      return "m";
    case Win32IDE::SK_Function:    return "f";
    case Win32IDE::SK_Constructor: return "c";
    case Win32IDE::SK_Variable:    return "v";
    case Win32IDE::SK_Field:       return "F";
    case Win32IDE::SK_Property:    return "p";
    case Win32IDE::SK_Enum:        return "E";
    case Win32IDE::SK_EnumMember:  return "e";
    case Win32IDE::SK_Constant:    return "K";
    case Win32IDE::SK_Namespace:   return "N";
    case Win32IDE::SK_Module:      return "M";
    default:             return ".";
    }
}

// ── Init ───────────────────────────────────────────────────────────────────
void Win32IDE::initOutlinePanel() {
    m_outlineSymbols.clear();
    m_outlineFilter.clear();
    m_outlineSortByName = false;
    LOG_INFO("[OutlinePanel] Initialized with enhanced features");
}

// ── Refresh from LSP ───────────────────────────────────────────────────────
void Win32IDE::refreshOutlineFromLSP() {
    m_outlineSymbols.clear();

    if (m_currentFile.empty()) return;

    // Try LSP textDocument/documentSymbol first
    // Fallback: parse current file locally
    if (!m_hwndEditor) return;

    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0) return;

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    // Simple parser for C/C++ symbols
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    OutlineSymbol* currentClass = nullptr;

    while (std::getline(stream, line)) {
        lineNum++;
        if (line.empty()) continue;

        // Strip leading whitespace for analysis
        size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string::npos) continue;
        std::string trimmed = line.substr(indent);

        // Class/struct detection
        if (trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) {
            OutlineSymbol sym;
            sym.kind = (trimmed[0] == 'c') ? SK_Class : SK_Struct;
            // Extract name
            size_t nameStart = trimmed.find(' ') + 1;
            size_t nameEnd = trimmed.find_first_of(" :{;", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
            sym.line = lineNum;
            sym.column = (int)indent;
            sym.endLine = lineNum;
            sym.detail = (sym.kind == SK_Class) ? "class" : "struct";
            m_outlineSymbols.push_back(sym);
            currentClass = &m_outlineSymbols.back();
            continue;
        }

        // Enum detection
        if (trimmed.find("enum ") == 0) {
            OutlineSymbol sym;
            sym.kind = SK_Enum;
            size_t nameStart = trimmed.find(' ') + 1;
            if (trimmed.find("class ") == 5) nameStart = trimmed.find(' ', 5) + 1;
            size_t nameEnd = trimmed.find_first_of(" :{;", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
            sym.line = lineNum;
            sym.column = (int)indent;
            sym.endLine = lineNum;
            sym.detail = "enum";
            m_outlineSymbols.push_back(sym);
            continue;
        }

        // Namespace detection
        if (trimmed.find("namespace ") == 0) {
            OutlineSymbol sym;
            sym.kind = SK_Namespace;
            size_t nameStart = 10;
            size_t nameEnd = trimmed.find_first_of(" {", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
            sym.line = lineNum;
            sym.column = (int)indent;
            sym.endLine = lineNum;
            sym.detail = "namespace";
            m_outlineSymbols.push_back(sym);
            continue;
        }

        // Function detection (simplified: looks for `type name(` pattern)
        size_t parenPos = trimmed.find('(');
        if (parenPos != std::string::npos && parenPos > 1) {
            // Check it's not a keyword
            if (trimmed.find("if") == 0 || trimmed.find("for") == 0 ||
                trimmed.find("while") == 0 || trimmed.find("switch") == 0 ||
                trimmed.find("return") == 0 || trimmed.find("//") == 0 ||
                trimmed.find("#") == 0) continue;

            // Extract function name (word before paren)
            size_t nameEnd = parenPos;
            while (nameEnd > 0 && trimmed[nameEnd-1] == ' ') nameEnd--;
            size_t nameStart = nameEnd;
            while (nameStart > 0 && (isalnum(trimmed[nameStart-1]) || trimmed[nameStart-1] == '_'
                   || trimmed[nameStart-1] == ':'))
                nameStart--;

            std::string funcName = trimmed.substr(nameStart, nameEnd - nameStart);
            if (funcName.empty() || funcName == "new" || funcName == "delete") continue;

            // Check if it's a definition (has { or is followed by {)
            bool isDefinition = trimmed.find('{') != std::string::npos ||
                                trimmed.find(';') == std::string::npos;

            OutlineSymbol sym;
            sym.kind = SK_Function;
            sym.name = funcName;
            sym.line = lineNum;
            sym.column = (int)(indent + nameStart);
            sym.endLine = lineNum;

            // Check for common method patterns
            if (funcName.find("::") != std::string::npos) {
                sym.kind = SK_Method;
                sym.detail = "method";
            } else {
                sym.detail = "function";
            }

            // Add: return type as detail
            if (nameStart > 0) {
                std::string prefix = trimmed.substr(0, nameStart);
                size_t typeEnd = prefix.find_last_not_of(" \t");
                if (typeEnd != std::string::npos) {
                    sym.detail = prefix.substr(0, typeEnd + 1) + " → " + sym.detail;
                }
            }

            // Add to class or top-level
            if (currentClass && indent > 0) {
                currentClass->children.push_back(sym);
            } else {
                m_outlineSymbols.push_back(sym);
            }
        }

        // #define constant detection
        if (trimmed.find("#define ") == 0) {
            OutlineSymbol sym;
            sym.kind = SK_Constant;
            size_t nameStart = 8;
            size_t nameEnd = trimmed.find_first_of(" (", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            sym.name = trimmed.substr(nameStart, nameEnd - nameStart);
            sym.line = lineNum;
            sym.column = 0;
            sym.endLine = lineNum;
            sym.detail = "#define";
            m_outlineSymbols.push_back(sym);
        }
    }

    // Update TreeView if exists
    if (m_hwndOutlineTree) {
        TreeView_DeleteAllItems(m_hwndOutlineTree);

        std::function<void(const std::vector<OutlineSymbol>&, HTREEITEM)> populateTree;
        populateTree = [&](const std::vector<OutlineSymbol>& symbols, HTREEITEM parent) {
            for (auto& sym : symbols) {
                // Apply filter
                if (!m_outlineFilter.empty()) {
                    std::string lowerName = sym.name;
                    std::string lowerFilter = m_outlineFilter;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                    if (lowerName.find(lowerFilter) == std::string::npos) continue;
                }

                // Build display text: "[icon] name : detail"
                std::string displayText = std::string(symbolKindIcon(sym.kind)) + "  " + sym.name;
                if (!sym.detail.empty()) displayText += "  (" + sym.detail + ")";

                TVINSERTSTRUCTA tvis = {};
                tvis.hParent = parent;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                tvis.item.pszText = (char*)displayText.c_str();
                tvis.item.lParam = (LPARAM)sym.line;

                HTREEITEM hItem = TreeView_InsertItem(m_hwndOutlineTree, &tvis);

                if (!sym.children.empty()) {
                    populateTree(sym.children, hItem);
                    if (sym.expanded) TreeView_Expand(m_hwndOutlineTree, hItem, TVE_EXPAND);
                }
            }
        };

        auto sorted = m_outlineSymbols;
        if (m_outlineSortByName) {
            std::sort(sorted.begin(), sorted.end(),
                [](const OutlineSymbol& a, const OutlineSymbol& b) { return a.name < b.name; });
        }
        populateTree(sorted, TVI_ROOT);
    }

    LOG_INFO("[OutlinePanel] Refreshed: " + std::to_string(m_outlineSymbols.size()) + " symbols");
}

// ── Filter & Sort ──────────────────────────────────────────────────────────
void Win32IDE::filterOutlineView(const std::string& filter) {
    m_outlineFilter = filter;
    refreshOutlineFromLSP();
}

void Win32IDE::sortOutlineView(bool byName) {
    m_outlineSortByName = byName;
    refreshOutlineFromLSP();
}

// ── Icon Rendering ─────────────────────────────────────────────────────────
void Win32IDE::renderOutlineIcons(HDC hdc, int kind, RECT iconRect) {
    COLORREF color = symbolKindColor(kind);
    const char* icon = symbolKindIcon(kind);

    // Draw colored circle background
    HBRUSH circleBrush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, circleBrush);
    int cx = (iconRect.left + iconRect.right) / 2;
    int cy = (iconRect.top + iconRect.bottom) / 2;
    int radius = 7;
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, oldBrush);
    DeleteObject(circleBrush);

    // Draw letter in center
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(30, 30, 30));
    RECT textRect = iconRect;
    DrawTextA(hdc, icon, 1, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

// ── Helpers ────────────────────────────────────────────────────────────────
std::string Win32IDE::outlineKindToString(int kind) const {
    switch (kind) {
    case SK_File:        return "File";
    case SK_Module:      return "Module";
    case SK_Namespace:   return "Namespace";
    case SK_Class:       return "Class";
    case SK_Method:      return "Method";
    case SK_Property:    return "Property";
    case SK_Field:       return "Field";
    case SK_Constructor: return "Constructor";
    case SK_Enum:        return "Enum";
    case SK_Interface:   return "Interface";
    case SK_Function:    return "Function";
    case SK_Variable:    return "Variable";
    case SK_Constant:    return "Constant";
    case SK_Struct:      return "Struct";
    case SK_EnumMember:  return "Enum Member";
    default:             return "Symbol";
    }
}

COLORREF Win32IDE::outlineKindColor(int kind) const {
    return symbolKindColor(kind);
}
