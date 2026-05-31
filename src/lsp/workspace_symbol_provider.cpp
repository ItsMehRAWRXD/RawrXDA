#include "lsp/workspace_symbol_provider.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>

namespace RawrXD::LSP {

// SymbolIndex implementation
SymbolIndex::SymbolIndex() = default;
SymbolIndex::~SymbolIndex() = default;

void SymbolIndex::addSymbol(const WorkspaceSymbol& symbol) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SymbolData data;
    data.symbol = symbol;

    m_symbols[symbol.location.uri][symbol.name] = std::move(data);
    m_byKind[symbol.kind].push_back(symbol.name);
    if (!symbol.containerName.empty()) {
        m_byContainer[symbol.containerName].push_back(symbol.name);
    }
}

void SymbolIndex::removeSymbol(const std::string& uri, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto uriIt = m_symbols.find(uri);
    if (uriIt == m_symbols.end()) return;

    auto symIt = uriIt->second.find(name);
    if (symIt == uriIt->second.end()) return;

    const auto& symbol = symIt->second.symbol;

    // Remove from kind index
    auto kindIt = m_byKind.find(symbol.kind);
    if (kindIt != m_byKind.end()) {
        auto& vec = kindIt->second;
        vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
    }

    // Remove from container index
    if (!symbol.containerName.empty()) {
        auto contIt = m_byContainer.find(symbol.containerName);
        if (contIt != m_byContainer.end()) {
            auto& vec = contIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
        }
    }

    uriIt->second.erase(symIt);
    if (uriIt->second.empty()) {
        m_symbols.erase(uriIt);
    }
}

void SymbolIndex::clearFile(const std::string& uri) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto uriIt = m_symbols.find(uri);
    if (uriIt == m_symbols.end()) return;

    for (const auto& [name, data] : uriIt->second) {
        const auto& symbol = data.symbol;

        auto kindIt = m_byKind.find(symbol.kind);
        if (kindIt != m_byKind.end()) {
            auto& vec = kindIt->second;
            vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
        }

        if (!symbol.containerName.empty()) {
            auto contIt = m_byContainer.find(symbol.containerName);
            if (contIt != m_byContainer.end()) {
                auto& vec = contIt->second;
                vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
            }
        }
    }

    m_symbols.erase(uriIt);
}

void SymbolIndex::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_symbols.clear();
    m_byKind.clear();
    m_byContainer.clear();
}

std::vector<WorkspaceSymbol> SymbolIndex::query(const std::string& pattern, size_t limit) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<WorkspaceSymbol> results;
    std::string lowerPattern = pattern;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

    for (const auto& [uri, symbols] : m_symbols) {
        for (const auto& [name, data] : symbols) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find(lowerPattern) != std::string::npos) {
                auto symbol = data.symbol;
                symbol.score = calculateScore(pattern, symbol);
                results.push_back(symbol);
            }
        }
    }

    std::sort(results.begin(), results.end(),
              [](const WorkspaceSymbol& a, const WorkspaceSymbol& b) {
                  return a.score > b.score;
              });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

std::vector<WorkspaceSymbol> SymbolIndex::queryExact(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<WorkspaceSymbol> results;
    for (const auto& [uri, symbols] : m_symbols) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            results.push_back(it->second.symbol);
        }
    }

    return results;
}

std::vector<WorkspaceSymbol> SymbolIndex::queryByKind(SymbolKind kind) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<WorkspaceSymbol> results;
    auto it = m_byKind.find(kind);
    if (it == m_byKind.end()) return results;

    for (const auto& name : it->second) {
        for (const auto& [uri, symbols] : m_symbols) {
            auto symIt = symbols.find(name);
            if (symIt != symbols.end()) {
                results.push_back(symIt->second.symbol);
            }
        }
    }

    return results;
}

float SymbolIndex::calculateScore(const std::string& pattern, const WorkspaceSymbol& symbol) {
    float score = 0.0f;

    // Exact match bonus
    if (symbol.name == pattern) {
        score += 100.0f;
    }

    // Case-insensitive match
    std::string lowerName = symbol.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::string lowerPattern = pattern;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

    if (lowerName == lowerPattern) {
        score += 50.0f;
    }

    // Prefix match
    if (lowerName.find(lowerPattern) == 0) {
        score += 25.0f;
    }

    // Contains match
    if (lowerName.find(lowerPattern) != std::string::npos) {
        score += 10.0f;
    }

    // Kind priority
    switch (symbol.kind) {
        case SymbolKind::Class:
        case SymbolKind::Interface:
        case SymbolKind::Struct:
            score += 5.0f;
            break;
        case SymbolKind::Function:
        case SymbolKind::Method:
            score += 3.0f;
            break;
        default:
            break;
    }

    return score;
}

size_t SymbolIndex::getSymbolCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [uri, symbols] : m_symbols) {
        count += symbols.size();
    }
    return count;
}

size_t SymbolIndex::getFileCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_symbols.size();
}

// WorkspaceSymbolProvider implementation
WorkspaceSymbolProvider::WorkspaceSymbolProvider()
    : m_index(std::make_unique<SymbolIndex>())
{}

WorkspaceSymbolProvider::~WorkspaceSymbolProvider() = default;

void WorkspaceSymbolProvider::initialize(const std::string& workspaceRoot) {
    m_workspaceRoot = workspaceRoot;
}

void WorkspaceSymbolProvider::shutdown() {
    pauseIndexing();
    m_index->clearAll();
}

void WorkspaceSymbolProvider::indexFile(const std::string& uri, const std::string& content) {
    auto symbols = parseSymbols(uri, content);
    m_index->clearFile(uri);
    for (const auto& symbol : symbols) {
        m_index->addSymbol(symbol);
    }
}

void WorkspaceSymbolProvider::reindexFile(const std::string& uri) {
    namespace fs = std::filesystem;

    std::string path = uri;
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    indexFile(uri, content);
}

void WorkspaceSymbolProvider::removeFile(const std::string& uri) {
    m_index->clearFile(uri);
}

std::future<void> WorkspaceSymbolProvider::indexWorkspaceAsync() {
    return std::async(std::launch::async, [this]() {
        doIndexWorkspace();
    });
}

void WorkspaceSymbolProvider::doIndexWorkspace() {
    namespace fs = std::filesystem;

    m_indexing = true;
    m_progress = 0.0f;

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(m_workspaceRoot)) {
        if (m_paused) {
            while (m_paused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (!m_indexing) break;

        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".h" || ext == ".hpp" ||
                ext == ".c" || ext == ".cc" || ext == ".cxx") {
                files.push_back(entry.path());
            }
        }
    }

    size_t total = files.size();
    size_t current = 0;

    for (const auto& file : files) {
        if (!m_indexing) break;

        std::string uri = "file://" + file.string();
        reindexFile(uri);

        current++;
        m_progress = static_cast<float>(current) / static_cast<float>(total);

        if (m_progressCallback) {
            m_progressCallback(file.string(), current, total);
        }
    }

    m_indexing = false;
    m_progress = 1.0f;
}

void WorkspaceSymbolProvider::pauseIndexing() {
    m_paused = true;
}

void WorkspaceSymbolProvider::resumeIndexing() {
    m_paused = false;
}

bool WorkspaceSymbolProvider::isIndexing() const {
    return m_indexing;
}

float WorkspaceSymbolProvider::getIndexingProgress() const {
    return m_progress;
}

std::vector<WorkspaceSymbol> WorkspaceSymbolProvider::symbol(const std::string& query, size_t limit) {
    return m_index->query(query, limit);
}

std::optional<WorkspaceSymbol> WorkspaceSymbolProvider::resolveSymbol(const std::string& uri,
                                                                          const std::string& name) {
    auto results = m_index->queryExact(name);
    for (const auto& sym : results) {
        if (sym.location.uri == uri) {
            return sym;
        }
    }
    return std::nullopt;
}

std::vector<WorkspaceSymbol> WorkspaceSymbolProvider::parseSymbols(const std::string& uri,
                                                                         const std::string& content) {
    std::vector<WorkspaceSymbol> symbols;

    // Simple regex-based parsing for C++
    std::regex classRegex(R"(\b(class|struct)\s+(\w+))");
    std::regex functionRegex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*(const)?\s*\{)");
    std::regex variableRegex(R"((\w+)\s+(\w+)\s*;)");
    std::regex enumRegex(R"(\benum\s+(class\s+)?(\w+))");
    std::regex namespaceRegex(R"(\bnamespace\s+(\w+))");

    std::istringstream stream(content);
    std::string line;
    uint32_t lineNum = 0;
    std::string currentNamespace;

    while (std::getline(stream, line)) {
        lineNum++;

        std::smatch match;
        if (std::regex_search(line, match, namespaceRegex)) {
            currentNamespace = match[1];
        }

        if (std::regex_search(line, match, classRegex)) {
            WorkspaceSymbol sym;
            sym.name = match[2];
            sym.kind = (match[1] == "class") ? SymbolKind::Class : SymbolKind::Struct;
            sym.containerName = currentNamespace;
            sym.location = {uri, lineNum, static_cast<uint32_t>(match.position(2)), lineNum,
                           static_cast<uint32_t>(match.position(2) + match[2].length())};
            symbols.push_back(sym);
        }

        if (std::regex_search(line, match, enumRegex)) {
            WorkspaceSymbol sym;
            sym.name = match[2];
            sym.kind = SymbolKind::Enum;
            sym.containerName = currentNamespace;
            sym.location = {uri, lineNum, static_cast<uint32_t>(match.position(2)), lineNum,
                           static_cast<uint32_t>(match.position(2) + match[2].length())};
            symbols.push_back(sym);
        }
    }

    return symbols;
}

void WorkspaceSymbolProvider::onIndexProgress(IndexProgressCallback callback) {
    m_progressCallback = callback;
}

// Global provider
WorkspaceSymbolProvider& getWorkspaceSymbolProvider() {
    static WorkspaceSymbolProvider provider;
    return provider;
}

std::string symbolKindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::File: return "File";
        case SymbolKind::Module: return "Module";
        case SymbolKind::Namespace: return "Namespace";
        case SymbolKind::Package: return "Package";
        case SymbolKind::Class: return "Class";
        case SymbolKind::Method: return "Method";
        case SymbolKind::Property: return "Property";
        case SymbolKind::Field: return "Field";
        case SymbolKind::Constructor: return "Constructor";
        case SymbolKind::Enum: return "Enum";
        case SymbolKind::Interface: return "Interface";
        case SymbolKind::Function: return "Function";
        case SymbolKind::Variable: return "Variable";
        case SymbolKind::Constant: return "Constant";
        case SymbolKind::String: return "String";
        case SymbolKind::Number: return "Number";
        case SymbolKind::Boolean: return "Boolean";
        case SymbolKind::Array: return "Array";
        case SymbolKind::Object: return "Object";
        case SymbolKind::Key: return "Key";
        case SymbolKind::Null: return "Null";
        case SymbolKind::EnumMember: return "EnumMember";
        case SymbolKind::Struct: return "Struct";
        case SymbolKind::Event: return "Event";
        case SymbolKind::Operator: return "Operator";
        case SymbolKind::TypeParameter: return "TypeParameter";
        default: return "Unknown";
    }
}

} // namespace RawrXD::LSP
