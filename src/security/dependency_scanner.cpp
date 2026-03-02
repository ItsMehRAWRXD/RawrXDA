#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

namespace RawrXD::Security {

struct DependencyRecord {
    std::string ecosystem;
    std::string name;
    std::string version;
};

class DependencyScanner {
public:
    std::vector<DependencyRecord> scanWorkspace(const std::string& root) {
        std::vector<DependencyRecord> out;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto fn = entry.path().filename().string();
            if (fn == "package.json") parsePackageJson(entry.path().string(), out);
            else if (fn == "requirements.txt") parseRequirements(entry.path().string(), out);
            else if (fn == "Cargo.toml") parseCargoToml(entry.path().string(), out);
            else if (entry.path().extension() == ".csproj") parseCsproj(entry.path().string(), out);
        }
        return out;
    }

private:
    void parsePackageJson(const std::string& file, std::vector<DependencyRecord>& out) {
        std::ifstream in(file); if (!in.is_open()) return;
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::regex depRe("\"([@a-zA-Z0-9._/-]+)\"\\s*:\\s*\"([^\"]+)\"");
        for (auto it = std::sregex_iterator(all.begin(), all.end(), depRe); it != std::sregex_iterator(); ++it) {
            out.push_back({"npm", (*it)[1].str(), (*it)[2].str()});
        }
    }

    void parseRequirements(const std::string& file, std::vector<DependencyRecord>& out) {
        std::ifstream in(file); if (!in.is_open()) return;
        std::string line;
        std::regex reqRe("^([a-zA-Z0-9_.-]+)==([^\\s]+)");
        while (std::getline(in, line)) {
            std::smatch m;
            if (std::regex_search(line, m, reqRe)) out.push_back({"pip", m[1].str(), m[2].str()});
        }
    }

    void parseCargoToml(const std::string& file, std::vector<DependencyRecord>& out) {
        std::ifstream in(file); if (!in.is_open()) return;
        std::string line;
        std::regex cargoRe("^([a-zA-Z0-9_-]+)\\s*=\\s*\"([^\"]+)\"");
        while (std::getline(in, line)) {
            std::smatch m;
            if (std::regex_search(line, m, cargoRe)) out.push_back({"cargo", m[1].str(), m[2].str()});
        }
    }

    void parseCsproj(const std::string& file, std::vector<DependencyRecord>& out) {
        std::ifstream in(file); if (!in.is_open()) return;
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::regex csRe("PackageReference\\s+Include=\"([^\"]+)\"\\s+Version=\"([^\"]+)\"");
        for (auto it = std::sregex_iterator(all.begin(), all.end(), csRe); it != std::sregex_iterator(); ++it) {
            out.push_back({"nuget", (*it)[1].str(), (*it)[2].str()});
        }
    }
};

static DependencyScanner g_dep;

} // namespace RawrXD::Security

extern "C" {

int RawrXD_Security_ScanDependencies(const char* workspaceRoot) {
    if (!workspaceRoot) return 0;
    auto deps = RawrXD::Security::g_dep.scanWorkspace(workspaceRoot);
    return static_cast<int>(deps.size());
}

}
