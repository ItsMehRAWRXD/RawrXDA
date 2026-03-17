// ============================================================================
// Win32IDE_SyntaxHighlight.cpp — Incremental Syntax Coloring Engine
// ============================================================================
// Provides real-time syntax coloring for the RichEdit editor control.
// Uses debounced EN_CHANGE → tokenize → EM_SETCHARFORMAT pipeline.
//
// Supported languages: C/C++, Python, JavaScript, PowerShell, JSON, Assembly
// Design: Zero background threads — all coloring is UI-thread, debounced
// via WM_TIMER to avoid typing latency.
//
// Uses the existing header signatures from Win32IDE.h:
//   tokenizeLine(line, lang, outTokens, inBlockComment)
//   applySyntaxHighlighting() / applySyntaxHighlightingRange(lineStart, lineEnd)
//   colorForToken(type) / markSyntaxDirty(lineStart, lineEnd)
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

// ============================================================================
// KEYWORD TABLES — per language
// ============================================================================

static const std::unordered_set<std::string> g_cppKeywords = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "break", "case", "catch", "class", "compl", "concept", "const", "consteval",
    "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return",
    "co_yield", "decltype", "default", "delete", "do", "dynamic_cast", "else",
    "enum", "explicit", "export", "extern", "false", "for", "friend", "goto",
    "if", "inline", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
    "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return", "sizeof", "static",
    "static_assert", "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
    "union", "using", "virtual", "volatile", "while", "xor", "xor_eq"
};

static const std::unordered_set<std::string> g_cppTypes = {
    "int", "long", "short", "char", "float", "double", "bool", "unsigned",
    "signed", "void", "size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t",
    "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t",
    "uintptr_t", "wchar_t", "char8_t", "char16_t", "char32_t",
    "string", "vector", "map", "unordered_map", "set", "unordered_set",
    "pair", "tuple", "array", "unique_ptr", "shared_ptr", "weak_ptr",
    "optional", "variant", "any", "span",
    // Win32 types
    "HWND", "HINSTANCE", "HDC", "HFONT", "HBRUSH", "HPEN", "HBITMAP",
    "LRESULT", "WPARAM", "LPARAM", "DWORD", "WORD", "BYTE", "BOOL",
    "RECT", "POINT", "SIZE", "COLORREF", "UINT", "LONG", "HRESULT",
    "WNDPROC", "CALLBACK", "WINAPI", "APIENTRY", "TRUE", "FALSE", "NULL",
    "HANDLE", "LPSTR", "LPCSTR", "LPWSTR", "LPCWSTR", "TCHAR",
    "HMODULE", "HGLOBAL", "LPVOID", "LPCVOID", "FARPROC", "ATOM"
};

static const std::unordered_set<std::string> g_cppPreprocessor = {
    "#include", "#define", "#ifdef", "#ifndef", "#endif", "#pragma",
    "#if", "#else", "#elif", "#undef", "#error", "#warning", "#line"
};

static const std::unordered_set<std::string> g_pythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"
};

static const std::unordered_set<std::string> g_pythonTypes = {
    "int", "float", "str", "bool", "list", "dict", "tuple", "set",
    "frozenset", "bytes", "bytearray", "memoryview", "complex", "range",
    "type", "object", "print", "len", "input", "open", "super", "self",
    "enumerate", "zip", "map", "filter", "sorted", "reversed", "isinstance",
    "issubclass", "hasattr", "getattr", "setattr", "delattr", "property",
    "staticmethod", "classmethod", "Exception", "ValueError", "TypeError",
    "KeyError", "IndexError", "AttributeError", "RuntimeError",
    "StopIteration", "IOError", "OSError", "FileNotFoundError"
};

static const std::unordered_set<std::string> g_jsKeywords = {
    "break", "case", "catch", "class", "const", "continue", "debugger",
    "default", "delete", "do", "else", "export", "extends", "false",
    "finally", "for", "function", "if", "import", "in", "instanceof",
    "let", "new", "null", "return", "super", "switch", "this", "throw",
    "true", "try", "typeof", "undefined", "var", "void", "while", "with",
    "yield", "async", "await", "of", "from", "as", "static", "get", "set"
};

static const std::unordered_set<std::string> g_jsTypes = {
    "Array", "Boolean", "Date", "Error", "Function", "JSON", "Map",
    "Math", "Number", "Object", "Promise", "Proxy", "Reflect", "RegExp",
    "Set", "String", "Symbol", "WeakMap", "WeakSet", "console", "document",
    "window", "navigator", "fetch", "setTimeout", "setInterval",
    "clearTimeout", "clearInterval", "parseInt", "parseFloat", "isNaN",
    "isFinite", "Buffer", "process", "require", "module", "exports",
    "globalThis", "Infinity", "NaN"
};

static const std::unordered_set<std::string> g_psKeywords = {
    "begin", "break", "catch", "class", "continue", "data", "define",
    "do", "dynamicparam", "else", "elseif", "end", "enum", "exit",
    "filter", "finally", "for", "foreach", "from", "function", "hidden",
    "if", "in", "inlinescript", "parallel", "param", "process", "return",
    "sequence", "switch", "throw", "trap", "try", "until", "using",
    "var", "while", "workflow"
};

static const std::unordered_set<std::string> g_psTypes = {
    "Write-Host", "Write-Output", "Write-Error", "Write-Warning",
    "Write-Verbose", "Write-Debug", "Get-Content", "Set-Content",
    "Get-Item", "Set-Item", "Remove-Item", "New-Item", "Get-ChildItem",
    "Get-Process", "Stop-Process", "Start-Process", "Get-Service",
    "Get-Command", "Get-Help", "Get-Member", "Select-Object",
    "Where-Object", "ForEach-Object", "Sort-Object", "Group-Object",
    "Measure-Object", "Compare-Object", "Test-Path", "Invoke-Command",
    "Invoke-Expression", "Import-Module", "Export-ModuleMember",
    "New-Object", "Add-Type", "Get-Variable", "Set-Variable"
};

static const std::unordered_set<std::string> g_asmKeywords = {
    "mov", "add", "sub", "mul", "div", "imul", "idiv", "inc", "dec",
    "and", "or", "xor", "not", "shl", "shr", "sal", "sar", "rol", "ror",
    "cmp", "test", "jmp", "je", "jne", "jz", "jnz", "jg", "jge", "jl",
    "jle", "ja", "jae", "jb", "jbe", "call", "ret", "push", "pop",
    "lea", "nop", "int", "syscall", "sysenter", "rep", "repne",
    "movzx", "movsx", "cdq", "cbw", "cwd", "cwde", "cdqe",
    "cmove", "cmovne", "cmovz", "cmovnz", "cmovg", "cmovge", "cmovl", "cmovle",
    "sete", "setne", "setg", "setge", "setl", "setle",
    "xchg", "bswap", "popcnt", "lzcnt", "tzcnt",
    "movaps", "movups", "movdqa", "movdqu", "addps", "subps", "mulps", "divps",
    "addpd", "subpd", "mulpd", "divpd", "vaddps", "vsubps", "vmulps", "vdivps",
    "section", "segment", "global", "extern", "db", "dw", "dd", "dq",
    "resb", "resw", "resd", "resq", "equ", "times", "incbin",
    "proc", "endp", "macro", "endm", ".code", ".data", ".model", ".stack",
    "invoke", "proto"
};

static const std::unordered_set<std::string> g_asmRegisters = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp",
    "ax", "bx", "cx", "dx", "si", "di", "sp", "bp",
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
    "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
    "cs", "ds", "es", "fs", "gs", "ss"
};

// ============================================================================
// HELPER: Detect language from file extension → returns lang string
// ============================================================================
static std::string detectLang(const std::string& filePath) {
    if (filePath.empty()) return "";
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = filePath.substr(dot);
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);

    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
        ext == ".cxx" || ext == ".cc" || ext == ".hxx" || ext == ".hh" ||
        ext == ".inl" || ext == ".ipp")
        return "cpp";
    if (ext == ".py" || ext == ".pyw" || ext == ".pyi")
        return "python";
    if (ext == ".js" || ext == ".jsx" || ext == ".ts" || ext == ".tsx" ||
        ext == ".mjs" || ext == ".cjs")
        return "javascript";
    if (ext == ".ps1" || ext == ".psm1" || ext == ".psd1")
        return "powershell";
    if (ext == ".json" || ext == ".jsonc" || ext == ".jsonl")
        return "json";
    if (ext == ".md" || ext == ".markdown")
        return "markdown";
    if (ext == ".asm" || ext == ".s" || ext == ".nasm" || ext == ".masm" || ext == ".inc")
        return "asm";
    return "";
}

// ============================================================================
// KEYWORD / TYPE / PREPROCESSOR LOOKUPS
// ============================================================================
bool Win32IDE::isKeyword(const std::string& word, const std::string& lang) const {
    if (lang == "cpp")        return g_cppKeywords.count(word) > 0;
    if (lang == "python")     return g_pythonKeywords.count(word) > 0;
    if (lang == "javascript") return g_jsKeywords.count(word) > 0;
    if (lang == "powershell") {
        std::string lower = word;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        return g_psKeywords.count(lower) > 0;
    }
    if (lang == "asm") {
        std::string lower = word;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        return g_asmKeywords.count(lower) > 0;
    }
    return false;
}

bool Win32IDE::isTypeKeyword(const std::string& word, const std::string& lang) const {
    if (lang == "cpp")        return g_cppTypes.count(word) > 0;
    if (lang == "python")     return g_pythonTypes.count(word) > 0;
    if (lang == "javascript") return g_jsTypes.count(word) > 0;
    if (lang == "powershell") return g_psTypes.count(word) > 0;
    if (lang == "asm") {
        std::string lower = word;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        return g_asmRegisters.count(lower) > 0;
    }
    return false;
}

bool Win32IDE::isPreprocessor(const std::string& word) const {
    return g_cppPreprocessor.count(word) > 0;
}

// ============================================================================
// TOKEN COLOR MAP (VS Code Dark+ inspired)
// ============================================================================
COLORREF Win32IDE::colorForToken(TokenType type) const {
    switch (type) {
        case TokenType::Keyword:      return RGB(86, 156, 214);    // Blue
        case TokenType::Type:         return RGB(78, 201, 176);    // Teal
        case TokenType::String:       return RGB(206, 145, 120);   // Orange-brown
        case TokenType::Comment:      return RGB(106, 153, 85);    // Green
        case TokenType::Number:       return RGB(181, 206, 168);   // Light green
        case TokenType::Preprocessor: return RGB(155, 89, 182);    // Purple
        case TokenType::Operator:     return RGB(212, 212, 212);   // Light grey
        case TokenType::Function:     return RGB(220, 220, 170);   // Yellow
        case TokenType::Macro:        return RGB(190, 140, 255);   // Lavender
        case TokenType::Default:
        default:                      return RGB(212, 212, 212);   // Default text
    }
}

// ============================================================================
// LINE TOKENIZER
//
// Signature matches header:
//   tokenizeLine(line, lang, outTokens, inBlockComment)
// 'start' field in SyntaxToken is offset within the line (0-based).
// 'inBlockComment' is carried across lines for multi-line /* */ tracking.
// ============================================================================
void Win32IDE::tokenizeLine(const std::string& line, const std::string& lang,
                             std::vector<SyntaxToken>& outTokens, bool& inBlockComment)
{
    outTokens.clear();
    int len = (int)line.size();
    int i = 0;

    // If inside a block comment from a previous line, scan for closing */
    if (inBlockComment) {
        size_t closePos = line.find("*/");
        if (closePos != std::string::npos) {
            int commentEnd = (int)(closePos + 2);
            outTokens.push_back({0, commentEnd, TokenType::Comment});
            inBlockComment = false;
            i = commentEnd;
        } else {
            outTokens.push_back({0, len, TokenType::Comment});
            return;
        }
    }

    while (i < len) {
        char ch = line[i];

        // ---- Whitespace: skip ----
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            i++;
            continue;
        }

        // ---- Single-line comments ----
        // C/C++/JS: //
        if ((lang == "cpp" || lang == "javascript") && ch == '/' && i + 1 < len && line[i + 1] == '/') {
            outTokens.push_back({i, len - i, TokenType::Comment});
            return;
        }
        // Python: #
        if (lang == "python" && ch == '#') {
            outTokens.push_back({i, len - i, TokenType::Comment});
            return;
        }
        // PowerShell: #
        if (lang == "powershell" && ch == '#') {
            outTokens.push_back({i, len - i, TokenType::Comment});
            return;
        }
        // Assembly: ;
        if (lang == "asm" && ch == ';') {
            outTokens.push_back({i, len - i, TokenType::Comment});
            return;
        }
        // JSON comments (jsonc): //
        if (lang == "json" && ch == '/' && i + 1 < len && line[i + 1] == '/') {
            outTokens.push_back({i, len - i, TokenType::Comment});
            return;
        }

        // ---- Block comments (C/C++/JS): /* ... */ ----
        if ((lang == "cpp" || lang == "javascript") && ch == '/' && i + 1 < len && line[i + 1] == '*') {
            int start = i;
            i += 2;
            size_t closePos = line.find("*/", i);
            if (closePos != std::string::npos) {
                i = (int)(closePos + 2);
                outTokens.push_back({start, i - start, TokenType::Comment});
            } else {
                outTokens.push_back({start, len - start, TokenType::Comment});
                inBlockComment = true;
                return;
            }
            continue;
        }

        // ---- Preprocessor (C/C++) ----
        if (lang == "cpp" && ch == '#') {
            int start = i;
            i++;
            while (i < len && (std::isalpha((unsigned char)line[i]) || line[i] == '_')) i++;
            outTokens.push_back({start, i - start, TokenType::Preprocessor});
            // Rest of preprocessor line is also preprocessor colored
            if (i < len) {
                outTokens.push_back({i, len - i, TokenType::Preprocessor});
                return;
            }
            continue;
        }

        // ---- Strings ----
        if (ch == '"' || ch == '\'') {
            // Python triple-quote
            if (lang == "python" && i + 2 < len && line[i + 1] == ch && line[i + 2] == ch) {
                int start = i;
                char tripleChar = ch;
                i += 3;
                while (i + 2 < len) {
                    if (line[i] == tripleChar && line[i + 1] == tripleChar && line[i + 2] == tripleChar) {
                        i += 3;
                        break;
                    }
                    i++;
                }
                if (i > len) i = len;
                outTokens.push_back({start, i - start, TokenType::String});
                continue;
            }

            char quote = ch;
            int start = i;
            i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            outTokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // ---- Backtick strings (JS template literals) ----
        if (lang == "javascript" && ch == '`') {
            int start = i;
            i++;
            while (i < len && line[i] != '`') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            outTokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // ---- Numbers ----
        if (std::isdigit((unsigned char)ch) ||
            (ch == '.' && i + 1 < len && std::isdigit((unsigned char)line[i + 1]))) {
            int start = i;
            if (ch == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                i += 2;
                while (i < len && std::isxdigit((unsigned char)line[i])) i++;
            } else if (ch == '0' && i + 1 < len && (line[i + 1] == 'b' || line[i + 1] == 'B')) {
                i += 2;
                while (i < len && (line[i] == '0' || line[i] == '1')) i++;
            } else {
                while (i < len && (std::isdigit((unsigned char)line[i]) || line[i] == '.')) i++;
                if (i < len && (line[i] == 'f' || line[i] == 'F' || line[i] == 'l' ||
                                line[i] == 'L' || line[i] == 'u' || line[i] == 'U'))
                    i++;
                if (i < len && (line[i] == 'e' || line[i] == 'E')) {
                    i++;
                    if (i < len && (line[i] == '+' || line[i] == '-')) i++;
                    while (i < len && std::isdigit((unsigned char)line[i])) i++;
                }
            }
            while (i < len && (line[i] == 'l' || line[i] == 'L' ||
                               line[i] == 'u' || line[i] == 'U')) i++;
            outTokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // ---- Identifiers / Keywords ----
        if (std::isalpha((unsigned char)ch) || ch == '_' ||
            (lang == "powershell" && ch == '$')) {
            int start = i;
            i++;
            while (i < len && (std::isalnum((unsigned char)line[i]) || line[i] == '_' ||
                               (lang == "powershell" && (line[i] == '-' || line[i] == ':')))) {
                i++;
            }
            std::string word = line.substr(start, i - start);

            // Function call: identifier followed by (
            bool isFunc = false;
            int peek = i;
            while (peek < len && (line[peek] == ' ' || line[peek] == '\t')) peek++;
            if (peek < len && line[peek] == '(') isFunc = true;

            // ALL_CAPS → Macro (C/C++ only)
            bool allCaps = true;
            for (char c : word) {
                if (c != '_' && !std::isupper((unsigned char)c) && !std::isdigit((unsigned char)c)) {
                    allCaps = false;
                    break;
                }
            }
            if (allCaps && word.size() >= 2 && lang == "cpp") {
                outTokens.push_back({start, (int)word.size(), TokenType::Macro});
            } else if (isKeyword(word, lang)) {
                outTokens.push_back({start, (int)word.size(), TokenType::Keyword});
            } else if (isTypeKeyword(word, lang)) {
                outTokens.push_back({start, (int)word.size(), TokenType::Type});
            } else if (isFunc) {
                outTokens.push_back({start, (int)word.size(), TokenType::Function});
            } else {
                outTokens.push_back({start, (int)word.size(), TokenType::Default});
            }
            continue;
        }

        // ---- Operators / Brackets ----
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
            ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '&' ||
            ch == '|' || ch == '^' || ch == '~' || ch == '?' || ch == ':' ||
            ch == ',' || ch == '.') {
            int start = i;
            i++;
            if (i < len) {
                char next = line[i];
                if ((ch == '=' && next == '=') || (ch == '!' && next == '=') ||
                    (ch == '<' && next == '=') || (ch == '>' && next == '=') ||
                    (ch == '&' && next == '&') || (ch == '|' && next == '|') ||
                    (ch == '<' && next == '<') || (ch == '>' && next == '>') ||
                    (ch == '-' && next == '>') || (ch == ':' && next == ':') ||
                    (ch == '=' && next == '>') || (ch == '+' && next == '+') ||
                    (ch == '-' && next == '-') || (ch == '+' && next == '=') ||
                    (ch == '-' && next == '=') || (ch == '*' && next == '=') ||
                    (ch == '/' && next == '='))
                    i++;
            }
            outTokens.push_back({start, i - start, TokenType::Operator});
            continue;
        }

        // ---- Everything else: default ----
        outTokens.push_back({i, 1, TokenType::Default});
        i++;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void Win32IDE::initSyntaxHighlighting() {
    m_syntaxHighlightEnabled = true;
    m_syntaxDirty = false;
    m_syntaxDirtyLineStart = -1;
    m_syntaxDirtyLineEnd = -1;

    // Detect language from current file extension
    m_syntaxLanguage = detectLang(m_currentFile);

    LOG_INFO("Syntax highlighting initialized. Language: " +
             (m_syntaxLanguage.empty() ? "(none)" : m_syntaxLanguage));

    // Trigger initial full coloring if we have content
    if (!m_syntaxLanguage.empty() && m_hwndEditor) {
        markSyntaxDirty();
    }
}

// ============================================================================
// MARK DIRTY — flags lines that need re-coloring
// ============================================================================
void Win32IDE::markSyntaxDirty(int lineStart, int lineEnd) {
    m_syntaxDirty = true;
    if (lineStart >= 0 && lineEnd >= 0) {
        if (m_syntaxDirtyLineStart < 0 || lineStart < m_syntaxDirtyLineStart)
            m_syntaxDirtyLineStart = lineStart;
        if (m_syntaxDirtyLineEnd < 0 || lineEnd > m_syntaxDirtyLineEnd)
            m_syntaxDirtyLineEnd = lineEnd;
    } else {
        m_syntaxDirtyLineStart = -1;
        m_syntaxDirtyLineEnd = -1;
    }
}

// ============================================================================
// APPLY SYNTAX HIGHLIGHTING — full document pass or delegates to range pass
// ============================================================================
void Win32IDE::applySyntaxHighlighting() {
    if (!m_syntaxHighlightEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage.empty()) return;
    if (!m_syntaxDirty) return;

    m_syntaxDirty = false;

    // If specific dirty range, use range-based coloring
    if (m_syntaxDirtyLineStart >= 0 && m_syntaxDirtyLineEnd >= 0) {
        applySyntaxHighlightingRange(m_syntaxDirtyLineStart, m_syntaxDirtyLineEnd);
        m_syntaxDirtyLineStart = -1;
        m_syntaxDirtyLineEnd = -1;
        return;
    }

    // Full document coloring
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0 || textLen > 500000) return;

    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Save state
    CHARRANGE oldSel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    POINT scrollPos;
    SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

    // Lock redraw + disable EN_CHANGE notifications during formatting
    SendMessage(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
    DWORD eventMask = (DWORD)SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, 0);

    // Tokenize and color each line
    bool inBlockComment = false;
    int offset = 0;

    while (offset < textLen) {
        int lineEnd = offset;
        while (lineEnd < textLen && text[lineEnd] != '\n') lineEnd++;

        std::string lineText = text.substr(offset, lineEnd - offset);

        std::vector<SyntaxToken> tokens;
        tokenizeLine(lineText, m_syntaxLanguage, tokens, inBlockComment);

        for (const auto& tok : tokens) {
            if (tok.type == TokenType::Default) continue;

            CHARRANGE cr;
            cr.cpMin = offset + tok.start;
            cr.cpMax = offset + tok.start + tok.length;
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);

            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = colorForToken(tok.type);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }

        offset = lineEnd + 1;
    }

    // Restore state
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, eventMask);
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);

    m_syntaxDirtyLineStart = -1;
    m_syntaxDirtyLineEnd = -1;
}

// ============================================================================
// RANGE-BASED SYNTAX HIGHLIGHTING — re-colors lineStart..lineEnd only
// ============================================================================
void Win32IDE::applySyntaxHighlightingRange(int lineStart, int lineEnd) {
    if (!m_syntaxHighlightEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage.empty()) return;

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    if (lineStart < 0) lineStart = 0;
    if (lineEnd >= lineCount) lineEnd = lineCount - 1;
    if (lineStart > lineEnd) return;

    // Expand range by 1 line in each direction for block comment boundaries
    if (lineStart > 0) lineStart--;
    if (lineEnd < lineCount - 1) lineEnd++;

    int charStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineStart, 0);
    int charEndLine = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineEnd, 0);
    int charEndLineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charEndLine, 0);
    int charEnd = charEndLine + charEndLineLen;
    if (charStart < 0) charStart = 0;
    if (charEnd > textLen) charEnd = textLen;

    std::string fullText(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &fullText[0], textLen + 1);
    fullText.resize(textLen);

    // Save state
    CHARRANGE oldSel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    POINT scrollPos;
    SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
    DWORD eventMask = (DWORD)SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, 0);

    // Determine block comment state before lineStart
    bool inBlockComment = false;
    if (lineStart > 0) {
        std::string prefix = fullText.substr(0, charStart);
        int depth = 0;
        for (size_t ci = 0; ci + 1 < prefix.size(); ci++) {
            if (prefix[ci] == '/' && prefix[ci + 1] == '*') { depth++; ci++; }
            else if (prefix[ci] == '*' && prefix[ci + 1] == '/') { depth--; ci++; }
        }
        inBlockComment = (depth > 0);
    }

    // Tokenize and color each line in the range
    for (int lineIdx = lineStart; lineIdx <= lineEnd; lineIdx++) {
        int lineCharStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lineCharStart, 0);
        if (lineCharStart < 0) break;

        std::string lineText = fullText.substr(lineCharStart, lineLen);

        // Reset entire line to default color
        {
            CHARRANGE cr = { lineCharStart, lineCharStart + lineLen };
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = colorForToken(TokenType::Default);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }

        std::vector<SyntaxToken> tokens;
        tokenizeLine(lineText, m_syntaxLanguage, tokens, inBlockComment);

        for (const auto& tok : tokens) {
            if (tok.type == TokenType::Default) continue;

            CHARRANGE cr;
            cr.cpMin = lineCharStart + tok.start;
            cr.cpMax = lineCharStart + tok.start + tok.length;
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);

            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = colorForToken(tok.type);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }
    }

    // Restore state
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, eventMask);
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}
