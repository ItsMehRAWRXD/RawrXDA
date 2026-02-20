#pragma once
#include "RawrXD_Lexer.h"
#include "KeywordHashTable.h"
#include <unordered_set>

namespace RawrXD {

class MASMLexer : public Lexer {
    std::unordered_set<std::wstring> instructions;
    std::unordered_set<std::wstring> registers;
    std::unordered_set<std::wstring> directives;
    
public:
    MASMLexer();
    void lex(const std::wstring& text, std::vector<Token>& outTokens) override;
    
private:
    bool isInstruction(const std::wstring& s) const;
    bool isRegister(const std::wstring& s) const;
    bool isDirective(const std::wstring& s) const;
    void processStringToken(const std::wstring& text, int& i, int len, std::vector<Token>& outTokens);
    void processNumberToken(const std::wstring& text, int& i, int len, std::vector<Token>& outTokens);
    void processIdentifierToken(const std::wstring& text, int& i, int len, std::vector<Token>& outTokens);
};

} // namespace RawrXD
