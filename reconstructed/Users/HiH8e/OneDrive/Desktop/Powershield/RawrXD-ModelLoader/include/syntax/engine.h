#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <unordered_set>

struct SyntaxToken {
    unsigned start{};
    unsigned length{};
    unsigned type{}; // 0 generic,1 number,2 identifier,3 keyword,4 string,5 comment
};

class LanguagePluginBase {
public:
    virtual ~LanguagePluginBase() = default;
    virtual std::string name() const = 0;
    virtual void lex(std::string_view text, std::vector<SyntaxToken>& out) = 0;
};

class GenericLanguagePlugin : public LanguagePluginBase {
public:
    std::string name() const override { return "generic"; }
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;
};

class CppLanguagePlugin : public LanguagePluginBase {
public:
    CppLanguagePlugin();
    std::string name() const override { return "cpp"; }
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;
private:
    std::unordered_set<std::string> m_keywords;
};

class PowerShellLanguagePlugin : public LanguagePluginBase {
public:
    PowerShellLanguagePlugin();
    std::string name() const override { return "powershell"; }
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;
private:
    std::unordered_set<std::string> m_keywords;
};

class SyntaxEngine {
public:
    SyntaxEngine();
    void setLanguage(LanguagePluginBase* lang); // no ownership
    void tokenize(std::string_view text, std::vector<SyntaxToken>& outTokens);
private:
    LanguagePluginBase* m_lang{};
    GenericLanguagePlugin m_fallback;
};
