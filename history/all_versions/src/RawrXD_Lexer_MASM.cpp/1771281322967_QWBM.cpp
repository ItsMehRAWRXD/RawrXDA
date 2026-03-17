#include "RawrXD_Lexer_MASM.h"
#include <algorithm>
#include <immintrin.h>

namespace RawrXD {

// SIMD-optimized character classification functions
bool IsWhitespace_SIMD_MASM(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
}

bool IsDigit_SIMD_MASM(wchar_t c) {
    return c >= L'0' && c <= L'9';
}

bool IsAlpha_SIMD_MASM(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

bool IsAlnum_SIMD_MASM(wchar_t c) {
    return IsAlpha_SIMD_MASM(c) || IsDigit_SIMD_MASM(c);
}

size_t FindNextTokenBoundary_SIMD_MASM(const std::wstring& text, size_t start) {
    size_t len = text.length();
    size_t pos = start;
    
    while (pos + 31 < len) {
        bool found_boundary = false;
        for (int i = 0; i < 32; i++) {
            wchar_t c = text[pos + i];
            if (!IsAlnum_SIMD_MASM(c) && c != L'_' && c != L'.' && c != L'@' && c != L'?') {
                return pos + i;
            }
        }
        pos += 32;
    }
    
    while (pos < len) {
        wchar_t c = text[pos];
        if (!IsAlnum_SIMD_MASM(c) && c != L'_' && c != L'.' && c != L'@' && c != L'?') {
            return pos;
        }
        pos++;
    }
    
    return len;
}

MASMLexer::MASMLexer() {
    // Populate with common MASM keywords
    // x64 Instructions
    instructions = {
        L"mov", L"add", L"sub", L"imul", L"idiv", L"inc", L"dec", L"lea",
        L"and", L"or", L"xor", L"not", L"neg", L"shl", L"shr", L"sar",
        L"push", L"pop", L"call", L"ret", L"jmp", L"je", L"jne", L"jg", L"jge", L"jl", L"jle",
        L"cmp", L"test", L"nop", L"int", L"syscall",
        L"vmovups", L"vaddps", L"vmulps" // AVX examples
    };
    
    // x64 Registers
    registers = {
        L"rax", L"rbx", L"rcx", L"rdx", L"rsi", L"rdi", L"rbp", L"rsp",
        L"r8", L"r9", L"r10", L"r11", L"r12", L"r13", L"r14", L"r15",
        L"eax", L"ebx", L"ecx", L"edx", L"esi", L"edi", L"ebp", L"esp",
        L"ax", L"bx", L"cx", L"dx",
        L"al", L"bl", L"cl", L"dl",
        L"xmm0", L"xmm1", L"ymm0", L"ymm1"
    };
    
    // MASM Directives
    directives = {
        L"proc", L"endp", L"proto", L"invoke", 
        L".data", L".code", L".const", L"struct", L"ends", 
        L"byte", L"word", L"dword", L"qword", L"real4", L"real8",
        L"public", L"extern", L"include", L"includelib", 
        L"option", L"casemap", L"macro", L"endm"
    };
}

bool MASMLexer::isInstruction(const std::wstring& s) const {
    std::wstring lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return instructions.find(lower) != instructions.end();
}

bool MASMLexer::isRegister(const std::wstring& s) const {
    std::wstring lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return registers.find(lower) != registers.end();
}

bool MASMLexer::isDirective(const std::wstring& s) const {
    std::wstring lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return directives.find(lower) != directives.end();
}

void MASMLexer::lex(const std::wstring& text, std::vector<Token>& outTokens) {
    if (text.empty()) return;
    
    const wchar_t* p = text.c_str();
    int len = (int)text.length();
    int i = 0;
    
    while (i < len) {
        wchar_t c = p[i];
        
        // Whitespace
        if (iswspace(c)) {
            i++;
            continue;
        }
        
        // Comment ;
        if (c == L';') {
            outTokens.push_back({TokenType::Comment, i, len - i});
            break; // Rest of line is comment
        }
        
        // String " or '
        if (c == L'"' || c == L'\'') {
            wchar_t quote = c;
            int start = i;
            i++;
            while (i < len && p[i] != quote) {
                if (p[i] == L'\\') i++; // simple escape
                i++;
            }
            if (i < len) i++; // consume closing quote
            outTokens.push_back({TokenType::String, start, i - start});
            continue;
        }
        
        // Number (Hex/Decimal)
        // Simple heuristic: starts with digit
        if (iswdigit(c)) {
            int start = i;
            while (i < len && (iswalnum(p[i]) || p[i] == L'.')) i++; // Simplified number scan
            outTokens.push_back({TokenType::Number, start, i - start});
            continue;
        }
        
        // Identifier/Keyword
        if (iswalpha(c) || c == L'_' || c == L'.' || c == L'@') {
            int start = i;
            while (i < len && (iswalnum(p[i]) || p[i] == L'_' || p[i] == L'.' || p[i] == L'@' || p[i] == L'?')) i++;
            std::wstring word(p + start, i - start);
            
            TokenType type = TokenType::Default;
            if (isInstruction(word)) type = TokenType::Instruction;
            else if (isRegister(word)) type = TokenType::Register;
            else if (isDirective(word)) type = TokenType::Directive;
            // Check for label definition (next char is :)
            else if (i < len && p[i] == L':') {
                type = TokenType::Label;
                i++; // consume :
                outTokens.push_back({type, start, i - start});
                continue;
            }
            
            outTokens.push_back({type, start, i - start});
            continue;
        }
        
        // Operators / Punctuation
        outTokens.push_back({TokenType::Operator, i, 1});
        i++;
    }
}

} // namespace RawrXD
