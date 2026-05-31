// =============================================================================
// autonomous_background_daemon.hpp
// RawrXD v14.2.1 Cathedral — Phase B: Continuous Background Autonomy
//
// Daemon threads that PROACTIVELY process the codebase:
//   - Dead code detection & refactoring
//   - Documentation freshness checking
//   - Hot-path optimization based on profiling
//   - Security audit on file-save
//   - Dependency vulnerability scanning
//
// Runs 24/7 on low-priority threads. No human ignition required.
// =============================================================================
#pragma once
#ifndef RAWRXD_AUTONOMOUS_BACKGROUND_DAEMON_HPP
#define RAWRXD_AUTONOMOUS_BACKGROUND_DAEMON_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/inotify.h>
#endif

namespace RawrXD {
namespace Autonomy {

// =============================================================================
// Result Types
// =============================================================================

struct DaemonResult {
    bool        success;
    const char* detail;
    int         errorCode;
    int         itemsProcessed;
    int         itemsFixed;
    double      durationMs;

    static DaemonResult ok(const char* msg, int processed = 0, int fixed = 0) {
        DaemonResult r;
        r.success        = true;
        r.detail         = msg;
        r.errorCode      = 0;
        r.itemsProcessed = processed;
        r.itemsFixed     = fixed;
        r.durationMs     = 0.0;
        return r;
    }

    static DaemonResult error(const char* msg, int code = -1) {
        DaemonResult r;
        r.success        = false;
        r.detail         = msg;
        r.errorCode      = code;
        r.itemsProcessed = 0;
        r.itemsFixed     = 0;
        r.durationMs     = 0.0;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class DaemonTaskType : uint8_t {
    DeadCodeScan,           // Find unreachable / uncalled functions
    DocumentationSync,      // Detect signature changes, update docs
    HotPathOptimize,        // Profile-guided optimization hints
    SecurityAudit,          // CVE scanning, unsafe patterns
    DependencyCheck,        // Unused deps, version vulnerabilities
    CodeQualityLint,        // Style, complexity, duplication
    TestCoverageGap,        // Find untested code paths
    MemoryLeakScan,         // Static analysis for leaks
    PerformanceRegression,  // Compare against baseline metrics
    RefactorSuggestion      // Suggest improvements to code structure
};

enum class DaemonPriority : uint8_t {
    Idle     = 0,  // Only when system completely idle
    Low      = 1,  // Background — minimal CPU impact
    Normal   = 2,  // Standard daemon processing
    High     = 3,  // Triggered by file-save events
    Critical = 4   // Security issue detected
};

enum class FileChangeType : uint8_t {
    Created,
    Modified,
    Deleted,
    Renamed
};

// =============================================================================
// Daemon Task
// =============================================================================

struct DaemonTask {
    uint64_t          taskId;
    DaemonTaskType    type;
    DaemonPriority    priority;
    char              targetPath[260];        // File or directory
    char              description[256];
    bool              inProgress;
    bool              completed;
    DaemonResult      result;
    std::chrono::steady_clock::time_point queuedAt;
    std::chrono::steady_clock::time_point startedAt;
};

// =============================================================================
// File Change Event
// =============================================================================

struct FileChangeEvent {
    FileChangeType  type;
    char            path[260];
    char            oldPath[260];    // For renames
    uint64_t        timestamp;       // ms since epoch
    uint32_t        fileSize;
};

// =============================================================================
// Scan Results
// =============================================================================

struct DeadCodeEntry {
    char     file[260];
    int      line;
    char     symbolName[256];
    char     symbolType[32];     // "function", "variable", "class", "typedef"
    float    confidence;         // 0.0 – 1.0
    bool     autoRemovable;      // Safe to auto-delete?
};

struct DocSyncEntry {
    char     file[260];
    int      line;
    char     functionName[256];
    char     oldSignature[512];
    char     newSignature[512];
    char     suggestedDoc[1024];
};

struct SecurityFinding {
    char     file[260];
    int      line;
    char     cveId[32];
    char     severity[16];       // "critical", "high", "medium", "low"
    char     description[512];
    char     suggestedFix[512];
    float    confidence;
};

struct OptimizationHint {
    char     file[260];
    int      line;
    char     functionName[256];
    double   currentLatencyMs;
    double   estimatedImproveMs;
    char     suggestion[512];
    char     category[64];       // "loop-unroll", "SIMD", "cache-align", "inline"
};

// =============================================================================
// Callbacks (C function pointers)
// =============================================================================

typedef void (*DaemonEventCallback)(DaemonTaskType type, const char* detail, void* userData);
typedef void (*FileChangeCallback)(const FileChangeEvent* event, void* userData);
typedef void (*DeadCodeCallback)(const DeadCodeEntry* entry, void* userData);
typedef void (*DocSyncCallback)(const DocSyncEntry* entry, void* userData);
typedef void (*SecurityCallback)(const SecurityFinding* finding, void* userData);
typedef void (*OptimizationCallback)(const OptimizationHint* hint, void* userData);

// =============================================================================
// Configuration
// =============================================================================

struct DaemonConfig {
    // Scan intervals (milliseconds)
    uint32_t deadCodeIntervalMs;       // Default: 300000 (5 min)
    uint32_t docSyncIntervalMs;        // Default: 60000  (1 min)
    uint32_t securityIntervalMs;       // Default: 600000 (10 min)
    uint32_t dependencyIntervalMs;     // Default: 3600000 (1 hr)
    uint32_t optimizeIntervalMs;       // Default: 900000 (15 min)
    uint32_t qualityIntervalMs;        // Default: 300000 (5 min)

    // File watcher
    bool     watchFileChanges;         // Default: true
    bool     triggerOnSave;            // Default: true
    int      maxPendingEvents;         // Default: 1000

    // Resource limits
    int      maxCpuPercent;            // Default: 15% (low impact)
    int      maxThreads;               // Default: 2
    int      maxMemoryMB;              // Default: 256

    // Auto-fix policies
    bool     autoFixDeadCode;          // Default: false (propose only)
    bool     autoFixDocs;              // Default: true
    bool     autoFixSecurity;          // Default: false (too risky)
    bool     autoFixFormatting;        // Default: true

    // Paths
    char     watchDirectory[260];
    char     excludePatterns[1024];    // Semicolon-separated glob patterns

    static DaemonConfig defaults() {
        DaemonConfig c;
        c.deadCodeIntervalMs   = 300000;
        c.docSyncIntervalMs    = 60000;
        c.securityIntervalMs   = 600000;
        c.dependencyIntervalMs = 3600000;
        c.optimizeIntervalMs   = 900000;
        c.qualityIntervalMs    = 300000;
        c.watchFileChanges     = true;
        c.triggerOnSave        = true;
        c.maxPendingEvents     = 1000;
        c.maxCpuPercent        = 15;
        c.maxThreads           = 2;
        c.maxMemoryMB          = 256;
        c.autoFixDeadCode      = false;
        c.autoFixDocs          = true;
        c.autoFixSecurity      = false;
        c.autoFixFormatting    = true;
        std::strncpy(c.watchDirectory, ".", sizeof(c.watchDirectory) - 1);
        std::strncpy(c.excludePatterns, "build;.git;node_modules;obj", sizeof(c.excludePatterns) - 1);
        return c;
    }
};

// =============================================================================
// Core Class: AutonomousBackgroundDaemon
// =============================================================================

class AutonomousBackgroundDaemon {
public:
    static AutonomousBackgroundDaemon& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    DaemonResult start();
    DaemonResult start(const DaemonConfig& config);
    DaemonResult stop();
    bool         isRunning() const { return m_running.load(); }

    // ── Configuration ──────────────────────────────────────────────────────
    void          setConfig(const DaemonConfig& config);
    DaemonConfig  getConfig() const;

    // ── Manual Triggers (in addition to automatic) ─────────────────────────
    uint64_t triggerDeadCodeScan(const char* directory = nullptr);
    uint64_t triggerDocSync(const char* directory = nullptr);
    uint64_t triggerSecurityAudit(const char* directory = nullptr);
    uint64_t triggerDependencyCheck();
    uint64_t triggerHotPathOptimize();
    uint64_t triggerFullScan();

    // ── File Change Notification (from IDE) ────────────────────────────────
    void onFileChanged(const FileChangeEvent& event);
    void onFileSaved(const char* path);
    void onProjectOpened(const char* rootPath);
    void onProjectClosed();

    // ── Task Queue Management ──────────────────────────────────────────────
    int  getPendingTaskCount() const;
    int  getCompletedTaskCount() const;
    bool cancelTask(uint64_t taskId);
    void clearCompletedTasks();
    void pauseProcessing();
    void resumeProcessing();
    bool isPaused() const { return m_paused.load(); }

    // ── Result Retrieval ───────────────────────────────────────────────────
    std::vector<DeadCodeEntry>     getDeadCodeFindings() const;
    std::vector<DocSyncEntry>      getDocSyncFindings() const;
    std::vector<SecurityFinding>   getSecurityFindings() const;
    std::vector<OptimizationHint>  getOptimizationHints() const;

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setEventCallback(DaemonEventCallback cb, void* ud);
    void setFileChangeCallback(FileChangeCallback cb, void* ud);
    void setDeadCodeCallback(DeadCodeCallback cb, void* ud);
    void setDocSyncCallback(DocSyncCallback cb, void* ud);
    void setSecurityCallback(SecurityCallback cb, void* ud);
    void setOptimizationCallback(OptimizationCallback cb, void* ud);

    // ── Sub-system Injection ───────────────────────────────────────────────
    // Static analyzer for dead code detection
    typedef std::vector<DeadCodeEntry> (*DeadCodeAnalyzerFn)(const char* dir, void* ud);
    void setDeadCodeAnalyzer(DeadCodeAnalyzerFn fn, void* ud);

    // Doc generator for signature sync
    typedef std::string (*DocGeneratorFn)(const char* funcSig, const char* context, void* ud);
    void setDocGenerator(DocGeneratorFn fn, void* ud);

    // Security scanner
    typedef std::vector<SecurityFinding> (*SecurityScannerFn)(const char* dir, void* ud);
    void setSecurityScanner(SecurityScannerFn fn, void* ud);

    // Performance profiler
    typedef std::vector<OptimizationHint> (*ProfilerFn)(const char* dir, void* ud);
    void setProfiler(ProfilerFn fn, void* ud);

    // ── Statistics ──────────────────────────────────────────────────────────
    struct Stats {
        uint64_t totalScans;
        uint64_t deadCodeFound;
        uint64_t docsUpdated;
        uint64_t securityIssuesFound;
        uint64_t optimizationsProposed;
        uint64_t autoFixesApplied;
        uint64_t fileChangesProcessed;
        double   totalCpuTimeMs;
        double   uptimeMs;
        std::chrono::steady_clock::time_point startedAt;
    };

    Stats getStats() const;
    void  resetStats();

    // ── JSON ───────────────────────────────────────────────────────────────
    std::string statsToJson() const;
    std::string findingsToJson() const;
    std::string statusToJson() const;

    // ── Notification System ────────────────────────────────────────────────
    struct Notification {
        char     title[128];
        char     message[512];
        char     category[64];
        uint64_t timestamp;
        bool     read;
    };

    std::vector<Notification> getNotifications(int maxCount = 50) const;
    void                      clearNotifications();
    int                       getUnreadCount() const;

private:
    AutonomousBackgroundDaemon();
    ~AutonomousBackgroundDaemon();
    AutonomousBackgroundDaemon(const AutonomousBackgroundDaemon&) = delete;
    AutonomousBackgroundDaemon& operator=(const AutonomousBackgroundDaemon&) = delete;

    // ── Thread Entry Points ────────────────────────────────────────────────
#ifdef _WIN32
    static DWORD WINAPI schedulerThreadProc(LPVOID param);
    static DWORD WINAPI workerThreadProc(LPVOID param);
    static DWORD WINAPI fileWatcherThreadProc(LPVOID param);
    HANDLE m_schedulerThread;
    HANDLE m_workerThreads[4];
    HANDLE m_fileWatcherThread;
#else
    static void* schedulerThreadProc(void* param);
    static void* workerThreadProc(void* param);
    static void* fileWatcherThreadProc(void* param);
    pthread_t m_schedulerThread;
    pthread_t m_workerThreads[4];
    pthread_t m_fileWatcherThread;
#endif

    // ── Internal Methods ───────────────────────────────────────────────────
    void schedulerLoop();
    void workerLoop();
    void fileWatcherLoop();
    void processTask(DaemonTask& task);
    void enqueueTask(DaemonTask task);
    void addNotification(const char* title, const char* msg, const char* category);
    void analyzeFileChange(const FileChangeEvent& event);
    bool shouldExclude(const char* path) const;
    uint64_t nextTaskId();

    // Task processors
    DaemonResult processDeadCodeScan(DaemonTask& task);
    DaemonResult processDocSync(DaemonTask& task);
    DaemonResult processSecurityAudit(DaemonTask& task);
    DaemonResult processDependencyCheck(DaemonTask& task);
    DaemonResult processHotPathOptimize(DaemonTask& task);
    DaemonResult processCodeQuality(DaemonTask& task);

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    mutable std::mutex m_queueMutex;
    mutable std::mutex m_findingsMutex;
    mutable std::mutex m_notifyMutex;

    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_paused{false};
    std::atomic<uint64_t> m_nextId{1};

    DaemonConfig m_config;

    // Task queues (priority-ordered)
    std::deque<DaemonTask>                          m_pendingTasks;
    std::unordered_map<uint64_t, DaemonTask>        m_completedTasks;

    // Accumulated findings
    std::vector<DeadCodeEntry>     m_deadCode;
    std::vector<DocSyncEntry>      m_docSync;
    std::vector<SecurityFinding>   m_security;
    std::vector<OptimizationHint>  m_optimizations;
    std::deque<Notification>       m_notifications;

    // File change event queue
    std::deque<FileChangeEvent>    m_fileEvents;

    // Last scan timestamps
    std::chrono::steady_clock::time_point m_lastDeadCodeScan;
    std::chrono::steady_clock::time_point m_lastDocSync;
    std::chrono::steady_clock::time_point m_lastSecurityScan;
    std::chrono::steady_clock::time_point m_lastDependencyCheck;
    std::chrono::steady_clock::time_point m_lastOptimizeScan;

    // ── Injected Subsystems ────────────────────────────────────────────────
    DeadCodeAnalyzerFn m_deadCodeAnalyzer;  void* m_deadCodeUD;
    DocGeneratorFn     m_docGenerator;      void* m_docGenUD;
    SecurityScannerFn  m_securityScanner;   void* m_secScanUD;
    ProfilerFn         m_profiler;          void* m_profilerUD;

    // ── Callbacks ──────────────────────────────────────────────────────────
    DaemonEventCallback    m_eventCb;       void* m_eventCbUD;
    FileChangeCallback     m_fileChangeCb;  void* m_fileChangeCbUD;
    DeadCodeCallback       m_deadCodeCb;    void* m_deadCodeCbUD;
    DocSyncCallback        m_docSyncCb;     void* m_docSyncCbUD;
    SecurityCallback       m_securityCb;    void* m_securityCbUD;
    OptimizationCallback   m_optimizeCb;    void* m_optimizeCbUD;

    // ── Telemetry ──────────────────────────────────────────────────────────
    alignas(64) Stats m_stats;
};

} // namespace Autonomy
} // namespace RawrXD

#endif // RAWRXD_AUTONOMOUS_BACKGROUND_DAEMON_HPP
