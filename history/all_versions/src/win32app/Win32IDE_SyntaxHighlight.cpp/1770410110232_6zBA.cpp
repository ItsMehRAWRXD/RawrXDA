// ============================================================================
// Win32IDE_SyntaxHighlight.cpp — Incremental Syntax Coloring Engine
// ============================================================================
// Provides real-time syntax coloring for the RichEdit editor control.
// Uses debounced EN_CHANGE → tokenize → EM_SETCHARFORMAT pipeline.
//
// Supported languages: C/C++, Python, JavaScript, PowerShell, JSON, Assembly
// Design: Zero background threads — all coloring is UI-thread, debounced
// to avoid typing latency.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

// ============================================================================
// KEYWORD TABLES — one per language
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
    "union", "using", "virtual", "void", "volatile", "while", "xor", "xor_eq",
    "#include", "#define", "#ifdef", "#ifndef", "#endif", "#pragma", "#if",
    "#else", "#elif", "#undef", "#error", "#warning"
};

static const std::unordered_set<std::string> g_cppBuiltinTypes = {
    "int", "long", "short", "char", "float", "double", "bool", "unsigned",
    "signed", "size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t", "uintptr_t",
    "wchar_t", "char8_t", "char16_t", "char32_t", "string", "vector", "map",
    "unordered_map", "set", "unordered_set", "pair", "tuple", "array",
    "unique_ptr", "shared_ptr", "weak_ptr", "optional", "variant",
    "HWND", "HINSTANCE", "HDC", "HFONT", "HBRUSH", "HPEN", "HBITMAP",
    "LRESULT", "WPARAM", "LPARAM", "DWORD", "WORD", "BYTE", "BOOL",
    "RECT", "POINT", "SIZE", "COLORREF", "UINT", "LONG", "HRESULT",
    "WNDPROC", "CALLBACK", "WINAPI", "APIENTRY", "TRUE", "FALSE", "NULL",
    "HANDLE", "LPSTR", "LPCSTR", "LPWSTR", "LPCWSTR", "TCHAR"
};

static const std::unordered_set<std::string> g_pythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"
};

static const std::unordered_set<std::string> g_pythonBuiltins = {
    "int", "float", "str", "bool", "list", "dict", "tuple", "set",
    "frozenset", "bytes", "bytearray", "memoryview", "complex", "range",
    "type", "object", "print", "len", "input", "open", "super", "self",
    "enumerate", "zip", "map", "filter", "sorted", "reversed", "isinstance",
    "issubclass", "hasattr", "getattr", "setattr", "delattr", "property",
    "staticmethod", "classmethod", "Exception", "ValueError", "TypeError",
    "KeyError", "IndexError", "AttributeError", "RuntimeError", "StopIteration",
    "IOError", "OSError", "FileNotFoundError"
};

static const std::unordered_set<std::string> g_jsKeywords = {
    "break", "case", "catch", "class", "const", "continue", "debugger",
    "default", "delete", "do", "else", "export", "extends", "false",
    "finally", "for", "function", "if", "import", "in", "instanceof",
    "let", "new", "null", "return", "super", "switch", "this", "throw",
    "true", "try", "typeof", "undefined", "var", "void", "while", "with",
    "yield", "async", "await", "of", "from", "as", "static", "get", "set"
};

static const std::unordered_set<std::string> g_jsBuiltins = {
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

static const std::unordered_set<std::string> g_psBuiltins = {
    "Write-Host", "Write-Output", "Write-Error", "Write-Warning",
    "Write-Verbose", "Write-Debug", "Get-Content", "Set-Content",
    "Get-Item", "Set-Item", "Remove-Item", "New-Item", "Get-ChildItem",
    "Get-Process", "Stop-Process", "Start-Process", "Get-Service",
    "Get-Command", "Get-Help", "Get-Member", "Select-Object",
    "Where-Object", "ForEach-Object", "Sort-Object", "Group-Object",
    "Measure-Object", "Compare-Object", "Test-Path", "Invoke-Command",
    "Invoke-Expression", "Import-Module", "Export-ModuleMember",
    "New-Object", "Add-Type", "Get-Variable", "Set-Variable",
    "$true", "$false", "$null", "$_", "$PSVersionTable", "$ErrorActionPreference",
    "$Error", "$Host", "$HOME", "$PWD", "$PSScriptRoot"
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
    // SSE/AVX
    "movaps", "movups", "movdqa", "movdqu", "addps", "subps", "mulps", "divps",
    "addpd", "subpd", "mulpd", "divpd", "vaddps", "vsubps", "vmulps", "vdivps",
    // Directives
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
// LANGUAGE DETECTION
// ============================================================================
Win32IDE::SyntaxLanguage Win32IDE::detectLanguageFromExtension(const std::string& filePath) const {
    if (filePath.empty()) return SyntaxLanguage::None;
    
    // Find last dot
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) return SyntaxLanguage::None;
    
    std::string ext = filePath.substr(dotPos);
    // Lowercase the extension
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
    
    if (ext == ".asm" || ext == ".s" || ext == ".nasm" || ext == ".masm" ||
        ext == ".inc")
        return SyntaxLanguage::Assembly;
    
    return SyntaxLanguage::None;
}

// ============================================================================
// KEYWORD LOOKUP
// ============================================================================
bool Win32IDE::isKeyword(const std::string& word, SyntaxLanguage lang) const {
    switch (lang) {
        case SyntaxLanguage::Cpp:        return g_cppKeywords.count(word) > 0;
        case SyntaxLanguage::Python:     return g_pythonKeywords.count(word) > 0;
        case SyntaxLanguage::JavaScript: return g_jsKeywords.count(word) > 0;
        case SyntaxLanguage::PowerShell: {
            // PowerShell keywords are case-insensitive
            std::string lower = word;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            return g_psKeywords.count(lower) > 0;
        }
        case SyntaxLanguage::Assembly: {
            std::string lower = word;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            return g_asmKeywords.count(lower) > 0;
        }
        default: return false;
    }
}

bool Win32IDE::isBuiltinType(const std::string& word, SyntaxLanguage lang) const {
    switch (lang) {
        case SyntaxLanguage::Cpp:        return g_cppBuiltinTypes.count(word) > 0;
        case SyntaxLanguage::Python:     return g_pythonBuiltins.count(word) > 0;
        case SyntaxLanguage::JavaScript: return g_jsBuiltins.count(word) > 0;
        case SyntaxLanguage::PowerShell: return g_psBuiltins.count(word) > 0;
        case SyntaxLanguage::Assembly:   return g_asmRegisters.count(word) > 0;
        default: return false;
    }
}

// ============================================================================
// TOKEN COLOR MAP (VS Code Dark+ inspired)
// ============================================================================
COLORREF Win32IDE::getTokenColor(TokenType type) const {
    switch (type) {
        case TokenType::Keyword:      return RGB(86, 156, 214);    // Blue
        case TokenType::BuiltinType:  return RGB(78, 201, 176);    // Teal
        case TokenType::String:       return RGB(206, 145, 120);   // Orange-brown
        case TokenType::Comment:      return RGB(106, 153, 85);    // Green
        case TokenType::Number:       return RGB(181, 206, 168);   // Light green
        case TokenType::Preprocessor: return RGB(155, 89, 182);    // Purple
        case TokenType::Operator:     return RGB(212, 212, 212);   // Light grey
        case TokenType::Function:     return RGB(220, 220, 170);   // Yellow
        case TokenType::Bracket:      return RGB(255, 215, 0);     // Gold
        case TokenType::Default:
        default:                      return RGB(212, 212, 212);   // Default text
    }
}

// ============================================================================
// LINE TOKENIZER — produces SyntaxToken list for a single line
// ============================================================================
std::vector<Win32IDE::SyntaxToken> Win32IDE::tokenizeLine(
    const std::string& line, int lineStartOffset, SyntaxLanguage lang) 
{
    std::vector<SyntaxToken> tokens;
    int len = (int)line.size();
    int i = 0;
    
    while (i < len) {
        char ch = line[i];
        
        // ---- Whitespace: skip ----
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            i++;
            continue;
        }
        
        // ---- Single-line comments ----
        if (i + 1 < len) {
            // C/C++/JS style: //
            if ((lang == SyntaxLanguage::Cpp || lang == SyntaxLanguage::JavaScript) &&
                ch == '/' && line[i + 1] == '/') {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
                break; // rest of line is comment
            }
            // Python style: #
            if (lang == SyntaxLanguage::Python && ch == '#') {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
                break;
            }
            // PowerShell: #
            if (lang == SyntaxLanguage::PowerShell && ch == '#') {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
                break;
            }
            // Assembly: ; (NASM/MASM style)
            if (lang == SyntaxLanguage::Assembly && ch == ';') {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
                break;
            }
            // JSON comments (jsonc): //
            if (lang == SyntaxLanguage::JSON && ch == '/' && line[i + 1] == '/') {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Comment});
                break;
            }
        }
        
        // ---- Block comments (C/C++/JS) ----
        if ((lang == SyntaxLanguage::Cpp || lang == SyntaxLanguage::JavaScript) &&
            i + 1 < len && ch == '/' && line[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i + 1 < len && !(line[i] == '*' && line[i + 1] == '/')) i++;
            if (i + 1 < len) i += 2; else i = len; // consume */
            tokens.push_back({lineStartOffset + start, i - start, TokenType::Comment});
            continue;
        }
        
        // ---- Preprocessor (C/C++) ----
        if (lang == SyntaxLanguage::Cpp && ch == '#') {
            int start = i;
            // Read the directive word
            i++; // skip #
            while (i < len && (std::isalpha((unsigned char)line[i]) || line[i] == '_')) i++;
            tokens.push_back({lineStartOffset + start, i - start, TokenType::Preprocessor});
            // Rest of the preprocessor line is also preprocessor colored
            if (i < len) {
                tokens.push_back({lineStartOffset + i, len - i, TokenType::Preprocessor});
                i = len;
            }
            continue;
        }
        
        // ---- Strings ----
        if (ch == '"' || ch == '\'') {
            // Python triple-quote check
            if (lang == SyntaxLanguage::Python && i + 2 < len &&
                line[i + 1] == ch && line[i + 2] == ch) {
                int start = i;
                char tripleChar = ch;
                i += 3;
                while (i + 2 < len && !(line[i] == tripleChar && line[i + 1] == tripleChar && line[i + 2] == tripleChar)) i++;
                if (i + 2 < len) i += 3; else i = len;
                tokens.push_back({lineStartOffset + start, i - start, TokenType::String});
                continue;
            }
            
            char quote = ch;
            int start = i;
            i++; // skip opening quote
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i++; // skip escape
                i++;
            }
            if (i < len) i++; // skip closing quote
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
            // Hex: 0x...
            if (ch == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                i += 2;
                while (i < len && std::isxdigit((unsigned char)line[i])) i++;
            }
            // Binary: 0b...
            else if (ch == '0' && i + 1 < len && (line[i + 1] == 'b' || line[i + 1] == 'B')) {
                i += 2;
                while (i < len && (line[i] == '0' || line[i] == '1')) i++;
            }
            else {
                while (i < len && (std::isdigit((unsigned char)line[i]) || line[i] == '.')) i++;
                // Suffix: f, l, u, etc.
                if (i < len && (line[i] == 'f' || line[i] == 'F' || line[i] == 'l' ||
                                line[i] == 'L' || line[i] == 'u' || line[i] == 'U'))
                    i++;
                // Scientific notation
                if (i < len && (line[i] == 'e' || line[i] == 'E')) {
                    i++;
                    if (i < len && (line[i] == '+' || line[i] == '-')) i++;
                    while (i < len && std::isdigit((unsigned char)line[i])) i++;
                }
            }
            // Size suffix: LL, ULL, etc.
            while (i < len && (line[i] == 'l' || line[i] == 'L' || 
                               line[i] == 'u' || line[i] == 'U')) i++;
            tokens.push_back({lineStartOffset + start, i - start, TokenType::Number});
            continue;
        }
        
        // ---- Identifiers / Keywords ----
        if (std::isalpha((unsigned char)ch) || ch == '_' || 
            (lang == SyntaxLanguage::PowerShell && ch == '$')) {
            int start = i;
            i++;
            while (i < len && (std::isalnum((unsigned char)line[i]) || line[i] == '_' ||
                               line[i] == '-' || // PowerShell cmdlets use hyphens
                               (lang == SyntaxLanguage::PowerShell && line[i] == ':'))) {
                // Only allow hyphens in PowerShell cmdlet names (e.g., Get-Content)
                if (line[i] == '-' && lang != SyntaxLanguage::PowerShell) break;
                if (line[i] == ':' && lang != SyntaxLanguage::PowerShell) break;
                i++;
            }
            std::string word = line.substr(start, i - start);
            
            // Check for function call: identifier followed by (
            bool isFunc = false;
            int peekIdx = i;
            while (peekIdx < len && (line[peekIdx] == ' ' || line[peekIdx] == '\t')) peekIdx++;
            if (peekIdx < len && line[peekIdx] == '(') isFunc = true;
            
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
            ch == '|' || ch == '^' || ch == '~' || ch == '?' || ch == ':') {
            int start = i;
            i++;
            // Multi-char operators: ==, !=, <=, >=, &&, ||, <<, >>, ->, ::, =>
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
        
        // ---- JSON colons and commas ----
        if (lang == SyntaxLanguage::JSON && (ch == ':' || ch == ',')) {
            tokens.push_back({lineStartOffset + i, 1, TokenType::Operator});
            i++;
            continue;
        }
        
        // ---- Everything else: default color, advance ----
        tokens.push_back({lineStartOffset + i, 1, TokenType::Default});
        i++;
    }
    
    return tokens;
}

// ============================================================================
// DOCUMENT TOKENIZER — tokenizes the entire editor content
// ============================================================================
std::vector<Win32IDE::SyntaxToken> Win32IDE::tokenizeDocument(
    const std::string& text, SyntaxLanguage lang) 
{
    std::vector<SyntaxToken> allTokens;
    allTokens.reserve(text.size() / 4); // Rough heuristic
    
    int offset = 0;
    int len = (int)text.size();
    
    // Track multi-line comment state for C/C++/JS
    bool inBlockComment = false;
    
    while (offset < len) {
        // Find end of current line
        int lineEnd = offset;
        while (lineEnd < len && text[lineEnd] != '\n') lineEnd++;
        
        std::string line = text.substr(offset, lineEnd - offset);
        
        // Handle block comments spanning multiple lines
        if (inBlockComment) {
            size_t endComment = line.find("*/");
            if (endComment != std::string::npos) {
                // Block comment ends on this line
                int commentEnd = (int)(endComment + 2);
                allTokens.push_back({offset, commentEnd, TokenType::Comment});
                inBlockComment = false;
                // Tokenize rest of line after block comment
                if (commentEnd < (int)line.size()) {
                    std::string rest = line.substr(commentEnd);
                    auto restTokens = tokenizeLine(rest, offset + commentEnd, lang);
                    allTokens.insert(allTokens.end(), restTokens.begin(), restTokens.end());
                }
            } else {
                // Entire line is still in block comment
                allTokens.push_back({offset, (int)line.size(), TokenType::Comment});
            }
        } else {
            // Check if this line starts a block comment
            auto lineTokens = tokenizeLine(line, offset, lang);
            
            // Scan for unclosed block comment in the tokens
            for (const auto& tok : lineTokens) {
                allTokens.push_back(tok);
            }
            
            // Check for unclosed /* in this line (C/C++/JS only)
            if (lang == SyntaxLanguage::Cpp || lang == SyntaxLanguage::JavaScript) {
                size_t commentStart = std::string::npos;
                for (size_t ci = 0; ci < line.size(); ci++) {
                    if (line[ci] == '/' && ci + 1 < line.size() && line[ci + 1] == '*') {
                        commentStart = ci;
                    }
                    if (line[ci] == '*' && ci + 1 < line.size() && line[ci + 1] == '/') {
                        commentStart = std::string::npos; // Closed within the line
                        ci++; // Skip /
                    }
                }
                if (commentStart != std::string::npos) {
                    inBlockComment = true;
                }
            }
        }
        
        // Move to next line
        offset = lineEnd + 1; // Skip the \n
    }
    
    return allTokens;
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void Win32IDE::initSyntaxColorizer() {
    m_syntaxColoringEnabled = true;
    m_syntaxColoringPending = false;
    m_syntaxColorTimerId = 0;
    m_lastColoredVersion = -1;
    m_editorContentVersion = 0;
    m_currentSyntaxLanguage = SyntaxLanguage::None;
    
    // Detect language from current file
    if (!m_currentFile.empty()) {
        m_currentSyntaxLanguage = detectLanguageFromExtension(m_currentFile);
    }
    
    LOG_INFO("Syntax colorizer initialized. Language: " + std::to_string((int)m_currentSyntaxLanguage));
}

// ============================================================================
// APPLY SYNTAX COLORING — full document pass
// ============================================================================
void Win32IDE::applySyntaxColoring() {
    if (!m_syntaxColoringEnabled || !m_hwndEditor) return;
    if (m_currentSyntaxLanguage == SyntaxLanguage::None) return;
    if (m_editorContentVersion == m_lastColoredVersion) return; // No changes
    
    // Get editor text length
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0 || textLen > 500000) {
        // Skip coloring for empty or very large files (>500KB) to avoid UI stall
        return;
    }
    
    // Get the full text
    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);
    
    // Save current selection and scroll position
    CHARRANGE oldSel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    POINT scrollPos;
    SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
    
    // Prevent visual flicker — lock redraw
    SendMessage(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
    
    // Tokenize the entire document
    auto tokens = tokenizeDocument(text, m_currentSyntaxLanguage);
    m_cachedTokens = tokens;
    
    // Apply formatting to each token
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::Default) continue; // Skip default — already correct
        
        CHARRANGE cr;
        cr.cpMin = tok.start;
        cr.cpMax = tok.start + tok.length;
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
        
        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = getTokenColor(tok.type);
        cf.dwEffects = 0; // Clear CFE_AUTOCOLOR
        SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Restore selection and scroll
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    
    // Re-enable redraw
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    
    m_lastColoredVersion = m_editorContentVersion;
}

// ============================================================================
// RANGE-BASED COLORING — for incremental updates (visible area only)
// ============================================================================
void Win32IDE::applySyntaxColoringForRange(int startChar, int endChar) {
    if (!m_syntaxColoringEnabled || !m_hwndEditor) return;
    if (m_currentSyntaxLanguage == SyntaxLanguage::None) return;
    
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;
    
    // Clamp range
    if (startChar < 0) startChar = 0;
    if (endChar > textLen) endChar = textLen;
    if (startChar >= endChar) return;
    
    // Expand range to full lines
    int startLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, startChar, 0);
    int endLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, endChar, 0);
    
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, startLine, 0);
    int nextLineAfterEnd = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, endLine + 1, 0);
    if (nextLineAfterEnd < 0) nextLineAfterEnd = textLen;
    
    // Get text for this range
    int rangeLen = nextLineAfterEnd - lineStart;
    if (rangeLen <= 0) return;
    
    std::string rangeText(rangeLen + 1, '\0');
    // Use EM_GETLINE for each line in range or get full text and substr
    std::string fullText(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &fullText[0], textLen + 1);
    fullText.resize(textLen);
    rangeText = fullText.substr(lineStart, rangeLen);
    
    // Save state
    CHARRANGE oldSel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    POINT scrollPos;
    SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
    
    SendMessage(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
    
    // Tokenize just this range
    auto tokens = tokenizeLine(rangeText, lineStart, m_currentSyntaxLanguage);
    
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::Default) continue;
        
        CHARRANGE cr;
        cr.cpMin = tok.start;
        cr.cpMax = tok.start + tok.length;
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
        
        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = getTokenColor(tok.type);
        cf.dwEffects = 0;
        SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ============================================================================
// DEBOUNCED EN_CHANGE HANDLER
// ============================================================================
void Win32IDE::onEditorContentChanged() {
    if (!m_syntaxColoringEnabled) return;
    
    m_editorContentVersion++;
    m_syntaxColoringPending = true;
    
    // Kill existing timer and start a new one (debounce)
    if (m_syntaxColorTimerId) {
        KillTimer(m_hwndMain, SYNTAX_COLOR_TIMER_ID);
        m_syntaxColorTimerId = 0;
    }
    
    // Store 'this' in window property so the static timer proc can find us
    SetPropA(m_hwndMain, "SYNTAX_IDE_PTR", (HANDLE)this);
    
    m_syntaxColorTimerId = SetTimer(m_hwndMain, SYNTAX_COLOR_TIMER_ID, 
                                     SYNTAX_COLOR_DEBOUNCE_MS, SyntaxColorTimerProc);
}

// ============================================================================
// TIMER CALLBACK — fires after debounce delay
// ============================================================================
void CALLBACK Win32IDE::SyntaxColorTimerProc(HWND hwnd, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/) {
    if (idEvent != SYNTAX_COLOR_TIMER_ID) return;
    
    // Kill the timer — one-shot behavior
    KillTimer(hwnd, SYNTAX_COLOR_TIMER_ID);
    
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "SYNTAX_IDE_PTR");
    if (!ide) return;
    
    ide->m_syntaxColorTimerId = 0;
    ide->m_syntaxColoringPending = false;
    
    // Apply coloring on the UI thread (we're already on it — this is a timer callback)
    ide->applySyntaxColoring();
}
