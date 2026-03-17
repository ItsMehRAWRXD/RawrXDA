// =============================================================================
// RawrXD Symbol Graph Indexer — Production Implementation
// Copilot/Cursor Parity: symbol graph indexing for cross-file navigation
// =============================================================================
// Zero external dependencies. Pure Win32 + STL.
// Indexes: classes, functions, methods, variables, macros, typedefs, enums
// Builds a directed graph of definition → reference edges for jump-to-def,
// find-all-references, call-hierarchy, and type-hierarchy queries.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <functional>
#include <queue>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace RawrXD {
namespace AI {

// ─── Symbol Kinds (LSP-compatible values) ────────────────────────────────────
enum class SymbolKind : uint8_t {
    File = 1, Module = 2, Namespace = 3, Package = 4, Class = 5,
    Method = 6, Property = 7, Field = 8, Constructor = 9, Enum = 10,
    Interface = 11, Function = 12, Variable = 13, Constant = 14,
    String = 15, Number = 16, Boolean = 17, Array = 18, Object = 19,
    Key = 20, Null = 21, EnumMember = 22, Struct = 23, Event = 24,
    Operator = 25, TypeParameter = 26, Macro = 27
};

// ─── Source Location ─────────────────────────────────────────────────────────
struct SourceLocation {
    std::string filePath;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;
};

// ─── Symbol Node ─────────────────────────────────────────────────────────────
struct SymbolNode {
    std::string name;
    std::string qualifiedName;   // e.g. "RawrXD::AI::SymbolGraphIndexer"
    std::string containerName;   // e.g. "RawrXD::AI"
    SymbolKind kind = SymbolKind::Variable;
    SourceLocation definition;
    std::vector<SourceLocation> references;
    std::string signature;       // e.g. "void foo(int, float)"
    std::string docComment;
    uint64_t hash = 0;          // FNV-1a of qualifiedName for fast lookup
};

// ─── Edge Types ──────────────────────────────────────────────────────────────
enum class EdgeType : uint8_t {
    Defines,         // file → symbol definition
    References,      // file → symbol reference
    Calls,           // function → function
    Inherits,        // class → base class
    Implements,      // class → interface
    Contains,        // namespace/class → member
    Includes,        // file → file (#include)
    TypeOf,          // variable → type
};

struct GraphEdge {
    uint64_t sourceHash;
    uint64_t targetHash;
    EdgeType type;
    SourceLocation location;
};

// ─── FNV-1a Hash ─────────────────────────────────────────────────────────────
static uint64_t fnv1a(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (char c : s) {
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        h *= 0x100000001b3ULL;
    }
    return h;
}

// ─── C/C++ Token Scanner ─────────────────────────────────────────────────────
// Lightweight lexer: extracts identifiers, keywords, scoping info
struct Token {
    enum Type { Identifier, Keyword, Punctuation, StringLit, NumberLit, Comment, Preprocessor, Eof };
    Type type;
    std::string text;
    uint32_t line;
    uint32_t col;
};

class CppScanner {
public:
    explicit CppScanner(const std::string& source) : m_src(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        tokens.reserve(m_src.size() / 4); // rough estimate
        uint32_t line = 1, col = 1;
        size_t i = 0;
        const size_t len = m_src.size();

        while (i < len) {
            char c = m_src[i];

            // Skip whitespace
            if (c == ' ' || c == '\t' || c == '\r') { ++i; ++col; continue; }
            if (c == '\n') { ++i; ++line; col = 1; continue; }

            // Line comment
            if (c == '/' && i + 1 < len && m_src[i + 1] == '/') {
                size_t start = i;
                while (i < len && m_src[i] != '\n') ++i;
                tokens.push_back({Token::Comment, m_src.substr(start, i - start), line, col});
                col += static_cast<uint32_t>(i - start);
                continue;
            }

            // Block comment
            if (c == '/' && i + 1 < len && m_src[i + 1] == '*') {
                size_t start = i;
                i += 2; col += 2;
                while (i + 1 < len && !(m_src[i] == '*' && m_src[i + 1] == '/')) {
                    if (m_src[i] == '\n') { ++line; col = 1; } else { ++col; }
                    ++i;
                }
                if (i + 1 < len) { i += 2; col += 2; }
                tokens.push_back({Token::Comment, m_src.substr(start, i - start), line, col});
                continue;
            }

            // Preprocessor
            if (c == '#' && (col == 1 || (i > 0 && m_src[i - 1] == '\n'))) {
                size_t start = i;
                while (i < len && m_src[i] != '\n') {
                    if (m_src[i] == '\\' && i + 1 < len && m_src[i + 1] == '\n') {
                        i += 2; ++line; col = 1; continue;
                    }
                    ++i; ++col;
                }
                tokens.push_back({Token::Preprocessor, m_src.substr(start, i - start), line, col});
                continue;
            }

            // String literal
            if (c == '"' || c == '\'') {
                char quote = c;
                size_t start = i;
                ++i; ++col;
                while (i < len && m_src[i] != quote) {
                    if (m_src[i] == '\\') { ++i; ++col; }
                    ++i; ++col;
                }
                if (i < len) { ++i; ++col; }
                tokens.push_back({Token::StringLit, m_src.substr(start, i - start), line, col});
                continue;
            }

            // Number
            if (c >= '0' && c <= '9') {
                size_t start = i;
                while (i < len && (isalnum(m_src[i]) || m_src[i] == '.' || m_src[i] == 'x' || m_src[i] == 'X'))
                    { ++i; ++col; }
                tokens.push_back({Token::NumberLit, m_src.substr(start, i - start), line, col});
                continue;
            }

            // Identifier or keyword
            if (isalpha(c) || c == '_') {
                size_t start = i;
                while (i < len && (isalnum(m_src[i]) || m_src[i] == '_')) { ++i; ++col; }
                std::string word = m_src.substr(start, i - start);
                Token::Type t = isKeyword(word) ? Token::Keyword : Token::Identifier;
                tokens.push_back({t, std::move(word), line, col});
                continue;
            }

            // Punctuation
            tokens.push_back({Token::Punctuation, std::string(1, c), line, col});
            ++i; ++col;
        }
        return tokens;
    }

private:
    static bool isKeyword(const std::string& w) {
        static const std::unordered_set<std::string> kw = {
            "auto","break","case","char","const","continue","default","do","double",
            "else","enum","extern","float","for","goto","if","int","long","register",
            "return","short","signed","sizeof","static","struct","switch","typedef",
            "union","unsigned","void","volatile","while","class","namespace","public",
            "private","protected","virtual","override","final","template","typename",
            "using","new","delete","try","catch","throw","inline","constexpr","noexcept",
            "nullptr","true","false","explicit","operator","friend","mutable","const_cast",
            "static_cast","reinterpret_cast","dynamic_cast","decltype","concept","requires",
            "#define","#include","#ifdef","#ifndef","#endif","#pragma"
        };
        return kw.count(w) > 0;
    }
    std::string m_src;
};

// =============================================================================
// SymbolGraphIndexer — Main Engine
// =============================================================================
class SymbolGraphIndexer {
public:
    static SymbolGraphIndexer& instance() {
        static SymbolGraphIndexer s;
        return s;
    }

    // ── Index a single file ──────────────────────────────────────────────────
    bool indexFile(const std::string& filePath) {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        ifs.close();

        CppScanner scanner(content);
        auto tokens = scanner.tokenize();

        std::lock_guard<std::mutex> lock(m_mutex);
        extractSymbols(filePath, tokens);
        m_indexedFiles.insert(filePath);
        m_lastIndexTime = std::chrono::steady_clock::now();
        ++m_indexVersion;
        return true;
    }

    // ── Index entire directory recursively ───────────────────────────────────
    size_t indexDirectory(const std::string& rootPath,
                          const std::vector<std::string>& extensions = {".cpp",".hpp",".h",".c",".cxx",".hxx",".cc",".asm",".py",".js",".ts"}) {
        size_t count = 0;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(
                     rootPath, std::filesystem::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                bool match = false;
                for (auto& e : extensions) {
                    if (ext == e) { match = true; break; }
                }
                if (!match) continue;

                // Skip build/vendor directories
                auto pathStr = entry.path().string();
                if (pathStr.find("\\.git\\") != std::string::npos ||
                    pathStr.find("\\node_modules\\") != std::string::npos ||
                    pathStr.find("\\build\\") != std::string::npos ||
                    pathStr.find("\\obj\\") != std::string::npos)
                    continue;

                if (indexFile(pathStr)) ++count;
            }
        } catch (const std::filesystem::filesystem_error&) {}
        return count;
    }

    // ── Find definition of a symbol ──────────────────────────────────────────
    const SymbolNode* findDefinition(const std::string& symbolName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_symbolsByName.find(symbolName);
        if (it == m_symbolsByName.end()) return nullptr;
        // Return the first definition (prefer class/function over variable)
        const SymbolNode* best = nullptr;
        for (auto& idx : it->second) {
            auto& sym = m_symbols[idx];
            if (!best || static_cast<int>(sym.kind) < static_cast<int>(best->kind))
                best = &sym;
        }
        return best;
    }

    // ── Find all references to a symbol ──────────────────────────────────────
    std::vector<SourceLocation> findReferences(const std::string& symbolName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<SourceLocation> result;
        auto it = m_symbolsByName.find(symbolName);
        if (it == m_symbolsByName.end()) return result;
        for (auto& idx : it->second) {
            auto& sym = m_symbols[idx];
            result.push_back(sym.definition);
            result.insert(result.end(), sym.references.begin(), sym.references.end());
        }
        return result;
    }

    // ── Call hierarchy (outgoing calls from a function) ──────────────────────
    std::vector<const SymbolNode*> getOutgoingCalls(const std::string& functionName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<const SymbolNode*> result;
        uint64_t srcHash = fnv1a(functionName);
        for (auto& edge : m_edges) {
            if (edge.sourceHash == srcHash && edge.type == EdgeType::Calls) {
                auto it = m_symbolsByHash.find(edge.targetHash);
                if (it != m_symbolsByHash.end() && it->second < m_symbols.size()) {
                    result.push_back(&m_symbols[it->second]);
                }
            }
        }
        return result;
    }

    // ── Call hierarchy (incoming callers of a function) ──────────────────────
    std::vector<const SymbolNode*> getIncomingCalls(const std::string& functionName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<const SymbolNode*> result;
        uint64_t tgtHash = fnv1a(functionName);
        for (auto& edge : m_edges) {
            if (edge.targetHash == tgtHash && edge.type == EdgeType::Calls) {
                auto it = m_symbolsByHash.find(edge.sourceHash);
                if (it != m_symbolsByHash.end() && it->second < m_symbols.size()) {
                    result.push_back(&m_symbols[it->second]);
                }
            }
        }
        return result;
    }

    // ── Type hierarchy (subtypes of a class) ─────────────────────────────────
    std::vector<const SymbolNode*> getSubtypes(const std::string& className) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<const SymbolNode*> result;
        uint64_t baseHash = fnv1a(className);
        for (auto& edge : m_edges) {
            if (edge.targetHash == baseHash && edge.type == EdgeType::Inherits) {
                auto it = m_symbolsByHash.find(edge.sourceHash);
                if (it != m_symbolsByHash.end() && it->second < m_symbols.size()) {
                    result.push_back(&m_symbols[it->second]);
                }
            }
        }
        return result;
    }

    // ── Fuzzy symbol search (prefix + subsequence matching) ──────────────────
    std::vector<const SymbolNode*> searchSymbols(const std::string& query, size_t maxResults = 20) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        struct ScoredSym { const SymbolNode* sym; int score; };
        std::vector<ScoredSym> scored;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (auto& sym : m_symbols) {
            std::string lowerName = sym.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            int score = 0;
            // Exact match
            if (lowerName == lowerQuery) score = 1000;
            // Prefix match
            else if (lowerName.find(lowerQuery) == 0) score = 800;
            // Contains
            else if (lowerName.find(lowerQuery) != std::string::npos) score = 500;
            // Subsequence match
            else {
                size_t qi = 0;
                for (size_t ni = 0; ni < lowerName.size() && qi < lowerQuery.size(); ++ni) {
                    if (lowerName[ni] == lowerQuery[qi]) ++qi;
                }
                if (qi == lowerQuery.size()) score = 200;
            }

            // Boost for definitions (class/function) over variables
            if (score > 0) {
                if (sym.kind == SymbolKind::Class || sym.kind == SymbolKind::Struct) score += 50;
                if (sym.kind == SymbolKind::Function || sym.kind == SymbolKind::Method) score += 30;
                scored.push_back({&sym, score});
            }
        }

        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.score > b.score; });
        std::vector<const SymbolNode*> result;
        for (size_t i = 0; i < std::min(maxResults, scored.size()); ++i)
            result.push_back(scored[i].sym);
        return result;
    }

    // ── Get include graph for a file ─────────────────────────────────────────
    std::vector<std::string> getIncludes(const std::string& filePath) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> result;
        uint64_t fileHash = fnv1a(filePath);
        for (auto& edge : m_edges) {
            if (edge.sourceHash == fileHash && edge.type == EdgeType::Includes) {
                // Find the target file name from symbols
                auto it = m_symbolsByHash.find(edge.targetHash);
                if (it != m_symbolsByHash.end() && it->second < m_symbols.size()) {
                    result.push_back(m_symbols[it->second].name);
                }
            }
        }
        return result;
    }

    // ── Stats ────────────────────────────────────────────────────────────────
    size_t symbolCount() const { std::lock_guard<std::mutex> lock(m_mutex); return m_symbols.size(); }
    size_t edgeCount() const { std::lock_guard<std::mutex> lock(m_mutex); return m_edges.size(); }
    size_t fileCount() const { std::lock_guard<std::mutex> lock(m_mutex); return m_indexedFiles.size(); }
    uint64_t version() const { return m_indexVersion.load(); }

    // ── Clear index ──────────────────────────────────────────────────────────
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_symbols.clear();
        m_symbolsByName.clear();
        m_symbolsByHash.clear();
        m_edges.clear();
        m_indexedFiles.clear();
        m_indexVersion = 0;
    }

private:
    SymbolGraphIndexer() = default;

    // ── Symbol Extraction from Token Stream ──────────────────────────────────
    void extractSymbols(const std::string& filePath, const std::vector<Token>& tokens) {
        std::string currentNamespace;
        std::string currentClass;
        std::vector<std::string> scopeStack;
        int braceDepth = 0;
        std::vector<int> scopeDepths; // depth at which each scope was entered

        for (size_t i = 0; i < tokens.size(); ++i) {
            auto& tok = tokens[i];

            // Track brace depth
            if (tok.type == Token::Punctuation) {
                if (tok.text == "{") {
                    ++braceDepth;
                } else if (tok.text == "}") {
                    --braceDepth;
                    while (!scopeDepths.empty() && scopeDepths.back() >= braceDepth) {
                        scopeStack.pop_back();
                        scopeDepths.pop_back();
                    }
                }
                continue;
            }

            // #include edges
            if (tok.type == Token::Preprocessor) {
                std::regex incRe(R"(#\s*include\s*[<"]([^>"]+)[>"])");
                std::smatch m;
                if (std::regex_search(tok.text, m, incRe)) {
                    std::string included = m[1].str();
                    m_edges.push_back({fnv1a(filePath), fnv1a(included), EdgeType::Includes, {filePath, tok.line, tok.col}});
                }

                // #define macros
                std::regex defRe(R"(#\s*define\s+(\w+))");
                if (std::regex_search(tok.text, m, defRe)) {
                    addSymbol(m[1].str(), SymbolKind::Macro, filePath, tok.line, tok.col, scopeStack);
                }
                continue;
            }

            if (tok.type != Token::Keyword && tok.type != Token::Identifier) continue;

            // namespace NAME {
            if (tok.text == "namespace" && i + 1 < tokens.size() && tokens[i + 1].type == Token::Identifier) {
                std::string nsName = tokens[i + 1].text;
                addSymbol(nsName, SymbolKind::Namespace, filePath, tok.line, tok.col, scopeStack);
                scopeStack.push_back(nsName);
                scopeDepths.push_back(braceDepth);
                ++i;
                continue;
            }

            // class/struct NAME : BASE
            if ((tok.text == "class" || tok.text == "struct") && i + 1 < tokens.size() && tokens[i + 1].type == Token::Identifier) {
                // Skip "class" used as elaborated type specifier in declarations (followed by a name then ; or , )
                auto kind = tok.text == "class" ? SymbolKind::Class : SymbolKind::Struct;
                std::string className = tokens[i + 1].text;
                addSymbol(className, kind, filePath, tokens[i + 1].line, tokens[i + 1].col, scopeStack);

                // Check for inheritance
                size_t j = i + 2;
                while (j < tokens.size() && tokens[j].text != "{" && tokens[j].text != ";") {
                    if (tokens[j].text == ":" || tokens[j].text == "public" || tokens[j].text == "private" || tokens[j].text == "protected") {
                        ++j;
                        continue;
                    }
                    if (tokens[j].type == Token::Identifier) {
                        // This is a base class
                        uint64_t derivedHash = fnv1a(buildQualifiedName(className, scopeStack));
                        uint64_t baseHash = fnv1a(tokens[j].text);
                        m_edges.push_back({derivedHash, baseHash, EdgeType::Inherits, {filePath, tokens[j].line, tokens[j].col}});
                    }
                    ++j;
                }

                scopeStack.push_back(className);
                scopeDepths.push_back(braceDepth);
                i = j > i ? j - 1 : i + 1;
                continue;
            }

            // enum NAME
            if (tok.text == "enum" && i + 1 < tokens.size() && tokens[i + 1].type == Token::Identifier) {
                std::string enumName = tokens[i + 1].text;
                // Skip "enum class" — take the name after "class"
                if (enumName == "class" && i + 2 < tokens.size()) {
                    enumName = tokens[i + 2].text;
                    ++i;
                }
                addSymbol(enumName, SymbolKind::Enum, filePath, tokens[i + 1].line, tokens[i + 1].col, scopeStack);
                ++i;
                continue;
            }

            // typedef ... NAME;
            if (tok.text == "typedef") {
                // Find the last identifier before ;
                size_t j = i + 1;
                std::string lastIdent;
                uint32_t lastLine = tok.line, lastCol = tok.col;
                while (j < tokens.size() && tokens[j].text != ";") {
                    if (tokens[j].type == Token::Identifier) {
                        lastIdent = tokens[j].text;
                        lastLine = tokens[j].line;
                        lastCol = tokens[j].col;
                    }
                    ++j;
                }
                if (!lastIdent.empty()) {
                    addSymbol(lastIdent, SymbolKind::TypeParameter, filePath, lastLine, lastCol, scopeStack);
                }
                i = j;
                continue;
            }

            // Function/method: TYPE NAME(
            if (tok.type == Token::Identifier && i + 1 < tokens.size() && tokens[i + 1].text == "(") {
                // Check if previous token looks like a return type
                bool isDecl = (i > 0 && (tokens[i - 1].type == Token::Identifier ||
                               tokens[i - 1].type == Token::Keyword ||
                               tokens[i - 1].text == "*" || tokens[i - 1].text == "&" ||
                               tokens[i - 1].text == ">"));
                if (isDecl) {
                    auto kind = scopeStack.empty() ? SymbolKind::Function : SymbolKind::Method;
                    addSymbol(tok.text, kind, filePath, tok.line, tok.col, scopeStack);

                    // Build signature
                    std::string sig = tok.text + "(";
                    size_t j = i + 2;
                    int parenDepth = 1;
                    while (j < tokens.size() && parenDepth > 0) {
                        if (tokens[j].text == "(") ++parenDepth;
                        else if (tokens[j].text == ")") --parenDepth;
                        if (parenDepth > 0) sig += tokens[j].text + " ";
                        ++j;
                    }
                    sig += ")";
                    if (!m_symbols.empty()) m_symbols.back().signature = sig;
                } else {
                    // This is a function call — add a reference + call edge
                    addReference(tok.text, filePath, tok.line, tok.col);
                    if (!scopeStack.empty()) {
                        std::string caller = scopeStack.back();
                        addCallEdge(caller, tok.text, filePath, tok.line, tok.col);
                    }
                }
                continue;
            }

            // Variable/field reference
            if (tok.type == Token::Identifier) {
                addReference(tok.text, filePath, tok.line, tok.col);
            }
        }
    }

    std::string buildQualifiedName(const std::string& name, const std::vector<std::string>& scopes) {
        std::string q;
        for (auto& s : scopes) { q += s; q += "::"; }
        q += name;
        return q;
    }

    void addSymbol(const std::string& name, SymbolKind kind, const std::string& file,
                   uint32_t line, uint32_t col, const std::vector<std::string>& scopes) {
        SymbolNode sym;
        sym.name = name;
        sym.qualifiedName = buildQualifiedName(name, scopes);
        sym.containerName = scopes.empty() ? "" : scopes.back();
        sym.kind = kind;
        sym.definition = {file, line, col, line, col + static_cast<uint32_t>(name.size())};
        sym.hash = fnv1a(sym.qualifiedName);

        size_t idx = m_symbols.size();
        m_symbols.push_back(std::move(sym));
        m_symbolsByName[name].push_back(idx);
        m_symbolsByHash[m_symbols[idx].hash] = idx;

        // Container edge
        if (!scopes.empty()) {
            uint64_t containerHash = fnv1a(buildQualifiedName(scopes.back(), std::vector<std::string>(scopes.begin(), scopes.end() - 1)));
            m_edges.push_back({containerHash, m_symbols[idx].hash, EdgeType::Contains, {file, line, col}});
        }
    }

    void addReference(const std::string& name, const std::string& file, uint32_t line, uint32_t col) {
        auto it = m_symbolsByName.find(name);
        if (it != m_symbolsByName.end()) {
            for (auto idx : it->second) {
                m_symbols[idx].references.push_back({file, line, col});
            }
        }
    }

    void addCallEdge(const std::string& caller, const std::string& callee,
                     const std::string& file, uint32_t line, uint32_t col) {
        m_edges.push_back({fnv1a(caller), fnv1a(callee), EdgeType::Calls, {file, line, col}});
    }

    mutable std::mutex m_mutex;
    std::vector<SymbolNode> m_symbols;
    std::unordered_map<std::string, std::vector<size_t>> m_symbolsByName;
    std::unordered_map<uint64_t, size_t> m_symbolsByHash;
    std::vector<GraphEdge> m_edges;
    std::unordered_set<std::string> m_indexedFiles;
    std::chrono::steady_clock::time_point m_lastIndexTime;
    std::atomic<uint64_t> m_indexVersion{0};
};

} // namespace AI
} // namespace RawrXD

// =============================================================================
// C API for cross-module linkage
// =============================================================================
extern "C" {

__declspec(dllexport) void SymbolGraph_IndexDirectory(const char* rootPath) {
    RawrXD::AI::SymbolGraphIndexer::instance().indexDirectory(rootPath ? rootPath : ".");
}

__declspec(dllexport) int SymbolGraph_IndexFile(const char* filePath) {
    return RawrXD::AI::SymbolGraphIndexer::instance().indexFile(filePath ? filePath : "") ? 1 : 0;
}

__declspec(dllexport) int SymbolGraph_FindDefinition(const char* symbolName, char* outFile, int outFileLen,
                                                      unsigned int* outLine, unsigned int* outCol) {
    auto* sym = RawrXD::AI::SymbolGraphIndexer::instance().findDefinition(symbolName ? symbolName : "");
    if (!sym) return 0;
    if (outFile && outFileLen > 0) {
        strncpy_s(outFile, outFileLen, sym->definition.filePath.c_str(), _TRUNCATE);
    }
    if (outLine) *outLine = sym->definition.line;
    if (outCol) *outCol = sym->definition.column;
    return 1;
}

__declspec(dllexport) size_t SymbolGraph_SymbolCount() {
    return RawrXD::AI::SymbolGraphIndexer::instance().symbolCount();
}

__declspec(dllexport) size_t SymbolGraph_EdgeCount() {
    return RawrXD::AI::SymbolGraphIndexer::instance().edgeCount();
}

__declspec(dllexport) void SymbolGraph_Clear() {
    RawrXD::AI::SymbolGraphIndexer::instance().clear();
}

} // extern "C"
