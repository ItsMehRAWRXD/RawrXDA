#include "clike_analyzer.hpp"
#include "../utils.hpp"

#include <regex>

static bool is_control_word(const std::string& w) {
    static const char* words[] = {"if","for","while","switch","return","catch","sizeof"};
    for (auto* x : words) if (w == x) return true; return false;
}

AnalysisResult CLikeAnalyzer::analyze(const std::string& path, const std::string& content) {
    AnalysisResult ar;
    auto lines = split_lines(content);

    bool in_block_comment = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        std::string trimmed = line;
        // naive trim
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c){return !std::isspace(c);}));
        bool is_comment_line = false;

        // block comment handling
        if (in_block_comment) {
            ar.metrics.commentLines++;
            if (line.find("*/") != std::string::npos) in_block_comment = false;
            continue;
        }
        if (line.find("/*") != std::string::npos) {
            in_block_comment = true;
            ar.metrics.commentLines++;
            if (line.find("*/") != std::string::npos) in_block_comment = false; // single line block
            continue;
        }
        if (trimmed.rfind("//", 0) == 0) {
            is_comment_line = true;
            ar.metrics.commentLines++;
        }

        ar.metrics.lines++;
        if (!trimmed.empty() && !is_comment_line) ar.metrics.codeLines++;

        // complexity heuristics
        static const std::regex rx_complex("\\b(if|for|while|case|&&|\\|\\|)\\b");
        if (std::regex_search(line, rx_complex)) ar.metrics.complexity++;

        // function definition (naive): return-type name(args) {
        static const std::regex rx_fn(R"(^\s*(?:[\w:\<\>\~\*\&\s]+?)\s+([A-Za-z_\~][\w:]*)\s*\([^;{}]*\)\s*\{)");
        std::smatch m;
        if (std::regex_search(line, m, rx_fn)) {
            std::string name = m[1].str();
            if (!is_control_word(name)) {
                ar.symbols.push_back(Symbol{name, "function", path, static_cast<int>(i + 1)});
            }
        }

        // class/struct definition
        static const std::regex rx_cls(R"(^\s*(class|struct)\s+([A-Za-z_]\w*))");
        if (std::regex_search(line, m, rx_cls)) {
            ar.symbols.push_back(Symbol{m[2].str(), "class", path, static_cast<int>(i + 1)});
        }

        // calls: name(args); avoid control words
        static const std::regex rx_call(R"(\b([A-Za-z_]\w*)\s*\()");
        auto begin = std::sregex_iterator(line.begin(), line.end(), rx_call);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string name = (*it)[1].str();
            if (!is_control_word(name)) {
                ar.references.push_back(Reference{name, path, static_cast<int>(i + 1)});
            }
        }
    }

    // ensure baseline complexity >= 1
    if (ar.metrics.complexity < 1) ar.metrics.complexity = 1;
    return ar;
}
