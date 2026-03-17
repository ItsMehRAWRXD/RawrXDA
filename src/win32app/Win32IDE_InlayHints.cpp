// ============================================================================
// Win32IDE_InlayHints.cpp — Feature 19: Inlay Type Hints
// Ghost text for auto-inferred types and parameter names
// Extends the ghost text renderer for type annotations
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <regex>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF INLAY_TYPE_COLOR     = RGB(104, 151, 187);  // muted blue
static const COLORREF INLAY_PARAM_COLOR    = RGB(150, 150, 150);  // gray
static const COLORREF INLAY_BG            = RGB(40, 40, 40);      // subtle background
static const COLORREF INLAY_BORDER        = RGB(60, 60, 60);

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initInlayHints() {
    m_inlayHintEntries.clear();
    m_inlayHintsEnabled = true;
    m_inlayHintFont = CreateFontA(-dpiScale(11), 0, 0, 0, FW_NORMAL,
        TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    LOG_INFO("[InlayHints] Initialized");
}

void Win32IDE::shutdownInlayHints() {
    m_inlayHintEntries.clear();
    if (m_inlayHintFont) { DeleteObject(m_inlayHintFont); m_inlayHintFont = nullptr; }
    LOG_INFO("[InlayHints] Shutdown");
}

// ── Refresh from editor content ────────────────────────────────────────────
void Win32IDE::refreshInlayHints() {
    m_inlayHintEntries.clear();
    if (!m_inlayHintsEnabled || !m_hwndEditor) return;

    int totalLen = GetWindowTextLengthA(m_hwndEditor);
    if (totalLen <= 0 || totalLen > 300000) return;

    std::string content(totalLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], totalLen + 1);
    content.resize(totalLen);

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    // Track known type contexts for inference
    struct VarDecl {
        std::string name;
        std::string type;
        int line;
    };
    std::vector<VarDecl> knownVars;

    while (std::getline(stream, line)) {
        lineNum++;
        if (line.empty()) continue;

        size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string::npos) continue;
        std::string trimmed = line.substr(indent);

        // ── Type hints for `auto` variables ──
        // Pattern: auto varName = expression;
        size_t autoPos = trimmed.find("auto ");
        if (autoPos != std::string::npos && autoPos < 5) {
            size_t nameStart = autoPos + 5;
            while (nameStart < trimmed.size() && trimmed[nameStart] == ' ') nameStart++;

            // Handle auto& and auto*
            if (nameStart < trimmed.size() && (trimmed[nameStart] == '&' || trimmed[nameStart] == '*'))
                nameStart++;
            while (nameStart < trimmed.size() && trimmed[nameStart] == ' ') nameStart++;

            size_t nameEnd = nameStart;
            while (nameEnd < trimmed.size() && (isalnum(trimmed[nameEnd]) || trimmed[nameEnd] == '_'))
                nameEnd++;

            if (nameEnd > nameStart) {
                std::string varName = trimmed.substr(nameStart, nameEnd - nameStart);

                // Try to infer type from initializer
                size_t eqPos = trimmed.find('=', nameEnd);
                std::string inferredType;

                if (eqPos != std::string::npos) {
                    std::string init = trimmed.substr(eqPos + 1);
                    // Trim whitespace
                    size_t valStart = init.find_first_not_of(" \t");
                    if (valStart != std::string::npos) {
                        init = init.substr(valStart);

                        // Infer from common patterns
                        if (init.find("std::string") == 0 || init.find("\"") == 0)
                            inferredType = "string";
                        else if (init.find("std::vector") == 0)
                            inferredType = "vector<...>";
                        else if (init.find("std::map") == 0 || init.find("std::unordered_map") == 0)
                            inferredType = "map<...>";
                        else if (init.find("std::make_unique") == 0)
                            inferredType = "unique_ptr<...>";
                        else if (init.find("std::make_shared") == 0)
                            inferredType = "shared_ptr<...>";
                        else if (init.find("true") == 0 || init.find("false") == 0)
                            inferredType = "bool";
                        else if (init.find("nullptr") == 0)
                            inferredType = "nullptr_t";
                        else if (init.find("0x") == 0 || init.find("0X") == 0)
                            inferredType = "int";
                        else if (init.find('.') != std::string::npos &&
                                 init.find_first_of("0123456789") == 0) {
                            if (init.back() == 'f' || init.find("f;") != std::string::npos)
                                inferredType = "float";
                            else
                                inferredType = "double";
                        }
                        else if (init.find_first_of("0123456789") == 0) {
                            if (init.find("LL") != std::string::npos || init.find("ll") != std::string::npos)
                                inferredType = "long long";
                            else if (init.find("UL") != std::string::npos)
                                inferredType = "unsigned long";
                            else
                                inferredType = "int";
                        }
                        else if (init.find("new ") == 0) {
                            size_t typeEnd = init.find_first_of("({[; ", 4);
                            if (typeEnd != std::string::npos)
                                inferredType = init.substr(4, typeEnd - 4) + "*";
                        }
                        else if (init.find("static_cast<") == 0 || init.find("dynamic_cast<") == 0 ||
                                 init.find("reinterpret_cast<") == 0) {
                            size_t start = init.find('<') + 1;
                            size_t end = init.find('>');
                            if (end != std::string::npos)
                                inferredType = init.substr(start, end - start);
                        }
                        else {
                            // Try to match function call pattern → inferred from function name
                            size_t callParen = init.find('(');
                            if (callParen != std::string::npos && callParen > 0) {
                                std::string funcName = init.substr(0, callParen);
                                // Common return type patterns
                                if (funcName.find("size") != std::string::npos ||
                                    funcName.find("count") != std::string::npos ||
                                    funcName.find("length") != std::string::npos)
                                    inferredType = "size_t";
                                else if (funcName.find("find") != std::string::npos)
                                    inferredType = "iterator";
                                else if (funcName.find("begin") != std::string::npos ||
                                         funcName.find("end") != std::string::npos)
                                    inferredType = "iterator";
                                else if (funcName.find("get") != std::string::npos ||
                                         funcName == "front" || funcName == "back")
                                    inferredType = "auto&";
                            }
                        }
                    }
                }

                if (!inferredType.empty()) {
                    InlayHintEntry hint;
                    hint.line = lineNum;
                    hint.column = (int)(indent + nameEnd);
                    hint.text = ": " + inferredType;
                    hint.kind = InlayHintKind::Type;
                    hint.color = INLAY_TYPE_COLOR;
                    m_inlayHintEntries.push_back(hint);

                    knownVars.push_back({varName, inferredType, lineNum});
                }
            }
        }

        // ── Parameter name hints for function calls ──
        // Pattern: someFunction(expr1, expr2) → show param names
        // Only for known functions or when we can infer from context
        size_t parenPos = trimmed.find('(');
        if (parenPos != std::string::npos && parenPos > 0 && autoPos == std::string::npos) {
            // Skip control flow
            if (trimmed.find("if") == 0 || trimmed.find("for") == 0 ||
                trimmed.find("while") == 0 || trimmed.find("switch") == 0 ||
                trimmed.find("return") == 0 || trimmed.find("class") == 0 ||
                trimmed.find("struct") == 0 || trimmed.find("//") == 0) continue;

            // Extract function name
            size_t nameEnd = parenPos;
            while (nameEnd > 0 && trimmed[nameEnd-1] == ' ') nameEnd--;
            size_t nameStart = nameEnd;
            while (nameStart > 0 && (isalnum(trimmed[nameStart-1]) || trimmed[nameStart-1] == '_'
                   || trimmed[nameStart-1] == ':'))
                nameStart--;

            // Only show param hints for calls, not definitions
            // Heuristic: if there's a type before the function name, it's likely a definition
            if (nameStart > 0) {
                std::string prefix = trimmed.substr(0, nameStart);
                size_t prefixEnd = prefix.find_last_not_of(" \t");
                if (prefixEnd != std::string::npos) {
                    std::string lastWord = "";
                    size_t wordStart = prefixEnd;
                    while (wordStart > 0 && (isalnum(prefix[wordStart-1]) || prefix[wordStart-1] == '_'))
                        wordStart--;
                    lastWord = prefix.substr(wordStart, prefixEnd - wordStart + 1);
                    // If prefix looks like a type, skip (it's a definition)
                    if (lastWord == "void" || lastWord == "int" || lastWord == "bool" ||
                        lastWord == "float" || lastWord == "double" || lastWord == "char" ||
                        lastWord == "auto" || lastWord == "const" || lastWord == "static" ||
                        lastWord == "virtual" || lastWord == "override")
                        continue;
                }
            }

            // For known Win32/common functions, provide parameter hints
            std::string funcName = trimmed.substr(nameStart, nameEnd - nameStart);
            // Strip class/namespace prefix
            size_t colonPos = funcName.rfind("::");
            std::string shortName = (colonPos != std::string::npos) ?
                funcName.substr(colonPos + 2) : funcName;

            // Known function parameter names
            struct KnownFunc {
                const char* name;
                std::vector<const char*> params;
            };
            static const KnownFunc knownFuncs[] = {
                {"CreateWindowExA", {"dwExStyle", "lpClassName", "lpWindowName", "dwStyle",
                                     "x", "y", "nWidth", "nHeight", "hWndParent", "hMenu",
                                     "hInstance", "lpParam"}},
                {"CreateFontA", {"cHeight", "cWidth", "cEscapement", "cOrientation", "cWeight",
                                 "bItalic", "bUnderline", "bStrikeOut", "iCharSet", "iOutPrecision",
                                 "iClipPrecision", "iQuality", "iPitchAndFamily", "pszFaceName"}},
                {"SendMessageA", {"hWnd", "msg", "wParam", "lParam"}},
                {"SetTimer", {"hWnd", "nIDEvent", "uElapse", "lpTimerFunc"}},
                {"MoveWindow", {"hWnd", "x", "y", "nWidth", "nHeight", "bRepaint"}},
                {"TextOutA", {"hdc", "x", "y", "lpString", "c"}},
                {"FillRect", {"hDC", "lprc", "hbr"}},
                {"CreateSolidBrush", {"color"}},
                {"RGB", {"r", "g", "b"}},
            };

            for (auto& kf : knownFuncs) {
                if (shortName == kf.name) {
                    // Parse arguments to count them
                    size_t argStart = parenPos + 1;
                    int argIndex = 0;
                    int depth = 0;
                    for (size_t i = argStart; i < trimmed.size() && argIndex < (int)kf.params.size(); i++) {
                        if (trimmed[i] == '(' || trimmed[i] == '[' || trimmed[i] == '{') depth++;
                        else if (trimmed[i] == ')' || trimmed[i] == ']' || trimmed[i] == '}') {
                            if (depth == 0) break;
                            depth--;
                        }
                        else if (trimmed[i] == ',' && depth == 0) {
                            argIndex++;
                        }
                    }

                    // Add parameter hint for first arg (at call site)
                    if (kf.params.size() > 0) {
                        InlayHintEntry hint;
                        hint.line = lineNum;
                        hint.column = (int)(indent + argStart);
                        hint.text = std::string(kf.params[0]) + ":";
                        hint.kind = InlayHintKind::Parameter;
                        hint.color = INLAY_PARAM_COLOR;
                        m_inlayHintEntries.push_back(hint);
                    }
                    break;
                }
            }
        }

        // ── Range-based for auto inference ──
        // Pattern: for (auto& x : container) → infer x type
        if (trimmed.find("for") == 0 && trimmed.find("auto") != std::string::npos) {
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                // Find container name after ':'
                std::string containerStr = trimmed.substr(colonPos + 1);
                size_t cStart = containerStr.find_first_not_of(" \t");
                if (cStart != std::string::npos) {
                    size_t cEnd = containerStr.find_first_of(" \t)", cStart);
                    if (cEnd != std::string::npos) {
                        std::string container = containerStr.substr(cStart, cEnd - cStart);

                        // Look up container type from known vars
                        for (auto& var : knownVars) {
                            if (var.name == container) {
                                std::string elemType = "auto";
                                if (var.type.find("vector") != std::string::npos)
                                    elemType = "element";
                                else if (var.type.find("map") != std::string::npos)
                                    elemType = "pair";
                                else if (var.type == "string")
                                    elemType = "char";

                                // Find the loop variable
                                size_t varPos = trimmed.find("auto");
                                size_t vStart = varPos + 4;
                                while (vStart < colonPos && (trimmed[vStart] == ' ' ||
                                       trimmed[vStart] == '&' || trimmed[vStart] == '*'))
                                    vStart++;
                                size_t vEnd = vStart;
                                while (vEnd < colonPos && (isalnum(trimmed[vEnd]) || trimmed[vEnd] == '_'))
                                    vEnd++;

                                if (vEnd > vStart) {
                                    InlayHintEntry hint;
                                    hint.line = lineNum;
                                    hint.column = (int)(indent + vEnd);
                                    hint.text = ": " + elemType;
                                    hint.kind = InlayHintKind::Type;
                                    hint.color = INLAY_TYPE_COLOR;
                                    m_inlayHintEntries.push_back(hint);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (m_hwndEditor) InvalidateRect(m_hwndEditor, NULL, FALSE);
    LOG_INFO("[InlayHints] Computed " + std::to_string(m_inlayHintEntries.size()) + " hints");
}

// ── Render (called from editor paint) ──────────────────────────────────────
void Win32IDE::renderInlayHints(HDC hdc, int lineY, int lineNumber) {
    if (!m_inlayHintsEnabled || m_inlayHintEntries.empty()) return;

    for (auto& hint : m_inlayHintEntries) {
        if (hint.line != lineNumber) continue;

        HFONT oldFont = (HFONT)SelectObject(hdc, m_inlayHintFont);
        SetBkMode(hdc, TRANSPARENT);

        TEXTMETRICA tm;
        GetTextMetricsA(hdc, &tm);

        // Calculate x position based on column
        // Approximate: use average char width * column
        int editorCharW = tm.tmAveCharWidth;
        int gutterW = dpiScale(50);
        int hintX = gutterW + hint.column * editorCharW;

        // Measure hint text
        SIZE hintSize;
        GetTextExtentPoint32A(hdc, hint.text.c_str(), (int)hint.text.size(), &hintSize);

        // Draw background pill
        RECT pillRect = {
            hintX - 2, lineY + 1,
            hintX + hintSize.cx + 4, lineY + hintSize.cy + 1
        };
        HBRUSH pillBrush = CreateSolidBrush(INLAY_BG);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, pillBrush);
        HPEN borderPen = CreatePen(PS_SOLID, 1, INLAY_BORDER);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        RoundRect(hdc, pillRect.left, pillRect.top, pillRect.right, pillRect.bottom, 4, 4);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(borderPen);
        DeleteObject(pillBrush);

        // Draw text
        SetTextColor(hdc, hint.color);
        TextOutA(hdc, hintX + 1, lineY + 1, hint.text.c_str(), (int)hint.text.size());

        SelectObject(hdc, oldFont);
    }
}

// ── Parse from LSP response ───────────────────────────────────────────────
void Win32IDE::parseInlayHintsFromLSP(const std::string& jsonResponse) {
    // Parse LSP textDocument/InlayHintEntry response
    // Expected format: [{position:{line,character}, label:string, kind:1|2}]
    // Kind 1 = Type, Kind 2 = Parameter

    // Simple JSON parser for inlay hints
    m_inlayHintEntries.clear();

    // For now, delegate to refreshInlayHints() which does local inference
    // When LSP provides hints, those will override local ones
    if (jsonResponse.empty()) {
        refreshInlayHints();
        return;
    }

    // Basic parsing (production would use nlohmann/json)
    size_t pos = 0;
    while ((pos = jsonResponse.find("\"line\":", pos)) != std::string::npos) {
        pos += 7;
        int line = atoi(jsonResponse.c_str() + pos);

        size_t charPos = jsonResponse.find("\"character\":", pos);
        if (charPos == std::string::npos) break;
        int character = atoi(jsonResponse.c_str() + charPos + 12);

        size_t labelPos = jsonResponse.find("\"label\":\"", pos);
        if (labelPos == std::string::npos) break;
        labelPos += 9;
        size_t labelEnd = jsonResponse.find("\"", labelPos);
        std::string label = jsonResponse.substr(labelPos, labelEnd - labelPos);

        size_t kindPos = jsonResponse.find("\"kind\":", pos);
        int kind = 1; // default: Type
        if (kindPos != std::string::npos) kind = atoi(jsonResponse.c_str() + kindPos + 7);

        InlayHintEntry hint;
        hint.line = line + 1; // LSP is 0-based
        hint.column = character;
        hint.text = label;
        hint.kind = (kind == 2) ? InlayHintKind::Parameter : InlayHintKind::Type;
        hint.color = (hint.kind == InlayHintKind::Parameter) ? INLAY_PARAM_COLOR : INLAY_TYPE_COLOR;
        m_inlayHintEntries.push_back(hint);
    }

    if (m_hwndEditor) InvalidateRect(m_hwndEditor, NULL, FALSE);
    LOG_INFO("[InlayHints] Parsed " + std::to_string(m_inlayHintEntries.size()) + " LSP hints");
}

// ── Color helper (inline) ──────────────────────────────────────────────────────────────────────
// inlayHintColor integrated inline at call sites
