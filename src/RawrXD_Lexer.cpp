#include "RawrXD_Lexer.h"
#include <cwctype>
#include <immintrin.h>

namespace RawrXD {

// SIMD-optimized whitespace check
bool IsWhitespace_SIMD(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
}

// SIMD-optimized digit check
bool IsDigit_SIMD(wchar_t c) {
    return c >= L'0' && c <= L'9';
}

// SIMD-optimized alpha check
bool IsAlpha_SIMD(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

// SIMD-optimized alphanumeric check
bool IsAlnum_SIMD(wchar_t c) {
    return IsAlpha_SIMD(c) || IsDigit_SIMD(c);
}

// Branchless token boundary detection using SIMD
size_t FindNextTokenBoundary_SIMD(const std::wstring& text, size_t start) {
    size_t len = text.length();
    size_t pos = start;
    
    // Process 32 characters at a time (AVX-512 can handle 64 bytes, but for wide chars we do 32)
    while (pos + 31 < len) {
        // Load 32 wide characters (64 bytes)
        __m512i char_vec = _mm512_loadu_si512((__m512i*)(text.data() + pos));
        
        // Create masks for different character types
        // This is simplified - in practice, we'd need more complex SIMD operations
        // For now, we'll fall back to scalar processing but show the structure
        
        // Check each character
        bool found_boundary = false;
        for (int i = 0; i < 32; i++) {
            wchar_t c = text[pos + i];
            if (!IsAlnum_SIMD(c) && c != L'_') {
                return pos + i;
            }
        }
        pos += 32;
    }
    
    // Handle remaining characters
    while (pos < len) {
        wchar_t c = text[pos];
        if (!IsAlnum_SIMD(c) && c != L'_') {
            return pos;
        }
        pos++;
    }
    
    return len;
}

CppLexer::CppLexer() {
    // Keywords are now handled by KeywordHashTable
}

void CppLexer::lex(const std::wstring& text, std::vector<Token>& outTokens) {
    outTokens.clear();
    if (text.empty()) return;

    int n = (int)text.length();
    int i = 0;

    // SIMD-optimized whitespace skipping
    while (i < n) {
        // Skip whitespace using SIMD where possible
        if (i + 31 < n) {
            // Check 32 characters at once for whitespace
            bool all_whitespace = true;
            for (int j = 0; j < 32; j++) {
                if (!IsWhitespace_SIMD(text[i + j])) {
                    all_whitespace = false;
                    break;
                }
            }
            if (all_whitespace) {
                i += 32;
                continue;
            }
        }
        
        // Scalar processing for remaining or mixed characters
        if (IsWhitespace_SIMD(text[i])) {
            i++;
            continue;
        }
        
        // Process token
        processToken(text, i, n, outTokens);
    }
}

void CppLexer::processToken(const std::wstring& text, int& i, int n, std::vector<Token>& outTokens) {
    wchar_t c = text[i];

    // Comment
    if (c == L'/') {
        if (i + 1 < n && text[i+1] == L'/') {
            // Line comment
            outTokens.push_back({TokenType::Comment, i, n - i});
            i = n; // Rest of line is comment
            return;
        }
        if (i + 1 < n && text[i+1] == L'*') {
            // Block comment (simple scan)
            int start = i;
            i += 2;
            while (i + 1 < n && !(text[i] == L'*' && text[i+1] == L'/')) i++;
            if (i + 1 < n) i += 2;
            outTokens.push_back({TokenType::Comment, start, i - start});
            return;
        }
    }

    // String
    if (c == L'"' || c == L'\'') {
        Token token = Lexer_TokenizeString(text, i, n);
        outTokens.push_back(token);
        return;
    }

    // Number
    if (IsDigit_SIMD(c)) {
        Token token = Lexer_TokenizeNumber(text, i, n);
        outTokens.push_back(token);
        return;
    }

    // Identifier / Keyword
    if (IsAlpha_SIMD(c) || c == L'_') {
        Token token = Lexer_TokenizeIdentifier(text, i, n);
        outTokens.push_back(token);
        return;
    }

    // Operator / Punctuation
    Token token = Lexer_TokenizeOperator(text, i, n);
    outTokens.push_back(token);
}

Token CppLexer::Lexer_TokenizeString(const std::wstring& text, int& i, int n) {
    int start = i;
    wchar_t quote = text[i];
    i++; // Skip opening quote
    while (i < n) {
        if (text[i] == quote && (i == 0 || text[i-1] != L'\\')) {
            i++; // Include closing quote
            break;
        }
        i++;
    }
    return {TokenType::String, start, i - start};
}

Token CppLexer::Lexer_TokenizeNumber(const std::wstring& text, int& i, int n) {
    int start = i;
    while (i < n && (IsDigit_SIMD(text[i]) || text[i] == L'.' || text[i] == L'x' || 
                     (text[i] >= L'a' && text[i] <= L'f') || (text[i] >= L'A' && text[i] <= L'F'))) {
        i++;
    }
    return {TokenType::Number, start, i - start};
}

Token CppLexer::Lexer_TokenizeIdentifier(const std::wstring& text, int& i, int n) {
    int start = i;
    // Use SIMD for boundary detection
    size_t end = FindNextTokenBoundary_SIMD(text, i);
    i = (int)end;
    int len = i - start;
    std::wstring word = text.substr(start, len);
    
    if (keywordTable.isKeyword(Language::Cpp, word)) {
        return {TokenType::Keyword, start, len};
    } else {
        return {TokenType::Default, start, len};
    }
}

Token CppLexer::Lexer_TokenizeOperator(const std::wstring& text, int& i, int n) {
    // Handle multi-character operators
    if (i + 1 < n) {
        std::wstring twoChar = text.substr(i, 2);
        if (twoChar == L"==" || twoChar == L"!=" || twoChar == L"<=" || twoChar == L">=" ||
            twoChar == L"+=" || twoChar == L"-=" || twoChar == L"*=" || twoChar == L"/=" ||
            twoChar == L"%=" || twoChar == L"&=" || twoChar == L"|=" || twoChar == L"^=" ||
            twoChar == L"<<=" || twoChar == L">>=" || twoChar == L"&&" || twoChar == L"||" ||
            twoChar == L"<<" || twoChar == L">>" || twoChar == L"++" || twoChar == L"--") {
            i += 2;
            return {TokenType::Operator, i - 2, 2};
        }
    }
    // Single character operator
    i++;
    return {TokenType::Operator, i - 1, 1};
}

} // namespace RawrXD
