#include "python_analyzer.hpp"
#include "../utils.hpp"

#include <regex>

static bool is_py_control(const std::string& w) {
    static const char* words[] = {"if","for","while","return","elif","and","or","not","lambda","with","class","def"};
    for (auto* x : words) if (w == x) return true; return false;
}

AnalysisResult PythonAnalyzer::analyze(const std::string& path, const std::string& content) {
    AnalysisResult ar;
    auto lines = split_lines(content);
    bool in_triple = false;
    char triple_char = '\0';
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c){return !std::isspace(c);}));

        // triple-quoted strings used as docstrings
        if (!in_triple) {
            if (trimmed.rfind("'''", 0) == 0 || trimmed.rfind("\"\"\"", 0) == 0) {
                in_triple = true; triple_char = trimmed[0];
                ar.metrics.commentLines++;
                if (trimmed.find(triple_char == '\'' ? "'''" : "\"\"\"") != std::string::npos && trimmed.size() > 5) in_triple = false;
                ar.metrics.lines++;
                continue;
            }
        } else {
            ar.metrics.commentLines++; ar.metrics.lines++;
            if (line.find(triple_char == '\'' ? "'''" : "\"\"\"") != std::string::npos) in_triple = false;
            continue;
        }

        if (trimmed.rfind("#", 0) == 0) {
            ar.metrics.commentLines++; ar.metrics.lines++;
            continue;
        }

        ar.metrics.lines++;
        if (!trimmed.empty()) ar.metrics.codeLines++;

        // complexity heuristics
        static const std::regex rx_complex("\\b(if|for|while|elif|and|or)\\b");
        if (std::regex_search(line, rx_complex)) ar.metrics.complexity++;

        // def and class
        static const std::regex rx_def(R"(^\s*def\s+([A-Za-z_]\w*)\s*\()");
        static const std::regex rx_cls(R"(^\s*class\s+([A-Za-z_]\w*))");
        std::smatch m;
        if (std::regex_search(line, m, rx_def)) {
            ar.symbols.push_back(Symbol{m[1].str(), "function", path, static_cast<int>(i + 1)});
        }
        if (std::regex_search(line, m, rx_cls)) {
            ar.symbols.push_back(Symbol{m[1].str(), "class", path, static_cast<int>(i + 1)});
        }

        // calls: name(
        static const std::regex rx_call(R"(\b([A-Za-z_]\w*)\s*\()");
        auto begin = std::sregex_iterator(line.begin(), line.end(), rx_call);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            const std::string name = (*it)[1].str();
            if (!is_py_control(name)) {
                ar.references.push_back(Reference{name, path, static_cast<int>(i + 1)});
            }
        }
    }

    if (ar.metrics.complexity < 1) ar.metrics.complexity = 1;
    return ar;
}
