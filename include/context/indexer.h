#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Context {

struct Symbol {
    std::string name;
    std::string kind;
    std::string file;
    int line{0};
};

struct IndexStats {
    size_t files_indexed{0};
    size_t symbols_found{0};
};

class Indexer {
public:
    explicit Indexer(const std::string& root);

    IndexStats build(bool recursive = true);
    std::vector<Symbol> findByName(const std::string& name) const;
    std::vector<Symbol> findByKind(const std::string& kind) const;
    std::vector<Symbol> findInFile(const std::string& file) const;

private:
    bool isCodeFile(const std::string& path);
    void indexFile(const std::string& path);

    std::string m_root;
    std::vector<Symbol> m_symbols;
    IndexStats m_stats;
};

} // namespace Context
} // namespace RawrXD
