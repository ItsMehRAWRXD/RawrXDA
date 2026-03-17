// ============================================================================
// include_resolver_subagent.hpp — Autonomous Include Dependency Resolver
// ============================================================================
// Production subagent that scans translation units for missing #include
// directives, resolves header search paths, detects circular includes,
// and auto-inserts missing dependencies.
//
// Architecture: C++20, Win32, no Qt, no exceptions
// Threading:    Mutex-protected. Thread-safe.
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include "autonomous_subagent.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"
#include "../subagent_core.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>

// ============================================================================
//  IncludePath — Represents one search path with priority
// ============================================================================
struct IncludePath {
    std::string path;               ///< Absolute directory path
    enum class Type : uint8_t {
        System,                     ///< -isystem / <angle> includes
        User,                       ///< -I / "quoted" includes
        Framework,                  ///< Framework-style (macOS compat)
        Project                     ///< Inferred from project root
    } type = Type::User;

    int priority = 0;              ///< Higher = searched first
    bool exists  = true;           ///< Path verified to exist on disk

    bool operator==(const IncludePath& o) const { return path == o.path && type == o.type; }
};

// ============================================================================
//  IncludeDirective — Parsed #include from source
// ============================================================================
struct IncludeDirective {
    std::string raw;               ///< Raw text: #include "foo.h" or <bar.h>
    std::string name;              ///< Just the filename/path portion
    int line = 0;                  ///< Source line number (1-based)
    bool isAngled = false;         ///< true = <>, false = ""
    bool isResolved = false;       ///< Successfully mapped to a file
    std::string resolvedPath;      ///< Absolute path if resolved
    std::string error;             ///< Error message if unresolved
};

// ============================================================================
//  IncludeGraphNode — One translation unit's include tree
// ============================================================================
struct IncludeGraphNode {
    std::string filePath;                           ///< Absolute path of this file
    std::vector<IncludeDirective> directives;       ///< All #include directives found
    std::vector<std::string> resolvedDeps;          ///< Successfully resolved dependencies
    std::vector<std::string> unresolvedDeps;        ///< Unresolved dependencies
    bool hasCycle = false;                          ///< Part of a circular include chain
    std::string guardMacro;                         ///< Include guard macro if detected
    bool hasPragmaOnce = false;                     ///< Has #pragma once
    int depth = 0;                                  ///< Depth in include tree
};

// ============================================================================
//  IncludeFixAction — One fix to apply
// ============================================================================
struct IncludeFixAction {
    enum class Type : uint8_t {
        AddInclude,                ///< Insert a missing #include
        RemoveInclude,             ///< Remove unused #include
        ReorderInclude,            ///< Move include to correct position
        AddGuard,                  ///< Add include guard / pragma once
        FixPath,                   ///< Correct an include path (case, separator)
        AddForwardDecl,            ///< Add forward declaration instead of full include
        BreakCycle                 ///< Break circular dependency
    } type;

    std::string filePath;          ///< Target file to modify
    int insertLine = -1;           ///< Line to insert at (-1 = auto)
    std::string oldText;           ///< Text to replace (empty for insert)
    std::string newText;           ///< Replacement / insertion text
    std::string reason;            ///< Human-readable reason
    int priority = 0;              ///< Fix priority (higher = more important)

    bool operator<(const IncludeFixAction& o) const { return priority > o.priority; }
};

// ============================================================================
//  IncludeResolverConfig — Configuration
// ============================================================================
struct IncludeResolverConfig {
    std::vector<IncludePath> searchPaths;           ///< Include search paths
    std::string projectRoot;                        ///< Project root directory
    int maxDepth = 64;                              ///< Max include depth before cycle
    int maxFilesPerScan = 4096;                     ///< Max files to scan in one pass
    bool autoInsertGuards = true;                   ///< Auto-add include guards
    bool preferPragmaOnce = true;                   ///< Prefer #pragma once over guards
    bool detectUnused = true;                       ///< Detect unused includes
    bool fixPaths = true;                           ///< Fix path separators / case
    bool addForwardDecls = true;                    ///< Prefer forward decls when possible
    bool breakCycles = true;                        ///< Attempt to break circular deps
    int maxRetries = 3;                             ///< Max retries per file on failure
    int perFileTimeoutMs = 30000;                   ///< Timeout per file scan

    /// File extensions to consider as source files
    std::vector<std::string> sourceExtensions = {
        ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx", ".inc", ".asm"
    };

    /// Known system headers (won't try to resolve to project files)
    std::unordered_set<std::string> systemHeaders;
};

// ============================================================================
//  IncludeResolverResult — Final outcome
// ============================================================================
struct IncludeResolverResult {
    bool success = false;
    std::string scanId;
    int filesScanned = 0;
    int includesResolved = 0;
    int includesUnresolved = 0;
    int fixesApplied = 0;
    int fixesFailed = 0;
    int cyclesDetected = 0;
    int guardsAdded = 0;
    std::vector<IncludeFixAction> appliedFixes;
    std::vector<IncludeFixAction> failedFixes;
    std::vector<std::string> cycleChains;           ///< Formatted cycle descriptions
    std::string error;
    int elapsedMs = 0;

    static IncludeResolverResult ok(const std::string& id, int scanned, int resolved) {
        IncludeResolverResult r;
        r.success = true;
        r.scanId = id;
        r.filesScanned = scanned;
        r.includesResolved = resolved;
        return r;
    }
    static IncludeResolverResult fail(const std::string& id, const std::string& msg) {
        IncludeResolverResult r;
        r.success = false;
        r.scanId = id;
        r.error = msg;
        return r;
    }

    std::string summary() const;
};

// ============================================================================
//  Callbacks
// ============================================================================
using IncludeResolverProgressCb = std::function<void(const std::string& scanId,
                                                      int filesProcessed, int totalFiles)>;
using IncludeResolverFixCb      = std::function<void(const std::string& scanId,
                                                      const IncludeFixAction& fix, bool applied)>;
using IncludeResolverCompleteCb = std::function<void(const IncludeResolverResult& result)>;

// ============================================================================
//  IncludeResolverSubAgent — Production include dependency resolver
// ============================================================================
class IncludeResolverSubAgent {
public:
    explicit IncludeResolverSubAgent(SubAgentManager* manager,
                                     AgenticEngine* engine,
                                     AgenticFailureDetector* detector = nullptr,
                                     AgenticPuppeteer* puppeteer = nullptr);
    ~IncludeResolverSubAgent();

    // ---- Configuration ----
    void setConfig(const IncludeResolverConfig& config);
    const IncludeResolverConfig& config() const { return m_config; }

    /// Add a search path at runtime
    void addSearchPath(const std::string& path, IncludePath::Type type = IncludePath::Type::User);

    /// Auto-detect search paths from project structure
    void autoDetectSearchPaths(const std::string& projectRoot);

    // ---- Single-File Operations ----

    /// Parse all #include directives from a source file
    std::vector<IncludeDirective> parseIncludes(const std::string& filePath) const;

    /// Resolve a single include directive to an absolute path
    std::string resolveInclude(const IncludeDirective& directive,
                                const std::string& includerPath) const;

    /// Build the full include graph for one file
    IncludeGraphNode buildIncludeGraph(const std::string& filePath,
                                       std::unordered_set<std::string>& visited,
                                       int depth = 0) const;

    // ---- Bulk Scan & Fix ----

    /// Scan files and resolve all includes (synchronous)
    IncludeResolverResult scan(const std::string& parentId,
                                const std::vector<std::string>& filePaths);

    /// Scan + auto-fix all found issues (synchronous)
    IncludeResolverResult scanAndFix(const std::string& parentId,
                                      const std::vector<std::string>& filePaths);

    /// Async variant — returns scan ID
    std::string scanAndFixAsync(const std::string& parentId,
                                 const std::vector<std::string>& filePaths,
                                 IncludeResolverCompleteCb onComplete = nullptr);

    // ---- Fix Generation ----

    /// Generate fixes for unresolved includes (without applying)
    std::vector<IncludeFixAction> generateFixes(
        const std::vector<IncludeGraphNode>& graph) const;

    /// Apply a batch of fixes
    int applyFixes(std::vector<IncludeFixAction>& fixes);

    // ---- Cycle Detection ----

    /// Detect circular include chains in a graph
    std::vector<std::vector<std::string>> detectCycles(
        const std::unordered_map<std::string, IncludeGraphNode>& graph) const;

    /// Generate fixes to break detected cycles
    std::vector<IncludeFixAction> generateCycleBreakers(
        const std::vector<std::vector<std::string>>& cycles) const;

    // ---- Header Guard Management ----

    /// Check if file has an include guard or #pragma once
    bool hasIncludeGuard(const std::string& filePath) const;

    /// Generate a guard macro name from file path
    std::string generateGuardMacro(const std::string& filePath) const;

    // ---- Callbacks ----
    void setOnProgress(IncludeResolverProgressCb cb) { m_onProgress = cb; }
    void setOnFix(IncludeResolverFixCb cb)           { m_onFix = cb; }
    void setOnComplete(IncludeResolverCompleteCb cb)  { m_onComplete = cb; }

    // ---- Status ----
    bool isRunning() const { return m_running.load(); }
    void cancel();

    struct Stats {
        int64_t totalScans = 0;
        int64_t totalFilesProcessed = 0;
        int64_t totalIncludesResolved = 0;
        int64_t totalFixesApplied = 0;
        int64_t totalCyclesDetected = 0;
        int64_t totalGuardsAdded = 0;
    };
    Stats getStats() const;
    void resetStats();

private:
    /// Read file contents (cached)
    std::string readFile(const std::string& path) const;

    /// Check if a file exists on disk
    bool fileExists(const std::string& path) const;

    /// Normalize path separators and resolve relative paths
    std::string normalizePath(const std::string& path) const;

    /// Determine if a header is a system header
    bool isSystemHeader(const std::string& name) const;

    /// Find the best insertion point for a new #include
    int findInsertionPoint(const std::string& fileContent,
                           bool isAngled) const;

    /// Apply a single fix to a file
    bool applySingleFix(const IncludeFixAction& fix);

    /// Self-heal a failed resolution via LLM
    std::string selfHealResolution(const IncludeDirective& directive,
                                    const std::string& filePath);

    /// DFS cycle detector
    void dfs(const std::string& node,
             const std::unordered_map<std::string, IncludeGraphNode>& graph,
             std::unordered_set<std::string>& visited,
             std::unordered_set<std::string>& stack,
             std::vector<std::string>& currentPath,
             std::vector<std::vector<std::string>>& cycles) const;

    /// Generate UUID
    std::string generateId() const;

    // ---- Members ----
    SubAgentManager*         m_manager   = nullptr;
    AgenticEngine*           m_engine    = nullptr;
    AgenticFailureDetector*  m_detector  = nullptr;
    AgenticPuppeteer*        m_puppeteer = nullptr;

    IncludeResolverConfig    m_config;
    mutable std::mutex       m_mutex;
    std::atomic<bool>        m_running{false};
    std::atomic<bool>        m_cancelled{false};

    // File content cache (mutable for const methods)
    mutable std::mutex       m_cacheMutex;
    mutable std::unordered_map<std::string, std::string> m_fileCache;

    // Include graph cache
    mutable std::unordered_map<std::string, IncludeGraphNode> m_graphCache;

    Stats                    m_stats;

    IncludeResolverProgressCb m_onProgress;
    IncludeResolverFixCb      m_onFix;
    IncludeResolverCompleteCb m_onComplete;
};
