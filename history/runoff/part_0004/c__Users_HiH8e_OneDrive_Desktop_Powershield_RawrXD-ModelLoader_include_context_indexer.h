#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace RawrXD {
namespace Context {

struct Symbol {
    std::string name;
    std::string kind;       // function, class, variable, file
    std::string file;
    int line = 0;
};

struct IndexStats {
    size_t files_indexed = 0;
    size_t symbols_found = 0;
};

class Indexer {
public:
    explicit Indexer(const std::string& root);

    // Build the index
    IndexStats build(bool recursive = true);

    // Query
    std::vector<Symbol> findByName(const std::string& name) const;
    std::vector<Symbol> findByKind(const std::string& kind) const;
    std::vector<Symbol> findInFile(const std::string& file) const;

    const std::vector<Symbol>& getAll() const { return m_symbols; }
    const IndexStats& getStats() const { return m_stats; }

private:
    std::string m_root;
    std::vector<Symbol> m_symbols;
    IndexStats m_stats;

    void indexFile(const std::string& path);
    static bool isCodeFile(const std::string& path);
};

} // namespace Context
} // namespace RawrXD
