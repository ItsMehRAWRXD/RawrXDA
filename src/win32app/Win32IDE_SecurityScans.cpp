// Win32IDE_SecurityScans.cpp — Security scan menu handlers (Top-50 P0)
// Wires Secrets, SAST, Dependency Audit into IDE; pushes results to ProblemsAggregator.
#include "Win32IDE.h"
#include "../core/problems_aggregator.hpp"
#include "../security/secrets_scanner.hpp"
#include "../security/sast_rule_engine.hpp"
#include "../security/dependency_audit.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// RunSecretsScan — Scan workspace/current file for API keys, tokens, high-entropy
// ---------------------------------------------------------------------------
void Win32IDE::RunSecretsScan() {
    using namespace RawrXD;
    using namespace RawrXD::Security;

    auto& agg = ProblemsAggregator::instance();
    agg.clear("Secrets");

    SecretsScanner scanner;
    std::vector<SecretFinding> findings;

    std::string root = m_currentDirectory.empty() ? "." : m_currentDirectory;

    // Scan current file if open
    if (!m_currentFile.empty()) {
        std::ifstream f(m_currentFile, std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            std::string content = ss.str();
            size_t n = scanner.scan(content, findings);
            scanner.reportToProblems(findings, m_currentFile);
            findings.clear();
            if (n > 0) appendToOutput("[Secrets] Found " + std::to_string(n) + " potential secret(s) in " + m_currentFile + "\n",
                "Output", OutputSeverity::Warning);
        }
    }

    // Scan workspace: iterate common config/source files (limited depth)
    try {
        int fileCount = 0;
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            // Focus on configs and source
            bool scan = (ext == ".json" || ext == ".env" || ext == ".config" || ext == ".yaml" || ext == ".yml" ||
                        ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".cpp" || ext == ".hpp" ||
                        ext == ".c" || ext == ".h" || ext == ".ps1" || path.find(".env") != std::string::npos);
            if (!scan) continue;
            if (path.find("node_modules") != std::string::npos || path.find(".git") != std::string::npos) continue;
            if (fileCount >= 500) break; // Limit scope
            std::ifstream f(path, std::ios::binary);
            if (!f) continue;
            std::ostringstream ss;
            ss << f.rdbuf();
            std::string content = ss.str();
            if (content.size() > 1024 * 1024) continue; // Skip >1MB
            size_t n = scanner.scan(content, findings);
            if (n > 0) {
                scanner.reportToProblems(findings, path);
                findings.clear();
            }
            fileCount++;
        }
    } catch (...) { /* ignore traversal errors */ }

    refreshProblemsView();
    appendToOutput("[Secrets] Scan complete. See Problems panel.\n", "Output", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// RunSastScan — Run SAST rules over current file or workspace
// ---------------------------------------------------------------------------
void Win32IDE::RunSastScan() {
    using namespace RawrXD;
    using namespace RawrXD::Security;

    auto& agg = ProblemsAggregator::instance();
    agg.clear("SAST");

    SastRuleEngine engine;
    std::vector<SastFinding> findings;

    auto scanFile = [&](const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return;
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        std::string ext = fs::path(path).extension().string();
        bool isCpp = (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" || ext == ".cc");
        if (!isCpp) return;
        engine.scan(path, content, findings);
    };

    if (!m_currentFile.empty()) {
        scanFile(m_currentFile);
    }

    std::string root = m_currentDirectory.empty() ? "." : m_currentDirectory;
    try {
        int count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (count >= 200) break;
            std::string path = entry.path().string();
            if (path.find(".git") != std::string::npos || path.find("node_modules") != std::string::npos) continue;
            scanFile(path);
            count++;
        }
    } catch (...) {}

    engine.reportToProblems(findings);
    refreshProblemsView();
    appendToOutput("[SAST] Scan complete. Found " + std::to_string(findings.size()) + " issue(s). See Problems panel.\n",
        "Output", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// RunDependencyAudit — SCA: audit package.json, requirements.txt, CMakeLists, etc.
// ---------------------------------------------------------------------------
void Win32IDE::RunDependencyAudit() {
    using namespace RawrXD;
    using namespace RawrXD::Security;

    auto& agg = ProblemsAggregator::instance();
    agg.clear("SCA");

    DependencyAudit depAudit;
    std::vector<DependencyEntry> entries;

    std::string root = m_currentDirectory.empty() ? "." : m_currentDirectory;
    size_t n = depAudit.auditDirectory(root, entries);
    depAudit.reportToProblems(entries);

    refreshProblemsView();
    appendToOutput("[SCA] Dependency audit complete. " + std::to_string(n) + " manifest(s) scanned. See Problems panel.\n",
        "Output", OutputSeverity::Info);
}
