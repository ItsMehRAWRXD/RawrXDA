// ============================================================================
// Win32IDE_SyntaxHighlight.cpp — Incremental Syntax Coloring Engine
// ============================================================================
// Provides real-time syntax coloring for the RichEdit editor control.
// Uses debounced EN_CHANGE → tokenize → EM_SETCHARFORMAT pipeline.
//
// Supported languages: C/C++, Python, JavaScript, PowerShell, JSON, Assembly
// Design: All coloring is UI-thread, debounced via WM_TIMER to avoid latency.
//
// Method signatures match Win32IDE.h declarations:
//   initSyntaxColorizer(), applySyntaxColoring()
//   applySyntaxColoringForRange(startChar, endChar)
//   tokenizeLine(line, lineStartOffset, lang) → vector<SyntaxToken>
//   tokenizeDocument(text, lang) → vector<SyntaxToken>
//   getTokenColor(type), detectLanguageFromExtension(filePath)
//   onEditorContentChanged(), SyntaxColorTimerProc(...)
//   isKeyword(word, lang), isBuiltinType(word, lang)
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

// ============================================================================
// RAII guard for EM_HIDESELECTION — guarantees selection visibility is restored
// even if the coloring path exits early (e.g. tokenization failure, OOM).
// Also manages WM_SETREDRAW and event mask as a single transaction.
// ============================================================================
class ScopedEditorFormatLock {
public:
    ScopedEditorFormatLock(HWND hwndEditor)
        : m_hwnd(hwndEditor), m_locked(false) {
        if (!m_hwnd || !IsWindow(m_hwnd)) return;
        SendMessage(m_hwnd, EM_EXGETSEL, 0, (LPARAM)&m_oldSel);
        SendMessage(m_hwnd, EM_GETSCROLLPOS, 0, (LPARAM)&m_scrollPos);
        SendMessage(m_hwnd, EM_HIDESELECTION, TRUE, 0);
        SendMessage(m_hwnd, WM_SETREDRAW, FALSE, 0);
        m_eventMask = (DWORD)SendMessage(m_hwnd, EM_SETEVENTMASK, 0, 0);
        m_locked = true;
    }
    ~ScopedEditorFormatLock() { Unlock(); }

    void Unlock() {
        if (!m_locked) return;
        m_locked = false;
        SendMessage(m_hwnd, EM_SETEVENTMASK, 0, m_eventMask);
        SendMessage(m_hwnd, EM_EXSETSEL, 0, (LPARAM)&m_oldSel);
        SendMessage(m_hwnd, EM_SETSCROLLPOS, 0, (LPARAM)&m_scrollPos);
        SendMessage(m_hwnd, EM_HIDESELECTION, FALSE, 0);
        SendMessage(m_hwnd, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    // Non-copyable
    ScopedEditorFormatLock(const ScopedEditorFormatLock&) = delete;
    ScopedEditorFormatLock& operator=(const ScopedEditorFormatLock&) = delete;

private:
    HWND m_hwnd;
    bool m_locked;
    CHARRANGE m_oldSel{};
    POINT m_scrollPos{};
    DWORD m_eventMask = 0;
};

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
    "WNDPROC", "CALLBACK", "WINAPI", "APIENTRY",
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
// KEYWORD / TYPE LOOKUPS
// ============================================================================
bool Win32IDE::isKeyword(const std::string& word, SyntaxLanguage lang) const {
    switch (lang) {
        case SyntaxLanguage::Cpp:
            return g_cppKeywords.count(word) > 0;
        case SyntaxLanguage::Python:
            return g_pythonKeywords.count(word) > 0;
        case SyntaxLanguage::JavaScript:
            return g_jsKeywords.count(word) > 0;
        case SyntaxLanguage::PowerShell: {
            std::string lower = word;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            return g_psKeywords.count(lower) > 0;
        }
        case SyntaxLanguage::Assembly: {
            std::string lower = word;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            return g_asmKeywords.count(lower) > 0;
        }
        default:
            return false;
    }
}

bool Win32IDE::isBuiltinType(const std::string& word, SyntaxLanguage lang) const {
    switch (lang) {
        case SyntaxLanguage::Cpp:
            return g_cppTypes.count(word) > 0;
        case SyntaxLanguage::Python:
            return g_pythonTypes.count(word) > 0;
        case SyntaxLanguage::JavaScript:
            return g_jsTypes.count(word) > 0;
        case SyntaxLanguage::PowerShell:
            return g_psTypes.count(word) > 0;
        case SyntaxLanguage::Assembly: {
            std::string lower = word;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            return g_asmRegisters.count(lower) > 0;
        }
        default:
            return false;
    }
}

// ============================================================================
// LANGUAGE DETECTION from file extension
// ============================================================================
Win32IDE::SyntaxLanguage Win32IDE::detectLanguageFromExtension(const std::string& filePath) const {
    if (filePath.empty()) return SyntaxLanguage::None;
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return SyntaxLanguage::None;
    std::string ext = filePath.substr(dot);
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);

    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
        ext == ".cxx" || ext == ".cc" || ext == ".hxx" || ext == ".hh" ||
        ext == ".inl" || ext == ".ipp")
        return SyntaxLanguage::Cpp;
    if (ext == ".py" || ext == ".pyw" || ext == ".pyi")
        return SyntaxLanguage::Python;
    if (ext == ".js" || ext == ".jsx" || ext == ".ts" || ext == ".tsx" ||
        ext == ".mjs" || ext == ".cjs")
        return SyntaxLanguage::JavaScript;
    if (ext == ".ps1" || ext == ".psm1" || ext == ".psd1")
        return SyntaxLanguage::PowerShell;
    if (ext == ".json" || ext == ".jsonc" || ext == ".jsonl")
        return SyntaxLanguage::JSON;
    if (ext == ".md" || ext == ".markdown")
        return SyntaxLanguage::Markdown;
    if (ext == ".asm" || ext == ".s" || ext == ".nasm" || ext == ".masm" || ext == ".inc")
        return SyntaxLanguage::Assembly;
    return SyntaxLanguage::None;
}

// ============================================================================
// TOKEN COLOR MAP — reads from the active theme's per-token syntax palette.
// Each IDETheme carries keywordColor, commentColor, stringColor etc.
// If the theme field is 0 (uninitialised) we fall back to VS Code Dark+ defaults.
// ============================================================================
COLORREF Win32IDE::getTokenColor(TokenType type) const {
    switch (type) {
        case TokenType::Keyword:
            return m_currentTheme.keywordColor     ? m_currentTheme.keywordColor     : RGB(86, 156, 214);
        case TokenType::BuiltinType:
            return m_currentTheme.typeColor         ? m_currentTheme.typeColor         : RGB(78, 201, 176);
        case TokenType::String:
            return m_currentTheme.stringColor       ? m_currentTheme.stringColor       : RGB(206, 145, 120);
        case TokenType::Comment:
            return m_currentTheme.commentColor      ? m_currentTheme.commentColor      : RGB(106, 153, 85);
        case TokenType::Number:
            return m_currentTheme.numberColor       ? m_currentTheme.numberColor       : RGB(181, 206, 168);
        case TokenType::Preprocessor:
            return m_currentTheme.preprocessorColor ? m_currentTheme.preprocessorColor : RGB(155, 89, 182);
        case TokenType::Operator:
            return m_currentTheme.operatorColor     ? m_currentTheme.operatorColor     : RGB(212, 212, 212);
        case TokenType::Function:
            return m_currentTheme.functionColor     ? m_currentTheme.functionColor     : RGB(220, 220, 170);
        case TokenType::Bracket:
            // Use accent color for brackets if available, else gold
            return m_currentTheme.accentColor       ? m_currentTheme.accentColor       : RGB(255, 215, 0);
        case TokenType::Default:
        default:
            return m_currentTheme.textColor         ? m_currentTheme.textColor         : RGB(212, 212, 212);
    }
}

// ============================================================================
// LINE TOKENIZER
//
// Signature: tokenizeLine(line, lineStartOffset, lang) → vector<SyntaxToken>
// SyntaxToken::start is absolute offset in the editor (lineStartOffset + local).
// Uses member m_inBlockComment for cross-line /* */ tracking.
// ============================================================================
std::vector<Win32IDE::SyntaxToken> Win32IDE::tokenizeLine(
    const std::string& line, int lineStartOffset, SyntaxLanguage lang)
{
    std::vector<SyntaxToken> tokens;
    int len = (int)line.size();
    int i = 0;

    // If inside a block comment from a previous line, scan for closing */
    if (m_inBlockComment) {
        size_t closePos = line.find("*/");
        if (closePos != std::string::npos) {
            int commentEnd = (int)(closePos + 2);
            tokens.push_back({lineStartOffset, commentEnd, TokenType::Comment});
            m_inBlockComment = false;
            i = commentEnd;
        } else {
            tokens.push_back({lineStartOffset, len, TokenType::Comment});
            return tokens;
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
        if ((lang == SyntaxLanguage::Cpp || lang == SyntaxLanguage::JavaScript) &&
            ch == '/' && i + 1 < len && line[i + 1] == '/') {
            tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
            return tokens;
        }
        if (lang == SyntaxLanguage::Python && ch == '#') {
            tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
            return tokens;
        }
        if (lang == SyntaxLanguage::PowerShell && ch == '#') {
            tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
            return tokens;
        }
        if (lang == SyntaxLanguage::Assembly && ch == ';') {
            tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
            return tokens;
        }
        if (lang == SyntaxLanguage::JSON && ch == '/' && i + 1 < len && line[i + 1] == '/') {
            tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
            return tokens;
        }

        // ---- Block comments (C/C++/JS): /* ... */ ----
        if ((lang == SyntaxLanguage::Cpp || lang == SyntaxLanguage::JavaScript) &&
            ch == '/' && i + 1 < len && line[i + 1] == '*') {
            int start = i;
            i += 2;
            size_t closePos = line.find("*/", i);
            if (closePos != std::string::npos) {
                i = (int)(closePos + 2);
                tokens.push_back({lineStartOffset + start, i - start, TokenType::Comment});
            } else {
                tokens.push_back({lineStartOffset + start, len - start, TokenType::Comment});
                m_inBlockComment = true;
                return tokens;
            }
            continue;
        }

        // ---- Preprocessor (C/C++) ----
        if (lang == SyntaxLanguage::Cpp && ch == '#') {
            int start = i;
            i++;
            while (i < len && (std::isalpha((unsigned char)line[i]) || line[i] == '_')) i++;
            // Color the entire rest of the preprocessor line
            tokens.push_back({lineStartOffset + start, len - start, TokenType::Preprocessor});
            return tokens;
        }

        // ---- Strings ----
        if (ch == '"' || ch == '\'') {
            // Python triple-quote
            if (lang == SyntaxLanguage::Python && i + 2 < len &&
                line[i + 1] == ch && line[i + 2] == ch) {
                int start = i;
                char tripleChar = ch;
                i += 3;
                while (i + 2 < len) {
                    if (line[i] == tripleChar && line[i + 1] == tripleChar &&
                        line[i + 2] == tripleChar) {
                        i += 3;
                        break;
                    }
                    i++;
                }
                if (i > len) i = len;
                tokens.push_back({lineStartOffset + start, i - start, TokenType::String});
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
            tokens.push_back({lineStartOffset + start, i - start, TokenType::String});
            continue;
        }

        // ---- Backtick strings (JS template literals) ----
        if (lang == SyntaxLanguage::JavaScript && ch == '`') {
            int start = i;
            i++;
            while (i < len && line[i] != '`') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            tokens.push_back({lineStartOffset + start, i - start, TokenType::String});
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
                if (i < len && (line[i] == 'e' || line[i] == 'E')) {
                    i++;
                    if (i < len && (line[i] == '+' || line[i] == '-')) i++;
                    while (i < len && std::isdigit((unsigned char)line[i])) i++;
                }
            }
            // Consume suffixes: f, F, l, L, u, U, ll, LL
            while (i < len && (line[i] == 'f' || line[i] == 'F' ||
                               line[i] == 'l' || line[i] == 'L' ||
                               line[i] == 'u' || line[i] == 'U')) i++;
            tokens.push_back({lineStartOffset + start, i - start, TokenType::Number});
            continue;
        }

        // ---- JSON keys (strings before colon) — handled by string parser above ----

        // ---- Identifiers / Keywords ----
        if (std::isalpha((unsigned char)ch) || ch == '_' ||
            (lang == SyntaxLanguage::PowerShell && ch == '$')) {
            int start = i;
            i++;
            while (i < len && (std::isalnum((unsigned char)line[i]) || line[i] == '_' ||
                               (lang == SyntaxLanguage::PowerShell &&
                                (line[i] == '-' || line[i] == ':')))) {
                i++;
            }
            std::string word = line.substr(start, i - start);

            // Function call: identifier followed by (
            bool isFunc = false;
            int peek = i;
            while (peek < len && (line[peek] == ' ' || line[peek] == '\t')) peek++;
            if (peek < len && line[peek] == '(') isFunc = true;

            if (isKeyword(word, lang)) {
                tokens.push_back({lineStartOffset + start, (int)word.size(), TokenType::Keyword});
            } else if (isBuiltinType(word, lang)) {
                tokens.push_back({lineStartOffset + start, (int)word.size(), TokenType::BuiltinType});
            } else if (isFunc) {
                tokens.push_back({lineStartOffset + start, (int)word.size(), TokenType::Function});
            } else {
                tokens.push_back({lineStartOffset + start, (int)word.size(), TokenType::Default});
            }
            continue;
        }

        // ---- Brackets ----
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}') {
            tokens.push_back({lineStartOffset + i, 1, TokenType::Bracket});
            i++;
            continue;
        }

        // ---- Operators ----
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
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
            tokens.push_back({lineStartOffset + start, i - start, TokenType::Operator});
            continue;
        }

        // ---- Semicolons (treated as operator for consistency) ----
        if (ch == ';' && lang != SyntaxLanguage::Assembly) {
            tokens.push_back({lineStartOffset + i, 1, TokenType::Operator});
            i++;
            continue;
        }

        // ---- Everything else: default ----
        tokens.push_back({lineStartOffset + i, 1, TokenType::Default});
        i++;
    }

    return tokens;
}

// ============================================================================
// DOCUMENT TOKENIZER — tokenizes full document text into flat token list
// ============================================================================
std::vector<Win32IDE::SyntaxToken> Win32IDE::tokenizeDocument(
    const std::string& text, SyntaxLanguage lang)
{
    std::vector<SyntaxToken> allTokens;
    if (lang == SyntaxLanguage::None || text.empty()) return allTokens;

    // Reset block comment state for full-document pass
    bool savedBlockComment = m_inBlockComment;
    m_inBlockComment = false;

    int len = (int)text.size();
    int offset = 0;

    while (offset < len) {
        int lineEnd = offset;
        while (lineEnd < len && text[lineEnd] != '\n') lineEnd++;

        std::string lineText = text.substr(offset, lineEnd - offset);
        auto lineTokens = tokenizeLine(lineText, offset, lang);

        for (auto& tok : lineTokens) {
            allTokens.push_back(tok);
        }

        offset = lineEnd + 1;
    }

    // Restore block comment state (shouldn't matter for full-doc but be safe)
    m_inBlockComment = savedBlockComment;

    return allTokens;
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void Win32IDE::initSyntaxColorizer() {
    m_syntaxColoringEnabled = true;
    m_syntaxDirty = false;
    m_inBlockComment = false;

    // Detect language from current file extension
    m_syntaxLanguage = detectLanguageFromExtension(m_currentFile);

    LOG_INFO("Syntax colorizer initialized. Language: " +
             std::to_string((int)m_syntaxLanguage) +
             " for file: " + (m_currentFile.empty() ? "(none)" : m_currentFile));

    // Trigger initial full coloring if we have content and a supported language
    if (m_syntaxLanguage != SyntaxLanguage::None && m_hwndEditor) {
        m_syntaxDirty = true;
        applySyntaxColoring();
    }
}

// ============================================================================
// GET VISIBLE EDITOR LINES — returns the range currently shown in the editor
//
// Computes the first and last visible line (0-based) using EM_GETFIRSTVISIBLELINE,
// the editor client rect, and font metrics.  Adds a ±8-line margin for
// smooth scrolling so the coloring is ready just before lines scroll into view.
// ============================================================================
Win32IDE::VisibleLineRange Win32IDE::getVisibleEditorLines() const {
    VisibleLineRange range = {0, 0, 0, 16};

    if (!m_hwndEditor) return range;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);

    // Get line height via font metrics
    RECT editorRect;
    GetClientRect(m_hwndEditor, &editorRect);
    int editorHeight = editorRect.bottom - editorRect.top;

    HDC hdc = GetDC(m_hwndEditor);
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndEditor, hdc);

    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;
    int visibleLineCount = editorHeight / lineHeight;

    const int VISIBLE_MARGIN = 8;
    int rangeStart = firstVisibleLine - VISIBLE_MARGIN;
    int rangeEnd   = firstVisibleLine + visibleLineCount + VISIBLE_MARGIN;
    if (rangeStart < 0) rangeStart = 0;
    if (rangeEnd >= lineCount) rangeEnd = lineCount - 1;

    range.firstLine  = rangeStart;
    range.lastLine   = rangeEnd;
    range.lineCount  = rangeEnd - rangeStart + 1;
    range.lineHeight = lineHeight;

    return range;
}

// ============================================================================
// GET CURRENT SYNTAX LANGUAGE — accessor for the detected language enum
// ============================================================================
Win32IDE::SyntaxLanguage Win32IDE::getCurrentSyntaxLanguage() const {
    return m_syntaxLanguage;
}

// ============================================================================
// APPLY SYNTAX COLORING FOR VISIBLE RANGE — named wrapper
//
// Forces the visible-range coloring path regardless of file size.  This is
// the entry point used by resize/scroll triggers that know only the visible
// portion changed.  Sets the dirty flag and delegates to applySyntaxColoring().
// ============================================================================
void Win32IDE::applySyntaxColoringForVisibleRange() {
    if (!m_syntaxColoringEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage == SyntaxLanguage::None) return;

    m_syntaxDirty = true;
    applySyntaxColoring();
}

// ============================================================================
// DEBOUNCED CONTENT CHANGE HANDLER
// Called from WM_NOTIFY EN_CHANGE / WM_COMMAND EN_CHANGE / WM_VSCROLL
// Sets a timer; when it fires, actual coloring runs.
// ============================================================================
void Win32IDE::onEditorContentChanged() {
    if (!m_syntaxColoringEnabled) return;
    if (m_syntaxLanguage == SyntaxLanguage::None) return;

    m_syntaxDirty = true;

    // Reset the debounce timer — each keystroke re-arms it
    if (m_hwndMain) {
        SetTimer(m_hwndMain, SYNTAX_COLOR_TIMER_ID, SYNTAX_COLOR_DELAY_MS, nullptr);
    }
}

// ============================================================================
// TIMER CALLBACK — static callback version (unused if fallback WM_TIMER used)
// ============================================================================
void CALLBACK Win32IDE::SyntaxColorTimerProc(HWND hwnd, UINT /*uMsg*/,
                                              UINT_PTR idEvent, DWORD /*dwTime*/)
{
    KillTimer(hwnd, idEvent);

    // Retrieve Win32IDE pointer from GWLP_USERDATA
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (ide && ide->m_syntaxColoringEnabled) {
        ide->applySyntaxColoring();
    }
}

// ============================================================================
// APPLY SYNTAX COLORING — visible-range optimized pass
//
// Performance optimization: Only colors the currently visible lines in the
// editor, plus a small margin above/below for smooth scrolling.  For files
// under ~5 KB we still do a full-document pass for simplicity.  For larger
// files this avoids the O(N) EM_SETCHARFORMAT sweep that freezes the UI.
//
// Block comment state is computed by scanning text *before* the visible
// range so multi-line comments stay correct even when partially off-screen.
// ============================================================================
void Win32IDE::applySyntaxColoring() {
    if (!m_syntaxColoringEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage == SyntaxLanguage::None) return;
    if (!m_syntaxDirty) return;

    m_syntaxDirty = false;

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    // Skip extremely large files to avoid UI freeze (> 500KB)
    if (textLen > 500000) {
        LOG_WARNING("File too large for syntax coloring: " + std::to_string(textLen) + " chars");
        return;
    }

    // --- Small-file fast path: full-document coloring for files <= 5 KB ---
    if (textLen <= 5000) {
        std::string text(textLen + 1, '\0');
        GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
        text.resize(textLen);

        // RAII lock: saves selection/scroll, hides selection, disables redraw.
        // Automatically restores on scope exit (including early returns).
        ScopedEditorFormatLock lock(m_hwndEditor);

        // Reset all text to default
        {
            CHARRANGE crAll = {0, textLen};
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&crAll);
            CHARFORMAT2A cfDefault = {};
            cfDefault.cbSize = sizeof(cfDefault);
            cfDefault.dwMask = CFM_COLOR;
            cfDefault.crTextColor = getTokenColor(TokenType::Default);
            cfDefault.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDefault);
        }

        auto tokens = tokenizeDocument(text, m_syntaxLanguage);
        for (const auto& tok : tokens) {
            if (tok.type == TokenType::Default) continue;
            CHARRANGE cr;
            cr.cpMin = tok.start;
            cr.cpMax = tok.start + tok.length;
            if (cr.cpMax > textLen) cr.cpMax = textLen;
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = getTokenColor(tok.type);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }

        // ScopedEditorFormatLock destructor restores selection, scroll, redraw
        return;
    }

    // --- Large-file path: visible-range only coloring ---

    // Determine the visible line range
    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);

    // Estimate visible line count from editor height + font metrics
    RECT editorRect;
    GetClientRect(m_hwndEditor, &editorRect);
    int editorHeight = editorRect.bottom - editorRect.top;

    // Get line height via font metrics
    HDC hdc = GetDC(m_hwndEditor);
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndEditor, hdc);

    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;
    int visibleLineCount = editorHeight / lineHeight;

    // Expand by a margin for smoother scrolling (8 lines above/below)
    const int VISIBLE_MARGIN = 8;
    int rangeStart = firstVisibleLine - VISIBLE_MARGIN;
    int rangeEnd   = firstVisibleLine + visibleLineCount + VISIBLE_MARGIN;
    if (rangeStart < 0) rangeStart = 0;
    if (rangeEnd >= lineCount) rangeEnd = lineCount - 1;

    // Convert line range → character range
    int charRangeStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, rangeStart, 0);
    int lastLineIdx    = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, rangeEnd, 0);
    int lastLineLen    = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lastLineIdx, 0);
    int charRangeEnd   = lastLineIdx + lastLineLen;
    if (charRangeStart < 0) charRangeStart = 0;
    if (charRangeEnd > textLen) charRangeEnd = textLen;

    // Read full text (needed for block-comment state before range)
    std::string fullText(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &fullText[0], textLen + 1);
    fullText.resize(textLen);

    // RAII lock: saves selection/scroll, hides selection, disables redraw.
    ScopedEditorFormatLock lock(m_hwndEditor);

    // Compute block comment state from all text *before* the visible range
    m_inBlockComment = false;
    if (charRangeStart > 0) {
        std::string prefix = fullText.substr(0, charRangeStart);
        int depth = 0;
        for (size_t ci = 0; ci + 1 < prefix.size(); ci++) {
            if (prefix[ci] == '/' && prefix[ci + 1] == '*') { depth++; ci++; }
            else if (prefix[ci] == '*' && prefix[ci + 1] == '/') { if (depth > 0) depth--; ci++; }
        }
        m_inBlockComment = (depth > 0);
    }

    // Reset the visible range to default color
    {
        CHARRANGE crRange = {(LONG)charRangeStart, (LONG)charRangeEnd};
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&crRange);
        CHARFORMAT2A cfDefault = {};
        cfDefault.cbSize = sizeof(cfDefault);
        cfDefault.dwMask = CFM_COLOR;
        cfDefault.crTextColor = getTokenColor(TokenType::Default);
        cfDefault.dwEffects = 0;
        SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDefault);
    }

    // Tokenize each line in the visible range
    for (int lineIdx = rangeStart; lineIdx <= rangeEnd; lineIdx++) {
        int lineCharStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lineCharStart, 0);
        if (lineCharStart < 0) break;

        // Guard: skip extremely long lines (>10 KB) to prevent UI freeze
        if (lineLen > 10000) {
            LOG_DEBUG("Syntax coloring: skipping line " + std::to_string(lineIdx) +
                      " — length " + std::to_string(lineLen) + " exceeds 10000 char limit");
            continue;
        }

        std::string lineText = fullText.substr(lineCharStart, lineLen);
        auto lineTokens = tokenizeLine(lineText, lineCharStart, m_syntaxLanguage);

        for (const auto& tok : lineTokens) {
            if (tok.type == TokenType::Default) continue;
            CHARRANGE cr;
            cr.cpMin = tok.start;
            cr.cpMax = tok.start + tok.length;
            if (cr.cpMax > textLen) cr.cpMax = textLen;
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = getTokenColor(tok.type);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }
    }

    // ScopedEditorFormatLock destructor handles restore
}

// ============================================================================
// RANGE-BASED SYNTAX COLORING — colors characters startChar..endChar only
// Used for incremental updates when only a portion of the document changed.
// ============================================================================
void Win32IDE::applySyntaxColoringForRange(int startChar, int endChar) {
    if (!m_syntaxColoringEnabled || !m_hwndEditor) return;
    if (m_syntaxLanguage == SyntaxLanguage::None) return;

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;
    if (startChar < 0) startChar = 0;
    if (endChar > textLen) endChar = textLen;
    if (startChar >= endChar) return;

    // Expand range to full lines for correct tokenization
    // Find line start for startChar
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, startChar, 0);
    int lineEnd = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, endChar, 0);

    // Expand by 1 line in each direction for block comment boundaries
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    if (lineStart > 0) lineStart--;
    if (lineEnd < lineCount - 1) lineEnd++;

    int charRangeStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineStart, 0);
    int lastLineIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineEnd, 0);
    int lastLineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lastLineIdx, 0);
    int charRangeEnd = lastLineIdx + lastLineLen;
    if (charRangeStart < 0) charRangeStart = 0;
    if (charRangeEnd > textLen) charRangeEnd = textLen;

    // Read full text (needed for block comment state detection before range)
    std::string fullText(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &fullText[0], textLen + 1);
    fullText.resize(textLen);

    // RAII lock: saves selection/scroll, hides selection, disables redraw.
    ScopedEditorFormatLock lock(m_hwndEditor);

    // Determine block comment state before the range starts
    m_inBlockComment = false;
    if (charRangeStart > 0) {
        std::string prefix = fullText.substr(0, charRangeStart);
        int depth = 0;
        for (size_t ci = 0; ci + 1 < prefix.size(); ci++) {
            if (prefix[ci] == '/' && prefix[ci + 1] == '*') { depth++; ci++; }
            else if (prefix[ci] == '*' && prefix[ci + 1] == '/') { depth--; ci++; }
        }
        m_inBlockComment = (depth > 0);
    }

    // Reset the range to default color
    {
        CHARRANGE crRange = {(LONG)charRangeStart, (LONG)charRangeEnd};
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&crRange);
        CHARFORMAT2A cfDefault = {};
        cfDefault.cbSize = sizeof(cfDefault);
        cfDefault.dwMask = CFM_COLOR;
        cfDefault.crTextColor = getTokenColor(TokenType::Default);
        cfDefault.dwEffects = 0;
        SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDefault);
    }

    // Tokenize each line in the range
    for (int lineIdx = lineStart; lineIdx <= lineEnd; lineIdx++) {
        int lineCharStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lineCharStart, 0);
        if (lineCharStart < 0) break;

        std::string lineText = fullText.substr(lineCharStart, lineLen);
        auto lineTokens = tokenizeLine(lineText, lineCharStart, m_syntaxLanguage);

        for (const auto& tok : lineTokens) {
            if (tok.type == TokenType::Default) continue;

            CHARRANGE cr;
            cr.cpMin = tok.start;
            cr.cpMax = tok.start + tok.length;
            if (cr.cpMax > textLen) cr.cpMax = textLen;
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);

            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = getTokenColor(tok.type);
            cf.dwEffects = 0;
            SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }
    }

    // Restore editor state
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, eventMask);
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, EM_HIDESELECTION, FALSE, 0);
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}
