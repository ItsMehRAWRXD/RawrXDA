#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <expected>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Forward declarations to avoid dependency issues if headers are missing in this context
// In a real build, these headers would be ensured to exist.
namespace RawrXD {
    class TokenGenerator;
    class SwarmOrchestrator;
    class ChainOfThought;
    namespace Net { class NetworkManager; }
    class MonacoEditor;
    class CPUInferenceEngine;
    class GGUFParser;
    class VulkanCompute;
}

// Check if headers exist, otherwise use forward decls effectively
#if __has_include("token_generator.h")
#include "token_generator.h"
#endif
#if __has_include("swarm_orchestrator.h")
#include "swarm_orchestrator.h"
#endif
#if __has_include("chain_of_thought.h")
#include "chain_of_thought.h"
#endif
#if __has_include("net_impl_win32.h")
#include "net_impl_win32.h"
#endif
#if __has_include("monaco_integration.h")
#include "monaco_integration.h"
#endif
#include "cpu_inference_engine.h"
#if __has_include("gguf_parser.h")
#include "gguf_parser.h"
#endif
#include "vulkan_compute.h"

namespace RawrXD {

enum class AuditError {
    Success = 0,
    InitializationFailed,
    ComponentNotFound,
    BenchmarkFailed,
    MetricsCollectionFailed,
    NetworkUnavailable,
    InferenceFailed,
    TokenizationFailed,
    AnalysisFailed,
    ReportGenerationFailed,
    Timeout,
    CancellationRequested
};

struct BenchmarkResult {
    float inferenceLatency;      // ms per token
    float tokenGenerationRate;   // tokens per second
    float memoryEfficiency;      // memory usage ratio
    float codeCompletionAccuracy; // percentage
    float bugDetectionRate;      // percentage
    float securityScanRate;      // percentage
    float refactoringSuccessRate; // percentage
    float documentationQuality;  // score 0-1
};

struct CompetitiveResult {
    std::string competitor; // "GitHub Copilot", "Cursor", "JetBrains"
    BenchmarkResult theirResult;
    BenchmarkResult ourResult;
    float performanceRatio; // ourResult / theirResult
    float accuracyRatio;
    float featureCoverage;  // percentage of their features we have
    std::vector<std::string> ourAdvantages;
    std::vector<std::string> ourDisadvantages;
};

struct CodeMetrics {
    size_t linesOfCode;
    size_t cyclomaticComplexity;
    size_t cognitiveComplexity;
    size_t halsteadVolume;
    size_t maintainabilityIndex;
    size_t securityIssues;
    size_t performanceBottlenecks;
    size_t codeSmells;
    float testCoverage;
    float documentationCoverage;
};

struct AuditSuggestion {
    std::string category; // "performance", "security", "maintainability", "accuracy"
    std::string description;
    std::string action;
    float impactScore; // 0-1
    float effortScore; // 0-1 (lower is easier)
    std::vector<std::string> prerequisites;
};

class IDEAuditor : public std::enable_shared_from_this<IDEAuditor> {
public:
    static IDEAuditor& getInstance();
    ~IDEAuditor();
    
    // Non-copyable
    IDEAuditor(const IDEAuditor&) = delete;
    IDEAuditor& operator=(const IDEAuditor&) = delete;
    
    // Real initialization
    std::expected<void, AuditError> initialize(
        std::shared_ptr<TokenGenerator> tokenizer,
        std::shared_ptr<SwarmOrchestrator> swarm,
        std::shared_ptr<ChainOfThought> chain,
        std::shared_ptr<Net::NetworkManager> network,
        std::shared_ptr<MonacoEditor> editor,
        std::shared_ptr<CPUInferenceEngine> inference,
        std::shared_ptr<GGUFParser> parser,
        std::shared_ptr<VulkanCompute> vulkan
    );
    
    // Real benchmarking
    std::expected<BenchmarkResult, AuditError> benchmarkInference(
        const std::string& testPrompt,
        size_t iterations
    );
    
    std::expected<BenchmarkResult, AuditError> benchmarkCodeCompletion(
        const std::vector<std::string>& testCases
    );
    
    std::expected<BenchmarkResult, AuditError> benchmarkBugDetection(
        const std::vector<std::string>& buggyCode
    );
    
    // Real competitive analysis
    std::expected<CompetitiveResult, AuditError> compareWithGitHubCopilot(
        const std::vector<std::string>& testCases
    );
    
    std::expected<CompetitiveResult, AuditError> compareWithCursor(
        const std::vector<std::string>& testCases
    );
    
    std::expected<CompetitiveResult, AuditError> compareWithJetBrains(
        const std::vector<std::string>& testCases
    );
    
    // Real code analysis
    std::expected<CodeMetrics, AuditError> analyzeCodebase(
        const std::string& path
    );
    
    std::expected<CodeMetrics, AuditError> analyzeCode(
        const std::string& code
    );
    
    // Real security scanning
    std::expected<std::vector<std::string>, AuditError> scanSecurity(
        const std::string& code
    );
    
    // Real performance analysis
    std::expected<std::vector<std::string>, AuditError> analyzePerformance(
        const std::string& code
    );
    
    // Real suggestion generation
    std::expected<std::vector<AuditSuggestion>, AuditError> generateSuggestions(
        const CodeMetrics& metrics,
        const BenchmarkResult& benchmark
    );
    
    // Real report generation
    std::expected<void, AuditError> generateReport(
        const std::string& path,
        const std::vector<CompetitiveResult>& comparisons,
        const std::vector<AuditSuggestion>& suggestions
    );
    
    // Real continuous monitoring
    void startMonitoring();
    void stopMonitoring();
    std::expected<nlohmann::json, AuditError> getMonitoringData() const;
    
    // Status
    bool isMonitoring() const { return m_monitoring.load(); }
    nlohmann::json getStatus() const;
    nlohmann::json getMetrics() const;
    
    // Logging
    void logAction(const std::string& component, const std::string& action, const std::string& details);
    void logError(const std::string& component, const std::string& error);
    void logBenchmark(const std::string& test, float score);
    void logComparison(const std::string& competitor, float ratio);
    
    // Public constructor for make_shared, though it's singleton-ish pattern in usage
    IDEAuditor();

private:
    // Real components
    std::shared_ptr<TokenGenerator> m_tokenizer;
    std::shared_ptr<SwarmOrchestrator> m_swarm;
    std::shared_ptr<ChainOfThought> m_chain;
    std::shared_ptr<Net::NetworkManager> m_network;
    std::shared_ptr<MonacoEditor> m_editor;
    std::shared_ptr<CPUInferenceEngine> m_inference;
    std::shared_ptr<GGUFParser> m_parser;
    std::shared_ptr<VulkanCompute> m_vulkan;
    
    // Real monitoring
    std::atomic<bool> m_monitoring{false};
    std::thread m_monitoringThread;
    mutable std::mutex m_mutex;
    
    // Real metrics
    std::atomic<size_t> m_totalAudits{0};
    std::atomic<size_t> m_successfulAudits{0};
    std::atomic<size_t> m_failedAudits{0};
    std::atomic<std::chrono::milliseconds> m_totalAuditTime{0};
    std::unordered_map<std::string, BenchmarkResult> m_benchmarkCache;
    std::unordered_map<std::string, CompetitiveResult> m_comparisonCache;
    std::vector<AuditSuggestion> m_suggestionsCache;
    
    // Real implementation methods
    std::expected<BenchmarkResult, AuditError> runInferenceBenchmark(
        const std::string& prompt,
        size_t iterations
    );
    
    std::expected<BenchmarkResult, AuditError> runCompletionBenchmark(
        const std::vector<std::string>& testCases
    );
    
    std::expected<BenchmarkResult, AuditError> runBugDetectionBenchmark(
        const std::vector<std::string>& buggyCode
    );
    
    std::expected<CodeMetrics, AuditError> calculateCodeMetrics(
        const std::string& code
    );
    
    std::expected<std::vector<std::string>, AuditError> runSecurityScan(
        const std::string& code
    );
    
    std::expected<std::vector<std::string>, AuditError> runPerformanceAnalysis(
        const std::string& code
    );
    
    std::expected<std::vector<AuditSuggestion>, AuditError> generateImprovementSuggestions(
        const CodeMetrics& metrics,
        const BenchmarkResult& benchmark
    );
    
    void monitoringLoop();
    void collectRealTimeMetrics();
    void analyzeTrends();
    void predictPerformance();
    
    // Real competitive API calls
    std::expected<std::string, AuditError> callGitHubCopilot(
        const std::string& prompt,
        const std::string& apiKey
    );
    
    std::expected<std::string, AuditError> callCursor(
        const std::string& prompt,
        const std::string& apiKey
    );
    
    std::expected<std::string, AuditError> callJetBrains(
        const std::string& prompt,
        const std::string& apiKey
    );
    
    // Real report generation
    void generateMarkdownReport(
        const std::string& path,
        const std::vector<CompetitiveResult>& comparisons,
        const std::vector<AuditSuggestion>& suggestions
    );
    
    void generateHTMLReport(
        const std::string& path,
        const std::vector<CompetitiveResult>& comparisons,
        const std::vector<AuditSuggestion>& suggestions
    );
    
    void generateJSONReport(
        const std::string& path,
        const std::vector<CompetitiveResult>& comparisons,
        const std::vector<AuditSuggestion>& suggestions
    );
    
    // Helpers
    std::string getTimestamp() const;
    float calculateConfidence(const std::string& result, const std::string& expected);
    float calculateSemanticSimilarity(const std::string& a, const std::string& b);
    std::vector<std::string> extractKeywords(const std::string& text);
    bool isValidCode(const std::string& code);
    bool isSecureCode(const std::string& code);
    bool isOptimizedCode(const std::string& code);
    std::string readFile(const std::string& path);

    // Logging
    std::ofstream m_logFile;
    std::string m_sessionLog;
    mutable std::mutex m_logMutex;
};

} // namespace RawrXD