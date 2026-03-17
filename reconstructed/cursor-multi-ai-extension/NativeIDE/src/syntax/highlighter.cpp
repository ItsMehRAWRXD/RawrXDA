#include "syntax_highlighter.h"
#include <algorithm>

SyntaxHighlighter::SyntaxHighlighter() : m_currentLanguage("text") {
    SetupGenericRules();
}

SyntaxHighlighter::~SyntaxHighlighter() = default;

void SyntaxHighlighter::SetLanguage(const std::string& language) {
    if (m_currentLanguage == language) {
        return;
    }
    
    m_currentLanguage = language;
    ClearRules();
    
    if (language == "cpp" || language == "c++") {
        SetupCppRules();
    } else if (language == "c") {
        SetupCRules();
    } else if (language == "assembly" || language == "asm") {
        SetupAssemblyRules();
    } else if (language == "python") {
        SetupPythonRules();
    } else if (language == "javascript" || language == "js") {
        SetupJavaScriptRules();
    } else {
        SetupGenericRules();
    }
}

std::vector<std::string> SyntaxHighlighter::GetSupportedLanguages() const {
    return {"text", "cpp", "c", "assembly", "python", "javascript"};
}

std::vector<SyntaxToken> SyntaxHighlighter::HighlightLine(const std::wstring& line) const {
    std::vector<SyntaxToken> tokens;
    
    if (line.empty()) {
        return tokens;
    }
    
    // Create a boolean array to track which characters have been matched
    std::vector<bool> matched(line.length(), false);
    
    // Apply rules in order of priority
    for (const auto& rule : m_rules) {
        std::wsregex_iterator begin(line.begin(), line.end(), rule.pattern);
        std::wsregex_iterator end;
        
        for (auto iter = begin; iter != end; ++iter) {
            const std::wsmatch& match = *iter;
            size_t start = match.position();
            size_t length = match.length();
            
            // Check if this range is already matched
            bool alreadyMatched = false;
            for (size_t i = start; i < start + length && i < matched.size(); ++i) {
                if (matched[i]) {
                    alreadyMatched = true;
                    break;
                }
            }
            
            if (!alreadyMatched) {
                // Mark this range as matched
                for (size_t i = start; i < start + length && i < matched.size(); ++i) {
                    matched[i] = true;
                }
                
                tokens.emplace_back(rule.tokenType, rule.color, start, length);
            }
        }
    }
    
    // Sort tokens by position
    std::sort(tokens.begin(), tokens.end(), [](const SyntaxToken& a, const SyntaxToken& b) {
        return a.start < b.start;
    });
    
    return tokens;
}

std::vector<uint32_t> SyntaxHighlighter::GetLineColors(const std::wstring& line) const {
    std::vector<uint32_t> colors(line.length(), COLOR_IDENTIFIER);
    
    auto tokens = HighlightLine(line);
    
    for (const auto& token : tokens) {
        size_t end = std::min(token.start + token.length, colors.size());
        for (size_t i = token.start; i < end; ++i) {
            colors[i] = token.color;
        }
    }
    
    return colors;
}

void SyntaxHighlighter::SetTokenColor(SyntaxToken::Type tokenType, uint32_t color) {
    // Update existing rules
    for (auto& rule : m_rules) {
        if (rule.tokenType == tokenType) {
            rule.color = color;
        }
    }
}

uint32_t SyntaxHighlighter::GetTokenColor(SyntaxToken::Type tokenType) const {
    switch (tokenType) {
        case SyntaxToken::Keyword: return COLOR_KEYWORD;
        case SyntaxToken::Type: return COLOR_TYPE;
        case SyntaxToken::String: return COLOR_STRING;
        case SyntaxToken::Character: return COLOR_CHARACTER;
        case SyntaxToken::Number: return COLOR_NUMBER;
        case SyntaxToken::Comment: return COLOR_COMMENT;
        case SyntaxToken::Operator: return COLOR_OPERATOR;
        case SyntaxToken::Preprocessor: return COLOR_PREPROCESSOR;
        case SyntaxToken::Punctuation: return COLOR_PUNCTUATION;
        default: return COLOR_IDENTIFIER;
    }
}

void SyntaxHighlighter::AddRule(const std::wstring& pattern, SyntaxToken::Type tokenType, uint32_t color, const std::wstring& name) {
    try {
        m_rules.emplace_back(pattern, tokenType, color, name);
    } catch (const std::regex_error&) {
        // Invalid regex pattern, ignore
    }
}

void SyntaxHighlighter::ClearRules() {
    m_rules.clear();
}

void SyntaxHighlighter::SetupCppRules() {
    // C++ keywords
    std::vector<std::wstring> keywords = {
        L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"auto",
        L"bitand", L"bitor", L"bool", L"break", L"case", L"catch",
        L"char", L"char8_t", L"char16_t", L"char32_t", L"class", L"compl",
        L"concept", L"const", L"consteval", L"constexpr", L"constinit", L"const_cast",
        L"continue", L"co_await", L"co_return", L"co_yield", L"decltype", L"default",
        L"delete", L"do", L"double", L"dynamic_cast", L"else", L"enum",
        L"explicit", L"export", L"extern", L"false", L"float", L"for",
        L"friend", L"goto", L"if", L"inline", L"int", L"long",
        L"mutable", L"namespace", L"new", L"noexcept", L"not", L"not_eq",
        L"nullptr", L"operator", L"or", L"or_eq", L"private", L"protected",
        L"public", L"register", L"reinterpret_cast", L"requires", L"return", L"short",
        L"signed", L"sizeof", L"static", L"static_assert", L"static_cast", L"struct",
        L"switch", L"template", L"this", L"thread_local", L"throw", L"true",
        L"try", L"typedef", L"typeid", L"typename", L"union", L"unsigned",
        L"using", L"virtual", L"void", L"volatile", L"wchar_t", L"while",
        L"xor", L"xor_eq"
    };
    AddKeywordRule(keywords, COLOR_KEYWORD);
    
    // C++ standard types
    std::vector<std::wstring> types = {
        L"int8_t", L"int16_t", L"int32_t", L"int64_t",
        L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
        L"size_t", L"ssize_t", L"ptrdiff_t", L"intptr_t", L"uintptr_t",
        L"std::string", L"std::wstring", L"std::vector", L"std::map",
        L"std::set", L"std::unordered_map", L"std::unordered_set",
        L"std::shared_ptr", L"std::unique_ptr", L"std::weak_ptr"
    };
    AddTypeRule(types, COLOR_TYPE);
    
    // Comments
    AddRule(L"//.*$", SyntaxToken::Comment, COLOR_COMMENT, L"line_comment");
    AddRule(L"/\\*[\\s\\S]*?\\*/", SyntaxToken::Comment, COLOR_COMMENT, L"block_comment");
    
    // Strings
    AddRule(L"\"(?:[^\"\\\\]|\\\\.)*\"", SyntaxToken::String, COLOR_STRING, L"string");
    AddRule(L"'(?:[^'\\\\]|\\\\.)*'", SyntaxToken::Character, COLOR_CHARACTER, L"character");
    
    // Raw strings
    AddRule(L"R\"\\([\\s\\S]*?\\)\"", SyntaxToken::String, COLOR_STRING, L"raw_string");
    
    // Numbers
    AddRule(L"\\b\\d+\\.\\d*(?:[eE][+-]?\\d+)?[fFlL]?\\b", SyntaxToken::Number, COLOR_NUMBER, L"float");
    AddRule(L"\\b\\d+(?:[eE][+-]?\\d+)?[fFlL]?\\b", SyntaxToken::Number, COLOR_NUMBER, L"number");
    AddRule(L"\\b0[xX][0-9a-fA-F]+[uUlL]*\\b", SyntaxToken::Number, COLOR_NUMBER, L"hex");
    AddRule(L"\\b0[bB][01]+[uUlL]*\\b", SyntaxToken::Number, COLOR_NUMBER, L"binary");
    AddRule(L"\\b0[0-7]+[uUlL]*\\b", SyntaxToken::Number, COLOR_NUMBER, L"octal");
    
    // Preprocessor
    AddRule(L"^\\s*#.*$", SyntaxToken::Preprocessor, COLOR_PREPROCESSOR, L"preprocessor");
    
    // Operators
    AddRule(L"[+\\-*/%=!<>&|^~?:.,;(){}\\[\\]]", SyntaxToken::Operator, COLOR_OPERATOR, L"operators");
}

void SyntaxHighlighter::SetupCRules() {
    // C keywords (subset of C++)
    std::vector<std::wstring> keywords = {
        L"auto", L"break", L"case", L"char", L"const", L"continue",
        L"default", L"do", L"double", L"else", L"enum", L"extern",
        L"float", L"for", L"goto", L"if", L"int", L"long",
        L"register", L"return", L"short", L"signed", L"sizeof", L"static",
        L"struct", L"switch", L"typedef", L"union", L"unsigned", L"void",
        L"volatile", L"while", L"_Bool", L"_Complex", L"_Imaginary",
        L"inline", L"restrict", L"_Alignas", L"_Alignof", L"_Atomic",
        L"_Static_assert", L"_Noreturn", L"_Thread_local", L"_Generic"
    };
    AddKeywordRule(keywords, COLOR_KEYWORD);
    
    // C standard types
    std::vector<std::wstring> types = {
        L"size_t", L"ssize_t", L"ptrdiff_t", L"wchar_t",
        L"int8_t", L"int16_t", L"int32_t", L"int64_t",
        L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
        L"intptr_t", L"uintptr_t", L"FILE"
    };
    AddTypeRule(types, COLOR_TYPE);
    
    // Use same rules as C++ for comments, strings, numbers, etc.
    AddRule(L"//.*$", SyntaxToken::Comment, COLOR_COMMENT, L"line_comment");
    AddRule(L"/\\*[\\s\\S]*?\\*/", SyntaxToken::Comment, COLOR_COMMENT, L"block_comment");
    AddRule(L"\"(?:[^\"\\\\]|\\\\.)*\"", SyntaxToken::String, COLOR_STRING, L"string");
    AddRule(L"'(?:[^'\\\\]|\\\\.)*'", SyntaxToken::Character, COLOR_CHARACTER, L"character");
    AddRule(L"\\b\\d+\\.\\d*(?:[eE][+-]?\\d+)?[fFlL]?\\b", SyntaxToken::Number, COLOR_NUMBER, L"float");
    AddRule(L"\\b\\d+(?:[eE][+-]?\\d+)?[fFlL]?\\b", SyntaxToken::Number, COLOR_NUMBER, L"number");
    AddRule(L"\\b0[xX][0-9a-fA-F]+[uUlL]*\\b", SyntaxToken::Number, COLOR_NUMBER, L"hex");
    AddRule(L"^\\s*#.*$", SyntaxToken::Preprocessor, COLOR_PREPROCESSOR, L"preprocessor");
    AddRule(L"[+\\-*/%=!<>&|^~?:.,;(){}\\[\\]]", SyntaxToken::Operator, COLOR_OPERATOR, L"operators");
}

void SyntaxHighlighter::SetupAssemblyRules() {
    // x86/x64 Assembly instructions
    std::vector<std::wstring> instructions = {
        L"mov", L"add", L"sub", L"mul", L"div", L"inc", L"dec",
        L"cmp", L"test", L"jmp", L"je", L"jne", L"jz", L"jnz",
        L"jg", L"jl", L"jge", L"jle", L"ja", L"jb", L"jae", L"jbe",
        L"call", L"ret", L"push", L"pop", L"lea", L"int", L"nop",
        L"and", L"or", L"xor", L"not", L"shl", L"shr", L"sal", L"sar",
        L"loop", L"loopz", L"loopnz", L"rep", L"repe", L"repne"
    };
    AddKeywordRule(instructions, COLOR_KEYWORD);
    
    // Registers
    std::vector<std::wstring> registers = {
        L"eax", L"ebx", L"ecx", L"edx", L"esi", L"edi", L"esp", L"ebp",
        L"rax", L"rbx", L"rcx", L"rdx", L"rsi", L"rdi", L"rsp", L"rbp",
        L"r8", L"r9", L"r10", L"r11", L"r12", L"r13", L"r14", L"r15",
        L"ax", L"bx", L"cx", L"dx", L"si", L"di", L"sp", L"bp",
        L"al", L"bl", L"cl", L"dl", L"ah", L"bh", L"ch", L"dh"
    };
    AddTypeRule(registers, COLOR_TYPE);
    
    // Comments
    AddRule(L";.*$", SyntaxToken::Comment, COLOR_COMMENT, L"comment");
    
    // Numbers
    AddRule(L"\\b\\d+[hH]\\b", SyntaxToken::Number, COLOR_NUMBER, L"hex_number");
    AddRule(L"\\b0[xX][0-9a-fA-F]+\\b", SyntaxToken::Number, COLOR_NUMBER, L"hex");
    AddRule(L"\\b\\d+\\b", SyntaxToken::Number, COLOR_NUMBER, L"decimal");
    
    // Labels
    AddRule(L"^\\s*\\w+:", SyntaxToken::Identifier, COLOR_IDENTIFIER, L"label");
    
    // Directives
    AddRule(L"\\.[a-zA-Z_]\\w*", SyntaxToken::Preprocessor, COLOR_PREPROCESSOR, L"directive");
}

void SyntaxHighlighter::SetupGenericRules() {
    // Basic string recognition
    AddRule(L"\"(?:[^\"\\\\]|\\\\.)*\"", SyntaxToken::String, COLOR_STRING, L"string");
    AddRule(L"'(?:[^'\\\\]|\\\\.)*'", SyntaxToken::Character, COLOR_CHARACTER, L"character");
    
    // Basic number recognition
    AddRule(L"\\b\\d+\\.\\d*\\b", SyntaxToken::Number, COLOR_NUMBER, L"float");
    AddRule(L"\\b\\d+\\b", SyntaxToken::Number, COLOR_NUMBER, L"number");
}

void SyntaxHighlighter::AddKeywordRule(const std::vector<std::wstring>& keywords, uint32_t color) {
    for (const auto& keyword : keywords) {
        std::wstring pattern = CreateWordBoundaryPattern(keyword);
        AddRule(pattern, SyntaxToken::Keyword, color, keyword);
    }
}

void SyntaxHighlighter::AddTypeRule(const std::vector<std::wstring>& types, uint32_t color) {
    for (const auto& type : types) {
        std::wstring pattern = CreateWordBoundaryPattern(type);
        AddRule(pattern, SyntaxToken::Type, color, type);
    }
}

std::wstring SyntaxHighlighter::EscapeRegex(const std::wstring& str) {
    std::wstring escaped;
    for (wchar_t ch : str) {
        if (ch == L'\\' || ch == L'^' || ch == L'$' || ch == L'.' || ch == L'|' ||
            ch == L'?' || ch == L'*' || ch == L'+' || ch == L'(' || ch == L')' ||
            ch == L'[' || ch == L']' || ch == L'{' || ch == L'}') {
            escaped += L'\\';
        }
        escaped += ch;
    }
    return escaped;
}

std::wstring SyntaxHighlighter::CreateWordBoundaryPattern(const std::wstring& word) {
    return L"\\b" + EscapeRegex(word) + L"\\b";
}