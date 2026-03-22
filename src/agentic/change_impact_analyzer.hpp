// change_impact_analyzer.hpp
// Intelligent pre-commit change impact analysis
// Predicts file/dependency ripple effects BEFORE commits, integrates with AgenticPlanningOrchestrator
//
// Architecture:
//   git diff → changed files → DependencyGraph traversal → impact zones → risk scoring
//   → integration with approval gates for auto-rejection of high-risk changes

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace Agentic {

// ============================================================================
// Impact Classification
// ============================================================================

enum class ImpactSeverity : uint8_t {
    None       = 0,   // No downstream impact
    Trivial    = 1,   // Documentation/comment-only change
    Minor      = 2,   // 1-3 files affected, no API change
    Moderate   = 3,   // 4-10 files affected, internal API change
    Major      = 4,   // 11-50 files affected, public API change
    Critical   = 5    // 50+ files or core infrastructure change
};

enum class ChangeCategory : uint8_t {
    ContentOnly,         // Whitespace, comments, formatting
    InternalLogic,       // Function body changes (no signature change)
    InterfaceChange,     // Header/API signature modification
    NewFile,             // New file added
    DeletedFile,         // File removed
    Rename,              // File/symbol renamed
    DependencyChange,    // Build dependency added/removed
    ConfigChange,        // Build config, CMakeLists, etc.
    TestChange,          // Test file modification
    Unknown
};

// ============================================================================
// Per-File Change Details
// ============================================================================

struct FileChange {
    std::string path;                  // Relative to workspace root
    ChangeCategory category;
    int lines_added     = 0;
    int lines_removed   = 0;
    int lines_modified  = 0;
    bool is_header      = false;       // .h/.hpp — triggers broader impact
    bool is_critical    = false;       // Core infrastructure file
    bool modifies_api   = false;       // Public interface changed
    bool has_tests      = false;       // Test coverage exists
    std::vector<std::string> changed_functions;   // Functions with diffs
    std::vector<std::string> changed_symbols;     // Modified symbol names
};

// ============================================================================
// Impact Zone — a cluster of affected files from one change
// ============================================================================

struct ImpactZone {
    std::string source_file;                       // The file that was changed
    std::vector<std::string> directly_affected;    // Files that include/import source
    std::vector<std::string> transitively_affected;// 2nd+ order dependencies
    std::vector<std::string> test_files_relevant;  // Test files covering this zone
    std::vector<std::string> build_targets_affected;// CMake targets impacted
    
    int direct_count()     const { return static_cast<int>(directly_affected.size()); }
    int transitive_count() const { return static_cast<int>(transitively_affected.size()); }
    int total_affected()   const { return direct_count() + transitive_count(); }
};

// ============================================================================
// Impact Report — full analysis result
// ============================================================================

struct ImpactReport {
    std::string report_id;
    std::chrono::system_clock::time_point generated_at;
    std::string workspace_root;
    std::string git_branch;
    std::string git_commit_hash;       // HEAD at analysis time
    
    // Input: what changed
    std::vector<FileChange> changed_files;
    int total_lines_added   = 0;
    int total_lines_removed = 0;
    
    // Output: what's affected
    std::vector<ImpactZone> impact_zones;
    
    // Aggregate metrics
    ImpactSeverity overall_severity = ImpactSeverity::None;
    float risk_score = 0.0f;          // 0.0 (safe) → 1.0 (extremely risky)
    int total_files_affected = 0;
    int critical_files_affected = 0;
    int api_breaking_changes = 0;
    int untested_changes = 0;          // Changes in files without test coverage
    bool requires_full_rebuild = false;
    int estimated_rebuild_time_sec = 0;
    
    // Recommendations
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    bool should_block_commit = false;  // True if risk exceeds threshold
    std::string block_reason;
    
    // Serialization
    nlohmann::json toJson() const;
    std::string toSummary() const;
    std::string toDetailedReport() const;
};

// ============================================================================
// Analyzer Configuration
// ============================================================================

struct ImpactAnalyzerConfig {
    // Risk thresholds
    float block_commit_threshold = 0.85f;   // Risk score above this blocks commit
    int   critical_file_limit    = 3;       // Max critical files before warning
    int   api_break_limit        = 1;       // Max API breaks before blocking
    int   max_affected_files     = 100;     // Beyond this = Critical severity
    
    // Depth control
    int   max_transitive_depth   = 5;       // How deep to follow dep chains
    bool  include_test_impact    = true;    // Analyze test file dependencies
    bool  include_build_targets  = true;    // Map to CMake targets
    
    // Integration
    bool  auto_generate_rollback_plan  = true;  // Create rollback plan for risky changes
    bool  integrate_with_approval_gates = true; // Feed into AgenticPlanningOrchestrator
    
    nlohmann::json toJson() const;
    static ImpactAnalyzerConfig fromJson(const nlohmann::json& j);
    static ImpactAnalyzerConfig Default();
    static ImpactAnalyzerConfig Strict();
    static ImpactAnalyzerConfig Permissive();
};

// ============================================================================
// Main Change Impact Analyzer
// ============================================================================

class ChangeImpactAnalyzer {
public:
    // Callback types
    using DependencyResolverFn = std::function<std::vector<std::string>(const std::string& file)>;
    using ReverseDependencyFn  = std::function<std::vector<std::string>(const std::string& file)>;
    using CriticalFileCheckFn  = std::function<bool(const std::string& file)>;
    using BuildTargetResolverFn = std::function<std::vector<std::string>(const std::string& file)>;

    ChangeImpactAnalyzer();
    ~ChangeImpactAnalyzer();
    
    // ---- Configuration ----
    void setConfig(const ImpactAnalyzerConfig& config);
    ImpactAnalyzerConfig getConfig() const;
    void setWorkspaceRoot(const std::string& root);
    
    // ---- Callback Wiring ----
    void setDependencyResolver(DependencyResolverFn fn);
    void setReverseDependencyResolver(ReverseDependencyFn fn);
    void setCriticalFileCheck(CriticalFileCheckFn fn);
    void setBuildTargetResolver(BuildTargetResolverFn fn);
    
    // ---- Core Analysis ----
    
    /// Analyze impact of currently staged git changes (git diff --cached)
    ImpactReport analyzeStagedChanges();
    
    /// Analyze impact of unstaged working directory changes (git diff)
    ImpactReport analyzeUnstagedChanges();
    
    /// Analyze impact of specific file list
    ImpactReport analyzeFileChanges(const std::vector<FileChange>& changes);
    
    /// Analyze impact of a single file change (quick check)
    ImpactZone analyzeFileImpact(const std::string& changed_file);
    
    // ---- Risk Scoring ----
    
    /// Compute overall risk score from impact zones
    float computeRiskScore(const ImpactReport& report) const;
    
    /// Classify severity from metrics
    ImpactSeverity classifySeverity(const ImpactReport& report) const;
    
    /// Should this change set block the commit?
    bool shouldBlockCommit(const ImpactReport& report) const;
    
    // ---- Report History ----
    
    /// Get last N analysis reports
    std::vector<const ImpactReport*> getRecentReports(int count = 10) const;
    
    /// Get specific report by ID
    const ImpactReport* getReport(const std::string& reportId) const;
    
    // ---- JSON Export ----
    nlohmann::json getFullStatusJson() const;
    
private:
    // Internal helpers
    void parseGitDiff(const std::string& diff_output, std::vector<FileChange>& changes);
    std::string runGitCommand(const std::string& args) const;
    ImpactZone computeImpactZone(const FileChange& change);
    void computeTransitiveDeps(const std::string& file, std::set<std::string>& visited,
                               std::vector<std::string>& result, int depth);
    void generateWarnings(ImpactReport& report);
    void generateSuggestions(ImpactReport& report);
    float scoreFileRisk(const FileChange& change) const;
    ChangeCategory categorizeChange(const FileChange& change) const;

    mutable std::mutex m_mutex;
    std::string m_workspaceRoot;
    ImpactAnalyzerConfig m_config;
    
    // Callbacks
    DependencyResolverFn m_depResolver;
    ReverseDependencyFn  m_revDepResolver;
    CriticalFileCheckFn  m_criticalCheck;
    BuildTargetResolverFn m_buildTargetResolver;
    
    // Report history (circular buffer)
    std::vector<std::unique_ptr<ImpactReport>> m_reports;
    int m_reportIdCounter = 0;
    static constexpr int MAX_REPORTS = 50;
};

} // namespace Agentic
