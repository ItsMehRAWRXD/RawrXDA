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
    return true;
}

void KeywordHashTable::initializeLanguage(Language lang) {
    auto& set = keywordSets[lang];

    switch (lang) {
        case Language::C:
            set = {
                L"auto", L"break", L"case", L"char", L"const", L"continue", L"default", L"do",
                L"double", L"else", L"enum", L"extern", L"float", L"for", L"goto", L"if",
                L"int", L"long", L"register", L"return", L"short", L"signed", L"sizeof", L"static",
                L"struct", L"switch", L"typedef", L"union", L"unsigned", L"void", L"volatile", L"while"
            };
            break;

        case Language::Cpp:
            set = {
                L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"atomic_cancel", L"atomic_commit",
                L"atomic_noexcept", L"auto", L"bitand", L"bitor", L"bool", L"break", L"case", L"catch",
                L"char", L"char8_t", L"char16_t", L"char32_t", L"class", L"compl", L"concept", L"const",
                L"consteval", L"constexpr", L"constinit", L"const_cast", L"continue", L"co_await",
                L"co_return", L"co_yield", L"decltype", L"default", L"delete", L"do", L"double",
                L"dynamic_cast", L"else", L"enum", L"explicit", L"export", L"extern", L"false", L"float",
                L"for", L"friend", L"goto", L"if", L"inline", L"int", L"long", L"mutable", L"namespace",
                L"new", L"noexcept", L"not", L"not_eq", L"nullptr", L"operator", L"or", L"or_eq",
                L"private", L"protected", L"public", L"reflexpr", L"register", L"reinterpret_cast",
                L"requires", L"return", L"short", L"signed", L"sizeof", L"static", L"static_assert",
                L"static_cast", L"struct", L"switch", L"synchronized", L"template", L"this",
                L"thread_local", L"throw", L"true", L"try", L"typedef", L"typeid", L"typename",
                L"union", L"unsigned", L"using", L"virtual", L"void", L"volatile", L"wchar_t", L"while",
                L"xor", L"xor_eq"
            };
            break;

        case Language::Assembly:
        case Language::MASM:
            set = {
                L"mov", L"add", L"sub", L"imul", L"idiv", L"inc", L"dec", L"lea",
                L"and", L"or", L"xor", L"not", L"neg", L"shl", L"shr", L"sar",
                L"push", L"pop", L"call", L"ret", L"jmp", L"je", L"jne", L"jg", L"jge", L"jl", L"jle",
                L"cmp", L"test", L"nop", L"int", L"syscall",
                L"vmovups", L"vaddps", L"vmulps", // AVX examples
                L"proc", L"endp", L"proto", L"invoke",
                L".data", L".code", L".const", L"struct", L"ends",
                L"byte", L"word", L"dword", L"qword", L"real4", L"real8",
                L"public", L"extern", L"include", L"includelib",
                L"option", L"casemap", L"macro", L"endm"
            };
            break;

        case Language::Python:
            set = {
                L"False", L"None", L"True", L"and", L"as", L"assert", L"async", L"await", L"break",
                L"class", L"continue", L"def", L"del", L"elif", L"else", L"except", L"finally",
                L"for", L"from", L"global", L"if", L"import", L"in", L"is", L"lambda", L"nonlocal",
                L"not", L"or", L"pass", L"raise", L"return", L"try", L"while", L"with", L"yield"
            };
            break;

        case Language::Java:
            set = {
                L"abstract", L"assert", L"boolean", L"break", L"byte", L"case", L"catch", L"char",
                L"class", L"const", L"continue", L"default", L"do", L"double", L"else", L"enum",
                L"extends", L"final", L"finally", L"float", L"for", L"goto", L"if", L"implements",
                L"import", L"instanceof", L"int", L"interface", L"long", L"native", L"new",
                L"package", L"private", L"protected", L"public", L"return", L"short", L"static",
                L"strictfp", L"super", L"switch", L"synchronized", L"this", L"throw", L"throws",
                L"transient", L"try", L"void", L"volatile", L"while"
            };
            break;

        case Language::JavaScript:
            set = {
                L"await", L"break", L"case", L"catch", L"class", L"const", L"continue", L"debugger",
                L"default", L"delete", L"do", L"else", L"enum", L"export", L"extends", L"false",
                L"finally", L"for", L"function", L"if", L"implements", L"import", L"in", L"instanceof",
                L"interface", L"let", L"new", L"null", L"package", L"private", L"protected", L"public",
                L"return", L"static", L"super", L"switch", L"this", L"throw", L"true", L"try",
                L"typeof", L"var", L"void", L"while", L"with", L"yield"
            };
            break;

        case Language::TypeScript:
            // Inherits from JavaScript plus additional
            set = {
                L"await", L"break", L"case", L"catch", L"class", L"const", L"continue", L"debugger",
                L"default", L"delete", L"do", L"else", L"enum", L"export", L"extends", L"false",
                L"finally", L"for", L"function", L"if", L"implements", L"import", L"in", L"instanceof",
                L"interface", L"let", L"new", L"null", L"package", L"private", L"protected", L"public",
                L"return", L"static", L"super", L"switch", L"this", L"throw", L"true", L"try",
                L"typeof", L"var", L"void", L"while", L"with", L"yield",
                // TypeScript specific
                L"any", L"as", L"boolean", L"constructor", L"declare", L"get", L"infer", L"is",
                L"keyof", L"module", L"namespace", L"never", L"object", L"readonly", L"require",
                L"set", L"string", L"symbol", L"type", L"undefined", L"unique", L"unknown", L"from"
            };
            break;

        case Language::Go:
            set = {
                L"break", L"case", L"chan", L"const", L"continue", L"default", L"defer", L"else",
                L"fallthrough", L"for", L"func", L"go", L"goto", L"if", L"import", L"interface",
                L"map", L"package", L"range", L"return", L"select", L"struct", L"switch", L"type",
                L"var"
            };
            break;

        case Language::Rust:
            set = {
                L"as", L"break", L"const", L"continue", L"crate", L"else", L"enum", L"extern",
                L"false", L"fn", L"for", L"if", L"impl", L"in", L"let", L"loop", L"match", L"mod",
                L"move", L"mut", L"pub", L"ref", L"return", L"self", L"Self", L"static", L"struct",
                L"super", L"trait", L"true", L"type", L"unsafe", L"use", L"where", L"while"
            };
            break;

        default:
            // Empty set for unsupported languages
            break;
    return true;
}

    return true;
}

bool KeywordHashTable::isKeyword(Language lang, const std::wstring& word) const {
    auto it = keywordSets.find(lang);
    if (it == keywordSets.end()) return false;
    return it->second.count(word) > 0;
    return true;
}

std::vector<std::wstring> KeywordHashTable::getKeywords(Language lang) const {
    auto it = keywordSets.find(lang);
    if (it == keywordSets.end()) return {};
    return std::vector<std::wstring>(it->second.begin(), it->second.end());
    return true;
}

void KeywordHashTable::addKeyword(Language lang, const std::wstring& keyword) {
    keywordSets[lang].insert(keyword);
    return true;
}

void KeywordHashTable::removeKeyword(Language lang, const std::wstring& keyword) {
    auto it = keywordSets.find(lang);
    if (it != keywordSets.end()) {
        it->second.erase(keyword);
    return true;
}

    return true;
}

} // namespace RawrXD
