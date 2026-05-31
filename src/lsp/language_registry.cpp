// ============================================================================
// language_registry.cpp — Phase 3: Multi-Language Support
// ============================================================================
// Language detection, per-language symbol extraction, and language-specific
// completion providers. Supports C/C++, Python, JS/TS, Rust, Go, Java, C#.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "lsp/language_registry.h"

#include <regex>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD::LSP {

// ============================================================================
// LanguageRegistry Singleton
// ============================================================================

LanguageRegistry& LanguageRegistry::instance() {
    static LanguageRegistry s_instance;
    return s_instance;
}

LanguageRegistry::LanguageRegistry() {
    registerBuiltInLanguages();
}

void LanguageRegistry::registerBuiltInLanguages() {
    registerLanguage(makeCppInfo());
    registerLanguage(makePythonInfo());
    registerLanguage(makeJavaScriptInfo());
    registerLanguage(makeTypeScriptInfo());
    registerLanguage(makeRustInfo());
    registerLanguage(makeGoInfo());
    registerLanguage(makeJavaInfo());
    registerLanguage(makeCSharpInfo());
}

// ============================================================================
// Built-in Language Definitions
// ============================================================================

LanguageInfo LanguageRegistry::makeCppInfo() {
    LanguageInfo info;
    info.id = "cpp";
    info.name = "C++";
    info.extensions = {".cpp", ".cc", ".cxx", ".hpp", ".hxx", ".h", ".inl"};
    info.astLanguage = LanguageId::Cpp;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "alignas","alignof","and","and_eq","asm","auto","bitand","bitor",
        "bool","break","case","catch","char","char8_t","char16_t","char32_t",
        "class","compl","concept","const","consteval","constexpr","constinit",
        "const_cast","continue","co_await","co_return","co_yield","decltype",
        "default","delete","do","double","dynamic_cast","else","enum",
        "explicit","export","extern","false","float","for","friend","goto",
        "if","inline","int","long","mutable","namespace","new","noexcept",
        "not","not_eq","nullptr","operator","or","or_eq","private","protected",
        "public","register","reinterpret_cast","requires","return","short",
        "signed","sizeof","static","static_assert","static_cast","struct",
        "switch","template","this","thread_local","throw","true","try",
        "typedef","typeid","typename","union","unsigned","using","virtual",
        "void","volatile","wchar_t","while","xor","xor_eq"
    };
    info.builtinTypes = {
        "bool","char","char8_t","char16_t","char32_t","double","float","int",
        "long","short","signed","unsigned","void","wchar_t","size_t","ptrdiff_t",
        "intptr_t","uintptr_t","int8_t","int16_t","int32_t","int64_t",
        "uint8_t","uint16_t","uint32_t","uint64_t","std::string","std::vector",
        "std::map","std::unordered_map","std::set","std::unordered_set",
        "std::unique_ptr","std::shared_ptr","std::optional","std::variant"
    };
    info.completionTriggers = {
        {".", true, false},
        {"::", false, true},
        {"->", true, false},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makePythonInfo() {
    LanguageInfo info;
    info.id = "python";
    info.name = "Python";
    info.extensions = {".py", ".pyw", ".pyi"};
    info.shebangPatterns = {"python", "python3", "python2"};
    info.astLanguage = LanguageId::Python;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "False","None","True","and","as","assert","async","await","break",
        "class","continue","def","del","elif","else","except","finally",
        "for","from","global","if","import","in","is","lambda","nonlocal",
        "not","or","pass","raise","return","try","while","with","yield"
    };
    info.builtinTypes = {
        "int","float","str","bool","list","dict","tuple","set","frozenset",
        "bytes","bytearray","memoryview","object","type","complex","range",
        "NoneType","Exception","BaseException","Iterator","Iterable"
    };
    info.completionTriggers = {
        {".", true, false},
        {"(", false, false},
    };
    info.lineComment = "#";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makeJavaScriptInfo() {
    LanguageInfo info;
    info.id = "javascript";
    info.name = "JavaScript";
    info.extensions = {".js", ".mjs", ".cjs", ".jsx"};
    info.shebangPatterns = {"node", "nodejs"};
    info.astLanguage = LanguageId::JavaScript;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "break","case","catch","class","const","continue","debugger","default",
        "delete","do","else","export","extends","finally","for","function",
        "if","import","in","instanceof","new","return","super","switch","this",
        "throw","try","typeof","var","void","while","with","yield","let","static"
    };
    info.builtinTypes = {
        "Number","String","Boolean","Object","Array","Function","Date","RegExp",
        "Error","Map","Set","WeakMap","WeakSet","Promise","Symbol","BigInt",
        "undefined","null","NaN","Infinity"
    };
    info.completionTriggers = {
        {".", true, false},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makeTypeScriptInfo() {
    LanguageInfo info = makeJavaScriptInfo();
    info.id = "typescript";
    info.name = "TypeScript";
    info.extensions = {".ts", ".tsx", ".mts", ".cts"};
    info.astLanguage = LanguageId::TypeScript;
    info.keywords.insert(info.keywords.end(), {
        "interface","type","namespace","module","declare","abstract","implements",
        "readonly","enum","public","private","protected","override","satisfies"
    });
    info.builtinTypes.insert(info.builtinTypes.end(), {
        "any","unknown","never","void","null","undefined","object","symbol",
        "Record","Partial","Required","Readonly","Pick","Omit","Exclude",
        "Extract","NonNullable","Parameters","ReturnType","InstanceType"
    });
    info.completionTriggers.push_back({"<", false, false});
    return info;
}

LanguageInfo LanguageRegistry::makeRustInfo() {
    LanguageInfo info;
    info.id = "rust";
    info.name = "Rust";
    info.extensions = {".rs"};
    info.astLanguage = LanguageId::Rust;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "as","break","const","continue","crate","else","enum","extern","false",
        "fn","for","if","impl","in","let","loop","match","mod","move","mut",
        "pub","ref","return","self","Self","static","struct","super","trait",
        "true","type","unsafe","use","where","while","async","await","dyn"
    };
    info.builtinTypes = {
        "i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128","usize",
        "f32","f64","bool","char","str","String","Vec","Box","Rc","Arc","Option",
        "Result","HashMap","BTreeMap","HashSet","BTreeSet","VecDeque","LinkedList"
    };
    info.completionTriggers = {
        {".", true, false},
        {"::", false, true},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makeGoInfo() {
    LanguageInfo info;
    info.id = "go";
    info.name = "Go";
    info.extensions = {".go"};
    info.astLanguage = LanguageId::Go;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "break","case","chan","const","continue","default","defer","else",
        "fallthrough","for","func","go","goto","if","import","interface","map",
        "package","range","return","select","struct","switch","type","var"
    };
    info.builtinTypes = {
        "bool","byte","complex64","complex128","error","float32","float64",
        "int","int8","int16","int32","int64","rune","string","uint","uint8",
        "uint16","uint32","uint64","uintptr","any","comparable"
    };
    info.completionTriggers = {
        {".", true, false},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makeJavaInfo() {
    LanguageInfo info;
    info.id = "java";
    info.name = "Java";
    info.extensions = {".java"};
    info.astLanguage = LanguageId::Java;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "abstract","assert","boolean","break","byte","case","catch","char",
        "class","const","continue","default","do","double","else","enum",
        "extends","final","finally","float","for","goto","if","implements",
        "import","instanceof","int","interface","long","native","new",
        "package","private","protected","public","return","short","static",
        "strictfp","super","switch","synchronized","this","throw","throws",
        "transient","try","void","volatile","while","record","sealed","permits"
    };
    info.builtinTypes = {
        "boolean","byte","char","double","float","int","long","short","void",
        "String","Object","Integer","Double","Float","Boolean","Character",
        "Long","Short","Byte","List","Map","Set","ArrayList","HashMap","HashSet"
    };
    info.completionTriggers = {
        {".", true, false},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

LanguageInfo LanguageRegistry::makeCSharpInfo() {
    LanguageInfo info;
    info.id = "csharp";
    info.name = "C#";
    info.extensions = {".cs"};
    info.astLanguage = LanguageId::CSharp;
    info.capabilities = LanguageCapability::All;
    info.keywords = {
        "abstract","as","base","bool","break","byte","case","catch","char",
        "checked","class","const","continue","decimal","default","delegate",
        "do","double","else","enum","event","explicit","extern","false",
        "finally","fixed","float","for","foreach","goto","if","implicit",
        "in","int","interface","internal","is","lock","long","namespace",
        "new","null","object","operator","out","override","params","private",
        "protected","public","readonly","ref","return","sbyte","sealed",
        "short","sizeof","stackalloc","static","string","struct","switch",
        "this","throw","true","try","typeof","uint","ulong","unchecked",
        "unsafe","ushort","using","virtual","void","volatile","while","yield"
    };
    info.builtinTypes = {
        "bool","byte","char","decimal","double","float","int","long","object",
        "sbyte","short","string","uint","ulong","ushort","void","dynamic",
        "var","List","Dictionary","HashSet","Queue","Stack","Task","ValueTuple"
    };
    info.completionTriggers = {
        {".", true, false},
        {"(", false, false},
    };
    info.lineComment = "//";
    info.blockCommentStart = "/*";
    info.blockCommentEnd = "*/";
    info.caseSensitive = true;
    return info;
}

// ============================================================================
// Registration / Detection
// ============================================================================

void LanguageRegistry::registerLanguage(const LanguageInfo& info) {
    std::unique_lock lock(m_mutex);
    m_languages[info.id] = info;
    for (const auto& ext : info.extensions) {
        m_extensionToLanguage[ext] = info.id;
    }
}

void LanguageRegistry::unregisterLanguage(const std::string& id) {
    std::unique_lock lock(m_mutex);
    auto it = m_languages.find(id);
    if (it != m_languages.end()) {
        for (const auto& ext : it->second.extensions) {
            m_extensionToLanguage.erase(ext);
        }
        m_languages.erase(it);
    }
}

std::optional<LanguageInfo> LanguageRegistry::detectLanguage(
    const std::string& uri,
    const std::string& content) const {
    // Extension-based
    fs::path p(uri);
    std::string ext = p.extension().string();
    if (!ext.empty()) {
        auto it = m_extensionToLanguage.find(ext);
        if (it != m_extensionToLanguage.end()) {
            return getLanguageById(it->second);
        }
    }
    // Shebang-based
    if (!content.empty() && content.size() > 2 && content[0] == '#' && content[1] == '!') {
        size_t nl = content.find('\n');
        std::string shebang = content.substr(0, nl != std::string::npos ? nl : content.size());
        std::shared_lock lock(m_mutex);
        for (const auto& [id, info] : m_languages) {
            for (const auto& pat : info.shebangPatterns) {
                if (shebang.find(pat) != std::string::npos) {
                    return info;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<LanguageInfo> LanguageRegistry::getLanguageById(
    const std::string& id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_languages.find(id);
    if (it != m_languages.end()) return it->second;
    return std::nullopt;
}

std::optional<LanguageInfo> LanguageRegistry::getLanguageByExtension(
    const std::string& ext) const {
    std::shared_lock lock(m_mutex);
    auto it = m_extensionToLanguage.find(ext);
    if (it != m_extensionToLanguage.end()) {
        auto lit = m_languages.find(it->second);
        if (lit != m_languages.end()) return lit->second;
    }
    return std::nullopt;
}

// ============================================================================
// Symbol Extraction
// ============================================================================

std::vector<SymbolInfo> LanguageRegistry::extractSymbols(
    const std::string& uri,
    const std::string& content,
    TreeSitterParser* parser) const {
    auto langOpt = detectLanguage(uri, content);
    if (!langOpt) return {};

    const auto& lang = *langOpt;

    // Try AST-based extraction if parser available
    if (parser && lang.astLanguage != LanguageId::Unknown) {
        auto root = parser->parse(uri, content, lang.astLanguage);
        if (root) {
            auto symbols = parser->extractSymbols(root, uri);
            if (!symbols.empty()) return symbols;
        }
    }

    // Fallback to regex
    return extractSymbolsRegex(uri, content, lang);
}

std::vector<SymbolInfo> LanguageRegistry::extractSymbolsRegex(
    const std::string& uri,
    const std::string& content,
    const LanguageInfo& lang) const {
    std::vector<SymbolInfo> symbols;
    std::istringstream iss(content);
    std::string line;
    uint32_t lineNum = 0;

    if (lang.id == "cpp" || lang.id == "c") {
        // Function definitions
        std::regex funcRegex(
            R"((\w[\w\s\*\&\<>:]*?)\s+(\w+)\s*\([^)]*\)\s*(?:const\s*)?\s*\{)");
        // Class/struct declarations
        std::regex classRegex(R"((?:class|struct)\s+(\w+))");
        // Variable declarations
        std::regex varRegex(R"((\w[\w\s\*\&\<>:]*?)\s+(\w+)\s*(?:=|;))");

        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, funcRegex)) {
                SymbolInfo info;
                info.name = m[2].str();
                info.kind = SymbolKind::Function;
                info.location.uri = uri;
                info.location.line = lineNum;
                info.location.character = static_cast<uint32_t>(m.position(2));
                info.location.endLine = lineNum;
                info.location.endCharacter = info.location.character +
                    static_cast<uint32_t>(info.name.size());
                symbols.push_back(info);
            }
            if (std::regex_search(line, m, classRegex)) {
                SymbolInfo info;
                info.name = m[1].str();
                info.kind = SymbolKind::Class;
                info.location.uri = uri;
                info.location.line = lineNum;
                info.location.character = static_cast<uint32_t>(m.position(1));
                info.location.endLine = lineNum;
                info.location.endCharacter = info.location.character +
                    static_cast<uint32_t>(info.name.size());
                symbols.push_back(info);
            }
            lineNum++;
        }
    } else if (lang.id == "python") {
        std::regex funcRegex(R"(^\s*def\s+(\w+))");
        std::regex classRegex(R"(^\s*class\s+(\w+))");
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, funcRegex)) {
                SymbolInfo info;
                info.name = m[1].str();
                info.kind = SymbolKind::Function;
                info.location.uri = uri;
                info.location.line = lineNum;
                info.location.character = static_cast<uint32_t>(m.position(1));
                info.location.endLine = lineNum;
                info.location.endCharacter = info.location.character +
                    static_cast<uint32_t>(info.name.size());
                symbols.push_back(info);
            }
            if (std::regex_search(line, m, classRegex)) {
                SymbolInfo info;
                info.name = m[1].str();
                info.kind = SymbolKind::Class;
                info.location.uri = uri;
                info.location.line = lineNum;
                info.location.character = static_cast<uint32_t>(m.position(1));
                info.location.endLine = lineNum;
                info.location.endCharacter = info.location.character +
                    static_cast<uint32_t>(info.name.size());
                symbols.push_back(info);
            }
            lineNum++;
        }
    }

    return symbols;
}

// ============================================================================
// Queries
// ============================================================================

std::vector<CompletionTrigger> LanguageRegistry::getCompletionTriggers(
    const std::string& languageId) const {
    auto lang = getLanguageById(languageId);
    if (lang) return lang->completionTriggers;
    return {};
}

std::vector<std::string> LanguageRegistry::getKeywords(
    const std::string& languageId) const {
    auto lang = getLanguageById(languageId);
    if (lang) return lang->keywords;
    return {};
}

std::vector<std::string> LanguageRegistry::getBuiltinTypes(
    const std::string& languageId) const {
    auto lang = getLanguageById(languageId);
    if (lang) return lang->builtinTypes;
    return {};
}

bool LanguageRegistry::supportsCapability(const std::string& languageId,
                                            LanguageCapability cap) const {
    auto lang = getLanguageById(languageId);
    if (lang) return hasCapability(lang->capabilities, cap);
    return false;
}

std::vector<std::string> LanguageRegistry::getRegisteredLanguageIds() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> ids;
    ids.reserve(m_languages.size());
    for (const auto& [id, _] : m_languages) ids.push_back(id);
    return ids;
}

} // namespace RawrXD::LSP
