#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD::Security {

struct SecretHit {
    std::string file;
    int line = 0;
    std::string type;
};

class SecretScanner {
public:
    std::vector<SecretHit> scanPath(const std::string& root) {
        std::vector<SecretHit> out;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".jpg" || ext == ".exe" || ext == ".dll") continue;
            auto hits = scanFile(entry.path().string());
            out.insert(out.end(), hits.begin(), hits.end());
        }
        return out;
    }

private:
    std::vector<SecretHit> scanFile(const std::string& file) {
        std::vector<SecretHit> out;
        std::ifstream in(file);
        if (!in.is_open()) return out;

        static const std::vector<std::pair<std::string, std::regex>> patterns = {
            {"AWS Access Key", std::regex("AKIA[0-9A-Z]{16}")},
            {"OpenAI Key", std::regex("sk-[A-Za-z0-9]{20,}")},
            {"Private Key", std::regex("-----BEGIN (RSA|EC|OPENSSH|PRIVATE) KEY-----")},
            {"Generic Password", std::regex("(password|passwd|pwd)\\s*[:=]\\s*[\"'][^\"']{6,}[\"']", std::regex::icase)}
        };

        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            for (const auto& p : patterns) {
                if (std::regex_search(line, p.second)) {
                    out.push_back({file, lineNo, p.first});
                }
            }
        }
        return out;
    }
};

static SecretScanner g_scanner;

} // namespace RawrXD::Security

extern "C" {

int RawrXD_Security_SecretScanPath(const char* rootPath) {
    if (!rootPath) return 0;
    auto hits = RawrXD::Security::g_scanner.scanPath(rootPath);
    return static_cast<int>(hits.size());
}

}
