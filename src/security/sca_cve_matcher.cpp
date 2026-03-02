#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace RawrXD::Security {

struct Dependency {
    std::string name;
    std::string version;
};

struct CveEntry {
    std::string package;
    std::string minVersion;
    std::string maxVersion;
    std::string cve;
    std::string severity;
    std::string fixed;
};

class ScaCveMatcher {
public:
    bool loadDeps(const std::string& depsPath) {
        deps_.clear();
        std::ifstream in(depsPath);
        if (!in.is_open()) return false;
        std::string line;
        while (std::getline(in, line)) {
            std::stringstream ss(line);
            std::string n, v;
            if (!std::getline(ss, n, ',')) continue;
            if (!std::getline(ss, v, ',')) continue;
            deps_.push_back({n, v});
        }
        return true;
    }

    bool loadCveDb(const std::string& dbPath) {
        db_.clear();
        std::ifstream in(dbPath);
        if (!in.is_open()) return false;
        std::string line;
        while (std::getline(in, line)) {
            std::stringstream ss(line);
            CveEntry e;
            if (!std::getline(ss, e.package, ',')) continue;
            if (!std::getline(ss, e.minVersion, ',')) continue;
            if (!std::getline(ss, e.maxVersion, ',')) continue;
            if (!std::getline(ss, e.cve, ',')) continue;
            if (!std::getline(ss, e.severity, ',')) continue;
            if (!std::getline(ss, e.fixed, ',')) continue;
            db_.push_back(e);
        }
        return true;
    }

    int matchCount() const {
        int count = 0;
        for (const auto& d : deps_) {
            for (const auto& c : db_) {
                if (d.name == c.package && inRange(d.version, c.minVersion, c.maxVersion)) {
                    ++count;
                }
            }
        }
        return count;
    }

private:
    static bool inRange(const std::string& v, const std::string& minV, const std::string& maxV) {
        // Lightweight lexical-semver comparison fallback
        return (v >= minV && v <= maxV);
    }

    std::vector<Dependency> deps_;
    std::vector<CveEntry> db_;
};

static ScaCveMatcher g_sca;

} // namespace RawrXD::Security

extern "C" {

bool RawrXD_Security_ScaLoadDependencies(const char* depsCsvPath) {
    if (!depsCsvPath) return false;
    return RawrXD::Security::g_sca.loadDeps(depsCsvPath);
}

bool RawrXD_Security_ScaLoadCveDb(const char* cveCsvPath) {
    if (!cveCsvPath) return false;
    return RawrXD::Security::g_sca.loadCveDb(cveCsvPath);
}

int RawrXD_Security_ScaMatchCount() {
    return RawrXD::Security::g_sca.matchCount();
}

}
