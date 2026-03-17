#include "lsp_client_impl.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

LSPClient::LSPClient() {}
LSPClient::~LSPClient() {}

bool LSPClient::connect(const std::string& uri) {
    // Placeholder: always succeeds
    if (m_callback) m_callback(std::string("LSP connected: ") + uri);
    return true;
}

bool LSPClient::disconnect() {
    if (m_callback) m_callback("LSP disconnected");
    return true;
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return {};
    std::ostringstream ss; ss << file.rdbuf();
    return ss.str();
}

static std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::regex word(R"([A-Za-z_][A-Za-z0-9_]+)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), word);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) tokens.push_back((*it).str());
    return tokens;
}

std::vector<std::string> LSPClient::complete(const std::string& file, int line, int column) {
    std::string content = readFile(file);
    if (content.empty()) return {};
    // Simple heuristic: suggest common tokens present in file
    auto tokens = tokenize(content);
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    // Top suggestions
    std::vector<std::string> suggestions;
    for (const auto& t : tokens) {
        if (t.size() >= 3) suggestions.push_back(t);
        if (suggestions.size() >= 16) break;
    }
    // Add some language keywords
    const char* keywords[] = {"int","void","auto","class","struct","return","if","for","while","switch","constexpr","template"};
    suggestions.insert(suggestions.end(), std::begin(keywords), std::end(keywords));
    return suggestions;
}

LSPClient::Definition LSPClient::gotoDefinition(const std::string& file, int line, int column) {
    Definition def; def.uri = file; def.line = 0; def.column = 0;
    std::string content = readFile(file);
    if (content.empty()) return def;
    // Extract symbol under cursor (naive)
    size_t pos = 0; int curLine = 0; for (; pos < content.size() && curLine < line; ++pos) if (content[pos] == '\n') ++curLine;
    size_t start = pos + column;
    size_t s = start, e = start;
    while (s > 0 && (isalnum((unsigned char)content[s-1]) || content[s-1]=='_')) --s;
    while (e < content.size() && (isalnum((unsigned char)content[e]) || content[e]=='_')) ++e;
    std::string symbol = content.substr(s, e - s);
    if (symbol.empty()) return def;
    // Search for a definition-like line
    std::regex defRegex(std::string("(^|\n)[^\n]*\\b") + symbol + std::string("\\b[^\n]*\n"));
    if (std::regex_search(content, defRegex)) {
        // Convert match position to line/col
        auto begin = std::sregex_iterator(content.begin(), content.end(), defRegex);
        if (begin != std::sregex_iterator()) {
            size_t matchPos = (*begin).position();
            int l = 0; size_t idx = 0; while (idx < matchPos) { if (content[idx++]=='\n') ++l; }
            int c = 0; while (idx < content.size() && content[idx] != symbol[0]) { if (content[idx++]=='\n') { l++; c = 0; } else ++c; }
            def.line = l; def.column = c;
        }
    }
    return def;
}

std::vector<LSPClient::Reference> LSPClient::findReferences(const std::string& file, int, int) {
    std::vector<Reference> refs;
    // Find symbol under cursor (reuse gotoDefinition extraction)
    std::string content = readFile(file);
    if (content.empty()) return refs;
    // For simplicity, use entire file tokens and count occurrences
    auto tokens = tokenize(content);
    std::unordered_map<std::string,int> counts;
    for (auto& t : tokens) counts[t]++;
    // Return top frequent tokens as "references"
    int added = 0;
    for (auto& [tok,cnt] : counts) {
        if (cnt >= 3) { refs.push_back({file, 0, 0}); if (++added >= 10) break; }
    }
    return refs;
}

std::vector<LSPClient::Diagnostic> LSPClient::getDiagnostics(const std::string& file) {
    std::vector<Diagnostic> diags;
    std::string content = readFile(file);
    if (content.empty()) return diags;
    // Simple diagnostic: warn on TODO
    std::regex todo(R"(TODO|FIXME)");
    auto it = std::sregex_iterator(content.begin(), content.end(), todo);
    for (; it != std::sregex_iterator(); ++it) {
        Diagnostic d; d.message = "Found TODO/FIXME"; d.severity = "warning";
        size_t pos = (*it).position(); int line = 0; for (size_t i=0;i<pos;++i) if (content[i]=='\n') ++line; d.line = line; d.column = 0;
        diags.push_back(d);
    }
    return diags;
}

void LSPClient::setCallback(std::function<void(const std::string&)> callback) {
    m_callback = std::move(callback);
}
