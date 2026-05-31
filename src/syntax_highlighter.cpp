#include "syntax_highlighter.hpp"
#include <cctype>

namespace RawrXD {

SyntaxHighlighter::SyntaxHighlighter() { InitializeCPPRules(); }

std::string SyntaxHighlighter::DetectLanguage(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) return "text";
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext=="cpp"||ext=="hpp"||ext=="h"||ext=="cc"||ext=="cxx") return "cpp";
    if (ext=="c") return "c";
    if (ext=="rs") return "rust";
    if (ext=="py") return "python";
    if (ext=="js") return "javascript";
    if (ext=="ts") return "typescript";
    if (ext=="asm"||ext=="masm"||ext=="nasm") return "assembly";
    if (ext=="java") return "java";
    if (ext=="go") return "go";
    if (ext=="cs") return "csharp";
    return "text";
}

void SyntaxHighlighter::SetLanguage(const std::string& language) {
    currentLanguage_ = language;
    rules_.clear(); keywords_.clear(); types_.clear();
    if (language=="cpp"||language=="c++") InitializeCPPRules();
    else if (language=="c") InitializeCRules();
    else if (language=="rust") InitializeRustRules();
    else if (language=="python") InitializePythonRules();
    else if (language=="assembly"||language=="asm") InitializeAsmRules();
    else if (language=="javascript"||language=="typescript") InitializeJavaScriptRules();
}

std::vector<Token> SyntaxHighlighter::HighlightLine(const std::string& line) {
    std::vector<Token> tokens;
    if (line.empty()) return tokens;
    // Phase 1: regex rules
    for (const auto& rule : rules_) {
        std::sregex_iterator it(line.begin(), line.end(), rule.pattern), end;
        for (; it != end; ++it)
            tokens.push_back({rule.type, (size_t)it->position(), (size_t)it->length()});
    }
    // Phase 2: identifier/keyword scan
    size_t i = 0;
    while (i < line.size()) {
        if (std::isspace((unsigned char)line[i])) { i++; continue; }
        bool covered = false;
        for (const auto& tok : tokens) {
            if (i >= tok.start && i < tok.start + tok.length) {
                covered = true; i = tok.start + tok.length; break;
            }
        }
        if (covered) continue;
        if (std::isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i]=='_')) i++;
            std::string word = line.substr(start, i - start);
            auto kw = keywords_.find(word);
            if (kw != keywords_.end())
                tokens.push_back({kw->second, start, i - start});
            else if (types_.count(word))
                tokens.push_back({TokenType::TYPE, start, i - start});
        } else { i++; }
    }
    // Sort
    std::sort(tokens.begin(), tokens.end(), [](const Token& a, const Token& b){ return a.start < b.start; });
    // Remove overlaps (first wins)
    std::vector<Token> filtered;
    for (const auto& tok : tokens) {
        bool overlaps = false;
        for (const auto& ex : filtered) {
            if (tok.start < ex.start + ex.length && tok.start + tok.length > ex.start) {
                overlaps = true; break;
            }
        }
        if (!overlaps) filtered.push_back(tok);
    }
    return filtered;
}

void SyntaxHighlighter::AddKeywords(const char** kws, size_t count, TokenType type) {
    for (size_t i = 0; i < count; ++i) keywords_[kws[i]] = type;
}

void SyntaxHighlighter::InitializeCPPRules() {
    using R = std::regex;
    rules_.push_back({TokenType::STRING,      R(R"("(?:[^"\\]|\\.)*")"),     10});
    rules_.push_back({TokenType::STRING,      R(R"('(?:[^'\\]|\\.)*')"),     10});
    rules_.push_back({TokenType::COMMENT,     R(R"(//.*$)"),                100});
    rules_.push_back({TokenType::COMMENT,     R(R"(/\*[\s\S]*?\*/)"),       100});
    rules_.push_back({TokenType::PREPROCESSOR,R(R"(^\s*#\s*\w+)"),          90});
    rules_.push_back({TokenType::NUMBER,      R(R"(\b0[xX][0-9a-fA-F]+\b)"),20});
    rules_.push_back({TokenType::NUMBER,      R(R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[fFlLuU]?\b)"),20});
    const char* kw[] = {
        "alignas","alignof","and","and_eq","asm","auto","bitand","bitor","bool","break",
        "case","catch","char","char8_t","char16_t","char32_t","class","compl","concept",
        "const","consteval","constexpr","constinit","const_cast","continue","co_await",
        "co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
        "else","enum","explicit","export","extern","false","float","for","friend","goto",
        "if","inline","int","long","mutable","namespace","new","noexcept","not","not_eq",
        "nullptr","operator","or","or_eq","private","protected","public","register",
        "reinterpret_cast","requires","return","short","signed","sizeof","static",
        "static_assert","static_cast","struct","switch","template","this","thread_local",
        "throw","true","try","typedef","typeid","typename","union","unsigned","using",
        "virtual","void","volatile","wchar_t","while","xor","xor_eq"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
    for (const char* t : {"int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t",
        "uint32_t","uint64_t","size_t","ptrdiff_t","intptr_t","uintptr_t","ssize_t",
        "string","wstring","vector","map","unordered_map","set","unordered_set","list",
        "deque","queue","stack","unique_ptr","shared_ptr","weak_ptr","optional","variant",
        "array","tuple","pair","function"}) types_.insert(t);
}

void SyntaxHighlighter::InitializeCRules() {
    using R = std::regex;
    rules_.push_back({TokenType::STRING,      R(R"("(?:[^"\\]|\\.)*")"),    10});
    rules_.push_back({TokenType::STRING,      R(R"('(?:[^'\\]|\\.)*')"),    10});
    rules_.push_back({TokenType::COMMENT,     R(R"(//.*$)"),               100});
    rules_.push_back({TokenType::COMMENT,     R(R"(/\*[\s\S]*?\*/)"),      100});
    rules_.push_back({TokenType::PREPROCESSOR,R(R"(^\s*#\s*\w+)"),         90});
    rules_.push_back({TokenType::NUMBER,      R(R"(\b\d+(?:\.\d+)?[fFlL]?\b)"),20});
    const char* kw[] = {
        "auto","break","case","char","const","continue","default","do","double","else",
        "enum","extern","float","for","goto","if","inline","int","long","register",
        "restrict","return","short","signed","sizeof","static","struct","switch",
        "typedef","union","unsigned","void","volatile","while","_Alignas","_Alignof",
        "_Atomic","_Bool","_Complex","_Generic","_Noreturn","_Static_assert","_Thread_local"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
    for (const char* t : {"int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t",
        "uint32_t","uint64_t","size_t","ptrdiff_t","time_t","FILE"}) types_.insert(t);
}

void SyntaxHighlighter::InitializeRustRules() {
    using R = std::regex;
    rules_.push_back({TokenType::STRING,  R(R"("(?:[^"\\]|\\.)*")"), 10});
    rules_.push_back({TokenType::COMMENT, R(R"(//.*$)"),            100});
    rules_.push_back({TokenType::COMMENT, R(R"(/\*[\s\S]*?\*/)"),   100});
    rules_.push_back({TokenType::NUMBER,  R(R"(\b\d+(?:\.\d+)?\b)"), 20});
    const char* kw[] = {
        "as","break","const","continue","crate","else","enum","extern","false","fn",
        "for","if","impl","in","let","loop","match","mod","move","mut","pub","ref",
        "return","self","Self","static","struct","super","trait","true","type","unsafe",
        "use","where","while","async","await","dyn"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
    for (const char* t : {"String","Vec","Option","Result","Box","Rc","Arc","Cell",
        "RefCell","HashMap","HashSet","i8","i16","i32","i64","i128","isize",
        "u8","u16","u32","u64","u128","usize","f32","f64","bool","char","str"}) types_.insert(t);
}

void SyntaxHighlighter::InitializePythonRules() {
    using R = std::regex;
    rules_.push_back({TokenType::STRING,  R(R"(\"\"\"[\s\S]*?\"\"\")"), 100});
    rules_.push_back({TokenType::STRING,  R(R"(\'\'\'[\s\S]*?\'\'\')"), 100});
    rules_.push_back({TokenType::STRING,  R(R"("(?:[^"\\]|\\.)*")"),     10});
    rules_.push_back({TokenType::STRING,  R(R"('(?:[^'\\]|\\.)*')"),     10});
    rules_.push_back({TokenType::COMMENT, R(R"(#.*$)"),                 100});
    rules_.push_back({TokenType::NUMBER,  R(R"(\b\d+(?:\.\d+)?[jJ]?\b)"),20});
    const char* kw[] = {
        "False","None","True","and","as","assert","async","await","break","class",
        "continue","def","del","elif","else","except","finally","for","from","global",
        "if","import","in","is","lambda","nonlocal","not","or","pass","raise","return",
        "try","while","with","yield"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
    for (const char* t : {"int","str","float","list","dict","tuple","set","frozenset","bool",
        "bytes","bytearray","object","type","property","staticmethod","classmethod"}) types_.insert(t);
}

void SyntaxHighlighter::InitializeAsmRules() {
    using R = std::regex;
    rules_.push_back({TokenType::COMMENT, R(R"(;.*$)"),                           100});
    rules_.push_back({TokenType::STRING,  R(R"("(?:[^"\\]|\\.)*")"),               10});
    rules_.push_back({TokenType::STRING,  R(R"('(?:[^'\\]|\\.)*')"),               10});
    rules_.push_back({TokenType::NUMBER,  R(R"(\b[0-9a-fA-F]+h\b|\b0[xX][0-9a-fA-F]+\b|\b\d+\b)"),20});
    const char* kw[] = {
        "mov","movzx","movsx","lea","push","pop","add","sub","mul","div","imul","idiv",
        "inc","dec","and","or","xor","not","neg","shl","shr","sar","sal","rol","ror",
        "rcl","rcr","jmp","je","jne","jz","jnz","ja","jna","jb","jnb","jg","jng","jl",
        "jnl","call","ret","cmp","test","nop","int","syscall","sysret","leave","enter",
        "loop","proc","endp","segment","ends","assume","db","dw","dd","dq","dt","equ",
        "org","offset","ptr","byte","word","dword","qword","tbyte","xmmword","ymmword",
        "zmmword","public","extrn","include","macro","endm"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
    for (const char* r : {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "eax","ebx","ecx","edx","esi","edi","ebp","esp",
        "eax","ebx","ecx","edx",
        "al","ah","bl","bh","cl","ch","dl","dh",
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
        "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
        "zmm0","zmm1","zmm2","zmm3","zmm4","zmm5","zmm6","zmm7"
    }) types_.insert(r);
}

void SyntaxHighlighter::InitializeJavaScriptRules() {
    using R = std::regex;
    rules_.push_back({TokenType::STRING,  R(R"("(?:[^"\\]|\\.)*")"), 10});
    rules_.push_back({TokenType::STRING,  R(R"('(?:[^'\\]|\\.)*')"), 10});
    rules_.push_back({TokenType::STRING,  R(R"(`(?:[^`\\]|\\.)*`)"), 10});
    rules_.push_back({TokenType::COMMENT, R(R"(//.*$)"),            100});
    rules_.push_back({TokenType::COMMENT, R(R"(/\*[\s\S]*?\*/)"),   100});
    rules_.push_back({TokenType::NUMBER,  R(R"(\b\d+(?:\.\d+)?\b)"), 20});
    const char* kw[] = {
        "break","case","catch","class","const","continue","debugger","default","delete",
        "do","else","export","extends","finally","for","function","if","import","in",
        "instanceof","new","return","super","switch","this","throw","try","typeof","var",
        "void","while","with","yield","let","static","await","async","of","from","as",
        "true","false","null","undefined"
    };
    AddKeywords(kw, sizeof(kw)/sizeof(kw[0]), TokenType::KEYWORD);
}

std::string SyntaxHighlighter::GetColorForToken(TokenType type, bool darkTheme) {
    if (darkTheme) {
        switch (type) {
            case TokenType::KEYWORD:      return "#C586C0";
            case TokenType::STRING:       return "#CE9178";
            case TokenType::NUMBER:       return "#B5CEA8";
            case TokenType::COMMENT:      return "#6A9955";
            case TokenType::PREPROCESSOR: return "#C586C0";
            case TokenType::TYPE:         return "#4EC9B0";
            case TokenType::FUNCTION:     return "#DCDCAA";
            case TokenType::OPERATOR:     return "#D4D4D4";
            default:                      return "#D4D4D4";
        }
    } else {
        switch (type) {
            case TokenType::KEYWORD:      return "#0000FF";
            case TokenType::STRING:       return "#A31515";
            case TokenType::NUMBER:       return "#098658";
            case TokenType::COMMENT:      return "#008000";
            case TokenType::PREPROCESSOR: return "#0000FF";
            case TokenType::TYPE:         return "#267F99";
            case TokenType::FUNCTION:     return "#795E26";
            case TokenType::OPERATOR:     return "#000000";
            default:                      return "#000000";
        }
    }
}

} // namespace RawrXD
