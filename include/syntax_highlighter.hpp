#ifndef RAWRXD_SYNTAX_HIGHLIGHTER_HPP
#define RAWRXD_SYNTAX_HIGHLIGHTER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <algorithm>

namespace RawrXD {

enum class TokenType { DEFAULT, KEYWORD, IDENTIFIER, STRING, NUMBER, COMMENT, OPERATOR, PREPROCESSOR, TYPE, FUNCTION };

struct Token { TokenType type; size_t start; size_t length; };

struct HighlightRule { TokenType type; std::regex pattern; int priority; };

class SyntaxHighlighter {
public:
    SyntaxHighlighter();
    void SetLanguage(const std::string& language);
    std::vector<Token> HighlightLine(const std::string& line);
    static std::string GetColorForToken(TokenType type, bool darkTheme = true);
    static std::string DetectLanguage(const std::string& filename);
private:
    void InitializeCPPRules();
    void InitializeCRules();
    void InitializeRustRules();
    void InitializePythonRules();
    void InitializeAsmRules();
    void InitializeJavaScriptRules();
    void AddKeywords(const char** keywords, size_t count, TokenType type);
    std::string currentLanguage_;
    std::vector<HighlightRule> rules_;
    std::unordered_map<std::string, TokenType> keywords_;
    std::unordered_set<std::string> types_;
};

} // namespace RawrXD
#endif // RAWRXD_SYNTAX_HIGHLIGHTER_HPP