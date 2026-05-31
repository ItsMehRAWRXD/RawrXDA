#include "lsp/semantic_tokens_provider.h"
#include <sstream>
#include <regex>
#include <set>

namespace RawrXD::LSP {

SemanticTokensProvider::SemanticTokensProvider() = default;
SemanticTokensProvider::~SemanticTokensProvider() = default;

SemanticTokensLegend SemanticTokensProvider::getLegend() const {
    SemanticTokensLegend legend;

    legend.tokenTypes = {
        "namespace", "type", "class", "enum", "interface", "struct", "typeParameter",
        "parameter", "variable", "property", "enumMember", "event", "function",
        "method", "macro", "keyword", "modifier", "comment", "string", "number",
        "regexp", "operator"
    };

    legend.tokenModifiers = {
        "declaration", "definition", "readonly", "static", "deprecated", "abstract",
        "async", "modification", "documentation", "defaultLibrary"
    };

    return legend;
}

SemanticTokens SemanticTokensProvider::provideSemanticTokens(const std::string& uri,
                                                            const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto tokens = parseTokens(content);
    auto encoded = encodeTokens(tokens);

    SemanticTokens result;
    result.data = encoded;
    result.resultId = std::to_string(++m_resultIdCounter);
    m_resultIds[uri] = *result.resultId;

    return result;
}

SemanticTokens SemanticTokensProvider::provideSemanticTokensRange(const std::string& uri,
                                                                 const std::string& content,
                                                                 const SemanticTokensRange& range) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Extract range content
    auto lines = splitLines(content);
    std::string rangeContent;

    for (uint32_t i = range.startLine; i <= range.endLine && i < lines.size(); ++i) {
        if (i == range.startLine) {
            rangeContent += lines[i].substr(range.startColumn) + "\n";
        } else if (i == range.endLine) {
            rangeContent += lines[i].substr(0, range.endColumn);
        } else {
            rangeContent += lines[i] + "\n";
        }
    }

    auto tokens = parseTokens(rangeContent);
    auto encoded = encodeTokens(tokens);

    SemanticTokens result;
    result.data = encoded;

    return result;
}

SemanticTokens SemanticTokensProvider::provideSemanticTokensDelta(const std::string& uri,
                                                                 const std::string& content,
                                                                 const std::string& previousResultId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto currentTokens = parseTokens(content);

    // For simplicity, return full tokens (proper delta would require storing previous tokens)
    auto encoded = encodeTokens(currentTokens);

    SemanticTokens result;
    result.data = encoded;
    result.resultId = std::to_string(++m_resultIdCounter);

    return result;
}

void SemanticTokensProvider::refresh() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultIds.clear();
    m_resultIdCounter = 0;
}

SemanticTokens SemanticTokensProvider::getCppSemanticTokens(const std::string& uri,
                                                             const std::string& content) {
    return provideSemanticTokens(uri, content);
}

std::vector<SemanticToken> SemanticTokensProvider::parseTokens(const std::string& content) {
    std::vector<SemanticToken> tokens;
    auto lines = splitLines(content);

    for (uint32_t lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const auto& line = lines[lineNum];

        // Skip comments
        if (line.find("//") == 0) {
            SemanticToken token;
            token.line = lineNum;
            token.startColumn = 0;
            token.length = static_cast<uint32_t>(line.length());
            token.tokenType = TokenType::Comment;
            token.tokenModifiers = 0;
            tokens.push_back(token);
            continue;
        }

        // Parse tokens
        std::regex tokenRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\b|"  // Identifiers
                             "(\"[^\"]*\")|"                       // Strings
                             "('[^']*')|"                          // Chars
                             "(0[xX][0-9a-fA-F]+|\\d+\\.\\d+|\\d+)|" // Numbers
                             "(//[^\\n]*)|"                        // Line comments
                             "(/\\*[\\s\\S]*?\\*/)|"                // Block comments
                             "(\\+\\+|--|==|!=|<=|>=|&&|\\|\\||"   // Operators
                             "<<|>>|->|::|\\+|-\\*|/|%|"           // More operators
                             "=|!|~|&|\\||\\^|\\*|/|%|"            // Even more operators
                             "<|>|\\?|:|;|,|\\.|\\[|\\]|"           // Punctuation
                             "\\(|\\)|\\{|\\})");                  // Brackets

        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());

        while (std::regex_search(searchStart, line.cend(), match, tokenRegex)) {
            SemanticToken token;
            token.line = lineNum;
            token.startColumn = static_cast<uint32_t>(match.position());
            token.length = static_cast<uint32_t>(match.length());

            std::string word = match[0];

            // Classify token
            token.tokenType = classifyToken(word, content, lineNum);
            token.tokenModifiers = calculateModifiers(word, content, lineNum);

            tokens.push_back(token);
            searchStart = match.suffix().first;
        }
    }

    return tokens;
}

std::vector<uint32_t> SemanticTokensProvider::encodeTokens(const std::vector<SemanticToken>& tokens) {
    std::vector<uint32_t> encoded;

    uint32_t prevLine = 0;
    uint32_t prevStartChar = 0;

    for (const auto& token : tokens) {
        // Delta encoding
        uint32_t deltaLine = token.line - prevLine;
        uint32_t deltaStartChar = (deltaLine == 0) ?
            (token.startColumn - prevStartChar) : token.startColumn;

        encoded.push_back(deltaLine);
        encoded.push_back(deltaStartChar);
        encoded.push_back(token.length);
        encoded.push_back(static_cast<uint32_t>(token.tokenType));
        encoded.push_back(token.tokenModifiers);

        prevLine = token.line;
        prevStartChar = token.startColumn;
    }

    return encoded;
}

TokenType SemanticTokensProvider::classifyToken(const std::string& word,
                                               const std::string& context,
                                               uint32_t line) {
    // Keywords
    static const std::set<std::string> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
        "public", "register", "reinterpret_cast", "requires", "return", "short",
        "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
        "switch", "template", "this", "thread_local", "throw", "true", "try",
        "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    };

    if (keywords.find(word) != keywords.end()) {
        return TokenType::Keyword;
    }

    // Types
    static const std::set<std::string> types = {
        "int", "long", "short", "char", "bool", "float", "double", "void",
        "size_t", "ptrdiff_t", "nullptr_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t"
    };

    if (types.find(word) != types.end()) {
        return TokenType::Type;
    }

    // Check context for classification
    auto lines = splitLines(context);
    if (line < lines.size()) {
        const auto& currentLine = lines[line];

        // Class/struct definition
        if (currentLine.find("class " + word) != std::string::npos ||
            currentLine.find("struct " + word) != std::string::npos) {
            return TokenType::Class;
        }

        // Function definition
        if (currentLine.find(word + "(") != std::string::npos) {
            // Check if it's a function call or definition
            size_t pos = currentLine.find(word);
            if (pos > 0 && (currentLine[pos - 1] == ' ' || currentLine[pos - 1] == '\t')) {
                // Likely a function definition
                return TokenType::Function;
            }
            return TokenType::Function;
        }

        // Variable declaration
        if (currentLine.find(word + ";") != std::string::npos ||
            currentLine.find(word + " =") != std::string::npos) {
            return TokenType::Variable;
        }
    }

    // Default to variable
    return TokenType::Variable;
}

uint32_t SemanticTokensProvider::calculateModifiers(const std::string& word,
                                                   const std::string& context,
                                                   uint32_t line) {
    uint32_t modifiers = 0;

    auto lines = splitLines(context);
    if (line >= lines.size()) return modifiers;

    const auto& currentLine = lines[line];

    // Check for declaration
    if (isDeclaration(currentLine, word)) {
        modifiers |= (1 << static_cast<uint32_t>(TokenModifier::Declaration));
    }

    // Check for definition
    if (isDefinition(currentLine, word)) {
        modifiers |= (1 << static_cast<uint32_t>(TokenModifier::Definition));
    }

    // Check for static
    if (isStatic(currentLine)) {
        modifiers |= (1 << static_cast<uint32_t>(TokenModifier::Static));
    }

    // Check for const
    if (isConst(currentLine)) {
        modifiers |= (1 << static_cast<uint32_t>(TokenModifier::Readonly));
    }

    return modifiers;
}

bool SemanticTokensProvider::isKeyword(const std::string& word) {
    static const std::set<std::string> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
        "public", "register", "reinterpret_cast", "requires", "return", "short",
        "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
        "switch", "template", "this", "thread_local", "throw", "true", "try",
        "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    };
    return keywords.find(word) != keywords.end();
}

bool SemanticTokensProvider::isType(const std::string& word) {
    static const std::set<std::string> types = {
        "int", "long", "short", "char", "bool", "float", "double", "void",
        "size_t", "ptrdiff_t", "nullptr_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t"
    };
    return types.find(word) != types.end();
}

bool SemanticTokensProvider::isFunction(const std::string& word, const std::string& context) {
    return context.find(word + "(") != std::string::npos;
}

bool SemanticTokensProvider::isVariable(const std::string& word, const std::string& context) {
    return context.find(word + ";") != std::string::npos ||
           context.find(word + " =") != std::string::npos;
}

bool SemanticTokensProvider::isDeclaration(const std::string& line, const std::string& word) {
    // Check if this is the first occurrence (simplified)
    return line.find(word) != std::string::npos;
}

bool SemanticTokensProvider::isDefinition(const std::string& line, const std::string& word) {
    // Check for function body or variable initialization
    return line.find(word + "(") != std::string::npos ||
           line.find(word + " =") != std::string::npos ||
           line.find(word + "{") != std::string::npos;
}

bool SemanticTokensProvider::isStatic(const std::string& line) {
    return line.find("static") != std::string::npos;
}

bool SemanticTokensProvider::isConst(const std::string& line) {
    return line.find("const") != std::string::npos;
}

std::vector<std::string> SemanticTokensProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
SemanticTokensProvider& getSemanticTokensProvider() {
    static SemanticTokensProvider provider;
    return provider;
}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Namespace: return "namespace";
        case TokenType::Type: return "type";
        case TokenType::Class: return "class";
        case TokenType::Enum: return "enum";
        case TokenType::Interface: return "interface";
        case TokenType::Struct: return "struct";
        case TokenType::TypeParameter: return "typeParameter";
        case TokenType::Parameter: return "parameter";
        case TokenType::Variable: return "variable";
        case TokenType::Property: return "property";
        case TokenType::EnumMember: return "enumMember";
        case TokenType::Event: return "event";
        case TokenType::Function: return "function";
        case TokenType::Method: return "method";
        case TokenType::Macro: return "macro";
        case TokenType::Keyword: return "keyword";
        case TokenType::Modifier: return "modifier";
        case TokenType::Comment: return "comment";
        case TokenType::String: return "string";
        case TokenType::Number: return "number";
        case TokenType::Regexp: return "regexp";
        case TokenType::Operator: return "operator";
        default: return "unknown";
    }
}

std::string tokenModifierToString(TokenModifier modifier) {
    switch (modifier) {
        case TokenModifier::Declaration: return "declaration";
        case TokenModifier::Definition: return "definition";
        case TokenModifier::Readonly: return "readonly";
        case TokenModifier::Static: return "static";
        case TokenModifier::Deprecated: return "deprecated";
        case TokenModifier::Abstract: return "abstract";
        case TokenModifier::Async: return "async";
        case TokenModifier::Modification: return "modification";
        case TokenModifier::Documentation: return "documentation";
        case TokenModifier::DefaultLibrary: return "defaultLibrary";
        default: return "unknown";
    }
}

} // namespace RawrXD::LSP
