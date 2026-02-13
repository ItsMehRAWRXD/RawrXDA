#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct SyntaxToken {
    std::size_t start{0};
    std::size_t length{0};
    int type{0};
};

class LanguagePluginBase {
public:
    virtual ~LanguagePluginBase() = default;
    virtual void lex(std::string_view text, std::vector<SyntaxToken>& out) = 0;
};

class GenericLanguagePlugin : public LanguagePluginBase {
public:
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;
};

class CppLanguagePlugin : public LanguagePluginBase {
public:
    CppLanguagePlugin();
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;

private:
    std::unordered_set<std::string> m_keywords;
};

class PowerShellLanguagePlugin : public LanguagePluginBase {
public:
    PowerShellLanguagePlugin();
    void lex(std::string_view text, std::vector<SyntaxToken>& out) override;

private:
    std::unordered_set<std::string> m_keywords;
};

class SyntaxEngine {
public:
    SyntaxEngine();
    void setLanguage(LanguagePluginBase* lang);
    void tokenize(std::string_view text, std::vector<SyntaxToken>& outTokens);

private:
    LanguagePluginBase* m_lang{nullptr};
    GenericLanguagePlugin m_fallback;
};
