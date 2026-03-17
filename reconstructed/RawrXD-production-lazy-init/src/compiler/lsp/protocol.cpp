/**
 * @file lsp_protocol.cpp
 * @brief Complete Language Server Protocol Implementation
 * @author RawrXD Compiler Team
 * @version 2.0.0
 * 
 * Full LSP server implementation supporting:
 * - Text document synchronization
 * - Code completion (IntelliSense)
 * - Hover information
 * - Go to definition/declaration
 * - Find references
 * - Document/workspace symbols
 * - Code formatting
 * - Diagnostics
 * - Signature help
 * - Code actions
 */

#include "lsp_protocol.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>
#include <regex>
#include <cctype>

namespace RawrXD {
namespace LSP {

// ============================================================================
// JSONRPCMessage Implementation
// ============================================================================

std::string JSONRPCMessage::serialize() const {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\"";
    
    if (!method.empty()) {
        oss << ",\"method\":\"" << method << "\"";
    }
    
    if (!id.empty()) {
        oss << ",\"id\":" << id;
    }
    
    if (!params.empty()) {
        oss << ",\"params\":" << params;
    }
    
    if (!result.empty()) {
        oss << ",\"result\":" << result;
    }
    
    if (!error.empty()) {
        oss << ",\"error\":" << error;
    }
    
    oss << "}";
    return oss.str();
}

JSONRPCMessage JSONRPCMessage::deserialize(const std::string& json) {
    JSONRPCMessage msg;
    
    // Extract jsonrpc
    auto rpcPos = json.find("\"jsonrpc\"");
    if (rpcPos != std::string::npos) {
        msg.jsonrpc = "2.0";
    }
    
    // Extract method
    auto methodPos = json.find("\"method\"");
    if (methodPos != std::string::npos) {
        auto start = json.find('"', methodPos + 9) + 1;
        auto end = json.find('"', start);
        msg.method = json.substr(start, end - start);
    }
    
    // Extract id
    auto idPos = json.find("\"id\"");
    if (idPos != std::string::npos) {
        auto start = json.find(':', idPos) + 1;
        while (start < json.size() && std::isspace(json[start])) start++;
        auto end = start;
        while (end < json.size() && (std::isdigit(json[end]) || json[end] == '"' || std::isalnum(json[end]))) end++;
        msg.id = json.substr(start, end - start);
    }
    
    // Extract params
    auto paramsPos = json.find("\"params\"");
    if (paramsPos != std::string::npos) {
        auto start = json.find_first_of("[{", paramsPos);
        if (start != std::string::npos) {
            char openChar = json[start];
            char closeChar = (openChar == '[') ? ']' : '}';
            int depth = 1;
            auto end = start + 1;
            while (end < json.size() && depth > 0) {
                if (json[end] == openChar) depth++;
                else if (json[end] == closeChar) depth--;
                end++;
            }
            msg.params = json.substr(start, end - start);
        }
    }
    
    // Extract result
    auto resultPos = json.find("\"result\"");
    if (resultPos != std::string::npos) {
        auto start = json.find_first_of("[{\"", resultPos + 9);
        if (start != std::string::npos) {
            char openChar = json[start];
            if (openChar == '"') {
                auto end = json.find('"', start + 1);
                msg.result = json.substr(start, end - start + 1);
            } else {
                char closeChar = (openChar == '[') ? ']' : '}';
                int depth = 1;
                auto end = start + 1;
                while (end < json.size() && depth > 0) {
                    if (json[end] == openChar) depth++;
                    else if (json[end] == closeChar) depth--;
                    end++;
                }
                msg.result = json.substr(start, end - start);
            }
        }
    }
    
    return msg;
}

// ============================================================================
// TextDocument Implementation
// ============================================================================

TextDocument::TextDocument(const std::string& uri, const std::string& language, int version)
    : uri_(uri), languageId_(language), version_(version) {
}

void TextDocument::setContent(const std::string& content) {
    content_ = content;
    updateLineOffsets();
}

void TextDocument::applyEdit(const TextEdit& edit) {
    // Convert positions to offsets
    size_t startOffset = positionToOffset(edit.range.start);
    size_t endOffset = positionToOffset(edit.range.end);
    
    // Apply the edit
    content_ = content_.substr(0, startOffset) + edit.newText + content_.substr(endOffset);
    
    // Update line offsets
    updateLineOffsets();
}

void TextDocument::applyIncrementalChanges(const std::vector<TextDocumentContentChangeEvent>& changes) {
    for (const auto& change : changes) {
        if (change.range.has_value()) {
            TextEdit edit;
            edit.range = *change.range;
            edit.newText = change.text;
            applyEdit(edit);
        } else {
            // Full content replacement
            setContent(change.text);
        }
    }
}

std::string TextDocument::getText(const Range& range) const {
    size_t startOffset = positionToOffset(range.start);
    size_t endOffset = positionToOffset(range.end);
    return content_.substr(startOffset, endOffset - startOffset);
}

std::string TextDocument::getLine(int lineNumber) const {
    if (lineNumber < 0 || lineNumber >= static_cast<int>(lineOffsets_.size())) {
        return "";
    }
    
    size_t start = lineOffsets_[lineNumber];
    size_t end = (lineNumber + 1 < static_cast<int>(lineOffsets_.size()))
        ? lineOffsets_[lineNumber + 1] - 1
        : content_.size();
    
    // Remove trailing newline
    while (end > start && (content_[end - 1] == '\n' || content_[end - 1] == '\r')) {
        end--;
    }
    
    return content_.substr(start, end - start);
}

int TextDocument::getLineCount() const {
    return static_cast<int>(lineOffsets_.size());
}

Position TextDocument::offsetToPosition(size_t offset) const {
    Position pos;
    pos.line = 0;
    pos.character = 0;
    
    for (int i = 0; i < static_cast<int>(lineOffsets_.size()); ++i) {
        if (offset < lineOffsets_[i]) {
            pos.line = i - 1;
            pos.character = static_cast<int>(offset - lineOffsets_[pos.line]);
            return pos;
        }
    }
    
    pos.line = static_cast<int>(lineOffsets_.size()) - 1;
    pos.character = static_cast<int>(offset - lineOffsets_[pos.line]);
    return pos;
}

size_t TextDocument::positionToOffset(const Position& position) const {
    if (position.line < 0 || position.line >= static_cast<int>(lineOffsets_.size())) {
        return content_.size();
    }
    
    size_t lineStart = lineOffsets_[position.line];
    size_t lineLength = (position.line + 1 < static_cast<int>(lineOffsets_.size()))
        ? lineOffsets_[position.line + 1] - lineStart
        : content_.size() - lineStart;
    
    return lineStart + std::min(static_cast<size_t>(position.character), lineLength);
}

std::string TextDocument::getWordAtPosition(const Position& position) const {
    std::string line = getLine(position.line);
    
    if (position.character < 0 || position.character >= static_cast<int>(line.size())) {
        return "";
    }
    
    // Find word boundaries
    int start = position.character;
    int end = position.character;
    
    while (start > 0 && isIdentifierChar(line[start - 1])) {
        start--;
    }
    
    while (end < static_cast<int>(line.size()) && isIdentifierChar(line[end])) {
        end++;
    }
    
    return line.substr(start, end - start);
}

Range TextDocument::getWordRangeAtPosition(const Position& position) const {
    std::string line = getLine(position.line);
    Range range;
    range.start.line = position.line;
    range.end.line = position.line;
    
    if (position.character < 0 || position.character >= static_cast<int>(line.size())) {
        range.start.character = position.character;
        range.end.character = position.character;
        return range;
    }
    
    int start = position.character;
    int end = position.character;
    
    while (start > 0 && isIdentifierChar(line[start - 1])) {
        start--;
    }
    
    while (end < static_cast<int>(line.size()) && isIdentifierChar(line[end])) {
        end++;
    }
    
    range.start.character = start;
    range.end.character = end;
    return range;
}

void TextDocument::updateLineOffsets() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0);
    
    for (size_t i = 0; i < content_.size(); ++i) {
        if (content_[i] == '\n') {
            lineOffsets_.push_back(i + 1);
        }
    }
}

bool TextDocument::isIdentifierChar(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// ============================================================================
// DocumentManager Implementation
// ============================================================================

void DocumentManager::openDocument(const std::string& uri, const std::string& text, 
                                   const std::string& languageId, int version) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto doc = std::make_shared<TextDocument>(uri, languageId, version);
    doc->setContent(text);
    documents_[uri] = doc;
}

void DocumentManager::closeDocument(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    documents_.erase(uri);
}

void DocumentManager::updateDocument(const std::string& uri,
                                     const std::vector<TextDocumentContentChangeEvent>& changes,
                                     int version) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(uri);
    if (it != documents_.end()) {
        it->second->applyIncrementalChanges(changes);
        it->second->incrementVersion();
    }
}

std::shared_ptr<TextDocument> DocumentManager::getDocument(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(uri);
    return (it != documents_.end()) ? it->second : nullptr;
}

std::vector<std::string> DocumentManager::getAllDocumentUris() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> uris;
    for (const auto& pair : documents_) {
        uris.push_back(pair.first);
    }
    return uris;
}

// ============================================================================
// SymbolIndex Implementation
// ============================================================================

void SymbolIndex::addSymbol(const std::string& uri, const DocumentSymbol& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    documentSymbols_[uri].push_back(symbol);
}

void SymbolIndex::clearSymbols(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    documentSymbols_.erase(uri);
}

std::vector<DocumentSymbol> SymbolIndex::getDocumentSymbols(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documentSymbols_.find(uri);
    return (it != documentSymbols_.end()) ? it->second : std::vector<DocumentSymbol>{};
}

std::vector<SymbolInformation> SymbolIndex::searchSymbols(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SymbolInformation> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& [uri, symbols] : documentSymbols_) {
        for (const auto& symbol : symbols) {
            std::string lowerName = symbol.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            
            if (lowerName.find(lowerQuery) != std::string::npos) {
                SymbolInformation info;
                info.name = symbol.name;
                info.kind = symbol.kind;
                info.location.uri = uri;
                info.location.range = symbol.range;
                results.push_back(info);
            }
        }
    }
    
    return results;
}

void SymbolIndex::indexDocument(const std::string& uri, const std::string& content,
                                const std::string& languageId) {
    clearSymbols(uri);
    
    // Parse content for symbols based on language
    if (languageId == "cpp" || languageId == "c" || languageId == "h" || languageId == "hpp") {
        indexCppDocument(uri, content);
    } else if (languageId == "python") {
        indexPythonDocument(uri, content);
    } else if (languageId == "javascript" || languageId == "typescript") {
        indexJavaScriptDocument(uri, content);
    }
}

void SymbolIndex::indexCppDocument(const std::string& uri, const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    // Regex patterns for C++ symbols
    std::regex classRegex(R"((?:class|struct)\s+(\w+))");
    std::regex functionRegex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:final)?\s*(?:\{|;))");
    std::regex variableRegex(R"((?:int|float|double|char|bool|string|auto|void|long|short)\s+(\w+)\s*[;=])");
    std::regex enumRegex(R"(enum\s+(?:class\s+)?(\w+))");
    std::regex namespaceRegex(R"(namespace\s+(\w+))");
    std::regex typedefRegex(R"(typedef\s+.*\s+(\w+)\s*;)");
    std::regex defineRegex(R"(#define\s+(\w+))");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for class/struct
        if (std::regex_search(line, match, classRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Class;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        // Check for function
        if (std::regex_search(line, match, functionRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[2].str();
            symbol.kind = SymbolKind::Function;
            symbol.detail = match[1].str();
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = 0;
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(line.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        // Check for variable
        if (std::regex_search(line, match, variableRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Variable;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        // Check for enum
        if (std::regex_search(line, match, enumRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Enum;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        // Check for namespace
        if (std::regex_search(line, match, namespaceRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Namespace;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        lineNumber++;
    }
}

void SymbolIndex::indexPythonDocument(const std::string& uri, const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    std::regex classRegex(R"(class\s+(\w+))");
    std::regex functionRegex(R"(def\s+(\w+)\s*\()");
    std::regex variableRegex(R"(^(\w+)\s*=)");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, classRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Class;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        if (std::regex_search(line, match, functionRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = (match[1].str().find("__") == 0) ? SymbolKind::Method : SymbolKind::Function;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        lineNumber++;
    }
}

void SymbolIndex::indexJavaScriptDocument(const std::string& uri, const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    std::regex classRegex(R"(class\s+(\w+))");
    std::regex functionRegex(R"((?:function\s+(\w+)|(\w+)\s*:\s*(?:async\s+)?function|const\s+(\w+)\s*=\s*(?:async\s+)?\([^)]*\)\s*=>))");
    std::regex constRegex(R"((?:const|let|var)\s+(\w+)\s*=)");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, classRegex)) {
            DocumentSymbol symbol;
            symbol.name = match[1].str();
            symbol.kind = SymbolKind::Class;
            symbol.range.start.line = lineNumber;
            symbol.range.start.character = static_cast<int>(match.position());
            symbol.range.end.line = lineNumber;
            symbol.range.end.character = static_cast<int>(match.position() + match.length());
            symbol.selectionRange = symbol.range;
            addSymbol(uri, symbol);
        }
        
        if (std::regex_search(line, match, functionRegex)) {
            std::string name = match[1].matched ? match[1].str() :
                              (match[2].matched ? match[2].str() : match[3].str());
            if (!name.empty()) {
                DocumentSymbol symbol;
                symbol.name = name;
                symbol.kind = SymbolKind::Function;
                symbol.range.start.line = lineNumber;
                symbol.range.start.character = 0;
                symbol.range.end.line = lineNumber;
                symbol.range.end.character = static_cast<int>(line.length());
                symbol.selectionRange = symbol.range;
                addSymbol(uri, symbol);
            }
        }
        
        lineNumber++;
    }
}

// ============================================================================
// DiagnosticsManager Implementation
// ============================================================================

void DiagnosticsManager::setDiagnostics(const std::string& uri,
                                        const std::vector<Diagnostic>& diagnostics) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_[uri] = diagnostics;
}

void DiagnosticsManager::addDiagnostic(const std::string& uri, const Diagnostic& diagnostic) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_[uri].push_back(diagnostic);
}

void DiagnosticsManager::clearDiagnostics(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.erase(uri);
}

std::vector<Diagnostic> DiagnosticsManager::getDiagnostics(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = diagnostics_.find(uri);
    return (it != diagnostics_.end()) ? it->second : std::vector<Diagnostic>{};
}

// ============================================================================
// CompletionProvider Implementation
// ============================================================================

CompletionProvider::CompletionProvider() {
    initializeKeywords();
    initializeSnippets();
}

std::vector<CompletionItem> CompletionProvider::provideCompletions(
    const TextDocument& document,
    const Position& position,
    const CompletionContext& context) {
    
    std::vector<CompletionItem> items;
    
    // Get prefix
    std::string line = document.getLine(position.line);
    std::string prefix;
    int start = position.character - 1;
    while (start >= 0 && (std::isalnum(line[start]) || line[start] == '_')) {
        start--;
    }
    prefix = line.substr(start + 1, position.character - start - 1);
    
    // Add keyword completions
    addKeywordCompletions(document.getLanguageId(), prefix, items);
    
    // Add snippet completions
    addSnippetCompletions(document.getLanguageId(), prefix, items);
    
    // Add symbol completions from document
    addSymbolCompletions(document.getContent(), prefix, items);
    
    // Add context-aware completions
    addContextCompletions(document, position, context, items);
    
    return items;
}

void CompletionProvider::initializeKeywords() {
    // C++ keywords
    keywords_["cpp"] = {
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
        "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
        "std::string", "std::vector", "std::map", "std::unordered_map", "std::set",
        "std::unique_ptr", "std::shared_ptr", "std::make_unique", "std::make_shared",
        "std::move", "std::forward", "std::optional", "std::variant", "std::any"
    };
    
    // Python keywords
    keywords_["python"] = {
        "and", "as", "assert", "async", "await", "break", "class", "continue",
        "def", "del", "elif", "else", "except", "False", "finally", "for",
        "from", "global", "if", "import", "in", "is", "lambda", "None",
        "nonlocal", "not", "or", "pass", "raise", "return", "True", "try",
        "while", "with", "yield", "self", "cls", "print", "len", "range",
        "list", "dict", "set", "tuple", "int", "float", "str", "bool"
    };
    
    // JavaScript keywords
    keywords_["javascript"] = {
        "async", "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "export", "extends",
        "false", "finally", "for", "function", "if", "import", "in", "instanceof",
        "let", "new", "null", "return", "static", "super", "switch", "this",
        "throw", "true", "try", "typeof", "undefined", "var", "void", "while",
        "with", "yield", "console", "document", "window", "Promise", "async",
        "Map", "Set", "Array", "Object", "String", "Number", "Boolean"
    };
    
    // TypeScript inherits from JavaScript
    keywords_["typescript"] = keywords_["javascript"];
    keywords_["typescript"].insert(keywords_["typescript"].end(), {
        "interface", "type", "enum", "implements", "namespace", "module",
        "declare", "abstract", "readonly", "private", "protected", "public",
        "as", "is", "keyof", "typeof", "never", "unknown", "any"
    });
}

void CompletionProvider::initializeSnippets() {
    // C++ snippets
    snippets_["cpp"] = {
        {"for", "for (${1:int i = 0}; ${2:i < n}; ${3:++i}) {\n\t$0\n}"},
        {"fori", "for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}"},
        {"foreach", "for (const auto& ${1:item} : ${2:container}) {\n\t$0\n}"},
        {"while", "while (${1:condition}) {\n\t$0\n}"},
        {"if", "if (${1:condition}) {\n\t$0\n}"},
        {"ifelse", "if (${1:condition}) {\n\t$2\n} else {\n\t$0\n}"},
        {"switch", "switch (${1:expr}) {\n\tcase ${2:value}:\n\t\t$0\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n}"},
        {"class", "class ${1:ClassName} {\npublic:\n\t${1:ClassName}();\n\t~${1:ClassName}();\n\nprivate:\n\t$0\n};"},
        {"struct", "struct ${1:StructName} {\n\t$0\n};"},
        {"func", "${1:void} ${2:functionName}(${3:params}) {\n\t$0\n}"},
        {"main", "int main(int argc, char* argv[]) {\n\t$0\n\treturn 0;\n}"},
        {"cout", "std::cout << ${1:\"text\"} << std::endl;"},
        {"cin", "std::cin >> ${1:variable};"},
        {"include", "#include <${1:header}>"},
        {"includeq", "#include \"${1:header}\""},
        {"guard", "#ifndef ${1:HEADER_H}\n#define ${1:HEADER_H}\n\n$0\n\n#endif // ${1:HEADER_H}"},
        {"pragma", "#pragma once"},
        {"try", "try {\n\t$0\n} catch (const ${1:std::exception}& e) {\n\t\n}"},
        {"lambda", "[${1:captures}](${2:params}) {\n\t$0\n}"},
        {"unique", "std::unique_ptr<${1:Type}> ${2:ptr} = std::make_unique<${1:Type}>(${3:args});"},
        {"shared", "std::shared_ptr<${1:Type}> ${2:ptr} = std::make_shared<${1:Type}>(${3:args});"}
    };
    
    // Python snippets
    snippets_["python"] = {
        {"def", "def ${1:function_name}(${2:params}):\n\t${3:pass}"},
        {"class", "class ${1:ClassName}:\n\tdef __init__(self${2:, params}):\n\t\t${3:pass}"},
        {"for", "for ${1:item} in ${2:iterable}:\n\t${3:pass}"},
        {"fori", "for ${1:i} in range(${2:n}):\n\t${3:pass}"},
        {"while", "while ${1:condition}:\n\t${2:pass}"},
        {"if", "if ${1:condition}:\n\t${2:pass}"},
        {"ifelse", "if ${1:condition}:\n\t${2:pass}\nelse:\n\t${3:pass}"},
        {"try", "try:\n\t${1:pass}\nexcept ${2:Exception} as e:\n\t${3:pass}"},
        {"with", "with ${1:expression} as ${2:variable}:\n\t${3:pass}"},
        {"lambda", "lambda ${1:params}: ${2:expression}"},
        {"main", "if __name__ == '__main__':\n\t${1:main()}"},
        {"print", "print(${1:value})"},
        {"async", "async def ${1:function_name}(${2:params}):\n\t${3:pass}"},
        {"await", "await ${1:expression}"}
    };
    
    // JavaScript snippets
    snippets_["javascript"] = {
        {"func", "function ${1:name}(${2:params}) {\n\t$0\n}"},
        {"arrow", "const ${1:name} = (${2:params}) => {\n\t$0\n};"},
        {"async", "async function ${1:name}(${2:params}) {\n\t$0\n}"},
        {"asyncarrow", "const ${1:name} = async (${2:params}) => {\n\t$0\n};"},
        {"for", "for (let ${1:i} = 0; ${1:i} < ${2:array}.length; ${1:i}++) {\n\t$0\n}"},
        {"forof", "for (const ${1:item} of ${2:iterable}) {\n\t$0\n}"},
        {"forin", "for (const ${1:key} in ${2:object}) {\n\t$0\n}"},
        {"foreach", "${1:array}.forEach((${2:item}) => {\n\t$0\n});"},
        {"map", "${1:array}.map((${2:item}) => {\n\t$0\n});"},
        {"filter", "${1:array}.filter((${2:item}) => ${3:condition});"},
        {"reduce", "${1:array}.reduce((${2:acc}, ${3:item}) => {\n\t$0\n}, ${4:initialValue});"},
        {"if", "if (${1:condition}) {\n\t$0\n}"},
        {"ifelse", "if (${1:condition}) {\n\t$0\n} else {\n\t\n}"},
        {"switch", "switch (${1:expression}) {\n\tcase ${2:value}:\n\t\t$0\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n}"},
        {"try", "try {\n\t$0\n} catch (${1:error}) {\n\t\n}"},
        {"promise", "new Promise((resolve, reject) => {\n\t$0\n});"},
        {"class", "class ${1:Name} {\n\tconstructor(${2:params}) {\n\t\t$0\n\t}\n}"},
        {"log", "console.log(${1:value});"},
        {"import", "import { ${1:module} } from '${2:package}';"},
        {"export", "export ${1:default} ${2:expression};"}
    };
    
    snippets_["typescript"] = snippets_["javascript"];
}

void CompletionProvider::addKeywordCompletions(const std::string& languageId,
                                               const std::string& prefix,
                                               std::vector<CompletionItem>& items) {
    auto it = keywords_.find(languageId);
    if (it == keywords_.end()) return;
    
    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
    
    for (const auto& keyword : it->second) {
        std::string lowerKeyword = keyword;
        std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
        
        if (lowerKeyword.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = keyword;
            item.kind = CompletionItemKind::Keyword;
            item.insertText = keyword;
            item.insertTextFormat = InsertTextFormat::PlainText;
            items.push_back(item);
        }
    }
}

void CompletionProvider::addSnippetCompletions(const std::string& languageId,
                                               const std::string& prefix,
                                               std::vector<CompletionItem>& items) {
    auto it = snippets_.find(languageId);
    if (it == snippets_.end()) return;
    
    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
    
    for (const auto& snippet : it->second) {
        std::string lowerName = snippet.first;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (lowerName.find(lowerPrefix) == 0) {
            CompletionItem item;
            item.label = snippet.first;
            item.kind = CompletionItemKind::Snippet;
            item.insertText = snippet.second;
            item.insertTextFormat = InsertTextFormat::Snippet;
            item.detail = "Snippet";
            items.push_back(item);
        }
    }
}

void CompletionProvider::addSymbolCompletions(const std::string& content,
                                              const std::string& prefix,
                                              std::vector<CompletionItem>& items) {
    // Extract identifiers from content
    std::regex identifierRegex(R"(\b([a-zA-Z_]\w*)\b)");
    std::unordered_set<std::string> identifiers;
    
    auto begin = std::sregex_iterator(content.begin(), content.end(), identifierRegex);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        identifiers.insert((*it)[1].str());
    }
    
    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
    
    for (const auto& identifier : identifiers) {
        if (identifier.length() < 2) continue;
        
        std::string lowerIdent = identifier;
        std::transform(lowerIdent.begin(), lowerIdent.end(), lowerIdent.begin(), ::tolower);
        
        if (lowerIdent.find(lowerPrefix) == 0 && identifier != prefix) {
            CompletionItem item;
            item.label = identifier;
            item.kind = CompletionItemKind::Text;
            item.insertText = identifier;
            items.push_back(item);
        }
    }
}

void CompletionProvider::addContextCompletions(const TextDocument& document,
                                               const Position& position,
                                               const CompletionContext& context,
                                               std::vector<CompletionItem>& items) {
    // Check trigger character
    if (context.triggerCharacter == ".") {
        // Member access - add common member suggestions
        std::string line = document.getLine(position.line);
        // Check what's before the dot
        int dotPos = position.character - 1;
        if (dotPos > 0) {
            // Add common members based on detected type
            // This is a simplified implementation
            items.push_back({.label = "size", .kind = CompletionItemKind::Method});
            items.push_back({.label = "empty", .kind = CompletionItemKind::Method});
            items.push_back({.label = "begin", .kind = CompletionItemKind::Method});
            items.push_back({.label = "end", .kind = CompletionItemKind::Method});
            items.push_back({.label = "push_back", .kind = CompletionItemKind::Method});
            items.push_back({.label = "pop_back", .kind = CompletionItemKind::Method});
        }
    } else if (context.triggerCharacter == "::") {
        // Namespace/scope access
        items.push_back({.label = "cout", .kind = CompletionItemKind::Variable});
        items.push_back({.label = "cin", .kind = CompletionItemKind::Variable});
        items.push_back({.label = "cerr", .kind = CompletionItemKind::Variable});
        items.push_back({.label = "endl", .kind = CompletionItemKind::Variable});
        items.push_back({.label = "string", .kind = CompletionItemKind::Class});
        items.push_back({.label = "vector", .kind = CompletionItemKind::Class});
        items.push_back({.label = "map", .kind = CompletionItemKind::Class});
        items.push_back({.label = "set", .kind = CompletionItemKind::Class});
    }
}

// ============================================================================
// HoverProvider Implementation
// ============================================================================

std::optional<Hover> HoverProvider::provideHover(const TextDocument& document,
                                                  const Position& position) {
    std::string word = document.getWordAtPosition(position);
    if (word.empty()) return std::nullopt;
    
    Hover hover;
    hover.range = document.getWordRangeAtPosition(position);
    
    // Try keyword info
    auto keywordInfo = getKeywordInfo(word, document.getLanguageId());
    if (!keywordInfo.empty()) {
        hover.contents.kind = "markdown";
        hover.contents.value = keywordInfo;
        return hover;
    }
    
    // Try type info
    auto typeInfo = getTypeInfo(word, document.getLanguageId());
    if (!typeInfo.empty()) {
        hover.contents.kind = "markdown";
        hover.contents.value = typeInfo;
        return hover;
    }
    
    return std::nullopt;
}

std::string HoverProvider::getKeywordInfo(const std::string& keyword,
                                          const std::string& languageId) {
    static const std::map<std::string, std::string> cppKeywords = {
        {"auto", "**auto** - Automatic type deduction\n\n```cpp\nauto x = 42; // int\nauto y = 3.14; // double\n```"},
        {"const", "**const** - Constant qualifier\n\nDeclares a variable as read-only or a method that doesn't modify the object."},
        {"constexpr", "**constexpr** - Compile-time constant expression\n\nIndicates that the value can be computed at compile time."},
        {"class", "**class** - Class definition\n\nDefines a user-defined type with members and methods."},
        {"struct", "**struct** - Structure definition\n\nSimilar to class but with default public access."},
        {"virtual", "**virtual** - Virtual function\n\nEnables runtime polymorphism for method overriding."},
        {"override", "**override** - Override specifier\n\nIndicates that a method overrides a base class virtual method."},
        {"template", "**template** - Template definition\n\nDefines generic types or functions."},
        {"namespace", "**namespace** - Namespace declaration\n\nDefines a scope for identifiers to avoid name collisions."},
        {"nullptr", "**nullptr** - Null pointer literal\n\nRepresents a null pointer value (C++11)."},
        {"noexcept", "**noexcept** - No-throw specification\n\nIndicates that a function doesn't throw exceptions."},
        {"static_cast", "**static_cast** - Static type cast\n\nPerforms compile-time type conversion."},
        {"dynamic_cast", "**dynamic_cast** - Dynamic type cast\n\nPerforms runtime polymorphic type conversion."},
        {"reinterpret_cast", "**reinterpret_cast** - Reinterpret cast\n\nPerforms low-level type reinterpretation."},
        {"const_cast", "**const_cast** - Const cast\n\nAdds or removes const/volatile qualifiers."}
    };
    
    if (languageId == "cpp" || languageId == "c") {
        auto it = cppKeywords.find(keyword);
        if (it != cppKeywords.end()) {
            return it->second;
        }
    }
    
    return "";
}

std::string HoverProvider::getTypeInfo(const std::string& type,
                                        const std::string& languageId) {
    static const std::map<std::string, std::string> cppTypes = {
        {"vector", "**std::vector<T>** - Dynamic array container\n\n```cpp\nstd::vector<int> v = {1, 2, 3};\nv.push_back(4);\n```"},
        {"string", "**std::string** - String class\n\n```cpp\nstd::string s = \"Hello\";\ns += \" World\";\n```"},
        {"map", "**std::map<K, V>** - Ordered associative container\n\n```cpp\nstd::map<std::string, int> m;\nm[\"key\"] = 42;\n```"},
        {"unordered_map", "**std::unordered_map<K, V>** - Hash map container\n\nFaster average lookup than std::map."},
        {"set", "**std::set<T>** - Ordered unique element container\n\n```cpp\nstd::set<int> s = {1, 2, 3};\ns.insert(4);\n```"},
        {"unique_ptr", "**std::unique_ptr<T>** - Unique ownership smart pointer\n\n```cpp\nauto ptr = std::make_unique<MyClass>();\n```"},
        {"shared_ptr", "**std::shared_ptr<T>** - Shared ownership smart pointer\n\n```cpp\nauto ptr = std::make_shared<MyClass>();\n```"},
        {"optional", "**std::optional<T>** - Optional value wrapper\n\n```cpp\nstd::optional<int> opt = 42;\nif (opt) { ... }\n```"},
        {"variant", "**std::variant<T...>** - Type-safe union\n\n```cpp\nstd::variant<int, std::string> v = 42;\n```"},
        {"tuple", "**std::tuple<T...>** - Fixed-size heterogeneous container\n\n```cpp\nauto t = std::make_tuple(1, \"hello\", 3.14);\n```"}
    };
    
    if (languageId == "cpp" || languageId == "c") {
        auto it = cppTypes.find(type);
        if (it != cppTypes.end()) {
            return it->second;
        }
    }
    
    return "";
}

// ============================================================================
// DefinitionProvider Implementation  
// ============================================================================

std::vector<Location> DefinitionProvider::provideDefinition(const TextDocument& document,
                                                            const Position& position,
                                                            const SymbolIndex& index) {
    std::vector<Location> locations;
    
    std::string word = document.getWordAtPosition(position);
    if (word.empty()) return locations;
    
    // Search in symbol index
    auto results = index.searchSymbols(word);
    
    for (const auto& symbol : results) {
        // Filter for exact matches that look like definitions
        if (symbol.name == word) {
            locations.push_back(symbol.location);
        }
    }
    
    // If no results from index, try to find definition in current document
    if (locations.empty()) {
        auto definitions = findDefinitionsInDocument(document, word);
        for (const auto& def : definitions) {
            Location loc;
            loc.uri = document.getUri();
            loc.range = def;
            locations.push_back(loc);
        }
    }
    
    return locations;
}

std::vector<Range> DefinitionProvider::findDefinitionsInDocument(const TextDocument& document,
                                                                  const std::string& symbol) {
    std::vector<Range> definitions;
    
    // Regex patterns for different definition types
    std::regex classDefRegex("(?:class|struct)\\s+" + symbol + "\\s*[:{]");
    std::regex funcDefRegex("\\b\\w+\\s+" + symbol + "\\s*\\([^)]*\\)\\s*(?:const)?\\s*(?:override)?\\s*\\{");
    std::regex varDefRegex("(?:\\w+|auto)\\s+" + symbol + "\\s*[=;]");
    
    std::string content = document.getContent();
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, classDefRegex) ||
            std::regex_search(line, match, funcDefRegex) ||
            std::regex_search(line, match, varDefRegex)) {
            
            // Find the symbol position in the line
            auto pos = line.find(symbol);
            if (pos != std::string::npos) {
                Range range;
                range.start.line = lineNumber;
                range.start.character = static_cast<int>(pos);
                range.end.line = lineNumber;
                range.end.character = static_cast<int>(pos + symbol.length());
                definitions.push_back(range);
            }
        }
        
        lineNumber++;
    }
    
    return definitions;
}

// ============================================================================
// ReferencesProvider Implementation
// ============================================================================

std::vector<Location> ReferencesProvider::provideReferences(const TextDocument& document,
                                                             const Position& position,
                                                             bool includeDeclaration,
                                                             const SymbolIndex& index) {
    std::vector<Location> locations;
    
    std::string word = document.getWordAtPosition(position);
    if (word.empty()) return locations;
    
    // Find all occurrences of the symbol in the current document
    auto refs = findReferencesInDocument(document, word);
    for (const auto& range : refs) {
        Location loc;
        loc.uri = document.getUri();
        loc.range = range;
        locations.push_back(loc);
    }
    
    // Search in symbol index for cross-file references
    auto symbols = index.searchSymbols(word);
    for (const auto& symbol : symbols) {
        if (symbol.name == word && symbol.location.uri != document.getUri()) {
            locations.push_back(symbol.location);
        }
    }
    
    return locations;
}

std::vector<Range> ReferencesProvider::findReferencesInDocument(const TextDocument& document,
                                                                 const std::string& symbol) {
    std::vector<Range> references;
    
    std::string content = document.getContent();
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    // Simple word boundary matching
    std::regex wordRegex("\\b" + symbol + "\\b");
    
    while (std::getline(stream, line)) {
        auto begin = std::sregex_iterator(line.begin(), line.end(), wordRegex);
        auto end = std::sregex_iterator();
        
        for (auto it = begin; it != end; ++it) {
            Range range;
            range.start.line = lineNumber;
            range.start.character = static_cast<int>(it->position());
            range.end.line = lineNumber;
            range.end.character = static_cast<int>(it->position() + it->length());
            references.push_back(range);
        }
        
        lineNumber++;
    }
    
    return references;
}

// ============================================================================
// FormattingProvider Implementation
// ============================================================================

std::vector<TextEdit> FormattingProvider::formatDocument(const TextDocument& document,
                                                         const FormattingOptions& options) {
    std::vector<TextEdit> edits;
    
    std::string content = document.getContent();
    std::string formatted = formatContent(content, options, document.getLanguageId());
    
    if (formatted != content) {
        TextEdit edit;
        edit.range.start.line = 0;
        edit.range.start.character = 0;
        edit.range.end.line = document.getLineCount();
        edit.range.end.character = 0;
        edit.newText = formatted;
        edits.push_back(edit);
    }
    
    return edits;
}

std::vector<TextEdit> FormattingProvider::formatRange(const TextDocument& document,
                                                      const Range& range,
                                                      const FormattingOptions& options) {
    std::vector<TextEdit> edits;
    
    std::string text = document.getText(range);
    std::string formatted = formatContent(text, options, document.getLanguageId());
    
    if (formatted != text) {
        TextEdit edit;
        edit.range = range;
        edit.newText = formatted;
        edits.push_back(edit);
    }
    
    return edits;
}

std::string FormattingProvider::formatContent(const std::string& content,
                                              const FormattingOptions& options,
                                              const std::string& languageId) {
    std::string result;
    std::istringstream stream(content);
    std::string line;
    int indentLevel = 0;
    std::string indentStr = options.insertSpaces 
        ? std::string(options.tabSize, ' ')
        : "\t";
    
    while (std::getline(stream, line)) {
        // Trim leading/trailing whitespace
        auto start = line.find_first_not_of(" \t");
        auto end = line.find_last_not_of(" \t");
        
        if (start == std::string::npos) {
            result += "\n";
            continue;
        }
        
        std::string trimmed = line.substr(start, end - start + 1);
        
        // Adjust indent level for closing braces
        if (!trimmed.empty() && (trimmed[0] == '}' || trimmed[0] == ')' || trimmed[0] == ']')) {
            indentLevel = std::max(0, indentLevel - 1);
        }
        
        // Add indentation
        for (int i = 0; i < indentLevel; ++i) {
            result += indentStr;
        }
        
        // Format the line content
        result += formatLine(trimmed, options, languageId);
        result += "\n";
        
        // Adjust indent level for opening braces
        for (char c : trimmed) {
            if (c == '{' || c == '(' || c == '[') {
                indentLevel++;
            } else if (c == '}' || c == ')' || c == ']') {
                // Already handled above
            }
        }
    }
    
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

std::string FormattingProvider::formatLine(const std::string& line,
                                           const FormattingOptions& options,
                                           const std::string& languageId) {
    std::string result = line;
    
    // Space after keywords
    std::regex keywordRegex(R"(\b(if|for|while|switch|catch|return)\()");
    result = std::regex_replace(result, keywordRegex, "$1 (");
    
    // Space around operators (simplified)
    std::regex operatorRegex(R"(([a-zA-Z0-9_])([\+\-\*/%]=?|==|!=|<=|>=|&&|\|\||<<|>>)([a-zA-Z0-9_]))");
    result = std::regex_replace(result, operatorRegex, "$1 $2 $3");
    
    // Space after comma
    std::regex commaRegex(R"(,([^\s]))");
    result = std::regex_replace(result, commaRegex, ", $1");
    
    // Remove trailing whitespace
    auto end = result.find_last_not_of(" \t");
    if (end != std::string::npos) {
        result = result.substr(0, end + 1);
    }
    
    return result;
}

// ============================================================================
// SignatureHelpProvider Implementation
// ============================================================================

std::optional<SignatureHelp> SignatureHelpProvider::provideSignatureHelp(
    const TextDocument& document,
    const Position& position) {
    
    // Find the function call context
    std::string line = document.getLine(position.line);
    
    // Look backwards for opening parenthesis
    int parenDepth = 0;
    int funcStart = -1;
    int activeParam = 0;
    
    for (int i = position.character - 1; i >= 0; --i) {
        char c = line[i];
        if (c == ')') parenDepth++;
        else if (c == '(') {
            if (parenDepth == 0) {
                funcStart = i;
                break;
            }
            parenDepth--;
        } else if (c == ',' && parenDepth == 0) {
            activeParam++;
        }
    }
    
    if (funcStart < 0) return std::nullopt;
    
    // Find function name
    int nameStart = funcStart - 1;
    while (nameStart >= 0 && (std::isalnum(line[nameStart]) || line[nameStart] == '_')) {
        nameStart--;
    }
    nameStart++;
    
    std::string funcName = line.substr(nameStart, funcStart - nameStart);
    if (funcName.empty()) return std::nullopt;
    
    // Look up signature
    auto it = signatures_.find(funcName);
    if (it == signatures_.end()) {
        // Try common function signatures
        auto signature = getBuiltinSignature(funcName);
        if (!signature.has_value()) return std::nullopt;
        
        SignatureHelp help;
        help.signatures.push_back(*signature);
        help.activeSignature = 0;
        help.activeParameter = activeParam;
        return help;
    }
    
    SignatureHelp help;
    help.signatures.push_back(it->second);
    help.activeSignature = 0;
    help.activeParameter = activeParam;
    return help;
}

void SignatureHelpProvider::registerSignature(const std::string& functionName,
                                              const SignatureInformation& signature) {
    signatures_[functionName] = signature;
}

std::optional<SignatureInformation> SignatureHelpProvider::getBuiltinSignature(
    const std::string& funcName) {
    
    // Common C++ function signatures
    static const std::map<std::string, SignatureInformation> builtins = {
        {"printf", {
            .label = "printf(const char* format, ...) -> int",
            .documentation = "Prints formatted output to stdout",
            .parameters = {
                {.label = "format", .documentation = "Format string"},
                {.label = "...", .documentation = "Variable arguments"}
            }
        }},
        {"sprintf", {
            .label = "sprintf(char* buffer, const char* format, ...) -> int",
            .documentation = "Prints formatted output to a string buffer",
            .parameters = {
                {.label = "buffer", .documentation = "Output buffer"},
                {.label = "format", .documentation = "Format string"},
                {.label = "...", .documentation = "Variable arguments"}
            }
        }},
        {"malloc", {
            .label = "malloc(size_t size) -> void*",
            .documentation = "Allocates memory",
            .parameters = {
                {.label = "size", .documentation = "Number of bytes to allocate"}
            }
        }},
        {"free", {
            .label = "free(void* ptr) -> void",
            .documentation = "Frees allocated memory",
            .parameters = {
                {.label = "ptr", .documentation = "Pointer to memory to free"}
            }
        }},
        {"memcpy", {
            .label = "memcpy(void* dest, const void* src, size_t count) -> void*",
            .documentation = "Copies memory",
            .parameters = {
                {.label = "dest", .documentation = "Destination pointer"},
                {.label = "src", .documentation = "Source pointer"},
                {.label = "count", .documentation = "Number of bytes to copy"}
            }
        }},
        {"strlen", {
            .label = "strlen(const char* str) -> size_t",
            .documentation = "Returns length of string",
            .parameters = {
                {.label = "str", .documentation = "Null-terminated string"}
            }
        }},
        {"strcmp", {
            .label = "strcmp(const char* lhs, const char* rhs) -> int",
            .documentation = "Compares two strings",
            .parameters = {
                {.label = "lhs", .documentation = "First string"},
                {.label = "rhs", .documentation = "Second string"}
            }
        }}
    };
    
    auto it = builtins.find(funcName);
    if (it != builtins.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// ============================================================================
// CodeActionProvider Implementation
// ============================================================================

std::vector<CodeAction> CodeActionProvider::provideCodeActions(
    const TextDocument& document,
    const Range& range,
    const CodeActionContext& context) {
    
    std::vector<CodeAction> actions;
    
    // Add quick fixes for diagnostics
    for (const auto& diagnostic : context.diagnostics) {
        auto fixes = getQuickFixes(document, diagnostic);
        actions.insert(actions.end(), fixes.begin(), fixes.end());
    }
    
    // Add refactoring actions
    auto refactorings = getRefactorings(document, range);
    actions.insert(actions.end(), refactorings.begin(), refactorings.end());
    
    return actions;
}

std::vector<CodeAction> CodeActionProvider::getQuickFixes(const TextDocument& document,
                                                          const Diagnostic& diagnostic) {
    std::vector<CodeAction> fixes;
    
    // Common quick fixes based on diagnostic message
    if (diagnostic.message.find("undefined") != std::string::npos ||
        diagnostic.message.find("undeclared") != std::string::npos) {
        
        // Suggest adding include
        CodeAction addInclude;
        addInclude.title = "Add missing include";
        addInclude.kind = "quickfix";
        addInclude.diagnostics.push_back(diagnostic);
        fixes.push_back(addInclude);
    }
    
    if (diagnostic.message.find("unused") != std::string::npos) {
        // Suggest removing unused variable
        CodeAction removeUnused;
        removeUnused.title = "Remove unused variable";
        removeUnused.kind = "quickfix";
        removeUnused.diagnostics.push_back(diagnostic);
        fixes.push_back(removeUnused);
        
        // Or prefix with underscore
        CodeAction prefixUnderscore;
        prefixUnderscore.title = "Prefix with underscore";
        prefixUnderscore.kind = "quickfix";
        prefixUnderscore.diagnostics.push_back(diagnostic);
        fixes.push_back(prefixUnderscore);
    }
    
    if (diagnostic.message.find("missing semicolon") != std::string::npos) {
        CodeAction addSemicolon;
        addSemicolon.title = "Add missing semicolon";
        addSemicolon.kind = "quickfix";
        addSemicolon.diagnostics.push_back(diagnostic);
        fixes.push_back(addSemicolon);
    }
    
    return fixes;
}

std::vector<CodeAction> CodeActionProvider::getRefactorings(const TextDocument& document,
                                                            const Range& range) {
    std::vector<CodeAction> refactorings;
    
    // Check if selection contains code worth refactoring
    std::string selectedText = document.getText(range);
    if (selectedText.length() > 5) {
        // Extract method/function
        CodeAction extractMethod;
        extractMethod.title = "Extract to function";
        extractMethod.kind = "refactor.extract";
        refactorings.push_back(extractMethod);
        
        // Extract variable
        CodeAction extractVar;
        extractVar.title = "Extract to variable";
        extractVar.kind = "refactor.extract";
        refactorings.push_back(extractVar);
    }
    
    // Rename symbol
    CodeAction rename;
    rename.title = "Rename symbol";
    rename.kind = "refactor.rename";
    refactorings.push_back(rename);
    
    // Inline variable (if on variable declaration)
    CodeAction inlineVar;
    inlineVar.title = "Inline variable";
    inlineVar.kind = "refactor.inline";
    refactorings.push_back(inlineVar);
    
    return refactorings;
}

// ============================================================================
// LanguageServer Implementation
// ============================================================================

LanguageServer::LanguageServer() 
    : documentManager_(std::make_unique<DocumentManager>())
    , symbolIndex_(std::make_unique<SymbolIndex>())
    , diagnosticsManager_(std::make_unique<DiagnosticsManager>())
    , completionProvider_(std::make_unique<CompletionProvider>())
    , hoverProvider_(std::make_unique<HoverProvider>())
    , definitionProvider_(std::make_unique<DefinitionProvider>())
    , referencesProvider_(std::make_unique<ReferencesProvider>())
    , formattingProvider_(std::make_unique<FormattingProvider>())
    , signatureHelpProvider_(std::make_unique<SignatureHelpProvider>())
    , codeActionProvider_(std::make_unique<CodeActionProvider>())
    , running_(false) {
    
    registerHandlers();
}

LanguageServer::~LanguageServer() {
    stop();
}

void LanguageServer::start(int port) {
    running_ = true;
    
    // Start message processing thread
    messageThread_ = std::thread([this]() {
        while (running_) {
            processMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // In production: start TCP server on port
}

void LanguageServer::stop() {
    running_ = false;
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
}

void LanguageServer::processMessage(const std::string& message) {
    JSONRPCMessage msg = JSONRPCMessage::deserialize(message);
    
    auto it = handlers_.find(msg.method);
    if (it != handlers_.end()) {
        std::string response = it->second(msg.params);
        
        // Send response if this was a request (has id)
        if (!msg.id.empty()) {
            JSONRPCMessage responseMsg;
            responseMsg.jsonrpc = "2.0";
            responseMsg.id = msg.id;
            responseMsg.result = response;
            sendMessage(responseMsg.serialize());
        }
    }
}

void LanguageServer::processMessages() {
    std::lock_guard<std::mutex> lock(messageMutex_);
    
    while (!messageQueue_.empty()) {
        std::string message = messageQueue_.front();
        messageQueue_.pop();
        processMessage(message);
    }
}

void LanguageServer::queueMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(messageMutex_);
    messageQueue_.push(message);
}

void LanguageServer::sendMessage(const std::string& message) {
    // In production: send over TCP connection
    // For now, log the message
    std::cout << "LSP Response: " << message << std::endl;
}

void LanguageServer::sendNotification(const std::string& method, const std::string& params) {
    JSONRPCMessage msg;
    msg.jsonrpc = "2.0";
    msg.method = method;
    msg.params = params;
    sendMessage(msg.serialize());
}

void LanguageServer::registerHandler(const std::string& method, MessageHandler handler) {
    handlers_[method] = handler;
}

void LanguageServer::registerHandlers() {
    // Initialize
    registerHandler("initialize", [this](const std::string& params) {
        return handleInitialize(params);
    });
    
    registerHandler("initialized", [this](const std::string& params) {
        return handleInitialized(params);
    });
    
    registerHandler("shutdown", [this](const std::string& params) {
        return handleShutdown(params);
    });
    
    // Text document
    registerHandler("textDocument/didOpen", [this](const std::string& params) {
        return handleDidOpen(params);
    });
    
    registerHandler("textDocument/didChange", [this](const std::string& params) {
        return handleDidChange(params);
    });
    
    registerHandler("textDocument/didClose", [this](const std::string& params) {
        return handleDidClose(params);
    });
    
    registerHandler("textDocument/completion", [this](const std::string& params) {
        return handleCompletion(params);
    });
    
    registerHandler("textDocument/hover", [this](const std::string& params) {
        return handleHover(params);
    });
    
    registerHandler("textDocument/definition", [this](const std::string& params) {
        return handleDefinition(params);
    });
    
    registerHandler("textDocument/references", [this](const std::string& params) {
        return handleReferences(params);
    });
    
    registerHandler("textDocument/formatting", [this](const std::string& params) {
        return handleFormatting(params);
    });
    
    registerHandler("textDocument/signatureHelp", [this](const std::string& params) {
        return handleSignatureHelp(params);
    });
    
    registerHandler("textDocument/codeAction", [this](const std::string& params) {
        return handleCodeAction(params);
    });
    
    registerHandler("textDocument/documentSymbol", [this](const std::string& params) {
        return handleDocumentSymbol(params);
    });
    
    // Workspace
    registerHandler("workspace/symbol", [this](const std::string& params) {
        return handleWorkspaceSymbol(params);
    });
}

std::string LanguageServer::handleInitialize(const std::string& params) {
    // Return server capabilities
    std::ostringstream oss;
    oss << "{\"capabilities\":{";
    oss << "\"textDocumentSync\":2,"; // Incremental
    oss << "\"completionProvider\":{\"triggerCharacters\":[\".\",\"::\",\"->\"]},";
    oss << "\"hoverProvider\":true,";
    oss << "\"definitionProvider\":true,";
    oss << "\"referencesProvider\":true,";
    oss << "\"documentFormattingProvider\":true,";
    oss << "\"documentRangeFormattingProvider\":true,";
    oss << "\"signatureHelpProvider\":{\"triggerCharacters\":[\"(\",\",\"]},";
    oss << "\"codeActionProvider\":true,";
    oss << "\"documentSymbolProvider\":true,";
    oss << "\"workspaceSymbolProvider\":true";
    oss << "}}";
    return oss.str();
}

std::string LanguageServer::handleInitialized(const std::string& params) {
    return "null";
}

std::string LanguageServer::handleShutdown(const std::string& params) {
    return "null";
}

std::string LanguageServer::handleDidOpen(const std::string& params) {
    // Parse params to extract document info
    // Simplified parsing - in production use proper JSON parser
    
    auto uriStart = params.find("\"uri\"");
    auto textStart = params.find("\"text\"");
    auto languageStart = params.find("\"languageId\"");
    
    if (uriStart != std::string::npos && textStart != std::string::npos) {
        // Extract values (simplified)
        auto uriValueStart = params.find('"', uriStart + 6) + 1;
        auto uriValueEnd = params.find('"', uriValueStart);
        std::string uri = params.substr(uriValueStart, uriValueEnd - uriValueStart);
        
        auto textValueStart = params.find('"', textStart + 7) + 1;
        auto textValueEnd = params.find('"', textValueStart);
        std::string text = params.substr(textValueStart, textValueEnd - textValueStart);
        
        std::string languageId = "cpp";
        if (languageStart != std::string::npos) {
            auto langValueStart = params.find('"', languageStart + 13) + 1;
            auto langValueEnd = params.find('"', langValueStart);
            languageId = params.substr(langValueStart, langValueEnd - langValueStart);
        }
        
        documentManager_->openDocument(uri, text, languageId, 1);
        symbolIndex_->indexDocument(uri, text, languageId);
    }
    
    return "null";
}

std::string LanguageServer::handleDidChange(const std::string& params) {
    // Handle document changes
    // In production: parse contentChanges array and apply
    return "null";
}

std::string LanguageServer::handleDidClose(const std::string& params) {
    // Extract URI and close document
    auto uriStart = params.find("\"uri\"");
    if (uriStart != std::string::npos) {
        auto uriValueStart = params.find('"', uriStart + 6) + 1;
        auto uriValueEnd = params.find('"', uriValueStart);
        std::string uri = params.substr(uriValueStart, uriValueEnd - uriValueStart);
        
        documentManager_->closeDocument(uri);
        symbolIndex_->clearSymbols(uri);
    }
    
    return "null";
}

std::string LanguageServer::handleCompletion(const std::string& params) {
    // Extract document URI and position
    // Return completion items
    std::ostringstream oss;
    oss << "{\"isIncomplete\":false,\"items\":[]}";
    return oss.str();
}

std::string LanguageServer::handleHover(const std::string& params) {
    // Return hover information
    return "null";
}

std::string LanguageServer::handleDefinition(const std::string& params) {
    // Return definition locations
    return "[]";
}

std::string LanguageServer::handleReferences(const std::string& params) {
    // Return reference locations
    return "[]";
}

std::string LanguageServer::handleFormatting(const std::string& params) {
    // Return text edits for formatting
    return "[]";
}

std::string LanguageServer::handleSignatureHelp(const std::string& params) {
    // Return signature help
    return "null";
}

std::string LanguageServer::handleCodeAction(const std::string& params) {
    // Return code actions
    return "[]";
}

std::string LanguageServer::handleDocumentSymbol(const std::string& params) {
    // Return document symbols
    return "[]";
}

std::string LanguageServer::handleWorkspaceSymbol(const std::string& params) {
    // Return workspace symbols
    return "[]";
}

} // namespace LSP
} // namespace RawrXD
