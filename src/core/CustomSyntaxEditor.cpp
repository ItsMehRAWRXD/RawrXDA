// ============================================================================
// CustomSyntaxEditor.cpp — Pure Win32 Custom Syntax Highlighting Editor
// ============================================================================
#include "CustomSyntaxEditor.h"
#include <windowsx.h>
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <sstream>
#include <cctype>

#pragma comment(lib, "comctl32.lib")

// Language definitions
namespace {
    LanguageDefinition CPP_LANGUAGE = {
        "C++",
        {".cpp", ".hpp", ".cc", ".h", ".cxx", ".hxx"},
        {"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", 
         "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t", 
         "class", "compl", "concept", "const", "consteval", "constexpr", "const_cast", 
         "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete", 
         "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", 
         "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", 
         "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", 
         "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast", 
         "requires", "return", "short", "signed", "sizeof", "static", "static_assert", 
         "static_cast", "struct", "switch", "template", "this", "thread_local", "throw", 
         "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", 
         "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"},
        {"int", "float", "double", "char", "bool", "void", "short", "long", 
         "signed", "unsigned", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
         "uint8_t", "uint16_t", "uint32_t", "uint64_t", "float", "double", "long double"},
        {"main", "printf", "scanf", "malloc", "free", "new", "delete", "sizeof"},
        {"NULL", "true", "false", "NULLPTR"},
        "//",
        "/*",
        "*/",
        '"',
        '\''
    };

    LanguageDefinition PYTHON_LANGUAGE = {
        "Python",
        {".py"},
        {"and", "as", "assert", "async", "await", "break", "class", "continue", 
         "def", "del", "elif", "else", "except", "False", "finally", "for", 
         "from", "global", "if", "import", "in", "is", "lambda", "None", 
         "nonlocal", "not", "or", "pass", "raise", "return", "True", "try", 
         "while", "with", "yield"},
        {"int", "float", "str", "bool", "list", "dict", "tuple", "set", 
         "complex", "bytes", "bytearray", "memoryview", "range", "frozenset"},
        {"print", "len", "range", "input", "type", "isinstance", "super", 
         "enumerate", "zip", "map", "filter", "reduce", "abs", "all", "any"},
        {"None", "True", "False", "NotImplemented", "Ellipsis"},
        "#",
        "\"\"\"",
        "\"\"\"",
        '"',
        '\''
    };

    LanguageDefinition JAVASCRIPT_LANGUAGE = {
        "JavaScript",
        {".js", ".jsx", ".ts", ".tsx"},
        {"abstract", "arguments", "await", "boolean", "break", "byte", "case", 
         "catch", "char", "class", "const", "continue", "debugger", "default", 
         "delete", "do", "double", "else", "enum", "eval", "export", "extends", 
         "false", "final", "finally", "float", "for", "function", "goto", "if", 
         "implements", "import", "in", "instanceof", "int", "interface", "let", 
         "long", "native", "new", "null", "package", "private", "protected", 
         "public", "return", "short", "static", "super", "switch", "synchronized", 
         "this", "throw", "throws", "transient", "true", "try", "typeof", "var", 
         "void", "volatile", "while", "with", "yield"},
        {"Number", "String", "Boolean", "Object", "Function", "Array", "Date", 
         "RegExp", "Error", "Map", "Set", "WeakMap", "WeakSet", "Promise", 
         "Symbol", "Proxy", "Reflect", "Intl", "ArrayBuffer", "DataView", 
         "Float32Array", "Float64Array", "Int8Array", "Int16Array", "Int32Array", 
         "Uint8Array", "Uint16Array", "Uint32Array", "Uint8ClampedArray"},
        {"console", "alert", "document", "window", "setTimeout", "setInterval", 
         "fetch", "Promise", "JSON", "Math", "Object", "Array", "String", "Number"},
        {"null", "undefined", "true", "false", "NaN", "Infinity"},
        "//",
        "/*",
        "*/",
        '"',
        '\''
    };
}

CustomSyntaxEditor::CustomSyntaxEditor()
    : m_hWnd(nullptr)
    , m_parent(nullptr)
    , m_visible(false)
    , m_textColor(AdobeRGBa(0.86f, 0.86f, 0.86f, 1.0f))
    , m_backgroundColor(AdobeRGBa(0.12f, 0.12f, 0.12f, 1.0f))
    , m_selectionColor(AdobeRGBa(0.17f, 0.38f, 0.58f, 1.0f))
    , m_lineNumberColor(AdobeRGBa(0.50f, 0.50f, 0.50f, 1.0f))
    , m_font(nullptr)
    , m_fontName(L"Consolas")
    , m_fontSize(14)
    , m_charWidth(8)
    , m_charHeight(16)
    , m_lineHeight(20)
    , m_cursorLine(0)
    , m_cursorCol(0)
    , m_selectionStartLine(0)
    , m_selectionStartCol(0)
    , m_selectionEndLine(0)
    , m_selectionEndCol(0)
    , m_hasSelection(false)
    , m_scrollX(0)
    , m_scrollY(0)
    , m_maxScrollX(0)
    , m_maxScrollY(0)
    , m_showLineNumbers(true)
    , m_lineNumberWidth(60)
    , m_memDC(nullptr)
    , m_memBitmap(nullptr)
    , m_bufferWidth(0)
    , m_bufferHeight(0) {
    
    // Convert AdobeRGBa to COLORREF
    m_textColorRef = AdobeRGBaToCOLORREF(m_textColor);
    m_backgroundColorRef = AdobeRGBaToCOLORREF(m_backgroundColor);
    m_selectionColorRef = AdobeRGBaToCOLORREF(m_selectionColor);
    m_lineNumberColorRef = AdobeRGBaToCOLORREF(m_lineNumberColor);
    
    // Initialize language definitions
    m_languages["cpp"] = CPP_LANGUAGE;
    m_languages["python"] = PYTHON_LANGUAGE;
    m_languages["javascript"] = JAVASCRIPT_LANGUAGE;
    
    // Set default language
    m_language = CPP_LANGUAGE;
    
    // Initialize with empty text
    m_lines.push_back({ "", {}, 0, m_lineHeight });
}

CustomSyntaxEditor::~CustomSyntaxEditor() {
    Destroy();
}

bool CustomSyntaxEditor::Create(HWND parent, int x, int y, int width, int height) {
    m_parent = parent;
    
    // Create font
    HDC hdc = GetDC(parent);
    int fontSize = -MulDiv(m_fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(parent, hdc);
    
    m_font = CreateFontW(
        fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        m_fontName.c_str()
    );
    
    // Create offscreen buffer
    // ResizeBuffer(width, height); // Will be implemented when needed
    
    return true;
}

void CustomSyntaxEditor::Destroy() {
    if (m_memBitmap) {
        DeleteObject(m_memBitmap);
        m_memBitmap = nullptr;
    }
    
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
}

void CustomSyntaxEditor::SetText(const std::string& text) {
    m_text = text;
    
    // Split text into lines
    m_lines.clear();
    std::istringstream stream(text);
    std::string line;
    int yPos = 0;
    
    while (std::getline(stream, line)) {
        EditorLine editorLine;
        editorLine.text = line;
        editorLine.yPos = yPos;
        editorLine.height = m_lineHeight;
        m_lines.push_back(editorLine);
        yPos += m_lineHeight;
    }
    
    // Add empty line if text ends with newline
    if (!text.empty() && text.back() == '\n') {
        m_lines.push_back({ "", {}, yPos, m_lineHeight });
    }
    
    // Parse syntax
    ParseSyntax();
    
    // Update scroll bars
    UpdateScrollBars();
    
    // Notify text change
    if (m_onTextChanged) {
        m_onTextChanged();
    }
}

std::string CustomSyntaxEditor::GetText() const {
    std::string result;
    for (size_t i = 0; i < m_lines.size(); i++) {
        result += m_lines[i].text;
        if (i < m_lines.size() - 1) {
            result += "\n";
        }
    }
    return result;
}

void CustomSyntaxEditor::SetLanguage(const std::string& language) {
    auto it = m_languages.find(language);
    if (it != m_languages.end()) {
        m_language = it->second;
        ParseSyntax();
    }
}

void CustomSyntaxEditor::Rehighlight() {
    ParseSyntax();
}

void CustomSyntaxEditor::SetTheme(const AdobeRGBa& textColor, const AdobeRGBa& backgroundColor,
                                 const AdobeRGBa& selectionColor, const AdobeRGBa& lineNumberColor) {
    m_textColor = textColor;
    m_backgroundColor = backgroundColor;
    m_selectionColor = selectionColor;
    m_lineNumberColor = lineNumberColor;
    
    m_textColorRef = AdobeRGBaToCOLORREF(textColor);
    m_backgroundColorRef = AdobeRGBaToCOLORREF(backgroundColor);
    m_selectionColorRef = AdobeRGBaToCOLORREF(selectionColor);
    m_lineNumberColorRef = AdobeRGBaToCOLORREF(lineNumberColor);
}

void CustomSyntaxEditor::Render(HDC hdc, const RECT& clientRect) {
    // Create compatible DC if needed
    if (!m_memDC || m_bufferWidth != clientRect.right || m_bufferHeight != clientRect.bottom) {
        // ResizeBuffer(clientRect.right, clientRect.bottom); // Will be implemented when needed
    }
    
    // Clear background
    HBRUSH bgBrush = CreateSolidBrush(m_backgroundColorRef);
    FillRect(m_memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);
    
    // Draw line numbers
    if (m_showLineNumbers) {
        DrawLineNumbers(m_memDC, clientRect);
    }
    
    // Draw text content
    DrawTextContent(m_memDC, clientRect);
    
    // Draw selection
    if (m_hasSelection) {
        DrawSelection(m_memDC, clientRect);
    }
    
    // Draw cursor
    DrawCursor(m_memDC, clientRect);
    
    // Blit to screen
    BitBlt(hdc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom,
           m_memDC, 0, 0, SRCCOPY);
}

void CustomSyntaxEditor::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_parent, &ps);
    Render(hdc, ps.rcPaint);
    EndPaint(m_parent, &ps);
}

void CustomSyntaxEditor::ParseSyntax() {
    for (auto& line : m_lines) {
        ParseLineSyntax(line);
    }
}

void CustomSyntaxEditor::ParseLineSyntax(EditorLine& line) {
    line.tokens.clear();
    
    if (line.text.empty()) {
        return;
    }
    
    size_t pos = 0;
    size_t length = line.text.length();
    
    while (pos < length) {
        // Skip whitespace
        if (std::isspace(line.text[pos])) {
            size_t start = pos;
            while (pos < length && std::isspace(line.text[pos])) {
                pos++;
            }
            line.tokens.push_back(SyntaxToken(SyntaxTokenType::Whitespace, start, pos - start, m_textColorRef));
            continue;
        }
        
        // Check for comments
        if (m_language.singleLineComment.empty() == false &&
            line.text.compare(pos, m_language.singleLineComment.length(), m_language.singleLineComment) == 0) {
            // Single line comment
            line.tokens.push_back(SyntaxToken(SyntaxTokenType::Comment, pos, length - pos, RGB(87, 166, 74)));
            break;
        }
        
        // Check for strings
        if (line.text[pos] == m_language.stringDelimiter) {
            size_t start = pos;
            pos++;
            bool escaped = false;
            
            while (pos < length) {
                if (escaped) {
                    escaped = false;
                    pos++;
                } else if (line.text[pos] == '\\') {
                    escaped = true;
                    pos++;
                } else if (line.text[pos] == m_language.stringDelimiter) {
                    pos++;
                    break;
                } else {
                    pos++;
                }
            }
            
            line.tokens.push_back(SyntaxToken(SyntaxTokenType::String, start, pos - start, RGB(206, 145, 120)));
            continue;
        }
        
        // Check for numbers
        if (std::isdigit(line.text[pos]) || (line.text[pos] == '.' && pos + 1 < length && std::isdigit(line.text[pos + 1]))) {
            size_t start = pos;
            while (pos < length && (std::isdigit(line.text[pos]) || line.text[pos] == '.' || line.text[pos] == 'e' || line.text[pos] == 'E' || line.text[pos] == '+' || line.text[pos] == '-')) {
                pos++;
            }
            line.tokens.push_back(SyntaxToken(SyntaxTokenType::Number, start, pos - start, RGB(181, 206, 168)));
            continue;
        }
        
        // Check for identifiers/keywords
        if (std::isalpha(line.text[pos]) || line.text[pos] == '_') {
            size_t start = pos;
            while (pos < length && (std::isalnum(line.text[pos]) || line.text[pos] == '_')) {
                pos++;
            }
            
            std::string word = line.text.substr(start, pos - start);
            SyntaxTokenType type = SyntaxTokenType::Identifier;
            COLORREF color = m_textColorRef;
            
            if (IsKeyword(word)) {
                type = SyntaxTokenType::Keyword;
                color = RGB(86, 156, 214);
            } else if (IsType(word)) {
                type = SyntaxTokenType::Type;
                color = RGB(78, 201, 176);
            } else if (IsFunction(word)) {
                type = SyntaxTokenType::Function;
                color = RGB(220, 220, 170);
            } else if (IsConstant(word)) {
                type = SyntaxTokenType::Constant;
                color = RGB(181, 206, 168);
            }
            
            line.tokens.push_back(SyntaxToken(type, start, pos - start, color));
            continue;
        }
        
        // Check for operators
        if (std::ispunct(line.text[pos])) {
            size_t start = pos;
            while (pos < length && std::ispunct(line.text[pos])) {
                pos++;
            }
            line.tokens.push_back(SyntaxToken(SyntaxTokenType::Operator, start, pos - start, m_textColorRef));
            continue;
        }
        
        // Unknown character
        pos++;
    }
}

bool CustomSyntaxEditor::IsKeyword(const std::string& word) const {
    for (const auto& kw : m_language.keywords) {
        if (kw == word) return true;
    }
    return false;
}

bool CustomSyntaxEditor::IsType(const std::string& word) const {
    for (const auto& type : m_language.types) {
        if (type == word) return true;
    }
    return false;
}

bool CustomSyntaxEditor::IsFunction(const std::string& word) const {
    for (const auto& func : m_language.functions) {
        if (func == word) return true;
    }
    return false;
}

bool CustomSyntaxEditor::IsConstant(const std::string& word) const {
    for (const auto& constant : m_language.constants) {
        if (constant == word) return true;
    }
    return false;
}

void CustomSyntaxEditor::DrawLineNumbers(HDC hdc, const RECT& clientRect) {
    // Set up colors
    SetTextColor(hdc, m_lineNumberColorRef);
    SetBkColor(hdc, m_backgroundColorRef);
    SetBkMode(hdc, OPAQUE);
    
    // Draw line numbers
    int firstVisibleLine = m_scrollY / m_lineHeight;
    int visibleLines = clientRect.bottom / m_lineHeight + 2;
    
    for (int i = 0; i < visibleLines; i++) {
        int lineNum = firstVisibleLine + i + 1;
        if (lineNum > (int)m_lines.size()) break;
        
        std::wstring numText = std::to_wstring(lineNum);
        RECT numRect = {
            2,
            i * m_lineHeight - (m_scrollY % m_lineHeight),
            m_lineNumberWidth - 2,
            (i + 1) * m_lineHeight - (m_scrollY % m_lineHeight)
        };
        
        DrawTextW(hdc, numText.c_str(), -1, &numRect,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void CustomSyntaxEditor::DrawTextContent(HDC hdc, const RECT& clientRect) {
    // Set text color
    SetTextColor(hdc, m_textColorRef);
    SetBkMode(hdc, TRANSPARENT);
    
    // Calculate visible range
    int firstVisibleLine = m_scrollY / m_lineHeight;
    int visibleLines = clientRect.bottom / m_lineHeight + 2;
    int xOffset = m_showLineNumbers ? m_lineNumberWidth : 0;
    
    // Draw visible lines
    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = firstVisibleLine + i;
        if (lineIdx >= (int)m_lines.size()) break;
        
        const EditorLine& line = m_lines[lineIdx];
        int yPos = i * m_lineHeight - (m_scrollY % m_lineHeight);
        
        // Draw tokens
        int xPos = xOffset - m_scrollX;
        for (const auto& token : line.tokens) {
            std::string tokenText = line.text.substr(token.start, token.length);
            
            // Convert to wide string
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, tokenText.c_str(), (int)tokenText.length(), nullptr, 0);
            std::vector<wchar_t> wideText(wideLen + 1);
            MultiByteToWideChar(CP_UTF8, 0, tokenText.c_str(), (int)tokenText.length(), wideText.data(), wideLen);
            wideText[wideLen] = L'\0';
            
            // Set token color
            SetTextColor(hdc, token.color);
            
            // Draw token
            TextOutW(hdc, xPos, yPos, wideText.data(), wideLen);
            
            // Advance position
            SIZE size;
            GetTextExtentPoint32W(hdc, wideText.data(), wideLen, &size);
            xPos += size.cx;
        }
    }
}

void CustomSyntaxEditor::DrawSelection(HDC hdc, const RECT& clientRect) {
    // Calculate selection rectangle
    int startLine = std::min(m_selectionStartLine, m_selectionEndLine);
    int endLine = std::max(m_selectionStartLine, m_selectionEndLine);
    
    int firstVisibleLine = m_scrollY / m_lineHeight;
    int visibleLines = clientRect.bottom / m_lineHeight + 2;
    int xOffset = m_showLineNumbers ? m_lineNumberWidth : 0;
    
    // Draw selection highlight
    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = firstVisibleLine + i;
        if (lineIdx < startLine || lineIdx > endLine) continue;
        if (lineIdx >= (int)m_lines.size()) break;
        
        int yPos = i * m_lineHeight - (m_scrollY % m_lineHeight);
        
        // Calculate selection bounds for this line
        int selStartCol = 0;
        int selEndCol = (int)m_lines[lineIdx].text.length();
        
        if (lineIdx == startLine) {
            selStartCol = (startLine == m_selectionStartLine) ? m_selectionStartCol : m_selectionEndCol;
        }
        if (lineIdx == endLine) {
            selEndCol = (endLine == m_selectionEndLine) ? m_selectionEndCol : m_selectionStartCol;
        }
        
        // Calculate pixel positions
        int xStart = xOffset - m_scrollX;
        for (int j = 0; j < selStartCol && j < (int)m_lines[lineIdx].text.length(); j++) {
            SIZE size;
            std::string ch = m_lines[lineIdx].text.substr(j, 1);
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, nullptr, 0);
            std::vector<wchar_t> wideCh(wideLen + 1);
            MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, wideCh.data(), wideLen);
            GetTextExtentPoint32W(hdc, wideCh.data(), wideLen, &size);
            xStart += size.cx;
        }
        
        int xEnd = xStart;
        for (int j = selStartCol; j < selEndCol && j < (int)m_lines[lineIdx].text.length(); j++) {
            SIZE size;
            std::string ch = m_lines[lineIdx].text.substr(j, 1);
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, nullptr, 0);
            std::vector<wchar_t> wideCh(wideLen + 1);
            MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, wideCh.data(), wideLen);
            GetTextExtentPoint32W(hdc, wideCh.data(), wideLen, &size);
            xEnd += size.cx;
        }
        
        // Draw selection rectangle
        RECT selRect = { xStart, yPos, xEnd, yPos + m_lineHeight };
        HBRUSH selBrush = CreateSolidBrush(m_selectionColorRef);
        FillRect(hdc, &selRect, selBrush);
        DeleteObject(selBrush);
    }
}

void CustomSyntaxEditor::DrawCursor(HDC hdc, const RECT& clientRect) {
    // Calculate cursor position
    int firstVisibleLine = m_scrollY / m_lineHeight;
    int visibleLineIdx = m_cursorLine - firstVisibleLine;
    
    if (visibleLineIdx < 0 || visibleLineIdx >= clientRect.bottom / m_lineHeight + 2) {
        return;
    }
    
    int xOffset = m_showLineNumbers ? m_lineNumberWidth : 0;
    int yPos = visibleLineIdx * m_lineHeight - (m_scrollY % m_lineHeight);
    
    // Calculate x position
    int xPos = xOffset - m_scrollX;
    if (m_cursorLine < (int)m_lines.size()) {
        for (int j = 0; j < m_cursorCol && j < (int)m_lines[m_cursorLine].text.length(); j++) {
            SIZE size;
            std::string ch = m_lines[m_cursorLine].text.substr(j, 1);
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, nullptr, 0);
            std::vector<wchar_t> wideCh(wideLen + 1);
            MultiByteToWideChar(CP_UTF8, 0, ch.c_str(), 1, wideCh.data(), wideLen);
            GetTextExtentPoint32W(hdc, wideCh.data(), wideLen, &size);
            xPos += size.cx;
        }
    }
    
    // Draw cursor
    RECT cursorRect = { xPos, yPos, xPos + 2, yPos + m_lineHeight };
    HBRUSH cursorBrush = CreateSolidBrush(m_textColorRef);
    FillRect(hdc, &cursorRect, cursorBrush);
    DeleteObject(cursorBrush);
}

void CustomSyntaxEditor::SetScrollPos(int x, int y) {
    m_scrollX = std::max(0, std::min(x, m_maxScrollX));
    m_scrollY = std::max(0, std::min(y, m_maxScrollY));
}

void CustomSyntaxEditor::GetScrollPos(int& x, int& y) const {
    x = m_scrollX;
    y = m_scrollY;
}

void CustomSyntaxEditor::UpdateScrollBars() {
    // Calculate max scroll
    m_maxScrollY = std::max(0, (int)(m_lines.size() * m_lineHeight));
    
    int maxLineLength = 0;
    for (const auto& line : m_lines) {
        maxLineLength = std::max(maxLineLength, (int)line.text.length());
    }
    m_maxScrollX = std::max(0, maxLineLength * m_charWidth);
}

void CustomSyntaxEditor::SetCursorPos(int line, int col) {
    m_cursorLine = std::max(0, std::min(line, (int)m_lines.size() - 1));
    m_cursorCol = std::max(0, std::min(col, (int)m_lines[m_cursorLine].text.length()));
    EnsureCursorVisible();
}

void CustomSyntaxEditor::GetCursorPos(int& line, int& col) const {
    line = m_cursorLine;
    col = m_cursorCol;
}

void CustomSyntaxEditor::SetSelection(int startLine, int startCol, int endLine, int endCol) {
    m_selectionStartLine = startLine;
    m_selectionStartCol = startCol;
    m_selectionEndLine = endLine;
    m_selectionEndCol = endCol;
    m_hasSelection = true;
}

void CustomSyntaxEditor::GetSelection(int& startLine, int& startCol, int& endLine, int& endCol) const {
    startLine = m_selectionStartLine;
    startCol = m_selectionStartCol;
    endLine = m_selectionEndLine;
    endCol = m_selectionEndCol;
}

void CustomSyntaxEditor::SetFont(const std::wstring& fontName, int fontSize) {
    m_fontName = fontName;
    m_fontSize = fontSize;
    
    // Recreate font
    if (m_font) {
        DeleteObject(m_font);
    }
    
    HDC hdc = GetDC(m_parent);
    int fontSizePixels = -MulDiv(m_fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_parent, hdc);
    
    m_font = CreateFontW(
        fontSizePixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        m_fontName.c_str()
    );
    
    // Update metrics
    HDC hdcMetrics = GetDC(m_parent);
    HFONT hOldFont = (HFONT)SelectObject(hdcMetrics, m_font);
    TEXTMETRIC tm;
    GetTextMetrics(hdcMetrics, &tm);
    m_charWidth = tm.tmAveCharWidth;
    m_charHeight = tm.tmHeight;
    m_lineHeight = m_charHeight + 4;
    SelectObject(hdcMetrics, hOldFont);
    ReleaseDC(m_parent, hdcMetrics);
    
    // Update line heights
    for (auto& line : m_lines) {
        line.height = m_lineHeight;
    }
    
    UpdateScrollBars();
}

void CustomSyntaxEditor::ShowLineNumbers(bool show) {
    m_showLineNumbers = show;
}

void CustomSyntaxEditor::Show() {
    m_visible = true;
}

void CustomSyntaxEditor::Hide() {
    m_visible = false;
}

bool CustomSyntaxEditor::IsVisible() const {
    return m_visible;
}

void CustomSyntaxEditor::SetFocus() {
    // Will be implemented when window handle is available
}

bool CustomSyntaxEditor::HasFocus() const {
    return false; // Will be implemented when window handle is available
}

void CustomSyntaxEditor::SetOnTextChangedCallback(std::function<void()> callback) {
    m_onTextChanged = callback;
}

void CustomSyntaxEditor::SetOnSelectionChangedCallback(std::function<void()> callback) {
    m_onSelectionChanged = callback;
}

int CustomSyntaxEditor::GetLineCount() const {
    return (int)m_lines.size();
}

int CustomSyntaxEditor::GetLineLength(int line) const {
    if (line >= 0 && line < (int)m_lines.size()) {
        return (int)m_lines[line].text.length();
    }
    return 0;
}

std::string CustomSyntaxEditor::GetLineText(int line) const {
    if (line >= 0 && line < (int)m_lines.size()) {
        return m_lines[line].text;
    }
    return "";
}

void CustomSyntaxEditor::EnsureCursorVisible() {
    // Ensure cursor is within visible area
    int visibleTop = m_scrollY;
    int visibleBottom = m_scrollY + m_bufferHeight;
    int cursorY = m_cursorLine * m_lineHeight;
    
    if (cursorY < visibleTop) {
        m_scrollY = cursorY;
    } else if (cursorY + m_lineHeight > visibleBottom) {
        m_scrollY = cursorY + m_lineHeight - m_bufferHeight;
    }
    
    m_scrollY = std::max(0, std::min(m_scrollY, m_maxScrollY));
}

void CustomSyntaxEditor::InsertText(int line, int col, const std::string& text) {
    if (line >= 0 && line < (int)m_lines.size()) {
        m_lines[line].text.insert(col, text);
        ParseLineSyntax(m_lines[line]);
        UpdateScrollBars();
        
        if (m_onTextChanged) {
            m_onTextChanged();
        }
    }
}

void CustomSyntaxEditor::DeleteRange(int startLine, int startCol, int endLine, int endCol) {
    if (startLine == endLine) {
        if (startLine >= 0 && startLine < (int)m_lines.size()) {
            m_lines[startLine].text.erase(startCol, endCol - startCol);
            ParseLineSyntax(m_lines[startLine]);
        }
    } else {
        // Multi-line delete - simplified implementation
        if (startLine >= 0 && startLine < (int)m_lines.size()) {
            m_lines[startLine].text.erase(startCol);
            
            // Remove intermediate lines
            for (int i = startLine + 1; i < endLine && i < (int)m_lines.size(); i++) {
                m_lines.erase(m_lines.begin() + i);
                i--;
                endLine--;
            }
            
            // Append remaining text from end line
            if (endLine >= 0 && endLine < (int)m_lines.size()) {
                m_lines[startLine].text += m_lines[endLine].text.substr(endCol);
                m_lines.erase(m_lines.begin() + endLine);
            }
            
            ParseLineSyntax(m_lines[startLine]);
        }
    }
    
    UpdateScrollBars();
    
    if (m_onTextChanged) {
        m_onTextChanged();
    }
}

size_t CustomSyntaxEditor::LineColToPos(int line, int col) const {
    size_t pos = 0;
    for (int i = 0; i < line && i < (int)m_lines.size(); i++) {
        pos += m_lines[i].text.length() + 1; // +1 for newline
    }
    pos += col;
    return pos;
}

void CustomSyntaxEditor::PosToLineCol(size_t pos, int& line, int& col) const {
    line = 0;
    col = 0;
    size_t currentPos = 0;
    
    for (int i = 0; i < (int)m_lines.size(); i++) {
        size_t lineEnd = currentPos + m_lines[i].text.length();
        if (pos <= lineEnd) {
            line = i;
            col = (int)(pos - currentPos);
            return;
        }
        currentPos = lineEnd + 1; // +1 for newline
    }
    
    line = (int)m_lines.size() - 1;
    col = (int)m_lines[line].text.length();
}

SyntaxTokenType CustomSyntaxEditor::IdentifyToken(const std::string& text, size_t pos) const {
    // This is a simplified implementation
    // In a full implementation, this would use a more sophisticated tokenizer
    
    if (std::isspace(text[pos])) {
        return SyntaxTokenType::Whitespace;
    }
    
    if (text[pos] == '"' || text[pos] == '\'') {
        return SyntaxTokenType::String;
    }
    
    if (std::isdigit(text[pos])) {
        return SyntaxTokenType::Number;
    }
    
    if (std::isalpha(text[pos]) || text[pos] == '_') {
        size_t start = pos;
        while (pos < text.length() && (std::isalnum(text[pos]) || text[pos] == '_')) {
            pos++;
        }
        std::string word = text.substr(start, pos - start);
        
        if (IsKeyword(word)) return SyntaxTokenType::Keyword;
        if (IsType(word)) return SyntaxTokenType::Type;
        if (IsFunction(word)) return SyntaxTokenType::Function;
        if (IsConstant(word)) return SyntaxTokenType::Constant;
        
        return SyntaxTokenType::Identifier;
    }
    
    if (std::ispunct(text[pos])) {
        return SyntaxTokenType::Operator;
    }
    
    return SyntaxTokenType::Unknown;
}

SIZE CustomSyntaxEditor::MeasureText(const std::string& text) const {
    SIZE size = { 0, 0 };
    if (!m_font || text.empty()) return size;
    
    HDC hdc = GetDC(m_parent);
    HFONT hOldFont = (HFONT)SelectObject(hdc, m_font);
    
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.length(), nullptr, 0);
    std::vector<wchar_t> wideText(wideLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.length(), wideText.data(), wideLen);
    wideText[wideLen] = L'\0';
    
    GetTextExtentPoint32W(hdc, wideText.data(), wideLen, &size);
    
    SelectObject(hdc, hOldFont);
    ReleaseDC(m_parent, hdc);
    
    return size;
}

void CustomSyntaxEditor::CalculateLineMetrics() {
    for (auto& line : m_lines) {
        MeasureLine(line);
    }
}

void CustomSyntaxEditor::MeasureLine(EditorLine& line) {
    // Calculate line width based on tokens
    int width = 0;
    for (const auto& token : line.tokens) {
        std::string tokenText = line.text.substr(token.start, token.length);
        SIZE size = MeasureText(tokenText);
        width += size.cx;
    }
    
    // Update line height
    line.height = m_lineHeight;
}
