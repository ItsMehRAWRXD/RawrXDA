// change_impact_analyzer.cpp
// Intelligent pre-commit change impact analysis — full implementation
// Predicts file/dependency ripple effects BEFORE commits

#include "change_impact_analyzer.hpp"
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <array>

namespace Agentic {

// ============================================================================
// ImpactReport Serialization
// ============================================================================

nlohmann::json ImpactReport::toJson() const {
    nlohmann::json j;
    j["report_id"] = report_id;
    j["workspace_root"] = workspace_root;
    j["git_branch"] = git_branch;
    j["git_commit"] = git_commit_hash;
    j["overall_severity"] = static_cast<int>(overall_severity);
    j["risk_score"] = risk_score;
    j["total_files_affected"] = total_files_affected;
    j["critical_files_affected"] = critical_files_affected;
    j["api_breaking_changes"] = api_breaking_changes;
    j["untested_changes"] = untested_changes;
    j["requires_full_rebuild"] = requires_full_rebuild;
    j["estimated_rebuild_time_sec"] = estimated_rebuild_time_sec;
    j["should_block_commit"] = should_block_commit;
    j["block_reason"] = block_reason;
    j["total_lines_added"] = total_lines_added;
    j["total_lines_removed"] = total_lines_removed;
    
    nlohmann::json changes = nlohmann::json::array();
    for (const auto& fc : changed_files) {
        nlohmann::json c;
        c["path"] = fc.path;
        c["category"] = static_cast<int>(fc.category);
        c["lines_added"] = fc.lines_added;
        c["lines_removed"] = fc.lines_removed;
        c["is_header"] = fc.is_header;
        c["is_critical"] = fc.is_critical;
        c["modifies_api"] = fc.modifies_api;
        changes.push_back(c);
    }
    j["changed_files"] = changes;
    
    nlohmann::json zones = nlohmann::json::array();
    for (const auto& z : impact_zones) {
        nlohmann::json zj;
        zj["source"] = z.source_file;
        zj["directly_affected"] = z.directly_affected;
        zj["transitively_affected"] = z.transitively_affected;
        zj["test_files"] = z.test_files_relevant;
        zj["build_targets"] = z.build_targets_affected;
        zones.push_back(zj);
    }
    j["impact_zones"] = zones;
    
    j["warnings"] = warnings;
    j["suggestions"] = suggestions;
    return j;
}

std::string ImpactReport::toSummary() const {
    std::ostringstream ss;
    
    static const std::array<const char*, 6> severity_names = {
        "None", "Trivial", "Minor", "Moderate", "Major", "Critical"
    };
    
    ss << "=== CHANGE IMPACT REPORT [" << report_id << "] ===\n";
    ss << "Severity: " << severity_names[static_cast<int>(overall_severity)] << "\n";
    ss << "Risk Score: " << (risk_score * 100.0f) << "%\n";
    ss << "Changed Files: " << changed_files.size() << "\n";
    ss << "Total Affected: " << total_files_affected << "\n";
    ss << "Critical Files: " << critical_files_affected << "\n";
    ss << "API Breaks: " << api_breaking_changes << "\n";
    ss << "Untested Changes: " << untested_changes << "\n";
    ss << "Lines: +" << total_lines_added << " / -" << total_lines_removed << "\n";
    
    if (should_block_commit) {
        ss << "\n*** COMMIT BLOCKED: " << block_reason << " ***\n";
    }
    
    if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& w : warnings) {
            ss << "  ! " << w << "\n";
        }
    }
    
    if (!suggestions.empty()) {
        ss << "\nSuggestions:\n";
        for (const auto& s : suggestions) {
            ss << "  > " << s << "\n";
        }
    }
    
    return ss.str();
}

std::string ImpactReport::toDetailedReport() const {
    std::ostringstream ss;
    ss << toSummary();
    
    ss << "\n--- IMPACT ZONES ---\n";
    for (const auto& zone : impact_zones) {
        ss << "\n[" << zone.source_file << "]\n";
        ss << "  Direct (" << zone.direct_count() << "): ";
        for (size_t i = 0; i < zone.directly_affected.size() && i < 10; ++i) {
            if (i > 0) ss << ", ";
            ss << zone.directly_affected[i];
        }
        if (zone.directly_affected.size() > 10)
            ss << " (+" << (zone.directly_affected.size() - 10) << " more)";
        ss << "\n";
        
        if (!zone.transitively_affected.empty()) {
            ss << "  Transitive (" << zone.transitive_count() << "): ";
            for (size_t i = 0; i < zone.transitively_affected.size() && i < 5; ++i) {
                if (i > 0) ss << ", ";
                ss << zone.transitively_affected[i];
            }
            if (zone.transitively_affected.size() > 5)
                ss << " (+" << (zone.transitively_affected.size() - 5) << " more)";
            ss << "\n";
        }
        
        if (!zone.test_files_relevant.empty()) {
            ss << "  Tests (" << zone.test_files_relevant.size() << "): ";
            for (size_t i = 0; i < zone.test_files_relevant.size() && i < 3; ++i) {
                if (i > 0) ss << ", ";
                ss << zone.test_files_relevant[i];
            }
            ss << "\n";
        }
    }
    
    ss << "\n--- CHANGED FILES ---\n";
    for (const auto& fc : changed_files) {
        ss << "  " << fc.path
           << " [+" << fc.lines_added << "/-" << fc.lines_removed << "]";
        if (fc.is_critical) ss << " [CRITICAL]";
        if (fc.modifies_api) ss << " [API]";
        if (!fc.has_tests) ss << " [NO-TEST]";
        ss << "\n";
    }
    
    return ss.str();
}

// ============================================================================
// ImpactAnalyzerConfig
// ============================================================================

nlohmann::json ImpactAnalyzerConfig::toJson() const {
    nlohmann::json j;
    j["block_threshold"] = block_commit_threshold;
    j["critical_file_limit"] = critical_file_limit;
    j["api_break_limit"] = api_break_limit;
    j["max_affected_files"] = max_affected_files;
    j["max_transitive_depth"] = max_transitive_depth;
    j["include_test_impact"] = include_test_impact;
    j["include_build_targets"] = include_build_targets;
    j["auto_rollback_plan"] = auto_generate_rollback_plan;
    j["integrate_approval_gates"] = integrate_with_approval_gates;
    return j;
}

ImpactAnalyzerConfig ImpactAnalyzerConfig::fromJson(const nlohmann::json& j) {
    ImpactAnalyzerConfig cfg;
    if (j.contains("block_threshold"))      cfg.block_commit_threshold = j["block_threshold"].get<float>();
    if (j.contains("critical_file_limit"))  cfg.critical_file_limit = j["critical_file_limit"].get<int>();
    if (j.contains("api_break_limit"))      cfg.api_break_limit = j["api_break_limit"].get<int>();
    if (j.contains("max_affected_files"))   cfg.max_affected_files = j["max_affected_files"].get<int>();
    if (j.contains("max_transitive_depth")) cfg.max_transitive_depth = j["max_transitive_depth"].get<int>();
    return cfg;
}

ImpactAnalyzerConfig ImpactAnalyzerConfig::Default() {
    return ImpactAnalyzerConfig{};
}

ImpactAnalyzerConfig ImpactAnalyzerConfig::Strict() {
    ImpactAnalyzerConfig cfg;
    cfg.block_commit_threshold = 0.60f;
    cfg.critical_file_limit = 1;
    cfg.api_break_limit = 0;
    cfg.max_affected_files = 50;
    cfg.max_transitive_depth = 8;
    return cfg;
}

ImpactAnalyzerConfig ImpactAnalyzerConfig::Permissive() {
    ImpactAnalyzerConfig cfg;
    cfg.block_commit_threshold = 0.95f;
    cfg.critical_file_limit = 10;
    cfg.api_break_limit = 5;
    cfg.max_affected_files = 500;
    cfg.max_transitive_depth = 3;
    return cfg;
}

// ============================================================================
// Constructor / Initialization
// ============================================================================

ChangeImpactAnalyzer::ChangeImpactAnalyzer()
    : m_config(ImpactAnalyzerConfig::Default()), m_reportIdCounter(0) {
}

ChangeImpactAnalyzer::~ChangeImpactAnalyzer() = default;

void ChangeImpactAnalyzer::setConfig(const ImpactAnalyzerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

ImpactAnalyzerConfig ChangeImpactAnalyzer::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void ChangeImpactAnalyzer::setWorkspaceRoot(const std::string& root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_workspaceRoot = root;
}

void ChangeImpactAnalyzer::setDependencyResolver(DependencyResolverFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depResolver = std::move(fn);
}

void ChangeImpactAnalyzer::setReverseDependencyResolver(ReverseDependencyFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_revDepResolver = std::move(fn);
}

void ChangeImpactAnalyzer::setCriticalFileCheck(CriticalFileCheckFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_criticalCheck = std::move(fn);
}

void ChangeImpactAnalyzer::setBuildTargetResolver(BuildTargetResolverFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buildTargetResolver = std::move(fn);
}

// ============================================================================
// Git Integration
// ============================================================================

std::string ChangeImpactAnalyzer::runGitCommand(const std::string& args) const {
    std::string cmd = "git -C \"" + m_workspaceRoot + "\" " + args;
    
    // Use _popen for subprocess on Windows
    std::string result;
    std::array<char, 4096> buffer;
    
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    
    if (!pipe) return "";
    
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return result;
}

void ChangeImpactAnalyzer::parseGitDiff(const std::string& diff_output,
                                         std::vector<FileChange>& changes) {
    std::istringstream stream(diff_output);
    std::string line;
    FileChange current;
    bool in_file = false;
    
    while (std::getline(stream, line)) {
        // Parse diff --numstat format: added\tremoved\tfilename
        if (line.size() >= 3 && (line[0] >= '0' && line[0] <= '9')) {
            if (in_file && !current.path.empty()) {
                changes.push_back(current);
            }
            
            current = FileChange{};
            in_file = true;
            
            std::istringstream ls(line);
            ls >> current.lines_added >> current.lines_removed >> current.path;
            current.lines_modified = std::min(current.lines_added, current.lines_removed);
            
            // Detect file type
            auto ext_pos = current.path.rfind('.');
            if (ext_pos != std::string::npos) {
                std::string ext = current.path.substr(ext_pos);
                current.is_header = (ext == ".h" || ext == ".hpp" || ext == ".hxx");
            }
            
            // Check if critical
            if (m_criticalCheck) {
                current.is_critical = m_criticalCheck(current.path);
            }
            
            // Categorize
            current.category = categorizeChange(current);
        }
        // Parse diff --name-status format: M\tfilename or A\tfilename
        else if (line.size() >= 2 && line[1] == '\t') {
            if (in_file && !current.path.empty()) {
                changes.push_back(current);
            }
            
            current = FileChange{};
            in_file = true;
            current.path = line.substr(2);
            
            switch (line[0]) {
                case 'A': current.category = ChangeCategory::NewFile; break;
                case 'D': current.category = ChangeCategory::DeletedFile; break;
                case 'R': current.category = ChangeCategory::Rename; break;
                case 'M': current.category = ChangeCategory::InternalLogic; break;
                default:  current.category = ChangeCategory::Unknown; break;
            }
            
            auto ext_pos = current.path.rfind('.');
            if (ext_pos != std::string::npos) {
                std::string ext = current.path.substr(ext_pos);
                current.is_header = (ext == ".h" || ext == ".hpp" || ext == ".hxx");
            }
        }
    }
    
    if (in_file && !current.path.empty()) {
        changes.push_back(current);
    }
}

// ============================================================================
// Core Analysis
// ============================================================================

ImpactReport ChangeImpactAnalyzer::analyzeStagedChanges() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Get staged changes via git diff --cached --numstat
    std::string numstat = runGitCommand("diff --cached --numstat");
    std::string namestat = runGitCommand("diff --cached --name-status");
    std::string branch = runGitCommand("rev-parse --abbrev-ref HEAD");
    std::string commit = runGitCommand("rev-parse --short HEAD");
    
    // Trim whitespace
    while (!branch.empty() && (branch.back() == '\n' || branch.back() == '\r'))
        branch.pop_back();
    while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r'))
        commit.pop_back();
    
    std::vector<FileChange> changes;
    parseGitDiff(numstat, changes);
    
    // If numstat gave us no results, try name-status
    if (changes.empty()) {
        parseGitDiff(namestat, changes);
    }
    
    ImpactReport report;
    report.report_id = "impact_" + std::to_string(m_reportIdCounter++);
    report.generated_at = std::chrono::system_clock::now();
    report.workspace_root = m_workspaceRoot;
    report.git_branch = branch;
    report.git_commit_hash = commit;
    report.changed_files = changes;
    
    // Compute impact zones for each changed file
    for (const auto& change : changes) {
        report.impact_zones.push_back(computeImpactZone(change));
        report.total_lines_added += change.lines_added;
        report.total_lines_removed += change.lines_removed;
    }
    
    // Aggregate metrics
    std::set<std::string> all_affected;
    for (const auto& zone : report.impact_zones) {
        for (const auto& f : zone.directly_affected)
            all_affected.insert(f);
        for (const auto& f : zone.transitively_affected)
            all_affected.insert(f);
    }
    report.total_files_affected = static_cast<int>(all_affected.size());
    
    // Count critical & untested
    for (const auto& change : changes) {
        if (change.is_critical) report.critical_files_affected++;
        if (change.modifies_api) report.api_breaking_changes++;
        if (!change.has_tests) report.untested_changes++;
    }
    
    // Compute risk and severity
    report.risk_score = computeRiskScore(report);
    report.overall_severity = classifySeverity(report);
    report.should_block_commit = shouldBlockCommit(report);
    
    // Generate warnings and suggestions
    generateWarnings(report);
    generateSuggestions(report);
    
    // Estimate rebuild time (rough: 2 sec per affected file)
    report.estimated_rebuild_time_sec = report.total_files_affected * 2;
    report.requires_full_rebuild = (report.total_files_affected > 50);
    
    // Store report
    auto stored = std::make_unique<ImpactReport>(report);
    if (m_reports.size() >= MAX_REPORTS) {
        m_reports.erase(m_reports.begin());
    }
    m_reports.push_back(std::move(stored));
    
    return report;
}

ImpactReport ChangeImpactAnalyzer::analyzeUnstagedChanges() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string numstat = runGitCommand("diff --numstat");
    std::string branch = runGitCommand("rev-parse --abbrev-ref HEAD");
    std::string commit = runGitCommand("rev-parse --short HEAD");
    
    while (!branch.empty() && (branch.back() == '\n' || branch.back() == '\r'))
        branch.pop_back();
    while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r'))
        commit.pop_back();
    
    std::vector<FileChange> changes;
    parseGitDiff(numstat, changes);
    
    ImpactReport report;
    report.report_id = "impact_unstaged_" + std::to_string(m_reportIdCounter++);
    report.generated_at = std::chrono::system_clock::now();
    report.workspace_root = m_workspaceRoot;
    report.git_branch = branch;
    report.git_commit_hash = commit;
    report.changed_files = changes;
    
    for (const auto& change : changes) {
        report.impact_zones.push_back(computeImpactZone(change));
        report.total_lines_added += change.lines_added;
        report.total_lines_removed += change.lines_removed;
    }
    
    std::set<std::string> all_affected;
    for (const auto& zone : report.impact_zones) {
        for (const auto& f : zone.directly_affected) all_affected.insert(f);
        for (const auto& f : zone.transitively_affected) all_affected.insert(f);
    }
    report.total_files_affected = static_cast<int>(all_affected.size());
    
    for (const auto& change : changes) {
        if (change.is_critical) report.critical_files_affected++;
        if (change.modifies_api) report.api_breaking_changes++;
        if (!change.has_tests) report.untested_changes++;
    }
    
    report.risk_score = computeRiskScore(report);
    report.overall_severity = classifySeverity(report);
    report.should_block_commit = shouldBlockCommit(report);
    generateWarnings(report);
    generateSuggestions(report);
    report.estimated_rebuild_time_sec = report.total_files_affected * 2;
    report.requires_full_rebuild = (report.total_files_affected > 50);
    
    auto stored = std::make_unique<ImpactReport>(report);
    if (m_reports.size() >= MAX_REPORTS) m_reports.erase(m_reports.begin());
    m_reports.push_back(std::move(stored));
    
    return report;
}

ImpactReport ChangeImpactAnalyzer::analyzeFileChanges(const std::vector<FileChange>& changes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ImpactReport report;
    report.report_id = "impact_manual_" + std::to_string(m_reportIdCounter++);
    report.generated_at = std::chrono::system_clock::now();
    report.workspace_root = m_workspaceRoot;
    report.changed_files = changes;
    
    for (const auto& change : changes) {
        report.impact_zones.push_back(computeImpactZone(change));
        report.total_lines_added += change.lines_added;
        report.total_lines_removed += change.lines_removed;
    }
    
    std::set<std::string> all_affected;
    for (const auto& zone : report.impact_zones) {
        for (const auto& f : zone.directly_affected) all_affected.insert(f);
        for (const auto& f : zone.transitively_affected) all_affected.insert(f);
    }
    report.total_files_affected = static_cast<int>(all_affected.size());
    
    for (const auto& change : changes) {
        if (change.is_critical) report.critical_files_affected++;
        if (change.modifies_api) report.api_breaking_changes++;
        if (!change.has_tests) report.untested_changes++;
    }
    
    report.risk_score = computeRiskScore(report);
    report.overall_severity = classifySeverity(report);
    report.should_block_commit = shouldBlockCommit(report);
    generateWarnings(report);
    generateSuggestions(report);
    report.estimated_rebuild_time_sec = report.total_files_affected * 2;
    report.requires_full_rebuild = (report.total_files_affected > 50);
    
    auto stored = std::make_unique<ImpactReport>(report);
    if (m_reports.size() >= MAX_REPORTS) m_reports.erase(m_reports.begin());
    m_reports.push_back(std::move(stored));
    
    return report;
}

ImpactZone ChangeImpactAnalyzer::analyzeFileImpact(const std::string& changed_file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    FileChange fc;
    fc.path = changed_file;
    fc.category = ChangeCategory::InternalLogic;
    auto ext_pos = changed_file.rfind('.');
    if (ext_pos != std::string::npos) {
        std::string ext = changed_file.substr(ext_pos);
        fc.is_header = (ext == ".h" || ext == ".hpp" || ext == ".hxx");
    }
    
    return computeImpactZone(fc);
}

// ============================================================================
// Impact Zone Computation
// ============================================================================

ImpactZone ChangeImpactAnalyzer::computeImpactZone(const FileChange& change) {
    ImpactZone zone;
    zone.source_file = change.path;
    
    // Get direct reverse dependencies (files that include/depend on this file)
    if (m_revDepResolver) {
        zone.directly_affected = m_revDepResolver(change.path);
    }
    
    // Compute transitive dependencies
    if (!zone.directly_affected.empty()) {
        std::set<std::string> visited;
        visited.insert(change.path);
        for (const auto& direct : zone.directly_affected) {
            visited.insert(direct);
        }
        
        for (const auto& direct : zone.directly_affected) {
            computeTransitiveDeps(direct, visited, zone.transitively_affected,
                                  m_config.max_transitive_depth);
        }
    }
    
    // Identify relevant test files
    if (m_config.include_test_impact) {
        auto allAffected = zone.directly_affected;
        allAffected.insert(allAffected.end(),
                          zone.transitively_affected.begin(),
                          zone.transitively_affected.end());
        allAffected.push_back(change.path);
        
        for (const auto& f : allAffected) {
            // Heuristic: files with "test" in the path are test files
            if (f.find("test") != std::string::npos ||
                f.find("Test") != std::string::npos ||
                f.find("_test.") != std::string::npos ||
                f.find("test_") != std::string::npos) {
                zone.test_files_relevant.push_back(f);
            }
        }
    }
    
    // Map to build targets
    if (m_config.include_build_targets && m_buildTargetResolver) {
        std::set<std::string> targets;
        targets.insert(m_buildTargetResolver(change.path).begin(),
                      m_buildTargetResolver(change.path).end());
        for (const auto& f : zone.directly_affected) {
            auto t = m_buildTargetResolver(f);
            targets.insert(t.begin(), t.end());
        }
        zone.build_targets_affected.assign(targets.begin(), targets.end());
    }
    
    return zone;
}

void ChangeImpactAnalyzer::computeTransitiveDeps(const std::string& file,
                                                  std::set<std::string>& visited,
                                                  std::vector<std::string>& result,
                                                  int depth) {
    if (depth <= 0) return;
    
    std::vector<std::string> deps;
    if (m_revDepResolver) {
        deps = m_revDepResolver(file);
    }
    
    for (const auto& dep : deps) {
        if (visited.find(dep) == visited.end()) {
            visited.insert(dep);
            result.push_back(dep);
            computeTransitiveDeps(dep, visited, result, depth - 1);
        }
    }
}

// ============================================================================
// Risk Scoring
// ============================================================================

float ChangeImpactAnalyzer::computeRiskScore(const ImpactReport& report) const {
    float score = 0.0f;
    
    // Factor 1: Number of affected files (0.0 - 0.3)
    float file_factor = std::min(1.0f,
        static_cast<float>(report.total_files_affected) / 
        static_cast<float>(m_config.max_affected_files));
    score += file_factor * 0.3f;
    
    // Factor 2: Critical files affected (0.0 - 0.25)
    if (report.critical_files_affected > 0) {
        float crit_factor = std::min(1.0f,
            static_cast<float>(report.critical_files_affected) / 
            static_cast<float>(m_config.critical_file_limit));
        score += crit_factor * 0.25f;
    }
    
    // Factor 3: API breaking changes (0.0 - 0.25)
    if (report.api_breaking_changes > 0) {
        float api_factor = std::min(1.0f,
            static_cast<float>(report.api_breaking_changes) / 
            static_cast<float>(std::max(1, m_config.api_break_limit)));
        score += api_factor * 0.25f;
    }
    
    // Factor 4: Untested changes (0.0 - 0.1)
    if (report.untested_changes > 0 && !report.changed_files.empty()) {
        float untested_ratio = static_cast<float>(report.untested_changes) / 
                              static_cast<float>(report.changed_files.size());
        score += untested_ratio * 0.1f;
    }
    
    // Factor 5: Header file modifications (0.0 - 0.1)
    int header_changes = 0;
    for (const auto& fc : report.changed_files) {
        if (fc.is_header) header_changes++;
    }
    if (header_changes > 0) {
        float header_factor = std::min(1.0f, static_cast<float>(header_changes) / 5.0f);
        score += header_factor * 0.1f;
    }
    
    return std::min(1.0f, score);
}

ImpactSeverity ChangeImpactAnalyzer::classifySeverity(const ImpactReport& report) const {
    if (report.total_files_affected == 0)   return ImpactSeverity::None;
    if (report.risk_score < 0.10f)          return ImpactSeverity::Trivial;
    if (report.risk_score < 0.30f)          return ImpactSeverity::Minor;
    if (report.risk_score < 0.55f)          return ImpactSeverity::Moderate;
    if (report.risk_score < 0.80f)          return ImpactSeverity::Major;
    return ImpactSeverity::Critical;
}

bool ChangeImpactAnalyzer::shouldBlockCommit(const ImpactReport& report) const {
    if (report.risk_score >= m_config.block_commit_threshold) {
        return true;
    }
    if (report.api_breaking_changes > m_config.api_break_limit) {
        return true;
    }
    if (report.critical_files_affected > m_config.critical_file_limit) {
        return true;
    }
    return false;
}

float ChangeImpactAnalyzer::scoreFileRisk(const FileChange& change) const {
    float risk = 0.0f;
    
    if (change.is_critical) risk += 0.4f;
    if (change.modifies_api) risk += 0.3f;
    if (change.is_header) risk += 0.15f;
    if (!change.has_tests) risk += 0.1f;
    
    // Large changes are riskier
    int total_lines = change.lines_added + change.lines_removed;
    if (total_lines > 500) risk += 0.05f;
    
    return std::min(1.0f, risk);
}

ChangeCategory ChangeImpactAnalyzer::categorizeChange(const FileChange& change) const {
    if (change.lines_added == 0 && change.lines_removed == 0) {
        return ChangeCategory::ContentOnly;
    }
    if (change.is_header && (change.lines_added > 0 || change.lines_removed > 0)) {
        return ChangeCategory::InterfaceChange;
    }
    // Check test files
    if (change.path.find("test") != std::string::npos ||
        change.path.find("Test") != std::string::npos) {
        return ChangeCategory::TestChange;
    }
    // Check build files
    if (change.path.find("CMakeLists") != std::string::npos ||
        change.path.find(".cmake") != std::string::npos) {
        return ChangeCategory::ConfigChange;
    }
    return ChangeCategory::InternalLogic;
}

// ============================================================================
// Warning & Suggestion Generation
// ============================================================================

void ChangeImpactAnalyzer::generateWarnings(ImpactReport& report) {
    if (report.critical_files_affected > 0) {
        report.warnings.push_back(
            "CRITICAL: " + std::to_string(report.critical_files_affected) + 
            " core infrastructure file(s) modified — review carefully");
    }
    
    if (report.api_breaking_changes > 0) {
        report.warnings.push_back(
            "API BREAK: " + std::to_string(report.api_breaking_changes) + 
            " public interface change(s) detected — downstream consumers affected");
    }
    
    if (report.untested_changes > 0) {
        report.warnings.push_back(
            "COVERAGE GAP: " + std::to_string(report.untested_changes) + 
            " changed file(s) have no test coverage");
    }
    
    if (report.total_files_affected > 50) {
        report.warnings.push_back(
            "BLAST RADIUS: " + std::to_string(report.total_files_affected) + 
            " files affected — consider splitting into smaller changes");
    }
    
    if (report.requires_full_rebuild) {
        report.warnings.push_back(
            "REBUILD: Full rebuild required — estimated " + 
            std::to_string(report.estimated_rebuild_time_sec) + "s");
    }
    
    // Check for header-only changes that have massive impact
    for (const auto& zone : report.impact_zones) {
        if (zone.total_affected() > 30) {
            report.warnings.push_back(
                "HIGH IMPACT: " + zone.source_file + " affects " + 
                std::to_string(zone.total_affected()) + " files");
        }
    }
    
    if (report.should_block_commit) {
        report.block_reason = "Risk score " + 
            std::to_string(static_cast<int>(report.risk_score * 100)) + 
            "% exceeds threshold " + 
            std::to_string(static_cast<int>(m_config.block_commit_threshold * 100)) + "%";
    }
}

void ChangeImpactAnalyzer::generateSuggestions(ImpactReport& report) {
    if (report.untested_changes > 0) {
        report.suggestions.push_back(
            "Add test coverage for modified files before committing");
    }
    
    if (report.api_breaking_changes > 0) {
        report.suggestions.push_back(
            "Update CHANGELOG and consider semantic versioning bump");
    }
    
    if (report.total_files_affected > 20 && report.changed_files.size() > 5) {
        report.suggestions.push_back(
            "Consider splitting this into multiple smaller commits for easier review");
    }
    
    // Suggest running relevant tests
    std::set<std::string> test_files;
    for (const auto& zone : report.impact_zones) {
        for (const auto& t : zone.test_files_relevant) {
            test_files.insert(t);
        }
    }
    if (!test_files.empty()) {
        report.suggestions.push_back(
            "Run " + std::to_string(test_files.size()) + " relevant test file(s) before commit");
    }
    
    // Suggest review for critical files
    if (report.critical_files_affected > 0) {
        report.suggestions.push_back(
            "Request code review from senior team member for critical file changes");
    }
}

// ============================================================================
// Report History
// ============================================================================

std::vector<const ImpactReport*> ChangeImpactAnalyzer::getRecentReports(int count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<const ImpactReport*> result;
    int start = std::max(0, static_cast<int>(m_reports.size()) - count);
    for (int i = start; i < static_cast<int>(m_reports.size()); ++i) {
        result.push_back(m_reports[i].get());
    }
    return result;
}

const ImpactReport* ChangeImpactAnalyzer::getReport(const std::string& reportId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& r : m_reports) {
        if (r->report_id == reportId) {
            return r.get();
        }
    }
    return nullptr;
}

// ============================================================================
// JSON Export
// ============================================================================

nlohmann::json ChangeImpactAnalyzer::getFullStatusJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    nlohmann::json j;
    j["workspace_root"] = m_workspaceRoot;
    j["config"] = m_config.toJson();
    j["total_reports"] = m_reports.size();
    
    if (!m_reports.empty()) {
        j["latest_report"] = m_reports.back()->toJson();
    }
    
    return j;
}

} // namespace Agentic
