// dependency_audit.hpp — In-house SCA: parse manifests, no external deps
// Parses package.json, requirements.txt, CMakeLists.txt; reports to ProblemsAggregator.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Security {

struct DependencyEntry {
    std::string name;
    std::string version;   // optional
    std::string manifestPath;
    int         line       = 0;
    std::string ecosystem; // "npm", "pip", "cmake", etc.
};

/** In-house dependency/SCA audit. Parses manifests with our own code only. */
class DependencyAudit {
public:
    DependencyAudit() = default;

    /** Audit a single file (package.json, requirements.txt, CMakeLists.txt, etc.). */
    bool auditFile(const std::string& path, std::vector<DependencyEntry>& out);

    /** Audit a directory: find manifests and audit each. */
    size_t auditDirectory(const std::string& dirPath, std::vector<DependencyEntry>& out);

    /** Push collected entries to ProblemsAggregator as SCA info (source "SCA"). */
    void reportToProblems(const std::vector<DependencyEntry>& entries);

private:
    bool auditPackageJson(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out);
    bool auditRequirementsTxt(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out);
    bool auditCMakeLists(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out);
    bool auditCargoToml(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out);
    bool auditVcpkgJson(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out);
};

} // namespace Security
} // namespace RawrXD
