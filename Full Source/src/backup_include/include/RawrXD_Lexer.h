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
    // States: 0 = normal, 1 = inside block comment
    virtual int lexStateful(const std::wstring& text, int startState, std::vector<Token>& outTokens) {
        if (startState == 1) {
            // Continue inside a block comment from previous line
            int i = 0;
            int n = static_cast<int>(text.size());
            while (i + 1 < n) {
                if (text[i] == L'*' && text[i+1] == L'/') {
                    outTokens.push_back({TokenType::Comment, 0, i + 2});
                    i += 2;
                    // Lex the rest normally
                    if (i < n) {
                        std::wstring rest = text.substr(i);
                        std::vector<Token> restTokens;
                        lex(rest, restTokens);
                        for (auto& t : restTokens) {
                            t.start += i;
                            outTokens.push_back(t);
                        }
                    }
                    return 0; // back to normal
                }
                i++;
            }
            // Entire line is still in block comment
            outTokens.push_back({TokenType::Comment, 0, n});
            return 1; // still in block comment
        }
        // Normal: lex and check if we end inside a block comment
        lex(text, outTokens);
        // Check if last token is an unterminated block comment
        // Look for /* without matching */
        int state = 0;
        int n = static_cast<int>(text.size());
        for (int i = 0; i < n - 1; ++i) {
            if (state == 0 && text[i] == L'/' && text[i+1] == L'*') { state = 1; i++; }
            else if (state == 1 && text[i] == L'*' && text[i+1] == L'/') { state = 0; i++; }
        }
        return state;
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
