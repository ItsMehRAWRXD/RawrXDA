#include "syntax_highlighter.h"
#include <regex>

namespace RawrXD::Editor {
    static const std::vector<std::pair<std::regex, TokenType>> cppRules = {
        {std::regex(R"(\b(int|float|double|char|bool|void|auto|const|static|inline|virtual|override|class|struct|enum|namespace|using|template|typename|public|private|protected|if|else|for|while|do|switch|case|default|break|continue|return|new|delete|try|catch|throw|sizeof|decltype|constexpr|consteval|constinit|co_await|co_return|co_yield|requires|concept|export|module|import)\b)"), TokenType::KEYWORD},
        {std::regex(R"(\b(true|false|nullptr|NULL)\b)"), TokenType::KEYWORD},
        {std::regex(R"(\b\d+\.?\d*([eE][+-]?\d+)?[fFdD]?\b)"), TokenType::NUMBER},
        {std::regex(R"("(?:[^"\\]|\\.)*")"), TokenType::STRING},
        {std::regex(R"(//.*$)"), TokenType::COMMENT},
        {std::regex(R"(/\*[\s\S]*?\*/)"), TokenType::COMMENT},
        {std::regex(R"(\b[A-Za-z_][A-Za-z0-9_]*\s*\()"), TokenType::FUNCTION},
        {std::regex(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)"), TokenType::IDENTIFIER},
        {std::regex(R"([+\-*/%=<>!&|^~]+)"), TokenType::OPERATOR},
    };

    static const std::vector<std::pair<std::regex, TokenType>> masmRules = {
        {std::regex(R"(\b(mov|push|pop|lea|call|ret|jmp|je|jne|jg|jl|jge|jle|ja|jb|add|sub|mul|div|and|or|xor|not|shl|shr|inc|dec|cmp|test|nop|db|dw|dd|dq|proc|endp|macro|endm|include|includelib|segment|ends|assume|public|extrn|invoke|proto|local|offset|ptr|type|sizeof|lengthof|rep|repe|repne|cmps|movs|stos|lods|scas|xchg|int|iret|cli|sti|pushf|popf|lahf|sahf|bswap|cpuid|rdtsc|xgetbv|vzeroupper|vmovups|vmovaps|vaddps|vmulps|vfmadd213ps|vxorps|vbroadcastss|vpermilps|vshufps|vextractf128|vinsertf128|vcvtps2ph|vcvtph2ps)\b)", std::regex::icase), TokenType::KEYWORD},
        {std::regex(R"(\b(rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r8|r9|r10|r11|r12|r13|r14|r15|eax|ebx|ecx|edx|esi|edi|ebp|esp|ax|bx|cx|dx|si|di|bp|sp|al|bl|cl|dl|ah|bh|ch|dh|xmm\d+|ymm\d+|zmm\d+|cr\d+|dr\d+)\b)", std::regex::icase), TokenType::IDENTIFIER},
        {std::regex(R"(;.*$)"), TokenType::COMMENT},
        {std::regex(R"(\b0[xX][0-9a-fA-F]+\b|\b\d+\b)"), TokenType::NUMBER},
        {std::regex(R"("(?:[^"\\]|\\.)*")"), TokenType::STRING},
        {std::regex(R"([+\-*/%]|\[|\]|\(|\))"), TokenType::OPERATOR},
    };

    std::vector<Token> SyntaxHighlighter::tokenizeLine(const std::string& line, Language lang) {
        std::vector<Token> tokens;
        const auto& rules = (lang == Language::MASM) ? masmRules : cppRules;

        size_t pos = 0;
        while (pos < line.size()) {
            bool matched = false;
            for (const auto& [re, type] : rules) {
                std::smatch match;
                std::string sub = line.substr(pos);
                if (std::regex_search(sub, match, re, std::regex_constants::match_continuous)) {
                    tokens.push_back({type, match.str(), pos});
                    pos += match.length();
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                // Skip unrecognized char
                ++pos;
            }
        }
        return tokens;
    }
}
