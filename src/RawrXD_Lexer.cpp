#include "RawrXD_Lexer.h"
#include <cwctype>

namespace RawrXD {

CppLexer::CppLexer() {
    keywords = {
        L"auto", L"break", L"case", L"char", L"const", L"continue", L"default", L"do",
        L"double", L"else", L"enum", L"extern", L"float", L"for", L"goto", L"if",
        L"int", L"long", L"register", L"return", L"short", L"signed", L"sizeof", L"static",
        L"struct", L"switch", L"typedef", L"union", L"unsigned", L"void", L"volatile", L"while",
        L"class", L"namespace", L"new", L"delete", L"this", L"true", L"false", L"public",
        L"protected", L"private", L"virtual", L"friend", L"template", L"typename", L"using"
    };
}

void CppLexer::lex(const std::wstring& text, std::vector<Token>& outTokens) {
    outTokens.clear();
    if (text.empty()) return;

    int n = (int)text.length();
    int i = 0;

    while (i < n) {
        wchar_t c = text[i];

        // Whitespace
        if (iswspace(c)) {
            i++;
            continue;
        }

        // Comment
        if (c == L'/') {
            if (i + 1 < n && text[i+1] == L'/') {
                // Line comment
                outTokens.push_back({TokenType::Comment, i, n - i});
                break; 
            }
            if (i + 1 < n && text[i+1] == L'*') {
                // Block comment (simple scan)
                int start = i;
                i += 2;
                while (i + 1 < n && !(text[i] == L'*' && text[i+1] == L'/')) i++;
                if (i + 1 < n) i += 2;
                outTokens.push_back({TokenType::Comment, start, i - start});
                continue;
            }
        }

        // String
        if (c == L'"' || c == L'\'') {
            int start = i;
            wchar_t quote = c;
            i++;
            while (i < n) {
                if (text[i] == quote && text[i-1] != L'\\') {
                    i++;
                    break;
                }
                i++;
            }
            outTokens.push_back({TokenType::String, start, i - start});
            continue;
        }

        // Number
        if (iswdigit(c)) {
            int start = i;
            while (i < n && (iswdigit(text[i]) || text[i] == L'.' || text[i] == L'x' || (text[i] >= L'a' && text[i] <= L'f'))) i++;
            outTokens.push_back({TokenType::Number, start, i - start});
            continue;
        }

        // Identifier / Keyword
        if (iswalpha(c) || c == L'_') {
            int start = i;
            while (i < n && (iswalnum(text[i]) || text[i] == L'_')) i++;
            int len = i - start;
            std::wstring word = text.substr(start, len);
            
            if (keywords.count(word)) {
                outTokens.push_back({TokenType::Keyword, start, len});
            } else {
                outTokens.push_back({TokenType::Default, start, len});
            }
            continue;
        }

        // Operator / Punctuation
        outTokens.push_back({TokenType::Operator, i, 1});
        i++;
    }
}

} // namespace RawrXD
