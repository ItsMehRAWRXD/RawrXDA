#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>

namespace RawrXD::Security {

struct CveRecord {
    std::string cve;
    std::string package;
    std::string affectedRange;
    std::string severity;
};

class CveCache {
public:
    bool load(const std::string& path) {
        std::lock_guard<std::mutex> lock(mu_);
        std::ifstream in(path);
        if (!in.is_open()) return false;

        db_.clear();
        std::string line;
        while (std::getline(in, line)) {
            // CSV: package,cve,range,severity
            std::stringstream ss(line);
            std::string pkg, cve, range, sev;
            if (!std::getline(ss, pkg, ',')) continue;
            if (!std::getline(ss, cve, ',')) continue;
            if (!std::getline(ss, range, ',')) continue;
            if (!std::getline(ss, sev, ',')) continue;
            db_[pkg].push_back({cve, pkg, range, sev});
        }
        return true;
    }

    std::vector<CveRecord> lookup(const std::string& package) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = db_.find(package);
        if (it == db_.end()) return {};
        return it->second;
    }

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::vector<CveRecord>> db_;
};

static CveCache g_cve;

} // namespace RawrXD::Security

extern "C" {

bool RawrXD_Security_LoadCveCache(const char* csvPath) {
    if (!csvPath) return false;
    return RawrXD::Security::g_cve.load(csvPath);
}

int RawrXD_Security_QueryCveCount(const char* packageName) {
    if (!packageName) return 0;
    auto matches = RawrXD::Security::g_cve.lookup(packageName);
    return static_cast<int>(matches.size());
}

}
