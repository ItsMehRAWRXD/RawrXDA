#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace RawrXD::Security {

struct SastFinding {
    std::string ruleId;
    std::string severity;
    std::string file;
    int line = 0;
    std::string message;
};

struct SastRule {
    std::string id;
    std::string severity;
    std::string message;
    std::regex pattern;
};

class SastEngine {
public:
    SastEngine() {
        rules_.push_back({"RAWRXD_SQLI_001", "high", "Potential SQL injection string concatenation", std::regex("(SELECT|INSERT|UPDATE|DELETE).*(\\+|std::string\\()")});
        rules_.push_back({"RAWRXD_CMD_001", "high", "Potential command injection via system/popen", std::regex("(system\\s*\\(|popen\\s*\\()")});
        rules_.push_back({"RAWRXD_SECRET_001", "critical", "Hardcoded credential/token pattern", std::regex("(api[_-]?key|token|secret|password)\\s*[:=]\\s*[\"'][^\"']{8,}[\"']", std::regex::icase)});
        rules_.push_back({"RAWRXD_MEM_001", "medium", "Unsafe C string copy", std::regex("(strcpy\\s*\\(|strcat\\s*\\()")});
    }

    std::vector<SastFinding> scanPath(const std::string& root) {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<SastFinding> out;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".c" && ext != ".h" && ext != ".hpp" && ext != ".py" && ext != ".js") continue;
            auto findings = scanFile(entry.path().string());
            out.insert(out.end(), findings.begin(), findings.end());
        }
        return out;
    }

    std::vector<SastFinding> scanFile(const std::string& file) {
        std::vector<SastFinding> out;
        std::ifstream in(file);
        if (!in.is_open()) return out;

        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            for (const auto& rule : rules_) {
                if (std::regex_search(line, rule.pattern)) {
                    out.push_back({rule.id, rule.severity, file, lineNo, rule.message});
                }
            }
        }
        return out;
    }

private:
    std::mutex mu_;
    std::vector<SastRule> rules_;
};

static SastEngine g_sast;

} // namespace RawrXD::Security

extern "C" {

int RawrXD_Security_SastScanPath(const char* rootPath) {
    if (!rootPath) return 0;
    auto findings = RawrXD::Security::g_sast.scanPath(rootPath);
    return static_cast<int>(findings.size());
}

int RawrXD_Security_SastScanFile(const char* filePath) {
    if (!filePath) return 0;
    auto findings = RawrXD::Security::g_sast.scanFile(filePath);
    return static_cast<int>(findings.size());
}

}
