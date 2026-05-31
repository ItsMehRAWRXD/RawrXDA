// ============================================================================
// smart_completion.cpp — Smart Code Completion Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "completion/smart_completion.h"
#include "ide/ast_completion_bridge.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <random>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Completion {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

std::string generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 32; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> splitWords(const std::string& str) {
    std::vector<std::string> words;
    std::string current;
    
    for (char c : str) {
        if (std::isalnum(c)) {
            current += c;
        } else {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        }
    }
    
    if (!current.empty()) {
        words.push_back(current);
    }
    
    return words;
}

int levenshteinDistance(const std::string& s1, const std::string& s2) {
    const int m = static_cast<int>(s1.length());
    const int n = static_cast<int>(s2.length());
    
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;
    
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    
    return dp[m][n];
}

float fuzzyMatch(const std::string& query, const std::string& target) {
    if (query.empty() || target.empty()) return 0.0f;
    
    std::string q = toLower(query);
    std::string t = toLower(target);
    
    // Exact match
    if (q == t) return 1.0f;
    
    // Prefix match
    if (t.find(q) == 0) return 0.9f;
    
    // Contains match
    if (t.find(q) != std::string::npos) return 0.7f;
    
    // Fuzzy match based on edit distance
    int distance = levenshteinDistance(q, t);
    int maxLen = static_cast<int>(std::max(q.length(), t.length()));
    
    if (maxLen == 0) return 0.0f;
    
    float similarity = 1.0f - (static_cast<float>(distance) / static_cast<float>(maxLen));
    return std::max(0.0f, similarity);
}

std::string extractIndentation(const std::string& line) {
    std::string indent;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            indent += c;
        } else {
            break;
        }
    }
    return indent;
}

std::string getCurrentLine(const std::string& text, uint32_t line) {
    std::istringstream stream(text);
    std::string currentLine;
    uint32_t current = 0;
    
    while (std::getline(stream, currentLine)) {
        if (current == line) {
            return currentLine;
        }
        current++;
    }
    
    return "";
}

std::vector<std::string> extractSymbols(const std::string& text) {
    std::vector<std::string> symbols;
    std::regex symbolRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    
    std::smatch match;
    std::string remaining = text;
    
    while (std::regex_search(remaining, match, symbolRegex)) {
        symbols.push_back(match[1].str());
        remaining = match.suffix();
    }
    
    return symbols;
}

bool isAfterKeyword(const std::string& text, const std::vector<std::string>& keywords) {
    std::string lower = toLower(text);
    for (const auto& keyword : keywords) {
        if (lower.length() >= keyword.length()) {
            std::string suffix = lower.substr(lower.length() - keyword.length());
            if (suffix == keyword) {
                return true;
            }
        }
    }
    return false;
}

std::string detectLanguage(const std::string& uri) {
    std::string ext = fs::path(uri).extension().string();
    
    static const std::unordered_map<std::string, std::string> extensions = {
        {".cpp", "cpp"}, {".cc", "cpp"}, {".cxx", "cpp"}, {".hpp", "cpp"}, {".h", "cpp"},
        {".c", "c"},
        {".ts", "typescript"}, {".tsx", "typescript"},
        {".js", "javascript"}, {".jsx", "javascript"},
        {".py", "python"},
        {".go", "go"},
        {".rs", "rust"},
        {".java", "java"},
        {".cs", "csharp"},
        {".rb", "ruby"},
        {".php", "php"},
        {".swift", "swift"},
        {".kt", "kotlin"},
        {".scala", "scala"},
        {".lua", "lua"},
        {".r", "r"},
        {".sql", "sql"},
        {".json", "json"},
        {".yaml", "yaml"}, {".yml", "yaml"},
        {".xml", "xml"},
        {".html", "html"},
        {".css", "css"}, {".scss", "css"},
        {".md", "markdown"}
    };
    
    auto it = extensions.find(ext);
    return it != extensions.end() ? it->second : "text";
}

} // anonymous namespace

// ============================================================================
// Language Rules Database
// ============================================================================

class LanguageRulesDatabase {
private:
    std::unordered_map<std::string, LanguageRules> rules_;
    
public:
    LanguageRulesDatabase() {
        initializeDefaultRules();
    }
    
    void initializeDefaultRules() {
        // C++ rules
        LanguageRules cpp;
        cpp.language = "cpp";
        cpp.keywords = {
            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
            "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
            "class", "compl", "concept", "const", "consteval", "constexpr", "const_cast",
            "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete",
            "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
            "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
            "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator",
            "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
            "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
            "static_cast", "struct", "switch", "template", "this", "thread_local", "throw",
            "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
            "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
        };
        cpp.builtins = {
            "std::vector", "std::string", "std::map", "std::set", "std::unordered_map",
            "std::unordered_set", "std::array", "std::list", "std::deque", "std::queue",
            "std::stack", "std::pair", "std::tuple", "std::optional", "std::variant",
            "std::any", "std::function", "std::shared_ptr", "std::unique_ptr", "std::weak_ptr",
            "std::make_shared", "std::make_unique", "std::move", "std::forward",
            "std::cout", "std::cin", "std::cerr", "std::endl", "std::flush"
        };
        cpp.types = {
            "int", "float", "double", "char", "bool", "void", "auto", "long", "short",
            "unsigned", "signed", "const", "volatile", "wchar_t", "char16_t", "char32_t",
            "char8_t", "nullptr_t", "size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t",
            "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t"
        };
        cpp.functionPattern = R"((?:auto\s+)?(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:->\s*\w+\s*)?\{)";
        cpp.classPattern = R"(class\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+\w+)*\s*\{)";
        cpp.variablePattern = R"((?:const\s+)?(?:auto\s+)?(\w+)\s*=\s*[^;]+;)";
        cpp.importPattern = R"(#include\s*[<"]([^>"]+)[>"])";
        rules_["cpp"] = cpp;
        
        // TypeScript rules
        LanguageRules ts;
        ts.language = "typescript";
        ts.keywords = {
            "abstract", "any", "as", "asserts", "async", "await", "bigint", "boolean",
            "break", "case", "catch", "class", "const", "constructor", "continue",
            "debugger", "declare", "default", "delete", "do", "else", "enum", "export",
            "extends", "false", "finally", "for", "from", "function", "get", "if",
            "implements", "import", "in", "infer", "instanceof", "interface", "is",
            "keyof", "let", "module", "namespace", "never", "new", "null", "number",
            "object", "of", "package", "private", "protected", "public", "readonly",
            "require", "global", "return", "set", "static", "string", "super", "switch",
            "symbol", "this", "throw", "true", "try", "type", "typeof", "undefined",
            "unique", "unknown", "var", "void", "while", "with", "yield"
        };
        ts.builtins = {
            "console", "window", "document", "Array", "Object", "String", "Number",
            "Boolean", "Function", "Symbol", "Map", "Set", "WeakMap", "WeakSet",
            "Promise", "Observable", "Subject", "BehaviorSubject", "ReplaySubject",
            "Component", "Injectable", "NgModule", "Pipe", "Directive", "ElementRef",
            "ViewChild", "ContentChild", "Input", "Output", "EventEmitter"
        };
        ts.types = {
            "string", "number", "boolean", "object", "any", "unknown", "never", "void",
            "null", "undefined", "bigint", "symbol"
        };
        ts.functionPattern = R"((?:async\s+)?function\s+(\w+)\s*<[^>]*>\s*\([^)]*\)\s*(?::\s*\w+\s*)?\{)";
        ts.classPattern = R"(class\s+(\w+)(?:\s+extends\s+\w+)?(?:\s+implements\s+\w+)*\s*\{)";
        ts.variablePattern = R"((?:const|let|var)\s+(\w+)\s*(?::\s*\w+\s*)?=\s*[^;]+;)";
        ts.importPattern = R"(import\s+(?:\{[^}]+\}|\w+)\s+from\s+['"]([^'"]+)['"])";
        ts.exportPattern = R"(export\s+(?:default\s+)?(?:class|function|const|let|var)\s+(\w+))";
        rules_["typescript"] = ts;
        
        // JavaScript rules
        LanguageRules js;
        js.language = "javascript";
        js.keywords = {
            "async", "await", "break", "case", "catch", "class", "const", "continue",
            "debugger", "default", "delete", "do", "else", "export", "extends", "false",
            "finally", "for", "function", "if", "import", "in", "instanceof", "let",
            "new", "null", "return", "static", "super", "switch", "this", "throw",
            "true", "try", "typeof", "undefined", "var", "void", "while", "with", "yield"
        };
        js.builtins = {
            "console", "window", "document", "Array", "Object", "String", "Number",
            "Boolean", "Function", "Symbol", "Map", "Set", "WeakMap", "WeakSet",
            "Promise", "Proxy", "Reflect", "JSON", "Math", "Date", "RegExp", "Error",
            "setTimeout", "setInterval", "clearTimeout", "clearInterval", "fetch"
        };
        js.functionPattern = R"((?:async\s+)?function\s+(\w+)\s*\([^)]*\)\s*\{)";
        js.classPattern = R"(class\s+(\w+)(?:\s+extends\s+\w+)?\s*\{)";
        js.variablePattern = R"((?:const|let|var)\s+(\w+)\s*=\s*[^;]+;)";
        js.importPattern = R"(import\s+(?:\{[^}]+\}|\w+)\s+from\s+['"]([^'"]+)['"])";
        rules_["javascript"] = js;
        
        // Python rules
        LanguageRules py;
        py.language = "python";
        py.keywords = {
            "False", "None", "True", "and", "as", "assert", "async", "await",
            "break", "class", "continue", "def", "del", "elif", "else", "except",
            "finally", "for", "from", "global", "if", "import", "in", "is", "lambda",
            "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
        };
        py.builtins = {
            "abs", "all", "any", "bin", "bool", "bytes", "callable", "chr", "dict",
            "dir", "divmod", "enumerate", "eval", "exec", "filter", "float", "format",
            "frozenset", "getattr", "globals", "hasattr", "hash", "help", "hex", "id",
            "input", "int", "isinstance", "issubclass", "iter", "len", "list", "locals",
            "map", "max", "min", "next", "object", "oct", "open", "ord", "pow", "print",
            "range", "repr", "reversed", "round", "set", "setattr", "slice", "sorted",
            "str", "sum", "super", "tuple", "type", "vars", "zip"
        };
        py.types = {
            "int", "float", "str", "bool", "list", "dict", "set", "tuple", "bytes",
            "None", "Any", "Union", "Optional", "List", "Dict", "Set", "Tuple"
        };
        py.functionPattern = R"(def\s+(\w+)\s*\([^)]*\)\s*(?:->\s*\w+\s*)?:)";
        py.classPattern = R"(class\s+(\w+)(?:\s*\([^)]*\))?\s*:)";
        py.variablePattern = R"((\w+)\s*=\s*[^;]+)";
        py.importPattern = R"(import\s+(\w+)|from\s+(\w+)\s+import)";
        rules_["python"] = py;
        
        // Go rules
        LanguageRules go;
        go.language = "go";
        go.keywords = {
            "break", "case", "chan", "const", "continue", "default", "defer", "else",
            "fallthrough", "for", "func", "go", "goto", "if", "import", "interface",
            "map", "package", "range", "return", "select", "struct", "switch", "type", "var"
        };
        go.builtins = {
            "append", "cap", "close", "complex", "copy", "delete", "imag", "len",
            "make", "new", "panic", "print", "println", "real", "recover"
        };
        go.types = {
            "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
            "int", "int8", "int16", "int32", "int64", "rune", "string", "uint", "uint8",
            "uint16", "uint32", "uint64", "uintptr"
        };
        go.functionPattern = R"(func\s+(?:\([^)]+\)\s+)?(\w+)\s*\([^)]*\)\s*(?:\([^)]*\))?\s*\{)";
        go.classPattern = R"(type\s+(\w+)\s+struct\s*\{)";
        go.variablePattern = R"((?:var|const)\s+(\w+)\s+(?:\w+\s*)?=)";
        go.importPattern = R"(import\s+(?:\([^)]+\)|['"]([^'"]+)['"])";
        rules_["go"] = go;
        
        // Rust rules
        LanguageRules rust;
        rust.language = "rust";
        rust.keywords = {
            "as", "async", "await", "break", "const", "continue", "crate", "dyn",
            "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
            "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
            "self", "Self", "static", "struct", "super", "trait", "true", "type",
            "unsafe", "use", "where", "while"
        };
        rust.builtins = {
            "println!", "print!", "format!", "vec!", "panic!", "assert!", "assert_eq!",
            "assert_ne!", "debug_assert!", "debug_assert_eq!", "debug_assert_ne!",
            "Option", "Result", "Vec", "String", "Box", "Rc", "Arc", "Cell", "RefCell",
            "HashMap", "HashSet", "BTreeMap", "BTreeSet"
        };
        rust.types = {
            "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16", "u32", "u64",
            "u128", "usize", "f32", "f64", "bool", "char", "str"
        };
        rust.functionPattern = R"(fn\s+(\w+)\s*[<\(][^)]*[\)>]\s*(?:->\s*\w+\s*)?\{)";
        rust.classPattern = R"(struct\s+(\w+)(?:<[^>]+>)?\s*\{)";
        rust.variablePattern = R"(let\s+(?:mut\s+)?(\w+)\s*(?::\s*\w+\s*)?=)";
        rust.importPattern = R"(use\s+([^;]+);)";
        rules_["rust"] = rust;
    }
    
    std::optional<LanguageRules> getRules(const std::string& language) const {
        auto it = rules_.find(language);
        if (it != rules_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void setRules(const std::string& language, const LanguageRules& rules) {
        rules_[language] = rules;
    }
};

// ============================================================================
// Pattern Database
// ============================================================================

class PatternDatabase {
private:
    std::vector<CompletionPattern> patterns_;
    
public:
    PatternDatabase() {
        initializeDefaultPatterns();
    }
    
    void initializeDefaultPatterns() {
        // C++ patterns
        patterns_.push_back({
            "cpp-for-loop",
            "For Loop",
            std::regex(R"(for\s*\(\s*$)"),
            "for (${1:int} ${2:i} = 0; ${2} < ${3:count}; ${2}++) {\n\t$0\n}",
            {"type", "var", "count"},
            "cpp",
            1.0f,
            false
        });
        
        patterns_.push_back({
            "cpp-while-loop",
            "While Loop",
            std::regex(R"(while\s*\(\s*$)"),
            "while (${1:condition}) {\n\t$0\n}",
            {"condition"},
            "cpp",
            1.0f,
            false
        });
        
        patterns_.push_back({
            "cpp-if-statement",
            "If Statement",
            std::regex(R"(if\s*\(\s*$)"),
            "if (${1:condition}) {\n\t$0\n}",
            {"condition"},
            "cpp",
            1.0f,
            false
        });
        
        patterns_.push_back({
            "cpp-function",
            "Function Definition",
            std::regex(R"((\w+)\s+(\w+)\s*\(\s*$)"),
            "${1:returnType} ${2:functionName}(${3:params}) {\n\t$0\n}",
            {"returnType", "functionName", "params"},
            "cpp",
            0.9f,
            true
        });
        
        patterns_.push_back({
            "cpp-class",
            "Class Definition",
            std::regex(R"(class\s+(\w+)\s*\{?\s*$)"),
            "class ${1:ClassName} {\npublic:\n\t${1}();\n\t~${1}();\nprivate:\n\t$0\n};",
            {"ClassName"},
            "cpp",
            0.9f,
            true
        });
        
        // TypeScript patterns
        patterns_.push_back({
            "ts-arrow-function",
            "Arrow Function",
            std::regex(R"((\w+)\s*=>\s*$)"),
            "const ${1:functionName} = (${2:params}): ${3:returnType} => {\n\t$0\n}",
            {"functionName", "params", "returnType"},
            "typescript",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "ts-interface",
            "Interface Definition",
            std::regex(R"(interface\s+(\w+)\s*\{?\s*$)"),
            "interface ${1:InterfaceName} {\n\t$0\n}",
            {"InterfaceName"},
            "typescript",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "ts-async-function",
            "Async Function",
            std::regex(R"(async\s+function\s*$)"),
            "async function ${1:functionName}(${2:params}): Promise<${3:returnType}> {\n\t$0\n}",
            {"functionName", "params", "returnType"},
            "typescript",
            1.0f,
            true
        });
        
        // Python patterns
        patterns_.push_back({
            "py-function",
            "Function Definition",
            std::regex(R"(def\s+(\w+)\s*\(\s*$)"),
            "def ${1:functionName}(${2:params}) -> ${3:returnType}:\n\t$0",
            {"functionName", "params", "returnType"},
            "python",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "py-class",
            "Class Definition",
            std::regex(R"(class\s+(\w+)(?:\([^)]*\))?\s*:\s*$)"),
            "class ${1:ClassName}:\n\tdef __init__(self):\n\t\t$0",
            {"ClassName"},
            "python",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "py-for-loop",
            "For Loop",
            std::regex(R"(for\s+(\w+)\s+in\s+$)"),
            "for ${1:var} in ${2:iterable}:\n\t$0",
            {"var", "iterable"},
            "python",
            1.0f,
            true
        });
        
        // Go patterns
        patterns_.push_back({
            "go-function",
            "Function Definition",
            std::regex(R"(func\s+(\w+)\s*\(\s*$)"),
            "func ${1:functionName}(${2:params}) ${3:returnType} {\n\t$0\n}",
            {"functionName", "params", "returnType"},
            "go",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "go-method",
            "Method Definition",
            std::regex(R"(func\s*\(\s*(\w+)\s+(\w+)\)\s+(\w+)\s*\(\s*$)"),
            "func (${1:receiver} ${2:type}) ${3:methodName}(${4:params}) ${5:returnType} {\n\t$0\n}",
            {"receiver", "type", "methodName", "params", "returnType"},
            "go",
            1.0f,
            true
        });
        
        // Rust patterns
        patterns_.push_back({
            "rust-function",
            "Function Definition",
            std::regex(R"(fn\s+(\w+)\s*[<\(]\s*$)"),
            "fn ${1:functionName}(${2:params}) -> ${3:returnType} {\n\t$0\n}",
            {"functionName", "params", "returnType"},
            "rust",
            1.0f,
            true
        });
        
        patterns_.push_back({
            "rust-impl",
            "Impl Block",
            std::regex(R"(impl\s+(\w+)\s*\{?\s*$)"),
            "impl ${1:TypeName} {\n\t$0\n}",
            {"TypeName"},
            "rust",
            1.0f,
            true
        });
    }
    
    std::vector<CompletionPattern> getPatterns(const std::string& language) const {
        std::vector<CompletionPattern> result;
        for (const auto& pattern : patterns_) {
            if (pattern.language == language || pattern.language.empty()) {
                result.push_back(pattern);
            }
        }
        return result;
    }
    
    void addPattern(const CompletionPattern& pattern) {
        patterns_.push_back(pattern);
    }
    
    void removePattern(const std::string& patternId) {
        patterns_.erase(
            std::remove_if(patterns_.begin(), patterns_.end(),
                [&patternId](const CompletionPattern& p) { return p.id == patternId; }),
            patterns_.end()
        );
    }
};

// ============================================================================
// Snippet Database
// ============================================================================

class SnippetDatabase {
private:
    std::vector<SnippetTemplate> snippets_;
    
public:
    SnippetDatabase() {
        initializeDefaultSnippets();
    }
    
    void initializeDefaultSnippets() {
        // C++ snippets
        snippets_.push_back({
            "Main Function",
            "main",
            "int main(int argc, char* argv[]) {\n\t$0\n\treturn 0;\n}",
            "Main function entry point",
            "cpp",
            {}
        });
        
        snippets_.push_back({
            "Include Guard",
            "ifndef",
            "#ifndef ${1:HEADER}_H\n#define ${1}_H\n\n$0\n\n#endif // ${1}_H",
            "Include guard for header files",
            "cpp",
            {"HEADER"}
        });
        
        snippets_.push_back({
            "Class Declaration",
            "class",
            "class ${1:ClassName} {\npublic:\n\t${1}();\n\t~${1}();\nprivate:\n\t$0\n};",
            "Class declaration template",
            "cpp",
            {"ClassName"}
        });
        
        // TypeScript snippets
        snippets_.push_back({
            "Console Log",
            "cl",
            "console.log($0);",
            "Console log statement",
            "typescript",
            {}
        });
        
        snippets_.push_back({
            "Arrow Function",
            "af",
            "const ${1:functionName} = (${2:params}): ${3:returnType} => {\n\t$0\n};",
            "Arrow function expression",
            "typescript",
            {"functionName", "params", "returnType"}
        });
        
        snippets_.push_back({
            "Interface",
            "interface",
            "interface ${1:InterfaceName} {\n\t$0\n}",
            "Interface definition",
            "typescript",
            {"InterfaceName"}
        });
        
        snippets_.push_back({
            "React Component",
            "rcc",
            "import React from 'react';\n\ninterface ${1:ComponentName}Props {\n\t$2\n}\n\nexport const ${1}: React.FC<${1}Props> = (props) => {\n\treturn (\n\t\t$0\n\t);\n};",
            "React functional component",
            "typescript",
            {"ComponentName"}
        });
        
        // Python snippets
        snippets_.push_back({
            "Main Block",
            "main",
            "if __name__ == '__main__':\n\t$0",
            "Main block entry point",
            "python",
            {}
        });
        
        snippets_.push_back({
            "Function Definition",
            "def",
            "def ${1:functionName}(${2:params}) -> ${3:returnType}:\n\t$0",
            "Function definition",
            "python",
            {"functionName", "params", "returnType"}
        });
        
        snippets_.push_back({
            "Class Definition",
            "class",
            "class ${1:ClassName}:\n\tdef __init__(self):\n\t\t$0",
            "Class definition",
            "python",
            {"ClassName"}
        });
        
        // Go snippets
        snippets_.push_back({
            "Main Function",
            "main",
            "func main() {\n\t$0\n}",
            "Main function entry point",
            "go",
            {}
        });
        
        snippets_.push_back({
            "Error Check",
            "iferr",
            "if err != nil {\n\treturn $0\n}",
            "Error check pattern",
            "go",
            {}
        });
        
        // Rust snippets
        snippets_.push_back({
            "Main Function",
            "main",
            "fn main() {\n\t$0\n}",
            "Main function entry point",
            "rust",
            {}
        });
        
        snippets_.push_back({
            "Function Definition",
            "fn",
            "fn ${1:functionName}(${2:params}) -> ${3:returnType} {\n\t$0\n}",
            "Function definition",
            "rust",
            {"functionName", "params", "returnType"}
        });
    }
    
    std::vector<SnippetTemplate> getSnippets(const std::string& language) const {
        std::vector<SnippetTemplate> result;
        for (const auto& snippet : snippets_) {
            if (snippet.language == language || snippet.language.empty()) {
                result.push_back(snippet);
            }
        }
        return result;
    }
    
    void addSnippet(const SnippetTemplate& snippet) {
        snippets_.push_back(snippet);
    }
    
    void removeSnippet(const std::string& name) {
        snippets_.erase(
            std::remove_if(snippets_.begin(), snippets_.end(),
                [&name](const SnippetTemplate& s) { return s.name == name; }),
            snippets_.end()
        );
    }
};

// ============================================================================
// Smart Completion Engine Implementation
// ============================================================================

class SmartCompletionEngine : public ISmartCompletionEngine {
private:
    CompletionSettings settings_;
    std::vector<std::unique_ptr<ICompletionProvider>> providers_;
    LanguageRulesDatabase languageRules_;
    PatternDatabase patternDatabase_;
    SnippetDatabase snippetDatabase_;
    std::unordered_map<std::string, uint32_t> usageStats_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> lastUsed_;
    
    // AST Context Bridge (NEW)
    std::shared_ptr<IDE::ASTCompletionBridge> astBridge_;
    
    ContextAnalysis analyzeContextInternal(const CompletionContext& context) const {
        ContextAnalysis analysis;
        
        // Extract indentation
        if (!context.recentLines.empty()) {
            analysis.indentation = extractIndentation(context.recentLines.back());
        }
        
        // Detect scope
        std::string lower = toLower(context.textBeforeCursor);
        
        // Check for keywords
        analysis.isAfterReturn = isAfterKeyword(context.textBeforeCursor, {"return", "return "});
        analysis.isAfterNew = isAfterKeyword(context.textBeforeCursor, {"new", "new "});
        analysis.isAfterDot = context.textBeforeCursor.find('.') != std::string::npos;
        
        // Check for function context
        std::regex functionRegex(R"((?:async\s+)?function\s+\w+\s*\([^)]*\)\s*\{)");
        if (std::regex_search(context.textBeforeCursor, functionRegex)) {
            analysis.isInFunction = true;
        }
        
        // Check for class context
        std::regex classRegex(R"(class\s+\w+[^{]*\{)");
        if (std::regex_search(context.textBeforeCursor, classRegex)) {
            analysis.isInClass = true;
        }
        
        // Check for loop context
        std::regex loopRegex(R"((?:for|while|do)\s*\([^)]*\)\s*\{)");
        if (std::regex_search(context.textBeforeCursor, loopRegex)) {
            analysis.isInLoop = true;
        }
        
        // Check for condition context
        std::regex conditionRegex(R"((?:if|else\s+if|switch)\s*\([^)]*\)\s*\{)");
        if (std::regex_search(context.textBeforeCursor, conditionRegex)) {
            analysis.isInCondition = true;
        }
        
        // Extract visible symbols
        auto symbols = extractSymbols(context.textBeforeCursor);
        for (const auto& symbol : symbols) {
            analysis.visibleVariables.push_back(symbol);
        }
        
        return analysis;
    }
    
    std::vector<CompletionItem> getKeywordCompletions(const CompletionContext& context) const {
        std::vector<CompletionItem> items;
        
        auto rules = languageRules_.getRules(context.language);
        if (!rules) return items;
        
        for (const auto& keyword : rules->keywords) {
            CompletionItem item;
            item.label = keyword;
            item.insertText = keyword;
            item.kind = CompletionItem::Kind::Keyword;
            item.sortText = keyword;
            item.filterText = keyword;
            item.detail = "Keyword";
            items.push_back(item);
        }
        
        return items;
    }
    
    std::vector<CompletionItem> getBuiltinCompletions(const CompletionContext& context) const {
        std::vector<CompletionItem> items;
        
        auto rules = languageRules_.getRules(context.language);
        if (!rules) return items;
        
        for (const auto& builtin : rules->builtins) {
            CompletionItem item;
            item.label = builtin;
            item.insertText = builtin;
            item.kind = CompletionItem::Kind::Function;
            item.sortText = builtin;
            item.filterText = builtin;
            item.detail = "Built-in";
            items.push_back(item);
        }
        
        return items;
    }
    
    std::vector<CompletionItem> getSnippetCompletions(const CompletionContext& context) const {
        std::vector<CompletionItem> items;
        
        auto snippets = snippetDatabase_.getSnippets(context.language);
        for (const auto& snippet : snippets) {
            CompletionItem item;
            item.label = snippet.name;
            item.insertText = snippet.body;
            item.kind = CompletionItem::Kind::Snippet;
            item.sortText = snippet.prefix;
            item.filterText = snippet.prefix;
            item.detail = snippet.description;
            item.insertTextFormat = CompletionItem::InsertTextFormat::Snippet;
            items.push_back(item);
        }
        
        return items;
    }
    
    std::vector<CompletionItem> getPatternCompletions(const CompletionContext& context) const {
        std::vector<CompletionItem> items;
        
        auto patterns = patternDatabase_.getPatterns(context.language);
        for (const auto& pattern : patterns) {
            if (std::regex_search(context.textBeforeCursor, pattern.pattern)) {
                CompletionItem item;
                item.label = pattern.name;
                item.insertText = pattern.template_;
                item.kind = CompletionItem::Kind::Snippet;
                item.sortText = pattern.name;
                item.detail = "Pattern";
                item.insertTextFormat = CompletionItem::InsertTextFormat::Snippet;
                item.isMultiLine = pattern.isMultiLine;
                items.push_back(item);
            }
        }
        
        return items;
    }
    
    std::vector<CompletionItem> getSymbolCompletions(const CompletionContext& context) const {
        std::vector<CompletionItem> items;
        
        // Extract symbols from defined symbols
        for (const auto& [name, type] : context.definedSymbols) {
            CompletionItem item;
            item.label = name;
            item.insertText = name;
            item.kind = CompletionItem::Kind::Variable;
            item.sortText = name;
            item.filterText = name;
            item.detail = type;
            items.push_back(item);
        }
        
        // Extract symbols from imported symbols
        for (const auto& symbol : context.importedSymbols) {
            CompletionItem item;
            item.label = symbol;
            item.insertText = symbol;
            item.kind = CompletionItem::Kind::Reference;
            item.sortText = symbol;
            item.filterText = symbol;
            item.detail = "Imported";
            items.push_back(item);
        }
        
        return items;
    }
    
    CompletionScore calculateScore(const CompletionItem& item, 
                                   const CompletionContext& context) const {
        CompletionScore score;
        
        // Fuzzy match score
        if (settings_.enableFuzzyMatching) {
            score.factors.fuzzyMatch = fuzzyMatch(context.textBeforeCursor, item.label);
        }
        
        // Frequency score
        auto usageIt = usageStats_.find(item.label);
        if (usageIt != usageStats_.end()) {
            score.factors.frequency = std::min(1.0f, usageIt->second / 100.0f);
        }
        
        // Recency score
        auto lastUsedIt = lastUsed_.find(item.label);
        if (lastUsedIt != lastUsed_.end()) {
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::hours>(now - lastUsedIt->second);
            score.factors.recency = std::max(0.0f, 1.0f - (duration.count() / 168.0f)); // 1 week decay
        }
        
        // Kind-based scoring
        switch (item.kind) {
            case CompletionItem::Kind::Keyword:
                score.factors.contextMatch = 0.3f;
                break;
            case CompletionItem::Kind::Function:
            case CompletionItem::Kind::Method:
                score.factors.contextMatch = 0.5f;
                break;
            case CompletionItem::Kind::Variable:
                score.factors.contextMatch = 0.7f;
                break;
            case CompletionItem::Kind::Snippet:
                score.factors.patternMatch = 0.8f;
                break;
            default:
                break;
        }
        
        // Calculate total score
        score.total = 
            score.factors.fuzzyMatch * 0.3f +
            score.factors.frequency * 0.2f +
            score.factors.recency * 0.2f +
            score.factors.contextMatch * 0.2f +
            score.factors.patternMatch * 0.1f;
        
        return score;
    }
    
public:
    SmartCompletionEngine() {
        // Initialize with default settings
        settings_.triggerCharacters = {".", "(", " ", "<", "\"", "'"};
    }
    
    // Configuration
    void setSettings(const CompletionSettings& settings) override {
        settings_ = settings;
    }
    
    CompletionSettings getSettings() const override {
        return settings_;
    }
    
    // Main completion API
    CompletionList getCompletions(const CompletionContext& context) override {
        CompletionList result;
        result.isIncomplete = false;
        result.timestamp = std::chrono::system_clock::now();
        
        std::vector<CompletionItem> allItems;
        
        // Capture AST context if bridge is available (NEW)
        IDE::ASTContext astContext;
        if (astBridge_) {
            astContext = astBridge_->captureASTContext(
                context.uri, context.language, 
                context.position.line, context.position.column);
            
            // Enrich completion context with AST information
            auto mutableContext = context; // Copy for modification
            astBridge_->enrichCompletionContext(mutableContext, astContext);
        }
        
        // Get completions from different sources
        auto keywords = getKeywordCompletions(context);
        allItems.insert(allItems.end(), keywords.begin(), keywords.end());
        
        auto builtins = getBuiltinCompletions(context);
        allItems.insert(allItems.end(), builtins.begin(), builtins.end());
        
        auto snippets = getSnippetCompletions(context);
        allItems.insert(allItems.end(), snippets.begin(), snippets.end());
        
        auto patterns = getPatternCompletions(context);
        allItems.insert(allItems.end(), patterns.begin(), patterns.end());
        
        auto symbols = getSymbolCompletions(context);
        allItems.insert(allItems.end(), symbols.begin(), symbols.end());
        
        // Get scope-aware completions from AST if available (NEW)
        if (astBridge_ && astContext.isValid) {
            // Extract prefix from context
            std::string prefix;
            size_t lastSpace = context.textBeforeCursor.rfind(' ');
            if (lastSpace != std::string::npos) {
                prefix = context.textBeforeCursor.substr(lastSpace + 1);
            } else {
                prefix = context.textBeforeCursor;
            }
            
            auto scopeCompletions = astBridge_->getScopeCompletions(astContext, prefix);
            allItems.insert(allItems.end(), scopeCompletions.begin(), scopeCompletions.end());
        }
        
        // Get completions from providers
        for (const auto& provider : providers_) {
            if (provider->canProvide(context)) {
                auto providerItems = provider->provideCompletions(context);
                allItems.insert(allItems.end(), providerItems.begin(), providerItems.end());
            }
        }
        
        // Score and rank completions
        auto ranked = rankCompletions(allItems, context);
        
        // Limit results
        if (ranked.size() > settings_.maxSuggestions) {
            ranked.resize(settings_.maxSuggestions);
            result.isIncomplete = true;
        }
        
        result.items = ranked;
        return result;
    }
    
    std::optional<CompletionItem> resolveCompletion(const CompletionItem& item) override {
        // Resolve completion details (e.g., documentation)
        for (const auto& provider : providers_) {
            auto resolved = provider->resolveCompletion(item);
            if (resolved) {
                return resolved;
            }
        }
        return item;
    }
    
    // Provider management
    void registerProvider(std::unique_ptr<ICompletionProvider> provider) override {
        providers_.push_back(std::move(provider));
    }
    
    void unregisterProvider(const std::string& name) override {
        providers_.erase(
            std::remove_if(providers_.begin(), providers_.end(),
                [&name](const std::unique_ptr<ICompletionProvider>& p) { 
                    return p->name() == name; 
                }),
            providers_.end()
        );
    }
    
    std::vector<std::string> getProviders() const override {
        std::vector<std::string> names;
        for (const auto& provider : providers_) {
            names.push_back(provider->name());
        }
        return names;
    }
    
    // Pattern management
    void addPattern(const CompletionPattern& pattern) override {
        patternDatabase_.addPattern(pattern);
    }
    
    void removePattern(const std::string& patternId) override {
        patternDatabase_.removePattern(patternId);
    }
    
    std::vector<CompletionPattern> getPatterns(const std::string& language) const override {
        return patternDatabase_.getPatterns(language);
    }
    
    // Snippet management
    void addSnippet(const SnippetTemplate& snippet) override {
        snippetDatabase_.addSnippet(snippet);
    }
    
    void removeSnippet(const std::string& name) override {
        snippetDatabase_.removeSnippet(name);
    }
    
    std::vector<SnippetTemplate> getSnippets(const std::string& language) const override {
        return snippetDatabase_.getSnippets(language);
    }
    
    // Language rules
    void setLanguageRules(const std::string& language, const LanguageRules& rules) override {
        languageRules_.setRules(language, rules);
    }
    
    std::optional<LanguageRules> getLanguageRules(const std::string& language) const override {
        return languageRules_.getRules(language);
    }
    
    // Context analysis
    ContextAnalysis analyzeContext(const CompletionContext& context) const override {
        return analyzeContextInternal(context);
    }
    
    // Scoring
    CompletionScore scoreCompletion(const CompletionItem& item,
                                   const CompletionContext& context) const override {
        return calculateScore(item, context);
    }
    
    std::vector<CompletionItem> rankCompletions(
        std::vector<CompletionItem> items,
        const CompletionContext& context) const override {
        
        // Score each item
        for (auto& item : items) {
            auto score = calculateScore(item, context);
            item.score = score.total;
        }
        
        // Sort by score
        std::sort(items.begin(), items.end(),
            [](const CompletionItem& a, const CompletionItem& b) {
                return a.score > b.score;
            });
        
        // Filter by minimum score
        if (settings_.minScore > 0.0f) {
            items.erase(
                std::remove_if(items.begin(), items.end(),
                    [this](const CompletionItem& item) { 
                        return item.score < settings_.minScore; 
                    }),
                items.end()
            );
        }
        
        return items;
    }
    
    // Statistics
    void recordUsage(const std::string& completionId) override {
        usageStats_[completionId]++;
        lastUsed_[completionId] = std::chrono::system_clock::now();
    }
    
    std::unordered_map<std::string, uint32_t> getUsageStatistics() const override {
        return usageStats_;
    }
    
    // Caching
    void clearCache() override {
        usageStats_.clear();
        lastUsed_.clear();
    }
    
    void invalidateFile(const std::string& uri) override {
        // Invalidate any cached completions for this file
        // For now, just clear usage stats for symbols from this file
        // In a real implementation, we would track file-specific caches
    }
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<ISmartCompletionEngine> createSmartCompletionEngine() {
    return std::make_unique<SmartCompletionEngine>();
}

} // namespace Completion
} // namespace RawrXD
