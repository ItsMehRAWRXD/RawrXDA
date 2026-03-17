#pragma once
#include "RawrXD_Win32_Foundation.h"
#include "KeywordHashTable.h"
#include <vector>
#include <unordered_set>

namespace RawrXD {

enum class TokenType {
    Default,
    Keyword,
    Instruction, // For MASM
    Register,    // For MASM
    Number,
    String,
    Comment,
    Operator,
    Preprocessor,
    Label,
    Directive,
    Type,
    Function,
    Variable
};

struct Token {
    TokenType type;
    int start;  // Relative to start of text chunk/line
    int length;
};

class Lexer {
public:
    virtual ~Lexer() = default;
    
    // Lex a single line or chunk of text logic
    // We assume line-based lexing for now for simplicity in the editor
    virtual void lex(const std::wstring& text, std::vector<Token>& outTokens) = 0;
    
    // For stateful lexing (multiline comments), we would need state input/output
    virtual int lexStateful(const std::wstring& text, int startState, std::vector<Token>& outTokens) {
        lex(text, outTokens);
        return 0; // Default state
    }
};

class CppLexer : public Lexer {
public:
    CppLexer();
    void lex(const std::wstring& text, std::vector<Token>& outTokens) override;
private:
    void processToken(const std::wstring& text, int& i, int n, std::vector<Token>& outTokens);
    Token Lexer_TokenizeString(const std::wstring& text, int& i, int n);
    Token Lexer_TokenizeNumber(const std::wstring& text, int& i, int n);
    Token Lexer_TokenizeIdentifier(const std::wstring& text, int& i, int n);
    Token Lexer_TokenizeOperator(const std::wstring& text, int& i, int n);
    KeywordHashTable keywordTable;
};

} // namespace RawrXD
