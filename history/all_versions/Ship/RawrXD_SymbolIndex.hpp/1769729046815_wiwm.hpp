// RawrXD_SymbolIndex.hpp - High-Performance Codebase Symbol Indexer
// Pure C++20 - No Qt Dependencies
// Fast LSP-style symbol navigation

#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

namespace RawrXD {

struct SymbolInfo {
    std::string name;
    std::string type; // function, class, variable, namespace
    std::string filePath;
    int lineNumber = 0;
    std::string signature;
};

class SymbolIndex {
public:
    void IndexDirectory(const std::string& rootPath) {
        symbols_.clear();
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
                    IndexFile(entry.path().string());
                }
            }
        }
    }

    std::vector<SymbolInfo> Search(const std::string& query) {
        std::vector<SymbolInfo> results;
        for (const auto& [name, sym] : symbols_) {
            if (name.find(query) != std::string::npos) {
                results.push_back(sym);
            }
        }
        return results;
    }

    SymbolInfo* FindSymbol(const std::string& name) {
        auto it = symbols_.find(name);
        return (it != symbols_.end()) ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, SymbolInfo> symbols_;

    void IndexFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;

        std::string line;
        int lineNum = 0;
        std::regex funcRe("^\\s*(\\w+)\\s+(\\w+)\\s*\\(");
        std::regex classRe("^\\s*class\\s+(\\w+)");
        
        while (std::getline(f, line)) {
            lineNum++;
            std::smatch m;
            
            if (std::regex_search(line, m, funcRe)) {
                SymbolInfo sym;
                sym.name = m.str(2);
                sym.type = "function";
                sym.filePath = path;
                sym.lineNumber = lineNum;
                sym.signature = line;
                symbols_[sym.name] = sym;
            } else if (std::regex_search(line, m, classRe)) {
                SymbolInfo sym;
                sym.name = m.str(1);
                sym.type = "class";
                sym.filePath = path;
                sym.lineNumber = lineNum;
                symbols_[sym.name] = sym;
            }
        }
    }
};

} // namespace RawrXD
