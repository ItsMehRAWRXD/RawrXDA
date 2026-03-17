#include "KeywordHashTable.h"
#include <algorithm>

namespace RawrXD {

KeywordHashTable::KeywordHashTable() {
    // Initialize common languages
    initializeLanguage(Language::C);
    initializeLanguage(Language::Cpp);
    initializeLanguage(Language::Assembly);
    initializeLanguage(Language::MASM);
    initializeLanguage(Language::Python);
    initializeLanguage(Language::Java);
    initializeLanguage(Language::JavaScript);
    initializeLanguage(Language::TypeScript);
    initializeLanguage(Language::Go);
    initializeLanguage(Language::Rust);
}

void KeywordHashTable::initializeLanguage(Language lang) {
    auto& set = keywordSets[lang];

    switch (lang) {
        case Language::C:
            set = {
                "auto", "break", "case", "char", "const", "continue", "default", "do",
                "double", "else", "enum", "extern", "float", "for", "goto", "if",
                "int", "long", "register", "return", "short", "signed", "sizeof", "static",
                "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
            };
            break;

        case Language::Cpp:
            set = {
                "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
                "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch",
                "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
                "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
                "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
                "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
                "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
                "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
                "private", "protected", "public", "reflexpr", "register", "reinterpret_cast",
                "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
                "static_cast", "struct", "switch", "synchronized", "template", "this",
                "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
                "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
                "xor", "xor_eq"
            };
            break;

        case Language::Assembly:
        case Language::MASM:
            set = {
                "mov", "add", "sub", "imul", "idiv", "inc", "dec", "lea",
                "and", "or", "xor", "not", "neg", "shl", "shr", "sar",
                "push", "pop", "call", "ret", "jmp", "je", "jne", "jg", "jge", "jl", "jle",
                "cmp", "test", "nop", "int", "syscall",
                "vmovups", "vaddps", "vmulps", // AVX examples
                "proc", "endp", "proto", "invoke",
                ".data", ".code", ".const", "struct", "ends",
                "byte", "word", "dword", "qword", "real4", "real8",
                "public", "extern", "include", "includelib",
                "option", "casemap", "macro", "endm"
            };
            break;

        case Language::Python:
            set = {
                "False", "None", "True", "and", "as", "assert", "async", "await", "break",
                "class", "continue", "def", "del", "elif", "else", "except", "finally",
                "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
                "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
            };
            break;

        case Language::Java:
            set = {
                "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
                "class", "const", "continue", "default", "do", "double", "else", "enum",
                "extends", "final", "finally", "float", "for", "goto", "if", "implements",
                "import", "instanceof", "int", "interface", "long", "native", "new",
                "package", "private", "protected", "public", "return", "short", "static",
                "strictfp", "super", "switch", "synchronized", "this", "throw", "throws",
                "transient", "try", "void", "volatile", "while"
            };
            break;

        case Language::JavaScript:
            set = {
                "await", "break", "case", "catch", "class", "const", "continue", "debugger",
                "default", "delete", "do", "else", "enum", "export", "extends", "false",
                "finally", "for", "function", "if", "implements", "import", "in", "instanceof",
                "interface", "let", "new", "null", "package", "private", "protected", "public",
                "return", "static", "super", "switch", "this", "throw", "true", "try",
                "typeof", "var", "void", "while", "with", "yield"
            };
            break;

        case Language::TypeScript:
            // Inherits from JavaScript plus additional
            set = {
                "await", "break", "case", "catch", "class", "const", "continue", "debugger",
                "default", "delete", "do", "else", "enum", "export", "extends", "false",
                "finally", "for", "function", "if", "implements", "import", "in", "instanceof",
                "interface", "let", "new", "null", "package", "private", "protected", "public",
                "return", "static", "super", "switch", "this", "throw", "true", "try",
                "typeof", "var", "void", "while", "with", "yield",
                // TypeScript specific
                "any", "as", "boolean", "constructor", "declare", "get", "infer", "is",
                "keyof", "module", "namespace", "never", "object", "readonly", "require",
                "set", "string", "symbol", "type", "undefined", "unique", "unknown", "from"
            };
            break;

        case Language::Go:
            set = {
                "break", "case", "chan", "const", "continue", "default", "defer", "else",
                "fallthrough", "for", "func", "go", "goto", "if", "import", "interface",
                "map", "package", "range", "return", "select", "struct", "switch", "type",
                "var"
            };
            break;

        case Language::Rust:
            set = {
                "as", "break", "const", "continue", "crate", "else", "enum", "extern",
                "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
                "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct",
                "super", "trait", "true", "type", "unsafe", "use", "where", "while"
            };
            break;

        default:
            // Empty set for unsupported languages
            break;
    }
}

bool KeywordHashTable::isKeyword(Language lang, const std::string& word) const {
    auto it = keywordSets.find(lang);
    if (it == keywordSets.end()) return false;
    return it->second.count(word) > 0;
}

std::vector<std::string> KeywordHashTable::getKeywords(Language lang) const {
    auto it = keywordSets.find(lang);
    if (it == keywordSets.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

void KeywordHashTable::addKeyword(Language lang, const std::string& keyword) {
    keywordSets[lang].insert(keyword);
}

void KeywordHashTable::removeKeyword(Language lang, const std::string& keyword) {
    auto it = keywordSets.find(lang);
    if (it != keywordSets.end()) {
        it->second.erase(keyword);
    }
}

} // namespace RawrXD