#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <mutex>

namespace RawrXD {

// ============================================================================
// Production GGUF Loader Diagnostics
// Thread-safe, comprehensive error tracking with actionable insights
// ============================================================================

class GGUFLoaderDiagnostics {
public:
    enum class LoadStage {
        kFileOpen,
        kHeaderParse,
        kMetadataRead,
        kVocabLoad,
        kWeightDecode,
        kVulkanInit,
        kTransformerBuild,
        kComplete,
        kUnknown
    };

    struct LoadEvent {
        LoadStage stage;
        std::string detail;
        std::chrono::steady_clock::time_point timestamp;
        bool success = true;
        std::string errorMessage;
        uint64_t memoryUsage = 0;
        double durationMs = 0.0;

        std::string StageName() const;
    };

    static GGUFLoaderDiagnostics& Instance();

    void StartLoad(const std::string& modelPath);
    void RecordStage(LoadStage stage, bool success, const std::string& detail = "",
                    const std::string& errorMsg = "", uint64_t memoryUsage = 0);
    void EndLoad();

    std::string GetDiagnosticReport();
    std::string GetJsonReport();
    const std::vector<LoadEvent>& GetEvents() const;

    // Advanced diagnostics
    std::vector<std::pair<std::string, std::string>> GetFailedStages() const;
    bool HasCriticalFailure() const;

private:
    GGUFLoaderDiagnostics();
    ~GGUFLoaderDiagnostics();
    GGUFLoaderDiagnostics(const GGUFLoaderDiagnostics&) = delete;
    GGUFLoaderDiagnostics& operator=(const GGUFLoaderDiagnostics&) = delete;

    mutable std::mutex m_mutex;
    std::vector<LoadEvent> m_events;
    std::string m_modelPath;
    std::chrono::steady_clock::time_point m_startTime;
    uint64_t m_totalMemoryPeak = 0;
    uint64_t m_currentLoadId = 0;

    std::string FormatMemory(uint64_t bytes) const;
    std::string EscapeJson(const std::string& str) const;
    void AnalyzeLoadPerformance();
    std::string GetRecommendedActions(const std::vector<std::pair<std::string, std::string>>& failures) const;
    std::string GetFailedStagesJson() const;
};

}  // namespace RawrXD
