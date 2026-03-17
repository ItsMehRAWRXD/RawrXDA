#pragma once
// ============================================================================
// Meta-Audit Engine — Autonomous Production Readiness Assessment
// Automatically audits codebase, predicts iteration requirements,
// generates TODOs, and orchestrates multi-cycle agent execution
// ============================================================================

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>

namespace rawrxd::agent {

// ============================================================================
// Audit Severity
// ============================================================================
enum class AuditSeverity : uint8_t {
    CRITICAL = 0,      // Production blocker
    HIGH = 1,          // Must fix before release
    MEDIUM = 2,        // Should fix
    LOW = 3,           // Nice to have
    INFO = 4           // Informational
};

// ============================================================================
// Audit Category
// ============================================================================
enum class AuditCategory : uint8_t {
    MEMORY_SAFETY = 0,
    THREAD_SAFETY = 1,
    RESOURCE_LEAK = 2,
    ERROR_HANDLING = 3,
    SECURITY = 4,
    PERFORMANCE = 5,
    OBSERVABILITY = 6,
    CONFIGURATION = 7,
    TESTING = 8,
    DOCUMENTATION = 9,
    CODE_QUALITY = 10,
    ARCHITECTURE = 11
};

// ============================================================================
// Audit Issue
// ============================================================================
struct AuditIssue {
    uint32_t id;
    AuditSeverity severity;
    AuditCategory category;
    char filePath[512];
    uint32_t lineNumber;
    char title[256];
    char description[1024];
    char recommendation[1024];
    uint32_t estimatedIterations;
    uint32_t estimatedModels;
    uint64_t estimatedTimeMs;
    float complexity;           // 0.0-1.0
    bool requiresMultiFile;
    uint32_t dependencyCount;
};

// ==================================================================================================================
// Audit Report
// ============================================================================
struct AuditReport {
    std::vector<AuditIssue> issues;
    uint64_t totalIssues;
    uint64_t criticalCount;
    uint64_t highCount;
    uint64_t mediumCount;
    uint64_t lowCount;
    uint64_t infoCount;
    
    uint64_t totalEstimatedIterations;
    uint64_t totalEstimatedTimeMs;
    float overallComplexity;
    
    std::chrono::steady_clock::time_point auditStartTime;
    std::chrono::steady_clock::time_point auditEndTime;
    uint64_t auditDurationMs;
};

// ============================================================================
// Audit Configuration
// ============================================================================
struct AuditConfig {
    bool scanMemorySafety;
    bool scanThreadSafety;
    bool scanResourceLeaks;
    bool scanErrorHandling;
    bool scanSecurity;
    bool scanPerformance;
    bool scanObservability;
    bool scanConfiguration;
    bool scanTesting;
    bool scanDocumentation;
    bool scanCodeQuality;
    bool scanArchitecture;
    
    bool includeThirdParty;
    bool includeTests;
    bool includeGenerated;
    
    uint32_t maxIssuesPerCategory;
    AuditSeverity minSeverity;
    
    const char* includePath;    // Glob pattern
    const char* excludePath;    // Glob pattern
};

// ============================================================================
// Audit Callback
// ============================================================================
using AuditProgressCallback = void(*)(float progress, const char* message, void* userData);
using IssueFoundCallback = void(*)(const AuditIssue& issue, void* userData);

// ============================================================================
// MetaAuditEngine
// ============================================================================
class MetaAuditEngine {
public:
    MetaAuditEngine();
    ~MetaAuditEngine();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // Configuration
    void setConfig(const AuditConfig& config);
    AuditConfig getConfig() const;
    
    void setProgressCallback(AuditProgressCallback callback, void* userData);
    void setIssueFoundCallback(IssueFoundCallback callback, void* userData);
    
    // Audit Execution
    AuditReport performFullAudit(const char* codebasePath);
    AuditReport performIncrementalAudit(const char* codebasePath, const char* sinceCommit = nullptr);
    
    // Targeted Audits
    std::vector<AuditIssue> scanMemorySafety(const char* codebasePath);
    std::vector<AuditIssue> scanThreadSafety(const char* codebasePath);
    std::vector<AuditIssue> scanResourceLeaks(const char* codebasePath);
    std::vector<AuditIssue> scanErrorHandling(const char* codebasePath);
    std::vector<AuditIssue> scanSecurity(const char* codebasePath);
    std::vector<AuditIssue> scanPerformance(const char* codebasePath);
    std::vector<AuditIssue> scanObservability(const char* codebasePath);
    std::vector<AuditIssue> scanConfiguration(const char* codebasePath);
    
    // Priority Analysis
    std::vector<AuditIssue> getTop20MostDifficult(const AuditReport& report);
    std::vector<AuditIssue> getTop20MostCritical(const AuditReport& report);
    std::vector<AuditIssue> getOptimalExecutionOrder(const std::vector<AuditIssue>& issues);
    
    // Iteration Prediction
    uint64_t predictTotalIterations(const std::vector<AuditIssue>& issues);
    uint64_t predictTotalTime(const std::vector<AuditIssue>& issues);
    uint32_t predictOptimalModelCount(const AuditIssue& issue);
    
    // Report Generation
    void generateTextReport(const AuditReport& report, const char* outputPath);
    void generateJsonReport(const AuditReport& report, const char* outputPath);
    void generateHtmlReport(const AuditReport& report, const char* outputPath);
    
    // Abort
    void abort();
    bool isAborting() const { return m_aborting.load(std::memory_order_acquire); }
    
private:
    // File Scanning
    std::vector<std::string> enumerateFiles(const char* path);
    bool shouldScanFile(const char* filePath);
    
    // Pattern Detection
    std::vector<AuditIssue> scanFileForMemoryIssues(const char* filePath);
    std::vector<AuditIssue> scanFileForThreadIssues(const char* filePath);
    std::vector<AuditIssue> scanFileForLeaks(const char* filePath);
    std::vector<AuditIssue> scanFileForErrorHandling(const char* filePath);
    std::vector<AuditIssue> scanFileForSecurityIssues(const char* filePath);
    std::vector<AuditIssue> scanFileForPerformanceIssues(const char* filePath);
    std::vector<AuditIssue> scanFileForObservabilityGaps(const char* filePath);
    std::vector<AuditIssue> scanFileForConfigHardcoding(const char* filePath);
    
    // Analysis
    float estimateIssueComplexity(const AuditIssue& issue);
    uint32_t estimateIterations(const AuditIssue& issue);
    uint32_t estimateModelCount(const AuditIssue& issue);
    uint64_t estimateTime(const AuditIssue& issue);
    uint32_t countDependencies(const AuditIssue& issue);
    
    // Utilities
    void notifyProgress(float progress, const char* message);
    void notifyIssueFound(const AuditIssue& issue);
    
    uint32_t getNextIssueId();
    
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_aborting;
    std::atomic<uint32_t> m_nextIssueId;
    
    AuditConfig m_config;
    
    // Callbacks
    AuditProgressCallback m_progressCallback;
    void* m_progressUserData;
    IssueFoundCallback m_issueCallback;
    void* m_issueUserData;
    
    // Caching
    mutable std::mutex m_cacheMutex;
    std::vector<std::string> m_fileCache;
};

} // namespace rawrxd::agent
