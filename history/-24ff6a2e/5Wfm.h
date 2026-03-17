#pragma once

#include "native_ide.h"

struct SyntaxToken {
    enum Type {
        None,
        Keyword,
        TypeKeyword,
        String,
        Character,
        Number,
        Comment,
        Operator,
        Preprocessor,
        Identifier,
        Punctuation,
        Whitespace
    };
    
    Type type = None;
    uint32_t color = 0x000000;  // RGB color
    size_t start = 0;
    size_t length = 0;
    
    SyntaxToken() = default;
    SyntaxToken(Type t, uint32_t c, size_t s, size_t l) : type(t), color(c), start(s), length(l) {}
};

struct SyntaxRule {
    std::wregex pattern;
    SyntaxToken::Type tokenType;
    uint32_t color;
    std::wstring name;
    
    SyntaxRule(const std::wstring& regex, SyntaxToken::Type type, uint32_t c, const std::wstring& n)
        : pattern(regex), tokenType(type), color(c), name(n) {}
};

class SyntaxHighlighter {
private:
    std::string m_currentLanguage;
    std::vector<SyntaxRule> m_rules;
    
    // Default color scheme
    static constexpr uint32_t COLOR_KEYWORD = 0x569CD6;      // Blue
    static constexpr uint32_t COLOR_TYPE = 0x4EC9B0;         // Teal
    static constexpr uint32_t COLOR_STRING = 0xD69D85;       // Light brown
    static constexpr uint32_t COLOR_CHARACTER = 0xD69D85;    // Light brown
    static constexpr uint32_t COLOR_NUMBER = 0xB5CEA8;       // Light green
    static constexpr uint32_t COLOR_COMMENT = 0x57A64A;      // Green
    static constexpr uint32_t COLOR_OPERATOR = 0xB4B4B4;     // Light gray
    static constexpr uint32_t COLOR_PREPROCESSOR = 0x9B9B9B; // Gray
    static constexpr uint32_t COLOR_IDENTIFIER = 0xDCDCDC;   // Light gray
    static constexpr uint32_t COLOR_PUNCTUATION = 0xDCDCDC;  // Light gray
    
public:
    SyntaxHighlighter();
    ~SyntaxHighlighter();
    
    // Language support
    void SetLanguage(const std::string& language);
    std::string GetLanguage() const { return m_currentLanguage; }
    std::vector<std::string> GetSupportedLanguages() const;
    
    // Highlighting
    std::vector<SyntaxToken> HighlightLine(const std::wstring& line) const;
    std::vector<uint32_t> GetLineColors(const std::wstring& line) const;
    
    // Color scheme
    void SetTokenColor(SyntaxToken::Type tokenType, uint32_t color);
    uint32_t GetTokenColor(SyntaxToken::Type tokenType) const;
    
    // Custom rules
    void AddRule(const std::wstring& pattern, SyntaxToken::Type tokenType, uint32_t color, const std::wstring& name = L"");
    void ClearRules();
    
private:
    void SetupCppRules();
    void SetupCRules();
    void SetupAssemblyRules();
    void SetupPythonRules();
    void SetupJavaScriptRules();
    void SetupGenericRules();
    
    void AddKeywordRule(const std::vector<std::wstring>& keywords, uint32_t color);
    void AddTypeRule(const std::vector<std::wstring>& types, uint32_t color);
    
    static std::wstring EscapeRegex(const std::wstring& str);
    static std::wstring CreateWordBoundaryPattern(const std::wstring& word);
};