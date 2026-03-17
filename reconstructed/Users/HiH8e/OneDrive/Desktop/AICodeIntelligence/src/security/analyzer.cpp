#include "security_analyzer.hpp"
#include "utils.hpp"

#include <regex>

static void add(std::vector<Finding>& out, const char* id, const char* title, const char* sev, const std::string& file, int line, const std::string& details) {
    out.push_back(Finding{ id, title, sev, file, line, details });
}

std::vector<Finding> SecurityAnalyzer::analyze(const std::string& language, const std::string& path, const std::string& content) const {
    std::vector<Finding> out;
    auto lines = split_lines(content);

    if (language == "clike") {
        std::regex rx_strcpy("\\b(strcpy|strcat|gets)\\s*\\(");
        std::regex rx_sprintf("\\b(sprintf|vsprintf)\\s*\\(");
        std::regex rx_system("\\b(system|popen)\\s*\\(");
        std::regex rx_secret(R"((api[_-]?key|secret|password)\s*[:=]\s*\"[^\"]{8,}\")", std::regex::icase);
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& L = lines[i];
            if (std::regex_search(L, rx_strcpy)) add(out, "CL001", "Unsafe copy", "High", path, (int)i+1, "Use of strcpy/strcat/gets: consider strncpy/strlcpy or safe alternatives.");
            if (std::regex_search(L, rx_sprintf)) add(out, "CL002", "Unbounded sprintf", "Medium", path, (int)i+1, "Use snprintf/safe formatting to avoid buffer overflows.");
            if (std::regex_search(L, rx_system)) add(out, "CL003", "Command execution", "High", path, (int)i+1, "Use of system/popen can lead to injection; validate inputs or avoid.");
            if (std::regex_search(L, rx_secret)) add(out, "CL004", "Hardcoded secret", "High", path, (int)i+1, "Potential hardcoded credential detected.");
        }
    } else if (language == "python") {
        std::regex rx_eval("\\b(eval|exec)\\s*\\(");
        std::regex rx_os_system("\\bos\\s*\\.\\s*system\\s*\\(");
        std::regex rx_subproc_shell("subprocess\\s*\\.\\s*(Popen|call|run)\\s*\\([^)]*shell\\s*=\\s*True");
        std::regex rx_pickle("\\bpickle\\s*\\.\\s*loads?\\s*\\(");
        std::regex rx_secret(R"((api[_-]?key|secret|password)\s*[:=]\s*\"[^\"]{8,}\")", std::regex::icase);
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& L = lines[i];
            if (std::regex_search(L, rx_eval)) add(out, "PY001", "Dynamic eval/exec", "High", path, (int)i+1, "Avoid eval/exec; use safe parsing or whitelists.");
            if (std::regex_search(L, rx_os_system)) add(out, "PY002", "os.system usage", "High", path, (int)i+1, "Command execution can be unsafe; validate inputs or avoid.");
            if (std::regex_search(L, rx_subproc_shell)) add(out, "PY003", "subprocess with shell=True", "High", path, (int)i+1, "shell=True risks injection; pass args list and avoid shell if possible.");
            if (std::regex_search(L, rx_pickle)) add(out, "PY004", "pickle deserialization", "Medium", path, (int)i+1, "Untrusted pickle data can execute code; avoid or restrict sources.");
            if (std::regex_search(L, rx_secret)) add(out, "PY005", "Hardcoded secret", "High", path, (int)i+1, "Potential hardcoded credential detected.");
        }
    } else {
        // could add generic patterns here
    }

    return out;
}
