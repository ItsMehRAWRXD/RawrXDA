#include "intelligent_codebase_engine.h"

#include <fstream>
#include <regex>
#include <sstream>

IntelligentCodebaseEngine::IntelligentCodebaseEngine() = default;
IntelligentCodebaseEngine::~IntelligentCodebaseEngine() = default;

std::vector<SymbolInfo> IntelligentCodebaseEngine::getSymbolsInFile(const std::string& filePath) {
    std::vector<SymbolInfo> symbols;

    std::ifstream in(filePath);
    if (!in.is_open()) {
        return symbols;
    }

    std::string line;
    int lineNumber = 0;
    const std::regex classRx(R"(\b(class|struct)\s+([A-Za-z_]\w*))");
    const std::regex funcRx(
        R"(\b([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(const)?\s*(\{|;))");

    while (std::getline(in, line)) {
        ++lineNumber;

        std::smatch m;
        if (std::regex_search(line, m, classRx)) {
            SymbolInfo s;
            s.type = m[1].str();
            s.name = m[2].str();
            s.filePath = filePath;
            s.lineNumber = lineNumber;
            s.signature = m[0].str();
            symbols.push_back(std::move(s));
        }

        if (std::regex_search(line, m, funcRx)) {
            SymbolInfo s;
            s.type = "function";
            s.returnType = m[1].str();
            s.name = m[2].str();
            s.filePath = filePath;
            s.lineNumber = lineNumber;
            s.signature = m[0].str();
            s.isConst = m[4].matched;

            std::istringstream args(m[3].str());
            std::string arg;
            while (std::getline(args, arg, ',')) {
                if (!arg.empty()) {
                    s.parameters.push_back(arg);
                }
            }
            symbols.push_back(std::move(s));
        }
    }

    return symbols;
}
