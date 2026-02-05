#define NOMINMAX
#include "context/indexer.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Context {

Indexer::Indexer(const std::string& root) : m_root(root) {}

bool Indexer::isCodeFile(const std::string& path) {
    static const char* exts[] = {".cpp", ".c", ".hpp", ".h", ".cc", ".hh", ".ini", ".md", ".txt"};
    fs::path p(path);
    auto e = p.extension().string();
    for (auto* x : exts) if (e == x) return true;
    return false;
}

IndexStats Indexer::build(bool recursive) {
    m_symbols.clear();
    m_stats = {};
    if (!fs::exists(m_root)) return m_stats;

    if (recursive) {
        for (auto& entry : fs::recursive_directory_iterator(m_root)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path().string();
            if (!isCodeFile(path)) continue;
            indexFile(path);
            m_stats.files_indexed++;
        }
    } else {
        for (auto& entry : fs::directory_iterator(m_root)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path().string();
            if (!isCodeFile(path)) continue;
            indexFile(path);
            m_stats.files_indexed++;
        }
    }
    m_stats.symbols_found = m_symbols.size();
    return m_stats;
}

void Indexer::indexFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return;

    std::string line;
    int lineno = 0;

    std::regex re_func(R"((?:^|\s)(?:[\w:\\*&<>]+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{)");
    std::regex re_class(R"((?:^|\s)class\s+([A-Za-z_][A-Za-z0-9_]*)\s*)");
    std::regex re_struct(R"((?:^|\s)struct\s+([A-Za-z_][A-Za-z0-9_]*)\s*)");
    std::regex re_var(R"((?:^|\s)(?:int|float|double|bool|auto|std::\w+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(=|;))");

    while (std::getline(ifs, line)) {
        lineno++;
        std::smatch m;
        if (std::regex_search(line, m, re_class)) {
            m_symbols.push_back({m[1], "class", path, lineno});
        } else if (std::regex_search(line, m, re_struct)) {
            m_symbols.push_back({m[1], "struct", path, lineno});
        } else if (std::regex_search(line, m, re_func)) {
            m_symbols.push_back({m[1], "function", path, lineno});
        } else if (std::regex_search(line, m, re_var)) {
            m_symbols.push_back({m[1], "variable", path, lineno});
        }
    }
}

std::vector<Symbol> Indexer::findByName(const std::string& name) const {
    std::vector<Symbol> out;
    for (const auto& s : m_symbols) if (s.name == name) out.push_back(s);
    return out;
}

std::vector<Symbol> Indexer::findByKind(const std::string& kind) const {
    std::vector<Symbol> out;
    for (const auto& s : m_symbols) if (s.kind == kind) out.push_back(s);
    return out;
}

std::vector<Symbol> Indexer::findInFile(const std::string& file) const {
    std::vector<Symbol> out;
    for (const auto& s : m_symbols) if (s.file == file) out.push_back(s);
    return out;
}

} // namespace Context
} // namespace RawrXD
