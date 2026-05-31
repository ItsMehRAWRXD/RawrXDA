#include "lsp/completion_provider.h"
#include "ipc/shm_channel.hpp"
#include "extension_kernel/autocomplete_protocol.h"
#include <sstream>
#include <regex>
#include <cstring>
#include <immintrin.h>

namespace {

std::optional<RawrXD::LSP::CompletionItem> tryKernelAutocomplete(
    const std::string& uri,
    const std::string& content,
    uint32_t line,
    uint32_t col,
    const std::string& prefix) {

    static RawrXD::IPC::ShmBiChannel channel;
    static bool connected = false;
    if (!connected) {
        connected = channel.open_client("RawrXD_Autocomplete", 256);
        if (!connected) return std::nullopt;
    }

    RawrXD::ExtensionKernel::CompletionRequest req{};
    req.version = RawrXD::ExtensionKernel::kAutocompleteWireVersion;
    req.line = line;
    req.col = col;

    const auto safe_copy = [](char* dst, size_t cap, const std::string& src) {
        const size_t n = std::min(cap - 1, src.size());
        std::memcpy(dst, src.data(), n);
        dst[n] = '\0';
    };

    safe_copy(req.filePath, sizeof(req.filePath), uri);
    safe_copy(req.content, sizeof(req.content), content);
    safe_copy(req.prefix, sizeof(req.prefix), prefix);

    if (!channel.send(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&req), sizeof(req)))) {
        return std::nullopt;
    }

    std::vector<uint8_t> raw;
    for (int i = 0; i < 1500; ++i) {
        if (channel.rx().read_copy(raw)) break;
        _mm_pause();
    }
    if (raw.size() < sizeof(RawrXD::ExtensionKernel::CompletionResult)) {
        return std::nullopt;
    }

    RawrXD::ExtensionKernel::CompletionResult res{};
    std::memcpy(&res, raw.data(), sizeof(res));
    if (res.version != RawrXD::ExtensionKernel::kAutocompleteWireVersion) {
        return std::nullopt;
    }
    if (res.text[0] == '\0') {
        return std::nullopt;
    }

    RawrXD::LSP::CompletionItem item;
    item.label = res.text;
    item.kind = RawrXD::LSP::CompletionItemKind::Text;
    item.insertText = res.text;
    item.preselect = true;
    item.sortText = std::string("00_kernel");
    item.detail = std::string("extension-kernel");
    return item;
}

} // namespace

namespace RawrXD::LSP {

CompletionProvider::CompletionProvider() = default;
CompletionProvider::~CompletionProvider() = default;

void CompletionProvider::setTriggerCharacters(const std::vector<std::string>& characters) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_triggerCharacters = characters;
}

std::vector<std::string> CompletionProvider::getTriggerCharacters() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_triggerCharacters;
}

std::vector<CompletionItem> CompletionProvider::provideCompletion(const std::string& uri,
                                                                      const std::string& content,
                                                                      uint32_t line,
                                                                      uint32_t column,
                                                                      const CompletionContext& context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CompletionItem> items;

    std::string prefix = getWordAtPosition(content, line, column);

    // C++ keywords
    auto keywords = getCppKeywords();
    for (const auto& kw : keywords) {
        if (kw.label.find(prefix) == 0 || prefix.empty()) {
            items.push_back(kw);
        }
    }

    // C++ types
    auto types = getCppTypes();
    for (const auto& type : types) {
        if (type.label.find(prefix) == 0 || prefix.empty()) {
            items.push_back(type);
        }
    }

    // Snippets
    auto snippets = getCppSnippets();
    for (const auto& snippet : snippets) {
        if (snippet.label.find(prefix) == 0 || prefix.empty()) {
            items.push_back(snippet);
        }
    }

    // Registered sources
    for (const auto& source : m_sources) {
        auto sourceItems = source(uri, prefix);
        items.insert(items.end(), sourceItems.begin(), sourceItems.end());
    }

    // Extension-kernel candidate (if channel/server is available)
    if (auto kernelItem = tryKernelAutocomplete(uri, content, line, column, prefix)) {
        items.push_back(std::move(*kernelItem));
    }

    // Sort by relevance
    std::sort(items.begin(), items.end(), [](const CompletionItem& a, const CompletionItem& b) {
        if (a.preselect != b.preselect) return a.preselect > b.preselect;
        if (a.sortText && b.sortText) return *a.sortText < *b.sortText;
        return a.label < b.label;
    });

    return items;
}

std::optional<CompletionItem> CompletionProvider::resolveCompletion(const CompletionItem& item) {
    // Resolve additional details for the item
    return item;
}

std::optional<SignatureHelp> CompletionProvider::provideSignatureHelp(const std::string& uri,
                                                                           const std::string& content,
                                                                           uint32_t line,
                                                                           uint32_t column) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find function call at position
    auto lines = splitLines(content);
    if (line >= lines.size()) return std::nullopt;

    std::string currentLine = lines[line];

    // Look for opening paren before cursor
    size_t parenPos = currentLine.rfind('(', column);
    if (parenPos == std::string::npos || parenPos > column) return std::nullopt;

    // Extract function name
    std::string beforeParen = currentLine.substr(0, parenPos);
    size_t nameStart = beforeParen.find_last_of(" \t.->::");
    if (nameStart == std::string::npos) nameStart = 0;
    else nameStart++;

    std::string funcName = beforeParen.substr(nameStart);

    // Build signature help
    SignatureHelp help;
    SignatureInformation sig;
    sig.label = funcName + "(params)";
    sig.documentation = "Function documentation";

    SignatureInformation::ParameterInfo param;
    param.label = "param1";
    param.documentation = "First parameter";
    sig.parameters.push_back(param);

    help.signatures.push_back(sig);
    help.activeSignature = 0;
    help.activeParameter = 0;

    return help;
}

void CompletionProvider::registerCompletionSource(
    std::function<std::vector<CompletionItem>(const std::string& uri, const std::string& prefix)> source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sources.push_back(source);
}

std::vector<CompletionItem> CompletionProvider::getCppKeywords() {
    std::vector<CompletionItem> items;
    std::vector<std::string> keywords = {
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

    for (const auto& kw : keywords) {
        CompletionItem item;
        item.label = kw;
        item.kind = CompletionItemKind::Keyword;
        item.insertText = kw;
        items.push_back(item);
    }

    return items;
}

std::vector<CompletionItem> CompletionProvider::getCppTypes() {
    std::vector<CompletionItem> items;
    std::vector<std::string> types = {
        "int", "long", "short", "char", "bool", "float", "double", "void",
        "size_t", "ptrdiff_t", "nullptr_t", "string", "vector", "map", "set",
        "array", "tuple", "optional", "variant", "unique_ptr", "shared_ptr",
        "weak_ptr", "function", "thread", "mutex", "atomic"
    };

    for (const auto& type : types) {
        CompletionItem item;
        item.label = type;
        item.kind = CompletionItemKind::TypeParameter;
        item.insertText = type;
        items.push_back(item);
    }

    return items;
}

std::vector<CompletionItem> CompletionProvider::getCppSnippets() {
    std::vector<CompletionItem> items;

    // For loop snippet
    CompletionItem forLoop;
    forLoop.label = "for";
    forLoop.kind = CompletionItemKind::Snippet;
    forLoop.insertText = "for (size_t ${1:i} = 0; ${1:i} < ${2:count}; ++${1:i}) {\n    $0\n}";
    forLoop.insertTextFormat = InsertTextFormat::Snippet;
    forLoop.detail = "For loop";
    items.push_back(forLoop);

    // If statement snippet
    CompletionItem ifStmt;
    ifStmt.label = "if";
    ifStmt.kind = CompletionItemKind::Snippet;
    ifStmt.insertText = "if (${1:condition}) {\n    $0\n}";
    ifStmt.insertTextFormat = InsertTextFormat::Snippet;
    ifStmt.detail = "If statement";
    items.push_back(ifStmt);

    // Class snippet
    CompletionItem classSnippet;
    classSnippet.label = "class";
    classSnippet.kind = CompletionItemKind::Snippet;
    classSnippet.insertText = "class ${1:Name} {\npublic:\n    ${1:Name}();\n    ~${1:Name}();\n\nprivate:\n    $0\n};";
    classSnippet.insertTextFormat = InsertTextFormat::Snippet;
    classSnippet.detail = "Class definition";
    items.push_back(classSnippet);

    // Main function snippet
    CompletionItem mainSnippet;
    mainSnippet.label = "main";
    mainSnippet.kind = CompletionItemKind::Snippet;
    mainSnippet.insertText = "int main(int argc, char* argv[]) {\n    $0\n    return 0;\n}";
    mainSnippet.insertTextFormat = InsertTextFormat::Snippet;
    mainSnippet.detail = "Main function";
    items.push_back(mainSnippet);

    return items;
}

std::string CompletionProvider::getWordAtPosition(const std::string& content,
                                                  uint32_t line,
                                                  uint32_t column) {
    auto lines = splitLines(content);
    if (line >= lines.size()) return "";

    std::string currentLine = lines[line];
    if (column > currentLine.length()) return "";

    // Find word boundaries
    size_t start = column;
    while (start > 0 && (std::isalnum(currentLine[start - 1]) || currentLine[start - 1] == '_')) {
        start--;
    }

    size_t end = column;
    while (end < currentLine.length() &&
           (std::isalnum(currentLine[end]) || currentLine[end] == '_')) {
        end++;
    }

    return currentLine.substr(start, end - start);
}

std::vector<std::string> CompletionProvider::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Global provider
CompletionProvider& getCompletionProvider() {
    static CompletionProvider provider;
    return provider;
}

std::string completionItemKindToString(CompletionItemKind kind) {
    switch (kind) {
        case CompletionItemKind::Text: return "Text";
        case CompletionItemKind::Method: return "Method";
        case CompletionItemKind::Function: return "Function";
        case CompletionItemKind::Constructor: return "Constructor";
        case CompletionItemKind::Field: return "Field";
        case CompletionItemKind::Variable: return "Variable";
        case CompletionItemKind::Class: return "Class";
        case CompletionItemKind::Interface: return "Interface";
        case CompletionItemKind::Module: return "Module";
        case CompletionItemKind::Property: return "Property";
        case CompletionItemKind::Unit: return "Unit";
        case CompletionItemKind::Value: return "Value";
        case CompletionItemKind::Enum: return "Enum";
        case CompletionItemKind::Keyword: return "Keyword";
        case CompletionItemKind::Snippet: return "Snippet";
        case CompletionItemKind::Color: return "Color";
        case CompletionItemKind::File: return "File";
        case CompletionItemKind::Reference: return "Reference";
        case CompletionItemKind::Folder: return "Folder";
        case CompletionItemKind::EnumMember: return "EnumMember";
        case CompletionItemKind::Constant: return "Constant";
        case CompletionItemKind::Struct: return "Struct";
        case CompletionItemKind::Event: return "Event";
        case CompletionItemKind::Operator: return "Operator";
        case CompletionItemKind::TypeParameter: return "TypeParameter";
        default: return "Unknown";
    }
}

} // namespace RawrXD::LSP
